.PATH : ${srcdir}/scripts
SCRIPTS= pgcc pg++

abspath != pwd

all-bin: $(SCRIPTS)

#
# Objects go in the obj directory for both BSD and GNU make but these
# scripts get put in the obj dir for BSD and the root dir for GNU.
#
pgcc: pgcc.sh
	sed -e 's!EXEC_PREFIX!$(exec_prefix)!g' \
        -e 's!BUILD_PREFIX!$(.CURDIR)!g' \
        -e 's!SRC_PREFIX!$(srcdir)!g' \
	    -e 's!COMPILER!gcc!g' \
		< $(srcdir)/scripts/pgcc.sh > pgcc.new
	chmod a+x pgcc.new
	mv -f pgcc.new pgcc
	ln -fs obj/pgcc ../pgcc

pg++: pgcc.sh
	sed -e 's!EXEC_PREFIX!$(exec_prefix)!g' \
        -e 's!BUILD_PREFIX!$(.CURDIR)!g' \
        -e 's!SRC_PREFIX!$(srcdir)!g' \
	    -e 's!COMPILER!g++!g' \
		< $(srcdir)/scripts/pgcc.sh > pg++.new
	chmod a+x pg++.new
	mv -f pg++.new pg++
	ln -fs obj/pg++ ../pg++
