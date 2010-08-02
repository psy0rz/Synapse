cmake_minimum_required(VERSION 2.6)

PROJECT(synapse)




# nodig voor json_spirit?
# IF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
# SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
# SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )
# ENDIF("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")


#uncomment this for releases or debugs
#SET(CMAKE_BUILD_TYPE Release)
SET(CMAKE_BUILD_TYPE Debug)

#if you don't want the full compiler output, remove the following line
SET(CMAKE_VERBOSE_MAKEFILE ON)

#add definitions, compiler switches, etc.
#ADD_DEFINITIONS(-Wall -O2 -pthread -pg -Werror)
ADD_DEFINITIONS(-Wall -O2 -pthread -Werror)

#list all source files here
file(GLOB sources *.cpp)
MESSAGE(STATUS "Detected source files: ${sources}")
ADD_EXECUTABLE(synapse ${sources})


#need to link to some other libraries ? just add them here
#TARGET_LINK_LIBRARIES(synapse boost_thread-mt dl boost_regex-mt boost_system-mt -pg json_spirit)
TARGET_LINK_LIBRARIES(synapse boost_thread-mt dl boost_regex-mt boost_system-mt json_spirit)


INCLUDE_DIRECTORIES(libs libs/json_spirit)

SUBDIRS(libs modules libs/json_spirit)
