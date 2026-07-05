#include "cpu.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void parse_cpu_line(const char *line, cpu_stat_t *out) {
    unsigned long long guest = 0, guest_nice = 0;
    sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
           &out->user, &out->nice, &out->system, &out->idle,
           &out->iowait, &out->irq, &out->softirq, &out->steal,
           &guest, &guest_nice);
    (void)guest;
    (void)guest_nice;
    out->idle_total = out->idle + out->iowait;
    out->total = out->user + out->nice + out->system + out->idle +
                 out->iowait + out->irq + out->softirq + out->steal;
}

static void parse_core_line(const char *line, cpu_stat_t *out, int *id) {
    sscanf(line, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu",
           id, &out->user, &out->nice, &out->system, &out->idle,
           &out->iowait, &out->irq, &out->softirq, &out->steal);
    out->idle_total = out->idle + out->iowait;
    out->total = out->user + out->nice + out->system + out->idle +
                 out->iowait + out->irq + out->softirq + out->steal;
}

static double calc_usage(const cpu_stat_t *cur, const cpu_stat_t *prev) {
    if (!prev || prev->total == 0) {
        return 0.0;
    }
    unsigned long long total_d = cur->total - prev->total;
    unsigned long long idle_d = cur->idle_total - prev->idle_total;
    if (total_d == 0) {
        return 0.0;
    }
    return 100.0 * (double)(total_d - idle_d) / (double)total_d;
}

static void read_model(bios_cpu_info_t *cpu) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(cpu->model, sizeof(cpu->model), "Unknown CPU");
        return;
    }
    char line[256];
    cpu->model[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                BIOS_STRNCPY(cpu->model, colon + 2);
                bios_trim(cpu->model);
                break;
            }
        }
    }
    fclose(f);
    if (cpu->model[0] == '\0') {
        snprintf(cpu->model, sizeof(cpu->model), "Unknown CPU");
    }
}

static void read_cache(bios_cpu_info_t *cpu) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cache size", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                BIOS_STRNCPY(cpu->cache_l2, colon + 2);
                bios_trim(cpu->cache_l2);
            }
        }
    }
    fclose(f);

    DIR *d = opendir("/sys/devices/system/cpu/cpu0/cache");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strncmp(ent->d_name, "index", 5) != 0) {
                continue;
            }
            char entry[32];
            BIOS_STRNCPY(entry, ent->d_name);

            char type_path[256];
            int plen = snprintf(type_path, sizeof(type_path),
                                "/sys/devices/system/cpu/cpu0/cache/%s/type", entry);
            if (plen < 0 || (size_t)plen >= sizeof(type_path)) {
                continue;
            }
            char type[32];
            if (bios_read_file(type_path, type, sizeof(type)) == 0 &&
                strcmp(type, "Cache") == 0) {
                char level_path[256];
                plen = snprintf(level_path, sizeof(level_path),
                                "/sys/devices/system/cpu/cpu0/cache/%s/level", entry);
                if (plen < 0 || (size_t)plen >= sizeof(level_path)) {
                    continue;
                }
                long level = bios_read_long(level_path);
                if (level == 3) {
                    char size_path[256];
                    plen = snprintf(size_path, sizeof(size_path),
                                    "/sys/devices/system/cpu/cpu0/cache/%s/size", entry);
                    if (plen >= 0 && (size_t)plen < sizeof(size_path)) {
                        bios_read_file(size_path, cpu->cache_l3, sizeof(cpu->cache_l3));
                    }
                }
            }
        }
        closedir(d);
    }
}

static void read_freq(bios_cpu_info_t *cpu) {
    long khz = bios_read_long("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (khz > 0) {
        cpu->freq_mhz = khz / 1000.0;
        return;
    }
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                cpu->freq_mhz = atof(colon + 2);
                break;
            }
        }
    }
    fclose(f);
}

static void read_thermal(bios_cpu_info_t *cpu) {
    long temp = bios_read_long("/sys/class/thermal/thermal_zone0/temp");
    if (temp > 0) {
        cpu->temp_c = temp / 1000.0;
    }

    cpu->core_temp_count = 0;
    for (int i = 0; i < BIOS_MAX_CPUS && cpu->core_temp_count < BIOS_MAX_CPUS; i++) {
        char direct[256];
        snprintf(direct, sizeof(direct),
                 "/sys/class/hwmon/hwmon0/temp%d_input", i + 2);
        long ct = bios_read_long(direct);
        if (ct > 0) {
            cpu->core_temps[cpu->core_temp_count++] = ct / 1000.0;
        }
    }
}

void bios_cpu_init(bios_cpu_info_t *cpu) {
    memset(cpu, 0, sizeof(*cpu));
    read_model(cpu);
    read_cache(cpu);
    cpu->num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu->num_cores > BIOS_MAX_CPUS) {
        cpu->num_cores = BIOS_MAX_CPUS;
    }
}

void bios_cpu_collect(bios_cpu_info_t *cpu,
                      const cpu_stat_t *cur, const cpu_stat_t *prev,
                      const cpu_stat_t *cur_cores, const cpu_stat_t *prev_cores,
                      int core_count) {
    if (cur && prev) {
        cpu->usage_total = calc_usage(cur, prev);
        bios_history_push(&cpu->usage_history, (float)cpu->usage_total);
    }

    int cores = core_count;
    if (cores > cpu->num_cores) {
        cores = cpu->num_cores;
    }
    for (int i = 0; i < cores; i++) {
        if (cur_cores && prev_cores) {
            cpu->usage_per_core[i] = calc_usage(&cur_cores[i], &prev_cores[i]);
        }
    }

    read_freq(cpu);
    read_thermal(cpu);
}

int bios_cpu_read_stats(cpu_stat_t *total, cpu_stat_t *cores, int max_cores,
                        int *core_count) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        return -1;
    }

    char line[512];
    bool got_total = false;
    int n = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            parse_cpu_line(line, total);
            got_total = true;
        } else if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9' &&
                   n < max_cores) {
            int id = 0;
            parse_core_line(line, &cores[n], &id);
            (void)id;
            n++;
        } else if (got_total && strncmp(line, "cpu", 3) != 0) {
            break;
        }
    }

    fclose(f);
    if (!got_total) {
        return -1;
    }
    *core_count = n;
    return 0;
}