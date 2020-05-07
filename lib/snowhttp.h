#pragma once

#include <map>
#include <queue>
#include <stack>

#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/ssl.h"

#include "events.h"

constexpr int concurrentConnections = 10;
constexpr int connUrlSize = 256;
constexpr int connBufferSize = 1 << 15U;
constexpr int connSockPriority = 6;
constexpr double queueCheckInterval = 0.001; // 1ms
constexpr double sessionRenewInterval = 3600; // 1hr

constexpr int loopN = 2;

inline uint64_t tls_compute_total = 0;

#define SNOW_DISABLE_NAGLE
#define SNOW_QUEUEING_ENABLED
#define SNOW_TLS_SESSION_REUSE
#define SNOW_NO_POST_BODY
//#define SNOW_MULTI_LOOP

enum method_enum {
    GET, POST, DELETE
};

struct snow_global_t;

void snow_init(snow_global_t *global);

void snow_destroy(snow_global_t *global);

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra), void *extra = nullptr,
             const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);

#ifdef SNOW_QUEUEING_ENABLED

void snow_enqueue(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra), void *extra = nullptr,
                  const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);

#endif

#ifdef SNOW_TLS_SESSION_REUSE

void snow_addWantedSession(snow_global_t *global, const std::string &url);

#endif

/////////////////////////////////////////////////////

constexpr int __TLS_DUMMY = -1;

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

struct ev_timer_snow {
    struct ev_timer t;
    void *data;
};

constexpr struct linger sock_linger0 = {1, 0};

struct snow_connection_t {
    int id;

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

    snow_global_t *global = nullptr;

#ifdef SNOW_TLS_SESSION_REUSE
    std::map<std::string, WOLFSSL_SESSION *> sessions;
#endif
};

struct snow_bareRequest_t {
    int method;
    const char *requestUrl;

    void (*write_cb)(char *data, size_t data_len, void *extra);

    void *cb_extra;
    const char *extraHeaders;
    size_t extraHeaders_size;
};

struct snow_global_t {
#ifndef SNOW_MULTI_LOOP
    ev_loop *loop = nullptr;
    std::map<std::string, struct addrinfo *> addrCache;
    std::queue<int, std::deque<int>> freeConnections;
#else
    ev_loop *loops[loopN];
    std::map<std::string, struct addrinfo *> addrCache;
    std::queue<int, std::deque<int>> freeConnections;
#endif

    WOLFSSL_CTX *wolfCtx = nullptr;

    struct ev_timer_snow mainTimer = {};
    struct ev_timer_snow sessionRenewTimer = {};

    snow_connection_t connections[concurrentConnections];
    std::queue<struct snow_bareRequest_t, std::deque<struct snow_bareRequest_t>> requestQueue;

#ifdef SNOW_TLS_SESSION_REUSE
#ifndef SNOW_MULTI_LOOP
    uint64_t waitForSessions = 0;
#else
    std::atomic<uint64_t> waitForSessions = 0;
#endif

    std::vector<std::string> wantedSessions;
#endif
};