# Copyright (c) 2006, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

ADD_WSHADOW_WARNING()

SET(STRINGS_SOURCES
  collations.cc
  collations_internal.cc
  ctype-big5.cc
  ctype-bin.cc
  ctype-cp932.cc
  ctype-czech.cc
  ctype-euc_kr.cc
  ctype-eucjpms.cc
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
  ctype.cc
  dtoa.cc
  int2str.cc
  my_strchr.cc
  my_strtoll10.cc
  my_uctype.cc
  sql_chars.cc
  str_alloc.cc
  xml.cc
)

MYSQL_ADD_EXECUTABLE(uca9dump uca9-dump.cc SKIP_INSTALL)

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  ADD_COMPILE_FLAGS(uca9-dump.cc COMPILE_FLAGS ${HAS_MISSING_PROFILE})
ENDIF()

SET(ZH_HANS_SRC_FILE ${CMAKE_SOURCE_DIR}/strings/lang_data/zh_hans.txt)
SET(ZH_HANS_DST_FILE ${CMAKE_BINARY_DIR}/strings/uca900_zh_tbls.cc)
SET(JA_HANS_SRC_FILE ${CMAKE_SOURCE_DIR}/strings/lang_data/ja_hans.txt)
SET(JA_HANS_DST_FILE ${CMAKE_BINARY_DIR}/strings/uca900_ja_tbls.cc)

ADD_CUSTOM_COMMAND(
  OUTPUT ${JA_HANS_DST_FILE}
  COMMAND uca9dump ja
  --in_file=${JA_HANS_SRC_FILE}
  --out_file=${JA_HANS_DST_FILE}
  DEPENDS uca9dump ${JA_HANS_SRC_FILE}
  )
ADD_CUSTOM_TARGET(strings_ja_hans DEPENDS ${JA_HANS_DST_FILE})

ADD_CUSTOM_COMMAND(
  OUTPUT ${ZH_HANS_DST_FILE}
  COMMAND uca9dump zh
  --in_file=${ZH_HANS_SRC_FILE}
  --out_file=${ZH_HANS_DST_FILE}
  DEPENDS uca9dump ${ZH_HANS_SRC_FILE}
  )
ADD_CUSTOM_TARGET(strings_zh_hans DEPENDS ${ZH_HANS_DST_FILE})

SET_SOURCE_FILES_PROPERTIES(
  ${JA_HANS_DST_FILE} ${ZH_HANS_DST_FILE}
  PROPERTIES GENERATED TRUE
  )

LIST(APPEND STRINGS_SOURCES ${JA_HANS_DST_FILE} ${ZH_HANS_DST_FILE})
# Avoid dependencies on perschema data defined in mysys
ADD_DEFINITIONS(-DDISABLE_MYSQL_THREAD_H)

IF(NOT WIN32)
  SET(STRINGS_COMPILE_OPTIONS COMPILE_OPTIONS "-fvisibility=hidden")
ENDIF()

ADD_CONVENIENCE_LIBRARY(strings ${STRINGS_SOURCES}
  ${STRINGS_COMPILE_OPTIONS}
  DEPENDENCIES strings_ja_hans strings_zh_hans
  )

# This is built to ensure that the strings library is indeed independent
# of other MySQL libraries.
IF(APPLE)
  # We do not build object libraries on Mac, so compile everything twice.
  ADD_SHARED_LIBRARY(strings_shared
    ${STRINGS_SOURCES}
    NO_UNDEFINED
    SKIP_INSTALL
    DEPENDENCIES strings_ja_hans strings_zh_hans
    )
ELSEIF(WIN32)
  # We need to export the desired symbols, and generate the import library.
  # Note the definition of MYSQL_LIBSTRINGS_DLL.
  ADD_SHARED_LIBRARY(strings_shared
    ${STRINGS_SOURCES}
    SKIP_INSTALL
    COMPILE_DEFINITIONS MYSQL_LIBSTRINGS_DLL
    DEPENDENCIES strings_ja_hans strings_zh_hans
    )
ELSE()
  ADD_SHARED_LIBRARY(strings_shared
    $<TARGET_OBJECTS:strings_objlib>
    NO_UNDEFINED
    SKIP_INSTALL
    DEPENDENCIES strings_ja_hans strings_zh_hans
    )
ENDIF()

MYSQL_ADD_EXECUTABLE(conf_to_src conf_to_src.cc SKIP_INSTALL)
TARGET_LINK_LIBRARIES(conf_to_src strings)

IF(HAS_MISSING_PROFILE)
  ADD_COMPILE_FLAGS(conf_to_src.cc COMPILE_FLAGS ${HAS_MISSING_PROFILE})
ENDIF()
