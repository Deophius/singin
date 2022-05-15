#include "dbman.h"
#include <iostream>

// Input: <dbname> <passwd>
// Output: The number of sessions that is currently active.
// If not during a card session, output is 0. Otherwise, it's 1.

int main() {
    std::string passwd, dbname;
    std::cin >> dbname >> passwd;
    try {
        Spirit::Connection conn(dbname, passwd);
        Spirit::Statement stmt(conn, "select count(OID) from 课程信息 where \
            datetime('now', 'localtime') >= 考勤开始时间 \
            and 考勤结束时间 >= datetime('now', 'localtime');"
        );
        auto row = stmt.next();
        std::cout << row->get<int>(0) << std::endl;
    } catch (...) {
        std::cerr << "in_signin: UKE" << std::endl;
        return 1;
    }
}