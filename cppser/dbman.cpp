#include "dbman.h"

namespace Spirit {
    Connection::Connection(const std::string& dbname, const std::string& passwd) {
        if (sqlite3_open(dbname.data(), &mDB))
            throw ErrorOpeningDatabase();
        sqlite3mc_config(mDB, "cipher", 5);
        sqlite3_key(mDB, passwd.data(), passwd.size());
    }

    Connection::~Connection() noexcept {
        sqlite3_close(mDB);
        mDB = nullptr;
    }

    Connection::Connection(Connection&& rhs) noexcept : mDB(rhs.mDB) {
        rhs.mDB = nullptr;
    }

    Connection& Connection::operator = (Connection&& rhs) noexcept {
        mDB = rhs.mDB;
        rhs.mDB = nullptr;
        return *this;
    }

    sqlite3* Connection::get() noexcept {
        return mDB;
    }

    Connection::operator sqlite3* () noexcept {
        return mDB;
    }

    Statement::Statement(Connection& conn, const std::string& sql) : mConn(conn) {
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

    Statement::~Statement() noexcept {
        sqlite3_finalize(mStatement);
        mStatement = nullptr;
    }

    sqlite3_stmt* Statement::get() noexcept {
        return mStatement;
    }

    std::optional<ResultRow> Statement::next() {
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

    bool Statement::is_end() noexcept {
        return mEnd;
    }

    ResultRow::ResultRow(const observer_ptr<Statement>& stmt) :
        mCnt(sqlite3_data_count(stmt->get())), mStmt(stmt)
    {}

    void Clock::fill(std::string& str, int n, int pos) {
        str[pos] = '0' + n / 10;
        str[pos + 1] = '0' + n % 10;
    }
    
    void Clock::set_date(std::string& str, const std::tm* ct) {
        // Year constituent
        const int year = ct->tm_year + 1900;
        str[0] = '0' + year / 1000;
        str[1] = '0' + year / 100 % 10;
        str[2] = '0' + year / 10 % 10;
        str[3] = '0' + year % 10;
        fill(str, ct->tm_mon + 1, 5);
        fill(str, ct->tm_mday, 8);
    }

    std::string Clock::get_timestr_template() {
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

    void Clock::set_hms(std::string& str, int ticks) {
        fill(str, ticks / 3600, 11);
        fill(str, ticks / 60 % 60, 14);
        fill(str, ticks % 60, 17);
    }

    Clock::Clock() {
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution dist(0, 9);
        mDigitGen = std::bind(dist, mt);
    }

    int Clock::str2time(const std::string& timestr) {
        if (timestr.length() != 8 || timestr[2] != ':' || timestr[5] != ':')
            throw std::logic_error("Time string format incorrect");
        auto toint = [&timestr](int pos) {
            return 10 * (timestr[pos] - '0') + (timestr[pos + 1] - '0');
        };
        return 3600 * toint(0) + 60 * toint(3) + toint(6);
    }

    std::string CurrentClock::operator() () {
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

    IncrementalClock::IncrementalClock() {
        const auto ct = []{
            auto t = std::time(nullptr);
            return std::localtime(&t);
        }();
        mTicks = ct->tm_hour * 3600 + ct->tm_min * 60 + ct->tm_sec;
    }

    std::string IncrementalClock::operator() () {
        std::string res = get_timestr_template();
        const auto ct = []{
            auto t = std::time(nullptr);
            return std::localtime(&t);
        }();
        set_date(res, ct);
        set_hms(res, mTicks);
        ++mTicks;
        return res;
    }

    RandomClock::RandomClock(int start, int end) {
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        // Cut 5 seconds off from each edge to simulate the real situation.
        std::uniform_int_distribution dist(start + 5, end - 5);
        mRandTime = std::bind(dist, mt);
    }

    std::string RandomClock::operator() () {
        std::string res = get_timestr_template();
        const auto ct = []{
            auto t = std::time(nullptr);
            return std::localtime(&t);
        }();
        set_date(res, ct);
        int rand_time = mRandTime();
        set_hms(res, rand_time);
        return res;
    }

    std::string DayBeginClock::operator() () {
        std::string res = "2030-03-02T00:00:00";
        const auto ct = []{
            auto t = std::time(nullptr);
            return std::localtime(&t);
        }();
        set_date(res, ct);
        return res;
    }

    std::vector<LessonInfo> get_lesson(Connection& conn) {
        const std::string query = "select ID, 考勤结束时间, 安排ID from \
        课程信息 where 考勤结束时间 > datetime('now', 'localtime', 'start of day') \
        and 考勤结束时间 < datetime('now', 'localtime', 'start of day', '1 day')";
        Statement stmt(conn, query);
        std::vector<LessonInfo> res;
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
            res.emplace_back();
            res.back().id = row->get<std::string>(0);
            res.back().endtime = Clock::str2time(row->get<std::string>(1).substr(11, 8));
            res.back().anpai = row->get<int>(2);
        }
        return res;
    }

    std::string get_machine(Connection& conn) {
        const std::string query = "select distinct TerminalID from Local_Visual_Publish";
        Statement stmt(conn, query);
        auto row = stmt.next();
        if (!row)
            // No suitable information found, return something impossible
            return "nonexistent";
        return row->get<std::string>(0);
    }

    std::vector<Student> report_absent(Connection& conn, const std::string& lesson_id) {
        using namespace std::literals;
        const std::string sql = "select 学生编号, 学生名称 from 上课考勤 where KeChengXinXi = '"s
            + lesson_id + "'and 打卡时间 is null and 是否排除考勤 = 0";
        Statement stmt(conn, sql);
        std::vector<Student> ans;
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
            ans.emplace_back();
            ans.back().id = row->get<std::string>(0);
            ans.back().name = row->get<std::string>(1);
        }
        return ans;
    }

    int write_record(Connection& conn, const std::string& lesson_id,
        std::vector<std::string> ids, Clock& clock
    ) {
        std::string sql;
        using namespace std::literals;
        {
            // Begin the transaction
            Statement stmt(conn, "begin transaction");
            stmt.next();
        }
        for (auto&& id : ids) {
            sql = "update 上课考勤 set 打卡时间='"s
                + clock()
                + "' where KeChengXinXi='"
                + lesson_id
                + "' and 学生编号='"
                + id
                + "'";
            try {
                Statement stmt(conn, sql);
                // Just to execute next.
                stmt.next();
            } catch (...) {
                // Some error has occured, return the error code.
                return ::sqlite3_errcode(conn);
            }
        }
        try {
            Statement stmt(conn, "end transaction");
            stmt.next();
        } catch (...) {
            return ::sqlite3_errcode(conn);
        }
        // Now return the OK code
        return SQLITE_OK;
    }

    // Some highly redundant code
    int write_record(Connection& conn, const std::string& lesson_id,
        const std::vector<Student>& stu, Clock& clock
    ) {
        std::string sql;
        using namespace std::literals;
        for (auto&& [unused, id] : stu) {
            sql = "update 上课考勤 set 打卡时间='"s
                + clock()
                + "' where KeChengXinXi='"
                + lesson_id
                + "' and 学生编号='"
                + id
                + "'";
            try {
                Statement stmt(conn, sql);
                // Just to execute next.
                stmt.next();
            } catch (...) {
                // Some error has occured, return the error code.
                return ::sqlite3_errcode(conn);
            }
        }
        // Now return the OK code
        return SQLITE_OK;
    }
}