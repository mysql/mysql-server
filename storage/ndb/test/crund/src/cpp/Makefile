#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

#  API_PKG_PATH	= com/mysql/cluster

  DDEFINES 	=

  DELIVERABLES	= NdbapiDriver tws

  CLEAN		= core *.o

  MOSTLYCLEAN	= $(DELIVERABLES) *.dSYM *% log_*

  DISTCLEAN	= *~ *% */*~

  INCLUDES	= '$(JAVA_INCLUDEOPTS)' -I. -I"$(MARTINS_LITTLE_HELPERS)" \
		  '$(NDB_INCLUDEOPT)'

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
  LDLIBS	= -L$(NDB_LIBDIR) -lndbclient
# undefined symbols when linking with
#  LOADLIBES	= -L$(MARTINS_LITTLE_HELPERS) -lutils
# which seems to be the same as
#  LOADLIBES	= $(MARTINS_LITTLE_HELPERS)/libutils.dylib
# but linking statically is fine:
  LOADLIBES	= $(MARTINS_LITTLE_HELPERS)/libutils.a

#------------------------------------------------------------------------------

.PHONY:	all depend dep run.ndbapi grind.ndbapi run.tws grind.tws

all:	$(DELIVERABLES)

dep depend:

run.ndbapi:	NdbapiDriver
	./NdbapiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
grind.ndbapi:	NdbapiDriver
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./NdbapiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

NdbapiDriver:	Driver.o CrundDriver.o CrundNdbapiOperations.o

run.tws:	tws
	./tws \
	-p ../../twsRun.properties -p ../../crundNdbapi.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
grind.tws:	tws
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./TwsDriver \
	-p ../../twsRun.properties -p ../../crundNdbapi.properties

tws:	Driver.o TwsDriver.o NdbapiTwsDriver.o

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
