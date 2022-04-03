#include <iostream>
#include <sstream>
#include "dbman.h"

// Input: dbname passwd <start timestr> <end timestr>
// Output: The body of the request to get the data for that session.
// Example output:
// {"ID":63190,"date":"2022-03-29T18:00:00","isloadfacedata":false,"faceversion":2}

std::string get_date(Spirit::Connection& conn) {
    const std::string query = "select date('now', 'localtime')";
    Spirit::Statement stmt(conn, query);
    return stmt.next()->get<std::string>(0);
}

int main() {
    std::string dbname, passwd, start, end;
    std::cin >> dbname >> passwd >> start >> end;
    Spirit::Connection conn(dbname, passwd);
    std::string date = get_date(conn);
    // The query stringstream
    std::ostringstream query;
    query << "select 上课开始时间, 安排ID from 课程信息 "
        << "where 考勤开始时间 == datetime('"
        << date
        << ' '
        << start
        << "') and 考勤结束时间 == datetime('"
        << date
        << ' '
        << end
        << "')";
    Spirit::Statement stmt(conn, query.str());
    // It should only yield one row.
    auto row = stmt.next();
    if (!row) {
        std::cerr << "Lesson not found!\n";
        return 1;
    }
    std::cout << "{\"ID\":"
        << row->get<int>(1)
        << ",\"date\":\""
        << date
        << "T"
        << row->get<std::string>(0).substr(11)
        << "\",\"isloadfacedata\":false,\"faceversion\":2}"
        << '\n';
}