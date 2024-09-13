cmake_minimum_required(VERSION 3.10)

project(ChatRoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(src/libs)

add_executable(servidor src/modelado/Servidor.cpp)

add_executable(cliente src/modelado/Cliente.cpp)
