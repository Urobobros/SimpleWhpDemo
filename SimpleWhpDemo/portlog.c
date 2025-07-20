#include "include/portlog.h"
#include <stdio.h>
#include <stdarg.h>

static FILE *g_portlog = NULL;

void PortLogStart(void)
{
    if (!g_portlog) {
        g_portlog = fopen("port.log", "wt");
    }
}

void PortLogEnd(void)
{
    if (g_portlog) {
        fclose(g_portlog);
        g_portlog = NULL;
    }
}

void PortLog(const char *fmt, ...)
{
    if (!g_portlog) PortLogStart();
    if (!g_portlog) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_portlog, fmt, ap);
    va_end(ap);
    fflush(g_portlog);
}
