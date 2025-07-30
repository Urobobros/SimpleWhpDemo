#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/pit.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    pit_init();
    /* program channel 0 to mode 3, lobyte/hibyte */
    outb(0x43, 0x36);
    outb(0x40, 0x34);
    outb(0x40, 0x12);
    /* read back */
    assert(inb(0x40) == 0x34);
    assert(inb(0x40) == 0x12);
    /* latch current count */
    outb(0x43, 0x00);
    assert(inb(0x40) == 0x34);
    assert(inb(0x40) == 0x12);
    printf("pit rw tests passed\n");
    return 0;
}
