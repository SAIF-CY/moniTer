#include "disk.h"

#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>

#define SECTOR_SIZE 512
#define DISKSTATS_MAX 64

typedef struct {
    char name[BIOS_STR_SHORT];
    unsigned long long read_sectors;
    unsigned long long write_sectors;
} diskstats_row_t;

static double elapsed_sec(const struct timespec *a, const struct timespec *b) {
    double da = a->tv_sec + a->tv_nsec / 1e9;
    double db = b->tv_sec + b->tv_nsec / 1e9;
    double d = db - da;
    return d > 0.001 ? d : 1.0;
}

static int find_disk_prev(disk_sample_t *prev, int prev_count, const char *name) {
    for (int i = 0; i < prev_count; i++) {
        if (strcmp(prev[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int read_diskstats_all(diskstats_row_t *rows, int max_rows) {
    FILE *ds = fopen("/proc/diskstats", "r");
    if (!ds) {
        return 0;
    }

    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), ds) && count < max_rows) {
        char ds_name[BIOS_STR_SHORT];
        unsigned long long rd_sectors, wr_sectors;
        int major, minor;
        unsigned long long dummy;
        int fields = sscanf(line, "%d %d %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                            &major, &minor, ds_name,
                            &dummy, &rd_sectors, &dummy, &dummy, &dummy,
                            &dummy, &wr_sectors, &dummy, &dummy, &dummy, &dummy);
        if (fields < 10) {
            continue;
        }
        BIOS_STRNCPY(rows[count].name, ds_name);
        rows[count].read_sectors = rd_sectors;
        rows[count].write_sectors = wr_sectors;
        count++;
    }
    fclose(ds);
    return count;
}

static const diskstats_row_t *match_diskstats(const diskstats_row_t *rows, int row_count,
                                              const char *devname) {
    size_t dv_len = strlen(devname);
    for (int i = 0; i < row_count; i++) {
        size_t ds_len = strlen(rows[i].name);
        if (strncmp(rows[i].name, devname, ds_len) == 0 ||
            strncmp(devname, rows[i].name, dv_len) == 0) {
            return &rows[i];
        }
    }
    return NULL;
}

static bool is_virtual_fs(const char *fstype) {
    return strcmp(fstype, "proc") == 0 || strcmp(fstype, "sysfs") == 0 ||
           strcmp(fstype, "devtmpfs") == 0 || strcmp(fstype, "tmpfs") == 0 ||
           strcmp(fstype, "devpts") == 0 || strcmp(fstype, "cgroup") == 0 ||
           strcmp(fstype, "cgroup2") == 0 || strcmp(fstype, "pstore") == 0 ||
           strcmp(fstype, "bpf") == 0 || strcmp(fstype, "tracefs") == 0 ||
           strcmp(fstype, "debugfs") == 0 || strcmp(fstype, "securityfs") == 0 ||
           strcmp(fstype, "mqueue") == 0 || strcmp(fstype, "hugetlbfs") == 0;
}

void bios_disk_init(void) {}

void bios_disk_collect(bios_disk_info_t *disks, int *count,
                       disk_sample_t *prev, int *prev_count) {
    *count = 0;

    diskstats_row_t stats[DISKSTATS_MAX];
    int stats_count = read_diskstats_all(stats, DISKSTATS_MAX);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    FILE *mtab = setmntent("/proc/mounts", "r");
    if (!mtab) {
        return;
    }

    struct mntent *mnt;
    while ((mnt = getmntent(mtab)) != NULL && *count < BIOS_MAX_DISKS) {
        if (is_virtual_fs(mnt->mnt_type)) {
            continue;
        }
        if (strncmp(mnt->mnt_fsname, "/dev/", 5) != 0) {
            continue;
        }

        struct statvfs st;
        if (statvfs(mnt->mnt_dir, &st) != 0) {
            continue;
        }

        bios_disk_info_t *d = &disks[*count];
        memset(d, 0, sizeof(*d));

        const char *devname = strrchr(mnt->mnt_fsname, '/');
        devname = devname ? devname + 1 : mnt->mnt_fsname;
        BIOS_STRNCPY(d->name, devname);
        BIOS_STRNCPY(d->mount, mnt->mnt_dir);

        unsigned long long total = (unsigned long long)st.f_blocks * st.f_frsize;
        unsigned long long avail = (unsigned long long)st.f_bavail * st.f_frsize;
        unsigned long long used = total - avail;
        d->total_gb = total / (1024ULL * 1024ULL * 1024ULL);
        d->used_gb = used / (1024ULL * 1024ULL * 1024ULL);
        if (total > 0) {
            d->usage_pct = 100.0 * (double)used / (double)total;
        }

        const diskstats_row_t *row = match_diskstats(stats, stats_count, devname);
        if (row) {
            int pidx = find_disk_prev(prev, *prev_count, row->name);
            if (pidx >= 0) {
                double dt = elapsed_sec(&prev[pidx].ts, &now);
                double rd = (double)(row->read_sectors - prev[pidx].read_sectors) * SECTOR_SIZE / dt;
                double wr = (double)(row->write_sectors - prev[pidx].write_sectors) * SECTOR_SIZE / dt;
                d->read_mbs = rd / (1024.0 * 1024.0);
                d->write_mbs = wr / (1024.0 * 1024.0);
                prev[pidx].read_sectors = row->read_sectors;
                prev[pidx].write_sectors = row->write_sectors;
                prev[pidx].ts = now;
            } else if (*prev_count < BIOS_MAX_DISKS) {
                BIOS_STRNCPY(prev[*prev_count].name, row->name);
                prev[*prev_count].read_sectors = row->read_sectors;
                prev[*prev_count].write_sectors = row->write_sectors;
                prev[*prev_count].ts = now;
                (*prev_count)++;
            }
        }

        (*count)++;
    }
    endmntent(mtab);
}