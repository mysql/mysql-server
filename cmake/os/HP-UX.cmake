# Copyright (c) 2010 Sun Microsystems, Inc.
# Use is subject to license terms.
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

INCLUDE(CheckCXXSourceCompiles)
# Enable 64 bit file offsets
SET(_LARGEFILE64_SOURCE 1)
SET(_FILE_OFFSET_BITS 64)
# If Itanium make shared library suffix .so
# OS understands both .sl and .so. CMake would
# use .sl, however MySQL prefers .so
IF(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "9000")
  SET(CMAKE_SHARED_LIBRARY_SUFFIX ".so" CACHE INTERNAL "" FORCE)
  SET(CMAKE_SHARED_MODULE_SUFFIX ".so" CACHE INTERNAL "" FORCE)
ENDIF()
IF(CMAKE_SYSTEM MATCHES "11")
  ADD_DEFINITIONS(-DHPUX11)
ENDIF()

IF(CMAKE_CXX_COMPILER_ID MATCHES "HP")
  # Enable standard C++ flags if required
  # HP seems a bit traditional and "new" features like ANSI for-scope
  # still require special flag to be set 
   CHECK_CXX_SOURCE_COMPILES(
   "int main()
    {
      for(int i=0; i<1; i++);
      for(int i=0; i<1; i++);
      return 0;
    }
   " HAVE_ANSI_FOR_SCOPE)
   IF(NOT HAVE_ANSI_FOR_SCOPE)
     # Enable conformant behavior
     SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Aa")
   ENDIF()
ENDIF()
