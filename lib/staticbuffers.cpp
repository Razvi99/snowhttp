#include <cstdlib>
#include <cstring>
#include "staticbuffers.h"

bool buff_push(struct buff_static_t *buff, char *data, size_t dataSize) {
    if (buff->head + dataSize > BUFFSIZE)
        return false;

    memcpy(&buff->buff[buff->head], data, dataSize);
    buff->head += dataSize;
}

char *buff_pull(struct buff_static_t *buff, size_t size){
    if(buff->tail + size > BUFFSIZE)
        return nullptr;

    buff->tail += size;
    return &buff->buff[buff->tail - size];
}

