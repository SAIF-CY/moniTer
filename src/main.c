/**
 * bios-monitor — Retro BIOS / hacker-style system monitor for Linux.
 * Uses ncurses for TUI and pthreads for background data collection.
 */

#include "data.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    bios_app_state_t state;
    bios_ui_t ui;

    if (bios_data_init(&state) != 0) {
        fprintf(stderr, "bios-monitor: failed to start data collector\n");
        return EXIT_FAILURE;
    }

    if (bios_ui_init(&ui) != 0) {
        bios_data_shutdown(&state);
        fprintf(stderr, "bios-monitor: failed to initialize UI\n");
        return EXIT_FAILURE;
    }

    bios_ui_run(&ui, &state);

    bios_ui_shutdown(&ui);
    bios_data_shutdown(&state);
    return EXIT_SUCCESS;
}