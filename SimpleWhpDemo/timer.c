#include "timer.h"
#include <stdlib.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

uint32_t timer_target = 0;
/* Default 1us tick to avoid division by zero before PIT sets value */
uint64_t TIMER_USEC = 1;

static pc_timer_t *timer_list = NULL;

void timer_enable(pc_timer_t *timer) {
    if (!timer->enabled) {
        timer->enabled = 1;
        /* Insert at head */
        timer->next = timer_list;
        if (timer_list)
            timer_list->prev = timer;
        timer->prev = NULL;
        timer_list = timer;
    }
}

void timer_disable(pc_timer_t *timer) {
    if (timer->enabled) {
        timer->enabled = 0;
        if (timer->prev)
            timer->prev->next = timer->next;
        else
            timer_list = timer->next;
        if (timer->next)
            timer->next->prev = timer->prev;
        timer->next = timer->prev = NULL;
    }
}

int64_t get_monotonic_ms() {
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return (counter.QuadPart * 1000) / freq.QuadPart;
}

void timer_process() {
    pc_timer_t *t = timer_list;
    int64_t start_ms = get_monotonic_ms();

    while (t) {
        pc_timer_t *next = t->next;
        int64_t now_ms = get_monotonic_ms();
        if (now_ms - start_ms > 100) {
            printf("Timer processing exceeded 100ms, aborting...\n");
            t->enabled = 0;
            timer_disable(t);
            t->callback(t->p);
            break;
        }

        if (t->callback) {
            t->enabled = 0;
            timer_disable(t);
            t->callback(t->p);
        }

        t = next;
    }
}



void timer_reset() {
    while (timer_list) {
        timer_disable(timer_list);
    }
    tsc = 0;
}

void timer_add(pc_timer_t *timer, void (*callback)(void *p), void *p, int start_timer) {
    memset(timer, 0, sizeof(*timer));
    timer->callback = callback;
    timer->p = p;
    if (start_timer)
        timer_enable(timer);
}
