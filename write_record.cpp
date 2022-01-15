#include "dbman.h"
#include <functional>
#include <iostream>
#include <sstream>

// Input: dk_curr <mode> [<start time> <end time>]
// <cardnums>
// ...
// <cardnum n> <time str n>
// Output: none

// [currmin, currmax): the range of ID for a DK session
// Returns the OptimisticLockField value write_record should set
int get_op_lock(int currmin, int currmax, Spirit::Connection& conn) {
    std::ostringstream query;
    query << "select distinct OptimisticLockField from 上课考勤 where "
        << currmin
        << " <= ID and ID < "
        << currmax
        << " and 打卡时间 is not null";
    Spirit::Statement stmt(conn, query.str());
    // Return the first value seen, it doesn't matter anyway
    auto row = stmt.next();
    // Make a guess if row is empty
    if (!row)
        return 2;
    return row->get<int>(0);
}

// Given a string of 'xx:xx:xx', returns the second it is in a day
// For example, '07:00:00' -> 3600 * 7
int str2time(const std::string& timestr) {
    if (timestr.length() != 8 || timestr[2] != ':' || timestr[5] != ':')
        throw std::logic_error("Time string format incorrect");
    auto toint = [&timestr](int pos) {
        return 10 * (timestr[pos] - '0') + (timestr[pos + 1] - '0');
    };
    return 3600 * toint(0) + 60 * toint(3) + toint(6);
}

class RandomTimeGetter {
private:
    // mGen: [0, 9]
    std::function<int()> mGen;
    // Used with start and end
    std::function<int()> mRandTime;
public:
    RandomTimeGetter(int start, int end) {
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution dist(0, 9);
        mGen = std::bind(dist, mt);
        std::uniform_int_distribution dist2(start + 5, end - 5);
        mRandTime = std::bind(dist2, mt);
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
        int rand_time = mRandTime();
        fill(rand_time / 3600, 11);
        fill(rand_time / 60 % 60, 14);
        fill(rand_time % 60, 17);
        return res;
    }
};

int main() {
    // Reads in the card number of people who need to sign in
    // and writes to the database.
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    int dk_tot = 3, dk_curr;
    std::cin >> dk_curr;
    const auto [currmin, currmax] = Spirit::get_id_range(dk_tot, dk_curr, conn);
    char mode;
    std::function<std::string()> get_time;
    std::cin >> mode;
    switch (mode) {
        case 'c': // current
            // no input
            get_time = Spirit::get_current_time;
            break;
        case 'r': {
            // random, read in 
            std::string start_time, end_time;
            std::cin >> start_time >> end_time;
            get_time = RandomTimeGetter(str2time(start_time), str2time(end_time));
            break;
        }
        default:
            std::cerr << "Unrecognized mode" << std::endl;
            return 1;
    }
    std::string card;
    while (std::cin) {
        std::cin >> card;
        // Do not explicitly check the card number to be valid,
        // because in the future the format might change.
        // Didn't cache oplock because this program needs to be defensive.
        std::ostringstream query;
        query << "update 上课考勤 set OptimisticLockField="
            << get_op_lock(currmin, currmax, conn)
            << " , 打卡时间='"
            << get_time()
            << "' where "
            << currmin
            << " <= ID and ID < "
            << currmax
            << " and 卡号='"
            << card
            << "'";
        Spirit::Statement stmt(conn, query.str());
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
        }
    }
}