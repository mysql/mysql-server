# Copyright (C) 2007 MySQL AB
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


INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

ADD_LIBRARY(ndbdbtup STATIC
            DbtupExecQuery.cpp
            DbtupBuffer.cpp
            DbtupRoutines.cpp
            DbtupCommit.cpp
            DbtupFixAlloc.cpp
            DbtupTrigger.cpp
            DbtupAbort.cpp
            DbtupPageMap.cpp
            DbtupPagMan.cpp
            DbtupStoredProcDef.cpp
            DbtupMeta.cpp
            DbtupTabDesMan.cpp
            DbtupGen.cpp
            DbtupIndex.cpp
            DbtupScan.cpp
            DbtupDebug.cpp
            DbtupDiskAlloc.cpp
            DbtupVarAlloc.cpp
            tuppage.cpp
            Undo_buffer.cpp
	    DbtupProxy.cpp
)
