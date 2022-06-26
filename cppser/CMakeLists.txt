set(SOURCES dbman.cpp logger.cpp dog_helper.cpp watchdog.cpp singer.cpp)
add_library(spirit ${SOURCES})

add_executable(rw_test test/rw_test.cpp)
target_link_libraries(rw_test sqlite3mc_x64 spirit)

add_executable(webio test/webio.cpp)
target_link_libraries(webio sqlite3mc_x64 C:/Windows/system32/ws2_32.dll spirit)

add_executable(watchd test/watchd.cpp)
target_link_libraries(watchd sqlite3mc_x64 C:/Windows/system32/ws2_32.dll spirit)

add_executable(song_test test/song_test.cpp)
target_link_libraries(song_test sqlite3mc_x64 C:/Windows/system32/ws2_32.dll spirit)