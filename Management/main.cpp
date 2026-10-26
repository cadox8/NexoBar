//
// Created by cadox8 on 23/10/2026.
//

#include "Management.h"

int main() {
    Management management;
    ManagementImp::set(&management);
    management.init();
    management.shutdown();
    ManagementImp::set(nullptr);
    return 0;
}
