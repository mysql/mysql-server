#------------------------------------------------------------------------------

  SRC		= ./src

.PHONY:	all help dep dbg opt prf clean clobber distclean
all:	dbg

help:
	@echo "dep		-- build dependencies"
	@echo "all		-- build all targets with default (debug) flags"
	@echo "dbg		-- build all targets with debug flags"
	@echo "opt		-- build all targets with optimizing flags"
	@echo "prf		-- build all targets with profiling flags"
	@echo "clean		-- delete temporary files"
	@echo "clobber		-- delete all temporary files"
	@echo "distclean	-- delete also backup files"

dep dbg opt prf clean clobber distclean:
	@cd $(SRC)/demo && $(MAKE) $(MFLAGS) $@
	@cd $(SRC)/demoj && $(MAKE) $(MFLAGS) $@
	@cd $(SRC)/myapi && $(MAKE) $(MFLAGS) $@
	@cd $(SRC)/jtie && $(MAKE) $(MFLAGS) $@
	@cd $(SRC)/myjapi && $(MAKE) $(MFLAGS) $@
	@cd $(SRC)/ndbjtie && $(MAKE) $(MFLAGS) $@

#------------------------------------------------------------------------------
