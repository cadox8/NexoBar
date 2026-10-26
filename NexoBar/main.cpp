//
// Created by cadox8 on 23/10/2026.
//

#include <exception>

#include "NexoBar.h"
#include "log/Log.h"

int main() {
    Log::Init();

    NexoBarImp::set(new NexoBar());
    NexoBarImp app;

    int exitCode = 0;
    try {
        app->run();
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Core) << "Application startup or runtime failed: " << error.what();
        exitCode = 1;
    } catch (...) {
        NEXO_ERROR(LogModule::Core) << "Application failed with an unknown error";
        exitCode = 1;
    }

    app->shutdown();
    NexoBarImp::set(nullptr);
    Log::Shutdown();
    return exitCode;
}
