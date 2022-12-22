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
            if (config[entry] <= 0) {
                error_dialog("Value error", entry + " should be positive!"s);
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
        auto check_bool = [&config](const char* entry) {
            if (!config.contains(entry)) {
                error_dialog("Missing entry", entry + " expected, but not found!"s);
                return false;
            }
            if (!config[entry].is_boolean()) {
                error_dialog("Type error", entry + " should be a bool!"s);
                return false;
            }
            return true;
        };
        return check_int("gs_port") && check_int("serv_port") && check_str("url_stu_new")
            && check_str("dbname") && check_str("passwd") && check_str("intro")
            && check_int("watchdog_poll") && check_int("retry_wait")
            && check_int("keep_logs") && check_int("timeout") && check_bool("auto_watchdog")
            && check_int("simul_limit") && check_int("local_limit");
    }

    void error_dialog(std::string_view caption, std::string_view text) {
        ::MessageBox(NULL, text.data(), caption.data(), MB_ICONERROR);
    }

    void kill_lock_mouse() {
        std::string cmd = "taskkill.exe /f /im LockMouse.exe";
        STARTUPINFO start;
        ::ZeroMemory(&start, sizeof(start));
        start.cb = sizeof(STARTUPINFO);
        PROCESS_INFORMATION proc_info;
        ::ZeroMemory(&proc_info, sizeof(proc_info));
        ::CreateProcessA(
            NULL, cmd.data(), NULL, NULL, TRUE,
            CREATE_NO_WINDOW, NULL, NULL, &start, &proc_info
        );
        // Wait until child process exits.
        ::WaitForSingleObject(proc_info.hProcess, INFINITE);
        // Close process and thread handles. 
        ::CloseHandle(proc_info.hProcess);
        ::CloseHandle(proc_info.hThread);
    }
}

int main() {
    using namespace Spirit;
    hide_window();
    Configuration config;
    {
        // Put these into a new scope to ensure the log will be closed when entering singer
        // First override the old contents here. The singer will open a new config.
        Logfile logfile("startup.log");
        logfile << "About to kill the lock mouse" << std::endl;
        kill_lock_mouse();
        logfile << "Called kill_lock_mouse, last error was " << ::GetLastError() << std::endl;
        std::ifstream istr("man.json");
        if (!istr) {
            logfile << "Cannot open configuration file, exiting.\n";
            error_dialog("Config error", "Cannot open the configuration!");
            return 1;
        }
        try {
            istr >> config;
        } catch (const decltype(config)::parse_error& ex) {
            error_dialog("Config error", ex.what());
            logfile << "Config error: " << ex.what();
            return 1;
        }
        istr.close();
        if (!validate(config)) {
            error_dialog("Config error", "Error with the man.json configuration file!");
            logfile << "Config error: didn't pass the validator test\n";
            return 1;
        }
        if (!check_db(config))
            logfile << "Warning: database is corrupt!\n";
    }
    // Now we can be absolutely sure that keep_logs exist and is larger than 0.
    auto logname = select_logfile("singer", config["keep_logs"]);
    std::filesystem::rename("startup.log", logname);
    Logfile logfile(logname, std::ios::out | std::ios::app);
    Watchdog watchdog(config);
    Singer singer(config);
    if (!config["auto_watchdog"])
        watchdog.pause();
    watchdog.start();
    singer.mainloop(watchdog, logfile);
}