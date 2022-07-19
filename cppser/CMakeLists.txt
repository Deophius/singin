set(SOURCES dbman.cpp logger.cpp dog_helper.cpp watchdog.cpp singer.cpp)
add_library(spirit SHARED ${SOURCES})
target_link_libraries(spirit C:/Windows/system32/ws2_32.dll sqlite3mc_x64)

add_executable(rw_test test/rw_test.cpp)
target_link_libraries(rw_test spirit)

add_executable(webio test/webio.cpp)
target_link_libraries(webio spirit)

add_executable(watchd test/watchd.cpp)
target_link_libraries(watchd spirit)

add_executable(song_test test/song_test.cpp)
target_link_libraries(song_test spirit)

add_executable(spiritd app.cpp)
target_link_libraries(spiritd spirit)