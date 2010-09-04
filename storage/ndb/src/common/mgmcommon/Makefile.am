# Copyright (C) 2004 MySQL AB
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

noinst_LTLIBRARIES = libmgmsrvcommon.la

libmgmsrvcommon_la_SOURCES = \
	ConfigRetriever.cpp \
	IPCConfig.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/mgmapi -I$(top_srcdir)/storage/ndb/src/mgmsrv

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapi.mk.am
include $(top_srcdir)/storage/ndb/config/type_mgmapiclient.mk.am

windoze-dsp: libmgmsrvcommon.dsp

libmgmsrvcommon.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libmgmsrvcommon_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
