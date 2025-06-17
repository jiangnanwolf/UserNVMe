#include "user_nvme.h"

int main() {

    UserNVMe userNVMe;
    
    int ret = userNVMe.initMMIO();
    if (ret) return ret;

    userNVMe.printCap();

    userNVMe.printVersion();

    userNVMe.setupAdminQueue();

    userNVMe.identifyController();

    return 0;
}