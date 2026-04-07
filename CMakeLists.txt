cmake_minimum_required(VERSION 3.10)
project(badapple)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

file(GLOB SRC_FILES src/*.cpp)

add_executable(badapple ${SRC_FILES})

target_link_libraries(badapple ${OpenCV_LIBS})