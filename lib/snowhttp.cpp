#include <cstring>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "snowhttp.h"

void snow_parseUrl(snow_connection_t *conn) {
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

void snow_resolveHost(snow_connection_t *conn) {
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

void snow_initConnection(snow_connection_t *conn) {
    conn->sockfd = socket(conn->address.sin_family, SOCK_STREAM, 0);

    char str[1000];
    inet_ntop(AF_INET, &conn->address.sin_addr, str, INET_ADDRSTRLEN);
    std::cerr<<str<<"\n";


    if (conn->sockfd == -1) {
        std::cerr << "Socket creation failed\n";
        assert(0);
    }

    if (connect(conn->sockfd, (struct sockaddr *) &conn->address, sizeof(conn->address)) != 0) {
        std::cerr << "Socket connection failed: " << errno << "\n";
        assert(0);
    }
}

void snow_sendRequest(snow_connection_t *conn){

    int size = sprintf(conn->requestBuff, "%s /%s HTTP/1.1\r\nHost: %s\r\n\r\n", method_strings[conn->method], conn->path, conn->hostname);
    write(conn->sockfd, conn->requestBuff, size);

    char buff[100000] = {};
    read(conn->sockfd, buff, sizeof(buff));

    std::cerr<<buff<<"\n";
}