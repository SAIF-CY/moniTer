#ifndef BIOS_MONITOR_SENSORS_H
#define BIOS_MONITOR_SENSORS_H

#include "common.h"

void bios_sensors_init(void);
void bios_sensors_shutdown(void);
void bios_sensors_collect(bios_sensor_t *sensors, int *count);

#endif /* BIOS_MONITOR_SENSORS_H */