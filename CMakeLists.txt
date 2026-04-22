cmake_minimum_required(VERSION 3.26)
project(AlissonAsk VERSION 0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json QUIET)

set(SOURCES
    main.cpp
    gemini_client.cpp
    conversation_manager.cpp
    chatbot_engine.cpp
    whatsapp_client.cpp
)

# httplib precisa de uma unidade de compilação própria quando CPPHTTPLIB_COMPILE está ativo
# Como usamos header-only, removemos CPPHTTPLIB_COMPILE e deixamos inline

add_executable(AlissonAsk ${SOURCES})

target_include_directories(AlissonAsk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /usr/local/include
    /usr/include/nlohmann
)

target_compile_definitions(AlissonAsk PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(AlissonAsk PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    cpp-httplib
    pthread
)

if(nlohmann_json_FOUND)
    target_link_libraries(AlissonAsk PRIVATE nlohmann_json::nlohmann_json)
endif()
