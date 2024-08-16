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


INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi
  )

ADD_CONVENIENCE_LIBRARY(ndbmgmcommon
  Config.cpp
  ConfigInfo.cpp
  ConfigObject.cpp
  ConfigRetriever.cpp
  ConfigSection.cpp
  InitConfigFileParser.cpp
  IPCConfig.cpp
  DnsCache.cpp
  thr_config.cpp
  LINK_LIBRARIES ndbmgmapi
  )

MY_CHECK_CXX_COMPILER_WARNING("format-overflow" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  ADD_COMPILE_FLAGS(thr_config.cpp COMPILE_FLAGS "-Wno-error=format-overflow")
ENDIF()

NDB_ADD_TEST(thr_config-t thr_config.cpp LIBS ndbmgmapi ndbgeneral ndbportlib)
NDB_ADD_TEST(InitConfigFileParser-t InitConfigFileParser.cpp
          LIBS ndbmgmcommon ndbgeneral ndbportlib)
