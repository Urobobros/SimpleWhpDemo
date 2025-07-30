#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/pit.h"
#include <assert.h>
#include <stdio.h>

static int cb_count;
static void cb(int new_out, int old_out) { (void)old_out; cb_count += new_out ? 1 : -1; }

int main() {
    io_init();
    pit_init();
    pit_set_out_func(&pit, 0, cb);
    outb(0x43, 0x36); // mode 3 lobyte/hibyte for ch0
    outb(0x40, 0x02);
    outb(0x40, 0x00);
    pit_tick_channel(&pit.ch[0], &pit.out[0], pit.out_func[0], 2); // two ticks cause toggle
    assert(cb_count != 0);
    printf("pit_tick test passed\n");
    return 0;
}
