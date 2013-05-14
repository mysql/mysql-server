# Copyright (c) 2004-2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

EXTRA_DIST = CMakeLists.txt \
  message.mc message.h message.rc MSG00001.bin

noinst_LTLIBRARIES = liblogger.la

liblogger_la_SOURCES = Logger.cpp \
  LogHandlerList.cpp LogHandler.cpp \
  ConsoleLogHandler.cpp FileLogHandler.cpp \
  SysLogHandler.cpp EventLogHandler.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am

