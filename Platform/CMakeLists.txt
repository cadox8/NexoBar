cmake_minimum_required(VERSION 3.24)

project(Platform VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time serialization)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)

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
        LibXml2::LibXml2
        OpenSSL::Crypto
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Crypt32 Ncrypt Bcrypt Advapi32)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME_DEPENDENCY_SET nexobar_runtime_dependencies
        RUNTIME DESTINATION . COMPONENT NexoBarRuntime
        LIBRARY DESTINATION . COMPONENT NexoBarRuntime
        ARCHIVE DESTINATION lib COMPONENT NexoBarDevelopment
)
