# Copyright (C) 2009 Sun Microsystems, Inc
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
 
#
# Headers which need to be checked for abi/api compatibility.
# API_PREPROCESSOR_HEADER will be used until mysql_priv.h stablizes
# after which TEST_PREPROCESSOR_HEADER will be used.
#
# We use gcc specific preprocessing command and sed/diff, so it will 
# only be run  on Unix and only if gcc is used.
IF(CMAKE_COMPILER_IS_GNUCC AND UNIX)
  IF(CMAKE_C_COMPILER MATCHES "ccache$")
    SET(COMPILER ${CMAKE_C_COMPILER_ARG1})
    STRING(REGEX REPLACE "^ " "" COMPILER ${COMPILER})
  ELSE()
    SET(COMPILER ${CMAKE_C_COMPILER})
  ENDIF()
  SET(API_PREPROCESSOR_HEADER 
    ${CMAKE_SOURCE_DIR}/include/mysql/plugin.h
    ${CMAKE_SOURCE_DIR}/include/mysql.h)

  SET(TEST_PREPROCESSOR_HEADER 
    ${CMAKE_SOURCE_DIR}/include/mysql/plugin.h
    ${CMAKE_SOURCE_DIR}/sql/mysql_priv.h
    ${CMAKE_SOURCE_DIR}/include/mysql.h)


  ADD_CUSTOM_TARGET(abi_check ALL
  COMMAND ${CMAKE_COMMAND} 
    -DCOMPILER=${COMPILER}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DBINARY_DIR=${CMAKE_BINARY_DIR}
    "-DABI_HEADERS=${API_PREPROCESSOR_HEADER}"
    -P ${CMAKE_SOURCE_DIR}/cmake/do_abi_check.cmake
    VERBATIM
  )

  ADD_CUSTOM_TARGET(abi_check_all
  COMMAND ${CMAKE_COMMAND} 
    -DCMAKE_C_COMPILER=${COMPILER} 
    -DCMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
    "-DABI_HEADERS=${TEST_PREPROCESSOR_HEADER}"
    -P ${CMAKE_SOURCE_DIR}/cmake/scripts/do_abi_check.cmake
    VERBATIM
  )
ENDIF()

