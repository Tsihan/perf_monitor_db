#!/bin/bash
#
# Helper script to integrate libperfmon into PostgreSQL source code
#

set -e

# Default PostgreSQL source path
PG_SRC_DIR="${1:-/mydata/postgresql-16.4}"

# Check if PostgreSQL source directory exists
if [ ! -d "$PG_SRC_DIR" ]; then
    echo "Error: PostgreSQL source directory does not exist: $PG_SRC_DIR"
    echo "Usage: $0 [postgresql_source_directory_path]"
    exit 1
fi

echo "==================================="
echo "libperfmon PostgreSQL Integration Script"
echo "==================================="
echo ""
echo "PostgreSQL source directory: $PG_SRC_DIR"
echo ""

# Get current script directory (libperfmon directory)
LIBPERFMON_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Step 1/4: Building libperfmon..."
cd "$LIBPERFMON_DIR"
if [ ! -f libperfmon.a ]; then
    make clean
    make
fi
echo "✓ libperfmon build complete"
echo ""

echo "Step 2/4: Copying library files to PostgreSQL source..."
# Copy static library
cp -v libperfmon.a "$PG_SRC_DIR/src/backend/"
echo "✓ Copied libperfmon.a"

# Copy header file
mkdir -p "$PG_SRC_DIR/src/include/utils"
cp -v perfmon.h "$PG_SRC_DIR/src/include/utils/"
echo "✓ Copied perfmon.h"
echo ""

echo "Step 3/4: Backing up and modifying PostgreSQL Makefile..."
BACKEND_MAKEFILE="$PG_SRC_DIR/src/backend/Makefile"

# Backup original Makefile
if [ ! -f "${BACKEND_MAKEFILE}.perfmon.bak" ]; then
    cp "$BACKEND_MAKEFILE" "${BACKEND_MAKEFILE}.perfmon.bak"
    echo "✓ Backed up original Makefile to ${BACKEND_MAKEFILE}.perfmon.bak"
else
    echo "✓ Makefile backup already exists, skipping backup"
fi

# Check if libperfmon has already been added
if grep -q "libperfmon.a" "$BACKEND_MAKEFILE"; then
    echo "✓ Makefile already contains libperfmon configuration, skipping modification"
else
    echo "" >> "$BACKEND_MAKEFILE"
    echo "# libperfmon support" >> "$BACKEND_MAKEFILE"
    echo "OBJS += libperfmon.a" >> "$BACKEND_MAKEFILE"
    echo "✓ Modified Makefile to add libperfmon support"
fi
echo ""

echo "Step 4/4: Creating example integration code..."

# Create perfmon_wrapper example file
WRAPPER_EXAMPLE="$PG_SRC_DIR/perfmon_wrapper_example.c"
cat > "$WRAPPER_EXAMPLE" << 'EOF'
/*
 * perfmon_wrapper_example.c
 *   Example: How to use libperfmon in PostgreSQL
 *
 * This file demonstrates how to create wrapper functions and macros to use libperfmon in PostgreSQL.
 * You can modify this code according to your needs and integrate it into the appropriate PostgreSQL source files.
 */

#include "postgres.h"
#include "utils/perfmon.h"
#include "utils/elog.h"

/* Global variables */
static perfmon_context_t *pg_perfmon_ctx = NULL;
static bool perfmon_enabled = false;

/*
 * Initialize performance monitoring (call at postmaster startup)
 */
void InitPerfMon(void)
{
    if (!perfmon_is_supported())
    {
        elog(WARNING, "Performance monitoring not supported on this system");
        return;
    }
    
    pg_perfmon_ctx = perfmon_init();
    if (!pg_perfmon_ctx)
    {
        elog(WARNING, "Failed to initialize perfmon: %s", perfmon_get_error());
        return;
    }
    
    perfmon_enabled = true;
    elog(LOG, "Performance monitoring initialized successfully");
}

/*
 * Cleanup performance monitoring (call at postmaster shutdown)
 */
void ShutdownPerfMon(void)
{
    if (pg_perfmon_ctx)
    {
        perfmon_cleanup(pg_perfmon_ctx);
        pg_perfmon_ctx = NULL;
        perfmon_enabled = false;
    }
}

/*
 * Convenience macros: use in any function
 */
#define PERFMON_START(name) \
    perfmon_stats_t perfmon_stats_##name; \
    bool perfmon_started_##name = false; \
    if (perfmon_enabled && pg_perfmon_ctx) { \
        perfmon_started_##name = perfmon_start(pg_perfmon_ctx); \
    }

#define PERFMON_END(name, label) \
    if (perfmon_started_##name && perfmon_stop(pg_perfmon_ctx, &perfmon_stats_##name)) { \
        elog(LOG, "[PERFMON] %s: cycles=%lu, insn=%lu, ipc=%.2f, " \
                  "cache_miss=%.2f%%, time=%.6fs", \
             label, \
             perfmon_stats_##name.cycles, \
             perfmon_stats_##name.instructions, \
             perfmon_stats_##name.insn_per_cycle, \
             perfmon_stats_##name.cache_miss_rate, \
             perfmon_stats_##name.elapsed_time_sec); \
    }

/*
 * Usage example: Add monitoring to ExecutorRun
 *
 * Modify src/backend/executor/execMain.c:
 *
 * void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
 * {
 *     PERFMON_START(executor);
 *     
 *     // ... original code ...
 *     
 *     PERFMON_END(executor, "ExecutorRun");
 * }
 */

EOF

echo "✓ Created example code: $WRAPPER_EXAMPLE"
echo ""

echo "==================================="
echo "Integration Complete!"
echo "==================================="
echo ""
echo "Next Steps:"
echo ""
echo "1. View the integration example code:"
echo "   cat $WRAPPER_EXAMPLE"
echo ""
echo "2. Add monitoring code to PostgreSQL source (refer to the example above)"
echo "   Recommended files to modify:"
echo "   - src/backend/executor/execMain.c"
echo "   - src/backend/executor/nodeSeqscan.c"
echo "   - src/backend/executor/nodeHashjoin.c"
echo ""
echo "3. Rebuild PostgreSQL:"
echo "   cd $PG_SRC_DIR"
echo "   make clean"
echo "   make -j\$(nproc)"
echo "   sudo make install"
echo ""
echo "4. Configure system permissions:"
echo "   echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid"
echo ""
echo "5. Restart PostgreSQL and check the logs"
echo ""
echo "For more details, please refer to:"
echo "  - README.md"
echo "  - POSTGRESQL_INTEGRATION.md"
echo ""

