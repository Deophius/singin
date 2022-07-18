#include "app.h"
#include <windows.h>
#include <string_view>
#include <fstream>

namespace Spirit {
    void hide_window() noexcept {
        ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
    }

    bool validate(const Configuration& config) {
        auto check_int = [&config](const char* entry) {
            return config.contains(entry) && config[entry].is_number_integer();
        };
        auto check_str = [&config](const char* entry) {
            return config.contains(entry) && config[entry].is_string();
        };
        return check_int("gs_port") && check_int("serv_port") && check_str("url_stu_new")
            && check_str("dbname") && check_str("passwd") && check_str("intro");
    }

    void error_dialog(std::string_view caption, std::string_view text) {
        ::MessageBox(NULL, text.data(), caption.data(), MB_ICONERROR);
    }
}

int main() {
    using namespace Spirit;
    hide_window();
    Configuration config;
    std::ifstream istr("man.json");
    if (!istr) {
        error_dialog("Config error", "Cannot open the configuration!");
        return 1;
    }
    istr >> config;
    istr.close();
    if (!validate(config)) {
        error_dialog("Config error", "Error with the man.json configuration file!");
        return 1;
    }
    Watchdog watchdog(config);
    Singer singer(config);
    watchdog.start();
    singer.mainloop();
}