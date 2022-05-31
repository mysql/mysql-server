# Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#
# NDB_ADD_EXECUTABLE(options source_files)
#    Add an executable file.
#    Options:
#        NDBTEST           link with NDB test library
#        NDBCLIENT         link with dynamic NDB API client library
#        STATIC_NDBCLIENT  link with static NDB API client library
#        MYSQLCLIENT       link with mysql client library
#

FUNCTION(NDB_ADD_EXECUTABLE target)
  SET(OPTIONS "NDBCLIENT" "STATIC_NDBCLIENT" "MYSQLCLIENT" "NDBTEST")
  CMAKE_PARSE_ARGUMENTS(OPT "${OPTIONS}" "" "" ${ARGN})

  MYSQL_ADD_EXECUTABLE(${target} ${OPT_UNPARSED_ARGUMENTS})
  SET_TARGET_PROPERTIES(${target} PROPERTIES ENABLE_EXPORTS TRUE)

  IF(OPT_NDBTEST)
    TARGET_LINK_LIBRARIES(${target} ndbNDBT)
  ENDIF()

  IF(OPT_NDBCLIENT)
    TARGET_LINK_LIBRARIES(${target} ndbclient_so)
  ELSEIF(OPT_STATIC_NDBCLIENT)
    TARGET_LINK_LIBRARIES(${target} ndbclient_static)
  ENDIF()

  IF(OPT_MYSQLCLIENT)
    TARGET_LINK_LIBRARIES(${target} libmysql)
  ENDIF()

  SET_PROPERTY(TARGET ${target}
    PROPERTY INSTALL_RPATH "\$ORIGIN/../${INSTALL_LIBDIR}")
  ADD_INSTALL_RPATH_FOR_OPENSSL(${target})

  IF(OPT_NDBCLIENT AND APPLE AND BUILD_IS_SINGLE_CONFIG)
    # install_name_tool [-change old new] input
    # @loader_path/../lib/ exists both in build and install directories.
    ADD_CUSTOM_COMMAND(TARGET ${target} POST_BUILD
      COMMAND install_name_tool -change
      "@rpath/$<TARGET_FILE_NAME:ndbclient_so>"
      "@loader_path/../lib/$<TARGET_FILE_NAME:ndbclient_so>"
      "$<TARGET_FILE:${target}>"
    )
  ENDIF()
ENDFUNCTION()
