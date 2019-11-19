cmake_minimum_required(VERSION 3.14)
project(OS C)

set(CMAKE_C_STANDARD 99)

add_executable(OS main.c experiment/webserver/exp3/webserver.c experiment/webserver/exp2/webserver.c experiment/PageReplace/main.c)