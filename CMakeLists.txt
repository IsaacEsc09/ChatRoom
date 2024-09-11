cmake_minimum_required(VERSION 3.10)

project(ChatRoom)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(ExternalProject)

set(JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/libs")

ExternalProject_Add(
    jsoncpp
    PREFIX ${CMAKE_BINARY_DIR}/jsoncpp
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG master
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

if(EXISTS "${JSON_INCLUDE_DIR}/json.hpp")
    include_directories(${JSON_INCLUDE_DIR})
else()
    ExternalProject_Get_Property(jsoncpp INSTALL_DIR)
    include_directories(${INSTALL_DIR}/include)
endif()

add_executable(servidor src/modelado/Servidor.cpp)
add_executable(cliente src/modelado/Cliente.cpp)
