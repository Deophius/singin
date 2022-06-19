#include "../singd.h"
#include <iostream>

int main() {
    using namespace Spirit;
    std::string dbname, passwd = "123", configstr;
    int anpai;
    std::cin >> dbname >> configstr;
    auto config = nlohmann::json::parse(configstr);
    Connection conn(dbname, passwd);
    auto lessons = get_lesson(conn);
    if (lessons.size() == 0) {
        std::cout << "Please check that there's at least one lesson in the DB!\n";
        return 1;
    }
    auto lesson = lessons.back();
    std::cout << "Lesson ANPAI is " << lesson.anpai << '\n';
    auto absent = report_absent(conn, lesson.id);
    std::cout << "There are " << absent.size() << " people absent.\n";
    Logfile output("output.txt");
    output << "Starting test session\n";
    try {
        auto result = get_leave_info(config, absent, lesson, output);
        std::cout << "For detailed trace, see output.txt.\n" << result.dump(4) << '\n';
    } catch (const NetworkError& ex) {
        std::cout << ex.what() << std::endl;
        return 1;
    }
}