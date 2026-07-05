#ifndef BIOS_MONITOR_CPU_H
#define BIOS_MONITOR_CPU_H

#include "common.h"

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long total;
    unsigned long long idle_total;
} cpu_stat_t;

void bios_cpu_init(bios_cpu_info_t *cpu);
void bios_cpu_collect(bios_cpu_info_t *cpu,
                      const cpu_stat_t *cur, const cpu_stat_t *prev,
                      const cpu_stat_t *cur_cores, const cpu_stat_t *prev_cores,
                      int core_count);
int bios_cpu_read_stats(cpu_stat_t *total, cpu_stat_t *cores, int max_cores,
                        int *core_count);

#endif /* BIOS_MONITOR_CPU_H */