#include "command_line_interface.h"

#include <progdn_core/ini_file.h>
#include <progdn_core/ip_address_helper.h>
#include <progdn_core/system_limits.h>
#include <progdn_core/system_log.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/utility/string_view.hpp>

#include <iostream>
#include <memory>

namespace progdn
{
    static boost::asio::ip::tcp::endpoint parse_to_ip_port(const std::string& str) {
        std::vector<std::string> components;
        components.reserve(2);
        try {
            boost::split(components, str, [](char c) { return (c == ':'); });
            auto ip = boost::asio::ip::address_v4::from_string(components.at(0));
            auto port = boost::lexical_cast<uint16_t>(components.at(1));
            return boost::asio::ip::tcp::endpoint(ip, port);
        } catch (const std::exception& e) {
            throw std::runtime_error("'" + str + "' is not an IP/port string: " + e.what());
        }
    }

    // HAProxy Protocol Header (Version 1)
    // https://www.haproxy.org/download/1.8/doc/proxy-protocol.txt
    namespace haproxy_protocol {

        static boost::string_view kEndMarker("\r\n");
        namespace tcp4 {
            static const size_t kMaximalHeaderSize = 56;
        }

        struct Header {
            IP_Host src_ip;
            IP_Host dst_ip;
            uint16_t src_port;
            uint16_t dst_port;
        };
    }

    struct Config {
        boost::asio::ip::tcp::endpoint listen;
        int mark;
        int table;

        Config(const boost::filesystem::path& filepath) {
            auto ini_file = IniFile::parse(filepath);
            listen = parse_to_ip_port(ini_file.get<std::string>("listen"));
            mark = ini_file.get<int>("mark");
            table = ini_file.get<int>("table");
        }
    };

    class Session : public boost::noncopyable
    {
    private:
        using CounterT = size_t;
        static std::atomic<CounterT> m_total_objects;
        static std::atomic<CounterT> m_next_id;

    private:
        const CounterT m_id;

    public:
        Session() : m_id(m_next_id.fetch_add(1)) {
            ++m_total_objects;
            if (Log::is_enabled())
                Log::debug(boost::format("Created session #%1% (total: %2%)") % m_id % m_total_objects);
        }

        ~Session() {
            --m_total_objects;
            if (Log::is_enabled())
                Log::debug(boost::format("Deleted session %1% (total: %2%)") % m_id % m_total_objects);
        }

    public:
        std::string name_as_prefix() const {
            return "[Session #" + std::to_string(id()) + "] ";
        }

        CounterT id() const noexcept {
            return m_id;
        }

        static CounterT total_objects() noexcept {
            return m_total_objects;
        }
    };
    std::atomic<Session::CounterT> Session::m_total_objects(0);
    std::atomic<Session::CounterT> Session::m_next_id(1);

    class Server : public std::enable_shared_from_this<Server>
    {
    private:
        std::shared_ptr<Config> m_config;
        bool m_is_shutdown_requested = false;
        std::shared_ptr<boost::asio::io_context> m_io_context;
        boost::asio::ip::tcp::acceptor m_acceptor;

    public:
        Server(
            const std::shared_ptr<Config>& config,
            const std::shared_ptr<boost::asio::io_context>& io_context) :
            m_config(config),
            m_io_context(io_context),
            m_acceptor(*m_io_context) {
        }

    public:
        void start(const boost::asio::ip::tcp::endpoint& listen) {
            Log::info("Listen: " + listen.address().to_string() + ':' + std::to_string(listen.port()));
            m_acceptor.open(boost::asio::ip::tcp::v4());
            // Option "reuse address" must be set in order to allow second instance after shutdown this one
            m_acceptor.set_option(boost::asio::ip::tcp::socket::reuse_address(true));
            m_acceptor.bind(listen);
            m_acceptor.listen();
            boost::asio::spawn(*m_io_context, std::bind(&Server::accept, shared_from_this(), std::placeholders::_1));
        }

    private:
        void accept(boost::asio::yield_context yield)
        {
            auto& io_context = *m_io_context;
            boost::asio::ip::tcp::socket client(io_context);

            boost::system::error_code error;
            m_acceptor.async_accept(client, yield[error]);
            if (!m_is_shutdown_requested)
                boost::asio::spawn(io_context, std::bind(&Server::accept, shared_from_this(), std::placeholders::_1));
            if (error) {
                if (Log::is_enabled() && error != boost::asio::error::operation_aborted)
                    Log::error("Cannot accept client: " + error.message());
                return;
            }

            try {
                Session session;
                try {
                    serve(session, client, yield);
                } catch (const std::exception& e) {
                    if (Log::is_enabled())
                        Log::error(session.name_as_prefix() + "Interrupted: " + e.what());
                }
            } catch (...) {
            }
        }

        void serve(const Session& session, boost::asio::ip::tcp::socket& peer_sock, boost::asio::yield_context& yield)
        {
            auto gen_log_prefix = [&session]() { return session.name_as_prefix(); };
            if (Log::is_enabled())
                Log::info(gen_log_prefix() + "Initiator: " + get_string_remote_endpoint(peer_sock));

            std::string error_text;
            std::array<char, haproxy_protocol::tcp4::kMaximalHeaderSize> buffer;
            auto recv_result = recv_proxy_header(peer_sock, yield, buffer, Log::is_enabled() ? &error_text : nullptr);
            const auto& proxy_header = recv_result.first;
            if (!proxy_header.is_initialized()) {
                if (Log::is_enabled())
                    Log::error(gen_log_prefix() + "Cannot receive proxy header: " + error_text);
                return;
            }
            const auto& payload = recv_result.second;

            auto& io_context = peer_sock.get_executor().context();
            boost::asio::ip::tcp::socket ds_sock(io_context, boost::asio::ip::tcp::v4());
            auto set_ds_sock_opt_int = [&ds_sock](int level, int optname, int optvalue) {
                if (::setsockopt(ds_sock.native_handle(), level, optname, &optvalue, sizeof(optvalue)) < 0) {
                    auto error = errno;
                    throw std::runtime_error("Cannot set option " + std::to_string(optname)
                                             + " to value " + std::to_string(optvalue)
                                             + " for the socket: " + strerror(error)
                                             + (error == EPERM ? " (need to be root)" : ""));
                }
            };
            // Make sure it will fail fast. There is no packet loss on loopback.
            set_ds_sock_opt_int(IPPROTO_TCP, TCP_SYNCNT, 2);
            set_ds_sock_opt_int(IPPROTO_IP, IP_TRANSPARENT, 1);
            // We can live without SO_REUSEADDR. But, since we are doing bind-before-connect, the 5-tuple will go into
            set_ds_sock_opt_int(SOL_SOCKET, SO_REUSEADDR, 1);
            set_ds_sock_opt_int(SOL_SOCKET, SO_MARK, m_config->mark);

            // Bind-before-connect to select source IP.
            auto client_ip = boost::asio::ip::address_v4(proxy_header->src_ip.host);
            ds_sock.bind({client_ip, proxy_header->src_port});

            auto dst_ip = boost::asio::ip::address_v4::loopback();
            boost::asio::ip::tcp::endpoint dst_endpoint(dst_ip, proxy_header->dst_port);
            ds_sock.async_connect(dst_endpoint, yield);

            if (!payload.empty())
                boost::asio::async_write(ds_sock, boost::asio::buffer(payload.data(), payload.size()), yield);

            boost::asio::spawn(io_context, std::bind(
                &Server::transmit_payload,
                shared_from_this(),
                session.id(),
                std::ref(peer_sock),
                std::ref(ds_sock),
                std::placeholders::_1));
            transmit_payload(session.id(), ds_sock, peer_sock, yield);
        }

        static
        std::pair<boost::optional<haproxy_protocol::Header>, boost::string_view>
        recv_proxy_header(
            boost::asio::ip::tcp::socket& peer_sock,
            boost::asio::yield_context& yield,
            std::array<char, haproxy_protocol::tcp4::kMaximalHeaderSize>& buffer,
            std::string* error_buffer = nullptr)
        {
            static const boost::posix_time::minutes kTimeToReceiveProxyHeader(1);
            auto& io_context = peer_sock.get_executor().context();
            boost::asio::deadline_timer timer(io_context);
            timer.expires_from_now(kTimeToReceiveProxyHeader);
            timer.async_wait([&peer_sock](const boost::system::error_code& error) {
                if (error != boost::asio::error::operation_aborted)
                    peer_sock.cancel();
            });

            size_t actual_buffer_size = 0;
            size_t proxy_header_size = 0;
            while (actual_buffer_size < buffer.size())
            {
                boost::system::error_code error;
                auto free_space = buffer.size() - actual_buffer_size;
                auto bytes_received = peer_sock.async_read_some(
                    boost::asio::buffer(&buffer.at(actual_buffer_size), free_space),
                    yield[error]);
                if (error) {
                    if (error_buffer)
                        *error_buffer = error.message();
                    return {};
                }
                actual_buffer_size += bytes_received;
                boost::string_view header(buffer.data(), actual_buffer_size);
                auto end_marker_pos = header.find(haproxy_protocol::kEndMarker);
                if (end_marker_pos != boost::string_view::npos) {
                    proxy_header_size = end_marker_pos;
                    // Replace end-marker with null, because futher processing will use string_view
                    buffer.at(proxy_header_size) = 0;
                    break;
                }
            }
            timer.cancel();

            haproxy_protocol::Header parsed_header;
            // Replace all space and end-marker with nulls, because further processing will use string_view
            for (size_t i = 0; i < proxy_header_size; ++i) {
                if (buffer[i] == ' ')
                    buffer[i] = 0;
            }
            size_t parsed_data_size = 0;
            for (size_t index = 1; index <= 6; ++index) {
                boost::string_view value(&buffer.at(parsed_data_size));
                switch (index)
                {
                case 1:
                    if (value != "PROXY") {
                        if (error_buffer)
                            *error_buffer = "Not a PROXY protocol header";
                        return {};
                    }
                    break;
                case 2:
                    if (value != "TCP4") {
                        if (error_buffer)
                            *error_buffer = "Only TCP4 is supported";
                        return {};
                    }
                    break;
                case 3:
                    parsed_header.src_ip = IP_Host(value.data());
                    break;
                case 4:
                    parsed_header.dst_ip = IP_Host(value.data());
                    break;
                case 5:
                    parsed_header.src_port = boost::lexical_cast<uint16_t>(value.data(), value.size());
                    break;
                case 6:
                    parsed_header.dst_port = boost::lexical_cast<uint16_t>(value.data(), value.size());
                    break;
                }
                parsed_data_size += value.size() + 1;
            }
            auto payload_begin = parsed_data_size + 1;
            auto payload_size = actual_buffer_size - payload_begin;
            if (payload_begin + payload_size != actual_buffer_size)
                throw std::logic_error("Buffer overflow on access to payload after Proxy Header");
            boost::string_view payload(&buffer.at(payload_begin), payload_size);
            return std::make_pair(parsed_header, payload);
        }

        static std::string get_string_remote_endpoint(const boost::asio::ip::tcp::socket& sock) {
            auto remote_endpoint = sock.remote_endpoint();
            return (remote_endpoint.address().to_string() + ':' + std::to_string(remote_endpoint.port()));
        }

        void transmit_payload(
            size_t session_id,
            boost::asio::ip::tcp::socket& src_sock,
            boost::asio::ip::tcp::socket& dst_sock,
            boost::asio::yield_context yield)
        {
            try {
                std::vector<char> buffer(8192);
                while (true)
                {
                    boost::system::error_code error;
                    auto bytes_received = src_sock.async_read_some(boost::asio::buffer(buffer.data(), buffer.size()), yield[error]);
                    if (error) {
                        if (error == boost::asio::error::eof)
                            dst_sock.shutdown(boost::asio::socket_base::shutdown_send, error);
                        else if (error != boost::asio::error::operation_aborted)
                            dst_sock.shutdown(boost::asio::socket_base::shutdown_both, error);
                        break;
                    }

                    boost::asio::async_write(dst_sock, boost::asio::buffer(buffer.data(), bytes_received), yield[error]);
                    if (error) {
                        src_sock.shutdown(boost::asio::socket_base::shutdown_receive, error);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                if (Log::is_enabled())
                    Log::error("[Session #%" + std::to_string(session_id) + "] Cannot transmit payload: " + e.what());
            } catch (...) {
            }
        }

    public:
        void shutdown() noexcept
        {
            try
            {
                if (!m_is_shutdown_requested)
                {
                    m_is_shutdown_requested = true;
                    try { m_acceptor.close(); } catch (...) {}

                    auto total_sessions = Session::total_objects();
                    if (total_sessions > 0)
                        Log::info("There are " + std::to_string(total_sessions) + " proxified connections. Waiting for finish...");
                }
            } catch (...) {}
        }
    };
}

int main(int argc, char** argv)
{
    using namespace progdn;
    Log::Deleter log_deleter;
    try {
        auto& log = Log::create_instance();
        log->emplace_writer<SystemLog>("progdn-rvi");

        CommandLineInterface cli(argc, argv);
        auto config = std::make_shared<progdn::Config>(cli.conf);
        if (cli.is_option_specified(cli.kOption_Background))
            if (::daemon(0, 0) != 0)
                throw std::runtime_error(std::string("Cannot run process in background: ") + ::strerror(errno));

        SystemLimits::unlimit_open_files_number();

        auto io_context = std::make_shared<boost::asio::io_context>();
        auto server = std::make_shared<progdn::Server>(config, io_context);
        server->start(config->listen);

        boost::asio::signal_set unix_signals(*io_context, SIGTERM);
        unix_signals.async_wait([server](const boost::system::error_code& error, int) {
            if (error != boost::asio::error::operation_aborted) {
                Log::info("Received SIGTERM");
                server->shutdown();
            }
        });

        if (!cli.is_option_specified(cli.kOption_Verbose))
            Log::delete_instance();
        io_context->run();

        return 0;
    } catch (const std::exception& e) {
        if (Log::is_enabled())
            Log::error(e.what());
        else
            std::cerr << e.what() << std::endl;
        return 1;
    }
}
