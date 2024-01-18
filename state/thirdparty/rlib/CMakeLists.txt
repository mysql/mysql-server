# Author: Ming Zhang
# Copyright (c) 2022

file(GLOB SOURCES "*.hpp")

add_library(rlib STATIC ${SOURCES})
set_target_properties(rlib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rlib ibverbs pthread)