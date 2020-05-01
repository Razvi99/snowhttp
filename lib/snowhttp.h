#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <netdb.h>

const int concurrentConnections = 1000;
const int connectionBufferSize = 1024;
const int connectionUrlSize = 256;
const int connectionRequestBuffSize = 1024;

enum method_enum {
    GET, POST, DELTE
};
const char method_strings[3][10] = {"GET", "POST", "DELETE"};

struct snow_connection_t {
    // protocol://hostname:port/path-and-file-name
    char requestUrl[connectionUrlSize] = {};
    char *protocol, *hostname, *path;
    int port = 0;
    int method = 0;

    char requestBuff[connectionRequestBuffSize];

    struct hostent hostent;
    char hostentBuff[2048];

    int sockfd, connfd;
    struct sockaddr_in address;

    int buffEnd = 0;
    uint8_t buff[connectionBufferSize] = {};
};

struct snow_global_t {
    int newConnId = 0;
    snow_connection_t connections[concurrentConnections];
};

void snow_parseUrl(snow_connection_t *conn);

void snow_resolveHost(snow_connection_t *conn);

void snow_initConnection(snow_connection_t *conn);

void snow_sendRequest(snow_connection_t *conn);