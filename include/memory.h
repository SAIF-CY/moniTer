#ifndef BIOS_MONITOR_MEMORY_H
#define BIOS_MONITOR_MEMORY_H

#include "common.h"

void bios_memory_init(bios_memory_info_t *mem);
void bios_memory_collect(bios_memory_info_t *mem);

#endif /* BIOS_MONITOR_MEMORY_H */