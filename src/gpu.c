#include "gpu.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_NVML
#include <nvml.h>
static bool nvml_ok = false;
#endif

void bios_gpu_init(void) {
#ifdef HAVE_NVML
    nvmlReturn_t r = nvmlInit();
    nvml_ok = (r == NVML_SUCCESS);
#endif
}

void bios_gpu_shutdown(void) {
#ifdef HAVE_NVML
    if (nvml_ok) {
        nvmlShutdown();
        nvml_ok = false;
    }
#endif
}

#ifdef HAVE_NVML
static void collect_nvml(bios_gpu_info_t *gpus, int *count) {
    if (!nvml_ok) {
        return;
    }
    unsigned int dev_count = 0;
    if (nvmlDeviceGetCount(&dev_count) != NVML_SUCCESS) {
        return;
    }
    for (unsigned int i = 0; i < dev_count && *count < BIOS_MAX_GPU; i++) {
        nvmlDevice_t dev;
        if (nvmlDeviceGetHandleByIndex(i, &dev) != NVML_SUCCESS) {
            continue;
        }
        bios_gpu_info_t *g = &gpus[*count];
        memset(g, 0, sizeof(*g));
        snprintf(g->name, sizeof(g->name), "GPU%d", i);

        char name[BIOS_STR_SHORT];
        if (nvmlDeviceGetName(dev, name, sizeof(name)) == NVML_SUCCESS) {
            BIOS_STRNCPY(g->name, name);
        }

        nvmlUtilization_t util;
        if (nvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS) {
            g->usage_pct = util.gpu;
        }

        unsigned int temp = 0;
        if (nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
            g->temp_c = temp;
        }

        nvmlMemory_t mem;
        if (nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS) {
            g->mem_total_mb = mem.total / (1024ULL * 1024ULL);
            g->mem_used_mb = mem.used / (1024ULL * 1024ULL);
        }

        g->available = true;
        bios_history_push(&g->usage_history, (float)g->usage_pct);
        (*count)++;
    }
}
#endif

static void collect_sysfs(bios_gpu_info_t *gpus, int *count) {
    DIR *d = opendir("/sys/class/drm");
    if (!d) {
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && *count < BIOS_MAX_GPU) {
        if (strncmp(ent->d_name, "card", 4) != 0) {
            continue;
        }
        /* Skip card0-DP-1 etc */
        for (const char *p = ent->d_name + 4; *p; p++) {
            if (*p < '0' || *p > '9') {
                goto next;
            }
        }

        /* cardN only — bounded copy silences -Wformat-truncation */
        char card[16];
        BIOS_STRNCPY(card, ent->d_name);

        char path[256];
        int plen = snprintf(path, sizeof(path),
                            "/sys/class/drm/%s/device/gpu_busy_percent", card);
        if (plen < 0 || (size_t)plen >= sizeof(path)) {
            goto next;
        }
        long busy = bios_read_long(path);

        bios_gpu_info_t *g = &gpus[*count];
        memset(g, 0, sizeof(*g));
        BIOS_STRNCPY(g->name, card);

        if (busy >= 0) {
            g->usage_pct = busy;
        }

        plen = snprintf(path, sizeof(path),
                        "/sys/class/drm/%s/device/hwmon/hwmon0/temp1_input", card);
        if (plen < 0 || (size_t)plen >= sizeof(path)) {
            goto next;
        }
        long temp = bios_read_long(path);
        if (temp > 0) {
            g->temp_c = temp / 1000.0;
        }

        g->available = (busy >= 0 || temp > 0);
        if (g->available) {
            bios_history_push(&g->usage_history, (float)g->usage_pct);
            (*count)++;
        }
    next:
        continue;
    }
    closedir(d);
}

void bios_gpu_collect(bios_gpu_info_t *gpus, int *count) {
    *count = 0;
#ifdef HAVE_NVML
    collect_nvml(gpus, count);
#endif
    if (*count == 0) {
        collect_sysfs(gpus, count);
    }
}