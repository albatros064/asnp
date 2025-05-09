cmake_minimum_required(VERSION 3.15)

project(asnp VERSION 0.1)

set(CMAKE_CPP_FLAGS "-g")
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(rapidyaml)

add_library(libelf OBJECT)
add_subdirectory(libelf)

add_executable(asnp $<TARGET_OBJECTS:libelf>)
#target_link_libraries(asnp config++)
target_link_libraries(asnp PUBLIC ryml::ryml)
target_include_directories(asnp PRIVATE rapidyaml/src rapidyaml/ext/c4core/src)
add_subdirectory(as)

add_executable(ldnp $<TARGET_OBJECTS:libelf>)
add_subdirectory(ld)
