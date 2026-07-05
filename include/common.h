#ifndef BIOS_MONITOR_COMMON_H
#define BIOS_MONITOR_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define BIOS_MAX_CPUS        256
#define BIOS_MAX_NET_IFACES  16
#define BIOS_MAX_DISKS       16
#define BIOS_MAX_PROCESSES   8
#define BIOS_MAX_SENSORS     32
#define BIOS_MAX_GPU         4
#define BIOS_HISTORY_LEN     60
#define BIOS_STR_SHORT       64
#define BIOS_STR_MED         128
#define BIOS_STR_LONG        256

#define BIOS_REFRESH_MS      1000
#define BIOS_PROC_SCAN_LIMIT 200
#define BIOS_UI_MAX_FPS      1

/* Safe copy: always null-terminates dest. dest must be a char array. */
#define BIOS_STRNCPY(dest, src) bios_strncpy((dest), (src), sizeof(dest))

typedef struct {
    char label[BIOS_STR_SHORT];
    float values[BIOS_HISTORY_LEN];
    int head;
    int count;
} bios_history_t;

typedef struct {
    char model[BIOS_STR_LONG];
    double freq_mhz;
    double usage_total;
    double usage_per_core[BIOS_MAX_CPUS];
    int num_cores;
    double temp_c;
    double core_temps[BIOS_MAX_CPUS];
    int core_temp_count;
    char cache_l2[BIOS_STR_SHORT];
    char cache_l3[BIOS_STR_SHORT];
    bios_history_t usage_history;
} bios_cpu_info_t;

typedef struct {
    unsigned long long total_kb;
    unsigned long long used_kb;
    unsigned long long free_kb;
    unsigned long long avail_kb;
    double usage_pct;
    unsigned long long swap_total_kb;
    unsigned long long swap_used_kb;
    double swap_pct;
    bios_history_t usage_history;
} bios_memory_info_t;

typedef struct {
    char name[BIOS_STR_SHORT];
    double usage_pct;
    double temp_c;
    unsigned long long mem_used_mb;
    unsigned long long mem_total_mb;
    bool available;
    bios_history_t usage_history;
} bios_gpu_info_t;

typedef struct {
    char name[BIOS_STR_SHORT];
    double rx_kbps;
    double tx_kbps;
    unsigned long long rx_total;
    unsigned long long tx_total;
    bool up;
} bios_net_iface_t;

typedef struct {
    char name[BIOS_STR_SHORT];
    char mount[BIOS_STR_SHORT];
    double usage_pct;
    unsigned long long total_gb;
    unsigned long long used_gb;
    double read_mbs;
    double write_mbs;
} bios_disk_info_t;

typedef struct {
    char name[BIOS_STR_SHORT];
    char label[BIOS_STR_SHORT];
    double value_c;
} bios_sensor_t;

typedef struct {
    int pid;
    char name[BIOS_STR_SHORT];
    double cpu_pct;
    unsigned long long mem_kb;
    double mem_pct;
} bios_process_t;

typedef struct {
    double load_1;
    double load_5;
    double load_15;
    long uptime_sec;
    char uptime_str[BIOS_STR_SHORT];
} bios_system_info_t;

typedef struct {
    bios_cpu_info_t cpu;
    bios_memory_info_t memory;
    bios_gpu_info_t gpus[BIOS_MAX_GPU];
    int gpu_count;
    bios_net_iface_t net[BIOS_MAX_NET_IFACES];
    int net_count;
    bios_disk_info_t disks[BIOS_MAX_DISKS];
    int disk_count;
    bios_sensor_t sensors[BIOS_MAX_SENSORS];
    int sensor_count;
    bios_process_t processes[BIOS_MAX_PROCESSES];
    int process_count;
    bios_system_info_t system;
    time_t collected_at;
    bool valid;
} bios_snapshot_t;

void bios_history_push(bios_history_t *h, float value);
float bios_history_get(const bios_history_t *h, int offset);
void bios_format_bytes(char *buf, size_t len, unsigned long long kb);
void bios_format_uptime(char *buf, size_t len, long seconds);
void bios_strncpy(char *dest, const char *src, size_t dest_size);
void bios_trim(char *s);
int bios_read_file(const char *path, char *buf, size_t len);
long bios_read_long(const char *path);
void bios_sleep_ms(int ms);

#endif /* BIOS_MONITOR_COMMON_H */