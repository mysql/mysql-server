# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE (CheckTypeSize)
INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists)
INCLUDE (TestBigEndian)

CHECK_INCLUDE_FILES (stdint.h HAVE_STDINT_H)
#depending on the platform
CHECK_INCLUDE_FILES (endian.h HAVE_ENDIAN_H)

# The header for glibc versions less than 2.9 will not
# have the endian conversion macros defined
IF (HAVE_ENDIAN_H)
  CHECK_SYMBOL_EXISTS (le64toh endian.h HAVE_LE64TOH)
  CHECK_SYMBOL_EXISTS (le32toh endian.h HAVE_LE32TOH)
  CHECK_SYMBOL_EXISTS (le16toh endian.h HAVE_LE16TOH)
  IF (HAVE_LE32TOH AND HAVE_LE16TOH AND HAVE_LE64TOH)
    SET (HAVE_ENDIAN_CONVERSION_MACROS 1)
  ENDIF(HAVE_LE32TOH AND HAVE_LE16TOH AND HAVE_LE64TOH)
ENDIF(HAVE_ENDIAN_H)

MESSAGE(STATUS " HAVE_ENDIAN_CONVERSION_MACROS ${HAVE_ENDIAN_CONVERSION_MACROS}")
MESSAGE(STATUS " HAVE_LE16TOH ${HAVE_LE16TOH}")
MESSAGE(STATUS " HAVE_LE32TOH ${HAVE_LE32TOH}")
MESSAGE(STATUS " HAVE_LE64TOH ${HAVE_LE64TOH}")
MESSAGE(STATUS " HAVE_ENDIAN_H ${HAVE_ENDIAN_H}")

IF (HAVE_STDINT_H)
  SET(CMAKE_EXTRA_INCLUDE_FILES stdint.h)
  CHECK_TYPE_SIZE("long long" LONG_LONG)
  CHECK_TYPE_SIZE(long LONG)
  CHECK_TYPE_SIZE(int INT)
  SET(CMAKE_EXTRA_INCLUDE_FILES)
ENDIF(HAVE_STDINT_H)

# TODO: Is it better to use __BIG_ENDIAN instead of IS_BIG_ENDIAN
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

SET(HAVE_INT 1)
SET(HAVE_LONG 1)
SET(HAVE_LONG_LONG 1)
