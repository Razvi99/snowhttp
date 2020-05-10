cmake_minimum_required(VERSION 3.15)
project(snowhttp)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "-lpthread")

file(GLOB SOURCES
        include ./lib/*.h
        include ./lib/*.cpp
        )

include_directories(snowhttp "lib/*" "./lib/wolf/wolfssl/")

find_package (Threads)

message("SOURCES = ${SOURCES}")
add_executable(snowhttp example.cpp ${SOURCES} lib/atomic.h)

target_link_libraries(snowhttp ${PROJECT_SOURCE_DIR}/lib/wolf/libwolfssl.a ${CMAKE_THREAD_LIBS_INIT})