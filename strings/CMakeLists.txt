# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/compile_flags.cmake)

SET(STRINGS_SOURCES
  ctype-big5.cc
  ctype-bin.cc
  ctype.cc
  ctype-cp932.cc
  ctype-czech.cc
  ctype-eucjpms.cc
  ctype-euc_kr.cc
  ctype-extra.cc
  ctype-gb18030.cc
  ctype-gb2312.cc
  ctype-gbk.cc
  ctype-latin1.cc
  ctype-mb.cc
  ctype-simple.cc
  ctype-sjis.cc
  ctype-tis620.cc
  ctype-uca.cc
  ctype-ucs2.cc
  ctype-ujis.cc
  ctype-utf8.cc
  ctype-win1250ch.cc
  decimal.cc
  dtoa.cc
  int2str.cc
  longlong2str.cc
  my_strchr.cc
  my_strtoll10.cc
  str2int.cc
  str_alloc.cc
  strcont.cc
  strmake.cc
  strxmov.cc
  strxnmov.cc
  xml.cc
)

# Aliasing bug in union in Clang
# Bug#26161442 MAIN.SELECT* TESTS ARE FAILING WITH CLANG 4.0
# Problem: wrong results for some corner cases when converting string to double.
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "4.0")
  ADD_COMPILE_FLAGS(dtoa.cc COMPILE_FLAGS "-fno-strict-aliasing")
ENDIF()

# Avoid dependencies on perschema data defined in mysys
ADD_DEFINITIONS(-DDISABLE_MYSQL_THREAD_H)
ADD_CONVENIENCE_LIBRARY(strings ${STRINGS_SOURCES})

MYSQL_ADD_EXECUTABLE(conf_to_src conf_to_src.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(conf_to_src strings)

MYSQL_ADD_EXECUTABLE(uca9dump uca9-dump.cc SKIP_INSTALL)
