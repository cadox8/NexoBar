cmake_minimum_required(VERSION 3.24)

project(Platform VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time serialization)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS *.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE
        PLATFORM_EXPORT
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        SQLite3::SQLite3
        Boost::serialization
        Boost::date_time
        nlohmann_json::nlohmann_json
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
