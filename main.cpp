#include <iostream>
#include <cstring>
#include <chrono>

#include "lib/snowhttp.h"

using namespace std;

auto start = std::chrono::steady_clock::now();

int recvcb = 0;

constexpr int req_test_n = 1;

ev_loop loop;
snow_global_t global;


void http_cb(char *data, size_t len, void *extra) {
    recvcb++;

    //setbuf(stdout, NULL);
    //printf("finished %lu\n", (uint64_t) extra);
    //printf("%d", (long) extra);
    //printf("%.*s\n", len, data);
    printf("%s\n", data);


    if (recvcb == req_test_n) {

        auto end = std::chrono::steady_clock::now();
        printf("total: %f ms\n", (double) std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e+6);
        printf("tls: %f ms\n", (double) tls_compute_total / 1e+6);

        exit(0);
    }
}

static void loop_cb(struct ev_loop *loop) {
    static unsigned int i = 0;
    static bool sent = 0;

    if (global.waitForSessions >= concurrentConnections && !sent) {
        //if(!sent){
        sent = 1;

        for (uint64_t id = 0; id < req_test_n; id++) {
            char url[256] = "https://api.binance.com/api/v3/time";
            snow_enqueue(&global, GET, url, http_cb, (void *) id);
        }
        start = std::chrono::steady_clock::now();
        tls_compute_total = 0;
    }

    i++;
}

int main(int argc, char **argv) {
    snow_addWantedSession(&global, "https://api.binance.com");
    snow_init(&global);

    ev_run(global.loop, loop_cb);

    snow_destroy(&global);

    return 0;
}
