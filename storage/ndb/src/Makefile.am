# Copyright (c) 2004-2007 MySQL AB
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

SUBDIRS = common mgmapi ndbapi . kernel mgmclient mgmsrv cw
EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/storage/ndb/config/common.mk.am

ndblib_LTLIBRARIES = libndbclient.la

libndbclient_la_SOURCES =

libndbclient_la_LDFLAGS = -version-info @NDB_SHARED_LIB_VERSION@ @NDB_LD_VERSION_SCRIPT@

libndbclient_la_LIBADD =  \
              ndbapi/libndbapi.la \
              $(top_builddir)/strings/libmystringslt.la \
              $(top_builddir)/mysys/libmysyslt.la \
              $(top_builddir)/dbug/libdbuglt.la \
              common/transporter/libtransporter.la   \
              common/debugger/libtrace.la     \
              common/debugger/signaldata/libsignaldataprint.la \
              mgmapi/libmgmapi.la \
              common/mgmcommon/libmgmsrvcommon.la \
              common/logger/liblogger.la       \
              common/portlib/libportlib.la      \
              common/util/libgeneral.la  @ZLIB_LIBS@

