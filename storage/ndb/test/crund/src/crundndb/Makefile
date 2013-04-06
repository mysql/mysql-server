#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

#  API_PKG_PATH	= com/mysql/cluster

  DDEFINES 	=

  DELIVERABLES	= libcrundndb.a libcrundndb.$(DLL_SUFFIX) NdbapiDriver tws

  GENERATED_H	= com_mysql_cluster_crund_NdbapiLoad.h

  CLEAN		= core *.o $(GENERATED_H)

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

.PHONY:	all depend dep run.driver grind.driver run.tws grind.tws

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

com_mysql_cluster_crund_NdbapiLoad.h:
	cd ../.. && ant dep

#com_mysql_cluster_NdbapiLoad.h:	$(API_PKG_PATH)/NdbapiLoad.class
#	$(COMPILE.javah) com.mysql.cluster.NdbapiLoad
#
#$(API_PKG_PATH)/NdbapiLoad.class:	\
#		$(API_PKG_PATH)/NdbapiLoad.java
#	$(COMPILE.java) $<

# not sure if needed:
#	LD_LIBRARY_PATH=$(NDB_LIBDIR):./Driver
run.driver:	NdbapiDriver
	./NdbapiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
# not sure if needed:
#	LD_LIBRARY_PATH=$(NDB_LIBDIR):./Driver
grind.driver:	NdbapiDriver
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./NdbapiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

NdbapiDriver:	Driver.o CrundDriver.o libcrundndb.a

# can list multiple archive members: lib.a(a.o b.o ...) lib.a(c.o)...
libcrundndb.a:	libcrundndb.a(CrundNdbapiOperations.o)	\
		libcrundndb.a(com_mysql_cluster_crund_NdbapiLoad.o)

# XXX cleanup & refactorize this rule!
# LINK.o
#libcrundndb.so:	CrundNdbapiOperations.o com_mysql_cluster_crund_NdbapiLoad.o
#libcrundndb.so:	libcrundndb.a
libcrundndb.so:	CrundNdbapiOperations.cpp
	$(LINK.cpp) \
	-fPIC -shared \
	-L$(NDB_LIBDIR) \
	-lndbclient \
	-o libcrundndb.so \
	CrundNdbapiOperations.cpp com_mysql_cluster_crund_NdbapiLoad.cpp

libcrundndb.dylib:	CrundNdbapiOperations.cpp
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	-L$(NDB_LIBDIR) \
	-lndbclient \
	-o libcrundndb.dylib \
	CrundNdbapiOperations.cpp com_mysql_cluster_crund_NdbapiLoad.cpp

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
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
