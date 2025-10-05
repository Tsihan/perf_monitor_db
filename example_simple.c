/*
 * Simple example demonstrating basic usage of libperfmon
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "perfmon.h"

/* Example workload function - matrix multiplication */
void matrix_multiply(int size) {
    int i, j, k;
    double **a, **b, **c;
    
    /* Allocate matrices */
    a = malloc(size * sizeof(double *));
    b = malloc(size * sizeof(double *));
    c = malloc(size * sizeof(double *));
    
    for (i = 0; i < size; i++) {
        a[i] = malloc(size * sizeof(double));
        b[i] = malloc(size * sizeof(double));
        c[i] = malloc(size * sizeof(double));
        
        for (j = 0; j < size; j++) {
            a[i][j] = (double)(i + j);
            b[i][j] = (double)(i - j);
            c[i][j] = 0.0;
        }
    }
    
    /* Perform multiplication */
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            for (k = 0; k < size; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    
    /* Use result to prevent optimization */
    volatile double sum = 0.0;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            sum += c[i][j];
        }
    }
    
    /* Free matrices */
    for (i = 0; i < size; i++) {
        free(a[i]);
        free(b[i]);
        free(c[i]);
    }
    free(a);
    free(b);
    free(c);
}

/* Example: Simple workload monitoring */
void example_basic_monitoring(void) {
    perfmon_context_t *ctx;
    perfmon_stats_t stats;
    
    printf("=== Basic Monitoring Example ===\n\n");
    
    /* Initialize performance monitoring */
    ctx = perfmon_init();
    if (!ctx) {
        fprintf(stderr, "Failed to initialize perfmon: %s\n", perfmon_get_error());
        return;
    }
    
    /* Start monitoring */
    if (!perfmon_start(ctx)) {
        fprintf(stderr, "Failed to start monitoring: %s\n", perfmon_get_error());
        perfmon_cleanup(ctx);
        return;
    }
    
    /* Execute workload */
    printf("Running matrix multiplication (500x500)...\n");
    matrix_multiply(500);
    
    /* Stop monitoring and get results */
    if (!perfmon_stop(ctx, &stats)) {
        fprintf(stderr, "Failed to stop monitoring: %s\n", perfmon_get_error());
        perfmon_cleanup(ctx);
        return;
    }
    
    /* Print results */
    perfmon_print_stats(&stats, STDOUT_FILENO);
    
    /* Cleanup */
    perfmon_cleanup(ctx);
}

/* Example: Multiple measurements */
void example_multiple_measurements(void) {
    perfmon_context_t *ctx;
    perfmon_stats_t stats1, stats2, stats3;
    
    printf("\n=== Multiple Measurements Example ===\n\n");
    
    ctx = perfmon_init();
    if (!ctx) {
        fprintf(stderr, "Failed to initialize perfmon: %s\n", perfmon_get_error());
        return;
    }
    
    /* Measurement 1: Small workload */
    printf("Measurement 1: Matrix 200x200\n");
    perfmon_start(ctx);
    matrix_multiply(200);
    perfmon_stop(ctx, &stats1);
    
    /* Measurement 2: Medium workload */
    printf("Measurement 2: Matrix 400x400\n");
    perfmon_start(ctx);
    matrix_multiply(400);
    perfmon_stop(ctx, &stats2);
    
    /* Measurement 3: Large workload */
    printf("Measurement 3: Matrix 600x600\n");
    perfmon_start(ctx);
    matrix_multiply(600);
    perfmon_stop(ctx, &stats3);
    
    /* Compare results */
    printf("\n=== Comparison ===\n");
    printf("Workload         Cycles          Instructions    Time(s)     IPC\n");
    printf("--------         ------          ------------    -------     ---\n");
    printf("200x200    %15lu  %15lu  %8.3f  %6.2f\n", 
           stats1.cycles, stats1.instructions, stats1.elapsed_time_sec, stats1.insn_per_cycle);
    printf("400x400    %15lu  %15lu  %8.3f  %6.2f\n", 
           stats2.cycles, stats2.instructions, stats2.elapsed_time_sec, stats2.insn_per_cycle);
    printf("600x600    %15lu  %15lu  %8.3f  %6.2f\n", 
           stats3.cycles, stats3.instructions, stats3.elapsed_time_sec, stats3.insn_per_cycle);
    
    perfmon_cleanup(ctx);
}

/* Check if performance monitoring is supported */
void check_support(void) {
    if (perfmon_is_supported()) {
        printf("Performance monitoring is SUPPORTED on this system.\n");
        exit(0);
    } else {
        printf("Performance monitoring is NOT SUPPORTED on this system.\n");
        printf("Possible reasons:\n");
        printf("  - Running in a container without CAP_PERFMON/CAP_SYS_ADMIN\n");
        printf("  - /proc/sys/kernel/perf_event_paranoid is too restrictive\n");
        printf("  - Hardware performance counters not available\n");
        printf("\nTry: echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    /* Check for support test flag */
    if (argc > 1 && strcmp(argv[1], "--check-support") == 0) {
        check_support();
    }
    
    printf("libperfmon - Performance Monitoring Library Example\n");
    printf("====================================================\n\n");
    
    /* Check if supported */
    if (!perfmon_is_supported()) {
        fprintf(stderr, "Error: Performance monitoring is not supported on this system.\n");
        fprintf(stderr, "Run with --check-support for more information.\n");
        return 1;
    }
    
    /* Run examples */
    example_basic_monitoring();
    example_multiple_measurements();
    
    printf("\nExamples completed successfully!\n");
    return 0;
}

