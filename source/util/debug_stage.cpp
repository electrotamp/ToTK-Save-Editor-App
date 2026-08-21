#include "util/totk_log.hpp"

#include <cstdarg>
#include <cstdio>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace totk {

namespace {

int gStageCounter = 0;

uint64_t stageNowMs() {
#if defined(__SWITCH__)
    return armTicksToNs(armGetSystemTick()) / 1000000ULL;
#else
    return 0;
#endif
}

}  // namespace

int debugStageCounter() { return gStageCounter; }

void resetStageLog() {
    gStageCounter = 0;
    TOTK_LOG("stage trace reset (nxlink stderr + nxlink/logs/run-*.log)");
}

void logStage(const char* fmt, ...) {
    char body[640];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    TOTK_LOG("stage #%04d t=%llums %s", ++gStageCounter, static_cast<unsigned long long>(stageNowMs()),
             body);
}

void logHeapSnapshot(const char* tag) {
    // mallinfo()/svcGetInfo fault on some libnx builds when called mid-frame; keep stages only.
    (void)tag;
}

}  // namespace totk
