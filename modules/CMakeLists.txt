cmake_minimum_required(VERSION 2.6)

PROJECT(modules)

#if you don't want the full compiler output, remove the following line

SET(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB modules *.module)
MESSAGE(STATUS "Detected synapse modules: ${modules}")
SUBDIRS(${modules})

