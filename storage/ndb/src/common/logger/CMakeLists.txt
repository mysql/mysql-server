# Copyright (c) 2008, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(WIN32)
  SET(EXTRA_SRC EventLogHandler.cpp)
ENDIF()

ADD_CONVENIENCE_LIBRARY(ndblogger
  BufferedLogHandler.cpp
  ConsoleLogHandler.cpp
  FileLogHandler.cpp
  LogHandler.cpp
  LogHandlerList.cpp
  Logger.cpp
  SysLogHandler.cpp
  ${EXTRA_SRC}
  LINK_LIBRARIES ndbgeneral
  )

# Exclude ndblogger_objlib from PGO on Windows to avoid linker error LNK1248
EXCLUDE_FROM_MSVC_PGO(ndblogger_objlib)

NDB_ADD_TEST(logger-t Logger.cpp  LIBS ndbgeneral ndbportlib ndbclient_static)
MY_CHECK_CXX_COMPILER_WARNING("error=maybe-uninitialized" HAS_WARN_FLAG)
IF(TARGET logger-t AND HAS_WARN_FLAG)
  TARGET_LINK_OPTIONS(logger-t PRIVATE ${HAS_WARN_FLAG})
ENDIF()
