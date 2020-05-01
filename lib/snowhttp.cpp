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

int snow_wssl_recv(WOLFSSL *ssl, char *buff, int sz, void *ctx) {
    std::cout << "recv\n";
    /* By default, ctx will be a pointer to the file descriptor to read from.
     * This can be changed by calling wolfSSL_SetIOReadCtx(). */
    int sockfd = *(int *) ctx;
    int recvd;
    /* Receive message from socket */
    if ((recvd = recv(sockfd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

        fprintf(stderr, "IO RECEIVE ERROR: ");
        switch (errno) {
#if EAGAIN != EWOULDBLOCK
            case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
#endif
            case EWOULDBLOCK:
                if (!wolfSSL_dtls(ssl) || wolfSSL_get_using_nonblock(ssl)) {
                    fprintf(stderr, "would block\n");
                    return WOLFSSL_CBIO_ERR_WANT_READ;
                } else {
                    fprintf(stderr, "socket timeout\n");
                    return WOLFSSL_CBIO_ERR_TIMEOUT;
                }
            case ECONNRESET:
                fprintf(stderr, "connection reset\n");
                return WOLFSSL_CBIO_ERR_CONN_RST;
            case EINTR:
                fprintf(stderr, "socket interrupted\n");
                return WOLFSSL_CBIO_ERR_ISR;
            case ECONNREFUSED:
                fprintf(stderr, "connection refused\n");
                return WOLFSSL_CBIO_ERR_WANT_READ;
            case ECONNABORTED:
                fprintf(stderr, "connection aborted\n");
                return WOLFSSL_CBIO_ERR_CONN_CLOSE;
            default:
                fprintf(stderr, "general error\n");
                return WOLFSSL_CBIO_ERR_GENERAL;
        }
    } else if (recvd == 0) {
        printf("Connection closed\n");
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

    /* successful receive */
    printf("my_IORecv: received %d bytes from %d\n", sz, sockfd);
    return recvd;
}

int snow_wssl_send(WOLFSSL *ssl, char *buff, int sz, void *ctx) {
    std::cout << "send\n";
    /* By default, ctx will be a pointer to the file descriptor to write to.
     * This can be changed by calling wolfSSL_SetIOWriteCtx(). */
    int sockfd = *(int *) ctx;
    int sent;

    /* Receive message from socket */
    if ((sent = send(sockfd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

        fprintf(stderr, "IO SEND ERROR: ");
        switch (errno) {
#if EAGAIN != EWOULDBLOCK
            case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
#endif
            case EWOULDBLOCK:
                fprintf(stderr, "would block\n");
                return WOLFSSL_CBIO_ERR_WANT_READ;
            case ECONNRESET:
                fprintf(stderr, "connection reset\n");
                return WOLFSSL_CBIO_ERR_CONN_RST;
            case EINTR:
                fprintf(stderr, "socket interrupted\n");
                return WOLFSSL_CBIO_ERR_ISR;
            case EPIPE:
                fprintf(stderr, "socket EPIPE\n");
                return WOLFSSL_CBIO_ERR_CONN_CLOSE;
            default:
                fprintf(stderr, "general error\n");
                return WOLFSSL_CBIO_ERR_GENERAL;
        }
    } else if (sent == 0) {
        printf("Connection closed\n");
        return 0;
    }

    /* successful send */
    printf("my_IOSend: sent %d bytes to %d\n", sz, sockfd);
    return sent;
}

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
            conn->port = 80;
        else if (strcmp(conn->protocol, "https") == 0)
            conn->port = 443;
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

void snow_initConnectionTLS(snow_global_t *global, snow_connection_t *conn) {
    if ((conn->ssl = wolfSSL_new(global->wolfCtx)) == nullptr) {
        fprintf(stderr, "wolfSSL_new error.\n");
        assert(0);
    }

    wolfSSL_set_fd(conn->ssl, conn->sockfd);
    wolfSSL_set_using_nonblock(conn->ssl, 1);

    char buffer[80];
    int ret = wolfSSL_connect(conn->ssl);
    int err;
    if (ret != SSL_SUCCESS) {
        err = wolfSSL_get_error(conn->ssl, ret);
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
    }
}

static void snow_io_read_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
    auto *p = (struct ev_io_snow *) w;
    auto *conn = (struct snow_connection_t *) p->data;

    bool eof = false;

    std::cerr<<"put from sock\n";

    buff_put_from_fd(&conn->readBuff, conn->sockfd, -1, &eof, nullptr, nullptr);


    std::cout<<conn->readBuff.buff;

    eof =  (strcmp(&conn->readBuff.buff[conn->readBuff.head - 2], "\n\n") == 0);

    if(eof){
        std::cerr<<"read all data\n";
    }

    exit(0);
    assert(0);

}

static void snow_io_write_cb(struct ev_loop *loop, struct ev_io *w, int revents) {

    auto *p = (struct ev_io_snow *) w;
    auto *conn = (struct snow_connection_t *) p->data;

    if (conn->connectionStatus == CONN_IN_PROGRESS) {
        int conn_r = connect(conn->sockfd, (struct sockaddr *) &conn->address, sizeof(conn->address));
        if (conn_r == 0) {
            conn->connectionStatus = CONN_ACK;
            std::cerr << "Connection ack\n";
        } else std::cerr << "waiting for ack\n";
    }
    if (conn->connectionStatus == CONN_ACK && !buff_empty(&conn->writeBuff)) {
        buff_pull_to_fd(&conn->writeBuff, conn->sockfd, buff_to_pull(&conn->writeBuff), nullptr, nullptr);
        std::cerr<<"pulled to sock\n";
    }

}

static void snow_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    std::cout << "time\n";
}

void snow_initConnection(snow_global_t *global, snow_connection_t *conn) {
    conn->sockfd = socket(conn->address.sin_family, SOCK_STREAM, 0);
    conn->connectionStatus = CONN_NO;

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

    //snow_initConnectionTLS(global, conn);
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

void snow_do(snow_global_t *global, int method, const char *url) {
    int id = global->newConnId;
    snow_connection_t *conn = &global->connections[id];

    strcpy(conn->requestUrl, url);
    conn->method = method;

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