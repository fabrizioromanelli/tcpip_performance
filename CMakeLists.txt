cmake_minimum_required(VERSION 2.8)

project(tcpip_performance)
include_directories("include")
set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++14" )
set( CMAKE_VERBOSE_MAKEFILE ON )
add_executable(${PROJECT_NAME} "src/main.cpp")
