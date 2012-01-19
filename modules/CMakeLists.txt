cmake_minimum_required(VERSION 2.6)

PROJECT(modules)

#if you don't want the full compiler output, remove the following line

SET(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB modules *.module)
MESSAGE(STATUS "Detected synapse modules: ${modules}")
SUBDIRS(${modules})

find_package(Doxygen)
if(DOXYGEN_FOUND)
 configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
 add_custom_target(doc ALL
  ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating API documentation with Doxygen" VERBATIM
 )
endif(DOXYGEN_FOUND)
