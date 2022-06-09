#ifndef SPIRIT_SINGD_H
#define SPIRIT_SINGD_H
#include <thread>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include "dbman.h"

// Spirit: The two daemon classes.
namespace Spirit {
    // Functionality from watchdog.pyw
    class Watchdog {
    public:
        // The configurations will be read from config. It will not change on
        // the fly once the thread is up and running.
        // For missing configurations, throws std::value_error.
        // Kennel is a shared mutex for accessing spirit.db's kennel table.
        Watchdog(const std::shared_ptr<Connection>& config, std::shared_mutex& kennel);

        // Like the one above, but moves from the shared pointer.
        Watchdog(std::shared_ptr<Connection>&& config, std::shared_mutex& kennel);

        // Disable copying
        Watchdog(const Watchdog&) = delete;
        Watchdog& operator = (const Watchdog&) = delete;

        // Moving is default.
        Watchdog(Watchdog&&) = default;
        Watchdog& operator = (Watchdog&&) = default;

        // Starts the daemon thread. Errors from standard library is transmitted up.
        // Should not block.
        void start();

        // During destruction, stops the daemon and joins the thread.
        virtual ~Watchdog() noexcept;
    private:
        // The smart pointer to the thread
        std::unique_ptr<std::thread> mThread;
        // The stop token, true means that a request for stop is in.
        std::atomic_bool mStopToken;
        // Mutex protecting kennel
        std::shared_mutex& mKennelMutex;
        // Shared access to the config file.
        std::shared_ptr<Connection> mConfig;

        // The worker thread. The necessary data is passed in through *this.
        void worker();
    };

    // Impl of the singin server, from dbman.pyw
    class Singer {
    public:
        // Initializes this with a shared configuration file.
        Singer(const std::shared_ptr<Connection>& config, std::shared_mutex& kennel);

        // Like above, but moves from the pointer.
        Singer(std::shared_ptr<Connection>&& config, std::shared_mutex& kennel);

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
        // Pointer to config db.
        std::shared_ptr<Connection> mConfig;
        // Kennel mutex
        std::shared_mutex& mKennel;
    };
}

#endif