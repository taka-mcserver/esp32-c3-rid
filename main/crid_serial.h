#ifndef CRID_SERIAL_H
#define CRID_SERIAL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void crid_serial_init(void);
void crid_serial_write(const char *data, size_t len);
void crid_serial_printf(const char *fmt, ...);
#ifdef __cplusplus
}
#endif
#endif
