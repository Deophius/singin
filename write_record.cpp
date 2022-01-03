#include "dbman.h"
#include <iostream>
#include <sstream>
#include <thread>

// Input: dk_curr
// whitespace-separated list of card numbers
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
    std::string card;
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    int dk_tot = 3, dk_curr;
    std::cin >> dk_curr;
    const auto [currmin, currmax] = Spirit::get_id_range(dk_tot, dk_curr, conn);
    while (std::cin) {
        std::cin >> card;
        // Do not explicitly check the card number to be valid,
        // because in the future the format might change.
        // Didn't cache oplock because this program needs to be defensive.
        std::ostringstream query;
        query << "update 上课考勤 set OptimisticLockField="
            << get_op_lock(currmin, currmax, conn)
            << " , 打卡时间='"
            << Spirit::get_current_time()
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
        // Wait for 0.7s to simulate the real scenario
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(700ms);
    }
}