cmake_minimum_required(VERSION 2.8)
project(final)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

set (LIBS "-pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fpermissive")

add_executable(${PROJECT_NAME} main.cpp get_opt.h)
target_link_libraries(${PROJECT_NAME} ${LIBS} )