#ifndef SPIRIT_SINGD_H
#define SPIRIT_SINGD_H
#include <thread>
#include <atomic>
#include <shared_mutex>
#include "dbman.h"

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
}

#endif