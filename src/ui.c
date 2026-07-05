#include "ui.h"
#include "util.h"

#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Retro BIOS palette */
#define COL_BG          COLOR_PAIR(1)
#define COL_CYAN        COLOR_PAIR(2)
#define COL_GREEN       COLOR_PAIR(3)
#define COL_WHITE       COLOR_PAIR(4)
#define COL_DIM         COLOR_PAIR(5)
#define COL_HIGHLIGHT   COLOR_PAIR(6)
#define COL_BAR_FILL    COLOR_PAIR(7)
#define COL_BAR_EMPTY   COLOR_PAIR(8)

typedef struct {
    int y, x, h, w;
    const char *title;
} bios_panel_rect_t;

static volatile sig_atomic_t g_resize = 0;

static void on_resize(int sig) {
    (void)sig;
    g_resize = 1;
}

static void init_colors(bool hacker_green) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_BLACK);
    if (hacker_green) {
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
        init_pair(5, COLOR_GREEN, COLOR_BLACK);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);
        init_pair(7, COLOR_GREEN, COLOR_BLACK);
        init_pair(8, COLOR_BLACK, COLOR_GREEN);
    } else {
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        init_pair(3, COLOR_GREEN, COLOR_BLACK);
        init_pair(4, COLOR_WHITE, COLOR_BLACK);
        init_pair(5, COLOR_CYAN, COLOR_BLACK);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);
        init_pair(7, COLOR_CYAN, COLOR_BLACK);
        init_pair(8, COLOR_BLACK, COLOR_CYAN);
    }
}

static void draw_box(WINDOW *win, int y, int x, int h, int w, const char *title,
                     bool active) {
    int attr = active ? COL_HIGHLIGHT : COL_CYAN;
    wattron(win, attr);
    mvwaddch(win, y, x, ACS_ULCORNER);
    mvwaddch(win, y, x + w - 1, ACS_URCORNER);
    mvwaddch(win, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(win, y + h - 1, x + w - 1, ACS_LRCORNER);
    for (int i = 1; i < w - 1; i++) {
        mvwaddch(win, y, x + i, ACS_HLINE);
        mvwaddch(win, y + h - 1, x + i, ACS_HLINE);
    }
    for (int i = 1; i < h - 1; i++) {
        mvwaddch(win, y + i, x, ACS_VLINE);
        mvwaddch(win, y + i, x + w - 1, ACS_VLINE);
    }
    if (title && w > (int)strlen(title) + 4) {
        mvwprintw(win, y, x + 2, " %s ", title);
    }
    wattroff(win, attr);
}

static void draw_dots_line(WINDOW *win, int y, int x, int width,
                           const char *label, const char *value, int attr) {
    char buf[256];
    bios_snprintf_dots(buf, sizeof(buf), label, value, width - 2);
    wattron(win, attr);
    mvwprintw(win, y, x + 1, "%-*s", width - 2, buf);
    wattroff(win, attr);
}

static void draw_bar(WINDOW *win, int y, int x, int width, double pct) {
    if (width < 4) {
        return;
    }
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }
    int fill = (int)((pct / 100.0) * (width - 2));
    wattron(win, COL_BAR_EMPTY);
    mvwaddch(win, y, x, '[');
    wattroff(win, COL_BAR_EMPTY);
    for (int i = 0; i < width - 2; i++) {
        if (i < fill) {
            wattron(win, COL_BAR_FILL);
            mvwaddch(win, y, x + 1 + i, ACS_BLOCK);
            wattroff(win, COL_BAR_FILL);
        } else {
            wattron(win, COL_DIM);
            mvwaddch(win, y, x + 1 + i, ACS_BULLET);
            wattroff(win, COL_DIM);
        }
    }
    wattron(win, COL_BAR_EMPTY);
    mvwaddch(win, y, x + width - 1, ']');
    wattroff(win, COL_BAR_EMPTY);
}

static void draw_sparkline(WINDOW *win, int y, int x, int width,
                           const bios_history_t *hist) {
    static const char levels[] = "_.-:=+*#%@";
    int nlevels = (int)sizeof(levels) - 1;
    int samples = hist->count < width ? hist->count : width;
    wattron(win, COL_GREEN);
    for (int i = 0; i < width; i++) {
        if (i < samples) {
            float v = bios_history_get(hist, samples - 1 - i);
            int idx = (int)((v / 100.0f) * (nlevels - 1));
            if (idx < 0) {
                idx = 0;
            }
            if (idx >= nlevels) {
                idx = nlevels - 1;
            }
            mvwaddch(win, y, x + i, levels[idx]);
        } else {
            mvwaddch(win, y, x + i, ' ');
        }
    }
    wattroff(win, COL_GREEN);
}

static void draw_header(WINDOW *win, int cols, bool hacker_green) {
    const char *title = "MR BIOS SYSTEM MONITOR v1.0";
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);

    wattron(win, COL_WHITE);
    mvwprintw(win, 0, 2, "%s", title);
    mvwprintw(win, 0, cols - (int)strlen(timestr) - 2, "%s", timestr);
    wattroff(win, COL_WHITE);

    wattron(win, COL_DIM);
    mvwprintw(win, 1, 2, "Mode: %s", hacker_green ? "HACKER GREEN" : "BIOS CYAN");
    mvwprintw(win, 1, cols - 42, "[Q]uit [R]efresh [C]olor [1/2] Panel");
    wattroff(win, COL_DIM);
}

static void draw_footer(WINDOW *win, int rows, int cols, int active_panel) {
    (void)cols;
    wattron(win, COL_DIM);
    mvwprintw(win, rows - 1, 2,
              "Active Panel: %s | F1=Help",
              active_panel == BIOS_PANEL_LEFT ? "HARDWARE SUMMARY" : "SYSTEM STATUS");
    wattroff(win, COL_DIM);
}

static void draw_left_panel(WINDOW *win, const bios_snapshot_t *snap,
                            const bios_panel_rect_t *p, bool active) {
    draw_box(win, p->y, p->x, p->h, p->w, p->title, active);
    int y = p->y + 2;
    int inner_w = p->w - 2;
    char val[BIOS_STR_MED];

    wattron(win, COL_GREEN);
    mvwprintw(win, y++, p->x + 2, "PROCESSOR");
    wattroff(win, COL_GREEN);

    snprintf(val, sizeof(val), "%.0f%%", snap->cpu.usage_total);
    draw_dots_line(win, y++, p->x, inner_w, "CPU Usage", val, COL_CYAN);
    draw_bar(win, y++, p->x + 2, inner_w - 4, snap->cpu.usage_total);
    y++;

    char model[BIOS_STR_LONG];
    snprintf(model, sizeof(model), "%.28s", snap->cpu.model);
    draw_dots_line(win, y++, p->x, inner_w, "Model", model, COL_DIM);

    snprintf(val, sizeof(val), "%.0f MHz", snap->cpu.freq_mhz);
    draw_dots_line(win, y++, p->x, inner_w, "Frequency", val, COL_DIM);

    if (snap->cpu.temp_c > 0) {
        snprintf(val, sizeof(val), "%.1f C", snap->cpu.temp_c);
        draw_dots_line(win, y++, p->x, inner_w, "CPU Temp", val, COL_GREEN);
    }

    if (snap->cpu.cache_l2[0]) {
        draw_dots_line(win, y++, p->x, inner_w, "Cache L2", snap->cpu.cache_l2, COL_DIM);
    }
    if (snap->cpu.cache_l3[0]) {
        draw_dots_line(win, y++, p->x, inner_w, "Cache L3", snap->cpu.cache_l3, COL_DIM);
    }

    /* Per-core usage (compact) */
    if (snap->cpu.num_cores > 0 && y < p->y + p->h - 8) {
        wattron(win, COL_DIM);
        mvwprintw(win, y++, p->x + 2, "CORES:");
        wattroff(win, COL_DIM);
        int per_row = (inner_w - 2) / 8;
        if (per_row < 1) {
            per_row = 1;
        }
        for (int i = 0; i < snap->cpu.num_cores && i < 16; i++) {
            int col = i % per_row;
            int row = i / per_row;
            snprintf(val, sizeof(val), "C%02d:%3.0f%%", i, snap->cpu.usage_per_core[i]);
            wattron(win, COL_CYAN);
            mvwprintw(win, y + row, p->x + 2 + col * 8, "%s", val);
            wattroff(win, COL_CYAN);
        }
        y += (snap->cpu.num_cores + per_row - 1) / per_row;
        if (y < p->y + p->h - 2) {
            draw_sparkline(win, y++, p->x + 2, inner_w - 4, &snap->cpu.usage_history);
        }
    }

    if (y < p->y + p->h - 6) {
        wattron(win, COL_GREEN);
        mvwprintw(win, y++, p->x + 2, "MEMORY");
        wattroff(win, COL_GREEN);

        char used[32], total[32];
        bios_format_bytes(used, sizeof(used), snap->memory.used_kb);
        bios_format_bytes(total, sizeof(total), snap->memory.total_kb);
        snprintf(val, sizeof(val), "%s / %s", used, total);
        draw_dots_line(win, y++, p->x, inner_w, "RAM Used", val, COL_CYAN);
        snprintf(val, sizeof(val), "%.1f%%", snap->memory.usage_pct);
        draw_dots_line(win, y++, p->x, inner_w, "RAM Usage", val, COL_CYAN);
        draw_bar(win, y++, p->x + 2, inner_w - 4, snap->memory.usage_pct);

        snprintf(val, sizeof(val), "%.1f%%", snap->memory.swap_pct);
        draw_dots_line(win, y++, p->x, inner_w, "Swap", val, COL_DIM);
        draw_sparkline(win, y++, p->x + 2, inner_w - 4, &snap->memory.usage_history);
    }

    if (y < p->y + p->h - 4 && snap->gpu_count > 0) {
        wattron(win, COL_GREEN);
        mvwprintw(win, y++, p->x + 2, "GPU");
        wattroff(win, COL_GREEN);
        for (int g = 0; g < snap->gpu_count && y < p->y + p->h - 2; g++) {
            const bios_gpu_info_t *gpu = &snap->gpus[g];
            char gname[20];
            snprintf(gname, sizeof(gname), "%.12s", gpu->name);
            snprintf(val, sizeof(val), "%.0f%%", gpu->usage_pct);
            draw_dots_line(win, y++, p->x, inner_w, gname, val, COL_CYAN);
            if (gpu->temp_c > 0) {
                snprintf(val, sizeof(val), "%.0f C", gpu->temp_c);
                draw_dots_line(win, y++, p->x, inner_w, "  Temp", val, COL_DIM);
            }
            if (gpu->mem_total_mb > 0) {
                snprintf(val, sizeof(val), "%llu/%llu MB",
                         gpu->mem_used_mb, gpu->mem_total_mb);
                draw_dots_line(win, y++, p->x, inner_w, "  VRAM", val, COL_DIM);
            }
        }
    }
}

static void draw_right_panel(WINDOW *win, const bios_snapshot_t *snap,
                             const bios_panel_rect_t *p, bool active) {
    draw_box(win, p->y, p->x, p->h, p->w, p->title, active);
    int y = p->y + 2;
    int inner_w = p->w - 2;
    char val[BIOS_STR_MED];

    wattron(win, COL_GREEN);
    mvwprintw(win, y++, p->x + 2, "STORAGE");
    wattroff(win, COL_GREEN);

    for (int i = 0; i < snap->disk_count && y < p->y + p->h - 14; i++) {
        const bios_disk_info_t *d = &snap->disks[i];
        snprintf(val, sizeof(val), "%.0f%%", d->usage_pct);
        draw_dots_line(win, y++, p->x, inner_w, d->name, val, COL_CYAN);
        snprintf(val, sizeof(val), "R:%.1f W:%.1f MB/s", d->read_mbs, d->write_mbs);
        wattron(win, COL_DIM);
        mvwprintw(win, y++, p->x + 2, "%s on %s", val, d->mount);
        wattroff(win, COL_DIM);
    }

    wattron(win, COL_GREEN);
    mvwprintw(win, y++, p->x + 2, "NETWORK");
    wattroff(win, COL_GREEN);

    for (int i = 0; i < snap->net_count && y < p->y + p->h - 10; i++) {
        const bios_net_iface_t *n = &snap->net[i];
        snprintf(val, sizeof(val), "DN:%.1f UP:%.1f KB/s", n->rx_kbps, n->tx_kbps);
        draw_dots_line(win, y++, p->x, inner_w, n->name, val, COL_CYAN);
    }

    wattron(win, COL_GREEN);
    mvwprintw(win, y++, p->x + 2, "SYSTEM");
    wattroff(win, COL_GREEN);

    snprintf(val, sizeof(val), "%.2f %.2f %.2f",
             snap->system.load_1, snap->system.load_5, snap->system.load_15);
    draw_dots_line(win, y++, p->x, inner_w, "Load Avg", val, COL_DIM);
    draw_dots_line(win, y++, p->x, inner_w, "Uptime", snap->system.uptime_str, COL_DIM);

    if (snap->sensor_count > 0 && y < p->y + p->h - 8) {
        wattron(win, COL_GREEN);
        mvwprintw(win, y++, p->x + 2, "SENSORS");
        wattroff(win, COL_GREEN);
        int shown = 0;
        for (int i = 0; i < snap->sensor_count && shown < 4 && y < p->y + p->h - 6; i++) {
            snprintf(val, sizeof(val), "%.1f C", snap->sensors[i].value_c);
            draw_dots_line(win, y++, p->x, inner_w, snap->sensors[i].label, val, COL_DIM);
            shown++;
        }
    }

    if (y < p->y + p->h - 4) {
        wattron(win, COL_GREEN);
        mvwprintw(win, y++, p->x + 2, "TOP PROCESSES");
        wattroff(win, COL_GREEN);
        wattron(win, COL_DIM);
        mvwprintw(win, y++, p->x + 2, "%-6s %-14s %6s %6s", "PID", "NAME", "CPU%", "MEM%");
        wattroff(win, COL_DIM);
        for (int i = 0; i < snap->process_count && y < p->y + p->h - 2; i++) {
            const bios_process_t *pr = &snap->processes[i];
            char pname[15];
            snprintf(pname, sizeof(pname), "%.14s", pr->name);
            wattron(win, COL_CYAN);
            mvwprintw(win, y++, p->x + 2, "%-6d %-14s %5.1f%% %5.1f%%",
                      pr->pid, pname, pr->cpu_pct, pr->mem_pct);
            wattroff(win, COL_CYAN);
        }
    }
}

static void render(bios_ui_t *ui, bios_app_state_t *state) {
    bios_snapshot_t snap;
    bios_data_get_snapshot(state, &snap);

    int rows = ui->rows;
    int cols = ui->cols;
    int mid = cols / 2;

    werase(stdscr);
    bkgd(COL_BG);

    draw_header(stdscr, cols, state->hacker_green);

    bios_panel_rect_t left = {
        .y = 3, .x = 1, .h = rows - 5, .w = mid - 2,
        .title = "Hardware Summary"
    };
    bios_panel_rect_t right = {
        .y = 3, .x = mid, .h = rows - 5, .w = cols - mid - 1,
        .title = "System Status"
    };

    draw_left_panel(stdscr, &snap, &left, state->active_panel == BIOS_PANEL_LEFT);
    draw_right_panel(stdscr, &snap, &right, state->active_panel == BIOS_PANEL_RIGHT);

    if (!snap.valid) {
        wattron(stdscr, COL_WHITE);
        mvwprintw(stdscr, rows / 2, cols / 2 - 10, " CALIBRATING... ");
        wattroff(stdscr, COL_WHITE);
    }

    draw_footer(stdscr, rows, cols, state->active_panel);
    refresh();
}

int bios_ui_init(bios_ui_t *ui) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    timeout(50);

    signal(SIGWINCH, on_resize);

    getmaxyx(stdscr, ui->rows, ui->cols);
    init_colors(false);
    ui->initialized = true;
    return 0;
}

void bios_ui_shutdown(bios_ui_t *ui) {
    (void)ui;
    endwin();
}

void bios_ui_handle_resize(bios_ui_t *ui) {
    endwin();
    refresh();
    clear();
    getmaxyx(stdscr, ui->rows, ui->cols);
}

static double timespec_elapsed_ms(const struct timespec *start,
                                  const struct timespec *end) {
    double s = start->tv_sec + start->tv_nsec / 1e9;
    double e = end->tv_sec + end->tv_nsec / 1e9;
    return (e - s) * 1000.0;
}

int bios_ui_run(bios_ui_t *ui, bios_app_state_t *state) {
    struct timespec last_render;
    clock_gettime(CLOCK_MONOTONIC, &last_render);

    while (atomic_load(&state->running)) {
        bool needs_render = false;

        if (g_resize) {
            g_resize = 0;
            bios_ui_handle_resize(ui);
            needs_render = true;
        }

        if (bios_data_consume_snapshot_updated(state)) {
            needs_render = true;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (timespec_elapsed_ms(&last_render, &now) >=
            1000.0 / (double)BIOS_UI_MAX_FPS) {
            needs_render = true;
        }

        int ch = getch();
        switch (ch) {
        case 'q':
        case 'Q':
            atomic_store(&state->running, false);
            break;
        case 'r':
        case 'R':
            bios_data_force_refresh(state);
            break;
        case 'c':
        case 'C':
            state->hacker_green = !state->hacker_green;
            init_colors(state->hacker_green);
            needs_render = true;
            break;
        case '1':
        case KEY_LEFT:
            state->active_panel = BIOS_PANEL_LEFT;
            needs_render = true;
            break;
        case '2':
        case KEY_RIGHT:
            state->active_panel = BIOS_PANEL_RIGHT;
            needs_render = true;
            break;
        case KEY_RESIZE:
            bios_ui_handle_resize(ui);
            needs_render = true;
            break;
        default:
            break;
        }

        if (needs_render) {
            render(ui, state);
            clock_gettime(CLOCK_MONOTONIC, &last_render);
        } else {
            bios_sleep_ms(50);
        }
    }
    return 0;
}