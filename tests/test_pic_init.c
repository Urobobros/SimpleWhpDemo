#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/pic.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    PicInit();
    for(int p=0x20;p<=0x21;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    for(int p=0xA0;p<=0xA1;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    printf("pic_init tests passed\n");
    return 0;
}
