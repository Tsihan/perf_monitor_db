/*
 * libperfmon - Performance Monitoring Library Implementation
 */

#define _GNU_SOURCE
#include "perfmon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <time.h>

/* Thread-local error message */
static __thread char error_msg[256] = {0};

/* Performance counter file descriptor */
typedef struct {
    int fd;
    bool enabled;
} perf_counter_t;

/* Performance monitor context structure */
struct perfmon_context {
    perf_counter_t counters[PERFMON_MAX_COUNTERS];
    struct timespec start_time;
    struct timespec end_time;
    bool is_running;
};

/* Set error message */
static void set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(error_msg, sizeof(error_msg), fmt, args);
    va_end(args);
}

/* Wrapper for perf_event_open syscall */
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

/* Setup a single performance counter */
static int setup_counter(uint32_t type, uint64_t config) {
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = type;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = config;
    pe.disabled = 1;
    pe.exclude_kernel = 0;
    pe.exclude_hv = 0;
    pe.inherit = 1;  /* Inherit to child processes */

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        set_error("Failed to open perf event (type=%u, config=%lu): %s",
                  type, config, strerror(errno));
    }

    return fd;
}

/* Initialize performance monitoring context */
perfmon_context_t *perfmon_init(void) {
    perfmon_context_t *ctx;
    int i;

    ctx = (perfmon_context_t *)calloc(1, sizeof(perfmon_context_t));
    if (!ctx) {
        set_error("Failed to allocate context: %s", strerror(errno));
        return NULL;
    }

    /* Initialize all counters as disabled */
    for (i = 0; i < PERFMON_MAX_COUNTERS; i++) {
        ctx->counters[i].fd = -1;
        ctx->counters[i].enabled = false;
    }

    /* Setup hardware counters */
    ctx->counters[PERFMON_CYCLES].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    ctx->counters[PERFMON_CYCLES].enabled = (ctx->counters[PERFMON_CYCLES].fd != -1);

    ctx->counters[PERFMON_INSTRUCTIONS].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
    ctx->counters[PERFMON_INSTRUCTIONS].enabled = (ctx->counters[PERFMON_INSTRUCTIONS].fd != -1);

    ctx->counters[PERFMON_BRANCHES].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
    ctx->counters[PERFMON_BRANCHES].enabled = (ctx->counters[PERFMON_BRANCHES].fd != -1);

    ctx->counters[PERFMON_BRANCH_MISSES].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
    ctx->counters[PERFMON_BRANCH_MISSES].enabled = (ctx->counters[PERFMON_BRANCH_MISSES].fd != -1);

    ctx->counters[PERFMON_CACHE_REFERENCES].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES);
    ctx->counters[PERFMON_CACHE_REFERENCES].enabled = (ctx->counters[PERFMON_CACHE_REFERENCES].fd != -1);

    ctx->counters[PERFMON_CACHE_MISSES].fd = 
        setup_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
    ctx->counters[PERFMON_CACHE_MISSES].enabled = (ctx->counters[PERFMON_CACHE_MISSES].fd != -1);

    /* Setup software counters */
    ctx->counters[PERFMON_PAGE_FAULTS].fd = 
        setup_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS);
    ctx->counters[PERFMON_PAGE_FAULTS].enabled = (ctx->counters[PERFMON_PAGE_FAULTS].fd != -1);

    ctx->counters[PERFMON_MINOR_FAULTS].fd = 
        setup_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN);
    ctx->counters[PERFMON_MINOR_FAULTS].enabled = (ctx->counters[PERFMON_MINOR_FAULTS].fd != -1);

    ctx->counters[PERFMON_MAJOR_FAULTS].fd = 
        setup_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ);
    ctx->counters[PERFMON_MAJOR_FAULTS].enabled = (ctx->counters[PERFMON_MAJOR_FAULTS].fd != -1);

    ctx->counters[PERFMON_CONTEXT_SWITCHES].fd = 
        setup_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);
    ctx->counters[PERFMON_CONTEXT_SWITCHES].enabled = (ctx->counters[PERFMON_CONTEXT_SWITCHES].fd != -1);

    ctx->counters[PERFMON_CPU_MIGRATIONS].fd = 
        setup_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS);
    ctx->counters[PERFMON_CPU_MIGRATIONS].enabled = (ctx->counters[PERFMON_CPU_MIGRATIONS].fd != -1);

    /* Setup cache counters (may not be supported on all systems) */
    uint64_t dtlb_config = (PERF_COUNT_HW_CACHE_DTLB) | 
                           (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    ctx->counters[PERFMON_DTLB_LOAD_MISSES].fd = 
        setup_counter(PERF_TYPE_HW_CACHE, dtlb_config);
    ctx->counters[PERFMON_DTLB_LOAD_MISSES].enabled = (ctx->counters[PERFMON_DTLB_LOAD_MISSES].fd != -1);

    uint64_t itlb_config = (PERF_COUNT_HW_CACHE_ITLB) | 
                           (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                           (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    ctx->counters[PERFMON_ITLB_MISSES].fd = 
        setup_counter(PERF_TYPE_HW_CACHE, itlb_config);
    ctx->counters[PERFMON_ITLB_MISSES].enabled = (ctx->counters[PERFMON_ITLB_MISSES].fd != -1);

    ctx->is_running = false;

    return ctx;
}

/* Start performance monitoring */
bool perfmon_start(perfmon_context_t *ctx) {
    int i;

    if (!ctx) {
        set_error("Invalid context");
        return false;
    }

    if (ctx->is_running) {
        set_error("Monitoring already running");
        return false;
    }

    /* Reset and enable all counters */
    for (i = 0; i < PERFMON_MAX_COUNTERS; i++) {
        if (ctx->counters[i].enabled && ctx->counters[i].fd != -1) {
            ioctl(ctx->counters[i].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(ctx->counters[i].fd, PERF_EVENT_IOC_ENABLE, 0);
        }
    }

    /* Record start time */
    clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
    ctx->is_running = true;

    return true;
}

/* Read a single counter value */
static uint64_t read_counter(int fd) {
    uint64_t count = 0;
    if (fd != -1) {
        if (read(fd, &count, sizeof(count)) != sizeof(count)) {
            return 0;
        }
    }
    return count;
}

/* Calculate time difference in seconds */
static double timespec_diff_sec(struct timespec *start, struct timespec *end) {
    double sec = (double)(end->tv_sec - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec) / 1e9;
    return sec + nsec;
}

/* Stop performance monitoring and collect results */
bool perfmon_stop(perfmon_context_t *ctx, perfmon_stats_t *stats) {
    int i;

    if (!ctx) {
        set_error("Invalid context");
        return false;
    }

    if (!ctx->is_running) {
        set_error("Monitoring not running");
        return false;
    }

    /* Record end time */
    clock_gettime(CLOCK_MONOTONIC, &ctx->end_time);

    /* Disable all counters */
    for (i = 0; i < PERFMON_MAX_COUNTERS; i++) {
        if (ctx->counters[i].enabled && ctx->counters[i].fd != -1) {
            ioctl(ctx->counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    if (stats) {
        /* Read all counter values */
        memset(stats, 0, sizeof(perfmon_stats_t));
        
        stats->cycles = read_counter(ctx->counters[PERFMON_CYCLES].fd);
        stats->instructions = read_counter(ctx->counters[PERFMON_INSTRUCTIONS].fd);
        stats->branches = read_counter(ctx->counters[PERFMON_BRANCHES].fd);
        stats->branch_misses = read_counter(ctx->counters[PERFMON_BRANCH_MISSES].fd);
        stats->cache_references = read_counter(ctx->counters[PERFMON_CACHE_REFERENCES].fd);
        stats->cache_misses = read_counter(ctx->counters[PERFMON_CACHE_MISSES].fd);
        stats->dtlb_load_misses = read_counter(ctx->counters[PERFMON_DTLB_LOAD_MISSES].fd);
        stats->itlb_misses = read_counter(ctx->counters[PERFMON_ITLB_MISSES].fd);
        stats->page_faults = read_counter(ctx->counters[PERFMON_PAGE_FAULTS].fd);
        stats->minor_faults = read_counter(ctx->counters[PERFMON_MINOR_FAULTS].fd);
        stats->major_faults = read_counter(ctx->counters[PERFMON_MAJOR_FAULTS].fd);
        stats->context_switches = read_counter(ctx->counters[PERFMON_CONTEXT_SWITCHES].fd);
        stats->cpu_migrations = read_counter(ctx->counters[PERFMON_CPU_MIGRATIONS].fd);

        /* Calculate elapsed time */
        stats->elapsed_time_sec = timespec_diff_sec(&ctx->start_time, &ctx->end_time);

        /* Calculate derived metrics */
        if (stats->cycles > 0) {
            stats->insn_per_cycle = (double)stats->instructions / (double)stats->cycles;
        } else {
            stats->insn_per_cycle = 0.0;
        }

        if (stats->branches > 0) {
            stats->branch_miss_rate = (double)stats->branch_misses / (double)stats->branches * 100.0;
        } else {
            stats->branch_miss_rate = 0.0;
        }

        if (stats->cache_references > 0) {
            stats->cache_miss_rate = (double)stats->cache_misses / (double)stats->cache_references * 100.0;
        } else {
            stats->cache_miss_rate = 0.0;
        }
    }

    ctx->is_running = false;
    return true;
}

/* Reset all counters */
bool perfmon_reset(perfmon_context_t *ctx) {
    int i;

    if (!ctx) {
        set_error("Invalid context");
        return false;
    }

    for (i = 0; i < PERFMON_MAX_COUNTERS; i++) {
        if (ctx->counters[i].enabled && ctx->counters[i].fd != -1) {
            ioctl(ctx->counters[i].fd, PERF_EVENT_IOC_RESET, 0);
        }
    }

    return true;
}

/* Cleanup and free resources */
void perfmon_cleanup(perfmon_context_t *ctx) {
    int i;

    if (!ctx) {
        return;
    }

    /* Close all file descriptors */
    for (i = 0; i < PERFMON_MAX_COUNTERS; i++) {
        if (ctx->counters[i].fd != -1) {
            close(ctx->counters[i].fd);
        }
    }

    free(ctx);
}

/* Print statistics to a file descriptor */
void perfmon_print_stats(const perfmon_stats_t *stats, int fd) {
    if (!stats) {
        return;
    }

    dprintf(fd, "\nPerformance Statistics:\n");
    dprintf(fd, "======================\n");
    dprintf(fd, "%20lu      cycles\n", stats->cycles);
    dprintf(fd, "%20lu      instructions              #    %.2f  insn per cycle\n",
            stats->instructions, stats->insn_per_cycle);
    dprintf(fd, "%20lu      branches\n", stats->branches);
    dprintf(fd, "%20lu      branch-misses             #    %.2f%% of all branches\n",
            stats->branch_misses, stats->branch_miss_rate);
    dprintf(fd, "%20lu      cache-references\n", stats->cache_references);
    dprintf(fd, "%20lu      cache-misses              #    %.3f%% of all cache refs\n",
            stats->cache_misses, stats->cache_miss_rate);
    dprintf(fd, "%20lu      dTLB-load-misses\n", stats->dtlb_load_misses);
    dprintf(fd, "%20lu      iTLB-misses\n", stats->itlb_misses);
    dprintf(fd, "%20lu      page-faults\n", stats->page_faults);
    dprintf(fd, "%20lu      minor-faults\n", stats->minor_faults);
    dprintf(fd, "%20lu      major-faults\n", stats->major_faults);
    dprintf(fd, "%20lu      cs\n", stats->context_switches);
    dprintf(fd, "%20lu      migrations\n", stats->cpu_migrations);
    dprintf(fd, "\n%20.9f seconds time elapsed\n", stats->elapsed_time_sec);
}

/* Get last error message */
const char *perfmon_get_error(void) {
    return error_msg;
}

/* Check if performance monitoring is supported */
bool perfmon_is_supported(void) {
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        return false;
    }

    close(fd);
    return true;
}

/* Enable specific counter */
bool perfmon_enable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type) {
    if (!ctx || type >= PERFMON_MAX_COUNTERS) {
        set_error("Invalid context or counter type");
        return false;
    }

    if (ctx->counters[type].fd != -1) {
        ctx->counters[type].enabled = true;
        return true;
    }

    return false;
}

/* Disable specific counter */
bool perfmon_disable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type) {
    if (!ctx || type >= PERFMON_MAX_COUNTERS) {
        set_error("Invalid context or counter type");
        return false;
    }

    ctx->counters[type].enabled = false;
    return true;
}

