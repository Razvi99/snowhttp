#include <iostream>
#include <cstring>
#include <chrono>

#include "lib/snowhttp.h"

using namespace std;

auto start = std::chrono::steady_clock::now();

int recvcb = 0;

constexpr int req_test_n = 160;

ev_loop loop;

snow_global_t global;


void http_cb(char *data, size_t len, void *extra) {
    recvcb++;

    //setbuf(stdout, NULL);
    printf("finished %lu\n", (uint64_t) extra);
    //printf("%d", (long) extra);
    printf("%.*s\n", len, data);

    if (recvcb == req_test_n) {
        recvcb = 0;

        auto end = std::chrono::steady_clock::now();
        printf("%ld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

        exit(0);
    }
    //cout << *(int*)extra<<"\n";
    //exit(0);
}

static void loop_cb(struct ev_loop *loop) {
    static int i = 0;

    if (i == 1) {
        for (uint64_t id = 0; id < req_test_n; id++) {
            char url[256] = "https://api.binance.com/api/v3/time";
            snow_enqueue(&global, GET, url, http_cb, (void*) id);
        }
        start = std::chrono::steady_clock::now();
    }

    i++;
}

int main() {
    snow_init(&global);

    ev_run(global.loop, loop_cb);

    snow_destroy(&global);

    return 0;
}
