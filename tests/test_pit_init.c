#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/pit.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    pit_init();
    for(int p=0x40;p<=0x43;p++) {
        assert(port_inb[p][0]);
        assert(port_outb[p][0]);
        assert(port_priv[p][0]==&pit);
    }
    pit_ps2_init();
    assert(port_priv[0x44][0]==&pit2);
    assert(port_priv[0x47][0]==&pit2);
    printf("pit_init tests passed\n");
    return 0;
}
