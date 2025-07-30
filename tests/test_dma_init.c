#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/dma.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    DmaInit();
    for(int p=0x00;p<0x10;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    for(int p=0x80;p<0x88;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    printf("dma_init tests passed\n");
    return 0;
}
