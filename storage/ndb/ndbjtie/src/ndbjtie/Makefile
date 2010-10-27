#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  JAVA_SRC	= .

# preprocessing defines:
  DDEFINES 	= 

  DELIVERABLES	= $(JAVA_SRC)/test/NdbJtieTest.class \
		libndbjtie.a libndbjtie.$(DLL_SUFFIX)

  GENERATED_H	= ndbjtie_Ndb_cluster_connection.h \
		ndbjtie_Ndb.h \
		ndbjtie_NdbError.h \
		ndbjtie_NdbTransaction.h \
		ndbjtie_NdbDictionary.h \
		ndbjtie_NdbDictionary_Object.h \
		ndbjtie_NdbDictionary_Table.h

  CLEAN		= core *.o *.class $(JAVA_SRC)/*/*.class $(GENERATED_H)

  CLOBBER	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *% */*~

  SRCS		= *.cpp

# currently, not needed:
#  OBJECTS	= $(SRC:.c=.o)
#  OBJECTS	+= $(SRC:.cc=.o)
#  OBJECTS	+= $(SRC:.cpp=.o)

  INCLUDES	= -I. -I.. -I../jtie $(JAVA_INCLUDEOPTS) $(NDB_INCLUDEOPTS)

  CLASSPATH	= ".:../jtie"

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
  LDLIBS	= -L${NDB_LIBDIR} -lndbclient

#------------------------------------------------------------------------------

.PHONY:	all clean clobber depend dep

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

ndbjtie_Ndb_cluster_connection.h:	$(JAVA_SRC)/ndbjtie/Ndb_cluster_connection.class
	$(COMPILE.javah) ndbjtie.Ndb_cluster_connection

ndbjtie_Ndb.h:	$(JAVA_SRC)/ndbjtie/Ndb.class
	$(COMPILE.javah) ndbjtie.Ndb

ndbjtie_NdbError.h:	$(JAVA_SRC)/ndbjtie/NdbError.class
	$(COMPILE.javah) ndbjtie.NdbError

ndbjtie_NdbDictionary.h ndbjtie_NdbDictionary_Object.h ndbjtie_NdbDictionary_Table.h:	\
		$(JAVA_SRC)/ndbjtie/NdbDictionary.class
	$(COMPILE.javah) ndbjtie.NdbDictionary

ndbjtie_NdbTransaction.h:	$(JAVA_SRC)/ndbjtie/NdbTransaction.class
	$(COMPILE.javah) ndbjtie.NdbTransaction

$(JAVA_SRC)/test/NdbJtieTest.class:	$(JAVA_SRC)/test/NdbJtieTest.java \
		$(JAVA_SRC)/ndbjtie/Ndb_cluster_connection.java \
		$(JAVA_SRC)/ndbjtie/Ndb.java \
		$(JAVA_SRC)/ndbjtie/NdbError.java
	$(COMPILE.java) $<

libndbjtie.a:	libndbjtie.a(ndbjtie_classes.o)

#
# Important for libs like $(LDLIBS) to come last!
#

libndbjtie.so:	ndbjtie_NdbJtie.o ../myapi/libmyapi.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libndbjtie.so \
	ndbjtie_classes.o \
	-Wl,-rpath -Wl,. libmyapi.so

#  gcc -I/usr/local/java/include -I/usr/local/java/include/genunix -fPIC -c Happy.c HappyImp.c
#  gcc -shared -Wl,-soname,libhappy.so.1 -o libhappy.so.1.0 Happy.o HappyImp.o

libndbjtie.dylib:	libndbjtie.a ndbjtie_classes.o
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	-o libndbjtie.dylib \
	ndbjtie_classes.o \
	libndbjtie.a \
	$(LDLIBS)

# not needed:
#	${NDB_LIBDIR}/libndbclient.a

# doesn't seem to have an effect
#	-Wl,-rpath -Wl,".:../myapi" \
#	../myapi/libmyapi.dylib \

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libmyapi.so
# or
#	-L. \
#	-lmyapi \
# ldd libndbjtie.so
#       libmyapi.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/myapi \
# ldd libndbjtie.so
#        libmyapi.so => /home/md/martins_little_helpers/src/myapi/libmyapi.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libndbjtie.so
#        libmyapi.so => ./libmyapi.so (0xb7edd000)

# statically linked references:
#	libmyapi.a
# ldd libndbjtie.so
#       ...

.PHONY: run.test
run.test:	$(JAVA_SRC)/test/NdbJtieTest.class
	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea test.NdbJtieTest

#	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea -Djava.library.path=".:../myapi" test.NdbJtieTest

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
