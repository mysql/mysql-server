# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 


#Enable 64 bit file offsets
SET(_LARGE_FILES 1)

# Fix xlC oddity - it complains about same inline function defined multiple times
# in different compilation units  
INCLUDE(CheckCXXCompilerFlag)
 CHECK_CXX_COMPILER_FLAG("-qstaticinline" HAVE_QSTATICINLINE)
 IF(HAVE_QSTATICINLINE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -qstaticinline")
 ENDIF()
 
# The following is required to export all symbols 
# (also with leading underscore)
STRING(REPLACE  "-bexpall" "-bexpfull" CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS
  "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")
STRING(REPLACE  "-bexpall" "-bexpfull" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS
  "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
