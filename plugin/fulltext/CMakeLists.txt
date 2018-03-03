# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
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

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="mecab")

# Example parser
MYSQL_ADD_PLUGIN(ftexample ./example/plugin_example.cc
                MODULE_ONLY MODULE_OUTPUT_NAME "mypluglib")

# Ngram parser
MYSQL_ADD_PLUGIN(ngram_parser ./ngram_parser/plugin_ngram.cc
                 DEFAULT MODULE_OUTPUT_NAME "libpluginngram")

# Mecab parser
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
INCLUDE(mecab)
MYSQL_CHECK_MECAB()

IF(BUNDLE_MECAB)
  ADD_DEFINITIONS(-DBUNDLE_MECAB)
ENDIF()

IF(WITH_DEBUG)
  ADD_DEFINITIONS(-DDEBUG)
ENDIF()

IF(WITH_MECAB)
  INCLUDE_DIRECTORIES(${MECAB_INCLUDE_DIR})

  MYSQL_ADD_PLUGIN(mecab_parser ./mecab_parser/plugin_mecab.cc
                   MODULE_ONLY MODULE_OUTPUT_NAME "libpluginmecab")

  TARGET_LINK_LIBRARIES(mecab_parser ${MECAB_LIBRARY})
ENDIF()
