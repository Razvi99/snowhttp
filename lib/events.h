#ifndef _EVENTS_H
#define _EVENTS_H

#ifdef __linux__

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE
#endif

#include <signal.h> // For signal enum
#include <stdint.h>

struct ev_loop;
struct ev_timer;
struct ev_io;
struct ev_signal;

typedef double ev_tstamp;

typedef void (*ev_timer_cb_t)(struct ev_loop *loop, struct ev_timer *w, int revents);

struct ev_timer {
    ev_timer_cb_t cb;
    void *data;
    uint64_t delay_us;
    uint64_t period_us;
    bool running;
    struct ev_timer *next;
};

typedef void (*ev_io_cb_t)(struct ev_loop *loop, struct ev_io *w, int revents);

struct ev_io {
    int fd;
    int mode; // EV_READ or EV_WRITE
    ev_io_cb_t cb;
    void *data;
    struct ev_io *next;
};

struct ev_loop {
    int pfd; // pollfd
    int brk; // If we must break the loop
    struct ev_timer *thead; // Pointer to the list of running timers
    struct ev_io *ihead;    // Pointer to the list of running fd monitors
};

typedef void (*ev_signal_cb_t)(struct ev_loop *loop, struct ev_signal *w, int revents);

struct ev_signal {
    ev_signal_cb_t cb;
    int signum;
    struct sigaction old;
};


void ev_init(int argc, char **argv);

#define EV_READ 1
#define EV_WRITE 2
#define EV_DEFAULT NULL
#define EVBREAK_ALL 0

void ev_break(struct ev_loop *loop, int mode);

typedef void (*ev_run_cb_t)(struct ev_loop *loop);

void ev_run(struct ev_loop *loop, ev_run_cb_t callback);

ev_tstamp ev_now(struct ev_loop *loop);

void ev_timer_init(struct ev_timer *tmr, ev_timer_cb_t ev_timer_cb, double delay, double period);

void ev_timer_start(struct ev_loop *loop, struct ev_timer *tmr);

void ev_timer_stop(struct ev_loop *loop, struct ev_timer *tmr);

void ev_io_init(struct ev_io *ev, ev_io_cb_t cb, int fd, int mode);

void ev_io_start(struct ev_loop *loop, struct ev_io *ev);

void ev_io_stop(struct ev_loop *loop, struct ev_io *ev);

void ev_signal_init(struct ev_signal *sgn, ev_signal_cb_t signal_cb, int signum);

void ev_signal_start(struct ev_loop *loop, struct ev_signal *sgn);

void ev_signal_stop(struct ev_loop *loop, struct ev_signal *sgn);


#else

#include <ev.h>
#define ev_init(argc, argv)

#endif

#endif