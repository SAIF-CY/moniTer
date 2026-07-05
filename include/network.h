#ifndef BIOS_MONITOR_NETWORK_H
#define BIOS_MONITOR_NETWORK_H

#include "common.h"

typedef struct {
    char name[BIOS_STR_SHORT];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    struct timespec ts;
} net_sample_t;

void bios_network_init(void);
void bios_network_collect(bios_net_iface_t *ifaces, int *count,
                          net_sample_t *prev, int *prev_count);

#endif /* BIOS_MONITOR_NETWORK_H */