#include "singd.h"
#include <algorithm>

namespace Spirit {
    // Chores come first.
    Watchdog::Watchdog(const Spirit::Configuration& config, std::shared_mutex& kennel) :
        mConfig(config), mKennelMutex(kennel)
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

    Singer::Singer(Spirit::Configuration& config, std::shared_mutex& mut) :
        mConfig(config), mKennelMutex(mut)
    {}

    void Watchdog::process_lesson(Connection& conn, const LessonInfo& lesson, Logfile& logfile) {
        auto absent = report_absent(conn, lesson.id);
        // The JSON result from server
        auto left = get_leave_info(mConfig, absent, lesson, logfile);
        std::vector<Student> need_card;
        need_card.reserve(absent.size());
        for (auto&& student : absent) {
            // true if should be included in need_card
            bool flag = true;
            for (auto&& j : left["result"])
                if (student.id == j.get<std::string>()) {
                    flag = false;
                    break;
                }
            if (flag)
                need_card.push_back(std::move(student));
        }
        IncrementalClock clock;
        write_record(conn, lesson.id, need_card, clock);
    }

    void Watchdog::worker() {
        // First, create a log file and report our existence.
        // Maybe std::endl will force the streams to flush, making the log up to date.
        // The performance overhead is negligible compared to 15 second polls.
        Logfile log("watchdog.log");
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
                // Flush every loop.
                LogSection log_section(log);
                // Lessons that are nearing an end.
                auto near_ending = near_exits(local_data);
                if (near_ending.empty() || near_ending.front().endtime == last_proc) {
                    // Nothing to do, or already processed.
                    std::this_thread::sleep_for(std::chrono::seconds(15));
                    continue;
                }
                auto& lesson = near_ending.front();
                // True if processing should proceed.
                bool flag = false;
                for (auto&& i : mConfig["watchdog"]) {
                    if (i.get<int>() == lesson.endtime) {
                        flag = true;
                        break;
                    }
                }
                if (!flag) {
                    log << "Lesson " << lesson.anpai << " is not in config.\n";
                    std::this_thread::sleep_for(std::chrono::seconds(15));
                    continue;
                }
                try {
                    log << "Start processing lesson " << lesson.anpai << '\n';
                    process_lesson(local_data, lesson, log);
                    // Now we have a good session
                    log << "process_lesson returned without error.\n";
                    last_proc = lesson.endtime;
                } catch (const NetworkError& ex) {
                    // Network error means that we can try again.
                    log << "NetworkError: " << ex.what() << '\n';
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } catch (const std::logic_error& ex) {
                    log << "logic_error: " << ex.what() << '\n';
                    // Very bad config file, just skip it
                    last_proc = lesson.endtime;
                } catch (const nlohmann::json::parse_error& ex) {
                    log << "Wrong format from server: " << ex.what() << '\n';
                }
            }
        } catch (const ErrorOpeningDatabase& ex) {
            log << "ErrorOpeningDatabase: " << ex.what() << '\n'
                << "Exiting because of failure.\n";
        }
    }
}