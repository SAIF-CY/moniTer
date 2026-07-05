#ifndef BIOS_MONITOR_DATA_H
#define BIOS_MONITOR_DATA_H

#include "common.h"
#include <pthread.h>
#include <stdatomic.h>

typedef enum {
    BIOS_PANEL_LEFT = 0,
    BIOS_PANEL_RIGHT,
    BIOS_PANEL_COUNT
} bios_panel_t;

typedef struct {
    bios_snapshot_t snapshot;
    pthread_mutex_t mutex;
    pthread_t collector_thread;
    atomic_bool running;
    atomic_bool force_refresh;
    atomic_bool snapshot_updated;
    int active_panel;
    bool hacker_green;
    bool initialized;
} bios_app_state_t;

int bios_data_init(bios_app_state_t *state);
void bios_data_shutdown(bios_app_state_t *state);
void bios_data_get_snapshot(bios_app_state_t *state, bios_snapshot_t *out);
void bios_data_force_refresh(bios_app_state_t *state);
bool bios_data_consume_snapshot_updated(bios_app_state_t *state);

#endif /* BIOS_MONITOR_DATA_H */