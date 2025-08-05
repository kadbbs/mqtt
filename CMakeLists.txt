cmake_minimum_required(VERSION 3.10.0)
project(sen_mqtt VERSION 0.1.0 LANGUAGES C CXX)

add_executable(sen_mqtt main.cpp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)