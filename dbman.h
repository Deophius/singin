#ifndef SPIRIT_DBMAN_H
#define SPIRIT_DBMAN_H
#include <sqlite3mc.h>

#include <chrono>
#include <experimental/memory>
#include <functional>
#include <optional>
#include <random>

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

        [[nodiscard]] sqlite3_stmt* get() noexcept {
            return mStatement;
        }

        [[nodiscard]] std::optional<ResultRow> next() {
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
    [[nodiscard]] T ResultRow::get(int col) {
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

    // This function returns the current time in the format
    // 2021-12-31 21:50:34.1234567
    class CurrentTimeGetter {
    private:
        std::function<int()> mGen;
    public:
        CurrentTimeGetter() {
            std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
            std::uniform_int_distribution dist(0, 9);
            mGen = std::bind(dist, mt);
        }

        [[nodiscard]] std::string operator() () {
            std::string res;
            // Let's do the random part first
            do {
                res = "0123-56-89 12:45:78.0123456";
                for (std::size_t pos = 20; pos <= 26; pos++)
                    res[pos] = mGen() + '0';
                while (res.back() == '0')
                    res.pop_back();
            } while (res.back() == '.'); // Turn back if only . is left
            const auto ct = []{
                auto t = std::time(nullptr);
                return std::localtime(&t);
            }();
            // Year constituent
            const int year = ct->tm_year + 1900;
            res[0] = '0' + year / 1000;
            res[1] = '0' + year / 100 % 10;
            res[2] = '0' + year / 10 % 10;
            res[3] = '0' + year % 10;
            auto fill = [&res](int num, int pos) {
                res[pos] = '0' + num / 10;
                res[pos + 1] = '0' + num % 10;
            };
            // Fill in month, date, hms
            fill(ct->tm_mon + 1, 5);
            fill(ct->tm_mday, 8);
            fill(ct->tm_hour, 11);
            fill(ct->tm_min, 14);
            fill(ct->tm_sec, 17);
            return res;
        }
    };

    CurrentTimeGetter get_current_time;
}

#endif