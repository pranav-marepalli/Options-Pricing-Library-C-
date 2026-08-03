cmake_minimum_required(VERSION 3.14)
project(options_lib CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(optionslib
    src/pricer.cpp
)
target_include_directories(optionslib PUBLIC include)

add_executable(options_tests tests/test_main.cpp)
target_link_libraries(options_tests PRIVATE optionslib)

add_executable(options_bench bench/bench.cpp)
target_link_libraries(options_bench PRIVATE optionslib)

find_package(Threads REQUIRED)
add_executable(options_api api/main.cpp)
target_include_directories(options_api PRIVATE third_party)
target_link_libraries(options_api PRIVATE optionslib Threads::Threads)
