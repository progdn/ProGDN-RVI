#pragma once

#include <memory>
#include <stdexcept>

namespace progdn
{
    template<typename T>
    class Singleton
    {       
    public:
        class Deleter
        {
        public:
            ~Deleter() {
                T::delete_instance();
            }
        };

    private:
        static std::unique_ptr<T> self;
        
    public:
        Singleton() = default;
        virtual ~Singleton() = default;
        
    public:
        template<typename... Arguments>
        static const std::unique_ptr<T>& create_instance(Arguments... arguments) {
            if (self)
                throw std::runtime_error("Singleton has been already called");
            return (self = std::unique_ptr<T>(new T(std::forward<Arguments>(arguments)...)));
        }

        static bool is_instance_created() noexcept {
            return (self != nullptr);
        }
        
        static const std::unique_ptr<T>& get_instance() {
            if (!self)
                throw std::runtime_error("Singleton has not been created yet");
            return self;
        }

        static const std::unique_ptr<T>& get_instance_or_null() {
            return self;
        }

        static void delete_instance() {
            self.reset();
        }
    };

    template<class T> std::unique_ptr<T> Singleton<T>::self(nullptr);
}
