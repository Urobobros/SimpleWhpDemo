#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/fdc.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    fdc_add();
    for(int p=0x3f0;p<=0x3f5;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
    }
    assert(port_inb[0x3f7][0]);
    assert(port_outb[0x3f7][0]);
    printf("fdc_add tests passed\n");
    return 0;
}
