#pragma once

#include <cstdint>

#define BUFFSIZE (1<<22U)

struct buff_static_t {
    char buff[BUFFSIZE] = {};
    size_t tail = 0;
    size_t head = 0;
};

int buff_to_pull(struct buff_static_t *buff);

bool buff_empty(struct buff_static_t *buff);

size_t buff_put_from_sock(struct buff_static_t *buff, void *f, int size, bool ssl);

bool buff_put(struct buff_static_t *buff, void *dest, size_t size);

bool buff_put(struct buff_static_t *buff, char *data, size_t dataSize);

size_t buff_pull_to_sock(struct buff_static_t *buff, void *f, size_t size, bool ssl);



