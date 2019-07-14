cmake_minimum_required(VERSION 3.4)
set (CMAKE_CXX_STANDARD 11)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -DDEBUG")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

project(png2svg)
aux_source_directory(./src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} png)
configure_file(example/test.png example/test.png COPYONLY)