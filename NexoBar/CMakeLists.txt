cmake_minimum_required(VERSION 3.24)
project(NexoBar VERSION 0.6.0 LANGUAGES CXX)

if (POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif ()

if (WIN32)
    enable_language(RC)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NEXOBAR_ES_CATALOG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/languages/es-ES.json")
set(NEXOBAR_EN_CATALOG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/languages/en-US.json")
file(READ "${NEXOBAR_ES_CATALOG_PATH}" NEXOBAR_ES_CATALOG)
file(READ "${NEXOBAR_EN_CATALOG_PATH}" NEXOBAR_EN_CATALOG)
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/languages/EmbeddedLanguages.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedLanguages.h"
        @ONLY
)

find_package(SQLite3 REQUIRED)
find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0 REQUIRED)
find_library(LIBUSB_LIBRARY NAMES usb-1.0 libusb-1.0 libusb REQUIRED)
add_library(NexoBarLibUsb UNKNOWN IMPORTED)
set_target_properties(NexoBarLibUsb PROPERTIES
        IMPORTED_LOCATION "${LIBUSB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_INCLUDE_DIR}"
)

include(FetchContent)
option(NEXOBAR_FETCH_RAYLIB "Fetch raylib when no package is installed" ON)
find_package(raylib CONFIG QUIET)
if (NOT TARGET raylib AND NOT TARGET raylib::raylib)
    if (NEXOBAR_FETCH_RAYLIB)
        set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
        set(SUPPORT_MODULE_RAUDIO ON CACHE BOOL "Build raylib audio support" FORCE)
        if (UNIX AND NOT APPLE)
            set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW Wayland support" FORCE)
            set(GLFW_BUILD_X11 ON CACHE BOOL "Build GLFW X11 support" FORCE)
        endif ()
        FetchContent_Declare(raylib
                GIT_REPOSITORY https://github.com/raysan5/raylib.git
                GIT_TAG 6.2
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif ()
endif ()

if (TARGET raylib)
    set(NEXOBAR_RAYLIB_TARGET raylib)
elseif (TARGET raylib::raylib)
    set(NEXOBAR_RAYLIB_TARGET raylib::raylib)
else ()
    message(FATAL_ERROR "raylib was not found; install its CMake package or enable NEXOBAR_FETCH_RAYLIB")
endif ()

# Package-provided imported targets are directory-scoped by default. Promote
# raylib so the root-level regression tests can link the same dependency.
get_target_property(NEXOBAR_RAYLIB_IMPORTED ${NEXOBAR_RAYLIB_TARGET} IMPORTED)
if (NEXOBAR_RAYLIB_IMPORTED)
    set_property(TARGET ${NEXOBAR_RAYLIB_TARGET} PROPERTY IMPORTED_GLOBAL TRUE)
endif ()

set(BACKEND
        printer/TicketPrinter.cpp
        printer/PrinterDiagnostics.cpp
        printer/UsbPrinterTransport.cpp
        sales/SaleService.cpp
        reporting/DailyCloseReceipt.cpp
        template/TicketTemplate.cpp
)

file(GLOB_RECURSE FRONTEND CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/components/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/screen/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/manager/*.cpp"
)

add_executable(${PROJECT_NAME}
        main.cpp
        NexoBar.cpp
        ${BACKEND}
        ${FRONTEND}
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME_DEPENDENCY_SET nexobar_runtime_dependencies
        RUNTIME DESTINATION . COMPONENT NexoBarRuntime)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/"
        DESTINATION resources COMPONENT NexoBarRuntime)

# Bundle only runtime libraries actually referenced by NexoBar.
set(NEXOBAR_RUNTIME_SEARCH_DIRECTORIES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if (WIN32)
    get_filename_component(NEXOBAR_VCPKG_LIB_DIR "${LIBUSB_LIBRARY}" DIRECTORY)
    get_filename_component(NEXOBAR_VCPKG_TRIPLET_DIR "${NEXOBAR_VCPKG_LIB_DIR}" DIRECTORY)
    list(APPEND NEXOBAR_RUNTIME_SEARCH_DIRECTORIES "${NEXOBAR_VCPKG_TRIPLET_DIR}/bin")
endif ()
install(RUNTIME_DEPENDENCY_SET nexobar_runtime_dependencies
        DIRECTORIES ${NEXOBAR_RUNTIME_SEARCH_DIRECTORIES}
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES
        ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\].*"
        "^/lib/.*" "^/lib64/.*" "^/usr/lib/.*" "^/usr/lib64/.*"
        DESTINATION .
        COMPONENT NexoBarRuntime)

# Re-run the post-build resource copy when a runtime asset or ticket template changes.
file(GLOB_RECURSE NEXOBAR_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${NEXOBAR_RESOURCE_FILES})

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    if (MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup)
    endif ()
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo/NexoBar.ico"
            "${CMAKE_CURRENT_BINARY_DIR}/NexoBar.ico"
            COPYONLY
    )
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/NexoBar.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/NexoBar.rc"
            @ONLY
    )
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/NexoBar.rc")
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Platform
        ${NEXOBAR_RAYLIB_TARGET}
        NexoBarLibUsb
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMENT "Copying NexoBar resources"
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()
