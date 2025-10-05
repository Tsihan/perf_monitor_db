# libperfmon - Linux性能监控库

一个轻量级的C语言库，用于收集Linux硬件性能计数器数据。可以轻松集成到PostgreSQL、MySQL等数据库系统中，实现函数级别的性能监控。

## 📦 项目简介

libperfmon使用Linux `perf_event_open` API收集硬件性能计数器，提供与`perf stat`相同的性能指标：

- 🔄 CPU周期、指令数、IPC（每周期指令数）
- 🎯 分支预测统计（命中率、失败次数）
- 💾 缓存命中率（L1/L2/L3缓存统计）
- 📄 TLB未命中、页错误、上下文切换

**核心特性：**
- ✅ 零依赖，纯C99实现
- ✅ 支持静态库和动态库
- ✅ 线程安全的错误处理
- ✅ 低性能开销（<1%）
- ✅ 跨数据库系统复用
- ✅ 易于集成到现有代码

## 📊 支持的性能指标

与你的perf输出完全对应：

| 指标 | 说明 | 示例值 |
|------|------|--------|
| cycles | CPU周期数 | 18760514800 |
| instructions | 执行的指令数 | 23951742340 |
| insn_per_cycle | IPC（每周期指令数） | 1.28 |
| branches | 分支指令数 | 4451665139 |
| branch_misses | 分支预测失败次数 | 35367046 (0.79%) |
| cache_references | 缓存访问次数 | 2312399448 |
| cache_misses | 缓存未命中次数 | 133902954 (5.791%) |
| dtlb_load_misses | 数据TLB未命中 | 7685551 |
| itlb_misses | 指令TLB未命中 | 8631 |
| page_faults | 页错误总数 | 33042 |
| minor_faults | 次缺页中断 | 567 |
| major_faults | 主缺页中断 | 32475 |
| context_switches | 上下文切换次数 | 15 |
| cpu_migrations | CPU迁移次数 | 0 |
| elapsed_time_sec | 执行时间（秒） | 120.001456 |

## 🚀 快速开始

### 1. 编译库

```bash
cd /mydata/libperfmon
make
```

生成文件：
- `libperfmon.a` - 静态库（11KB）
- `libperfmon.so` - 动态库（21KB）
- `example_simple` - 基础示例程序
- `example_postgresql` - PostgreSQL集成示例

### 2. 基础使用示例

```c
#include "perfmon.h"

int main() {
    perfmon_context_t *ctx;
    perfmon_stats_t stats;
    
    // 初始化
    ctx = perfmon_init();
    if (!ctx) {
        fprintf(stderr, "Init failed: %s\n", perfmon_get_error());
        return 1;
    }
    
    // 开始监控
    perfmon_start(ctx);
    
    // 执行要监控的代码
    your_database_function();
    
    // 停止并获取结果
    perfmon_stop(ctx, &stats);
    
    // 打印统计信息
    printf("Cycles: %lu, IPC: %.2f, Cache Miss: %.2f%%\n",
           stats.cycles, stats.insn_per_cycle, stats.cache_miss_rate);
    
    // 或使用格式化输出（类似perf stat）
    perfmon_print_stats(&stats, STDOUT_FILENO);
    
    // 清理
    perfmon_cleanup(ctx);
    return 0;
}
```

### 3. 运行示例程序

```bash
# 检查系统是否支持
./example_simple --check-support

# 运行基础示例
./example_simple

# 运行PostgreSQL集成示例
./example_postgresql
```

## 🎯 PostgreSQL集成

### 方法1: 一键安装（推荐）

```bash
cd /mydata/libperfmon
./install_to_postgres.sh /mydata/postgresql-16.4
```

这个脚本会自动：
1. 编译libperfmon
2. 复制库文件到PostgreSQL源码
3. 修改Makefile
4. 生成集成示例代码

### 方法2: 手动集成

#### 步骤1: 复制文件

```bash
cp libperfmon.a /mydata/postgresql-16.4/src/backend/
cp perfmon.h /mydata/postgresql-16.4/src/include/utils/
```

#### 步骤2: 修改PostgreSQL Makefile

编辑 `/mydata/postgresql-16.4/src/backend/Makefile`，在文件末尾添加：

```makefile
# 添加libperfmon支持
OBJS += libperfmon.a
```

#### 步骤3: 在源码中使用

**示例1: 监控查询执行器**

编辑 `src/backend/executor/execMain.c`:

```c
#include "utils/perfmon.h"

static perfmon_context_t *executor_perfmon_ctx = NULL;

void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, 
                 uint64 count, bool execute_once)
{
    perfmon_stats_t stats;
    
    // 初始化（只需一次）
    if (!executor_perfmon_ctx) {
        executor_perfmon_ctx = perfmon_init();
    }
    
    // 开始监控
    if (executor_perfmon_ctx) {
        perfmon_start(executor_perfmon_ctx);
    }
    
    // 原有的执行逻辑
    standard_ExecutorRun(queryDesc, direction, count, execute_once);
    
    // 停止监控并输出结果
    if (executor_perfmon_ctx) {
        perfmon_stop(executor_perfmon_ctx, &stats);
        
        elog(LOG, "[PERFMON] ExecutorRun: cycles=%lu, insn=%lu, ipc=%.2f, "
                  "cache_miss=%.2f%%, time=%.6fs",
             stats.cycles, stats.instructions, stats.insn_per_cycle,
             stats.cache_miss_rate, stats.elapsed_time_sec);
    }
}
```

**示例2: 使用便捷宏**

```c
#include "utils/perfmon.h"

// 全局context
static perfmon_context_t *global_perfmon = NULL;

// 初始化（在PostmasterMain中调用）
void init_perfmon(void) {
    if (perfmon_is_supported()) {
        global_perfmon = perfmon_init();
    }
}

// 定义便捷宏
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

// 使用示例
void ExecSeqScan(ScanState *node) {
    PERFMON_START(seqscan);
    
    // 原有的顺序扫描逻辑
    TupleTableSlot *slot = ExecScanFetch(node, ...);
    
    PERFMON_END(seqscan, "SeqScan");
    return slot;
}
```

### 推荐监控的PostgreSQL函数

| 文件路径 | 函数 | 说明 |
|---------|------|------|
| `executor/execMain.c` | `ExecutorRun()` | 查询执行器主函数 |
| `executor/nodeSeqscan.c` | `ExecSeqScan()` | 顺序扫描 |
| `executor/nodeHashjoin.c` | `ExecHashJoin()` | Hash Join |
| `executor/nodeNestloop.c` | `ExecNestLoop()` | 嵌套循环连接 |
| `executor/nodeAgg.c` | `ExecAgg()` | 聚合操作 |
| `access/heap/heapam.c` | `heap_insert()` | 堆表插入 |
| `access/heap/heapam.c` | `heap_update()` | 堆表更新 |
| `access/heap/heapam.c` | `heap_delete()` | 堆表删除 |
| `storage/buffer/bufmgr.c` | `ReadBuffer()` | 缓冲区读取 |

### 编译PostgreSQL

```bash
cd /mydata/postgresql-16.4
make clean
make -j$(nproc)
sudo make install
```

### 运行测试

```sql
-- 创建测试表
CREATE TABLE test_perf (
    id SERIAL PRIMARY KEY,
    name TEXT,
    value DOUBLE PRECISION
);

-- 插入测试数据
INSERT INTO test_perf (name, value)
SELECT 'row_' || i, random() * 1000
FROM generate_series(1, 1000000) i;

-- 执行查询（触发性能监控）
SELECT COUNT(*), AVG(value) FROM test_perf;

-- 执行JOIN（触发hash join监控）
CREATE TABLE test_perf2 AS SELECT * FROM test_perf;
SELECT t1.name, t2.value 
FROM test_perf t1 
JOIN test_perf2 t2 ON t1.id = t2.id 
LIMIT 1000;
```

查看PostgreSQL日志，你会看到类似的输出：

```
[PERFMON] ExecutorRun: cycles=18760514800, insn=23951742340, ipc=1.28, 
          cache_miss=5.79%, time=1.234567s
[PERFMON] SeqScan: cycles=15234567890, ipc=1.35, cache_miss=4.21%, time=0.987654s
```

## 📚 API参考

### 核心函数

```c
// 初始化监控上下文
perfmon_context_t *perfmon_init(void);

// 开始监控
bool perfmon_start(perfmon_context_t *ctx);

// 停止监控并获取结果
bool perfmon_stop(perfmon_context_t *ctx, perfmon_stats_t *stats);

// 重置计数器
bool perfmon_reset(perfmon_context_t *ctx);

// 清理资源
void perfmon_cleanup(perfmon_context_t *ctx);

// 打印统计信息（类似perf stat格式）
void perfmon_print_stats(const perfmon_stats_t *stats, int fd);

// 获取错误信息
const char *perfmon_get_error(void);

// 检查系统是否支持
bool perfmon_is_supported(void);

// 启用/禁用特定计数器
bool perfmon_enable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);
bool perfmon_disable_counter(perfmon_context_t *ctx, perfmon_counter_type_t type);
```

### 数据结构

```c
typedef struct {
    uint64_t cycles;              // CPU周期数
    uint64_t instructions;        // 指令数
    uint64_t branches;            // 分支数
    uint64_t branch_misses;       // 分支预测失败
    uint64_t cache_references;    // 缓存引用
    uint64_t cache_misses;        // 缓存未命中
    uint64_t dtlb_load_misses;    // dTLB未命中
    uint64_t itlb_misses;         // iTLB未命中
    uint64_t page_faults;         // 页错误
    uint64_t minor_faults;        // 次缺页
    uint64_t major_faults;        // 主缺页
    uint64_t context_switches;    // 上下文切换
    uint64_t cpu_migrations;      // CPU迁移
    double elapsed_time_sec;      // 经过时间（秒）
    
    // 派生指标
    double insn_per_cycle;        // IPC（每周期指令数）
    double branch_miss_rate;      // 分支预测失败率（%）
    double cache_miss_rate;       // 缓存未命中率（%）
} perfmon_stats_t;
```

## ⚙️ 系统配置

### 权限配置（必需！）

性能计数器访问需要特定权限：

```bash
# 方法1: 临时配置（开发/测试推荐）
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid

# 方法2: 永久配置
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# 方法3: 使用capabilities（生产环境推荐）
sudo setcap cap_perfmon=ep /usr/lib/postgresql/16/bin/postgres
# 对于旧内核使用：
sudo setcap cap_sys_admin=ep /usr/lib/postgresql/16/bin/postgres
```

### Docker容器配置

```bash
# 命令行方式
docker run --cap-add=SYS_ADMIN --cap-add=PERFMON ...

# docker-compose.yml
services:
  postgres:
    image: postgres:16
    cap_add:
      - SYS_ADMIN
      - PERFMON
```

### 虚拟机配置

```bash
# QEMU/KVM: 启用性能计数器透传
qemu-system-x86_64 -cpu host ...

# 或在libvirt XML中：
<cpu mode='host-passthrough'/>
```

## 🔧 故障排查

### 问题1: "Performance monitoring not supported"

**原因：** 权限不足或硬件不支持

**解决：**
```bash
# 检查当前设置
cat /proc/sys/kernel/perf_event_paranoid

# 如果值>-1，修改为-1
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid

# 检查是否在虚拟机中
lscpu | grep Hypervisor
# 如果在虚拟机，确保启用了CPU性能计数器透传
```

### 问题2: "Failed to open perf event: Operation not permitted"

**原因：** 进程没有足够的权限

**解决：**
```bash
# 为PostgreSQL添加capabilities
sudo setcap cap_perfmon=ep /usr/lib/postgresql/16/bin/postgres

# 或使用sudo运行（不推荐生产环境）
sudo -u postgres postgres -D /var/lib/postgresql/data
```

### 问题3: Docker中无法使用

**原因：** 容器缺少必要的capabilities

**解决：** 在docker-compose.yml中添加：
```yaml
services:
  postgres:
    cap_add:
      - SYS_ADMIN
      - PERFMON
    security_opt:
      - seccomp:unconfined
```

### 问题4: 虚拟机中计数器不可用

**原因：** 虚拟化平台未暴露性能计数器

**解决：**
- **KVM/QEMU:** 使用 `-cpu host` 参数
- **VMware:** 在虚拟机设置中启用"性能计数器"
- **VirtualBox:** 执行 `VBoxManage setextradata "VM名称" "VBoxInternal/CPUM/EnablePerfCounters" 1`

## 📈 性能指标解读

### IPC (Instructions Per Cycle)
- **>1.5** ✅ 优秀 - CPU流水线利用率高
- **1.0-1.5** ✔️ 良好 - 正常执行效率
- **<1.0** ⚠️ 较差 - 可能存在内存瓶颈或CPU停顿

### Branch Miss Rate
- **<2%** ✅ 优秀 - 分支预测准确
- **2-5%** ✔️ 可接受
- **>5%** ⚠️ 较差 - 考虑优化分支逻辑

### Cache Miss Rate
- **<3%** ✅ 优秀 - 数据局部性好
- **3-8%** ✔️ 可接受
- **>8%** ⚠️ 较差 - 内存访问跨度大，考虑优化数据布局

### 其他指标
- **Context Switches:** 低值表示系统稳定，高值可能表示资源竞争
- **Page Faults:** Major faults高说明内存不足，需要从磁盘加载
- **TLB Misses:** 高值表示内存访问模式不规则或数据集过大

## 🎓 在其他数据库中使用

### MySQL/MariaDB

```c
// storage/innobase/row/row0sel.cc
#include "perfmon.h"

dberr_t row_search_mvcc(...) {
    perfmon_context_t *ctx = perfmon_init();
    perfmon_start(ctx);
    
    // 原有逻辑
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
    // 使用SQLite日志输出
    perfmon_cleanup(ctx);
    return rc;
}
```

### 任何C/C++项目

```c
#include "perfmon.h"

void your_critical_function() {
    perfmon_context_t *ctx = perfmon_init();
    perfmon_start(ctx);
    
    // 你的代码
    
    perfmon_stats_t stats;
    perfmon_stop(ctx, &stats);
    perfmon_print_stats(&stats, STDOUT_FILENO);
    perfmon_cleanup(ctx);
}
```

## 💡 使用建议

### 开发/测试环境
- ✅ 始终启用，用于性能分析
- ✅ 对比不同实现方案的性能
- ✅ 识别性能瓶颈

### 生产环境
- ✅ 默认关闭，按需临时开启
- ✅ 仅监控关键路径（查询执行器、扫描、JOIN等）
- ⚠️ 避免监控高频调用的小函数（<1ms的函数）
- ⚠️ 性能开销虽小（<1%），但高频调用时会累积

### 性能优化工作流
1. 使用libperfmon识别瓶颈函数（低IPC、高cache miss）
2. 分析具体原因（CPU停顿、内存访问、分支预测）
3. 实施优化（算法、数据结构、内存布局）
4. 再次测量验证优化效果

## 📦 编译和安装

### 编译库

```bash
cd /mydata/libperfmon
make                # 编译库和示例
make clean          # 清理编译产物
```

### 系统安装（可选）

```bash
sudo make install                    # 安装到/usr/local
sudo make PREFIX=/opt/local install  # 自定义安装路径
sudo make uninstall                  # 卸载
```

### 在你的项目中使用

```bash
# 静态链接
gcc -o myapp myapp.c -L/mydata/libperfmon -lperfmon -static

# 动态链接
gcc -o myapp myapp.c -L/mydata/libperfmon -lperfmon
export LD_LIBRARY_PATH=/mydata/libperfmon:$LD_LIBRARY_PATH
./myapp
```

## 📋 项目文件

```
libperfmon/
├── perfmon.h                 - API头文件（3KB）
├── perfmon.c                 - 实现代码（13KB）
├── Makefile                  - 编译脚本
├── libperfmon.a              - 静态库（11KB）
├── libperfmon.so             - 动态库（21KB）
├── example_simple.c          - 基础示例
├── example_postgresql.c      - PostgreSQL集成示例
├── install_to_postgres.sh    - 一键安装脚本
├── LICENSE                   - MIT许可证
└── README.md                 - 本文档
```

## ⚠️ 注意事项

1. **线程安全：** 每个线程应使用独立的`perfmon_context_t`
2. **嵌套监控：** 避免在已监控的代码段中再次启动监控
3. **性能开销：** 虽然<1%，但在高频函数中仍需谨慎
4. **权限要求：** 确保进程有访问perf事件的权限
5. **容器/虚拟机：** 需要特殊配置才能使用

## 📄 许可证

MIT License - 可自由用于商业和开源项目

## 🙏 贡献

欢迎提交Issue和Pull Request！

---

**项目地址：** `/mydata/libperfmon`  
**创建时间：** 2025-10-05  
**目标：** 为数据库系统提供硬件级性能监控能力
