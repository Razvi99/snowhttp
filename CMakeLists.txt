cmake_minimum_required(VERSION 3.15)
project(snowhttp)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
#set(CMAKE_CXX_FLAGS "-lrt")

file(GLOB SOURCES
        include ./lib/*.h
        include ./lib/*.cpp
        include ./lib/wolf/wolfssl-4.2.0/*
        )

include_directories(snowhttp "lib/*" "./lib/wolf/wolfssl-4.2.0/")

message("SOURCES = ${SOURCES}")
add_executable(snowhttp main.cpp ${SOURCES})

target_link_libraries(snowhttp /home/razvi/Documents/quant/snowhttp/lib/wolf/libwolfssl.a)