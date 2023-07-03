<<<<<<< HEAD
# Copyright (c) 2018, 2022, Oracle and/or its affiliates.
=======
# Copyright (c) 2018, 2023, Oracle and/or its affiliates.
>>>>>>> pr/231
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
<<<<<<< HEAD
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
=======
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
>>>>>>> pr/231

FILE(READ ${INFILE} LDD_FILE_CONTENTS)
STRING(REPLACE "\n" ";" LDD_FILE_LINES ${LDD_FILE_CONTENTS})

SET(ASAN_LIBRARY_NAME)
FOREACH(LINE ${LDD_FILE_LINES})
<<<<<<< HEAD
  STRING(REGEX MATCH "^[\t ]*(libasan.so.[0-9]) => ([/a-zA-Z0-9._-]+)" XXX ${LINE})
=======
  STRING(REGEX MATCH "^[\t ]*(libasan.so.[0-9]) => ([/a-z0-9.]+)" XXX ${LINE})
>>>>>>> pr/231
  IF(CMAKE_MATCH_1)
#    MESSAGE(STATUS "LINE ${LINE}")
#    MESSAGE(STATUS "XXX ${XXX}")
#    MESSAGE(STATUS "CMAKE_MATCH_1 ${CMAKE_MATCH_1}")
#    MESSAGE(STATUS "CMAKE_MATCH_2 ${CMAKE_MATCH_2}")
    SET(ASAN_LIBRARY_NAME ${CMAKE_MATCH_2})
  ENDIF()
<<<<<<< HEAD
  STRING(REGEX MATCH "^[\t ]*(libtirpc.so.[0-9]) => ([/a-zA-Z0-9._-]+)" XXX ${LINE})
  IF(CMAKE_MATCH_1)
#    MESSAGE(STATUS "LINE ${LINE}")
#    MESSAGE(STATUS "XXX ${XXX}")
#    MESSAGE(STATUS "CMAKE_MATCH_1 ${CMAKE_MATCH_1}")
#    MESSAGE(STATUS "CMAKE_MATCH_2 ${CMAKE_MATCH_2}")
    SET(TIRPC_LIBRARY_NAME ${CMAKE_MATCH_2})
  ENDIF()
ENDFOREACH()
FILE(WRITE ${OUTFILE}
  "const char *asan_library_name=\"${ASAN_LIBRARY_NAME}\";"
  "const char *tirpc_library_name=\"${TIRPC_LIBRARY_NAME}\";")
=======
ENDFOREACH()
FILE(WRITE ${OUTFILE}
  "const char *asan_library_name=\"${ASAN_LIBRARY_NAME}\";")
>>>>>>> pr/231
