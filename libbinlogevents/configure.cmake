# Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
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
#

INCLUDE(CheckTypeSize)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckFunctionExists)
INCLUDE(TestBigEndian)

# depending on the platform, we may or may not have this file
CHECK_INCLUDE_FILES(endian.h HAVE_ENDIAN_H)

CHECK_FUNCTION_EXISTS(strndup HAVE_STRNDUP)

# The header for glibc versions less than 2.9 will not
# have the endian conversion macros defined
IF(HAVE_ENDIAN_H)
  CHECK_SYMBOL_EXISTS(le64toh endian.h HAVE_LE64TOH)
  CHECK_SYMBOL_EXISTS(le32toh endian.h HAVE_LE32TOH)
  CHECK_SYMBOL_EXISTS(le16toh endian.h HAVE_LE16TOH)
  IF(HAVE_LE32TOH AND HAVE_LE16TOH AND HAVE_LE64TOH)
    SET(HAVE_ENDIAN_CONVERSION_MACROS 1)
  ENDIF()
ENDIF()

SET(CMAKE_EXTRA_INCLUDE_FILES stdint.h)
CHECK_TYPE_SIZE("long long" LONG_LONG)
CHECK_TYPE_SIZE(long LONG)
CHECK_TYPE_SIZE(int INT)
SET(CMAKE_EXTRA_INCLUDE_FILES)

# TODO: Is it better to use __BIG_ENDIAN instead of IS_BIG_ENDIAN
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)

SET(HAVE_INT 1)
SET(HAVE_LONG 1)
SET(HAVE_LONG_LONG 1)
