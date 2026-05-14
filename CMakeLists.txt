cmake_minimum_required(VERSION 3.16)
project(macro_risk_engine
    VERSION 0.1.0
    DESCRIPTION "Live macro data driven risk/direction engine for individual assets"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

include(FetchContent)

# --- nlohmann/json (header only) ---------------------------------------------
find_package(nlohmann_json 3.7.0 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(json)
endif()

# --- libcurl -----------------------------------------------------------------
find_package(CURL REQUIRED)

# --- Threads -----------------------------------------------------------------
find_package(Threads REQUIRED)

# --- Library + executable ----------------------------------------------------
set(ENGINE_SOURCES
    src/util/http_client.cpp
    src/util/time_utils.cpp
    src/util/logger.cpp
    src/models.cpp
    src/config.cpp
    src/data/fred_client.cpp
    src/data/yahoo_client.cpp
    src/data/coingecko_client.cpp
    src/data/econ_calendar.cpp
    src/data/price_repository.cpp
    src/engine/historical_analyzer.cpp
    src/engine/statistical_engine.cpp
    src/engine/monte_carlo.cpp
    src/engine/risk_calculator.cpp
    src/engine/scanner.cpp
    src/report/reporter.cpp)

add_library(macro_engine STATIC ${ENGINE_SOURCES})
target_include_directories(macro_engine PUBLIC src)
target_link_libraries(macro_engine
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC CURL::libcurl
    PUBLIC Threads::Threads)

add_executable(macro_risk src/main.cpp)
target_link_libraries(macro_risk PRIVATE macro_engine)

# --- Optional tests ----------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_statistics tests/test_statistics.cpp)
    target_link_libraries(test_statistics PRIVATE macro_engine)
    add_test(NAME statistics COMMAND test_statistics)
endif()

install(TARGETS macro_risk RUNTIME DESTINATION bin)
