#pragma once

#include <map>
#include <queue>
#include <stack>
#include <atomic>
#include <thread>
#include "atomic.h"

#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/ssl.h"

#include "events.h"

constexpr int concurrentConnections = 256;
constexpr int connUrlSize = 256;
constexpr int connBufferSize = 1 << 15U;
constexpr int connSockPriority = 6;
constexpr int connSockTimeout = 2000; // in ms

constexpr double mainTimerInterval = 0.001; // 1ms - coincides with queue checking
constexpr double sessionRenewInterval = 3600; // 1hr

constexpr int multi_loop_max = 16;
inline int multi_loop_n_runtime = 8;

#define SNOW_DISABLE_NAGLE
#define SNOW_QUEUEING_ENABLED
#define SNOW_TLS_SESSION_REUSE
#define SNOW_NO_POST_BODY
#define SNOW_MULTI_LOOP

enum method_enum {
    GET, POST, DELETE
};

enum error_enum {
    HOSTNAME_RESOLVE, WOLFSSL_NEW, CHUNKED_DATA_PARSING, WOLFSSL_CONNECT, HEADER_PARSING, SOCK_CREATION, SOCK_CONNECTION,
    SOCK_WRITE_ERR, SOCK_READ_ERR, SOCK_READ_CLOSED, URL_MALFORMATTED, BUFF_WRITE_SMALL, BUFF_READ_SMALL, CONN_TIMEOUT, NO_FREE_CONN
};

struct snow_global_t;

void snow_init(snow_global_t *global);

void snow_destroy(snow_global_t *global);

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra),
             void (*err_cb)(int err, void *extra),
             void *extra = nullptr, const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);

#ifdef SNOW_QUEUEING_ENABLED

void snow_enqueue(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra),
                  void (*err_cb)(int err, void *extra),
                  void *extra = nullptr, const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);

#endif

#ifdef SNOW_TLS_SESSION_REUSE

void snow_addWantedSession(snow_global_t *global, const std::string &url);

#endif

#ifdef SNOW_MULTI_LOOP

void snow_spawnLoops(snow_global_t *global);

void snow_joinLoops(snow_global_t *global);

#endif

/////////////////////////////////////////////////////

constexpr int __TLS_DUMMY = -1;

#define SNOW_LIKELY(x) __builtin_expect(!!(x), 1)
#define SNOW_UNLIKELY(x) __builtin_expect(!!(x), 0)

const char method_strings[3][10] = {"GET", "POST", "DELETE"};

enum conn_status_enum {
    CONN_UNREADY, // before socket()
    CONN_IN_PROGRESS, // before conect()
    CONN_ACK, // connect() received ACK
    CONN_TLS_HANDSHAKE, // wolfssl tls started
    CONN_READY, // ready to send http request
    CONN_WAITING, // waiting for first response
    CONN_RECEIVING, // received first response, potentially waiting for more
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

struct ev_timer_snow {
    struct ev_timer t;
    void *data;
};

constexpr struct linger sock_linger0 = {1, 0};

struct snow_connection_t {
    int id;

    ev_loop *loop;

    char requestUrl[connUrlSize] = {};
    char *protocol = nullptr, *hostname = nullptr, *path = nullptr, *query = nullptr;
    const char *portPtr = nullptr;

    int port = 0;
    int method = 0;
    bool secure = false;

    const char *extraHeaders = nullptr;
    size_t extraHeaders_size = 0;

    struct addrinfo *addrinfo = nullptr;
    struct addrinfo hints = {};

    int sockfd = 0;
    int connectionStatus = 0;
    uint64_t creationTime;

    WOLFSSL *ssl = nullptr;

    struct ev_io_snow ior = {}, iow = {};

    buff_static_t writeBuff;
    buff_static_t readBuff;

    int expectedContentLen = 0;
    char *content = nullptr;
    size_t contentLen = 0;
    bool chunked = false;

    void *extra_cb = nullptr;

    void (*write_cb)(char *data, size_t data_len, void *extra) = nullptr;

    void (*err_cb)(int err, void *extra) = nullptr;

    snow_global_t *global = nullptr;

#ifdef SNOW_TLS_SESSION_REUSE
    std::map<std::string, WOLFSSL_SESSION *> sessions;
#endif
};

struct snow_bareRequest_t {
    int method;
    const char *requestUrl;

    void *extra_cb;

    void (*write_cb)(char *data, size_t data_len, void *extra);

    void (*err_cb)(int err, void *extra);

    const char *extraHeaders;
    size_t extraHeaders_size;
};

struct snow_global_t {
#ifndef SNOW_MULTI_LOOP
    ev_loop *loop = nullptr;
    std::map<std::string, struct addrinfo *> addrCache;
    std::queue<int, std::deque<int>> freeConnections;
#else
    int rr_loop = 0;
    std::thread threads[multi_loop_max];
    ev_loop *loops[multi_loop_max];
    ev_loop *loop = nullptr;

    atomic::map<std::string, struct addrinfo *> addrCache;
    atomic::queue<int, std::deque<int>> freeConnections;
#endif

    WOLFSSL_CTX *wolfCtx = nullptr;

    struct ev_timer_snow mainTimer = {};
    struct ev_timer_snow sessionRenewTimer = {};

    snow_connection_t connections[concurrentConnections];
    std::queue<struct snow_bareRequest_t, std::deque<struct snow_bareRequest_t>> requestQueue;

#ifdef SNOW_TLS_SESSION_REUSE
    std::vector<std::string> wantedSessions;
#endif
};