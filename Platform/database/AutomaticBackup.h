#pragma once

#include "../export.h"
#include "Connection.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace Nearon {
    class PLATFORM_API AutomaticBackup {
    public:
        struct Options {
            std::chrono::hours interval{24};
            std::chrono::days retention{30};
        };

        AutomaticBackup(Connection& source, std::filesystem::path directory, Options options = {},
                        std::function<void(const std::string&)> onError = {});
        ~AutomaticBackup();
        AutomaticBackup(const AutomaticBackup&) = delete;
        AutomaticBackup& operator=(const AutomaticBackup&) = delete;

        void start();
        void stop();
        void backupNow();

    private:
        void run();
        void prune();

        Connection& source_;
        std::filesystem::path directory_;
        Options options_;
        std::function<void(const std::string&)> onError_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stopping_ = false;
        bool started_ = false;
        std::thread worker_;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
