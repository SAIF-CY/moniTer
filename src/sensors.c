#include "sensors.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SENSORS
#include <sensors/sensors.h>
static bool sensors_ok = false;
#endif

void bios_sensors_init(void) {
#ifdef HAVE_SENSORS
    if (sensors_init(NULL) == 0) {
        sensors_ok = true;
    }
#endif
}

void bios_sensors_shutdown(void) {
#ifdef HAVE_SENSORS
    if (sensors_ok) {
        sensors_cleanup();
        sensors_ok = false;
    }
#endif
}

static void collect_thermal_zones(bios_sensor_t *sensors, int *count) {
    DIR *d = opendir("/sys/class/thermal");
    if (!d) {
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && *count < BIOS_MAX_SENSORS) {
        if (strncmp(ent->d_name, "thermal_zone", 12) != 0) {
            continue;
        }
        char zone[32];
        BIOS_STRNCPY(zone, ent->d_name);

        char type_path[256];
        char temp_path[256];
        int plen = snprintf(type_path, sizeof(type_path),
                            "/sys/class/thermal/%s/type", zone);
        if (plen < 0 || (size_t)plen >= sizeof(type_path)) {
            continue;
        }
        plen = snprintf(temp_path, sizeof(temp_path),
                        "/sys/class/thermal/%s/temp", zone);
        if (plen < 0 || (size_t)plen >= sizeof(temp_path)) {
            continue;
        }

        char type[BIOS_STR_SHORT];
        long temp = bios_read_long(temp_path);
        if (temp <= 0) {
            continue;
        }
        if (bios_read_file(type_path, type, sizeof(type)) != 0) {
            BIOS_STRNCPY(type, zone);
        }

        bios_sensor_t *s = &sensors[*count];
        BIOS_STRNCPY(s->name, zone);
        BIOS_STRNCPY(s->label, type);
        s->value_c = temp / 1000.0;
        (*count)++;
    }
    closedir(d);
}

#ifdef HAVE_SENSORS
static void collect_libsensors(bios_sensor_t *sensors, int *count) {
    if (!sensors_ok) {
        return;
    }
    const sensors_chip_name *chip;
    int chip_nr = 0;
    while ((chip = sensors_get_detected_chips(NULL, &chip_nr)) != NULL &&
           *count < BIOS_MAX_SENSORS) {
        const sensors_feature *feat;
        int feat_nr = 0;
        while ((feat = sensors_get_features(chip, &feat_nr)) != NULL &&
               *count < BIOS_MAX_SENSORS) {
            if (feat->type != SENSORS_FEATURE_TEMP) {
                continue;
            }
            const sensors_subfeature *sf =
                sensors_get_subfeature(chip, feat, SENSORS_SUBFEATURE_TEMP_INPUT);
            if (!sf) {
                continue;
            }
            double val = 0;
            if (sensors_get_value(chip, sf->number, &val) != 0) {
                continue;
            }
            bios_sensor_t *s = &sensors[*count];
            snprintf(s->name, sizeof(s->name), "%s", chip->prefix);
            snprintf(s->label, sizeof(s->label), "%s", feat->name);
            s->value_c = val;
            (*count)++;
        }
    }
}
#endif

void bios_sensors_collect(bios_sensor_t *sensors, int *count) {
    *count = 0;
#ifdef HAVE_SENSORS
    collect_libsensors(sensors, count);
#endif
    if (*count < 4) {
        collect_thermal_zones(sensors, count);
    }
}