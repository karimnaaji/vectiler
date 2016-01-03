cmake_minimum_required(VERSION 2.8.8)
project(objexport)

# aobaker
add_subdirectory(${PROJECT_SOURCE_DIR}/3rdparty/aobaker/thekla)
add_subdirectory(${PROJECT_SOURCE_DIR}/3rdparty/aobaker/vendor/poshlib)

exec_program(brew
    ARGS "info embree | tr ' ' '\n' | grep Cellar"
    OUTPUT_VARIABLE EMBREE_PATH)

exec_program(brew
    ARGS "info tbb | tr ' ' '\n' | grep Cellar"
    OUTPUT_VARIABLE TBB_PATH)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
endif()

add_definitions(
    -O3
    -I${EMBREE_PATH}/include
    -I${TBB_PATH}/include
    -Wall)

link_directories(
    ${EMBREE_PATH}/lib
    ${TBB_PATH}/lib)

include_directories(
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/thekla/thekla
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/vendor/flag
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/vendor/stb
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/vendor/tinyobj)

add_library(aobaker
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/raytrace.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/aobaker/aobaker.cpp)

file(GLOB_RECURSE CPP_SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE C_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/src/*.h)

set(INCLUDE_DIRS "")
set(LIBS "")

foreach(_headerFile ${HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list(APPEND INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

find_package(CURL)
if(CURL_FOUND)
    set(INCLUDE_DIRS ${INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
    set(LIBS ${LIBS} ${CURL_LIBRARIES})
else(CURL_FOUND)
    message(FATAL_ERROR "Could not find the CURL library and development files.")
endif(CURL_FOUND)

set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/glm)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/rapidjson/include)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/earcut.hpp/include)

include_directories(${INCLUDE_DIRS})

add_executable(objexport ${CPP_SOURCES} ${C_SOURCES})
target_link_libraries(objexport ${LIBS} aobaker thekla_atlas embree tbb)
