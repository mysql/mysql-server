#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

# preprocessing defines:
  DDEFINES 	= 

  DELIVERABLES	= libdemo.a libdemo.$(DLL_SUFFIX) demo_test

  CLEAN		= core *.o *.class

  CLOBBER	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *%

  SRCS		= *.cpp

# currently, not needed:
#  OBJECTS	= $(SRC:.c=.o)
#  OBJECTS	+= $(SRC:.cc=.o)
#  OBJECTS	+= $(SRC:.cpp=.o)

  INCLUDES	= -I. -I.. $(JAVA_INCLUDEOPTS)

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
# CLOCK_GETTIME requires -lrt
  LDLIBS	=
#  LDLIBS	= -lrt

#------------------------------------------------------------------------------

.PHONY:	all clean clobber depend dep

all:	$(DELIVERABLES)

demo_test:	libdemo.a

libdemo.a:	libdemo.a(demo.o)

libdemo.so:	libdemo.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libdemo.so \
	demo.o \
	libdemo.a

libdemo.dylib:	libdemo.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	$(LDLIBS) \
	-o libdemo.dylib \
	demo.o \
	libdemo.a

#	-Wl,-rpath -Wl,. libdemo.dylib
# demo.o hrt_stopwatch.o hrt_demo.o

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libdemo.so
# or
#	-L. \
#	-ldemo \
# ldd libjnitest.so
#       libdemo.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/demo \
# ldd libjnitest.so
#        libdemo.so => /home/md/martins_little_helpers/src/demo/libdemo.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libjnitest.so
#        libdemo.so => ./libdemo.so (0xb7edd000)

# statically linked references:
#	libdemo.a
# ldd libjnitest.so
#       ...

dep depend:	

.PHONY: run.test
run.test:	demo_test
	./demo_test

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
