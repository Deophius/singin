#include <iostream>
#include <fstream>
#include <cstdlib>
#include "../singd.h"

int main() {
    using namespace Spirit;
    nlohmann::json config;
    std::ifstream config_file("man.json", std::ios::in);
    config_file >> config;
    std::shared_mutex mut;
    Watchdog watchdog(config, mut);
    watchdog.start();
    std::system("pause");
}