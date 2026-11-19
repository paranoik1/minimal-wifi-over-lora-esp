# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)
set(EXTRA_COMPONENT_DIRS "../managed_components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tests)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
