cmake_minimum_required(VERSION 3.17)
project(libfabric_example)

set(CMAKE_CXX_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(OFI REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE OFI::OFI)
