#
# Invoked from scripts/make_binary_distribution as "sh BinDist.sh".
# Prints list of dirs and files to include under mysql/ndb.
#

# top dir

grep -v '^#' <<__END__
#ReleaseNotes.html
.defs.mk
Defs.mk
configure
Makefile
Epilogue.mk
SrcDist.sh
BinDist.sh
mysqlclusterenv.sh
__END__

# subset of bins, libs

grep -v '^#' <<__END__
bin/
bin/mysqlcluster
bin/mysqlcluster_install_db
bin/mysqlclusterd
lib/
__END__

# docs

#find docs/*.html docs/*.pdf -print | sort -t/

# include

find include -print | grep -v /SCCS | sort -t/

# config

find config -print | grep -v /SCCS | sort -t/

# tools

find tools -print | grep -v /SCCS | grep -v '\.o' | grep -v tools/ndbsql | sort -t/

# home

find home -print | grep -v /SCCS | sort -t/

# test

find test -print | grep -v /SCCS | grep -v '\.o' | grep -v test/odbc | sort -t/

# src

find src -print | grep -v /SCCS | grep -v '\.o' | grep -v src/client/odbc | grep -v cpcc-win32 | sort -t/

# demos

find demos -print | grep -v /SCCS | grep -v '\.o' | sort -t/

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
