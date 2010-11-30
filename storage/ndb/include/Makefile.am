# Copyright (C) 2004-2006 MySQL AB
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

include $(top_srcdir)/storage/ndb/config/common.mk.am

ndbinclude_HEADERS = \
ndb_constants.h \
ndb_init.h \
ndb_types.h \
ndb_version.h

ndbapiinclude_HEADERS = \
ndbapi/ndbapi_limits.h \
ndbapi/ndb_opt_defaults.h \
ndbapi/Ndb.hpp \
ndbapi/NdbApi.hpp \
ndbapi/NdbTransaction.hpp \
ndbapi/NdbDictionary.hpp \
ndbapi/NdbError.hpp \
ndbapi/NdbEventOperation.hpp \
ndbapi/NdbIndexOperation.hpp \
ndbapi/NdbOperation.hpp \
ndbapi/ndb_cluster_connection.hpp \
ndbapi/NdbBlob.hpp \
ndbapi/NdbPool.hpp \
ndbapi/NdbRecAttr.hpp \
ndbapi/NdbReceiver.hpp \
ndbapi/NdbScanFilter.hpp \
ndbapi/NdbScanOperation.hpp \
ndbapi/NdbIndexScanOperation.hpp \
ndbapi/NdbIndexStat.hpp \
ndbapi/ndberror.h

mgmapiinclude_HEADERS = \
mgmapi/mgmapi.h \
mgmapi/mgmapi_error.h \
mgmapi/mgmapi_debug.h \
mgmapi/mgmapi_config_parameters.h \
mgmapi/mgmapi_config_parameters_debug.h \
mgmapi/ndb_logevent.h \
mgmapi/ndbd_exit_codes.h

noinst_HEADERS = \
ndb_global.h \
ndb_net.h

EXTRA_DIST = debugger editline kernel logger mgmcommon \
portlib transporter util

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`

windoze-dsp:
