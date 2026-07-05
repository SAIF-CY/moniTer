#include "process.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int pid;
    char name[BIOS_STR_SHORT];
    double cpu_pct;
    unsigned long long mem_kb;
    double mem_pct;
    unsigned long long utime;
    unsigned long long stime;
} proc_entry_t;

static int cmp_cpu(const void *a, const void *b) {
    const proc_entry_t *pa = a;
    const proc_entry_t *pb = b;
    if (pb->cpu_pct > pa->cpu_pct) {
        return 1;
    }
    if (pb->cpu_pct < pa->cpu_pct) {
        return -1;
    }
    return 0;
}

static int cmp_mem(const void *a, const void *b) {
    const proc_entry_t *pa = a;
    const proc_entry_t *pb = b;
    if (pb->mem_kb > pa->mem_kb) {
        return 1;
    }
    if (pb->mem_kb < pa->mem_kb) {
        return -1;
    }
    return 0;
}

static proc_sample_t *find_prev(proc_sample_t *prev, int prev_count, int pid) {
    for (int i = 0; i < prev_count; i++) {
        if (prev[i].pid == pid) {
            return &prev[i];
        }
    }
    return NULL;
}

static void proc_prev_compact(proc_sample_t *prev, int *prev_count,
                              const int *alive_pids, int alive_count) {
    int write_idx = 0;
    for (int read_idx = 0; read_idx < *prev_count; read_idx++) {
        bool is_alive = false;
        for (int i = 0; i < alive_count; i++) {
            if (prev[read_idx].pid == alive_pids[i]) {
                is_alive = true;
                break;
            }
        }
        if (is_alive) {
            if (write_idx != read_idx) {
                prev[write_idx] = prev[read_idx];
            }
            write_idx++;
        }
    }
    *prev_count = write_idx;
}

static int read_proc_stat(int pid, char *name, size_t namelen,
                          unsigned long long *utime,
                          unsigned long long *stime,
                          unsigned long long *rss_pages) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    char *open_paren = strchr(line, '(');
    char *close_paren = strrchr(line, ')');
    if (!open_paren || !close_paren) {
        return -1;
    }
    size_t len = (size_t)(close_paren - open_paren - 1);
    if (len >= namelen) {
        len = namelen - 1;
    }
    memcpy(name, open_paren + 1, len);
    name[len] = '\0';

    char *rest = close_paren + 2;
    char *fields[30];
    int nf = 0;
    while (nf < 30) {
        while (*rest == ' ') {
            rest++;
        }
        if (*rest == '\0') {
            break;
        }
        fields[nf++] = rest;
        while (*rest && *rest != ' ') {
            rest++;
        }
        if (*rest == '\0') {
            break;
        }
        *rest++ = '\0';
    }
    if (nf < 24) {
        return -1;
    }
    *utime = strtoull(fields[11], NULL, 10);
    *stime = strtoull(fields[12], NULL, 10);
    *rss_pages = strtoull(fields[21], NULL, 10);
    return 0;
}

void bios_process_init(void) {}

void bios_process_collect(bios_process_t *procs, int *count,
                          proc_sample_t *prev, int *prev_count,
                          unsigned long long total_jiffies,
                          unsigned long long total_mem_kb) {
    DIR *d = opendir("/proc");
    if (!d) {
        *count = 0;
        return;
    }

    proc_entry_t entries[512];
    int alive_pids[512];
    int nentries = 0;
    int nalive = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && nentries < 512) {
        if (ent->d_name[0] < '1' || ent->d_name[0] > '9') {
            continue;
        }
        int pid = atoi(ent->d_name);
        if (pid <= 0) {
            continue;
        }

        char name[BIOS_STR_SHORT];
        unsigned long long utime, stime, rss;
        if (read_proc_stat(pid, name, sizeof(name), &utime, &stime, &rss) != 0) {
            continue;
        }

        if (nalive < 512) {
            alive_pids[nalive++] = pid;
        }

        proc_entry_t *e = &entries[nentries++];
        e->pid = pid;
        BIOS_STRNCPY(e->name, name);
        e->utime = utime;
        e->stime = stime;
        e->mem_kb = (rss * (unsigned long long)page_size) / 1024ULL;
        e->mem_pct = total_mem_kb > 0
                         ? 100.0 * (double)e->mem_kb / (double)total_mem_kb
                         : 0.0;
        e->cpu_pct = 0.0;
    }
    closedir(d);

    proc_prev_compact(prev, prev_count, alive_pids, nalive);

    int scan_count = nentries < BIOS_PROC_SCAN_LIMIT ? nentries : BIOS_PROC_SCAN_LIMIT;
    if (nentries > scan_count) {
        qsort(entries, (size_t)nentries, sizeof(entries[0]), cmp_mem);
    }

    for (int i = 0; i < scan_count; i++) {
        proc_entry_t *e = &entries[i];
        proc_sample_t *ps = find_prev(prev, *prev_count, e->pid);
        if (ps) {
            unsigned long long delta =
                (e->utime + e->stime) - (ps->utime + ps->stime);
            if (total_jiffies > 0) {
                e->cpu_pct = 100.0 * (double)delta / (double)total_jiffies;
            }
            ps->utime = e->utime;
            ps->stime = e->stime;
        } else if (*prev_count < 512) {
            prev[*prev_count].pid = e->pid;
            prev[*prev_count].utime = e->utime;
            prev[*prev_count].stime = e->stime;
            (*prev_count)++;
        }
    }

    qsort(entries, (size_t)scan_count, sizeof(entries[0]), cmp_cpu);

    *count = scan_count < BIOS_MAX_PROCESSES ? scan_count : BIOS_MAX_PROCESSES;
    for (int i = 0; i < *count; i++) {
        procs[i].pid = entries[i].pid;
        BIOS_STRNCPY(procs[i].name, entries[i].name);
        procs[i].cpu_pct = entries[i].cpu_pct;
        procs[i].mem_kb = entries[i].mem_kb;
        procs[i].mem_pct = entries[i].mem_pct;
    }
}