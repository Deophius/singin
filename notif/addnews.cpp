#include <iostream>
#include <string_view>
#include <sstream>
#include "dbman.h"

// Input: <dbname> <passwd>
// Then for each notification, there are four strings:
// <type> <start> <end> <content>
// <type>: One of SHADOWLANTERN, POSTER
// <start>, <end>: in format 2020-04-20T10:00:02
// <content>: A string that contains no whitespace
// Output: for each successful write, prints out a 1
// Once a failure occurs, the program terminates.

int get_sysid(Spirit::Connection& conn) {
    // Gets the maximum system id that we can use
    Spirit::Statement stmt(conn,"select max(SysID) from LocalTaskPlan where \
        datetime('now', 'localtime') > ExecEndDateTime");
    return stmt.next()->get<int>(0);
}

std::string get_sql(
    std::string_view type,
    std::string_view start,
    std::string_view end,
    std::string_view target,
    int level,
    int sysid
) {
    std::ostringstream ss;
    ss << "update LocalTaskPlan set AllowParallel = 1, InfoLevel="
        << level
        << ", ExecuteType='"
        << type
        << "', Target='"
        << target
        << "', ExecStartDateTime='"
        << start
        << "', ExecEndDateTime='"
        << end
        << "' where SysID = "
        << sysid;
    return ss.str();
}

// Returns true if OK, false if not.
bool handle(Spirit::Connection& conn) {
    std::string type, start, end, cont;
    int level = 0;
    std::cin >> type >> start >> end >> cont;
    start[10] = ' ';
    end[10] = ' ';
    if (type == "POSTER")
        level = 100;
    else if (type == "SHADOWLANTERN")
        level = 1;
    else {
        std::cerr << "Unknown type: " << type << '\n';
        return false;
    }
    Spirit::Statement stmt(conn, get_sql(type, start, end, cont, level, get_sysid(conn)));
    // Exhaust the statement and that's OK.
    while (true) {
        auto row = stmt.next();
        if (!row)
            break;
    }
    return true;
}

int main() {
    std::string dbname, passwd;
    std::cin >> dbname >> passwd;
    Spirit::Connection conn(dbname, passwd);
    while (std::cin) {
        if (handle(conn))
            std::cout << '1';
        else {
            std::cout << '\n';
            return 1;
        }
    }
    std::cout << '\n';
}