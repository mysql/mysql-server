#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  DDEFINES 	=

  DELIVERABLES	= crund crundAB tws

  CLEAN		= core *.o

  MOSTLYCLEAN	= $(DELIVERABLES) *.dSYM *% log_*

  DISTCLEAN	= *~ *% */*~

  INCLUDES	= -I. -I"$(MARTINS_LITTLE_HELPERS)" \
		  $(NDB_INCLUDEOPT0) $(NDB_INCLUDEOPT1)

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

.PHONY:	all depend dep run.crund run.crundAB grind.crund run.tws grind.tws

all:	$(DELIVERABLES)

dep depend:

run.crund run.c++crund:	crund
	./crund -p ../../crundRun.properties -p ../../crundNdb.properties

run.crundAB:	crundAB
	./crundAB -p ../../crundRun.properties -p ../../crundNdb.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
grind.crund:	crund
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./crund -p ../../crundRun.properties -p ../../crundNdb.properties

crund:	Driver.o CrundDriver.o CrundLoad.o NdbapiAB.o

crundAB:	Driver.o CrundDriver.o CrundLoad.o NdbapiAB.o

tws:	Driver.o TwsDriver.o NdbapiTwsDriver.o

run.tws:	tws
	./tws -p ../../twsRun.properties -p ../../crundNdb.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
grind.tws:	tws
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./tws -p ../../twsRun.properties -p ../../crundNdb.properties

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
