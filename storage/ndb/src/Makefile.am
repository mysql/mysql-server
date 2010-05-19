# Copyright (C) 2004-2005 MySQL AB
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

SUBDIRS = common mgmapi ndbapi . kernel mgmclient mgmsrv cw

include $(top_srcdir)/storage/ndb/config/common.mk.am

ndblib_LTLIBRARIES = libndbclient.la

libndbclient_la_SOURCES =

libndbclient_la_LDFLAGS = -version-info @NDB_SHARED_LIB_VERSION@ @NDB_LD_VERSION_SCRIPT@

libndbclient_la_LIBADD =  \
              ndbapi/libndbapi.la \
              common/transporter/libtransporter.la   \
              common/debugger/libtrace.la     \
              common/debugger/signaldata/libsignaldataprint.la \
              mgmapi/libmgmapi.la \
              common/mgmcommon/libmgmsrvcommon.la \
              common/logger/liblogger.la       \
              common/portlib/libportlib.la      \
              common/util/libgeneral.la 

windoze-dsp: libndbclient.dsp

libndbclient.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(ndblib_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ dummy.cpp
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(libndbclient_la_LIBADD)
	@touch dummy.cpp
