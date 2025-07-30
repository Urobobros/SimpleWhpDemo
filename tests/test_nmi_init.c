#define IO_TEST_ACCESS
#include "windows.h"
#include "../SimpleWhpDemo/io.h"
#include "../SimpleWhpDemo/nmi.h"
#include <assert.h>
#include <stdio.h>

int main() {
    io_init();
    NmiInit();
    assert(port_outb[0xA0][0]);
    assert(port_priv[0xA0][0] == NULL); // priv not used
    printf("nmi_init tests passed\n");
    return 0;
}
