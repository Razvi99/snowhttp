#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <cerrno>
#include "staticbuffers.h"
#include <unistd.h>
#include <climits>
#include <iostream>

bool buff_put(struct buff_static_t *buff, char *data, size_t dataSize) {
    if (buff->head + dataSize > BUFFSIZE)
        return false;

    memcpy(&buff->buff[buff->head], data, dataSize);
    buff->head += dataSize;
}


bool buff_pull(struct buff_static_t *buff, void *dest, size_t size) {
    if (buff->tail + size > BUFFSIZE)
        return false;

    buff->tail += size;
    memcpy(dest, &buff->buff[buff->tail], size);
    return true;
}

int buff_to_pull(struct buff_static_t *buff) {
    return buff->head - buff->tail;
}

bool buff_empty(struct buff_static_t *buff) {
    return buff->head == buff->tail;
}

size_t buff_pull_to_fd(struct buff_static_t *buff, const int &fd, size_t size, int (*wr)(int fd, void *buf, size_t count, void *arg), void *arg) {

    size_t remain;

    if (buff->tail + size > BUFFSIZE)
        assert(0);

    remain = size;

    while (remain > 0) {
        ssize_t ret;

        if (wr) {
            ret = wr(fd, &buff->buff[buff->tail], remain, arg);
            if (ret == -1) // error
                return -1;
            else if (ret == EWOULDBLOCK) // pending
                break;
        } else {
            ret = write(fd, &buff->buff[buff->tail], remain);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN)
                    break;

                return -1;
            }
        }

        remain -= ret;
        buff->tail += ret;
    }

    return size - remain;
}

size_t buff_put_from_fd(struct buff_static_t *buff, const int &fd, int size, bool *eof, int (*rd)(int fd, void *buf, size_t count, void *arg), void *arg) {

    size_t remain;

    if (size < 0)
        size = INT_MAX;

    remain = size;
    *eof = false;

    while (remain) {
        ssize_t ret;
        size_t head_room = BUFFSIZE - buff->head;

        // TODO: add unlikely?
        if (!head_room) {
            std::cerr << "no head space put fd\n";
            assert(0);
        }

        if (rd) {
            ret = rd(fd, &buff->buff[buff->head], head_room, arg);
            if (ret == -1) // error
                return -1;
            else if (ret == EWOULDBLOCK) // pending
                break;
        } else {
            ret = read(fd, &buff->buff[buff->head], head_room);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == ENOTCONN || errno == EWOULDBLOCK)
                    break;

                assert(0);
            }

        }

        if (!ret) {
            *eof = true;
            break;
        }

        assert(ret > 0);

        buff->head += ret;
        remain -= ret;

    }
    return size - remain;
}