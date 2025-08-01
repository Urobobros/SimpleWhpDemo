#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "timer.h"

/*Timer*/
typedef struct PIT_nr {
        int nr;
        struct PIT *pit;
} PIT_nr;

typedef struct PIT {
        uint32_t l[3];
        pc_timer_t timer[3];
        uint8_t m[3];
        uint8_t ctrl, ctrls[3];
        int wp, rm[3], wm[3];
        uint16_t rl[3];
        int thit[3];
        int delay[3];
        int rereadlatch[3];
        int gate[3];
        int out[3];
        int running[3];
        int enabled[3];
        int newcount[3];
        int count[3];
        int using_timer[3];
        int initial[3];
        int latched[3];
        int disabled[3];

        uint8_t read_status[3];
        int do_read_status[3];

        PIT_nr pit_nr[3];

        void (*set_out_funcs[3])(int new_out, int old_out);
} PIT;

extern PIT pit;
extern PIT pit2;
