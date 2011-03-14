# Copyright 2010, Oracle and/or its affiliates. All rights reserved.
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

INCLUDE_DIRECTORIES(${WIX_DIR}/../SDK/inc)
LINK_DIRECTORIES(${WIX_DIR}/../SDK/lib)

SET(WIXCA_SOURCES CustomAction.cpp CustomAction.def)

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
	LINK_LIBRARIES(wcautil_x64 dutil_x64 msi version)
ELSE()
	LINK_LIBRARIES(wcautil dutil msi version)
ENDIF()
	
ADD_LIBRARY(wixca SHARED ${WIXCA_SOURCES})
