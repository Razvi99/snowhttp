
#ifdef __linux__

#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <malloc.h> // for NULL
#include <time.h>
#include <string.h>
#include "events.h"


static uint64_t ev_now_us() {
  struct timespec t;
  
  // Get the time
  clock_gettime(CLOCK_MONOTONIC, &t); 
  
  return t.tv_sec * 1000000ULL + (t.tv_nsec / 1000ULL);
}


void ev_init(int argc, char **argv) {
}

static struct ev_loop* ev_setup(struct ev_loop* loop) {
  static struct ev_loop defLoop = {-1,0,NULL,NULL};
  if (!loop) loop = &defLoop;
  if (loop->pfd < 0) {
    loop->pfd = epoll_create1(0);
    if (loop->pfd < 0) {
      return loop;
    }
  }
  return loop;
}

static void ev_timer_sorted_insert(struct ev_loop* loop, struct ev_timer* tmr) {
  struct ev_timer* p, *pp;
  
  // Insert it at the proper position
  pp = NULL, p = loop->thead;
  while (p && tmr->delay_us > p->delay_us) {
    pp = p; p = p->next;
  }
  tmr->next = p;
  if (pp) pp->next = tmr;
  else loop->thead = tmr;
}

static uint64_t ev_timer_check_expired(struct ev_loop* loop) {
  
  // Get the timeout to the next timer event
  uint64_t max_wait_us = 1000000ULL;
  uint64_t now_us = ev_now_us();
  
  // Check all running timers
  struct ev_timer* tmr = loop->thead;
  while (tmr) {
    
    // If we did not reach the first timer to trigger...
    if (now_us < tmr->delay_us) {
      max_wait_us = tmr->delay_us - now_us;
      break;
    }
      
    // call the timer fn
    tmr->cb(loop,tmr,1);
    
    // If timer is still running (user did not cancel it)
    if (tmr->running) {

      // Now, we must resort it ... remove it first
      loop->thead = tmr->next;

      // Re add it to the proper place if repetitive
      if (tmr->period_us > 0) {
        
        // Add the increment to the timer
        tmr->delay_us += tmr->period_us;

        // Insert it at the proper position
        ev_timer_sorted_insert(loop, tmr);
      }
      
    } 
    /* else, the timer was stopped from the timer callback, 
       and the _stop function removed it from the linked list */

    // Point to the next timer descriptor in the chain
    tmr = loop->thead;
    
  }
  return max_wait_us;
}

void ev_break(struct ev_loop* loop, int mode) {
  loop = ev_setup(loop);
  loop->brk = 1;
}

#define MAX_EVENTS 64
static int ev_loop(void* arg) {
  struct ev_loop* loop = (struct ev_loop*) arg;
  
  struct epoll_event events[MAX_EVENTS];    
  
  // Get the timeout to the next timer event
  uint64_t max_wait_us = ev_timer_check_expired(loop);
  
  // Poll for events
  int event_count = epoll_wait(loop->pfd, events, MAX_EVENTS, 0);

//#define TEST
#ifdef TEST
  wsc_log_err("epoll_wait %d", event_count);
#endif
  
  // If timeout, handle it
  if (event_count == 0) {
    
    // Timed out - Check and execute expired timers
    ev_timer_check_expired(loop);
    
  } else {
    int i;
    // perform the proper callback to the event handlers
    for(i = 0; i < event_count; i++) {
      struct ev_io* ev = (struct ev_io*) events[i].data.ptr;
      int fd = ev->fd;
      
      // Find the proper handler - We are warrantied this one, or the next is the proper entry
      if (events[i].events & EPOLLIN) {
        struct ev_io* p = ev;
        while (p && p->fd == fd) {
          if (p->mode == EV_READ) {
            p->cb(loop, p, 1);
            break;
          }
          p = p->next;
        }
      }
      
      if (events[i].events & EPOLLOUT) {
        struct ev_io* p = ev;
        while (p && p->fd == fd) {
          if (p->mode == EV_WRITE) {
            p->cb(loop, p, 1);
            break;
          }
          p = p->next;
        }
      }
      
    }
  }
  /* Handle breaks by ending loop */
  if (loop->brk) {
    return -1;
  }
  return 0;
}

void ev_run(struct ev_loop* loop, ev_run_cb_t callback) {
  loop = ev_setup(loop);
  
  /* Call the event loop while the loop must run */
  if (callback) {
	  while (ev_loop(loop) == 0) callback(loop);
  } else {
	  while (ev_loop(loop) == 0);
  }
}

ev_tstamp ev_now(struct ev_loop* loop) {
  struct timespec t;
  
  loop = ev_setup(loop);
  
  // Get the time
  clock_gettime(CLOCK_MONOTONIC, &t); 
  
  return t.tv_sec + t.tv_nsec / 1000000000.0;
}

void ev_timer_init(struct ev_timer* tmr, ev_timer_cb_t cb, double delay, double period) {
  tmr->cb = cb;
  tmr->delay_us = (uint64_t)(delay * 1000000ULL); // To microseconds
  tmr->period_us = (uint64_t)(period * 1000000ULL); // To microseconds
  tmr->running = false;
  tmr->next = NULL;
}

void ev_timer_start(struct ev_loop* loop, struct ev_timer* tmr) {
  struct ev_timer* p, *pp;
  
  loop = ev_setup(loop);

  // Timer is running
  tmr->running = true;

  // Make sure it is NOT already registered
  p = loop->thead;
  while (p && p != tmr) {
    p = p->next;
  }
  if (p) return;
  
  // Make the delay relative to now
  tmr->delay_us += ev_now_us();

  // Insert it at the proper position
  ev_timer_sorted_insert(loop, tmr);
}

void ev_timer_stop(struct ev_loop* loop, struct ev_timer* tmr) {
  struct ev_timer* p, *pp;
  
  loop = ev_setup(loop);
  
  // Timer is stopped
  tmr->running = false;

  // Remove it from the timer chain
  pp = NULL, p = loop->thead;
  while (p && p != tmr) {
    pp = p; p = p->next;
  }
  
  // If not registered, do nothing
  if (!p) return;
  
  // Remove it from the chain
  if (pp) pp->next = tmr->next;
  else loop->thead = tmr->next;
}

void ev_io_init(struct ev_io* ev, ev_io_cb_t cb, int fd, int mode) {
  ev->fd = fd;
  ev->cb = cb;
  ev->mode = mode;
  ev->next = NULL;
}

void ev_io_start(struct ev_loop* loop, struct ev_io* ev) {
  struct ev_io* p, *pp;  
  int r;
  
  struct epoll_event event;
  loop = ev_setup(loop);
  
  // Make sure it is NOT already registered
  p = loop->ihead;
  while (p && p != ev) {
    p = p->next;
  }
  if (p) return;
  
  // Insert the event at the list at the proper position, if possible
  pp = NULL, p = loop->ihead;
  while (p && ev->fd > p->fd) {
    pp = p; p = p->next;
  }
  ev->next = p;
  if (pp) pp->next = ev;
  else loop->ihead = ev;
  
  // Check if the fd was already registered ...
  if (p && p->fd == ev->fd) {
    
    // fd was already registered. Lets assume for the opposite operation
    event.events = EPOLLIN | EPOLLOUT;
    event.data.ptr = ev; // Lets store the descriptor to the first
    
    r = epoll_ctl(loop->pfd, EPOLL_CTL_MOD, ev->fd, &event);
#ifdef TEST
    wsc_log_err("EPOLL_CTL_MOD %d fc:%d\n", r, ev->fd);
#endif
  } else {
    
    // fd was not registered.
    event.events = ev->mode == EV_READ ? EPOLLIN : EPOLLOUT;
    event.data.ptr = ev;
    
    r = epoll_ctl(loop->pfd, EPOLL_CTL_ADD, ev->fd, &event);
#ifdef TEST
    wsc_log_err("EPOLL_CTL_ADD %d fc:%d\n", r, ev->fd);
#endif
  }
}

void ev_io_stop(struct ev_loop* loop, struct ev_io* ev) {
  struct ev_io* p, *pp;  
  int r;
  
  struct epoll_event event = {0};
  loop = ev_setup(loop);
  
  // Remove it from the io chain
  pp = NULL, p = loop->ihead;
  while (p && p != ev) {
    pp = p; p = p->next;
  }
  
  // If not registered, do nothing
  if (!p) return;
  
  // Remove it from the list
  if (pp) pp->next = ev->next;
  else loop->ihead = ev->next;
  
  // Check if there is a remaining registration
  p = loop->ihead;
  while (p && p->fd != ev->fd) p = p->next;
  if (p) {
    
    // Still another monitor exists
    event.events = p->mode == EV_READ ? EPOLLIN : EPOLLOUT;
    event.data.ptr = p;
    
    r = epoll_ctl(loop->pfd, EPOLL_CTL_MOD, p->fd, &event);
#ifdef TEST
    wsc_log_err("EPOLL_CTL_MOD %d fc:%d\n", r, p->fd);
#endif
  } else {
    
    // No other monitors exist - Deregister the event
    r = epoll_ctl(loop->pfd, EPOLL_CTL_DEL, ev->fd, &event);
#ifdef TEST
    wsc_log_err("EPOLL_CTL_DEL %d fc:%d\n", r, ev->fd);
#endif
  }
}

static struct ev_signal* gsgn[32] = {0};
static struct ev_loop* gloop[32] = {0};
static void sighandler(int signum) {
  gsgn[signum]->cb(gloop[signum],gsgn[signum],1);
}

void ev_signal_init(struct ev_signal* sgn, ev_signal_cb_t cb,int signum) {
  sgn->cb = cb;
  sgn->signum = signum;
}

void ev_signal_start(struct ev_loop* loop, struct ev_signal* sgn) {
  struct sigaction act;
  memset(&act,0,sizeof(act));
  sigemptyset(&act.sa_mask);
  act.sa_handler = sighandler;
  gsgn[sgn->signum] = sgn;
  gloop[sgn->signum] = loop;
  sigaction(sgn->signum, &act, &sgn->old);
}

void ev_signal_stop(struct ev_loop* loop, struct ev_signal* sgn) {
  sigaction(sgn->signum, &sgn->old, NULL);
}


#endif
