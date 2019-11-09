cmake_minimum_required(VERSION 2.8)
project(final)
find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD 11)

add_executable(${PROJECT_NAME} main.cpp get_opt.h)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} )