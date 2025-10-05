# libperfmon - Linux Performance Monitoring Library

A lightweight C library for collecting Linux hardware performance counter data. Can be easily integrated into database systems like PostgreSQL, MySQL, etc., to provide function-level performance monitoring.

## 📦 Project Overview

libperfmon uses the Linux `perf_event_open` API to collect hardware performance counters, providing the same metrics as `perf stat`:

- 🔄 CPU cycles, instructions, IPC (Instructions Per Cycle)
- 🎯 Branch prediction statistics (hit rate, miss count)
- 💾 Cache hit rate (L1/L2/L3 cache statistics)
- 📄 TLB misses, page faults, context switches

**Core Features:**
- ✅ Zero dependencies, pure C99 implementation
- ✅ Supports both static and dynamic libraries
- ✅ Thread-safe error handling
- ✅ Low performance overhead (<1%)
- ✅ Reusable across database systems
- ✅ Easy to integrate into existing code

## 📊 Supported Performance Metrics

Fully compatible with perf output:

| Metric | Description | Example Value |
|--------|-------------|---------------|
| cycles | CPU cycles | 18760514800 |
| instructions | Instructions executed | 23951742340 |
| insn_per_cycle | IPC (Instructions Per Cycle) | 1.28 |
| branches | Branch instructions | 4451665139 |
| branch_misses | Branch prediction misses | 35367046 (0.79%) |
| cache_references | Cache accesses | 2312399448 |
| cache_misses | Cache misses | 133902954 (5.791%) |
| dtlb_load_misses | Data TLB misses | 7685551 |
| itlb_misses | Instruction TLB misses | 8631 |
| page_faults | Total page faults | 33042 |
| minor_faults | Minor page faults | 567 |
| major_faults | Major page faults | 32475 |
| context_switches | Context switches | 15 |
| cpu_migrations | CPU migrations | 0 |
| elapsed_time_sec | Elapsed time (seconds) | 120.001456 |

## 🚀 Quick Start

### 1. Build the Library

```bash
cd /mydata/libperfmon
make
```

Generated files:
- `libperfmon.a` - Static library (11KB)
- `libperfmon.so` - Dynamic library (21KB)
- `example_simple` - Basic example program
- `example_postgresql` - PostgreSQL integration example

### 2. Basic Usage Example

```c
#include "perfmon.h"

int main() {
    perfmon_context_t *ctx;
    perfmon_stats_t stats;
    
    // Initialize
    ctx = perfmon_init();
    if (!ctx) {
        fprintf(stderr, "Init failed: %s\n", perfmon_get_error());
        return 1;
    }
    
    // Start monitoring
    perfmon_start(ctx);
    
    // Execute code to be monitored
    your_database_function();
    
    // Stop and get results
    perfmon_stop(ctx, &stats);
    
    // Print statistics
    printf("Cycles: %lu, IPC: %.2f, Cache Miss: %.2f%%\n",
           stats.cycles, stats.insn_per_cycle, stats.cache_miss_rate);
    
    // Or use formatted output (similar to perf stat)
    perfmon_print_stats(&stats, STDOUT_FILENO);
    
    // Cleanup
    perfmon_cleanup(ctx);
    return 0;
}
```

### 3. Run Example Programs

```bash
# Check system support
./example_simple --check-support

# Run basic example
./example_simple

# Run PostgreSQL integration example
./example_postgresql
```

## 🎯 PostgreSQL Integration

### Method 1: One-Click Installation (Recommended)

```bash
cd /mydata/libperfmon
./install_to_postgres.sh /mydata/postgresql-16.4
```

This script will automatically:
1. Build libperfmon
2. Copy library files to PostgreSQL source
3. Modify Makefile
4. Generate integration example code

### Method 2: Manual Integration

#### Step 1: Copy Files

```bash
cp libperfmon.a /mydata/postgresql-16.4/src/backend/
cp perfmon.h /mydata/postgresql-16.4/src/include/utils/
```

#### Step 2: Modify PostgreSQL Makefile

Edit `/mydata/postgresql-16.4/src/backend/Makefile`, add at the end of file:

```makefile
# Add libperfmon support
OBJS += libperfmon.a
```

#### Step 3: Use in Source Code

**Example 1: Monitor Query Executor**

Edit `src/backend/executor/execMain.c`:

```c
#include "utils/perfmon.h"

static perfmon_context_t *executor_perfmon_ctx = NULL;

void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, 
                 uint64 count, bool execute_once)
{
    perfmon_stats_t stats;
    
    // Initialize (only once)
    if (!executor_perfmon_ctx) {
        executor_perfmon_ctx = perfmon_init();
    }
    
    // Start monitoring
    if (executor_perfmon_ctx) {
        perfmon_start(executor_perfmon_ctx);
    }
    
    // Original execution logic
    standard_ExecutorRun(queryDesc, direction, count, execute_once);
    
    // Stop monitoring and output results
    if (executor_perfmon_ctx) {
        perfmon_stop(executor_perfmon_ctx, &stats);
        
        elog(LOG, "[PERFMON] ExecutorRun: cycles=%lu, insn=%lu, ipc=%.2f, "
                  "cache_miss=%.2f%%, time=%.6fs",
             stats.cycles, stats.instructions, stats.insn_per_cycle,
             stats.cache_miss_rate, stats.elapsed_time_sec);
    }
}
```

**Example 2: Using Convenience Macros**

```c
#include "utils/perfmon.h"

// Global context
static perfmon_context_t *global_perfmon = NULL;

// Initialize (call in PostmasterMain)
void init_perfmon(void) {
    if (perfmon_is_supported()) {
        global_perfmon = perfmon_init();
    }
}

// Define convenience macros
#define PERFMON_START(name) \
    perfmon_stats_t perfmon_##name; \
    bool perfmon_active_##name = false; \
    if (global_perfmon && perfmon_start(global_perfmon)) { \
        perfmon_active_##name = true; \
    }

#define PERFMON_END(name, label) \
    if (perfmon_active_##name) { \
        perfmon_stop(global_perfmon, &perfmon_##name); \
        elog(LOG, "[PERFMON] %s: cycles=%lu, ipc=%.2f, cache_miss=%.2f%%, time=%.6fs", \
             label, perfmon_##name.cycles, perfmon_##name.insn_per_cycle, \
             perfmon_##name.cache_miss_rate, perfmon_##name.elapsed_time_sec); \
    }

// Usage example
void ExecSeqScan(ScanState *node) {
    PERFMON_START(seqscan);
    
    // Original sequential scan logic
    TupleTableSlot *slot = ExecScanFetch(node, ...);
    
    PERFMON_END(seqscan, "SeqScan");
    return slot;
}
```

### Recommended PostgreSQL Functions to Monitor

| File Path | Function | Description |
|-----------|----------|-------------|
| `executor/execMain.c` | `ExecutorRun()` | Query executor main function |
| `executor/nodeSeqscan.c` | `ExecSeqScan()` | Sequential scan |
| `executor/nodeHashjoin.c` | `ExecHashJoin()` | Hash Join |
| `executor/nodeNestloop.c` | `ExecNestLoop()` | Nested loop join |
| `executor/nodeAgg.c` | `ExecAgg()` | Aggregation operation |
| `access/heap/heapam.c` | `heap_insert()` | Heap table insert |
| `access/heap/heapam.c` | `heap_update()` | Heap table update |
| `access/heap/heapam.c` | `heap_delete()` | Heap table delete |
| `storage/buffer/bufmgr.c` | `ReadBuffer()` | Buffer read |

### Build PostgreSQL

```bash
cd /mydata/postgresql-16.4
make clean
make -j$(nproc)
sudo make install
```

### Run Tests

```sql
-- Create test table
CREATE TABLE test_perf (
    id SERIAL PRIMARY KEY,
    name TEXT,
    value DOUBLE PRECISION
);

-- Insert test data
INSERT INTO test_perf (name, value)
SELECT 'row_' || i, random() * 1000
FROM generate_series(1, 1000000) i;

-- Execute query (trigger performance monitoring)
SELECT COUNT(*), AVG(value) FROM test_perf;

-- Execute JOIN (trigger hash join monitoring)
CREATE TABLE test_perf2 AS SELECT * FROM test_perf;
SELECT t1.name, t2.value 
FROM test_perf t1 
JOIN test_perf2 t2 ON t1.id = t2.id 
LIMIT 1000;
```

Check PostgreSQL logs, you will see output similar to:

```
[PERFMON] ExecutorRun: cycles=18760514800, insn=23951742340, ipc=1.28, 
          cache_miss=5.79%, time=1.234567s
[PERFMON] SeqScan: cycles=15234567890, ipc=1.35, cache_miss=4.21%, time=0.987654s
```

## 📚 API Reference

### Core Functions

```c
// Initialize monitoring context
perfmon_context_t *perfmon_init(void);

// Start monitoring
bool perfmon_start(perfmon_context_t *ctx);

// Stop monitoring and get results
bool perfmon_stop(perfmon_context_t *ctx, perfmon_stats_t *stats);

// Reset counters
bool perfmon_reset(perfmon_context_t *ctx);

// Cleanup resources
void perfmon_cleanup(perfmon_context_t *ctx);

// Print statistics (perf stat format)
void perfmon_print_stats(const perfmon_stats_t *stats, int fd);

// Get error message
const char *perfmon_get_error(void);

// Check if system is supported
bool perfmon_is_supported(void);

// Enable/disable specific counters
bool perfmon_enable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);
bool perfmon_disable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);
```

### Data Structures

```c
typedef struct {
    uint64_t cycles;              // CPU cycles
    uint64_t instructions;        // Instructions executed
    uint64_t branches;            // Branch instructions
    uint64_t branch_misses;       // Branch prediction misses
    uint64_t cache_references;    // Cache references
    uint64_t cache_misses;        // Cache misses
    uint64_t dtlb_load_misses;    // dTLB misses
    uint64_t itlb_misses;         // iTLB misses
    uint64_t page_faults;         // Page faults
    uint64_t minor_faults;        // Minor page faults
    uint64_t major_faults;        // Major page faults
    uint64_t context_switches;    // Context switches
    uint64_t cpu_migrations;      // CPU migrations
    double elapsed_time_sec;      // Elapsed time (seconds)
    
    // Derived metrics
    double insn_per_cycle;        // IPC (Instructions Per Cycle)
    double branch_miss_rate;      // Branch miss rate (%)
    double cache_miss_rate;       // Cache miss rate (%)
} perfmon_stats_t;
```

## ⚙️ System Configuration

### Permission Configuration (Required!)

Performance counter access requires specific permissions:

```bash
# Method 1: Temporary configuration (recommended for dev/test)
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid

# Method 2: Permanent configuration
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# Method 3: Use capabilities (recommended for production)
sudo setcap cap_perfmon=ep /usr/lib/postgresql/16/bin/postgres
# For older kernels use:
sudo setcap cap_sys_admin=ep /usr/lib/postgresql/16/bin/postgres
```

### Docker Container Configuration

```bash
# Command line method
docker run --cap-add=SYS_ADMIN --cap-add=PERFMON ...

# docker-compose.yml
services:
  postgres:
    image: postgres:16
    cap_add:
      - SYS_ADMIN
      - PERFMON
```

### Virtual Machine Configuration

```bash
# QEMU/KVM: Enable performance counter passthrough
qemu-system-x86_64 -cpu host ...

# Or in libvirt XML:
<cpu mode='host-passthrough'/>
```

## 🔧 Troubleshooting

### Issue 1: "Performance monitoring not supported"

**Cause:** Insufficient permissions or hardware not supported

**Solution:**
```bash
# Check current setting
cat /proc/sys/kernel/perf_event_paranoid

# If value > -1, change to -1
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid

# Check if running in VM
lscpu | grep Hypervisor
# If in VM, ensure CPU performance counter passthrough is enabled
```

### Issue 2: "Failed to open perf event: Operation not permitted"

**Cause:** Process doesn't have sufficient permissions

**Solution:**
```bash
# Add capabilities to PostgreSQL
sudo setcap cap_perfmon=ep /usr/lib/postgresql/16/bin/postgres

# Or run with sudo (not recommended for production)
sudo -u postgres postgres -D /var/lib/postgresql/data
```

### Issue 3: Cannot use in Docker

**Cause:** Container lacks necessary capabilities

**Solution:** Add to docker-compose.yml:
```yaml
services:
  postgres:
    cap_add:
      - SYS_ADMIN
      - PERFMON
    security_opt:
      - seccomp:unconfined
```

### Issue 4: Counters unavailable in VM

**Cause:** Virtualization platform doesn't expose performance counters

**Solution:**
- **KVM/QEMU:** Use `-cpu host` parameter
- **VMware:** Enable "Performance Counters" in VM settings
- **VirtualBox:** Execute `VBoxManage setextradata "VM_NAME" "VBoxInternal/CPUM/EnablePerfCounters" 1`

## 📈 Performance Metrics Interpretation

### IPC (Instructions Per Cycle)
- **>1.5** ✅ Excellent - High CPU pipeline utilization
- **1.0-1.5** ✔️ Good - Normal execution efficiency
- **<1.0** ⚠️ Poor - Possible memory bottleneck or CPU stalls

### Branch Miss Rate
- **<2%** ✅ Excellent - Accurate branch prediction
- **2-5%** ✔️ Acceptable
- **>5%** ⚠️ Poor - Consider optimizing branch logic

### Cache Miss Rate
- **<3%** ✅ Excellent - Good data locality
- **3-8%** ✔️ Acceptable
- **>8%** ⚠️ Poor - Large memory access span, consider optimizing data layout

### Other Metrics
- **Context Switches:** Low values indicate system stability, high values may indicate resource contention
- **Page Faults:** High major faults indicate insufficient memory, requiring disk loading
- **TLB Misses:** High values indicate irregular memory access patterns or oversized datasets

## 🎓 Using with Other Databases

### MySQL/MariaDB

```c
// storage/innobase/row/row0sel.cc
#include "perfmon.h"

dberr_t row_search_mvcc(...) {
    perfmon_context_t *ctx = perfmon_init();
    perfmon_start(ctx);
    
    // Original logic
    dberr_t err = row_search_mvcc_impl(...);
    
    perfmon_stats_t stats;
    perfmon_stop(ctx, &stats);
    sql_print_information("[PERFMON] InnoDB row_search: cycles=%lu, ipc=%.2f",
                          stats.cycles, stats.insn_per_cycle);
    perfmon_cleanup(ctx);
    return err;
}
```

### SQLite

```c
// src/vdbe.c
#include "perfmon.h"

int sqlite3_step(sqlite3_stmt *pStmt) {
    perfmon_context_t *ctx = perfmon_init();
    perfmon_start(ctx);
    
    int rc = sqlite3Step((Vdbe*)pStmt);
    
    perfmon_stats_t stats;
    perfmon_stop(ctx, &stats);
    // Use SQLite logging output
    perfmon_cleanup(ctx);
    return rc;
}
```

### Any C/C++ Project

```c
#include "perfmon.h"

void your_critical_function() {
    perfmon_context_t *ctx = perfmon_init();
    perfmon_start(ctx);
    
    // Your code
    
    perfmon_stats_t stats;
    perfmon_stop(ctx, &stats);
    perfmon_print_stats(&stats, STDOUT_FILENO);
    perfmon_cleanup(ctx);
}
```

## 💡 Usage Recommendations

### Development/Testing Environment
- ✅ Always enable for performance analysis
- ✅ Compare performance of different implementations
- ✅ Identify performance bottlenecks

### Production Environment
- ✅ Disable by default, enable temporarily as needed
- ✅ Only monitor critical paths (query executor, scans, JOINs, etc.)
- ⚠️ Avoid monitoring frequently-called small functions (<1ms functions)
- ⚠️ Performance overhead is small (<1%), but accumulates with high-frequency calls

### Performance Optimization Workflow
1. Use libperfmon to identify bottleneck functions (low IPC, high cache miss)
2. Analyze specific causes (CPU stalls, memory access, branch prediction)
3. Implement optimizations (algorithms, data structures, memory layout)
4. Measure again to verify optimization effects

## 📦 Building and Installation

### Build Library

```bash
cd /mydata/libperfmon
make                # Build library and examples
make clean          # Clean build artifacts
```

### System Installation (Optional)

```bash
sudo make install                    # Install to /usr/local
sudo make PREFIX=/opt/local install  # Custom installation path
sudo make uninstall                  # Uninstall
```

### Using in Your Project

```bash
# Static linking
gcc -o myapp myapp.c -L/mydata/libperfmon -lperfmon -static

# Dynamic linking
gcc -o myapp myapp.c -L/mydata/libperfmon -lperfmon
export LD_LIBRARY_PATH=/mydata/libperfmon:$LD_LIBRARY_PATH
./myapp
```

## 📋 Project Files

```
libperfmon/
├── perfmon.h                 - API header file (3KB)
├── perfmon.c                 - Implementation code (13KB)
├── Makefile                  - Build script
├── libperfmon.a              - Static library (11KB)
├── libperfmon.so             - Dynamic library (21KB)
├── example_simple.c          - Basic example
├── example_postgresql.c      - PostgreSQL integration example
├── install_to_postgres.sh    - One-click installation script
├── LICENSE                   - MIT License
└── README.md                 - This documentation
```

## ⚠️ Important Notes

1. **Thread Safety:** Each thread should use its own `perfmon_context_t`
2. **Nested Monitoring:** Avoid starting monitoring again in already-monitored code sections
3. **Performance Overhead:** Although <1%, still use caution in high-frequency functions
4. **Permission Requirements:** Ensure process has access to perf events
5. **Containers/VMs:** Special configuration required

## 📄 License

MIT License - Free to use in commercial and open-source projects

## 🙏 Contributing

Issues and Pull Requests are welcome!

---

**Project Location:** `/mydata/libperfmon`  
**Created:** 2025-10-05  
**Goal:** Provide hardware-level performance monitoring capabilities for database systems
