/*
 * libperfmon - Performance Monitoring Library
 * 
 * A lightweight C library for collecting hardware performance counters
 * using Linux perf_event_open API.
 * 
 * This library can be integrated into database systems (PostgreSQL, MySQL, etc.)
 * to monitor function-level performance metrics.
 */

#ifndef PERFMON_H
#define PERFMON_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Performance counter types */
typedef enum {
    PERFMON_CYCLES = 0,
    PERFMON_INSTRUCTIONS,
    PERFMON_BRANCHES,
    PERFMON_BRANCH_MISSES,
    PERFMON_CACHE_REFERENCES,
    PERFMON_CACHE_MISSES,
    PERFMON_DTLB_LOAD_MISSES,
    PERFMON_ITLB_MISSES,
    PERFMON_PAGE_FAULTS,
    PERFMON_MINOR_FAULTS,
    PERFMON_MAJOR_FAULTS,
    PERFMON_CONTEXT_SWITCHES,
    PERFMON_CPU_MIGRATIONS,
    PERFMON_MAX_COUNTERS
} perfmon_counter_type_t;

/* Performance statistics structure */
typedef struct {
    uint64_t cycles;
    uint64_t instructions;
    uint64_t branches;
    uint64_t branch_misses;
    uint64_t cache_references;
    uint64_t cache_misses;
    uint64_t dtlb_load_misses;
    uint64_t itlb_misses;
    uint64_t page_faults;
    uint64_t minor_faults;
    uint64_t major_faults;
    uint64_t context_switches;
    uint64_t cpu_migrations;
    double elapsed_time_sec;  /* elapsed time in seconds */
    
    /* Derived metrics */
    double insn_per_cycle;
    double branch_miss_rate;
    double cache_miss_rate;
} perfmon_stats_t;

/* Performance monitor context (opaque handle) */
typedef struct perfmon_context perfmon_context_t;

/*
 * Initialize performance monitoring context
 * Returns: context handle on success, NULL on failure
 */
perfmon_context_t *perfmon_init(void);

/*
 * Start performance monitoring
 * Returns: true on success, false on failure
 */
bool perfmon_start(perfmon_context_t *ctx);

/*
 * Stop performance monitoring and collect results
 * Returns: true on success, false on failure
 */
bool perfmon_stop(perfmon_context_t *ctx, perfmon_stats_t *stats);

/*
 * Reset all counters (without stopping)
 * Returns: true on success, false on failure
 */
bool perfmon_reset(perfmon_context_t *ctx);

/*
 * Cleanup and free resources
 */
void perfmon_cleanup(perfmon_context_t *ctx);

/*
 * Print statistics to a file descriptor (stdout, stderr, or custom file)
 * format: human-readable format
 */
void perfmon_print_stats(const perfmon_stats_t *stats, int fd);

/*
 * Get last error message
 */
const char *perfmon_get_error(void);

/*
 * Check if performance monitoring is supported on this system
 * Returns: true if supported, false otherwise
 */
bool perfmon_is_supported(void);

/*
 * Enable/disable specific counters (for fine-grained control)
 * By default, all counters are enabled
 */
bool perfmon_enable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);
bool perfmon_disable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* PERFMON_H */

