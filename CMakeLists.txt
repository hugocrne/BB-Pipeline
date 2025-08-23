cmake_minimum_required(VERSION 3.20)
project(bb-pipeline VERSION 1.0.0 LANGUAGES CXX)

# Configuration C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options de build
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

# Configuration des flags selon le type de build
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

# Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Répertoires include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Recherche des dépendances
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

# Dépendances optionnelles pour démarrer (seront activées progressivement)
# Pour l'instant, on compile juste avec les dépendances de base

# Création des répertoires
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/bbp)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/configs)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wordlists)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/schemas)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Bibliothèque core partagée
add_library(bbp-core STATIC
    src/core/config.cpp
    src/core/csv.cpp
    src/core/logger.cpp
    src/core/rate_limit.cpp
    src/core/scope.cpp
    src/core/http.cpp
    src/core/tls.cpp
    src/core/cache.cpp
    src/core/fingerprints.cpp
    src/core/soft404.cpp
    src/core/util.cpp
)

# Linking des dépendances de base
target_link_libraries(bbp-core
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CURL_LIBRARIES}
    Threads::Threads
)

# Include directories
target_include_directories(bbp-core PRIVATE 
    ${CURL_INCLUDE_DIRS}
)

# Binaires des modules
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
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/main.cpp)
        add_executable(bbp-${MODULE}
            src/${MODULE}/main.cpp
            src/${MODULE}/${MODULE}.cpp
        )
        target_link_libraries(bbp-${MODULE} bbp-core)
    endif()
endforeach()

# Orchestrateur bbpctl
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/bbpctl/main.cpp)
    add_executable(bbpctl src/bbpctl/main.cpp)
    target_link_libraries(bbpctl bbp-core)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS bbpctl DESTINATION bin)
foreach(MODULE ${MODULES})
    if(TARGET bbp-${MODULE})
        install(TARGETS bbp-${MODULE} DESTINATION bin)
    endif()
endforeach()

install(DIRECTORY configs/ DESTINATION share/bb-pipeline/configs)
install(DIRECTORY wordlists/ DESTINATION share/bb-pipeline/wordlists)
install(DIRECTORY schemas/ DESTINATION share/bb-pipeline/schemas)
install(DIRECTORY scripts/ DESTINATION share/bb-pipeline/scripts
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)