#ifndef BIOS_MONITOR_UI_H
#define BIOS_MONITOR_UI_H

#include "data.h"

typedef struct {
    int rows;
    int cols;
    bool initialized;
} bios_ui_t;

int bios_ui_init(bios_ui_t *ui);
void bios_ui_shutdown(bios_ui_t *ui);
void bios_ui_handle_resize(bios_ui_t *ui);
int bios_ui_run(bios_ui_t *ui, bios_app_state_t *state);

#endif /* BIOS_MONITOR_UI_H */