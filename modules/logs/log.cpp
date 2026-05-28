#include "logs/log.h"
#include "path/path.h"
#include <cstdio>
#include <ctime>
#include <string>
#include <windows.h>

static FILE* g_logFile = nullptr;

namespace logm {
    void Init() {
        std::wstring logPath = path::GetExeDir() + L"\\wallpaper.log";
        if (g_logFile) {
            std::fclose(g_logFile);
            g_logFile = nullptr;
        }
        g_logFile = _wfopen(logPath.c_str(), L"w");
    }

    Logger::Logger(const char* level) : level_(level) {}

    Logger::~Logger() {
        if (g_logFile && !buffer_.str().empty()) {
            std::time_t t = std::time(nullptr);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char timeBuf[32] = {};
            std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);
            std::fprintf(g_logFile, "[%s] %s - %s\n", level_, timeBuf, buffer_.str().c_str());
            std::fflush(g_logFile);
        }
    }

    Logger& Logger::operator<<(const std::wstring& val) {
        int len = WideCharToMultiByte(CP_UTF8, 0, val.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string utf8(static_cast<size_t>(len) - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, val.c_str(), -1, utf8.data(), len, nullptr, nullptr);
            buffer_ << utf8;
        }
        return *this;
    }
}
