#include "dbman.h"
#include <iostream>

int main() {
    using namespace Spirit;
    Connection conn(dbname, passwd);
    std::cout << get_machine(conn) << std::endl;
    auto lessons = get_lesson(conn);
    for (const auto& [start, end, id] : lessons) {
        std::cout << id << ' ' << start << ' ' << end << '\n';
    }
}