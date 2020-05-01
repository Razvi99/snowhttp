#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <cerrno>
#include "staticbuffers.h"
#include <unistd.h>
#include <climits>
#include <iostream>
#include <wolfssl/ssl.h>

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

size_t buff_pull_to_sock(struct buff_static_t *buff, void *f, size_t size, bool ssl) {
    size_t remain;

    if (buff->tail + size > BUFFSIZE) {
        std::cerr<<"ERR: send buffer too small\n";
        assert(0);
    }

    remain = size;

    while (remain > 0) {
        ssize_t ret;

        if (ssl) {
            ret = wolfSSL_write((WOLFSSL *) f, &buff->buff[buff->tail], remain);

            if (ret == -1) {
                int err = wolfSSL_get_error((WOLFSSL *) f, ret);

                if (err == WOLFSSL_ERROR_WANT_WRITE)
                    break;
                else {
                    char buffer[256];
                    fprintf(stderr, "sock put error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                    assert(0);
                }
            }
        } else {
            ret = write(*(int *) f, &buff->buff[buff->tail], remain);
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

    return remain;
}

size_t buff_put_from_sock(struct buff_static_t *buff, void *f, int size, bool *eof, bool ssl) {

    size_t remain;

    if (size < 0)
        size = INT_MAX;

    remain = size;
    *eof = false;

    while (remain) {
        ssize_t ret;
        size_t head_room = BUFFSIZE - buff->head;

        // TODO: add unlikely?
        if (head_room <= 0) {
            std::cerr << "ERR: receive buffer too small\n";
            assert(0);
        }

        if (ssl) {
            ret = wolfSSL_read((WOLFSSL *) f, &buff->buff[buff->head], head_room);

            if (ret == -1) {
                int err = wolfSSL_get_error((WOLFSSL *) f, ret);

                if (err == WOLFSSL_ERROR_WANT_READ)
                    break;
                else {
                    char buffer[256];
                    fprintf(stderr, "sock put error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                    assert(0);
                }
            }
        } else {
            ret = read(*(int *) f, &buff->buff[buff->head], head_room);
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
    return remain;
}