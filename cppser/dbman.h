#ifndef SPIRIT_DBMAN_H
#define SPIRIT_DBMAN_H
#include <sqlite3mc.h>

#include <chrono>
#include <experimental/memory>
#include <functional>
#include <optional>
#include <random>
#include <vector>
#include <nlohmann/json.hpp>

namespace Spirit {
    using namespace std::string_literals;
    using std::experimental::observer_ptr;

    // Directly "borrow" the config structure.
    using Configuration = nlohmann::json;

    struct ErrorOpeningDatabase : public std::runtime_error {
        ErrorOpeningDatabase() : runtime_error("Error opening database!") {}
    };

    // Simple wrapper for a database
    class Connection {
    private:
        sqlite3* mDB = nullptr;
    public:
        // Opens a database
        explicit Connection(const std::string& dbname, const std::string& passwd);

        virtual ~Connection() noexcept;

        // Must disable copy
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        // Move constructors
        Connection(Connection&& rhs) noexcept;

        Connection& operator = (Connection&& rhs) noexcept;

        sqlite3* get() noexcept;

        operator sqlite3* () noexcept;
    };

    struct ConnectionInvalid : public std::runtime_error {
        ConnectionInvalid() : runtime_error("The connection is not valid!")
        {}
    };

    struct MultipleSQLStatements : public std::logic_error {
        MultipleSQLStatements() : logic_error("The string contains multiple SQL statements.")
        {}
    };

    struct PrepareError : public std::runtime_error {
        PrepareError(const char* msg) : runtime_error("Error preparing statement: "s + msg)
        {}
    };

    struct DiffConnectionError : public std::logic_error {
        DiffConnectionError() : logic_error("The two Statements have different bound connections.")
        {}
    };

    // Forward decl
    class Statement;

    // A row in the SQL result
    class ResultRow {
    private:
        const int mCnt;
        observer_ptr<Statement> mStmt;
    public:
        // Defined later
        ResultRow(const observer_ptr<Statement>& mStmt);

        template <typename T>
        T get(int col);
    };

    // This class represents a query
    class Statement {
    private:
        Connection& mConn;
        sqlite3_stmt* mStatement = nullptr;
        bool mEnd = false;
    public:
        // Constructor calls sqlite3_prepare_v2
        // Expects that conn is a valid connection, 
        Statement(Connection& conn, const std::string& sql);

        // Destructor calls sqlite3_finalize
        virtual ~Statement() noexcept;

        // Disable copying
        Statement(const Statement&) = delete;
        Statement& operator= (const Statement&) = delete;

        // Move constructor is OK
        Statement(Statement&& rhs) noexcept = default;
        Statement& operator = (Statement&& rhs) noexcept = default;

        sqlite3_stmt* get() noexcept;

        std::optional<ResultRow> next();

        bool is_end() noexcept;
    };

    template <typename T>
    T ResultRow::get(int col) {
        // Index starts from 0
        if (col < 0 || col >= mCnt)
            throw std::out_of_range("col is out of range!");
        if constexpr (std::is_same_v<T, int>)
            return sqlite3_column_int(mStmt->get(), col);
        else if constexpr (std::is_same_v<T, long long>)
            return sqlite3_column_int64(mStmt->get(), col);
        else if constexpr (std::is_same_v<T, std::string>)
            return reinterpret_cast<const char*>(sqlite3_column_text(mStmt->get(), col));
        else
            throw std::logic_error("Type not supported!");
    }

    // Base class for a clock (to get the time string for the db)
    class Clock {
    private:
        std::function<int()> mDigitGen;
    public:
        // These functions are to be used in subclasses. Make sure str is a 
        // string with the correct format.

        // Fills str[pos] and str[pos + 1] with two-digit number n
        // If n < 10, a prefix 0 is added, such as 08
        static void fill(std::string& str, int n, int pos);

        // ct is the current time
        static void set_date(std::string& str, const std::tm* ct);

        // Returns a time str template with the millisecond part set with random digits
        std::string get_timestr_template();

        // Sets the hms part according to ticks
        static void set_hms(std::string& str, int ticks);

        Clock();

        virtual ~Clock() noexcept = default;

        // This is the interface subclasses need to implement
        virtual std::string operator () () = 0;

        // Given a string of 'xx:xx:xx', returns the second it is in a day
        // For example, '07:00:00' -> 3600 * 7
        static int str2time(const std::string& timestr);

        // Reverse of above.
        // Format: 07:20:00
        static std::string time2str(int ticks);
    };

    // This clock returns the current time in the format
    // 2021-12-31 21:50:34.1234567
    class CurrentClock : public Clock {
    public:
        [[nodiscard]] virtual std::string operator() () override;

        virtual ~CurrentClock() = default;
    };

    class IncrementalClock : public Clock {
    private:
        // ticks since start of day
        int mTicks;
    public:
        IncrementalClock();

        virtual std::string operator() () override;
    };

    // This clock returns a random time in a specified region
    class RandomClock : public Clock {
    private:
        std::function<int()> mRandTime;
    public:
        // The int argument can be obtained by calling str2time()
        RandomClock(int start, int end);

        virtual ~RandomClock() noexcept = default;

        virtual std::string operator() () override;
    };

    // This clock returns the timestr for a particular time, with the T character in place.
    // Intended for API manipulation, not for sign in purposes!
    class GivenTimeClock {
    public:
        // The time is given as the tick.
        [[nodiscard]] std::string operator() (int ticks);
    };

    // This structure represents a Lesson with all its data included
    struct LessonInfo {
        // Endtime of the session.
        int endtime;
        // Lesson ID as a string obtained from DB
        std::string id;
        // Anpai id, required for accessing the get_leave_info API.
        int anpai;
    };

    // Returns a vector of today's lessons' information
    // Expects that the database now contains the required info
    std::vector<LessonInfo> get_lesson(Connection& conn);

    // Returns the machine's ID
    std::string get_machine(Connection& conn);

    // This represents a student, with his or her name and card number.
    struct Student {
        // name: UTF-8 encoded string.
        // id: student's id
        std::string name, id;
    };

    // Gets a vector of Students who are still absent.
    std::vector<Student> report_absent(Connection& conn, const std::string& lesson_id);

    // Writes records to the database using the given clock for the given card numbers
    // for the given lesson. (Whew)
    // If access to the database is interrupted, will retry.
    // This is a little slow, so you can use async() to launch it.
    // Returns the SQLite error code.
    int write_record(Connection& conn, const std::string& lesson_id,
        const std::vector<std::string>& cards, Clock& clock);

    // Actually the same as above, just a convenience function.
    int write_record(Connection& conn, const std::string& lesson_id,
        const std::vector<Student>& stu, Clock& clock);
}

#endif