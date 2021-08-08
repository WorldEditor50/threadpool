cmake_minimum_required(VERSION 3.5)

project(ThreadPool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#thread pool c++11
add_library(THREADPOOL_CPP11
    ${PROJECT_SOURCE_DIR}/threadpool_cpp11/threadpool.h
    ${PROJECT_SOURCE_DIR}/threadpool_cpp11/threadpool.cpp)
#nonlock thread pool
add_library(THREADPOOL_NONLOCK
    ${PROJECT_SOURCE_DIR}/NonLockThreadPool/threadpool.h
    ${PROJECT_SOURCE_DIR}/NonLockThreadPool/threadpool.cpp)
#thread pool linux
add_library(THREADPOOL_LINUX
    ${PROJECT_SOURCE_DIR}/threadpool_linux/threadpool.h
    ${PROJECT_SOURCE_DIR}/threadpool_linux/threadpool.cpp)
add_executable(ThreadPool main.cpp)
target_link_libraries(ThreadPool THREADPOOL_CPP11)
