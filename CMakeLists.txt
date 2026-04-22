cmake_minimum_required(VERSION 3.26)
project(AlissonAsk VERSION 0.6 LANGUAGES CXX)

# ── C++23 ──────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── vcpkg dependencies ─────────────────────────────────────────
# Certifique-se de configurar com:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path>/vcpkg/scripts/buildsystems/vcpkg.cmake
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# ── Fontes do projeto ──────────────────────────────────────────
set(SOURCES
    main.cpp
    gemini_client.cpp
    conversation_manager.cpp
    chatbot_engine.cpp
    neural_network.cpp
    run_server_patch.cpp
)

# ── Executável principal ───────────────────────────────────────
add_executable(AlissonAsk ${SOURCES})

# ── Include dirs ───────────────────────────────────────────────
target_include_directories(AlissonAsk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Link libraries ─────────────────────────────────────────────
target_link_libraries(AlissonAsk PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ── Flags de compilação ────────────────────────────────────────
if(MSVC)
    target_compile_options(AlissonAsk PRIVATE /W4 /utf-8)
else()
    target_compile_options(AlissonAsk PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Habilita HTTPS no httplib ──────────────────────────────────
target_compile_definitions(AlissonAsk PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# ── Mensagem de build ──────────────────────────────────────────
message(STATUS "AlissonAsk V0.6 — Build configurado com C++23")
message(STATUS "Para rodar: ./AlissonAsk (demo) ou ./AlissonAsk --server")
