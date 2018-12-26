# Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(STRINGS_SOURCES bchange.c ctype-big5.c ctype-bin.c ctype-cp932.c
                ctype-czech.c ctype-euc_kr.c ctype-eucjpms.c ctype-extra.c ctype-gb2312.c ctype-gbk.c ctype-gb18030.c
                ctype-latin1.c ctype-mb.c ctype-simple.c ctype-sjis.c ctype-tis620.c ctype-uca.c
                ctype-ucs2.c ctype-ujis.c ctype-utf8.c ctype-win1250ch.c ctype.c decimal.c dtoa.c int2str.c
                is_prefix.c llstr.c longlong2str.c my_strtoll10.c my_vsnprintf.c
                str2int.c str_alloc.c strcend.c strend.c strfill.c strmake.c
                my_stpmov.c my_stpnmov.c
                strxmov.c strxnmov.c xml.c
		my_strchr.c strcont.c strappend.c)

IF(NOT HAVE_STRNLEN)
  # OSX does not have strnlen
  SET(STRINGS_SOURCES ${STRINGS_SOURCES} strnlen.c)
ENDIF()
# Avoid dependencies on perschema data defined in mysys
ADD_DEFINITIONS(-DDISABLE_MYSQL_THREAD_H)
ADD_CONVENIENCE_LIBRARY(strings ${STRINGS_SOURCES})

ADD_EXECUTABLE(conf_to_src EXCLUDE_FROM_ALL conf_to_src.c)
TARGET_LINK_LIBRARIES(conf_to_src strings)
