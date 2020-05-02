#include <cstring>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wolfssl/ssl.h>
#include "netinet/tcp.h"

#include "snowhttp.h"

void snow_parseUrl(snow_global_t *global, snow_connection_t *conn) {
    char *protocol_end = strstr(conn->requestUrl, "://");
    if (protocol_end != nullptr) {
        conn->protocol = conn->requestUrl;
        *protocol_end = 0;
    } else
        assert(0);

    char *it;
    for (it = protocol_end + 3; *it != ':' && *it != '/' && it != nullptr; it++);

    if (it == nullptr) assert(0);

    if (*it == ':') { // we have port
        conn->hostname = protocol_end + 3;
        *it = 0;
        conn->path = strchr(it + 1, '/');

        if (conn->path == nullptr) assert(0);
        *conn->path = 0;  // set slash to 0
        conn->path++;

        conn->port = atoi(it + 1);

    } else { // no port
        conn->hostname = protocol_end + 3;
        *it = 0; // set slash to 0
        conn->path = it + 1;
    }

    if (conn->port == 0) {
        if (strcmp(conn->protocol, "http") == 0)
            conn->port = 80, conn->secure = false;
        else if (strcmp(conn->protocol, "https") == 0)
            conn->port = 443, conn->secure = true;
        else
            assert(0);
    }
}

void snow_resolveHost(snow_global_t *global, snow_connection_t *conn) {
    int error;
    struct hostent *result;

    gethostbyname_r(conn->hostname, &conn->hostent, conn->hostentBuff, 2048, &result, &error);

    if (result == nullptr) {
        std::cerr << "Hostname resolve error: " << error << "\n";
        assert(0);
    }

    bzero(&conn->address, sizeof(conn->address));

    memcpy(&conn->address.sin_addr, conn->hostent.h_addr_list[0], conn->hostent.h_length);
    conn->address.sin_family = AF_INET;
    conn->address.sin_port = htons(conn->port);
}

void snow_startTLSHandshake(snow_global_t *global, snow_connection_t *conn) {
    if ((conn->ssl = wolfSSL_new(global->wolfCtx)) == nullptr) {
        fprintf(stderr, "wolfSSL_new error.\n");
        assert(0);
    }

    wolfSSL_set_fd(conn->ssl, conn->sockfd);
    wolfSSL_set_using_nonblock(conn->ssl, 1);

    conn->connectionStatus = CONN_TLS_HANDSHAKE; // will be processed in write cb & read cb
}

static void snow_io_read_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
    auto *conn = (struct snow_connection_t *) ((struct ev_io_snow *) w)->data;

    if (conn->connectionStatus == CONN_WAITING || conn->connectionStatus == CONN_RECEIVING) {
        size_t readSize = buff_put_from_sock(&conn->readBuff, conn->secure ? (void *) conn->ssl : (void *) &conn->sockfd, -1, conn->secure);

        if (!readSize) return; // no read

        if (conn->connectionStatus == CONN_WAITING) {
            conn->connectionStatus = CONN_RECEIVING;

            char *chunked = strstr(&conn->readBuff.buff[conn->readBuff.tail], "Transfer-Encoding: chunked\r\n");

            if (chunked) conn->chunked = true;

            char *p = strstr(&conn->readBuff.buff[conn->readBuff.tail], "\r\n\r\n");

            if (p == nullptr) {
                std::cerr << "ERR: could not find the end of headers\n";
                assert(0);
            }

            p += 4; // skip over \r\n\r\n

            conn->readBuff.tail = p - conn->readBuff.buff;
            conn->content = p;

        }

        if (conn->chunked) {
            if (strcmp(&conn->readBuff.buff[conn->readBuff.head - 5], "0\r\n\r\n") == 0) {
                std::cout << "end\n";

                // TODO: remove and check sizes
                // TODO: segmentation fault on release build - (-O3)

                conn->write_cb(conn->content, conn->extra_cb);
                conn->connectionStatus = CONN_DONE;
            }
        } else if (strcmp(&conn->readBuff.buff[conn->readBuff.head - 4], "\r\n\r\n") == 0) {
            conn->write_cb(conn->content, conn->extra_cb);
            conn->connectionStatus = CONN_DONE;
        }


    } else if (conn->connectionStatus == CONN_TLS_HANDSHAKE) {
        char buffer[80];
        int ret = wolfSSL_connect(conn->ssl);
        int err;
        if (ret != SSL_SUCCESS) {
            err = wolfSSL_get_error(conn->ssl, ret);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                fprintf(stderr, "error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                assert(0);
            }
        } else {
            conn->connectionStatus = CONN_READY;
            std::cerr << "TLS handshake finished\n";
        }
    }
}

static void snow_io_write_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
    auto *conn = (struct snow_connection_t *) ((struct ev_io_snow *) w)->data;

    if (conn->connectionStatus == CONN_IN_PROGRESS) {
        int conn_r = connect(conn->sockfd, (struct sockaddr *) &conn->address, sizeof(conn->address));
        if (conn_r == 0) {
            conn->connectionStatus = CONN_ACK;
            if (conn->secure)
                snow_startTLSHandshake(conn->global, conn);
            else conn->connectionStatus = CONN_READY;
            std::cerr << "Connection ack\n";
        };
    }

    if (conn->connectionStatus == CONN_TLS_HANDSHAKE) {
        char buffer[80];
        int ret = wolfSSL_connect(conn->ssl);
        int err;
        if (ret != SSL_SUCCESS) {
            err = wolfSSL_get_error(conn->ssl, ret);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                fprintf(stderr, "error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
                assert(0);
            }
        } else {
            conn->connectionStatus = CONN_READY;
            std::cerr << "TLS handshake finished\n";
        }
    }

    if (conn->connectionStatus == CONN_READY && !buff_empty(&conn->writeBuff)) {
        int rem = buff_pull_to_sock(&conn->writeBuff, conn->secure ? (void *) conn->ssl : (void *) &conn->sockfd, buff_to_pull(&conn->writeBuff),
                                    conn->secure);

        if (rem == 0) conn->connectionStatus = CONN_WAITING;

        std::cerr << "rem = " << rem << "\n";
    }

}

static void snow_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    std::cout << "time\n";
}

void snow_initConnection(snow_global_t *global, snow_connection_t *conn) {
    conn->sockfd = socket(conn->address.sin_family, SOCK_STREAM, 0);
    conn->connectionStatus = CONN_UNREADY;

    int flag = 1; // disable nagle
    setsockopt(conn->sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

    fcntl(conn->sockfd, F_SETFL, fcntl(conn->sockfd, F_GETFL, 0) | O_NONBLOCK); // set socket to non blocking

    ev_io_init((struct ev_io *) &conn->ior, snow_io_read_cb, conn->sockfd, EV_READ);
    ev_io_init((struct ev_io *) &conn->iow, snow_io_write_cb, conn->sockfd, EV_WRITE);
    conn->ior.data = conn;
    conn->iow.data = conn;

    char str[1000];
    inet_ntop(AF_INET, &conn->address.sin_addr, str, INET_ADDRSTRLEN);
    std::cerr << str << "\n";


    if (conn->sockfd == -1) {
        std::cerr << "Socket creation failed\n";
        assert(0);
    }

    int conn_r = connect(conn->sockfd, (struct sockaddr *) &conn->address, sizeof(conn->address));
    conn->connectionStatus = CONN_IN_PROGRESS;

    if (conn_r != 0 && errno != EINPROGRESS) {
        std::cerr << "Socket connection failed: " << errno << "\n";
        assert(0);
    }

    ev_io_start(global->loop, (struct ev_io *) &conn->ior);
    ev_io_start(global->loop, (struct ev_io *) &conn->iow);

    ev_timer_init(&global->timer, snow_timer_cb, 0.0, 1.0);
    //ev_timer_start(global->loop, &global->timer);
}

void snow_sendRequest(snow_global_t *global, snow_connection_t *conn) {

    int size = sprintf(conn->requestBuff, "%s /%s HTTP/1.1\r\nHost: %s\r\n\r\n", method_strings[conn->method], conn->path, conn->hostname);

    /* if (wolfSSL_write(conn->ssl, conn->requestBuff, size) != size) {
         std::cerr << "wolfSSL_write failed\n";
         char buffer[80];
         int err = wolfSSL_get_error(conn->ssl, 0);
         wolfSSL_ERR_error_string(err, buffer);
         printf("%s\n", buffer);
         assert(0);
     }*/
    buff_put(&conn->writeBuff, conn->requestBuff, size);
    //write(conn->sockfd, conn->requestBuff, size);
    return;
    char buff[100000] = {};

    if (wolfSSL_read(conn->ssl, buff, sizeof(buff)) <= 0) {
        std::cerr << "wolfSSL_write failed\n";
        char buffer[80];
        int err = wolfSSL_get_error(conn->ssl, 0);
        wolfSSL_ERR_error_string(err, buffer);
        printf("%s\n", buffer);
        assert(0);
    }
    wolfSSL_free(conn->ssl);

    //read(conn->sockfd, buff, sizeof(buff));

    std::cerr << buff << "\n";
}

void snow_do(snow_global_t *global, int method, const char *url, void (*write_cb)(char *data, void *extra), void *extra) {
    int id = global->newConnId;
    snow_connection_t *conn = &global->connections[id];
    conn->global = global;

    strcpy(conn->requestUrl, url);
    conn->method = method;
    conn->write_cb = write_cb;
    conn->extra_cb = extra;

    snow_parseUrl(global, conn);
    snow_resolveHost(global, conn);
    snow_initConnection(global, conn);
    snow_sendRequest(global, conn);

    global->newConnId++;
    global->newConnId %= concurrentConnections;
}

void snow_init(snow_global_t *global) {
    wolfSSL_Init();

    if ((global->wolfCtx = wolfSSL_CTX_new(wolfTLSv1_2_client_method())) == nullptr) {
        fprintf(stderr, "wolfSSL_CTX_new error.\n");
        assert(0);
    }

    // Load CA certificates into WOLFSSL_CTX
    if (wolfSSL_CTX_load_verify_locations(global->wolfCtx, "/etc/ssl/certs/ca-certificates.crt", nullptr) != SSL_SUCCESS) {
        fprintf(stderr, "Error loading /etc/ssl/certs/ca-certificates.crt");
        assert(0);
    }

}

void snow_destroy(snow_global_t *global) {
    wolfSSL_CTX_free(global->wolfCtx);
    wolfSSL_Cleanup();
}