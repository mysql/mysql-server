SUBDIRS = src test tools .

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

EXTRA_DIST = include

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  if test "$$subdir" != "."; then \
	    files="`find $$subdir -name '*\.h'` `find $$subdir -name '*\.hpp'`"; \
	    for f in $$files; do \
	      if test -d "$(distdir)/`dirname $$f`" -a ! -e "$(distdir)/$$f"; then \
	        cp $$f $(distdir)/$$f; \
	      fi; \
	    done; \
	  fi; \
	done
