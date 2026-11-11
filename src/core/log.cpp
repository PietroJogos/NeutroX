// Neutro X - registro. Ver log.h.
#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace nx {

namespace {
std::mutex g_mtx;
FILE* g_file = nullptr;
int g_errors = 0;

void vlog(const char* level, const char* fmt, va_list ap) {
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    std::lock_guard<std::mutex> lock(g_mtx);
    std::fprintf(stderr, "[%s] %s\n", level, buf);
    if (g_file) {
        std::fprintf(g_file, "[%s] %s\n", level, buf);
        std::fflush(g_file);
    }
}
}  // namespace

void logSetFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
    if (!path.empty()) g_file = std::fopen(path.c_str(), "w");
}

void logInfo(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog("INFO", fmt, ap); va_end(ap); }
void logWarn(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog("AVISO", fmt, ap); va_end(ap); }
void logError(const char* fmt, ...) {
    { std::lock_guard<std::mutex> lock(g_mtx); ++g_errors; }
    va_list ap; va_start(ap, fmt); vlog("ERRO", fmt, ap); va_end(ap);
}
int logErrorCount() { std::lock_guard<std::mutex> lock(g_mtx); return g_errors; }

}  // namespace nx
