#include "dbman.h" 
#include <iostream>
#include <sstream>
#include <utility>

// Input: how many DKs there are a day (currently 3), current DK num (start from 0)
// Output: the card ids of those who haven't DK

std::pair<int, int> read() {
    int a, b;
    std::cin >> a >> b;
    return { a, b };
}

int main() {
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    const auto [dk_tot, dk_curr] = read();
    // The id for this DK is [currmin, currmax)
    const auto [currmin, currmax] = Spirit::get_id_range(dk_tot, dk_curr, conn);
    std::ostringstream query;
    query << "select 卡号 from 上课考勤 where " << currmin << " <= ID and ID < "
        << currmax << " and 打卡时间 is null";
    Spirit::Statement stmt(conn, query.str());
    while (true) {
        auto row = stmt.next();
        if (!row)
            break;
        std::cout << row->get<std::string>(0) << '\n';
    }
}