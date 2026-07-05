#include "data.h"

#include "cpu.h"
#include "disk.h"
#include "gpu.h"
#include "memory.h"
#include "network.h"
#include "process.h"
#include "sensors.h"

#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>

typedef struct {
    cpu_stat_t cpu_total;
    cpu_stat_t cpu_cores[BIOS_MAX_CPUS];
    int core_count;
    net_sample_t net_prev[BIOS_MAX_NET_IFACES];
    int net_prev_count;
    disk_sample_t disk_prev[BIOS_MAX_DISKS];
    int disk_prev_count;
    proc_sample_t proc_prev[512];
    int proc_prev_count;
    bool first_sample;
} collector_ctx_t;

static void collect_system(bios_system_info_t *sys) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        sys->uptime_sec = si.uptime;
        sys->load_1 = si.loads[0] / 65536.0;
        sys->load_5 = si.loads[1] / 65536.0;
        sys->load_15 = si.loads[2] / 65536.0;
        bios_format_uptime(sys->uptime_str, sizeof(sys->uptime_str), sys->uptime_sec);
    }
}

static void collector_wait(bios_app_state_t *state) {
    int remaining = BIOS_REFRESH_MS;
    while (remaining > 0 && atomic_load(&state->running)) {
        if (atomic_exchange(&state->force_refresh, false)) {
            break;
        }
        int slice = remaining > 50 ? 50 : remaining;
        bios_sleep_ms(slice);
        remaining -= slice;
    }
}

static void *collector_thread(void *arg) {
    bios_app_state_t *state = arg;
    collector_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.first_sample = true;

    while (atomic_load(&state->running)) {
        cpu_stat_t cur_total;
        cpu_stat_t cur_cores[BIOS_MAX_CPUS];
        int cur_core_count = 0;
        if (bios_cpu_read_stats(&cur_total, cur_cores, BIOS_MAX_CPUS, &cur_core_count) != 0) {
            collector_wait(state);
            continue;
        }

        unsigned long long jiffies_delta = 0;
        if (!ctx.first_sample) {
            jiffies_delta = cur_total.total - ctx.cpu_total.total;
        }

        bios_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));

        if (!ctx.first_sample) {
            bios_cpu_collect(&snap.cpu, &cur_total, &ctx.cpu_total,
                             cur_cores, ctx.cpu_cores, cur_core_count);
        } else {
            bios_cpu_init(&snap.cpu);
        }

        bios_memory_collect(&snap.memory);
        bios_gpu_collect(snap.gpus, &snap.gpu_count);
        bios_network_collect(snap.net, &snap.net_count, ctx.net_prev, &ctx.net_prev_count);
        bios_disk_collect(snap.disks, &snap.disk_count, ctx.disk_prev, &ctx.disk_prev_count);
        bios_sensors_collect(snap.sensors, &snap.sensor_count);

        if (!ctx.first_sample && jiffies_delta > 0) {
            bios_process_collect(snap.processes, &snap.process_count,
                                 ctx.proc_prev, &ctx.proc_prev_count, jiffies_delta,
                                 snap.memory.total_kb);
        }

        collect_system(&snap.system);
        snap.collected_at = time(NULL);
        snap.valid = !ctx.first_sample;

        pthread_mutex_lock(&state->mutex);
        state->snapshot = snap;
        pthread_mutex_unlock(&state->mutex);
        atomic_store(&state->snapshot_updated, true);

        ctx.cpu_total = cur_total;
        memcpy(ctx.cpu_cores, cur_cores, sizeof(cur_cores[0]) * (size_t)cur_core_count);
        ctx.core_count = cur_core_count;
        ctx.first_sample = false;

        collector_wait(state);
    }
    return NULL;
}

int bios_data_init(bios_app_state_t *state) {
    memset(state, 0, sizeof(*state));
    pthread_mutex_init(&state->mutex, NULL);
    atomic_store(&state->running, true);
    atomic_store(&state->force_refresh, false);
    atomic_store(&state->snapshot_updated, false);
    state->active_panel = BIOS_PANEL_LEFT;

    bios_gpu_init();
    bios_sensors_init();
    bios_network_init();
    bios_disk_init();
    bios_process_init();
    bios_cpu_init(&state->snapshot.cpu);
    bios_memory_init(&state->snapshot.memory);

    if (pthread_create(&state->collector_thread, NULL, collector_thread, state) != 0) {
        bios_sensors_shutdown();
        bios_gpu_shutdown();
        pthread_mutex_destroy(&state->mutex);
        return -1;
    }

    state->initialized = true;
    return 0;
}

void bios_data_shutdown(bios_app_state_t *state) {
    if (!state->initialized) {
        return;
    }
    atomic_store(&state->running, false);
    pthread_join(state->collector_thread, NULL);
    bios_sensors_shutdown();
    bios_gpu_shutdown();
    pthread_mutex_destroy(&state->mutex);
    state->initialized = false;
}

void bios_data_get_snapshot(bios_app_state_t *state, bios_snapshot_t *out) {
    pthread_mutex_lock(&state->mutex);
    *out = state->snapshot;
    pthread_mutex_unlock(&state->mutex);
}

void bios_data_force_refresh(bios_app_state_t *state) {
    atomic_store(&state->force_refresh, true);
}

bool bios_data_consume_snapshot_updated(bios_app_state_t *state) {
    return atomic_exchange(&state->snapshot_updated, false);
}