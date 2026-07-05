#include "network.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static double elapsed_sec(const struct timespec *a, const struct timespec *b) {
    double da = a->tv_sec + a->tv_nsec / 1e9;
    double db = b->tv_sec + b->tv_nsec / 1e9;
    double d = db - da;
    return d > 0.001 ? d : 1.0;
}

static int find_prev(net_sample_t *prev, int prev_count, const char *name) {
    for (int i = 0; i < prev_count; i++) {
        if (strcmp(prev[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void bios_network_init(void) {}

void bios_network_collect(bios_net_iface_t *ifaces, int *count,
                          net_sample_t *prev, int *prev_count) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) {
        *count = 0;
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    char line[512];
    if (!fgets(line, sizeof(line), f) || !fgets(line, sizeof(line), f)) {
        fclose(f);
        *count = 0;
        return;
    }

    *count = 0;
    while (fgets(line, sizeof(line), f) && *count < BIOS_MAX_NET_IFACES) {
        char name[BIOS_STR_SHORT];
        unsigned long long rx_bytes, tx_bytes;
        unsigned long long dummy;
        char *colon = strchr(line, ':');
        if (!colon) {
            continue;
        }
        *colon = '\0';
        BIOS_STRNCPY(name, line);
        bios_trim(name);

        if (strcmp(name, "lo") == 0) {
            continue;
        }

        int n = sscanf(colon + 1,
                       "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &rx_bytes, &dummy, &dummy, &dummy, &dummy, &dummy,
                       &dummy, &dummy, &tx_bytes, &dummy, &dummy, &dummy,
                       &dummy, &dummy, &dummy, &dummy);
        if (n < 9) {
            continue;
        }

        bios_net_iface_t *iface = &ifaces[*count];
        memset(iface, 0, sizeof(*iface));
        BIOS_STRNCPY(iface->name, name);
        iface->rx_total = rx_bytes;
        iface->tx_total = tx_bytes;
        iface->up = true;

        int pidx = find_prev(prev, *prev_count, name);
        if (pidx >= 0) {
            double dt = elapsed_sec(&prev[pidx].ts, &now);
            double rx_delta = (double)(rx_bytes - prev[pidx].rx_bytes);
            double tx_delta = (double)(tx_bytes - prev[pidx].tx_bytes);
            iface->rx_kbps = (rx_delta / dt) / 1024.0;
            iface->tx_kbps = (tx_delta / dt) / 1024.0;
            prev[pidx].rx_bytes = rx_bytes;
            prev[pidx].tx_bytes = tx_bytes;
            prev[pidx].ts = now;
        } else if (*prev_count < BIOS_MAX_NET_IFACES) {
            BIOS_STRNCPY(prev[*prev_count].name, name);
            prev[*prev_count].rx_bytes = rx_bytes;
            prev[*prev_count].tx_bytes = tx_bytes;
            prev[*prev_count].ts = now;
            (*prev_count)++;
        }

        (*count)++;
    }
    fclose(f);
}