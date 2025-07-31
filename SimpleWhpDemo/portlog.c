#include "include/portlog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static FILE *g_portlog = NULL;
static unsigned int g_portlog_lines = 0;
static uint64_t g_portlog_start_us = 0;
static uint64_t g_portlog_index = 0;

static uint64_t portlog_now_us(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER count;
    if (!freq.QuadPart) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000ULL / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
#endif
}

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
        g_portlog_index = 0;
        g_portlog_start_us = portlog_now_us();
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
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
#ifdef _MSC_VER
    vsnprintf(buf, sizeof(buf), fmt, ap);
#else
    vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
    va_end(ap);
    double ms = 0.0;
    if (g_portlog_start_us)
        ms = (double)(portlog_now_us() - g_portlog_start_us) / 1000.0;
    size_t len = strlen(buf);
    if (len && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    fprintf(g_portlog, "%s [%10.3f ms] index: %llu\n", buf, ms,
            (unsigned long long)g_portlog_index++);
    fflush(g_portlog);
    g_portlog_lines++;
}
