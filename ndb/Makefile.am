## find * -name '*.hpp' -print | grep -v SCCS | grep -v odbc | sed 's/\.hpp/\.hpp \\/' > tmp.out
## find * -name '*.h' -print | grep -v SCCS | grep -v odbc | sed 's/\.h/\.h \\/' >> tmp.out

SUBDIRS = . include src test tools

ndbinclude_HEADERS = \
include/ndb_types.h \
include/ndb_version.h

ndbapiinclude_HEADERS = \
include/ndbapi/ndbapi_limits.h \
include/ndbapi/Ndb.hpp \
include/ndbapi/NdbApi.hpp \
include/ndbapi/NdbConnection.hpp \
include/ndbapi/NdbCursorOperation.hpp \
include/ndbapi/NdbDictionary.hpp \
include/ndbapi/NdbError.hpp \
include/ndbapi/NdbEventOperation.hpp \
include/ndbapi/NdbIndexOperation.hpp \
include/ndbapi/NdbOperation.hpp \
include/ndbapi/NdbPool.hpp \
include/ndbapi/NdbRecAttr.hpp \
include/ndbapi/NdbReceiver.hpp \
include/ndbapi/NdbResultSet.hpp \
include/ndbapi/NdbScanFilter.hpp \
include/ndbapi/NdbScanOperation.hpp \
include/ndbapi/NdbSchemaCon.hpp \
include/ndbapi/NdbSchemaOp.hpp

mgmapiinclude_HEADERS = \
include/mgmapi/mgmapi.h \
include/mgmapi/mgmapi_debug.h

noinst_HEADERS =
