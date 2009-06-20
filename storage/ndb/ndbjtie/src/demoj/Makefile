#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

  JAVA_SRC	= .

  DDEFINES 	= 

  DELIVERABLES	= $(JAVA_SRC)/test/DemojTest.class \
		libdemoj.a libdemoj.$(DLL_SUFFIX)

  GENERATED_H	= demoj_A.h

  CLEAN		= core *.o *.class $(JAVA_SRC)/*/*.class $(GENERATED_H)

  CLOBBER	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *% */*~

  SRCS		= *.cpp

  INCLUDES	= -I. -I.. -I../demo $(JAVA_INCLUDEOPTS)

  CLASSPATH	= "."

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
# CLOCK_GETTIME requires -lrt
  LDLIBS	= -L../demo -ldemo
#  LDLIBS	= -lrt

#------------------------------------------------------------------------------

.PHONY:	all clean clobber depend dep

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

demoj_A.h:	$(JAVA_SRC)/demoj/A.class
	$(COMPILE.javah) demoj.A

$(JAVA_SRC)/test/DemojTest.class:	$(JAVA_SRC)/test/DemojTest.java \
		$(JAVA_SRC)/demoj/A.java
	$(COMPILE.java) $<

libdemoj.a:	libdemoj.a(demoj_A.o)

#
# Important for libs like $(LDLIBS) to come last!
#

libdemoj.so:	demoj_Demoj.o ../demo/libdemo.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libdemoj.so \
	demoj_Demoj.o \
	-Wl,-rpath -Wl,. libdemo.so

#  gcc -I/usr/local/java/include -I/usr/local/java/include/genunix -fPIC -c Happy.c HappyImp.c
#  gcc -shared -Wl,-soname,libhappy.so.1 -o libhappy.so.1.0 Happy.o HappyImp.o

libdemoj.dylib:	libdemoj.a demoj_A.o ../demo/libdemo.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	-o libdemoj.dylib \
	demoj_A.o \
	libdemoj.a \
	../demo/libdemo.a

#	$(LDLIBS)

# sufficient at runtime (statically linked):
#	../demo/libdemo.a

# not sufficient at runtime (apparently dynamically linked and no path info):
#	$(LDLIBS)

# doesn't seem to have an effect
#	-Wl,-rpath -Wl,".:../demo" \
#	../demo/libdemo.dylib \

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libdemo.so
# or
#	-L. \
#	-ldemo \
# ldd libdemoj.so
#       libdemo.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/demo \
# ldd libdemoj.so
#        libdemo.so => /home/md/martins_little_helpers/src/demo/libdemo.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libdemoj.so
#        libdemo.so => ./libdemo.so (0xb7edd000)

# statically linked references:
#	libdemo.a
# ldd libdemoj.so
#       ...

.PHONY: run.test
run.test:	$(JAVA_SRC)/test/DemojTest.class
	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea test.DemojTest

#	$(JAVA) $(JAVAFLAGS) -classpath $(CLASSPATH) -ea -Djava.library.path=".:../demo" test.DemojTest

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
