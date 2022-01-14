#include "dbman.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

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

int main() {
    // Reads in the card number of people who need to sign in
    // and writes to the database.
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    int dk_tot = 3, dk_curr;
    std::cin >> dk_curr;
    const auto [currmin, currmax] = Spirit::get_id_range(dk_tot, dk_curr, conn);
    char mode;
    std::unique_ptr<Spirit::Clock> clock;
    std::cin >> mode;
    switch (mode) {
        case 'c': // current
            // no input
            clock.reset(new Spirit::CurrentClock());
            break;
        case 'r': {
            // random, read in 
            std::string start_time, end_time;
            std::cin >> start_time >> end_time;
            clock.reset(new Spirit::RandomClock(
                Spirit::RandomClock::str2time(start_time),
                Spirit::RandomClock::str2time(end_time)
            ));
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
            << (*clock)()
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
        // This feature is kept
        if (mode == 'c')
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }
}