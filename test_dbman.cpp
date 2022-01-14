#include "dbman.h"
#include <iostream>

int main() {
    using namespace Spirit;
    Connection conn(dbname, passwd);
    const std::string query_str = u8"select ID,学生名称,打卡时间,CreationTime from 上课考勤 \
    where 打卡时间 > datetime('2021-12-31 21:00:00');";
    Statement stmt(conn, query_str);
    while (true) {
        auto row = stmt.next();
        if (!row)
            break;
        else {
            std::cout << row->get<int>(0) << " " << row->get<std::string>(1) << std::endl;
        }
    }
}