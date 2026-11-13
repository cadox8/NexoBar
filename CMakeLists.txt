cmake_minimum_required(VERSION 3.24)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()
if (POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif ()

project(NexoBarCore VERSION 1.0.0)
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The language catalogs are embedded through file(READ). Register them once at
# the root so Ninja reconfigures both executables whenever either JSON changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/NexoBar/resources/languages/es-ES.json"
        "${CMAKE_SOURCE_DIR}/NexoBar/resources/languages/en-US.json")

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
