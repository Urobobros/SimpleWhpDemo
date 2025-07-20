#ifndef PORTLOG_H
#define PORTLOG_H

#ifdef __cplusplus
extern "C" {
#endif

void PortLogStart(void);
void PortLog(const char *fmt, ...);
void PortLogEnd(void);

#ifdef __cplusplus
}
#endif

#endif /* PORTLOG_H */
