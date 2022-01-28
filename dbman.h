#ifndef SPIRIT_DBMAN_H
#define SPIRIT_DBMAN_H
#include <sqlite3mc.h>

#include <chrono>
#include <experimental/memory>
#include <functional>
#include <optional>
#include <random>
#include <vector>

namespace Spirit {
    using namespace std::string_literals;
    using std::experimental::observer_ptr;

    // Cannot use std::string_view here because std::string_view might have a buffer
    // that doesn't end with \0
    const std::string dbname = "E:/program/singin/localData.db";
    const std::string passwd = "123";

    struct ErrorOpeningDatabase : public std::runtime_error {
        ErrorOpeningDatabase() : runtime_error("Error opening database!") {}
    };

    // Simple wrapper for a database
    class Connection {
    private:
        sqlite3* mDB = nullptr;
    public:
        // Opens a database
        explicit Connection(const std::string& dbname, const std::string& passwd) {
            if (sqlite3_open(dbname.data(), &mDB))
                throw ErrorOpeningDatabase();
            sqlite3mc_config(mDB, "cipher", 5);
            sqlite3_key(mDB, passwd.data(), passwd.size());
        }

        virtual ~Connection() noexcept {
            sqlite3_close(mDB);
            mDB = nullptr;
        }

        // Must disable copy
        Connection(const Connection&) = delete;
        Connection& operator = (const Connection&) = delete;

        // Move constructors
        Connection(Connection&& rhs) noexcept : mDB(rhs.mDB) {
            rhs.mDB = nullptr;
        }

        Connection& operator = (Connection&& rhs) noexcept {
            mDB = rhs.mDB;
            rhs.mDB = nullptr;
            return *this;
        }

        sqlite3* get() noexcept {
            return mDB;
        }

        operator sqlite3* () noexcept {
            return mDB;
        }
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
        Statement(Connection& conn, const std::string& sql) : mConn(conn) {
            if (!conn.get())
                throw ConnectionInvalid();
            // The pointer to the "tail" in the function call
            const char* tail = nullptr;
            const int rc = sqlite3_prepare_v2(mConn, sql.data(), sql.size(), &mStatement, &tail);
            if (not tail)
                throw MultipleSQLStatements();
            if (rc != SQLITE_OK)
                throw PrepareError(sqlite3_errmsg(mConn));
        }

        // Destructor calls sqlite3_finalize
        virtual ~Statement() noexcept {
            sqlite3_finalize(mStatement);
            mStatement = nullptr;
        }

        // Disable copying
        Statement(const Statement&) = delete;
        Statement& operator= (const Statement&) = delete;

        // Move constructor is OK
        Statement(Statement&& rhs) noexcept : mConn(rhs.mConn), mStatement(rhs.mStatement) {
            rhs.mStatement = nullptr;
        }

        // Move assignment is not allowed
        Statement& operator = (Statement&& rhs) noexcept = delete;

        sqlite3_stmt* get() noexcept {
            return mStatement;
        }

        std::optional<ResultRow> next() {
            if (mEnd)
                return std::nullopt;
            const int rc = sqlite3_step(mStatement);
            if (rc == SQLITE_ROW)
                return ResultRow(observer_ptr<Statement>(this));
            else if (rc == SQLITE_DONE) {
                mEnd = true;
                return std::nullopt;
            }
            else
                throw std::runtime_error(sqlite3_errmsg(mConn.get()));
        }

        bool is_end() noexcept {
            return mEnd;
        }
    };

    ResultRow::ResultRow(const observer_ptr<Statement>& stmt) :
        mCnt(sqlite3_data_count(stmt->get())), mStmt(stmt)
    {}

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
    protected:
        // These functions are to be used in subclasses. Make sure str is a 
        // string with the correct format.

        // Fills str[pos] and str[pos + 1] with two-digit number n
        // If n < 10, a prefix 0 is added, such as 08
        void fill(std::string& str, int n, int pos) {
            str[pos] = '0' + n / 10;
            str[pos + 1] = '0' + n % 10;
        }

        // ct is the current time
        void set_date(std::string& str, const std::tm* ct) {
            // Year constituent
            const int year = ct->tm_year + 1900;
            str[0] = '0' + year / 1000;
            str[1] = '0' + year / 100 % 10;
            str[2] = '0' + year / 10 % 10;
            str[3] = '0' + year % 10;
            fill(str, ct->tm_mon + 1, 5);
            fill(str, ct->tm_mday, 8);
        }

        // Returns a time str template with the millisecond part set with random digits
        std::string get_timestr_template() {
            std::string str;
            do {
                str = "0123-56-89 12:45:78.0123456";
                for (std::size_t pos = 20; pos <= 26; pos++)
                    str[pos] = mDigitGen() + '0';
                while (str.back() == '0')
                    str.pop_back();
            } while (str.back() == '.'); // Turn back if only . is left
            return str;
        }

    public:
        Clock() {
            std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
            std::uniform_int_distribution dist(0, 9);
            mDigitGen = std::bind(dist, mt);
        }

        virtual ~Clock() noexcept = default;

        // This is the interface subclasses need to implement
        virtual std::string operator () () = 0;

        // Given a string of 'xx:xx:xx', returns the second it is in a day
        // For example, '07:00:00' -> 3600 * 7
        static int str2time(const std::string& timestr) {
            if (timestr.length() != 8 || timestr[2] != ':' || timestr[5] != ':')
                throw std::logic_error("Time string format incorrect");
            auto toint = [&timestr](int pos) {
                return 10 * (timestr[pos] - '0') + (timestr[pos + 1] - '0');
            };
            return 3600 * toint(0) + 60 * toint(3) + toint(6);
        }
    };

    // This clock returns the current time in the format
    // 2021-12-31 21:50:34.1234567
    class CurrentClock : public Clock {
    public:
        [[nodiscard]] virtual std::string operator() () override {
            std::string res = get_timestr_template();
            const auto ct = []{
                auto t = std::time(nullptr);
                return std::localtime(&t);
            }();
            set_date(res, ct);
            fill(res, ct->tm_hour, 11);
            fill(res, ct->tm_min, 14);
            fill(res, ct->tm_sec, 17);
            return res;
        }

        virtual ~CurrentClock() = default;
    };

    class IncrementalClock : public Clock {
    private:
        // ticks since start of day
        int mTicks;
    public:
        IncrementalClock() {
            const auto ct = []{
                auto t = std::time(nullptr);
                return std::localtime(&t);
            }();
            mTicks = ct->tm_hour * 3600 + ct->tm_min * 60 + ct->tm_sec;
        }

        virtual std::string operator() () override {
            std::string res = get_timestr_template();
            const auto ct = []{
                auto t = std::time(nullptr);
                return std::localtime(&t);
            }();
            set_date(res, ct);
            fill(res, mTicks / 3600, 11);
            fill(res, mTicks / 60 % 60, 14);
            fill(res, mTicks % 60, 17);
            ++mTicks;
            return res;
        }
    };

    // This clock returns a random time in a specified region
    class RandomClock : public Clock {
    private:
        std::function<int()> mRandTime;
    public:
        // The int argument can be obtained by calling str2time()
        RandomClock(int start, int end) {
            std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
            // Cut 5 seconds off from each edge to simulate the real situation.
            std::uniform_int_distribution dist(start + 5, end - 5);
            mRandTime = std::bind(dist, mt);
        }

        virtual ~RandomClock() noexcept = default;

        virtual std::string operator() () override {
            std::string res = get_timestr_template();
            const auto ct = []{
                auto t = std::time(nullptr);
                return std::localtime(&t);
            }();
            set_date(res, ct);
            int rand_time = mRandTime();
            fill(res, rand_time / 3600, 11);
            fill(res, rand_time / 60 % 60, 14);
            fill(res, rand_time % 60, 17);
            return res;
        }
    };

    // This structure represents a Lesson with all its data included
    struct LessonInfo {
        // Time to start and end punching card
        std::string start_time, end_time;
        // Lesson ID as a string obtained from DB
        std::string id;
    };

    // Returns a vector of today's lessons' information
    // start_time and end_time has format xx:xx:xx
    std::vector<LessonInfo> get_lesson(Connection& conn) {
        // Expects that the database now contains the required info
        const std::string query = "select ID, 考勤开始时间, 考勤结束时间 from \
        课程信息 where CreationTime > datetime('now', 'localtime', 'start of day')";
        Statement stmt(conn, query);
        std::vector<LessonInfo> res;
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
            res.emplace_back();
            res.back().id = row->get<std::string>(0);
            res.back().start_time = row->get<std::string>(1).substr(11, 8);
            res.back().end_time = row->get<std::string>(2).substr(11, 8);
        }
        return res;
    }

    // Returns the machine's ID
    std::string get_machine(Connection& conn) {
        const std::string query = "select distinct TerminalID from Local_Visual_Publish";
        Statement stmt(conn, query);
        auto row = stmt.next();
        if (!row)
            // No suitable information found, return something impossible
            return "nonexistent";
        return row->get<std::string>(0);
    }
}

#endif