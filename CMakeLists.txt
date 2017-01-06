cmake_minimum_required(VERSION 2.8.7)
project(vectiler)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

message(STATUS "CXX Flags ${CMAKE_CXX_FLAGS}")

include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB_RECURSE C_SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE VECTILER_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
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
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/flag)

include_directories(${INCLUDE_DIRS})

add_library(vectiler ${VECTILER_SOURCES})

add_executable(vectiler.out ${C_SOURCES})

set(VECTILER_LIBS ${LIBS})

target_link_libraries(vectiler.out ${VECTILER_LIBS} vectiler -lcurl)

# if (BUILD_TESTS)
#     message(STATUS "Build unit tests")
#     add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
# endif()
