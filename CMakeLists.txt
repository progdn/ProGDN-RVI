cmake_minimum_required(VERSION 2.8)
project(progdn_rvi)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# Enabled "warning: control reaches end of non-void function"
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wreturn-type")

SET(CMAKE_EXE_LINKER_FLAGS "-static")

if (DEFINED PROGDN_RVI_VERSION)
    add_definitions(-DPROGDN_RVI_VERSION="${PROGDN_RVI_VERSION}")
else()
    add_definitions(-DPROGDN_RVI_VERSION="dev")
endif()

find_package(Boost 1.69 COMPONENTS coroutine context date_time iostreams filesystem program_options system thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# Disable warning: Boost.ASIO 1.69 still uses deprecated Boost.Coroutine
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

# Add open source version of ProGDN Core
set(PROGDN_CORE_SRC ${CMAKE_SOURCE_DIR}/src/progdn_core)
include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(
    progdn-rvi
    ${PROGDN_CORE_SRC}/log.cpp
    ${PROGDN_CORE_SRC}/system_limits.cpp
    ${PROGDN_CORE_SRC}/system_log.cpp
    src/command_line_interface.cpp
    src/main.cpp)

target_link_libraries(progdn-rvi ${Boost_LIBRARIES} pthread rt)
