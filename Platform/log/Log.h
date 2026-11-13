#pragma once

#include <iostream>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <sstream>
#include <string>
#include <filesystem>
#include <ranges>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
#endif

#include <boost/date_time/posix_time/posix_time.hpp>

namespace fs = std::filesystem;

// ================= ENUMS =================
enum class LogLevel { Debug, Info, Warning, Error };

enum class LogModule { Core, Printer, Database };

// ================= CONSOLE COLORS =================
enum class ConsoleColor {
    Reset,
    Black, Red, Green, Yellow, Blue, Magenta, Cyan, White,
    Bold, BoldOff
};

inline const char* ToAnsi(const ConsoleColor& color) {
    switch (color) {
        case ConsoleColor::Reset: return "\033[0m";
        case ConsoleColor::Black: return "\033[30m";
        case ConsoleColor::Red: return "\033[31m";
        case ConsoleColor::Green: return "\033[32m";
        case ConsoleColor::Yellow: return "\033[33m";
        case ConsoleColor::Blue: return "\033[34m";
        case ConsoleColor::Magenta: return "\033[35m";
        case ConsoleColor::Cyan: return "\033[36m";
        case ConsoleColor::White: return "\033[37m";
        case ConsoleColor::Bold: return "\033[1m";
        case ConsoleColor::BoldOff: return "\033[22m";
        default: return "\033[0m";
    }
}

// ================= HELPERS =================
inline const std::string ToString(const LogLevel& l) {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNK";
}

inline std::string ToString(const LogModule& m) {
    switch (m) {
        case LogModule::Core: return "Core";
        case LogModule::Printer: return "Printer";
        case LogModule::Database: return "Database";
        default: return "Unknown";
    }
}

inline std::string TimestampFile() {
    auto now = boost::posix_time::second_clock::local_time();
    std::string ts = boost::posix_time::to_iso_extended_string(now);
    std::ranges::replace(ts, ':', '-');
    std::ranges::replace(ts, 'T', '_');
    return ts;
}

inline std::string TimestampPretty() {
    const auto now = boost::posix_time::microsec_clock::local_time();

    std::ostringstream ss;
    ss.imbue(std::locale(ss.getloc(),
                         new boost::posix_time::time_facet("%d-%m-%Y %H:%M:%S.%f")));

    ss << now;
    return ss.str();
}

inline std::string StripColors(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
            while (i < text.size() && text[i] != 'm') ++i;
            if (i < text.size()) ++i;
        } else {
            result += text[i];
        }
    }
    return result;
}

// ================= MESSAGE =================
struct LogMessage {
    LogLevel level;
    LogModule module;
    std::string text;

    LogMessage(const LogLevel& lvl, const LogModule& mod, std::string&& msg)
        : level(lvl), module(mod), text(std::move(msg)) {}
};

// ================= LOGGER =================
class Log {
public:
    static void Init(const size_t& maxFileSize = 5 * 1024 * 1024) {
        if (m_running) return;

#ifdef _WIN32
        EnableANSI();
#endif

        m_maxFileSize = maxFileSize;
        fs::create_directories("logs/");
        m_filename = "logs/Log_NexoBar_" + TimestampFile() + ".log";
        m_file.open(m_filename, std::ios::out | std::ios::app);

        m_running = true;
        m_thread = std::thread(ProcessQueue);
    }

    static void Shutdown() {
        m_running = false;
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
        if (m_file.is_open()) m_file.close();
    }

    static void Push(const LogLevel& lvl, const LogModule& mod, std::string&& msg) {
        std::lock_guard lock(m_mutex);
        m_queue.emplace(lvl, mod, std::move(msg));
        m_cv.notify_one();
    }

private:
    static inline std::queue<LogMessage> m_queue;
    static inline std::mutex m_mutex;
    static inline std::condition_variable m_cv;
    static inline std::thread m_thread;
    static inline std::atomic<bool> m_running{false};
    static inline std::ofstream m_file;
    static inline std::string m_filename;
    static inline size_t m_maxFileSize = 0;

#ifdef _WIN32
    static void EnableANSI() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, mode);
            }
        }
    }
#endif

    static void RotateIfNeeded() {
        if (!m_file.is_open() || fs::file_size(m_filename) < m_maxFileSize) return;
        m_file.close();
        m_filename = "logs/Log_NexoBar_" + TimestampFile() + ".log";
        m_file.open(m_filename, std::ios::out | std::ios::app);
    }

    static void ProcessQueue() {
        while (m_running || !m_queue.empty()) {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [] { return !m_running || !m_queue.empty(); });

            while (!m_queue.empty()) {
                LogMessage msg = std::move(m_queue.front());
                m_queue.pop();
                lock.unlock();
                Write(msg);
                lock.lock();
            }
        }
    }

    static void Write(const LogMessage& msg) {
        const std::string prefix = TimestampPretty() + " [" + ToString(msg.level) + "] [" + ToString(msg.module) + "] ";

        std::cout << "\033[0m" << Color(msg.level) << prefix << "\033[0m" << msg.text << "\033[0m" << std::endl;

        if (m_file.is_open()) {
            std::string clean = StripColors(msg.text);
            m_file << prefix << clean << std::endl;
            m_file.flush();
            RotateIfNeeded();
        }
    }

    static const char* Color(const LogLevel& lvl) {
        switch (lvl) {
            case LogLevel::Debug: return "\033[34m"; // Blue
            case LogLevel::Info: return "\033[32m"; // Green
            case LogLevel::Warning: return "\033[33m"; // Yellow
            case LogLevel::Error: return "\033[31m"; // Red
            default: return "\033[0m";
        }
    }
};

// ================= LOG STREAM =================
class LogStream {
public:
    LogStream(const LogLevel& lvl, const LogModule& mod)
        : level(lvl), module(mod) {}

    ~LogStream() {
        Log::Push(level, module, std::move(ss.str()));
    }

    template<typename T>
    LogStream& operator<<(const T& v) {
        ss << v;
        return *this;
    }

    LogStream& operator<<(const bool& v) {
        ss << (v ? "true" : "false");
        return *this;
    }

    LogStream& operator<<(const ConsoleColor& color) {
        ss << ToAnsi(color);
        return *this;
    }

private:
    LogLevel level;
    LogModule module;
    std::stringstream ss;
};

// ================= MACROS =================
#define NEXO_DEBUG(mod)   LogStream(LogLevel::Debug,   mod)
#define NEXO_INFO(mod)    LogStream(LogLevel::Info,    mod)
#define NEXO_WARN(mod)    LogStream(LogLevel::Warning, mod)
#define NEXO_WWARN(mod)   NEXO_WARN(mod)
#define NEXO_ERROR(mod)   LogStream(LogLevel::Error,   mod)
