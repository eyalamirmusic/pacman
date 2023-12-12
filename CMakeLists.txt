cmake_minimum_required(VERSION 3.27)
project(PacMan)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 2.5 COMPONENTS graphics audio REQUIRED)

file(GLOB_RECURSE src_files ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
add_executable(PacMan ${src_files})

target_link_libraries(PacMan PRIVATE sfml-graphics sfml-audio)
target_include_directories(PacMan PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

file(COPY res DESTINATION ${CMAKE_BINARY_DIR})

