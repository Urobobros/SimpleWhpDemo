#ifndef PORTLOG_H
#define PORTLOG_H

#ifdef __cplusplus
extern "C" {
#endif

void PortLogStart(void);
void PortLog(const char *fmt, ...);
void PortLogEnd(void);

#ifdef PORT_DEBUG
#define PORT_LOG(fmt, ...) PortLog(fmt, __VA_ARGS__)
#else
#define PORT_LOG(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORTLOG_H */
