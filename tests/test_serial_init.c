#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/serial.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    serial1_init(0x3f8,4,1);
    serial2_init(0x2f8,3,1);
    for(int p=0x3f8;p<0x400;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    for(int p=0x2f8;p<0x300;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    printf("serial init tests passed\n");
    return 0;
}
