#include <iostream>
#include <cstring>
#include <chrono>

#include "lib/snowhttp.h"

using namespace std;

auto start = std::chrono::steady_clock::now();

int recvcb = 0;

constexpr int req_test_n = 320;

ev_loop loop;

snow_global_t global;

bool haveTlsSessions = false;

void http_cb(char *data, size_t len, void *extra) {
    recvcb++;

    //setbuf(stdout, NULL);
    //printf("finished %lu\n", (uint64_t) extra);
    //printf("%d", (long) extra);
    //fprintf(stderr,"%.*s\n", len, data);


    if (recvcb == req_test_n / 2) {
        haveTlsSessions = true;

        auto end = std::chrono::steady_clock::now();
        printf("total: %f ms\n", (double) std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e+6);
        printf("tls: %f ms\n", (double) tls_compute_total / 1e+6);
        sleep(1);
        //exit(0);
    }

    if (recvcb == req_test_n) {

        auto end = std::chrono::steady_clock::now();
        printf("total: %f ms\n", (double) std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1e+6);
        printf("tls: %f ms\n", (double) tls_compute_total / 1e+6);

        exit(0);
    }
    //cout << *(int*)extra<<"\n";
    //exit(0);
}

bool can_send = 1;


static void loop_cb(struct ev_loop *loop) {
    static int i = 0;

/*    if (can_send) {
        for (uint64_t id = 0; id < req_test_n; id++) {
            char url[256] = "https://google.com/a.html";
            snow_enqueue(&global, GET, url, http_cb, (void *) id);
        }
        start = std::chrono::steady_clock::now();

        can_send = 0;
    } */


    if (can_send) {
        for (uint64_t id = 0; id < req_test_n / 2; id++) {
            char url[256] = "https://google.com/a.html";
            snow_enqueue(&global, GET, url, http_cb, (void *) id);
        }
        start = std::chrono::steady_clock::now();

        can_send = 0;
    }

    if (haveTlsSessions) {
        printf("sending\n");
        for (uint64_t id = 0; id < req_test_n / 2; id++) {
            char url[256] = "https://google.com/a.html";
            snow_enqueue(&global, GET, url, http_cb, (void *) id);
        }
        start = std::chrono::steady_clock::now();
        tls_compute_total = 0;

        haveTlsSessions = false;
    }

    i++;
}

int main() {
    snow_init(&global);

    ev_run(global.loop, loop_cb);

    snow_destroy(&global);

    return 0;
}
