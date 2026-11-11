// Neutro X - registro simples (stderr + arquivo opcional).
#pragma once
#include <string>

namespace nx {

void logSetFile(const std::string& path);  // "" desativa arquivo
void logInfo(const char* fmt, ...);
void logWarn(const char* fmt, ...);
void logError(const char* fmt, ...);
int logErrorCount();

}  // namespace nx
