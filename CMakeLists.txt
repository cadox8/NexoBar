cmake_minimum_required(VERSION 3.24)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

project(NexoBarCore)
cmake_policy(SET CMP0167 OLD)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if (NOT DEFINED VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
endif ()

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif ()

# Commons
add_subdirectory(Platform)

# Application
add_subdirectory(NexoBar)

# Debug
add_subdirectory(Management)

# Tests
option(NEXOBAR_BUILD_TESTS "Build NexoBar tests" ON)
if (NEXOBAR_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(NexoBarTests
            tests/OrmTests.cpp
            tests/HospitalitySchemaTests.cpp
            tests/SaleServiceTests.cpp
            NexoBar/sales/SaleService.cpp
    )
    target_compile_definitions(NexoBarTests PRIVATE
            NEXOBAR_TEST_LANGUAGE_DIR="${CMAKE_SOURCE_DIR}/NexoBar/resources/languages"
    )
    target_link_libraries(NexoBarTests PRIVATE Platform GTest::gtest_main)
    target_include_directories(NexoBarTests PRIVATE NexoBar)
    include(GoogleTest)
    gtest_discover_tests(NexoBarTests)
endif ()
