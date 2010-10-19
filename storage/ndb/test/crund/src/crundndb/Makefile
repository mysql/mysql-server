#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

#  API_PKG_PATH	= com/mysql/cluster

  DDEFINES 	=

  DELIVERABLES	= libcrundndb.a libcrundndb.$(DLL_SUFFIX) NdbApiDriver 

  GENERATED_H	= com_mysql_cluster_crund_NdbApiLoad.h

  CLEAN		= core *.o $(GENERATED_H)

  MOSTLYCLEAN	= $(DELIVERABLES) *.dSYM *% log_*

  DISTCLEAN	= *~ *% */*~

  INCLUDES	= $(JAVA_INCLUDEOPTS) -I. -I"$(MARTINS_LITTLE_HELPERS)"	\
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
  LOADLIBES	= $(MARTINS_LITTLE_HELPERS)/libutils.a

#------------------------------------------------------------------------------

.PHONY:	all depend dep run.driver valgrind.driver

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

com_mysql_cluster_crund_NdbApiLoad.h:
	cd ../.. && ant dep

#com_mysql_cluster_NdbApiLoad.h:	$(API_PKG_PATH)/NdbApiLoad.class
#	$(COMPILE.javah) com.mysql.cluster.NdbApiLoad
#
#$(API_PKG_PATH)/NdbApiLoad.class:	\
#		$(API_PKG_PATH)/NdbApiLoad.java
#	$(COMPILE.java) $<

# not sure if needed:
#	LD_LIBRARY_PATH=${NDB_LIBDIR}:./Driver
run.driver:	NdbApiDriver
	./NdbApiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

# for JVM processes, try running valgrind with
# --smc-check=all --trace-children=yes --leak-check=full --show-reachable=yes
# not sure if needed:
#	LD_LIBRARY_PATH=${NDB_LIBDIR}:./Driver
valgrind.driver:	NdbApiDriver
	valgrind \
	--leak-check=full --show-reachable=yes --trace-children=yes \
	./NdbApiDriver \
	-p ../../crundRun.properties -p ../../crundNdbapi.properties

NdbApiDriver:	Driver.o CrundDriver.o libcrundndb.a

# can list multiple archive members: lib.a(a.o b.o ...) lib.a(c.o)...
libcrundndb.a:	libcrundndb.a(CrundNdbApiOperations.o)	\
		libcrundndb.a(com_mysql_cluster_crund_NdbApiLoad.o)

# XXX cleanup & refactorize this rule!
# LINK.o
#libcrundndb.so:	CrundNdbApiOperations.o com_mysql_cluster_crund_NdbApiLoad.o
#libcrundndb.so:	libcrundndb.a
libcrundndb.so:	CrundNdbApiOperations.cpp
	$(LINK.cpp) \
	-fPIC -shared \
	-L${NDB_LIBDIR} \
	-lndbclient \
	-o libcrundndb.so \
	CrundNdbApiOperations.cpp com_mysql_cluster_crund_NdbApiLoad.cpp

libcrundndb.dylib:	CrundNdbApiOperations.cpp
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	-L${NDB_LIBDIR} \
	-lndbclient \
	-o libcrundndb.dylib \
	CrundNdbApiOperations.cpp com_mysql_cluster_crund_NdbApiLoad.cpp

# Solaris cc?
# -#          <!-- verbose mode -->
# -G          <!-- create shared object -->
# cc \
# 	-# \
# 	-I. \
# 	-I${JAVA_HOME}/include \
# 	-I${JAVA_HOME}/include/solaris \
# 	-I${NDB_INCLUDE} \
# 	-I${NDB_INCLUDE}/ndbapi \
# 	-L${NDB_LIB} \
# 	-lndbclient \
# 	-G \
# 	-o libcrundndb.so \
# 	../src/crundndb/*.cpp

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
