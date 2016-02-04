# Copyright (c) 2009, 2016, Oracle and/or its affiliates. All rights reserved.
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

# Charsets and collations
IF(NOT DEFAULT_CHARSET)
  SET(DEFAULT_CHARSET "latin1")
ENDIF()

IF(NOT DEFAULT_COLLATION)
  SET(DEFAULT_COLLATION "latin1_swedish_ci")
ENDIF()

SET(CHARSETS ${DEFAULT_CHARSET} latin1 utf8 utf8mb4)

SET(CHARSETS_AVAILABLE
  armscii8
  ascii
  big5
  binary
  cp1250
  cp1251
  cp1256
  cp1257
  cp850
  cp852
  cp866
  cp932
  dec8
  eucjpms
  euckr
  gb18030
  gb2312
  gbk
  geostd8
  greek
  hebrew
  hp8
  keybcs2
  koi8r
  koi8u
  latin1
  latin2
  latin5
  latin7
  macce
  macroman
  sjis
  swe7
  tis620
  ucs2
  ujis
  utf16
  utf16le
  utf32
  utf8
  utf8mb4
)

IF(WITH_EXTRA_CHARSETS AND NOT WITH_EXTRA_CHARSETS STREQUAL "all")
  MESSAGE(WARNING "Option WITH_EXTRA_CHARSETS is no longer supported")
ENDIF()

SET(CHARSETS ${CHARSETS} ${CHARSETS_AVAILABLE})

SET(MYSQL_DEFAULT_CHARSET_NAME "${DEFAULT_CHARSET}") 
SET(MYSQL_DEFAULT_COLLATION_NAME "${DEFAULT_COLLATION}")

FOREACH(cs in ${CHARSETS})
  SET(HAVE_CHARSET_${cs} 1)
ENDFOREACH()
