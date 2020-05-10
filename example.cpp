#include <iostream>
#include <cstring>
#include <chrono>
#include <cassert>

#include "lib/snowhttp.h"

using namespace std;

auto start = std::chrono::steady_clock::now();

std::atomic<int> recvcb = 0;
constexpr int req_test_n = 100;

/////////
snow_global_t global = {};
ev_loop loops[multi_loop_max];
/////////

void http_cb(char *data, size_t len, void *extra) {
    recvcb++;

    //printf("finished %lu\n", (uint64_t) extra);
    //printf("%.*s\n", len, data);

    if (recvcb == req_test_n) {

        auto end = std::chrono::steady_clock::now();
        printf("total: %f ms\n", (double) std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e+6);

        for (int i = 0; i < multi_loop_n_runtime; i++) {
            loops[i].brk = 1;
        }
    }
}

void err_cb(int err, void *extra) {
    fprintf(stderr, "conn %lu, error: %d\n", (uint64_t) extra, err);
    recvcb++;
}

void assignLoops(snow_global_t *g) {
    for (int i = 0; i < multi_loop_n_runtime; i++) {
        loops[i] = {-1, 0, nullptr, nullptr};
        g->loops[i] = &loops[i];
    }
}

int main(int argc, char **argv) {
    if (argc == 2) {
        multi_loop_n_runtime = atoi(argv[1]);
        assert(multi_loop_n_runtime <= multi_loop_max);
    }

    assignLoops(&global);
    snow_addWantedSession(&global, "https://api.binance.com");
    snow_init(&global);
    snow_spawnLoops(&global);

    sleep(1);

    for (uint64_t id = 0; id < req_test_n; id++) {
        snow_enqueue(&global, GET, "https://api.binance.com/api/v3/ping", http_cb, err_cb);
    }
    start = std::chrono::steady_clock::now();


    snow_joinLoops(&global);
    snow_destroy(&global);

    return 0;
}
