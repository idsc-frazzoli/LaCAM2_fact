cmake_minimum_required(VERSION 3.16)
project(lacam2-project CXX)

add_subdirectory(./lacam2)
add_subdirectory(./third_party/argparse)

add_executable(main main.cpp)
target_compile_features(main PUBLIC cxx_std_17)
target_link_libraries(main lacam2 argparse)
