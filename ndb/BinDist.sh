#
# Invoked from scripts/make_binary_distribution as "sh BinDist.sh".
# Prints list of dirs and files to include under mysql/ndb.
#

# release notes

grep -v '^#' <<__END__
#ReleaseNotes.html
mysqlclusterenv.sh
__END__

# subset of bins, libs, includes

grep -v '^#' <<__END__
bin/
bin/ndb
bin/mgmtsrvr
bin/mgmtclient
bin/mysqlcluster
bin/mysqlcluster_install_db
bin/mysqlclusterd
bin/restore
bin/ndb_rep
bin/desc
bin/flexBench
bin/select_all
bin/select_count
bin/delete_all
#bin/ndbsql
bin/drop_tab
bin/drop_index
bin/list_tables
bin/waiter
lib/
lib/libNEWTON_API.a
lib/libNEWTON_API.so
lib/libNDB_API.a
lib/libNDB_API.so
lib/libMGM_API.a
lib/libMGM_API.so
#lib/libNDB_ODBC.so
lib/libMGM_API_pic.a
lib/libNDB_API_pic.a
include/
include/ndb_types.h
include/ndb_version.h
include/mgmapi/
include/mgmapi/mgmapi.h
include/mgmapi/mgmapi_debug.h
include/ndbapi/
include/ndbapi/ndbapi_limits.h
include/ndbapi/AttrType.hpp
include/ndbapi/Ndb.hpp
include/ndbapi/NdbApi.hpp
include/ndbapi/NdbConnection.hpp
include/ndbapi/NdbCursorOperation.hpp
include/ndbapi/NdbDictionary.hpp
include/ndbapi/NdbError.hpp
include/ndbapi/NdbEventOperation.hpp
include/ndbapi/NdbIndexOperation.hpp
include/ndbapi/NdbOperation.hpp
include/ndbapi/NdbPool.hpp
include/ndbapi/NdbRecAttr.hpp
include/ndbapi/NdbReceiver.hpp
include/ndbapi/NdbResultSet.hpp
include/ndbapi/NdbScanFilter.hpp
include/ndbapi/NdbScanOperation.hpp
include/ndbapi/NdbSchemaCon.hpp
include/ndbapi/NdbSchemaOp.hpp
include/newtonapi/dba.h
include/newtonapi/defs/pcn_types.h
__END__

#if [ -f /usr/local/lib/libstdc++.a ]; then
#  cp /usr/local/lib/libstdc++.a lib/.
#  echo lib/libstdc++.a
#fi
#if [ -f /usr/local/lib/libstdc++.so.5 ]; then
#  cp /usr/local/lib/libstdc++.so.5 lib/.
#  echo lib/libstdc++.so.5
#fi
#if [ -f /usr/local/lib/libgcc_s.so.1 ]; then
#  cp /usr/local/lib/libgcc_s.so.1 lib/.
#  echo lib/libgcc_s.so.1
#fi

# docs

#find docs/*.html docs/*.pdf -print | sort -t/

# demos

find demos -print | grep -v /SCCS | sort -t/

# examples

grep -v '^#' <<__END__
examples/
examples/Makefile
examples/ndbapi_example1/
examples/ndbapi_example1/Makefile
examples/ndbapi_example1/ndbapi_example1.cpp
examples/ndbapi_example2/
examples/ndbapi_example2/Makefile
examples/ndbapi_example2/ndbapi_example2.cpp
examples/ndbapi_example3/
examples/ndbapi_example3/Makefile
examples/ndbapi_example3/ndbapi_example3.cpp
examples/ndbapi_example4/
examples/ndbapi_example4/Makefile
examples/ndbapi_example4/ndbapi_example4.cpp
examples/ndbapi_example5/
examples/ndbapi_example5/Makefile
examples/ndbapi_example5/ndbapi_example5.cpp
examples/select_all/
examples/select_all/Makefile
examples/select_all/select_all.cpp
__END__

exit 0
