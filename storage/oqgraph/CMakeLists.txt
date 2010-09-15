# Copyright (C) 2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE (CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES(
"#include <boost/version.hpp>
#if BOOST_VERSION >= 104000
#else
#error oops
#endif
int main() { return 0; }" BOOST_OK)

IF(BOOST_OK)
  INCLUDE("${PROJECT_SOURCE_DIR}/storage/mysql_storage_engine.cmake")

  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OQGRAPH /EHsc")

  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/sql
                      ${CMAKE_SOURCE_DIR}/regex
                      ${CMAKE_SOURCE_DIR}/extra/yassl/include)

  SET(OQGRAPH_SOURCES ha_oqgraph.cc graphcore.cc)
  MYSQL_STORAGE_ENGINE(OQGRAPH)
ENDIF()

