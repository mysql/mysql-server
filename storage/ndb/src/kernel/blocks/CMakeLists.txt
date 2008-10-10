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


ADD_SUBDIRECTORY(cmvmi)
ADD_SUBDIRECTORY(dbacc)
ADD_SUBDIRECTORY(dbdict)
ADD_SUBDIRECTORY(dbdih)
ADD_SUBDIRECTORY(dblqh)
ADD_SUBDIRECTORY(dbtc)
ADD_SUBDIRECTORY(dbtup)
ADD_SUBDIRECTORY(ndbfs)
ADD_SUBDIRECTORY(ndbcntr)
ADD_SUBDIRECTORY(qmgr)
ADD_SUBDIRECTORY(trix)
ADD_SUBDIRECTORY(backup)
ADD_SUBDIRECTORY(dbutil)
ADD_SUBDIRECTORY(suma)
ADD_SUBDIRECTORY(dbtux)
ADD_SUBDIRECTORY(dbinfo)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

ADD_LIBRARY(ndbblocks  STATIC
            diskpage.cpp
            lgman.cpp
            pgman.cpp
	    RestoreProxy.cpp
            restore.cpp
            tsman.cpp
	    LocalProxy.cpp
           )

ADD_EXECUTABLE(ndb_print_file
	       print_file.cpp
	       diskpage.cpp
	       dbtup/tuppage.cpp
	      )
TARGET_LINK_LIBRARIES(ndb_print_file ndbclient dbug mysys strings)
