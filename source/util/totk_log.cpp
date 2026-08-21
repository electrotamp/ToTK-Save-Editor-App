#include "util/totk_log.hpp"

#include <cstdarg>
#include <cstdio>

#if defined(__SWITCH__)
#include <switch.h>
#endif

extern "C" {

void totk_log_session_start(void) { totk::logSessionStart(); }

void totk_logf(const char* fmt, ...) {
    char buffer[768];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    totk::logMessage("%s", buffer);
}

}  // extern "C"

namespace totk {

namespace {

void appendToFile(const char* path, const char* line) {
#if defined(__SWITCH__)
    FILE* file = std::fopen(path, "a");
    if (file) {
        std::fprintf(file, "%s\n", line);
        std::fclose(file);
    }
#else
    (void)path;
    (void)line;
#endif
}

void writeLogLine(const char* prefix, const char* path, const char* line) {
    char formatted[832];
    std::snprintf(formatted, sizeof(formatted), "[%s] %s", prefix, line);
    std::fprintf(stderr, "%s\n", formatted);
    std::fflush(stderr);
    appendToFile(path, formatted);
}

void formatWithTimestamp(char* buffer, size_t size, const char* prefix, const char* body) {
#if defined(__SWITCH__)
    const u64 ticks = armGetSystemTick();
    const u64 ms = armTicksToNs(ticks) / 1000000ULL;
    std::snprintf(buffer, size, "%s t=%llums %s", prefix, static_cast<unsigned long long>(ms), body);
#else
    std::snprintf(buffer, size, "%s %s", prefix, body);
#endif
}

}  // namespace

void logMessage(const char* fmt, ...) {
    char body[768];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    writeLogLine("totk", "sdmc:/totk-save-editor.log", body);
}

void logAction(const char* fmt, ...) {
    char body[768];
    std::va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    char stamped[832];
    formatWithTimestamp(stamped, sizeof(stamped), "action", body);
    writeLogLine("totk", "sdmc:/totk-save-editor-actions.log", stamped);
}

void logSessionStart() {
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "session start build=%s", TOTK_BUILD_ID);
    writeLogLine("totk", "sdmc:/totk-save-editor.log", "");
    writeLogLine("totk", "sdmc:/totk-save-editor.log", "====================");
    writeLogLine("totk", "sdmc:/totk-save-editor.log", buffer);
    writeLogLine("totk", "sdmc:/totk-save-editor.log", "nxlink: all [totk] lines mirror here (stderr)");
    writeLogLine("totk", "sdmc:/totk-save-editor.log", "sdmc backup: sdmc:/totk-save-editor.log (optional)");
    writeLogLine("totk", "sdmc:/totk-save-editor.log", "====================");

    writeLogLine("totk", "sdmc:/totk-save-editor-actions.log", "");
    writeLogLine("totk", "sdmc:/totk-save-editor-actions.log", "====================");
    writeLogLine("totk", "sdmc:/totk-save-editor-actions.log", buffer);
    writeLogLine("totk", "sdmc:/totk-save-editor-actions.log", "====================");
}

}  // namespace totk
