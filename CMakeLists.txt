cmake_minimum_required(VERSION 3.16)
project(scilexers)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(lexlib)
add_subdirectory(lexers)

