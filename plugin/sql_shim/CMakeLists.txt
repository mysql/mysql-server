cmake_minimum_required(VERSION 2.6)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/cmake")
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phpembedVersion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/phpembedVersion.h)

#add_subdirectory(src)
#cmake_minimum_required(VERSION 2.6)
########### next target ###############

#SET(phpembed_LIB_SRCS
#   php_cxx.cpp
#   php_arr.cpp
#   php_stl.cpp
#   php_tok.cpp

#   php_arr.h
#   php_cxx.h
#   php_stl.h
#   php_tok_consts.h
#   php_tok.h
#)

SET(example_SRC
  sql_shim.cc
)

#ADD_DEFINITIONS(-D ZTS)

#FIND_LIBRARY(PHP_LOC php5 PATHS ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/libs ${PLUGINDIR})

#INCLUDE_DIRECTORIES(
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/warpsql_php/src
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/include
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/main
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/sapi/embed
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/Zend
#  ${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/extern/php/TSRM
#) 
#add_definitions(-std=c++11) 

#ADD_LIBRARY(php5 SHARED IMPORTED)
#SET_TARGET_PROPERTIES(php5 PROPERTIES IMPORTED_LOCATION ${PHP_LOC} )

#add_library(phpembed SHARED ${phpembed_LIB_SRCS})
MYSQL_ADD_PLUGIN(sql_shim ${example_SRC} MODULE_ONLY)
#target_link_libraries(warpsql_php php5)

#install(TARGETS phpembed DESTINATION ${PLUGINDIR})
