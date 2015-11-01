cmake_minimum_required(VERSION 2.8.11)
project(obj-export)

set(CMAKE_CXX_FLAGS "-g -Wall -g -O0 -fms-extensions -std=c++1y")

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

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
    set(LIBS ${LIBS} ${CURL_LIBRARIES} )
else(CURL_FOUND)
    message(FATAL_ERROR "Could not find the CURL library and development files.")
endif(CURL_FOUND)


include_directories(3rdparty/rapidjson/include)

include_directories(${INCLUDE_DIRS})

add_executable(obj-export.out ${SOURCES})
target_link_libraries(obj-export.out ${LIBS})