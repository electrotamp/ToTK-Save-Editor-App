#pragma once

#include <cstdint>

namespace totk {

uint64_t perfNowMs();

void perfLog(const char* fmt, ...);

class PerfScope {
public:
    explicit PerfScope(const char* name);
    ~PerfScope();

    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;

private:
    const char* name_;
    uint64_t startMs_;
};

class FrameMonitor {
public:
    static void tick();
};

void beginImageBatch(const char* label, int generation);
void recordImageLoad(bool ok, long bytes, uint64_t loadMs, const char* path);
void endImageBatch(int tileCount, int generation);

}  // namespace totk

#ifdef TOTK_NXLINK_DEBUG
#define TOTK_PERF(fmt, ...) totk::perfLog(fmt, ##__VA_ARGS__)
#define TOTK_PERF_SCOPE_CONCAT_(a, b) a##b
#define TOTK_PERF_SCOPE_CONCAT(a, b) TOTK_PERF_SCOPE_CONCAT_(a, b)
// __LINE__-suffixed so multiple scopes can coexist in one enclosing scope
// (e.g. a whole-function scope plus a later "remaining work" scope) without
// each call site needing its own nested {} block just to avoid a name clash.
#define TOTK_PERF_SCOPE(name) totk::PerfScope TOTK_PERF_SCOPE_CONCAT(_totkPerfScope, __LINE__)(name)
#else
#define TOTK_PERF(fmt, ...) ((void)0)
#define TOTK_PERF_SCOPE(name) ((void)0)
#endif
