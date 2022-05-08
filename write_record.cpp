#include "dbman.h"
#include <iostream>
#include <memory>
#include <sstream>

// Input: dbname passwd dk_curr <mode>
// <names>
// Output: none

// Returns the OptimisticLockField value write_record should set
int get_op_lock(const std::string& id, Spirit::Connection& conn) {
    std::ostringstream query;
    query << "select distinct OptimisticLockField from 上课考勤 where KeChengXinXi='"
        << id
        << "' and 打卡时间 is not null";
    Spirit::Statement stmt(conn, query.str());
    // Return the first value seen, it doesn't matter anyway
    auto row = stmt.next();
    // Make a guess if row is empty
    if (!row)
        return 2;
    return row->get<int>(0);
}

int main() {
    std::string dbname, passwd;
    std::cin >> dbname >> passwd;
    Spirit::Connection conn(dbname, passwd);
    int dk_curr;
    std::cin >> dk_curr;
    const auto lessons = Spirit::get_lesson(conn);
    if (dk_curr < 0 || dk_curr >= (int)lessons.size()) {
        std::cerr << "dk_curr is out of range\n";
        return 1;
    }
    char mode;
    std::unique_ptr<Spirit::Clock> clock;
    std::cin >> mode;
    switch (mode) {
        case 'c': // current
            clock.reset(new Spirit::IncrementalClock());
            break;
        case 'r': {
            // random
            clock.reset(new Spirit::RandomClock(
                Spirit::RandomClock::str2time(lessons[dk_curr].start_time),
                Spirit::RandomClock::str2time(lessons[dk_curr].end_time)
            ));
            break;
        }
        default:
            std::cerr << "Unrecognized mode" << std::endl;
            return 1;
    }
    std::string name;
    const int op_lock = get_op_lock(lessons[dk_curr].id, conn);
    while (std::cin) {
        std::cin >> name;
        std::ostringstream query;
        query << "update 上课考勤 set OptimisticLockField="
            << op_lock
            << " , 打卡时间='"
            << (*clock)()
            << "' where KeChengXinXi='"
            << lessons[dk_curr].id
            << "' and 学生名称='"
            << name
            << "'";
        while (true) {
            try {
                Spirit::Statement stmt(conn, query.str());
                auto row = stmt.next();
                if (!row)
                    // Only upon successful operation can this statement be reached.
                    break;
            } catch (const Spirit::PrepareError& ex) {
                // Do nothing and try it again.
            }
        }
    }
}