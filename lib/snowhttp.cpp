/*
MIT License

Copyright (c) 2020 Razvan Dan David

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "snowhttp.h"

#include <iostream>

#include <thread>
#include <cassert>
#include <cerrno>
#include <climits>
#include <chrono>
#include <netinet/tcp.h>
#include "misc/logger.h"

void snow_processConnError(snow_connection_t *conn, int err) {
    if (conn->err_cb) conn->err_cb(err, conn->extra_cb);

    if (conn->connectionStatus > CONN_UNREADY) {
        ev_io_stop(conn->loop, (ev_io *) &conn->ior);
        ev_io_stop(conn->loop, (ev_io *) &conn->iow);
    }

    if (conn->sockfd != 0) {
        setsockopt(conn->sockfd, SOL_SOCKET, SO_LINGER, &sock_linger0, sizeof(struct linger));
        close(conn->sockfd);
    }

    if (conn->secure && conn->ssl) wolfSSL_free(conn->ssl);

    conn->connectionStatus = CONN_DONE;
    conn->global->freeConnections.push(conn->id);  // atomic if multi loop
}

size_t snow_buff_pull_to_sock(struct buff_static_t<writeBuffSize> *buff, snow_connection_t *conn, size_t size) {
    size_t remain = size;

    if (buff->tail + size > writeBuffSize) {
        snow_processConnError(conn, BUFF_WRITE_SMALL);
        return -1;
    }

    while (remain > 0) {
        ssize_t ret;

        if (conn->secure) {
            ret = wolfSSL_write(conn->ssl, &buff->buff[buff->tail], remain);
            if (ret == -1) {
                int err = wolfSSL_get_error(conn->ssl, ret);
                if (SNOW_LIKELY(err == WOLFSSL_ERROR_WANT_WRITE))
                    break;
                else {
                    snow_processConnError(conn, SOCK_WRITE_ERR);
                    return -1;
#ifdef SNOW_DEBUG
                    char buffer[256];
                    fprintf(stderr, "sock pull error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                    assert(0);
#endif
                }
            }
        } else {
            ret = write(conn->sockfd, &buff->buff[buff->tail], remain);
            if (ret < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN) break;
                snow_processConnError(conn, SOCK_WRITE_ERR);
                return -1;
            }
        }
        remain -= ret;
        buff->tail += ret;
    }
    return remain;
}

size_t snow_buff_put_from_sock(struct buff_static_t<readBuffSize> *buff, snow_connection_t *conn, int size) {
    size_t remain, total = 0;

    if (size < 0) size = INT_MAX;
    remain = size;

    while (remain) {
        ssize_t ret;
        size_t head_room = readBuffSize - buff->head;

        if (head_room <= 0) {
            snow_processConnError(conn, BUFF_READ_SMALL);
            return 0;
        }

        if (conn->secure) {
            ret = wolfSSL_read(conn->ssl, &buff->buff[buff->head], head_room);
            if (ret == -1) {
                int err = wolfSSL_get_error(conn->ssl, ret);
                if (SNOW_LIKELY(err == WOLFSSL_ERROR_WANT_READ))
                    break;
                else {
                    snow_processConnError(conn, SOCK_READ_ERR);
                    return 0;
#ifdef SNOW_DEBUG
                    char buffer[256];
                    fprintf(stderr, "sock put error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                    assert(0);
#endif
                }
            }
        } else {
            ret = read(conn->sockfd, &buff->buff[buff->head], head_room);
            if (ret < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == ENOTCONN || errno == EWOULDBLOCK) break;
                snow_processConnError(conn, SOCK_READ_ERR);
                return 0;
            }
        }

        if (SNOW_UNLIKELY(ret == 0)) {
            snow_processConnError(conn, SOCK_READ_CLOSED);
            return 0;
#ifdef SNOW_DEBUG
            char buffer[256];
                    fprintf(stderr, "sock put error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                    assert(0);
#endif
        }

        if (SNOW_UNLIKELY(ret <= 0)) {
            snow_processConnError(conn, SOCK_READ_ERR);
            return 0;
        }

        buff->head += ret;
        remain -= ret;
        total += ret;
    }
    return total;
}

void snow_parseUrl(snow_connection_t *conn) {
    char *protocol_end = strstr(conn->requestUrl, "://");
    if (protocol_end != nullptr) {
        conn->protocol = conn->requestUrl;
        *protocol_end = 0;
    } else {
        snow_processConnError(conn, URL_MALFORMATTED);
        return;
    }

    char *it;
    for (it = protocol_end + 3; *it != ':' && *it != '/' && *it != '\0'; it++);

    if (*it == ':') { // we have port
        conn->hostname = protocol_end + 3;
        *it = 0;
        conn->path = strchr(it + 1, '/');

        if (conn->path == nullptr) {
            snow_processConnError(conn, URL_MALFORMATTED);
            return;
        }

        *conn->path = 0;  // set slash to 0
        conn->path++;

        conn->port = atoi(it + 1);
        conn->portPtr = it + 1;

    } else { // no port
        conn->hostname = protocol_end + 3;
        *it = 0; // set slash to 0
        conn->path = it + 1;
    }

#ifndef SNOW_NO_POST_BODY
    conn->query = strchr(conn->path, '?');
#endif

    if (conn->port == 0) {
        if (strcmp(conn->protocol, "http") == 0)
            conn->port = 80, conn->portPtr = "80", conn->secure = false;
        else if (strcmp(conn->protocol, "https") == 0)
            conn->port = 443, conn->portPtr = "443", conn->secure = true;
        else
            snow_processConnError(conn, URL_MALFORMATTED);
    }
}

void snow_resolveHost(snow_connection_t *conn) {
    std::string hostPort = conn->hostname;
    hostPort.append(conn->portPtr);

    auto it = conn->global->addrCache.find(hostPort); // atomic if multi loop

    if (it != conn->global->addrCache.end()) {
        conn->addrinfo = it->second;
    } else {
        conn->hints.ai_family = AF_INET;
        conn->hints.ai_socktype = SOCK_STREAM;
        conn->hints.ai_flags |= AI_NUMERICSERV;

        int ret = getaddrinfo(conn->hostname, conn->portPtr, &conn->hints, &conn->addrinfo);

        if (SNOW_UNLIKELY(ret != 0)) {
            snow_processConnError(conn, HOSTNAME_RESOLVE);
            return;
        }

        conn->global->addrCache.insert({hostPort, conn->addrinfo});  // atomic if multi loop
    }
}

void snow_startTLSHandshake(snow_connection_t *conn) {
    if ((SNOW_UNLIKELY((conn->ssl = wolfSSL_new(conn->global->wolfCtx)) == nullptr))) {
        snow_processConnError(conn, WOLFSSL_NEW);
        return;
    }

#ifdef SNOW_TLS_SESSION_REUSE
    if (conn->method != __TLS_DUMMY) {
        std::string hostPort = conn->hostname;
        hostPort.append(conn->portPtr);

        auto session = conn->sessions.find(hostPort);

        if (session != conn->sessions.end()) {
            if (wolfSSL_set_session(conn->ssl, session->second) != SSL_SUCCESS) {
                fprintf(stderr, "ERR: failed to set cached session\n");
            }
        } else fprintf(stderr, "WARN: could not find resumable session\n");
    }
#endif

    wolfSSL_set_fd(conn->ssl, conn->sockfd);
    wolfSSL_set_using_nonblock(conn->ssl, 1);

    conn->connectionStatus = CONN_TLS_HANDSHAKE; // will be processed in write cb & read cb
}

void snow_terminateConn(snow_connection_t *conn) {
    if (conn->write_cb) conn->write_cb(conn->content, conn->contentLen, conn->extra_cb);

    setsockopt(conn->sockfd, SOL_SOCKET, SO_LINGER, &sock_linger0, sizeof(struct linger));
    close(conn->sockfd);

    if (conn->secure) wolfSSL_free(conn->ssl);

    conn->connectionStatus = CONN_DONE;
    conn->global->freeConnections.push(conn->id);  // atomic if multi loop
}

void snow_parseChunks(snow_connection_t *conn) {
    char *chunkBegin = conn->content;
    char *newCopyStart = chunkBegin;

    // parse chunks & verify sizes
    while (chunkBegin < conn->readBuff.buff + conn->readBuff.head) {
        size_t chunkLen = strtol(chunkBegin, nullptr, 16);
        conn->contentLen += chunkLen;

        char *chunkData = strstr(chunkBegin, "\r\n");

        if (!chunkData) {
            snow_processConnError(conn, CHUNKED_DATA_PARSING);
            return;
        }

        chunkData += 2; // skip \r\n

        memcpy(newCopyStart, chunkData, chunkLen); // copy & compute next copy location
        newCopyStart += chunkLen;

        if (!(chunkData[chunkLen] == '\r' && chunkData[chunkLen + 1] == '\n')) {  // end of chunk
            snow_processConnError(conn, CHUNKED_DATA_PARSING);
            return;
        }

        chunkBegin = chunkData + chunkLen + 2; // skip to new beginning
    }

    *newCopyStart = 0; // terminate string
    conn->readBuff.head = newCopyStart - conn->readBuff.buff + 1; // reclaim some buff space


    if (conn->contentLen != (size_t) (newCopyStart - conn->content)) { // got all data
        snow_processConnError(conn, CHUNKED_DATA_PARSING);
        return;
    }
}

void snow_continueTLSHandshake(snow_connection_t *conn) {
    int ret = wolfSSL_connect(conn->ssl);

    if (SNOW_UNLIKELY(ret != SSL_SUCCESS)) {

        int err = wolfSSL_get_error(conn->ssl, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            snow_processConnError(conn, WOLFSSL_CONNECT);
            return;
#ifdef SNOW_DEBUG
            char buffer[80];
            fprintf(stderr, "error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
            assert(0);
#endif
        }

    } else {
        conn->connectionStatus = CONN_READY;

#ifdef SNOW_TLS_SESSION_REUSE
        if (conn->method == __TLS_DUMMY) {
            std::string hostPort = conn->hostname;
            hostPort.append(conn->portPtr);

            auto session = conn->sessions.find(hostPort);

            if (session != conn->sessions.end()) { // remove and free old session
                wolfSSL_SESSION_free(session->second);
                conn->sessions.erase(hostPort);
            }

            conn->sessions.insert({hostPort, wolfSSL_get_session(conn->ssl)}); // insert new session

            ev_io_stop(conn->loop, (struct ev_io *) &conn->ior);
            ev_io_stop(conn->loop, (struct ev_io *) &conn->iow);
            snow_terminateConn(conn);
        }
#endif
    }
}

void snow_processFirstResponse(snow_connection_t *conn) {
    conn->connectionStatus = CONN_RECEIVING;

    char *chunked = strstr(&conn->readBuff.buff[conn->readBuff.tail], "\r\nTransfer-Encoding: chunked\r\n");
    if (chunked) conn->chunked = true;

    char *pLen = strstr(&conn->readBuff.buff[conn->readBuff.tail], "\r\nContent-Length: ");
    if (pLen) conn->expectedContentLen = atoi(pLen + 18);

    char *pEnd = strstr(&conn->readBuff.buff[conn->readBuff.tail], "\r\n\r\n");

    if (SNOW_UNLIKELY(pEnd == nullptr)) {
        snow_processConnError(conn, HEADER_PARSING);
        return;
    }

    pEnd += 4; // skip over \r\n\r\n

    conn->readBuff.tail = pEnd - conn->readBuff.buff;
    conn->content = pEnd;
}

static void snow_io_read_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
    auto *conn = (struct snow_connection_t *) ((struct ev_io_snow *) w)->data;

    if (conn->connectionStatus == CONN_TLS_HANDSHAKE) {
        snow_continueTLSHandshake(conn);
    }

    if (conn->connectionStatus == CONN_WAITING || conn->connectionStatus == CONN_RECEIVING) {
        size_t readSize = snow_buff_put_from_sock(&conn->readBuff, conn, -1);

        if (!readSize) return; // no read

        if (conn->connectionStatus == CONN_WAITING) {
            snow_processFirstResponse(conn);
        }

        if ((conn->chunked)) {
            if (strcmp(&conn->readBuff.buff[conn->readBuff.head - 5], "0\r\n\r\n") == 0) {
                snow_parseChunks(conn);
                snow_terminateConn(conn);
                ev_io_stop(loop, (struct ev_io *) &conn->ior);
            }
        } else {
            if (conn->expectedContentLen) { // header Content-Length was present
                if (&conn->readBuff.buff[conn->readBuff.head] - conn->content >= conn->expectedContentLen) {
                    conn->contentLen = conn->expectedContentLen;
                    snow_terminateConn(conn);
                    ev_io_stop(loop, (struct ev_io *) &conn->ior);
                }
            } else if (strcmp(&conn->readBuff.buff[conn->readBuff.head - 1], "\n") == 0) {
                conn->contentLen = &conn->readBuff.buff[conn->readBuff.head] - conn->content;
                snow_terminateConn(conn);
                ev_io_stop(loop, (struct ev_io *) &conn->ior);
            }
        }

    }
}

int snow_sendRequest(snow_connection_t *conn) {
    int rem = snow_buff_pull_to_sock(&conn->writeBuff, conn, conn->writeBuff.to_pull());

    if (rem == 0) conn->connectionStatus = CONN_WAITING;

    return rem;
}

void snow_checkConnected(snow_connection_t *conn) {
    int conn_r = connect(conn->sockfd, conn->addrinfo->ai_addr, conn->addrinfo->ai_addrlen);
    if (conn_r == 0) {
        conn->connectionStatus = CONN_ACK;
        if (conn->secure)
            snow_startTLSHandshake(conn);
        else conn->connectionStatus = CONN_READY;
    }
}

static void snow_io_write_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
    auto *conn = (struct snow_connection_t *) ((struct ev_io_snow *) w)->data;

    if (conn->connectionStatus == CONN_IN_PROGRESS) {
        snow_checkConnected(conn);
    }

    if (conn->connectionStatus == CONN_TLS_HANDSHAKE) {
        snow_continueTLSHandshake(conn);
    }

    if (conn->connectionStatus == CONN_READY && !conn->writeBuff.empty()) {
        if (snow_sendRequest(conn) == 0)
            ev_io_stop(loop, (struct ev_io *) &conn->iow);
    }
}

void snow_initConnection(snow_connection_t *conn) {
    conn->sockfd = socket(conn->addrinfo->ai_family, conn->addrinfo->ai_socktype, conn->addrinfo->ai_protocol);

    setsockopt(conn->sockfd, SOL_SOCKET, SO_PRIORITY, &connSockPriority, sizeof(int));

#ifdef SNOW_DISABLE_NAGLE
    int nagle = 1;
    setsockopt(conn->sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle, sizeof(int));
#endif

    fcntl(conn->sockfd, F_SETFL, fcntl(conn->sockfd, F_GETFL, 0) | O_NONBLOCK);

    conn->connectionStatus = CONN_IN_PROGRESS;

    ev_io_init((struct ev_io *) &conn->ior, snow_io_read_cb, conn->sockfd, EV_READ);
    ev_io_init((struct ev_io *) &conn->iow, snow_io_write_cb, conn->sockfd, EV_WRITE);
    conn->ior.data = conn;
    conn->iow.data = conn;

    if (SNOW_UNLIKELY(conn->sockfd == -1)) {
        snow_processConnError(conn, SOCK_CREATION);
        return;
    }

    int conn_r = connect(conn->sockfd, conn->addrinfo->ai_addr, conn->addrinfo->ai_addrlen);

    if (SNOW_UNLIKELY(conn_r != 0 && errno != EINPROGRESS)) {
        snow_processConnError(conn, SOCK_CONNECTION);
        return;
    }

    ev_io_start(conn->loop, (struct ev_io *) &conn->ior);
    ev_io_start(conn->loop, (struct ev_io *) &conn->iow);
}

void snow_bufferRequest(snow_connection_t *conn) {
    int size = 0;

#ifndef SNOW_NO_POST_BODY
    if (conn->method == POST && conn->query) {
        *conn->query = 0; // terminate path at ?
        size = sprintf(conn->writeBuff.buff,
                       "%s /%s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Content-Type: application/x-www-form-urlencoded\r\n"
                       "Content-len: %zu\r\n"
                       "%.*s\r\n"
                       "%s",
                       method_strings[conn->method], conn->path, conn->hostname, strlen(conn->query + 1),
                       (int) conn->extraHeaders_size, conn->extraHeaders, conn->query + 1);
        printf("%s", conn->writeBuff.buff);
    } else
#endif
    {
        size = sprintf(conn->writeBuff.buff, "%s /%s HTTP/1.1\r\n"
                                             "Host: %s\r\n"
                                             "%.*s\r\n",
                       method_strings[conn->method], conn->path, conn->hostname, (int) conn->extraHeaders_size, conn->extraHeaders);
    }

    conn->writeBuff.head += size;
}

void snow_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    auto *global = (struct snow_global_t *) ((struct ev_timer_snow *) w)->data;

    uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    for (int id = 0; id < concurrentConnections; id++) {
        if (global->connections[id].connectionStatus > CONN_UNREADY && global->connections[id].connectionStatus < CONN_DONE &&
            time - global->connections[id].creationTime > connSockTimeout) {

            snow_processConnError(&global->connections[id], CONN_TIMEOUT);
        }
    }


    while (!global->requestQueue.empty() && !global->freeConnections.empty()) { // check for free connections
        snow_bareRequest_t req = global->requestQueue.front();
        global->requestQueue.pop();

        snow_do(global, req.method, req.requestUrl, req.write_cb, req.err_cb, req.extra_cb, req.extraHeaders, req.extraHeaders_size);
    }
}

#ifdef SNOW_TLS_SESSION_REUSE

void snow_timer_renew_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    auto *global = (struct snow_global_t *) ((struct ev_timer_snow *) w)->data;

    for (const std::string &url : global->wantedSessions) {
        for (int i = 0; i < concurrentConnections; ++i) {
            snow_enqueue(global, __TLS_DUMMY, url.c_str(), nullptr,
                         [](int err, void *extra) { fprintf(stderr, "ERR: __TLS_DUMMY encountered error: %d\n", err); },
                         nullptr, nullptr, 0);
        }
    }

    log_debug("internal snowhttp: renewing sessions");
}

void snow_renew_sessions(snow_global_t *global){
    for (const std::string &url : global->wantedSessions) {
        for (int i = 0; i < concurrentConnections; ++i) {
            snow_enqueue(global, __TLS_DUMMY, url.c_str(), nullptr,
                         [](int err, void *extra) { fprintf(stderr, "ERR: __TLS_DUMMY encountered error: %d\n", err); },
                         nullptr, nullptr, 0);
        }
    }

    log_debug("internal snowhttp: renewing sessions");
}

#endif

///// PUBLIC

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra),
             void (*err_cb)(int err, void *extra),
             void *extra, const char *extraHeaders, size_t extraHeaders_size) {

    if (global->freeConnections.empty()) { // check for free connections
        err_cb(NO_FREE_CONN, extra);
        return;
    }

    int id = global->freeConnections.front();
    global->freeConnections.pop();

    snow_connection_t *conn = &global->connections[id];

#ifdef SNOW_TLS_SESSION_REUSE
    memset(conn, 0, sizeof(struct snow_connection_t) - (sizeof(std::map<std::string, WOLFSSL_SESSION *>)));
#else
    memset(conn, 0, sizeof(struct snow_connection_t));
#endif

    conn->id = id;

#ifdef SNOW_MULTI_LOOP
    conn->loop = global->loops[global->rr_loop];
    global->rr_loop = (global->rr_loop + 1) % multi_loop_n_runtime;
#else
    conn->loop = global->loop;
#endif

    conn->global = global;

    strcpy(conn->requestUrl, url);
    conn->method = method;
    conn->write_cb = write_cb;
    conn->err_cb = err_cb;
    conn->extra_cb = extra;

    if (extraHeaders_size) assert(extraHeaders != nullptr);
    conn->extraHeaders = extraHeaders;
    conn->extraHeaders_size = extraHeaders_size;

    conn->creationTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    snow_parseUrl(conn);
    snow_resolveHost(conn);
    snow_initConnection(conn);

    if (conn->method != __TLS_DUMMY) snow_bufferRequest(conn);
}

void snow_enqueue(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, size_t data_len, void *extra),
                  void (*err_cb)(int err, void *extra),
                  void *extra, const char *extraHeaders, size_t extraHeaders_size) {

    if (global->freeConnections.empty()) { // check for free connections
        global->requestQueue.push({method, url, extra, write_cb, err_cb, extraHeaders, extraHeaders_size});
        return;
    }

    snow_do(global, method, url, write_cb, err_cb, extra, extraHeaders, extraHeaders_size);
}

#ifdef SNOW_TLS_SESSION_REUSE

void snow_addWantedSession(snow_global_t *global, const std::string &url) {
    global->wantedSessions.push_back(url);
}

#endif

#ifdef SNOW_MULTI_LOOP

void snow_spawnLoops(snow_global_t *global) {
    for (int id = 0; id < multi_loop_n_runtime; id++)
        global->threads[id] = std::thread([](ev_loop *loop) { ev_run(loop, nullptr); }, global->loops[id]);
}

void snow_joinLoops(snow_global_t *global) {
    for (int id = 0; id < multi_loop_n_runtime; id++)
        global->threads[id].join();
}

#endif

void snow_init(snow_global_t *global) {
    wolfSSL_Init();

    global->wolfCtx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());

    if (global->wolfCtx == nullptr) {
        fprintf(stderr, "ERR: wolfSSL_CTX_new error.\n");
        assert(0);
    }

    if (wolfSSL_CTX_UseSessionTicket(global->wolfCtx) != SSL_SUCCESS) {
        fprintf(stderr, "ERR: ticket enable error.\n");
        assert(0);
    }

    if (wolfSSL_CTX_set_session_cache_mode(global->wolfCtx, SSL_SESS_CACHE_NO_AUTO_CLEAR) != SSL_SUCCESS) {
        fprintf(stderr, "ERR: could not turn session cache flushing off.\n");
        assert(0);
    }

    // Load CA certificates into WOLFSSL_CTX
    if (wolfSSL_CTX_load_verify_locations(global->wolfCtx, "/etc/ssl/certs/ca-certificates.crt", nullptr) != SSL_SUCCESS) {
        fprintf(stderr, "ERR: Error loading /etc/ssl/certs/ca-certificates.crt");
        assert(0);
    }

    for (int i = 0; i < concurrentConnections; i++)
        global->freeConnections.push(i);  // atomic if multi loop

#ifdef SNOW_MULTI_LOOP
    global->loop = global->loops[0];
#endif

#ifdef SNOW_QUEUEING_ENABLED
    global->mainTimer.data = global;
    ev_timer_init((struct ev_timer *) &global->mainTimer, snow_timer_cb, 0, mainTimerInterval);
    ev_timer_start(global->loop, (struct ev_timer *) &global->mainTimer);
#endif

#ifdef SNOW_TLS_SESSION_REUSE
    global->sessionRenewTimer.data = global;
    ev_timer_init((struct ev_timer *) &global->sessionRenewTimer, snow_timer_renew_cb, 0, sessionRenewInterval); // reset the sessions hourly
    ev_timer_start(global->loop, (struct ev_timer *) &global->sessionRenewTimer);
#endif
}

void snow_destroy(snow_global_t *global) {
    wolfSSL_CTX_free(global->wolfCtx);
    wolfSSL_Cleanup();
}