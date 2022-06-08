#include "../dbman.h"
#include <iostream>
#include <fstream>
#include <chrono>

int main() {
    std::string dbname, lessonid;
    std::cin >> dbname >> lessonid;
    std::string passwd = "123";
    Spirit::Connection conn(dbname, passwd);
    auto absent = Spirit::report_absent(conn, lessonid);
    std::ofstream fout("output.txt", std::ios::out);
    for (auto&& [name, cardnum] : absent) {
        fout << name << ' ' << cardnum << '\n';
    }
    Spirit::IncrementalClock clock;
    auto begint = std::chrono::system_clock::now();
    auto ret = Spirit::write_record(conn, lessonid, absent, clock);
    auto endt = std::chrono::system_clock::now();
    fout << "After " << std::chrono::duration_cast<std::chrono::milliseconds>
        (endt - begint).count() << "ms, ";
    fout << "write record returned " << ret << '\n';
}