#pragma once
#include <sstream>
#include <string>

namespace logm {
    void Init();

    class Logger {
    public:
        explicit Logger(const char* level);
        ~Logger();

        template<typename T>
        Logger& operator<<(const T& val) {
            buffer_ << val;
            return *this;
        }

        Logger& operator<<(const std::wstring& val);

    private:
        std::ostringstream buffer_;
        const char* level_;
    };
}

#define LOG_INFO  ::logm::Logger("INFO")
#define LOG_WARN  ::logm::Logger("WARN")
#define LOG_ERR   ::logm::Logger("ERR ")
