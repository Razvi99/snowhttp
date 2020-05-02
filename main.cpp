#include <iostream>
#include <cstring>

#include "lib/snowhttp.h"

using namespace std;

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents) {
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        printf("Normal quit\n");
    }
}

void http_cb(char *data, void *extra){
    printf("%s\n", data);
}

snow_global_t global;
static void loop_cb(struct ev_loop *loop) {
    static int i = 0;
    if(i == 1){
        char url[256] = "https://api.binance.com/api/v3/exchangeInfo";
        snow_do(&global, GET, url, http_cb, nullptr);
    }
    i++;
}

int main() {

    global.loop = EV_DEFAULT;

    // libev stuff
    struct ev_signal signal_watcher{};
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(global.loop, &signal_watcher);

    snow_init(&global);

    ev_run(global.loop, loop_cb);

    snow_destroy(&global);
    return 0;
}
