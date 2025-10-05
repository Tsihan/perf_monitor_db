#!/bin/bash
#
# 将libperfmon集成到PostgreSQL源码的辅助脚本
#

set -e

# 默认PostgreSQL源码路径
PG_SRC_DIR="${1:-/mydata/postgresql-16.4}"

# 检查PostgreSQL源码目录是否存在
if [ ! -d "$PG_SRC_DIR" ]; then
    echo "错误: PostgreSQL源码目录不存在: $PG_SRC_DIR"
    echo "用法: $0 [postgresql源码目录路径]"
    exit 1
fi

echo "==================================="
echo "libperfmon PostgreSQL集成脚本"
echo "==================================="
echo ""
echo "PostgreSQL源码目录: $PG_SRC_DIR"
echo ""

# 获取当前脚本所在目录(libperfmon目录)
LIBPERFMON_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "步骤 1/4: 编译libperfmon..."
cd "$LIBPERFMON_DIR"
if [ ! -f libperfmon.a ]; then
    make clean
    make
fi
echo "✓ libperfmon编译完成"
echo ""

echo "步骤 2/4: 复制库文件到PostgreSQL源码..."
# 复制静态库
cp -v libperfmon.a "$PG_SRC_DIR/src/backend/"
echo "✓ 复制 libperfmon.a"

# 复制头文件
mkdir -p "$PG_SRC_DIR/src/include/utils"
cp -v perfmon.h "$PG_SRC_DIR/src/include/utils/"
echo "✓ 复制 perfmon.h"
echo ""

echo "步骤 3/4: 备份和修改PostgreSQL Makefile..."
BACKEND_MAKEFILE="$PG_SRC_DIR/src/backend/Makefile"

# 备份原始Makefile
if [ ! -f "${BACKEND_MAKEFILE}.perfmon.bak" ]; then
    cp "$BACKEND_MAKEFILE" "${BACKEND_MAKEFILE}.perfmon.bak"
    echo "✓ 备份原始Makefile到 ${BACKEND_MAKEFILE}.perfmon.bak"
else
    echo "✓ Makefile备份已存在,跳过备份"
fi

# 检查是否已经添加了libperfmon
if grep -q "libperfmon.a" "$BACKEND_MAKEFILE"; then
    echo "✓ Makefile已包含libperfmon配置,跳过修改"
else
    echo "" >> "$BACKEND_MAKEFILE"
    echo "# libperfmon support" >> "$BACKEND_MAKEFILE"
    echo "OBJS += libperfmon.a" >> "$BACKEND_MAKEFILE"
    echo "✓ 修改Makefile添加libperfmon支持"
fi
echo ""

echo "步骤 4/4: 创建示例集成代码..."

# 创建perfmon_wrapper示例文件
WRAPPER_EXAMPLE="$PG_SRC_DIR/perfmon_wrapper_example.c"
cat > "$WRAPPER_EXAMPLE" << 'EOF'
/*
 * perfmon_wrapper_example.c
 *   示例: 如何在PostgreSQL中使用libperfmon
 *
 * 这个文件展示了如何创建封装函数和宏来在PostgreSQL中使用libperfmon。
 * 你可以根据实际需求修改这些代码并集成到相应的PostgreSQL源文件中。
 */

#include "postgres.h"
#include "utils/perfmon.h"
#include "utils/elog.h"

/* 全局变量 */
static perfmon_context_t *pg_perfmon_ctx = NULL;
static bool perfmon_enabled = false;

/*
 * 初始化性能监控(在postmaster启动时调用)
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
 * 清理性能监控(在postmaster关闭时调用)
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
 * 便捷宏: 在任意函数中使用
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
 * 使用示例: 在ExecutorRun中添加监控
 *
 * 修改 src/backend/executor/execMain.c:
 *
 * void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
 * {
 *     PERFMON_START(executor);
 *     
 *     // ... 原有代码 ...
 *     
 *     PERFMON_END(executor, "ExecutorRun");
 * }
 */

EOF

echo "✓ 创建示例代码: $WRAPPER_EXAMPLE"
echo ""

echo "==================================="
echo "集成完成!"
echo "==================================="
echo ""
echo "下一步操作:"
echo ""
echo "1. 查看集成示例代码:"
echo "   cat $WRAPPER_EXAMPLE"
echo ""
echo "2. 在PostgreSQL源码中添加监控代码(参考上面的示例)"
echo "   推荐修改的文件:"
echo "   - src/backend/executor/execMain.c"
echo "   - src/backend/executor/nodeSeqscan.c"
echo "   - src/backend/executor/nodeHashjoin.c"
echo ""
echo "3. 重新编译PostgreSQL:"
echo "   cd $PG_SRC_DIR"
echo "   make clean"
echo "   make -j\$(nproc)"
echo "   sudo make install"
echo ""
echo "4. 配置系统权限:"
echo "   echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid"
echo ""
echo "5. 重启PostgreSQL并查看日志"
echo ""
echo "更多详细信息请参考:"
echo "  - README.md"
echo "  - POSTGRESQL_INTEGRATION.md"
echo ""

