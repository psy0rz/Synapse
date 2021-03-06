#this is just a basic CMakeLists.txt, for more information see the cmake manpage

cmake_minimum_required(VERSION 2.6)

#Determine name of the .so module:
#A module should always be in a modulename.module directory!.
string(REGEX REPLACE ".*/([^/]*).module$" "\\1" module_name "${CMAKE_CURRENT_BINARY_DIR}")
MESSAGE(STATUS "Module name of ${CMAKE_CURRENT_BINARY_DIR} is: '${module_name}'")


INCLUDE(FindPkgConfig) 

pkg_check_modules (LIBCURL libcurl>=7.21.3)
IF(LIBCURL_FOUND)
	pkg_check_modules (OAUTH oauth>=0.9.4)
	IF(OAUTH_FOUND)
		MESSAGE(STATUS "Building with oauth support.")
		ADD_DEFINITIONS(-DOAUTH  )
	ELSE(OAUTH_FOUND)
		MESSAGE(WARNING " Building without oauth support, modules like twitter depend on this and will not work without it.")
	ENDIF(OAUTH_FOUND)

	#we need to set CURL_LIBRARIES, since the pkg_check_modules method is not setting LIBCURL_LIBRARIES correct?
	INCLUDE(FindCURL)

	#add definitions, compiler switches, etc.
	ADD_DEFINITIONS()
	
	#automaticly add all sourcefiles to current module
	file(GLOB sources *.cpp)
	ADD_LIBRARY(${module_name} MODULE ${sources})
	
	#need to link to some other libraries ? just add them here
	TARGET_LINK_LIBRARIES(${module_name} ${CURL_LIBRARIES} ${OAUTH_LIBRARIES} )
	
	INCLUDE_DIRECTORIES(../..)
	
	#add an install target here
	#INSTALL_FILES(...)
	#INSTALL_PROGRAMS(...)
	#INSTALL_TARGET(...)
ELSE(LIBCURL_FOUND)
	MESSAGE(WARNING " curl libraries not found, not building ${module_name}")
ENDIF(LIBCURL_FOUND)


