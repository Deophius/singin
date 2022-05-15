#include "dbman.h"
#include <iostream>

// Input: <dbpath> <passwd>
// Output: The machine ID, such as NJ301

int main() {
    std::string dbname, passwd;
    std::cin >> dbname >> passwd;
    try {
        Spirit::Connection conn(dbname, passwd);
        Spirit::Statement stmt(conn, "select TerminalID from Local_Visual_Publish");
        auto row = stmt.next();
        std::cout << row->get<std::string>(0) << std::endl;
    } catch (...) {
        std::cerr << "Something failed with the database stuff!" << std::endl;
        return 1;
    }
}