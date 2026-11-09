cmake_minimum_required(VERSION 3.16)

project(my-zcat VERSION 1.2.0 LANGUAGES CXX)

find_package(gxrio REQUIRED)

add_executable(my-zcat ${PROJECT_SOURCE_DIR}/my-zcat.cpp)
target_link_libraries(my-zcat gxrio::gxrio)
