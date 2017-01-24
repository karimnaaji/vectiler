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

set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/platform)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(EXECUTABLE_NAME vectiler)
    set(VECTILER_SOURCES ${VECTILER_SOURCES} ${PROJECT_SOURCE_DIR}/platform/windows_httpget.cpp)
else()
    set(EXECUTABLE_NAME vectiler.out)
    find_package(CURL)
    if(CURL_FOUND)
        set(INCLUDE_DIRS ${INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
        set(VECTILER_LIBS ${VECTILER_LIBS} ${CURL_LIBRARIES})
        set(VECTILER_SOURCES ${VECTILER_SOURCES} ${PROJECT_SOURCE_DIR}/platform/curl_httpget.cpp)
    else()
        message(FATAL_ERROR "Could not find the CURL library and development files.")
    endif()
endif()

set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/glm)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/rapidjson/include)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/earcut.hpp/include)
set(INCLUDE_DIRS ${INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/3rdparty/flag)

include_directories(${INCLUDE_DIRS})
add_library(vectiler_core ${VECTILER_SOURCES})
add_executable(${EXECUTABLE_NAME} ${C_SOURCES})

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_link_libraries(${EXECUTABLE_NAME} ${VECTILER_LIBS} vectiler_core)
else()
    target_link_libraries(${EXECUTABLE_NAME} ${VECTILER_LIBS} vectiler_core -lcurl)
endif()

# if (BUILD_TESTS)
#     message(STATUS "Build unit tests")
#     add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
# endif()
