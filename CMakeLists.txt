cmake_minimum_required(VERSION 2.5)
project(PatternFinder)

set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
 
include_directories("${CMAKE_BINARY_DIR}/../code/include")

FILE(GLOB HEADERFILES ${CMAKE_BINARY_DIR}/../code/include/*.h)
FILE(GLOB SRCFILES ${CMAKE_BINARY_DIR}/../code/src/*.cpp)

IF(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    FILE(GLOB WINFILES ${CMAKE_BINARY_DIR}/../code/src/windows/*.cpp)
ENDIF()

add_executable(PatternFinder ${SRCFILES} ${HEADERFILES} ${WINFILES})
target_compile_features(PatternFinder PRIVATE cxx_range_for)

IF(UNIX)
    target_link_libraries(PatternFinder pthread)
ENDIF()
