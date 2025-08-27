# EN: CMake configuration for BB-Pipeline tests
# FR: Configuration CMake pour les tests BB-Pipeline

cmake_minimum_required(VERSION 3.20)

# --- Find Google Test
find_package(GTest REQUIRED)

# --- Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# --- Test executables
set(TEST_SOURCES
    test_memory_manager.cpp
    test_error_recovery.cpp
    test_schema_validator.cpp
    test_streaming_parser.cpp
    test_batch_writer.cpp
    test_delta_compression.cpp
    test_query_engine.cpp
    test_pipeline_engine.cpp
    test_resume_system.cpp
    test_dry_run_system.cpp
    test_progress_monitor.cpp
    test_resource_monitor.cpp
)

# --- Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    string(REPLACE ".cpp" "" TEST_NAME ${TEST_SOURCE})
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    target_link_libraries(${TEST_NAME}
        bbp-core
        GTest::GTest
        GTest::Main
        pthread
    )
    
    # --- Add to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# --- Memory Manager integration test (simple test without GoogleTest)
add_executable(test_memory_manager_simple test_memory_manager_simple.cpp)
target_link_libraries(test_memory_manager_simple bbp-core)
add_test(NAME test_memory_manager_simple COMMAND test_memory_manager_simple)

# --- Error Recovery integration test (simple test without GoogleTest)
add_executable(test_error_recovery_simple test_error_recovery_simple.cpp)
target_link_libraries(test_error_recovery_simple bbp-core)
add_test(NAME test_error_recovery_simple COMMAND test_error_recovery_simple)

# --- Merger Engine simple test (without GoogleTest)
add_executable(test_merger_engine_simple test_merger_engine_simple.cpp)
target_link_libraries(test_merger_engine_simple bbp-core)
add_test(NAME test_merger_engine_simple COMMAND test_merger_engine_simple)

# --- Integration test (if main.cpp exists)
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp)
#     add_executable(test_integration test_integration.cpp)
#     target_link_libraries(test_integration
#         bbp-core
#         GTest::GTest
#         GTest::Main
#         pthread
#     )
#     add_test(NAME test_integration COMMAND test_integration)
# endif()

# --- Coverage target (if enabled)
if(ENABLE_COVERAGE)
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --list coverage.info
            COMMAND ${GENHTML_PATH} coverage.info --output-directory coverage_html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
        )
    endif()
endif()

# --- Benchmark executables (if enabled)
if(BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    
    set(BENCHMARK_SOURCES
        benchmark_logger.cpp
        benchmark_rate_limiter.cpp
        benchmark_cache_system.cpp
        benchmark_thread_pool.cpp
        benchmark_signal_handler.cpp
    )
    
    foreach(BENCHMARK_SOURCE ${BENCHMARK_SOURCES})
        string(REPLACE ".cpp" "" BENCHMARK_NAME ${BENCHMARK_SOURCE})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${BENCHMARK_SOURCE})
            add_executable(${BENCHMARK_NAME} ${BENCHMARK_SOURCE})
            target_link_libraries(${BENCHMARK_NAME}
                bbp-core
                benchmark::benchmark
                benchmark::benchmark_main
            )
        endif()
    endforeach()
endif()

# --- Custom test target for all tests
add_custom_target(run_all_tests
    COMMAND ctest --output-on-failure
    DEPENDS ${TEST_SOURCES}
    COMMENT "Running all tests"
)