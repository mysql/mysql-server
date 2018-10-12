# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

FILE(READ ${INFILE} LDD_FILE_CONTENTS)
STRING(REPLACE "\n" ";" LDD_FILE_LINES ${LDD_FILE_CONTENTS})

SET(ASAN_LIBRARY_NAME)
FOREACH(LINE ${LDD_FILE_LINES})
  STRING(REGEX MATCH "^[\t ]*(libasan.so.[0-9]) => ([/a-z0-9.]+)" XXX ${LINE})
  IF(CMAKE_MATCH_1)
#    MESSAGE(STATUS "LINE ${LINE}")
#    MESSAGE(STATUS "XXX ${XXX}")
#    MESSAGE(STATUS "CMAKE_MATCH_1 ${CMAKE_MATCH_1}")
#    MESSAGE(STATUS "CMAKE_MATCH_2 ${CMAKE_MATCH_2}")
    SET(ASAN_LIBRARY_NAME ${CMAKE_MATCH_2})
  ENDIF()
ENDFOREACH()
FILE(WRITE ${OUTFILE}
  "const char *asan_library_name=\"${ASAN_LIBRARY_NAME}\";")
