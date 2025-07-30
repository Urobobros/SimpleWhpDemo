#include "include/portlog.h"
#include <stdio.h>
#include <stdarg.h>

static FILE *g_portlog = NULL;
static unsigned int g_portlog_lines = 0;

void PortLogStart(void)
{
    if (!g_portlog) {
#ifdef _MSC_VER
        if (fopen_s(&g_portlog, "port.log", "wt") != 0)
            g_portlog = NULL;
#else
        g_portlog = fopen("port.log", "wt");
#endif
        g_portlog_lines = 0;
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
    if (g_portlog_lines >= PORTLOG_MAX_LINES)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_portlog, fmt, ap);
    va_end(ap);
    fflush(g_portlog);
    g_portlog_lines++;
}
