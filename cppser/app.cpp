#include "app.h"
#include <windows.h>
#include <string_view>
#include <fstream>

namespace Spirit {
    void hide_window() noexcept {
        ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
    }

    static bool check_db(const Configuration& config) {
        try {
            Connection conn(config["dbname"], config["passwd"]);
            Statement(conn, "select count(type) from sqlite_master").next();
            Statement(conn, "select count(学生名称), count(打卡时间), count(学生编号) from 上课考勤")
                .next();
            Statement(conn, "select count(安排ID), count(考勤结束时间), count(ID) from 课程信息").next();
            Statement(conn, "select count(TerminalID) from Local_Visual_Publish").next();
            return true;
        } catch (const ErrorOpeningDatabase& ex) {
            error_dialog("Error opening database", ex.what());
        } catch (const SQLError& ex) {
            error_dialog(
                "Bad database file",
                ex.what() + "\nMaybe your file is corrupt or password is wrong."s
            );
        }
        return false;
    }

    bool validate(const Configuration& config) {
        auto check_int = [&config](const char* entry) {
            if (!config.contains(entry)) {
                error_dialog("Missing entry", entry + " expected, but not found!"s);
                return false;
            }
            if (!config[entry].is_number_integer()) {
                error_dialog("Type error", entry + " should be an int!"s);
                return false;
            }
            return true;
        };
        auto check_str = [&config](const char* entry) {
            if (!config.contains(entry)) {
                error_dialog("Missing entry", entry + " expected, but not found!"s);
                return false;
            }
            if (!config[entry].is_string()) {
                error_dialog("Type error", entry + " should be a string!"s);
                return false;
            }
            return true;
        };
        return check_int("gs_port") && check_int("serv_port") && check_str("url_stu_new")
            && check_str("dbname") && check_str("passwd") && check_str("intro")
            && check_int("watchdog_poll") && check_int("retry_wait") && check_db(config);
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
    try {
        istr >> config;
    } catch (const decltype(config)::parse_error& ex) {
        error_dialog("Config error", ex.what());
        return 1;
    }
    istr.close();
    if (!validate(config)) {
        error_dialog("Config error", "Error with the man.json configuration file!");
        return 1;
    }
    Watchdog watchdog(config);
    Singer singer(config);
    watchdog.start();
    singer.mainloop(watchdog);
}