add_executable(rw_test test/rw_test.cpp dbman.cpp)
target_link_libraries(rw_test sqlite3mc_x64)

add_executable(webio test/webio.cpp dbman.cpp logger.cpp dog_helper.cpp watchdog.cpp)
target_link_libraries(webio sqlite3mc_x64 C:/Windows/system32/ws2_32.dll)

add_executable(watchd test/watchd.cpp dbman.cpp logger.cpp dog_helper.cpp watchdog.cpp)
target_link_libraries(watchd sqlite3mc_x64 C:/Windows/system32/ws2_32.dll)