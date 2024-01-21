cmake_minimum_required(VERSION 3.15)

project(asnp VERSION 0.1)

set(CMAKE_CPP_FLAGS "-g")
set(CMAKE_CXX_STANDARD 20)

add_executable(asnp main.cpp assemble.cpp arch.cpp segment.cpp assemble.h token.h arch.h error.h segment.h)
target_link_libraries(asnp config++)
#add_subdirectory(nbus)
