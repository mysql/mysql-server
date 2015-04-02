# Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

# NdbApi examples
ADD_EXECUTABLE(ndb_ndbapi_array_simple ndbapi_array_simple/ndbapi_array_simple.cpp)
ADD_EXECUTABLE(ndb_ndbapi_array_using_adapter ndbapi_array_using_adapter/ndbapi_array_using_adapter.cpp)
ADD_EXECUTABLE(ndb_ndbapi_async ndbapi_async/ndbapi_async.cpp)
ADD_EXECUTABLE(ndb_ndbapi_async1 ndbapi_async1/ndbapi_async1.cpp)
ADD_EXECUTABLE(ndb_ndbapi_blob ndbapi_blob/ndbapi_blob.cpp)
ADD_EXECUTABLE(ndb_ndbapi_blob_ndbrecord ndbapi_blob_ndbrecord/main.cpp)
ADD_EXECUTABLE(ndb_ndbapi_event ndbapi_event/ndbapi_event.cpp)
ADD_EXECUTABLE(ndb_ndbapi_recattr_vs_record ndbapi_recattr_vs_record/main.cpp)
ADD_EXECUTABLE(ndb_ndbapi_retries ndbapi_retries/ndbapi_retries.cpp)
ADD_EXECUTABLE(ndb_ndbapi_scan ndbapi_scan/ndbapi_scan.cpp)
ADD_EXECUTABLE(ndb_ndbapi_simple ndbapi_simple/ndbapi_simple.cpp)
ADD_EXECUTABLE(ndb_ndbapi_simple_dual ndbapi_simple_dual/main.cpp)
ADD_EXECUTABLE(ndb_ndbapi_simple_index ndbapi_simple_index/main.cpp)
ADD_EXECUTABLE(ndb_ndbapi_s_i_ndbrecord ndbapi_s_i_ndbrecord/main.cpp)

# MgmApi examples
ADD_EXECUTABLE(ndb_mgmapi_logevent mgmapi_logevent/main.cpp)
ADD_EXECUTABLE(ndb_mgmapi_logevent2 mgmapi_logevent2/main.cpp)

# Link each example program with both ndbclient and mysql libraries
SET(EXAMPLES
  ndb_ndbapi_array_simple
  ndb_ndbapi_array_using_adapter
  ndb_ndbapi_async
  ndb_ndbapi_async1
  ndb_ndbapi_blob
  ndb_ndbapi_blob_ndbrecord
  ndb_ndbapi_event
  ndb_ndbapi_recattr_vs_record
  ndb_ndbapi_retries
  ndb_ndbapi_scan
  ndb_ndbapi_simple
  ndb_ndbapi_simple_dual
  ndb_ndbapi_simple_index
  ndb_ndbapi_s_i_ndbrecord
  ndb_mgmapi_logevent
  ndb_mgmapi_logevent2
)
FOREACH(EXAMPLE ${EXAMPLES})
  TARGET_LINK_LIBRARIES(${EXAMPLE} libmysql)
  TARGET_LINK_LIBRARIES(${EXAMPLE} ndbclient_so)
ENDFOREACH()

