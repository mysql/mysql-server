# Copyright (C) 2009 Sun Microsystems,Inc
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02

IF(WIN32)
  SET(LIBPREFIX "")
ELSE()
  SET(LIBPREFIX "lib")
ENDIF()

SET(SEMISYNC_MASTER_SOURCES  semisync.cc semisync_master.cc semisync_master_plugin.cc
 semisync.h semisync_master.h)

SET(SEMISYNC_MASTER_PLUGIN_DYNAMIC ${LIBPREFIX}semisync_master)
MYSQL_PLUGIN(SEMISYNC_MASTER)

SET(SEMISYNC_SLAVE_SOURCES  semisync.cc semisync_slave.cc semisync_slave_plugin.cc
semisync.h semisync_slave.h )
SET(SEMISYNC_SLAVE_PLUGIN_DYNAMIC ${LIBPREFIX}semisync_slave)
MYSQL_PLUGIN(SEMISYNC_SLAVE)

