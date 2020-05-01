cmake_minimum_required(VERSION 3.15)
project(snowhttp)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES
        include ./lib/*.h
        include ./lib/*.cpp
        )

include_directories(snowhttp "lib/*")

message("SOURCES = ${SOURCES}")
add_executable(snowhttp main.cpp ${SOURCES})