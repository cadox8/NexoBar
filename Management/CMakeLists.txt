cmake_minimum_required(VERSION 3.24)
project(Management VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

set(MENUS
        menus/MenuBase.cpp
        menus/MainMenu.cpp
        menus/LicenseMenu.cpp
        menus/BusinessMenu.cpp
)

add_executable(${PROJECT_NAME}
        main.cpp
        Management.cpp
        ${MENUS}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        Platform
        ftxui::screen
        ftxui::dom
        ftxui::component
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Winspool)
endif ()
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()
