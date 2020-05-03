#include <iostream>
#include <cstring>
#include <chrono>
#include <fstream>

#include "lib/snowhttp.h"

using namespace std;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents) {
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        printf("Normal quit\n");
    }
}

auto start = std::chrono::steady_clock::now();

int recvcb = 0;

const int req_test_n = 10;

void http_cb(char *data, size_t len, void *extra) {
    recvcb++;

    setbuf(stdout, NULL);
    printf("%.*s\n", len, data);

    if (recvcb == req_test_n) {
        auto end = std::chrono::steady_clock::now();
        cout << "Elapsed: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

        //printf("%.*s\n", len, data);

        recvcb = 0;
        exit(0);
    }


    //cout << *(int*)extra<<"\n";
    //exit(0);
}

snow_global_t global;

int idArr[req_test_n + 5];

static void loop_cb(struct ev_loop *loop) {
    static int i = 0;

    if (i == 1)start = std::chrono::steady_clock::now();

    if (i >= 1 && i <= req_test_n) {
        char url[256] = "https://api.binance.com/api/v3/time";
        idArr[i] = i;
        snow_do(&global, GET, url, http_cb, &idArr[i]);
    }

    i++;
}

void timercb(struct ev_loop *loop, struct ev_timer *w, int revents) {
    start = std::chrono::steady_clock::now();

    for (int i = 0; i < req_test_n; i++) {
        char url[256] = "https://api.binance.com/api/v3/time";
        idArr[i] = i;
        snow_do(&global, GET, url, http_cb, &idArr[i]);
    }

}

int main() {

    global.loop = EV_DEFAULT;

    // libev stuff
    struct ev_signal signal_watcher{};
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(global.loop, &signal_watcher);

    snow_init(&global);

    //ev_timer timer{};
    //ev_timer_init(&timer, timercb, 0, 5);
    //ev_timer_start(global.loop, &timer);

    ev_run(global.loop, loop_cb);

    snow_destroy(&global);

    return 0;
}
