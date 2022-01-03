#include "dbman.h" 
#include <iostream>

int main() {
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    const std::string query_str = u8"select 卡号 from 上课考勤 \
    where OptimisticLockField = 1";
    Spirit::Statement stmt(conn, query_str);
    while (true) {
        auto row = stmt.next();
        if (!row)
            break;
        std::cout << row->get<std::string>(0) << '\n';
    }
}