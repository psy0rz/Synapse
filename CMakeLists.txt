cmake_minimum_required(VERSION 2.6)

#To build a release use:
# cmake -DCMAKE_BUILD_TYPE=Release .
SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Specifies wether or not create a Debug build")

PROJECT(synapse)

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")



# nodig voor json_spirit?
# IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
# SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
# SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
# ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")


SET(CMAKE_COLOR_MAKEFILE ON)

#if you don't want the full compiler output, remove the following line
#SET(CMAKE_VERBOSE_MAKEFILE ON)

#add definitions, compiler switches, etc.
if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
	ADD_DEFINITIONS(-std=c++11 -Wall -O0 -pthread -Werror -fno-strict-aliasing  -fexceptions)
else(${CMAKE_BUILD_TYPE} STREQUAL Debug)
	ADD_DEFINITIONS(-Wall -O3 -pthread -Werror -fno-strict-aliasing  -fexceptions)
endif(${CMAKE_BUILD_TYPE} STREQUAL Debug)

#use less copmiler mem so it compiles on a raspberry with 187 mem.
if(${CMAKE_BUILD_TYPE} STREQUAL ReleaseLowMem)
	ADD_DEFINITIONS(--param ggc-min-expand=100 --param ggc-min-heapsize=8192)
endif(${CMAKE_BUILD_TYPE} STREQUAL ReleaseLowMem)

#list all source files here
file(GLOB sources *.cpp)
MESSAGE(STATUS "Detected source files: ${sources}")
ADD_EXECUTABLE(synapse ${sources})


#################### ADD BOOST STUFF HERE
INCLUDE(FindBoost)
set(Boost_USE_MULTITHREADED ON)
find_package( Boost 1.40.0 REQUIRED thread regex system filesystem)



#need to link to some other libraries ? add the to the modules CMakeList.txt, not here!
#TARGET_LINK_LIBRARIES(synapse boost_thread-mt dl boost_regex-mt boost_system-mt -pg json_spirit)
TARGET_LINK_LIBRARIES(synapse ${Boost_LIBRARIES} json_spirit dl exception)

INCLUDE_DIRECTORIES(libs libs/json_spirit ${Boost_INCLUDE_DIRS})

SUBDIRS(libs modules)
