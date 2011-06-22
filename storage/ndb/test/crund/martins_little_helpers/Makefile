#------------------------------------------------------------------------------

  SRC		= ./src

.PHONY:	all help dep dbg opt prf clean mostlyclean distclean check
all:	dbg

help:
	@echo "dep		-- build dependencies"
	@echo "all		-- build all targets with default flags"
	@echo "dbg		-- build all targets with debug flags"
	@echo "opt		-- build all targets with optimizing flags"
	@echo "prf		-- build all targets with profiling flags"
	@echo "clean		-- delete temporary files"
	@echo "clobber		-- delete also log files"
	@echo "distclean	-- delete also database and backup files"
	@echo "check		-- perform self-tests (if any)"

dep dbg opt prf clean mostlyclean distclean check:
	@cd $(SRC)/utils && $(MAKE) $(MFLAGS) $@

#------------------------------------------------------------------------------
