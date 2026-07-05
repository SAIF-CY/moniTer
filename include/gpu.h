#ifndef BIOS_MONITOR_GPU_H
#define BIOS_MONITOR_GPU_H

#include "common.h"

void bios_gpu_init(void);
void bios_gpu_shutdown(void);
void bios_gpu_collect(bios_gpu_info_t *gpus, int *count);

#endif /* BIOS_MONITOR_GPU_H */