#pragma once

#include <map>
#include "wolfssl/ssl.h"
#include "events.h"

const int concurrentConnections = 2500;
const int connUrlSize = 256;
const int connBufferSize = 1 << 15U;

enum method_enum {
    GET, POST, DELETE
};

struct snow_global_t;

void snow_init(snow_global_t *global);

void snow_destroy(snow_global_t *global);

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t dataLen, void *extra), void *extra);

#define DISABLE_NAGLE

/////////////////////////////////////////////////////

#define BUFFSIZE connBufferSize

#define SNOW_LIKELY(x) __builtin_expect(!!(x), 1)
#define SNOW_UNLIKELY(x) __builtin_expect(!!(x), 0)

const char method_strings[3][10] = {"GET", "POST", "DELETE"};

enum conn_status_enum {
    CONN_UNREADY, // before connect()
    CONN_IN_PROGRESS, // connect() waiting for ACK
    CONN_ACK, // connect() received ACK
    CONN_TLS_HANDSHAKE, // wolfssl tls started
    CONN_READY, // ready to send http request
    CONN_WAITING,
    CONN_RECEIVING,
    CONN_DONE // received http response
};

struct buff_static_t {
    char buff[connBufferSize] = {};
    size_t tail = 0;
    size_t head = 0;
};

struct ev_io_snow {
    struct ev_io io;
    void *data;
};

struct snow_connection_t {
    char requestUrl[connUrlSize] = {};
    char *protocol = nullptr, *hostname = nullptr, *path = nullptr, *portPtr = nullptr;
    int port = 0;
    int method = 0;
    bool secure = false;

    struct addrinfo *addrinfo = nullptr;
    struct addrinfo hints = {};

    int sockfd = 0;
    int connectionStatus = 0;

    WOLFSSL *ssl{};

    struct ev_io_snow ior = {}, iow = {};

    buff_static_t writeBuff;
    buff_static_t readBuff;

    char *content = nullptr;
    size_t contentLen = 0;
    bool chunked = false;

    void *extra_cb = nullptr;

    void (*write_cb)(char *data, size_t data_len, void *extra){};

    snow_global_t *global{};
};

struct snow_global_t {
    ev_loop *loop = nullptr;
    WOLFSSL_CTX *wolfCtx = nullptr;

    struct ev_timer timer = {};
    std::map<std::string, struct addrinfo *> addrCache;

    int newConnId = 0;
    snow_connection_t connections[concurrentConnections];
};