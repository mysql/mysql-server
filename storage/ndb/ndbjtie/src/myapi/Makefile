#------------------------------------------------------------------------------

# read the generic settings
include	../../Makefile.defaults
include	../../env.properties

#------------------------------------------------------------------------------

# preprocessing defines:
  DDEFINES 	= 

  DELIVERABLES	= libmyapi.a libmyapi.$(DLL_SUFFIX) myapi_test

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

myapi_test:	libmyapi.a

libmyapi.a:	libmyapi.a(myapi.o)

libmyapi.so:	libmyapi.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libmyapi.so \
	myapi.o \
	libmyapi.a

libmyapi.dylib:	libmyapi.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	$(LDLIBS) \
	-o libmyapi.dylib \
	myapi.o \
	libmyapi.a

#	-Wl,-rpath -Wl,. libmyapi.dylib
# myapi.o hrt_stopwatch.o hrt_myapi.o

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libmyapi.so
# or
#	-L. \
#	-lmyapi \
# ldd libjnitest.so
#       libmyapi.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/myapi \
# ldd libjnitest.so
#        libmyapi.so => /home/md/martins_little_helpers/src/myapi/libmyapi.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libjnitest.so
#        libmyapi.so => ./libmyapi.so (0xb7edd000)

# statically linked references:
#	libmyapi.a
# ldd libjnitest.so
#       ...

dep depend:	

.PHONY: run.test
run.test:	myapi_test
	./myapi_test

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
# DO NOT DELETE
