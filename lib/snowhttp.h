#pragma once

#include <map>
#include <queue>
#include <stack>

#include "wolfssl/options.h"
#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/ssl.h"

#include "events.h"


constexpr int concurrentConnections = 128;
constexpr int connUrlSize = 256;
constexpr int connBufferSize = 1 << 15U;
constexpr int connSockPriority = 6;
constexpr double queueCheckInterval = 0.001; // 1ms

inline uint64_t tls_compute_total = 0;

#define DISABLE_NAGLE
#define QUEUEING_ENABLED
#define TLS_SESSION_REUSE

enum method_enum {
    GET, POST, DELETE
};

struct snow_global_t;

void snow_init(snow_global_t *global);

void snow_destroy(snow_global_t *global);

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra), void *extra = nullptr,
             const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);

#ifdef QUEUEING_ENABLED
void snow_enqueue(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra), void *extra = nullptr,
             const char *extraHeaders = nullptr, size_t extraHeaders_size = 0);
#endif

#ifdef TLS_SESSION_REUSE
#endif

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

struct ev_timer_snow {
    struct ev_timer t;
    void *data;
};

struct snow_connection_t {
    int id;

    char requestUrl[connUrlSize] = {};
    char *protocol = nullptr, *hostname = nullptr, *path = nullptr;
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

    WOLFSSL *ssl{};

    struct ev_io_snow ior = {}, iow = {};

    buff_static_t writeBuff;
    buff_static_t readBuff;

    char *content = nullptr;
    size_t contentLen = 0;
    bool chunked = false;

    void *extra_cb = nullptr;

    void (*write_cb)(char *data, size_t data_len, void *extra) = nullptr;

    snow_global_t *global{};

#ifdef TLS_SESSION_REUSE
    bool shouldRenewTicket = true;
    std::map<std::string, WOLFSSL_SESSION*> sessions;
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
    ev_loop *loop = {};

    WOLFSSL_CTX *wolfCtx = nullptr;

    struct ev_timer_snow mainTimer = {};
    struct ev_timer_snow renewTicketsTimer = {};
    std::map<std::string, struct addrinfo *> addrCache;

    snow_connection_t connections[concurrentConnections];

    struct linger sock_linger0 = {1, 0};

    std::queue<int, std::deque<int>> freeConnections;
    std::queue<struct snow_bareRequest_t, std::deque<struct snow_bareRequest_t>> requestQueue;
};