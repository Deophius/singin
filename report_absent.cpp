#include "dbman.h" 
#include <iostream>
#include <sstream>
#include <utility>

// Input: current DK num (start from 0)
// Output: the card ids of those who haven't DK

int main() {
    Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
    const int dk_tot = 3;
    int dk_curr;
    std::cin >> dk_curr;
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