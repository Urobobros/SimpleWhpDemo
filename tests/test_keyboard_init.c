#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/keyboard.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    KeyboardXtInit();
    for(int p=0x60;p<=0x63;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    printf("keyboard_xt_init tests passed\n");
    return 0;
}
