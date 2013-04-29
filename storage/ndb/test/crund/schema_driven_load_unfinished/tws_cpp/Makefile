#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  DDEFINES 	=

  DELIVERABLES	= TwsDriver 

  GENERATED_H	=

  CLEAN		= core *.o $(GENERATED_H)

  MOSTLYCLEAN	= $(DELIVERABLES) *.dSYM *% log_*

  DISTCLEAN	= *~ *% */*~

  INCLUDES	= -I. -I"$(MARTINS_LITTLE_HELPERS)"	\
		  $(NDB_INCLUDEOPT0) $(NDB_INCLUDEOPT1)

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
  LDLIBS	= -L${NDB_LIBDIR} -lndbclient
# XXX undefined symbols when linking with 
#  LOADLIBES	= -L$(MARTINS_LITTLE_HELPERS) -lutils
# XXX which seems to be the same as
#  LOADLIBES	= $(MARTINS_LITTLE_HELPERS)/libutils.dylib
# XXX but linking statically is fine:
#  LOADLIBES	= $(MARTINS_LITTLE_HELPERS)/libutils.a

#------------------------------------------------------------------------------

.PHONY:	all help depend dep dbg opt prf clean mostlyclean distclean \
	run.driver valgrind.driver

all:	$(DELIVERABLES)

help:
	@echo "build targets:"
	@echo "  dep            -- build the include dependencies"
	@echo "  dbg            -- build all targets with debug flags"
	@echo "  opt            -- build all targets with optimizing flags"
	@echo "  prf            -- build all targets with profiling flags"
	@echo ""
	@echo "clean targets:"
	@echo "  clean          -- delete temporary files"
	@echo "  mostlyclean    -- also delete all generated files"
	@echo "  distclean      -- delete also database and server log files"
	@echo ""
	@echo "run targets:"
	@echo "  run.driver     -- run the benchmark's driver program"
	@echo "  grind.driver   -- run the driver with valgrind memchecker"
	@echo ""

dep depend:	$(GENERATED_H)

run.driver:	TwsDriver
	./TwsDriver \
	-p ../run.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
grind.driver:	TwsDriver
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./TwsDriver \
	-p ../run.properties

TwsDriver:	Driver.o NdbApiTwsDriver.o

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
