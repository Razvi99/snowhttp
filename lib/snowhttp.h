#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <netdb.h>
#include <wolfssl/ssl.h>
#include "events.h"
#include "staticbuffers.h"

const int concurrentConnections = 10;
const int connectionUrlSize = 256;
const int connectionRequestBuffSize = 1024;

enum method_enum {
    GET, POST, DELTE
};
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

struct ev_io_snow {
    struct ev_io io;
    void *data;
};

struct snow_global_t;

struct snow_connection_t {
    // protocol://hostname:port/path-and-file-name
    char requestUrl[connectionUrlSize] = {};
    char *protocol, *hostname, *path;
    int port = 0;
    int method = 0;
    bool secure = false;

    char requestBuff[connectionRequestBuffSize];

    struct hostent hostent;
    char hostentBuff[2048] = {};

    int sockfd, connfd;
    struct sockaddr_in address;
    int connectionStatus = 0;

    WOLFSSL *ssl;

    struct ev_io_snow ior, iow;

    buff_static_t writeBuff;
    buff_static_t readBuff;

    char *content;
    size_t contentLen = 0;
    bool chunked = false;
    bool chunkEnd = false;

    void *extra_cb;
    void (*write_cb)(char *data, void *extra);

    snow_global_t *global;
};

struct snow_global_t {
    ev_loop *loop;
    WOLFSSL_CTX *wolfCtx = nullptr;

    struct ev_timer timer;

    int newConnId = 0;
    snow_connection_t connections[concurrentConnections];
};

void snow_init(snow_global_t *global);

void snow_destroy(snow_global_t *global);

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, void *extra), void *extra);