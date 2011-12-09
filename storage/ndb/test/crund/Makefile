#------------------------------------------------------------------------------

  SRC		= ./src

.PHONY:	all help dep dbg opt prf clean mostlclean clobber distclean check
all:	dbg

help:
	@echo "dep		-- build dependencies"
	@echo "all		-- build all targets with default flags"
	@echo "dbg		-- build all targets with debug flags"
	@echo "opt		-- build all targets with optimizing flags"
	@echo "prf		-- build all targets with profiling flags"
	@echo "clean		-- delete temporary files"
	@echo "mostlyclean	-- also delete all generated files"
	@echo "distclean	-- delete also database and server log files"
	@echo "check		-- perform self-tests (if any)"

dep dbg opt prf clean mostlyclean clobber check:
	ant $@
	@cd $(SRC)/crundndb && $(MAKE) $(MFLAGS) $@

distclean:
	rm -rf scripts/ndblog
	rm -f *~
	ant $@
	rm -f ./.ant-targets-build.xml
	@cd $(SRC)/crundndb && $(MAKE) $(MFLAGS) $@

#------------------------------------------------------------------------------

# read the generic settings
include	env.properties

# not sure if needed:
#	LD_LIBRARY_PATH=${NDB_LIBDIR}:$(SRC)/crundndb/Driver 
run.driver:	$(SRC)/crundndb/NdbApiDriver
	$(SRC)/crundndb/NdbApiDriver \
	-p crundRun.properties -p crundNdbapi.properties
#
#run.driver:
#	@cd $(SRC)/crundndb && $(MAKE) $(MFLAGS) $@

#------------------------------------------------------------------------------
