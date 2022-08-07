set(SOURCES dbman.cpp logger.cpp dog_helper.cpp watchdog.cpp singer.cpp)
add_library(spirit SHARED ${SOURCES} libspirit.rc)
target_link_libraries(spirit C:/Windows/system32/ws2_32.dll sqlite3mc_x64)

file(COPY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/spirit.ico ${CMAKE_CURRENT_BINARY_DIR}/spirit.ico)
file(COPY_FILE ${CMAKE_SOURCE_DIR}/sqlite3mc_x64.dll ${CMAKE_CURRENT_BINARY_DIR}/sqlite3mc_x64.dll)

add_executable(rw_test test/rw_test.cpp)
target_link_libraries(rw_test spirit)

add_executable(webio test/webio.cpp)
target_link_libraries(webio spirit)

add_executable(watchd test/watchd.cpp)
target_link_libraries(watchd spirit)

add_executable(spiritd WIN32 app.cpp spiritd.rc)
target_link_libraries(spiritd spirit)