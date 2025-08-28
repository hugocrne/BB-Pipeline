cmake_minimum_required(VERSION 3.20)
project(bb-pipeline VERSION 1.0.0 LANGUAGES CXX)

# --- C++ config
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (ASan/UBSan/LSan)" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# --- Build types flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

# --- Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- Sanitizers (désactivés par défaut)
if(ENABLE_SANITIZERS)
  message(STATUS "Enabling sanitizers")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    # LSan n'est pas toujours dispo sur AppleClang; on reste minimal côté link:
    add_link_options(-fsanitize=address -fsanitize=undefined)
  endif()
endif()

# --- Coverage
if(ENABLE_COVERAGE)
  message(STATUS "Enabling code coverage")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
  endif()
endif()

# --- Dépendances
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)
find_package(nlohmann_json REQUIRED)

# --- Additional dependencies for Kill Switch
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRC32C crc32c)
if(NOT CRC32C_FOUND)
    # EN: Fall back to built-in implementation if crc32c not found
    # FR: Utiliser l'implémentation intégrée si crc32c n'est pas trouvé
    message(STATUS "crc32c not found, using built-in checksum implementation")
endif()

# --- Include directories for new modular structure
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Ne plus créer/forcer des dossiers src/* (tu gères l'arbo)
# (On garde uniquement les dossiers de données/outils)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/configs)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wordlists)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/schemas)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# --- Core library avec infrastructure modulaire
add_library(bbp-core
  src/infrastructure/logging/logger.cpp
  src/infrastructure/networking/rate_limiter.cpp
  src/infrastructure/networking/cache_system.cpp
  src/infrastructure/threading/thread_pool.cpp
  src/infrastructure/config/config_manager.cpp
  src/infrastructure/cli/config_override.cpp
  src/infrastructure/system/signal_handler.cpp
  src/infrastructure/system/memory_manager.cpp
  src/infrastructure/system/error_recovery.cpp
  src/csv/schema_validator.cpp
  src/csv/streaming_parser.cpp
  src/csv/batch_writer.cpp
  src/csv/merger_engine.cpp
  src/csv/delta_compression.cpp
  src/csv/query_engine.cpp
  src/orchestrator/pipeline_engine.cpp
  # src/orchestrator/resume_system.cpp
  # src/orchestrator/dry_run_system.cpp
  # src/orchestrator/progress_monitor.cpp
  # src/orchestrator/resource_monitor.cpp
  src/orchestrator/stage_selector.cpp
  src/orchestrator/kill_switch.cpp
)
target_link_libraries(bbp-core PUBLIC
  OpenSSL::SSL
  OpenSSL::Crypto
  CURL::libcurl
  Threads::Threads
  ZLIB::ZLIB
  ${YAMLCPP_LIBRARIES}
  nlohmann_json::nlohmann_json
)
target_link_directories(bbp-core PUBLIC ${YAMLCPP_LIBRARY_DIRS})
target_include_directories(bbp-core PUBLIC ${YAMLCPP_INCLUDE_DIRS})
target_include_directories(bbp-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Orchestrateur: binaire unique à partir de src/main.cpp
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
  add_executable(bbpctl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/types/scope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/types/scopeEntry.cpp
  )
  target_link_libraries(bbpctl PRIVATE bbp-core)
endif()

# --- Modules optionnels : créés seulement si src/<module>/main.cpp existe
set(MODULES
  subhunter
  httpxpp
  headless
  dirbff
  jsintel
  apiparser
  apitester
  mobile
  changes
  aggregator
)

foreach(MODULE ${MODULES})
  set(SRCS)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/main.cpp)
    list(APPEND SRCS src/${MODULE}/main.cpp)
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/${MODULE}.cpp)
    list(APPEND SRCS src/${MODULE}/${MODULE}.cpp)
  endif()
  if(SRCS)
    add_executable(bbp-${MODULE} ${SRCS})
    target_link_libraries(bbp-${MODULE} PRIVATE bbp-core)
  endif()
endforeach()

# --- Tests (optionnels)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --- Installation
if(TARGET bbpctl)
  install(TARGETS bbpctl DESTINATION bin)
endif()
foreach(MODULE ${MODULES})
  if(TARGET bbp-${MODULE})
    install(TARGETS bbp-${MODULE} DESTINATION bin)
  endif()
endforeach()

install(DIRECTORY configs/   DESTINATION share/bb-pipeline/configs)
install(DIRECTORY wordlists/ DESTINATION share/bb-pipeline/wordlists)
install(DIRECTORY schemas/   DESTINATION share/bb-pipeline/schemas)
install(DIRECTORY scripts/   DESTINATION share/bb-pipeline/scripts
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE)
