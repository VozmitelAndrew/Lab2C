cmake_minimum_required(VERSION 3.28)
project(Labka2 C)

set(CMAKE_C_STANDARD 23)

add_executable(Labka2 src/main.c
        src/HTTP_Server.c)
