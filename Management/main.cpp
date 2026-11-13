//
// Created by cadox8 on 23/10/2026.
//

#include "Management.h"

#include <exception>
#include <iostream>
#include <string_view>

int main(const int argc, char** argv) {
    Management management;
    ManagementImp::set(&management);
    if (argc >= 2 && std::string_view(argv[1]) == "--initialize-defaults") {
        try {
            const std::filesystem::path databasePath = argc >= 3 ? argv[2] : "nexobar.db";
            management.initializeDefaults(databasePath);
            std::cout << "Default data updated without modifying the existing business." << '\n';
            management.shutdown();
            ManagementImp::set(nullptr);
            return 0;
        } catch (const std::exception& error) {
            std::cerr << management.userError(error) << '\n';
            management.shutdown();
            ManagementImp::set(nullptr);
            return 1;
        }
    }
    management.init();
    management.shutdown();
    ManagementImp::set(nullptr);
    return 0;
}
