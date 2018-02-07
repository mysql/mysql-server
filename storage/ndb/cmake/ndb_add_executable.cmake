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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

INCLUDE(mysql_add_executable)

#
# NDB_ADD_EXECUTABLE(options source_files)
#    Add an executable file.
#    Options:
#        NDBTEST           link with NDB test and dynamic NDB client libraries
#        NDBCLIENT         link with dynamic NDB API client library
#        STATIC_NDBCLIENT  link with static NDB API client library
#        MYSQLCLIENT       link with mysql client library
#

FUNCTION(NDB_ADD_EXECUTABLE target)
  SET(PASSTHROUGH_ARGS ${target})    # Args passed on to mysql_add_executable()
  SET(OPTIONS "NDBCLIENT" "STATIC_NDBCLIENT" "MYSQLCLIENT" "NDBTEST")

  FOREACH(arg ${ARGN})
    LIST(FIND OPTIONS ${arg} index)
    IF(index GREATER -1)
      SET(OPT_${arg} TRUE)
    ELSE()
      LIST(APPEND PASSTHROUGH_ARGS ${arg})
    ENDIF()
  ENDFOREACH(arg)

  MYSQL_ADD_EXECUTABLE(${PASSTHROUGH_ARGS})

  IF(OPT_NDBTEST)
    TARGET_LINK_LIBRARIES(${target} ndbNDBT ndbclient_so)
  ELSEIF(OPT_NDBCLIENT)
    TARGET_LINK_LIBRARIES(${target} ndbclient_so)
  ELSEIF(OPT_STATIC_NDBCLIENT)
    TARGET_LINK_LIBRARIES(${target} ndbclient_static)
  ENDIF()

  IF(OPT_MYSQLCLIENT)
    TARGET_LINK_LIBRARIES(${target} libmysql)
  ENDIF()

ENDFUNCTION()
