#include "../singd.h"
#include <fstream>

int main() {
    using namespace Spirit;
    Configuration config;
    {
        std::ifstream file("man.json", std::ios::in);
        file >> config;
    }
    Singer singer(config);
    singer.mainloop();
}