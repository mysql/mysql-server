cmake_minimum_required(VERSION 2.6)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/plugin/warpsql-plugins/cmake")
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phpembedVersion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/phpembedVersion.h)

#add_subdirectory(src)
#cmake_minimum_required(VERSION 2.6)
########### next target ###############

MYSQL_ADD_PLUGIN(sql_shim ${example_SRC} MODULE_ONLY)
