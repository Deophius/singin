#ifndef SPIRIT_SINGD_H
#define SPIRIT_SINGD_H
#include <thread>
#include <memory>
#include <atomic>
#include "dbman.h"
#include "logger.h"

// Spirit: The two daemon classes.
namespace Spirit {
    // Functionality from watchdog.pyw
    // To avoid data races, we prohibit setting watchdog config if there are lessons
    // that are about to end.
    class Watchdog {
    public:
        // config provides observer access to the config file.
        // The owner should be the main thread.
        Watchdog(const Spirit::Configuration& config);

        // Disable copying
        Watchdog(const Watchdog&) = delete;
        Watchdog& operator = (const Watchdog&) = delete;

        // Moving is not OK because of the reference.
        Watchdog(Watchdog&&) = delete;
        Watchdog& operator = (Watchdog&&) = delete;

        // Starts the daemon thread. Errors from standard library is transmitted up.
        // Should not block.
        void start();

        // Asks the watchdog to pause, returns immediately.
        void pause() noexcept;

        // Asks the watchdog to resume.
        void resume() noexcept;

        // During destruction, stops the daemon and joins the thread.
        virtual ~Watchdog() noexcept;
    private:
        // The smart pointer to the thread
        std::unique_ptr<std::thread> mThread{ nullptr };
        // The stop token, true means that a request for stop is in.
        std::atomic_bool mStopToken{ false };
        // The pause token, true means that watchdog should not process lessons,
        // but not exit, waiting for this to become false.
        std::atomic_bool mPauseToken{ false };
        // Shared access to the config.
        const Spirit::Configuration& mConfig;

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
        Singer(const Spirit::Configuration& config);

        // Disable copying
        Singer(const Singer&) = delete;
        Singer& operator = (const Singer&) = delete;

        // Moving is default.
        Singer(Singer&&) = default;
        Singer& operator = (Singer&&) = default;

        // As in the design, this daemon will occupy the "main thread", so
        // its function is called mainloop. Returns after receiving a quit
        // command
        void mainloop(Watchdog& watchdog, Logfile& logfile);
    private:
        // Ref to the configuration var.
        const Spirit::Configuration& mConfig;

        // A unique pointer to the local database. This is valid only after mainloop
        // has been called.
        std::unique_ptr<Connection> mLocalData;

        // Many handlers for the various commands.
        // They should take a json&, a logfile& and return another json as result.
        // For the structure of the request and responses, see dbserv/dbman.pyw.
        
        // sessid starts from 0
        nlohmann::json handle_rep_abs(const nlohmann::json& request, Logfile& log);

        nlohmann::json handle_wrt_rec(const nlohmann::json& request, Logfile& log);

        nlohmann::json handle_today(const nlohmann::json& request, Logfile& log);

        nlohmann::json handle_restart(const nlohmann::json& request, Logfile& log);

        nlohmann::json handle_notice(const nlohmann::json& request, Logfile& log);
        
        nlohmann::json handle_doggie(const nlohmann::json& request, Logfile& log, Watchdog& watchdog);
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
    nlohmann::json get_stu_new(
        const Configuration& config,
        const std::vector<Student>& absent,
        const LessonInfo& lesson,
        Logfile& logfile,
        int timeout = 5
    );

    // This function sends a message to the GS port.
    // Exception: NetworkError if errors related to socket occurs.
    // However, because this function connects to localhost, even if the port specified
    // has no sockets bound, there won't be an exception.
    void send_to_gs(const Configuration& config, Logfile& log, const std::string& msg);
}

#endif