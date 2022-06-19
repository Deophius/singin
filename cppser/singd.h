#ifndef SPIRIT_SINGD_H
#define SPIRIT_SINGD_H
#include <thread>
#include <atomic>
#include <shared_mutex>
#include "dbman.h"
#include "logger.h"

// Spirit: The two daemon classes.
namespace Spirit {
    // Functionality from watchdog.pyw
    class Watchdog {
    public:
        // config provides observer access to the config file.
        // The owner should be the main thread. Every time we try to read
        // synchronously, we must first acquire the lock.
        Watchdog(const Spirit::Configuration& config, std::shared_mutex& mut);

        // Disable copying
        Watchdog(const Watchdog&) = delete;
        Watchdog& operator = (const Watchdog&) = delete;

        // Moving is not OK because of the reference.
        Watchdog(Watchdog&&) = delete;
        Watchdog& operator = (Watchdog&&) = delete;

        // Starts the daemon thread. Errors from standard library is transmitted up.
        // Should not block.
        void start();

        // During destruction, stops the daemon and joins the thread.
        // If the program is running normally, this should never be reached.
        virtual ~Watchdog() noexcept;
    private:
        // The smart pointer to the thread
        std::unique_ptr<std::thread> mThread{ nullptr };
        // The stop token, true means that a request for stop is in.
        std::atomic_bool mStopToken{ false };
        // Shared access to the config.
        const Spirit::Configuration& mConfig;
        // Mutex protecting the configuration, let's call it the kennel mutex.
        std::shared_mutex& mKennelMutex;

        // The worker thread. The necessary data is passed in through *this.
        void worker();

        // Process a lesson. Automatically checks whoever is absent, gets the leave info
        // and writes those who are absent and at school to database.
        // However, this does not check whether the lesson has been processed.
        // Exceptions: NetworkError, logic_error, nlohmann::json::parse_error
        void process_lesson(Connection& localdata, const LessonInfo& lesson, Logfile& logfile);
    };

    // Impl of the singin server, from dbman.pyw
    class Singer {
    public:
        // Initializes this with a shared configuration file and the lock.
        Singer(Spirit::Configuration& config, std::shared_mutex& mutex);

        // Disable copying
        Singer(const Singer&) = delete;
        Singer& operator = (const Singer&) = delete;

        // Moving is default.
        Singer(Singer&&) = default;
        Singer& operator = (Singer&&) = default;

        // As in the design, this daemon will occupy the "main thread", so
        // its function is called mainloop.
        // The user should create a Watchdog instance and start it before calling
        // mainloop and pass it in.
        [[noreturn]] void mainloop(Watchdog& watchdog);
    private:
        // Ref to the configuration var. Because modifications occur in this thread,
        // this is not const.
        Spirit::Configuration& mConfig;
        // The lock protecting mConfig.
        std::shared_mutex& mKennelMutex;
    };

    // Pull out the helper functions to facilitate testing.
    // Returns the list of lessons that will end DK in less than 3 minutes.
    std::vector<LessonInfo> near_exits(Connection& conn);

    // Error class for network errors
    struct NetworkError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // This function parses the data in the config. The ref parameters are output.
    // Throws logic_error if the URL is empty or doesn't contain a host name like 127.0.0.1
    void parse_url(const Configuration& config, std::string& host, std::string& url);

    // Executes the request to leave_info and returns the result body.
    // Throws std::runtime_error on any error.
    // This is a very lengthy operation, so you **must** use a future/promise mechanism to invoke it
    // in order to achieve reasonable timeout functionality (std::async wouldn't work because the future's
    // dtor waits in that case)
    // Because when this is executing in a different thread, the parent should be waiting,
    // so we dare pass log files around.
    nlohmann::json execute_request(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile
    );

    // A wrapper around execute request which handles the timeouts.
    // Timeout is expressed in seconds.
    // If the result is retrieved within time, returns the result.
    // Throws NetworkError on network related errors or time out.
    // logic_error if the URL in the config cannot be interpreted.
    // nlohmann::json::parse_error if the response from the server
    // cannot be parsed as JSON.
    nlohmann::json get_leave_info(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile,
        int timeout = 5
    );
}

#endif