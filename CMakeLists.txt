cmake_minimum_required(VERSION 3.02)

project(message_bus)

set(CMAKE_CXX_STANDARD 11)

add_executable(test_message_bus test.cpp message_bus.hpp)
