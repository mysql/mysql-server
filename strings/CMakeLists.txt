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

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

SET(STRINGS_SOURCES bchange.c bcmp.c bfill.c bmove512.c bmove_upp.c ctype-big5.c ctype-bin.c ctype-cp932.c
                ctype-czech.c ctype-euc_kr.c ctype-eucjpms.c ctype-extra.c ctype-gb2312.c ctype-gbk.c
                ctype-latin1.c ctype-mb.c ctype-simple.c ctype-sjis.c ctype-tis620.c ctype-uca.c
                ctype-ucs2.c ctype-ujis.c ctype-utf8.c ctype-win1250ch.c ctype.c decimal.c int2str.c
                is_prefix.c llstr.c longlong2str.c my_strtoll10.c my_vsnprintf.c r_strinstr.c
                str2int.c str_alloc.c strcend.c strend.c strfill.c strmake.c strmov.c strnmov.c 
                strtod.c strtol.c strtoll.c strtoul.c strtoull.c strxmov.c strxnmov.c xml.c
		my_strchr.c strcont.c strinstr.c strnlen.c)

IF(NOT SOURCE_SUBLIBS)
  ADD_LIBRARY(strings ${STRINGS_SOURCES})
ENDIF(NOT SOURCE_SUBLIBS)
