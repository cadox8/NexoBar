cmake_minimum_required(VERSION 3.24)
project(NexoBar VERSION 0.1.0 LANGUAGES CXX)

if (WIN32)
    enable_language(RC)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

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

set(BACKEND
        printer/TicketPrinter.cpp
        printer/PrinterDiagnostics.cpp
        sales/SaleService.cpp
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

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        Platform
        ${NEXOBAR_RAYLIB_TARGET}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        COMMENT "Copying NexoBar resources"
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Winspool)
endif ()
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()
