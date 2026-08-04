cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0167 NEW)
project(tradingbot_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# Crow (framework HTTP/WebSocket header-only) récupéré automatiquement au premier
# "cmake .." — nécessite une connexion internet la première fois seulement.
include(FetchContent)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.3.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Crow)

include_directories(
    include
    ${Boost_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIRS}
)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(tradingbot_engine ${SOURCES})

target_link_libraries(tradingbot_engine
    Crow::Crow
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${PQXX_LIBRARIES}
)

target_compile_options(tradingbot_engine PRIVATE -Wall -Wextra -O2)
