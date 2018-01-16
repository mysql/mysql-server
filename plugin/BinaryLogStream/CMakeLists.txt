# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#cmake_minimum_required(VERSION 2.8)

#find_package(${CMAKE_SOURCE_DIR}/libaws-cpp-sdk-core.a)
#find_package(${CMAKE_SOURCE_DIR}/libaws-cpp-sdk-kinesis.a)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -std=c++11")  #this line sets all the cmake flags and ignores warnings
set(AWS_LIBS libaws-cpp-sdk-kinesis.a libaws-cpp-sdk-core.a) #this links AWS libraries kinesis and core to project
set(CURL_LIBRARY "-lcurl") 
find_package(CURL REQUIRED)  
include_directories(${CURL_INCLUDE_DIR})

MYSQL_ADD_PLUGIN(Binary_Stream Bin_Stream.cpp   #last argument is the name of the Shared Object that is generated and first argument is the name needed to install in mysql
  MODULE_ONLY MODULE_OUTPUT_NAME "Binary_Stream"
  LINK_LIBRARIES  ${AWS_LIBS}  ${CURL_LIBRARIES} )  
#target_link_libraries(bin_stream ${CMAKE_SOURCE_DIR}/libaws-cpp-sdk-core.a)
#target_link_libraries(bin_stream ${CMAKE_SOURCE_DIR}/libaws-cpp-sdk-kinesis.a)  




