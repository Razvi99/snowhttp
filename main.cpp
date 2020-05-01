#include <iostream>
#include <cstring>

#include "lib/snowhttp.h"

using namespace std;

int main() {
    snow_connection_t test_conn;
    char url[256] = "http://localhost/";
    strcpy(test_conn.requestUrl, url);

    snow_parseUrl(&test_conn);
    snow_resolveHost(&test_conn);
    snow_initConnection(&test_conn);
    snow_sendRequest(&test_conn);

    return 0;
}
