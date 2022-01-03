#include "dbman.h"
#include <iostream>
#include <sstream>
#include <thread>

int main() {
    // Reads in the card number of people who need to sign in
    // and writes to the database.
    std::string card;
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    while (std::cin) {
        std::cin >> card;
        // Do not explicitly check the card number to be valid,
        // because in the future the format might change.
        std::ostringstream query;
        query << "update 上课考勤 set OptimisticLockField=2, 打卡时间='"
            << Spirit::get_current_time()
            << "' where OptimisticLockField=1 and 卡号='"
            << card
            << "'";
        Spirit::Statement stmt(conn, query.str());
        while (true) {
            auto row = stmt.next();
            if (!row)
                break;
        }
        // Wait for 0.5s to simulate the real scenario
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(700ms);
    }
}