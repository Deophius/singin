cmake_minimum_required(VERSION 3.15)
project(singin VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BOOST_INCLUDE D:/boost_include)
include_directories(${BOOST_INCLUDE})

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(cppser)