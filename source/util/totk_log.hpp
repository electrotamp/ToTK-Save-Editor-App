#pragma once

#include <cstdio>

#ifndef TOTK_BUILD_ID
#define TOTK_BUILD_ID __DATE__ " " __TIME__
#endif

namespace totk {

void logMessage(const char* fmt, ...);
void logAction(const char* fmt, ...);
void logSessionStart();

}  // namespace totk

#define TOTK_LOG(fmt, ...) totk::logMessage(fmt, ##__VA_ARGS__)
#define TOTK_ACTION(fmt, ...) totk::logAction(fmt, ##__VA_ARGS__)
