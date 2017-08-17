# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/components/logging
  ${CMAKE_SOURCE_DIR}/components/mysql_server
  ${CMAKE_SOURCE_DIR}/libbinlogevents/include
)

REMOVE_DEFINITIONS(-DMYSQL_SERVER)

MYSQL_ADD_COMPONENT(log_sink_syseventlog
  log_sink_syseventlog.cc
  MODULE)

MYSQL_ADD_COMPONENT(log_sink_json
  log_sink_json.cc
  MODULE)

# IF (WITH_SYSTEMD)
#
# MYSQL_ADD_COMPONENT(log_sink_systemd
#   log_sink_systemd.cc
#   MODULE)
# TARGET_LINK_LIBRARIES(component_log_sink_systemd systemd mysys)
#
# ENDIF()
#
# MYSQL_ADD_COMPONENT(log_sink_xml
#   log_sink_xml.cc
#   MODULE)
#
# MYSQL_ADD_COMPONENT(log_filter_draugnet
#   log_filter_draugnet.cc
#   MODULE)

MYSQL_ADD_COMPONENT(log_sink_test
  log_sink_test.cc
  MODULE)
