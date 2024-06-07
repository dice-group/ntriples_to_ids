cmake_minimum_required(VERSION 3.28)
project(ntriples_to_ids)

set(CMAKE_CXX_STANDARD 20)

add_executable(ntriples_to_ids main.cpp)

target_include_directories(ntriples_to_ids PUBLIC include)
