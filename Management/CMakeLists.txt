cmake_minimum_required(VERSION 3.24)
project(Management VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ftxui CONFIG QUIET)
if (NOT TARGET ftxui::component)
    include(FetchContent)
    FetchContent_Declare(
            ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
            GIT_TAG v7.0.3
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ftxui)
endif ()

set(MENUS
        menus/MenuBase.cpp
        menus/MainMenu.cpp
        menus/LicenseMenu.cpp
        menus/BusinessMenu.cpp
        menus/SimulationMenu.cpp
)

add_executable(${PROJECT_NAME}
        main.cpp
        Management.cpp
        ../NexoBar/sales/SaleService.cpp
        ../NexoBar/fiscal/FiscalService.cpp
        ${MENUS}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../NexoBar)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Platform
        ftxui::screen
        ftxui::dom
        ftxui::component
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
