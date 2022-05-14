#include "dbman.h" 
#include <iostream>
#include <sstream>

// Input: dbname passwd
// current DK num (start from 0)
// Output: the names of those who haven't DK, separated by newlines

int main() {
    std::string dbname, passwd;
    std::cin >> dbname >> passwd;
    Spirit::Connection conn(dbname, passwd);
    auto lessons = Spirit::get_lesson(conn);
    int dk_curr;
    std::cin >> dk_curr;
    if (dk_curr < 0 || dk_curr >= (int)lessons.size()) {
        std::cerr << "dk_curr is out of range\n";
        return 1;
    }
    std::ostringstream query;
    query << "select 学生名称 from 上课考勤 where KeChengXinXi = '"
        << lessons[dk_curr].id
        << "' and 打卡时间 is null";
    Spirit::Statement stmt(conn, query.str());
    while (true) {
        auto row = stmt.next();
        if (!row)
            break;
        std::cout << row->get<std::string>(0) << '\n';
    }
}