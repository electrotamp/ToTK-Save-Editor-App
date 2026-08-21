#pragma once

namespace totk {

int debugStageCounter();
void logStage(const char* fmt, ...);
void resetStageLog();
void logHeapSnapshot(const char* tag);

}  // namespace totk

#ifdef TOTK_NXLINK_DEBUG
#define TOTK_STAGE(fmt, ...) totk::logStage(fmt, ##__VA_ARGS__)
#define TOTK_HEAP(tag) totk::logHeapSnapshot(tag)
#else
#define TOTK_STAGE(fmt, ...) ((void)0)
#define TOTK_HEAP(tag) ((void)0)
#endif
