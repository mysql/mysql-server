
include $(top_srcdir)/ndb/config/common.mk.am

ndbinclude_HEADERS = \
ndb_types.h \
ndb_version.h

ndbapiinclude_HEADERS = \
ndbapi/ndbapi_limits.h \
ndbapi/Ndb.hpp \
ndbapi/NdbApi.hpp \
ndbapi/NdbConnection.hpp \
ndbapi/NdbCursorOperation.hpp \
ndbapi/NdbDictionary.hpp \
ndbapi/NdbError.hpp \
ndbapi/NdbEventOperation.hpp \
ndbapi/NdbIndexOperation.hpp \
ndbapi/NdbOperation.hpp \
ndbapi/NdbBlob.hpp \
ndbapi/NdbPool.hpp \
ndbapi/NdbRecAttr.hpp \
ndbapi/NdbReceiver.hpp \
ndbapi/NdbResultSet.hpp \
ndbapi/NdbScanFilter.hpp \
ndbapi/NdbScanOperation.hpp \
ndbapi/NdbIndexScanOperation.hpp \
ndbapi/ndberror.h

mgmapiinclude_HEADERS = \
mgmapi/mgmapi.h \
mgmapi/mgmapi_debug.h

noinst_HEADERS = \
ndb_global.h \
ndb_net.h \
mgmapi/mgmapi_config_parameters.h \
mgmapi/mgmapi_config_parameters_debug.h

EXTRA_DIST = debugger editline kernel logger mgmcommon \
portlib transporter util

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`
