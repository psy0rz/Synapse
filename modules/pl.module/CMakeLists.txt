#this is just a basic CMakeLists.txt, for more information see the cmake manpage

cmake_minimum_required(VERSION 2.6)

#add definitions, compiler switches, etc.
ADD_DEFINITIONS(-Wall -O2)

#Determine name of the .so module:
#A module should always be in a modulename.module directory!.
string(REGEX REPLACE ".*/([^/]*).module$" "\\1" module_name "${CMAKE_CURRENT_BINARY_DIR}")
MESSAGE(STATUS "Module name of ${CMAKE_CURRENT_BINARY_DIR} is: '${module_name}'")

#automaticly add all sourcefiles to current module
file(GLOB sources *.cpp)
ADD_LIBRARY(${module_name} MODULE ${sources})

#need boost librarys? add them here:
INCLUDE(FindBoost)
set(Boost_USE_MULTITHREADED ON)
find_package( Boost 1.40.0 REQUIRED filesystem )

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(${module_name} ${Boost_LIBRARIES} )

INCLUDE_DIRECTORIES(../..)

