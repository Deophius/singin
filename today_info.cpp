#include "dbman.h"
#include <iostream>

// Input: none
// Output:
// <machineID>
// <start timestr1> <end timestr1>
// ...
// <start timestr n> <end timestr n>
// Example Output:
// NJ303
// 06:20:00 07:20:00
// 17:15:00 17:55:00
// 20:05:00 20:20:00
// 
// Note: if there are DK information today, return value is 0 and data in stdout
// otherwise, returns 1 (But machine id will still be in stdout)

int main() {
	using namespace std::chrono_literals;
	Spirit::Connection conn(Spirit::dbname, Spirit::passwd);
	std::cout << Spirit::get_machine(conn) << '\n';
	auto lessons = Spirit::get_lesson(conn);
	if (lessons.size() == 0) {
		std::cerr << "No DK information" << std::endl;
		return 1;
	}
	for (const auto& [start, end, id] : Spirit::get_lesson(conn))
		std::cout << start << ' ' << end << '\n';
}