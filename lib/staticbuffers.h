#pragma once

#include <cstdint>

#define BUFFSIZE (1<<14) // 16k

struct buff_static_t {
    char buff[BUFFSIZE] = {};
    int tail = 0;
    int head = 0;
};

int buff_to_pull(struct buff_static_t *buff);
bool buff_empty(struct buff_static_t *buff);

size_t buff_put_from_fd(struct buff_static_t *buff, const int &fd, int size, bool *eof, int (*rd)(int fd, void *buf, size_t count, void *arg), void *arg);
bool buff_put(struct buff_static_t *buff, void *dest, size_t size);
bool buff_put(struct buff_static_t *buff, char *data, size_t dataSize);
size_t buff_pull_to_fd(struct buff_static_t *buff, const int &fd, size_t size, int (*wr)(int fd, void *buf, size_t count, void *arg), void *arg);