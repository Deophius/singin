#include "singd.h"

namespace Spirit {
    // Chores come first.
    Watchdog::Watchdog(const Spirit::Configuration& config) :
        mConfig(config)
    {}

    Watchdog::~Watchdog() noexcept {
        mStopToken = true;
        if (mThread && mThread->joinable()) {
            mThread->join();
        }
    }

    void Watchdog::start() {
        mThread.reset(new std::thread([this]{ worker(); }));
    }

    void Watchdog::pause() noexcept {
        mPauseToken = true;
    }

    void Watchdog::resume() noexcept {
        mPauseToken = false;
    }

    void Watchdog::simul_sign(Connection& conn, const LessonInfo& lesson, Logfile& logfile) {
        auto absent = report_absent(conn, lesson.id);
        // The JSON result from server
        auto stu_new = get_stu_new(mConfig, absent, lesson, logfile);
        // People who need DK
        std::vector<Student> need_card;
        need_card.reserve(absent.size());
        // People who are invalid, represented as names
        std::vector<std::string> invalid;
        invalid.reserve(60);
        // First calculate the invalids
        for (auto&& stu : stu_new["result"]["students"]) {
            if (stu["Invalid"])
                invalid.push_back(stu["StudentName"]);
        }
        // Then O(n2) calculate the difference.
        for (auto&& i : absent) {
            bool flag = true;
            for (auto&& j : invalid)
                if (i.name == j) {
                    flag = false;
                    break;
                }
            if (flag)
                need_card.push_back(std::move(i));
        }
        logfile << "Invalid: " << invalid.size() << "   Need card: " << need_card.size() << '\n';
        if (need_card.empty())
            return;
        // If we restart here, we can take advantage of the restarting time,
        // to avoid collision.
        // Because both exceptions can be fallen through without affecting the other code,
        // so we handle them in this function instead of propagating them upward.
        try {
            send_to_gs(mConfig, logfile, "$DoRestart");
        } catch (const NetworkError& ex) {
            logfile << "Networking error when restarting GS: " << ex.what() << '\n';
        } catch (const GSError& ex) {
            logfile << "GS internal error when we asked it to restart, quite strange! Output:\n"
                << ex.what() << '\n';
        }
        RandomClock clock(lesson.endtime - 300, lesson.endtime - 120);
        write_record(conn, lesson.id, need_card, clock);
    }

    void Watchdog::local_sign(Connection& localdata, const LessonInfo& lesson, Logfile& logfile) {
        auto need_card = report_absent(localdata, lesson.id, true);
        logfile << "Need card: " << need_card.size() << '\n';
        RandomClock clock(lesson.endtime - 300, lesson.endtime - 120);
        write_record(localdata, lesson.id, need_card, clock);
        // See the comment above
        try {
            send_to_gs(mConfig, logfile, "$DoRestart");
        } catch (const NetworkError& ex) {
            logfile << "Networking error when restarting GS: " << ex.what() << '\n';
        } catch (const GSError& ex) {
            logfile << "GS internal error when we asked it to restart, quite strange! Output:\n"
                << ex.what() << '\n';
        }
    }

    void Watchdog::worker() {
        // First, create a log file and report our existence.
        // Maybe std::endl will force the streams to flush, making the log up to date.
        // The performance overhead is negligible compared to 15 second polls.
        Logfile log(select_logfile("watchdog", mConfig["keep_logs"]));
        log << "Watchdog launched." << std::endl;
        // Then read the config db for localdata's name and password
        std::string dbname, passwd;
        try {
            dbname = mConfig["dbname"];
            passwd = mConfig["passwd"];
            if (dbname.empty())
                throw std::runtime_error("No dbname specified!");
            if (passwd.empty())
                throw std::runtime_error("No passwd specified!");
        } catch (const std::runtime_error& ex) {
            log << "Error when reading configuration for dbname or passwd!\n"
                << "ex.what(): " << ex.what() << '\n';
            return;
        }
        try {
            Connection local_data(dbname, passwd);
            // The last lesson processed, expressed as endtime.
            int last_proc = -1;
            // Mainloop here
            while (true) {
                // First check for stop requests
                if (mStopToken) {
                    log << "Requested stop.\n";
                    return;
                }
                // Then check if paused
                if (mPauseToken) {
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["watchdog_poll"]));
                    continue;
                }
                // Flush every loop.
                LogSection log_section(log);
                // Lessons that are nearing an end.
                std::vector<LessonInfo> near_ending;
                try {
                    near_ending = near_exits(local_data, mConfig["simul_limit"]);
                } catch (const SQLError& ex) {
                    log << "Encountering SQL error when calling near_exits()\n"
                        << "SQLError: " << ex.what() << '\n';
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["retry_wait"]));
                    continue;
                }
                if (near_ending.empty() || near_ending.front().endtime == last_proc) {
                    // Nothing to do, or already processed.
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["watchdog_poll"]));
                    continue;
                }
                auto& lesson = near_ending.front();
                try {
                    if (lesson.endtime - CurrentClock().get_ticks() >= mConfig["local_limit"]) {
                        log << "Start web-based processing lesson " << lesson.anpai << '\n';
                        simul_sign(local_data, lesson, log);
                    } else {
                        log << "Too impatient, resort to local sign in!\n";
                        local_sign(local_data, lesson, log);
                    }
                    // Now we have a good session
                    log << "process_lesson returned successfully.\n";
                    last_proc = lesson.endtime;
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["watchdog_poll"]));
                } catch (const NetworkError& ex) {
                    // Network error means that we can try again.
                    log << "NetworkError: " << ex.what() << '\n';
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["retry_wait"]));
                } catch (const std::logic_error& ex) {
                    log << "logic_error: " << ex.what() << '\n';
                    // Very bad config file, just skip it
                    last_proc = lesson.endtime;
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["retry_wait"]));
                } catch (const nlohmann::json::parse_error& ex) {
                    log << "Wrong format from server: " << ex.what() << '\n';
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["retry_wait"]));
                } catch (const SQLError& ex) {
                    log << "SQL Error: " << ex.what() << '\n';
                    std::this_thread::sleep_for(std::chrono::seconds(mConfig["retry_wait"]));
                }
            }
        } catch (const ErrorOpeningDatabase& ex) {
            log << "ErrorOpeningDatabase: " << ex.what() << '\n'
                << "Exiting because of failure.\n";
        } catch (const std::exception& ex) {
            log << "Unexpected std::exception: " << ex.what() << '\n';
            log << "Exiting!\n";
        }
    }
}