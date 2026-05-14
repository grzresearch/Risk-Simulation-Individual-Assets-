cmake_minimum_required(VERSION 3.16)
project(macro_risk_engine
    VERSION 0.2.0
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

# -- Version / git provenance baked into the binary --------------------------
execute_process(COMMAND git rev-parse --short=12 HEAD
                OUTPUT_VARIABLE MACRO_GIT_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(NOT MACRO_GIT_SHA)
    set(MACRO_GIT_SHA "unknown")
endif()
add_compile_definitions(
    MACRO_VERSION="${PROJECT_VERSION}"
    MACRO_GIT_SHA="${MACRO_GIT_SHA}")

include(FetchContent)

# --- nlohmann/json (header only) --------------------------------------------
find_package(nlohmann_json 3.7.0 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(json)
endif()

# --- cpp-httplib (header only) ----------------------------------------------
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3)
FetchContent_MakeAvailable(httplib)

# --- libcurl -----------------------------------------------------------------
find_package(CURL REQUIRED)

# --- Threads / OpenMP --------------------------------------------------------
find_package(Threads REQUIRED)
find_package(OpenMP)

# --- Optional libpq (Postgres) ----------------------------------------------
find_package(PostgreSQL QUIET)

# --- Library + executable ----------------------------------------------------
set(ENGINE_SOURCES
    src/util/http_client.cpp
    src/util/time_utils.cpp
    src/util/holiday_calendar.cpp
    src/util/logger.cpp
    src/util/json_logger.cpp
    src/util/thread_pool.cpp
    src/models.cpp
    src/config.cpp
    src/data/fred_client.cpp
    src/data/yahoo_client.cpp
    src/data/coingecko_client.cpp
    src/data/econ_calendar.cpp
    src/data/price_repository.cpp
    src/data/consensus_provider.cpp
    src/engine/historical_analyzer.cpp
    src/engine/statistical_engine.cpp
    src/engine/monte_carlo.cpp
    src/engine/risk_calculator.cpp
    src/engine/scanner.cpp
    src/engine/garch.cpp
    src/engine/regime.cpp
    src/engine/multitest.cpp
    src/engine/calibration.cpp
    src/engine/surprise.cpp
    src/engine/linalg.cpp
    src/engine/portfolio_risk.cpp
    src/engine/stress.cpp
    src/engine/backtest.cpp
    src/persist/json_store.cpp
    src/persist/postgres_store.cpp
    src/server/metrics.cpp
    src/server/http_server.cpp
    src/alerts/slack_alerter.cpp
    src/governance/audit_log.cpp
    src/governance/model_card.cpp
    src/governance/rbac.cpp
    src/report/reporter.cpp
    src/report/csv_reporter.cpp
    src/report/html_reporter.cpp)

add_library(macro_engine STATIC ${ENGINE_SOURCES})
target_include_directories(macro_engine PUBLIC src)
target_link_libraries(macro_engine
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC CURL::libcurl
    PUBLIC Threads::Threads
    PUBLIC httplib::httplib)

if(OpenMP_CXX_FOUND)
    target_link_libraries(macro_engine PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP enabled")
endif()

if(PostgreSQL_FOUND)
    target_compile_definitions(macro_engine PRIVATE MACRO_HAVE_LIBPQ=1)
    target_include_directories(macro_engine PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(macro_engine PUBLIC ${PostgreSQL_LIBRARIES})
    message(STATUS "Postgres backend enabled (libpq found)")
else()
    message(STATUS "Postgres backend disabled (libpq not found)")
endif()

add_executable(macro_risk src/main.cpp)
target_link_libraries(macro_risk PRIVATE macro_engine)

# --- Optional tests ----------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_statistics tests/test_statistics.cpp)
    target_link_libraries(test_statistics PRIVATE macro_engine)
    add_test(NAME statistics COMMAND test_statistics)

    add_executable(test_advanced tests/test_advanced.cpp)
    target_link_libraries(test_advanced PRIVATE macro_engine)
    add_test(NAME advanced COMMAND test_advanced)

    add_executable(test_production tests/test_production.cpp)
    target_link_libraries(test_production PRIVATE macro_engine)
    add_test(NAME production COMMAND test_production)
endif()

install(TARGETS macro_risk RUNTIME DESTINATION bin)
