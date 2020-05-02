cmake_minimum_required(VERSION 3.15)
project(snowhttp)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()


file(GLOB SOURCES
        include ./lib/*.h
        include ./lib/*.cpp
        )

include_directories(snowhttp "lib/*")

message("SOURCES = ${SOURCES}")
add_executable(snowhttp main.cpp ${SOURCES} lib/staticbuffers.cpp lib/staticbuffers.h)

target_link_libraries(snowhttp /home/razvi/Documents/quant/snowhttp/lib/wolf/libwolfssl.a)