#this is just a basic CMakeLists.txt, for more information see the cmake manpage

cmake_minimum_required(VERSION 2.6)

INCLUDE(FindCURL)
#pkg_check_modules (CURL libcurl>=7.0)
IF(CURL_FOUND)

	#add definitions, compiler switches, etc.
	ADD_DEFINITIONS(-Wall -O2)
	
	#Determine name of the .so module:
	#A module should always be in a modulename.module directory!.
	string(REGEX REPLACE ".*/([^/]*).module$" "\\1" module_name "${CMAKE_CURRENT_BINARY_DIR}")
	MESSAGE(STATUS "Module name of ${CMAKE_CURRENT_BINARY_DIR} is: '${module_name}'")
	
	#automaticly add all sourcefiles to current module
	file(GLOB sources *.cpp)
	ADD_LIBRARY(${module_name} MODULE ${sources})
	
	#need to link to some other libraries ? just add them here
	TARGET_LINK_LIBRARIES(${module_name} ${CURL_LIBRARIES})
	
	INCLUDE_DIRECTORIES(../..)
	
	#add an install target here
	#INSTALL_FILES(...)
	#INSTALL_PROGRAMS(...)
	#INSTALL_TARGET(...)

ENDIF(CURL_FOUND)

