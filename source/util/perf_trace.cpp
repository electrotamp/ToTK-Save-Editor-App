#include "util/perf_trace.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "util/totk_log.hpp"

#if defined(__SWITCH__)
#include <malloc.h>
#include <switch.h>
#endif

namespace totk {

namespace {

constexpr uint64_t kSlowImageLoadMs = 8;
constexpr uint64_t kFrameSummaryWindowMs = 5000;
constexpr uint64_t kMemSummaryWindowMs = 2000;

#if defined(__SWITCH__)
void logMemorySummary() {
    u64 totalMem = 0, usedMem = 0, totalNonSys = 0, usedNonSys = 0, isApp = 0;
    svcGetInfo(&totalMem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&usedMem, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&totalNonSys, InfoType_TotalNonSystemMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&usedNonSys, InfoType_UsedNonSystemMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&isApp, InfoType_IsApplication, CUR_PROCESS_HANDLE, 0);

    const uint64_t totalMb = totalMem / (1024 * 1024);
    const uint64_t usedMb = usedMem / (1024 * 1024);
    const uint64_t freeMb = totalMb > usedMb ? totalMb - usedMb : 0;
    const uint64_t nonSysTotalMb = totalNonSys / (1024 * 1024);
    const uint64_t nonSysUsedMb = usedNonSys / (1024 * 1024);
    const uint64_t nonSysFreeMb = nonSysTotalMb > nonSysUsedMb ? nonSysTotalMb - nonSysUsedMb : 0;

    perfLog(
        "mem total_mb=%llu used_mb=%llu free_mb=%llu nonsys_total_mb=%llu nonsys_used_mb=%llu "
        "nonsys_free_mb=%llu is_application=%llu",
        static_cast<unsigned long long>(totalMb), static_cast<unsigned long long>(usedMb),
        static_cast<unsigned long long>(freeMb), static_cast<unsigned long long>(nonSysTotalMb),
        static_cast<unsigned long long>(nonSysUsedMb), static_cast<unsigned long long>(nonSysFreeMb),
        static_cast<unsigned long long>(isApp));

    // svcGetInfo's "used" figure reflects libnx's one-time svcSetHeapSize land-grab at boot
    // (claims ~all available memory as heap territory immediately), not live allocations. What
    // actually moves as the app runs is newlib's own arena accounting via mallinfo().
    const struct mallinfo mi = mallinfo();
    const uint64_t arenaMb = static_cast<uint64_t>(mi.arena) / (1024 * 1024);
    const uint64_t heapInUseMb = static_cast<uint64_t>(mi.uordblks) / (1024 * 1024);
    const uint64_t heapFreeMb = static_cast<uint64_t>(mi.fordblks) / (1024 * 1024);
    perfLog("heap arena_mb=%llu in_use_mb=%llu free_in_arena_mb=%llu", static_cast<unsigned long long>(arenaMb),
            static_cast<unsigned long long>(heapInUseMb), static_cast<unsigned long long>(heapFreeMb));
}
#else
void logMemorySummary() {}
#endif

struct ImageBatchState {
    const char* label = nullptr;
    int generation = 0;
    uint64_t startMs = 0;
    uint64_t loads = 0;
    uint64_t failures = 0;
    uint64_t bytes = 0;
    uint64_t decodeMs = 0;
    uint64_t slowLoads = 0;
};

ImageBatchState gImageBatch;

const char* shortPath(const char* path) {
    if (!path || !path[0]) return "(null)";
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

}  // namespace

uint64_t perfNowMs() {
#if defined(__SWITCH__)
    return armTicksToNs(armGetSystemTick()) / 1000000ULL;
#else
    return 0;
#endif
}

void perfLog(const char* fmt, ...) {
#if !defined(TOTK_NXLINK_DEBUG)
    (void)fmt;
    return;
#else
    char body[640];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    TOTK_LOG("perf: t=%llums %s", static_cast<unsigned long long>(perfNowMs()), body);
#endif
}

PerfScope::PerfScope(const char* name) : name_(name), startMs_(perfNowMs()) {
    perfLog("begin %s", name_);
}

PerfScope::~PerfScope() {
    perfLog("end %s ms=%llu", name_, static_cast<unsigned long long>(perfNowMs() - startMs_));
}

void FrameMonitor::tick() {
#if !defined(TOTK_NXLINK_DEBUG)
    return;
#else
    static uint64_t lastMs = 0;
    static uint64_t windowStartMs = 0;
    static uint64_t frameCount = 0;
    static uint64_t totalDt = 0;
    static unsigned hitches33 = 0;
    static unsigned hitches50 = 0;
    static unsigned hitches100 = 0;
    static uint64_t memWindowStartMs = 0;

    const uint64_t now = perfNowMs();
    if (lastMs == 0) {
        lastMs = now;
        windowStartMs = now;
        memWindowStartMs = now;
        logMemorySummary();
        return;
    }

    if (now - memWindowStartMs >= kMemSummaryWindowMs) {
        memWindowStartMs = now;
        logMemorySummary();
    }

    const uint64_t dt = now - lastMs;
    lastMs = now;
    frameCount++;
    totalDt += dt;

    if (dt >= 100) {
        ++hitches100;
        perfLog("hitch ms=%llu tier=100", static_cast<unsigned long long>(dt));
    } else if (dt >= 50) {
        ++hitches50;
        perfLog("hitch ms=%llu tier=50", static_cast<unsigned long long>(dt));
    } else if (dt >= 33) {
        ++hitches33;
    }

    if (now - windowStartMs < kFrameSummaryWindowMs) {
        return;
    }

    const uint64_t avgMs = frameCount > 0 ? totalDt / frameCount : 0;
    perfLog("frame_summary frames=%llu avg_ms=%llu hitches33=%u hitches50=%u hitches100=%u",
            static_cast<unsigned long long>(frameCount), static_cast<unsigned long long>(avgMs), hitches33,
            hitches50, hitches100);

    windowStartMs = now;
    frameCount = 0;
    totalDt = 0;
    hitches33 = 0;
    hitches50 = 0;
    hitches100 = 0;
#endif
}

void beginImageBatch(const char* label, int generation) {
#if !defined(TOTK_NXLINK_DEBUG)
    (void)label;
    (void)generation;
    return;
#else
    gImageBatch.label = label ? label : "icons";
    gImageBatch.generation = generation;
    gImageBatch.startMs = perfNowMs();
    gImageBatch.loads = 0;
    gImageBatch.failures = 0;
    gImageBatch.bytes = 0;
    gImageBatch.decodeMs = 0;
    gImageBatch.slowLoads = 0;
#endif
}

void recordImageLoad(bool ok, long bytes, uint64_t loadMs, const char* path) {
#if !defined(TOTK_NXLINK_DEBUG)
    (void)ok;
    (void)bytes;
    (void)loadMs;
    (void)path;
    return;
#else
    gImageBatch.loads++;
    gImageBatch.decodeMs += loadMs;
    if (!ok) {
        gImageBatch.failures++;
        return;
    }

    if (bytes > 0) {
        gImageBatch.bytes += static_cast<uint64_t>(bytes);
    }

    if (loadMs < kSlowImageLoadMs) {
        return;
    }

    gImageBatch.slowLoads++;
    perfLog("image_slow ms=%llu bytes=%ld path=%s", static_cast<unsigned long long>(loadMs), bytes,
            shortPath(path));
#endif
}

void endImageBatch(int tileCount, int generation) {
#if !defined(TOTK_NXLINK_DEBUG)
    (void)tileCount;
    (void)generation;
    return;
#else
    if (gImageBatch.startMs == 0) {
        return;
    }

    const uint64_t wallMs = perfNowMs() - gImageBatch.startMs;
    perfLog(
        "icon_batch label=%s tiles=%d gen=%d wall_ms=%llu decode_ms=%llu loads=%llu fail=%llu slow=%llu bytes=%llu",
        gImageBatch.label, tileCount, generation, static_cast<unsigned long long>(wallMs),
        static_cast<unsigned long long>(gImageBatch.decodeMs), static_cast<unsigned long long>(gImageBatch.loads),
        static_cast<unsigned long long>(gImageBatch.failures), static_cast<unsigned long long>(gImageBatch.slowLoads),
        static_cast<unsigned long long>(gImageBatch.bytes));

    gImageBatch = {};
#endif
}

}  // namespace totk
