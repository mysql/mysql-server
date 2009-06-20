#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  JAVA_SRC	= .

# preprocessing defines:
  DDEFINES 	= 

  DELIVERABLES	= $(JAVA_SRC)/test/MyJapiTest.class \
		libmyjapi.a libmyjapi.$(DLL_SUFFIX)

  GENERATED_H	= myjapi_MyJapi.h myjapi_MyJapiCtypes.h \
		myjapi_A.h myjapi_B0.h myjapi_B1.h

  CLEAN		= core *.o *.class $(JAVA_SRC)/*/*.class $(GENERATED_H)

  CLOBBER	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *% */*~

  SRCS		= *.cpp

# currently, not needed:
#  OBJECTS	= $(SRC:.c=.o)
#  OBJECTS	+= $(SRC:.cc=.o)
#  OBJECTS	+= $(SRC:.cpp=.o)

  INCLUDES	= -I. -I.. -I../myapi -I../jtie $(JAVA_INCLUDEOPTS)

  CLASSPATH	= ".:../jtie"

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
# CLOCK_GETTIME requires -lrt
  LDLIBS	= -L../myapi -lmyapi
#  LDLIBS	= -lrt

#------------------------------------------------------------------------------

.PHONY:	all clean clobber depend dep

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

myjapi_MyJapi.h:	$(JAVA_SRC)/myjapi/MyJapi.class
	$(COMPILE.javah) myjapi.MyJapi

myjapi_MyJapiCtypes.h:	$(JAVA_SRC)/myjapi/MyJapiCtypes.class
	$(COMPILE.javah) myjapi.MyJapiCtypes

myjapi_A.h:	$(JAVA_SRC)/myjapi/A.class
	$(COMPILE.javah) myjapi.A

myjapi_B0.h:	$(JAVA_SRC)/myjapi/B0.class
	$(COMPILE.javah) myjapi.B0

myjapi_B1.h:	$(JAVA_SRC)/myjapi/B1.class
	$(COMPILE.javah) myjapi.B1

$(JAVA_SRC)/test/MyJapiTest.class:	$(JAVA_SRC)/test/MyJapiTest.java \
		$(JAVA_SRC)/myjapi/MyJapi.java \
		$(JAVA_SRC)/myjapi/MyJapiCtypes.java \
		$(JAVA_SRC)/myjapi/A.java \
		$(JAVA_SRC)/myjapi/B0.java \
		$(JAVA_SRC)/myjapi/B1.java
	$(COMPILE.java) $<

libmyjapi.a:	libmyjapi.a(myjapi_MyJapi.o myjapi_MyJapiCtypes.o myjapi_classes.o)

#
# Important for libs like $(LDLIBS) to come last!
#

libmyjapi.so:	myjapi_MyJapi.o ../myapi/libmyapi.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libmyjapi.so \
	myjapi_MyJapi.o \
	-Wl,-rpath -Wl,. libmyapi.so

#  gcc -I/usr/local/java/include -I/usr/local/java/include/genunix -fPIC -c Happy.c HappyImp.c
#  gcc -shared -Wl,-soname,libhappy.so.1 -o libhappy.so.1.0 Happy.o HappyImp.o

libmyjapi.dylib:	libmyjapi.a myjapi_MyJapi.o myjapi_MyJapiCtypes.o myjapi_classes.o ../myapi/libmyapi.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	-o libmyjapi.dylib \
	myjapi_MyJapi.o \
	myjapi_MyJapiCtypes.o \
	myjapi_classes.o \
	libmyjapi.a \
	../myapi/libmyapi.a

#	$(LDLIBS)

# sufficient at runtime (statically linked):
#	../myapi/libmyapi.a

# not sufficient at runtime (apparently dynamically linked and no path info):
#	$(LDLIBS)

# doesn't seem to have an effect
#	-Wl,-rpath -Wl,".:../myapi" \
#	../myapi/libmyapi.dylib \

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libmyapi.so
# or
#	-L. \
#	-lmyapi \
# ldd libmyjapi.so
#       libmyapi.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/myapi \
# ldd libmyjapi.so
#        libmyapi.so => /home/md/martins_little_helpers/src/myapi/libmyapi.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libmyjapi.so
#        libmyapi.so => ./libmyapi.so (0xb7edd000)

# statically linked references:
#	libmyapi.a
# ldd libmyjapi.so
#       ...

.PHONY: run.test
run.test:	$(JAVA_SRC)/test/MyJapiTest.class
	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea test.MyJapiTest

#	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea -Djava.library.path=".:../myapi" test.MyJapiTest

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
