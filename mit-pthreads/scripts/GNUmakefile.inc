VPATH := $(VPATH):${srcdir}/scripts
SCRIPTS= pgcc pg++

abspath := $(shell pwd)

all-bin: $(SCRIPTS)

pgcc: pgcc.sh
	sed -e 's!EXEC_PREFIX!$(exec_prefix)!g' \
        -e 's!BUILD_PREFIX!$(abspath)!g' \
        -e 's!SRC_PREFIX!$(srcdir)!g' \
	    -e 's!COMPILER!gcc!g' \
		< $(srcdir)/scripts/pgcc.sh > pgcc.new
	chmod a+x pgcc.new
	mv -f pgcc.new pgcc

pg++: pgcc.sh
	sed -e 's!EXEC_PREFIX!$(exec_prefix)!g' \
        -e 's!BUILD_PREFIX!$(abspath)!g' \
        -e 's!SRC_PREFIX!$(srcdir)!g' \
	    -e 's!COMPILER!g++!g' \
		< $(srcdir)/scripts/pgcc.sh > pg++.new
	chmod a+x pg++.new
	mv -f pg++.new pg++
