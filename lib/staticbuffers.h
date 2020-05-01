#pragma once

#include <cstdint>

#define BUFFSIZE 1000

struct buff_static_t {
    char buff[BUFFSIZE] = {};
    int tail = 0;
    int head = 0;
};