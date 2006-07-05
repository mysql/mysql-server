
include $(top_srcdir)/ndb/config/common.mk.am

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
ndbapi/ndberror.h

mgmapiinclude_HEADERS = \
mgmapi/mgmapi.h \
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

# Don't update the files from bitkeeper
%::SCCS/s.%
