add_executable(rw_test test/rw_test.cpp dbman.cpp)
target_link_libraries(rw_test sqlite3mc_x64)