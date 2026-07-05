#ifndef BIOS_MONITOR_UTIL_H
#define BIOS_MONITOR_UTIL_H

#include <stddef.h>

size_t bios_snprintf_dots(char *buf, size_t buflen, const char *label,
                          const char *value, int total_width);

#endif /* BIOS_MONITOR_UTIL_H */