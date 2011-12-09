#------------------------------------------------------------------------------

# read the generic settings
include	../../../Makefile.defaults
include	../../../env.properties

#------------------------------------------------------------------------------

  API_PKG_PATH	= .
#  API_PKG_PATH	= utils

# Linux:
#		 -DHRT_REALTIME_METHOD=HRT_USE_CLOCK_GETTIME \
#		 -DHRT_CPUTIME_METHOD=HRT_USE_CLOCK_GETTIME \
# MacOS X:
#		 -DHRT_REALTIME_METHOD=HRT_USE_GETTIMEOFDAY \
#		 -DHRT_CPUTIME_METHOD=HRT_USE_GETRUSAGE \

  DDEFINES 	= \
		 -DHRT_REALTIME_METHOD=HRT_USE_GETTIMEOFDAY \
		 -DHRT_CPUTIME_METHOD=HRT_USE_GETRUSAGE \
		 #-DHRT_REALTIME_METHOD=HRT_USE_CLOCK_GETTIME \
		 #-DHRT_CPUTIME_METHOD=HRT_USE_CLOCK_GETTIME \
		 #-DHRT_REALTIME_METHOD=HRT_USE_GETTIMEOFDAY \
		 #-DHRT_CPUTIME_METHOD=HRT_USE_GETRUSAGE \
		 #-DHRT_REALTIME_METHOD=HRT_USE_TIMES \
		 #-DHRT_CPUTIME_METHOD=HRT_USE_TIMES \
		 #-DHRT_REALTIME_METHOD=HRT_USE_ANSI_TIME \
		 #-DHRT_CPUTIME_METHOD=HRT_USE_ANSI_CLOCK \
		 #-D_POSIX_C_SOURCE=199309 \
		 #-D_POSIX_C_SOURCE=200112 \
		 #-DHAVE_UNISTD_H \
		 #-DHAVE_BOOL \

# check source-level flags:
# _GNU_SOURCE
# _POSIX_SOURCE
# _XOPEN_SOURCE
# _POSIX_C_SOURCE
# _UNIX_STD

  DELIVERABLES	= libutils.a libutils.$(DLL_SUFFIX) utils.jar \
		$(TESTS) $(TESTS1)

  TESTS		= hrt_utils_test hrt_stopwatch_test Properties_test \
		$(API_PKG_PATH)/HrtStopwatch.class \
		$(API_PKG_PATH)/HrtProfiler.class \
		$(API_PKG_PATH)/HrtStopwatchTest.class

  TESTS1	= $(API_PKG_PATH)/JniInstrumentationTest.class \
		libjnitest.$(DLL_SUFFIX)

  GENERATED_H	= utils_HrtStopwatch.h utils_JniInstrumentationTest.h

  CLEAN		= core *.o $(API_PKG_PATH)/*.class $(GENERATED_H)

  MOSTLYCLEAN	= $(DELIVERABLES) *.dSYM

  DISTCLEAN	= *~ *% */*~

  INCLUDES	= $(JAVA_INCLUDEOPTS) -I.

  CLASSPATH	= ".."

# for library dependencies, LOADLIBES and LDLIBS have been used historically
# this convention is used sometimes: use
#   LDLIBS for project-wide libs (like -lc or -lm)
#   LOADLIBES for libs for the individual case
# check with non-gnu makes: use of LOADLIBES, LDLIBS
# CLOCK_GETTIME requires -lrt
  LDLIBS	=
#  LDLIBS	= -lrt

#------------------------------------------------------------------------------

.PHONY:	all depend dep

all:	$(DELIVERABLES)

dep depend:	$(GENERATED_H)

#------------------------------------------------------------------------------

utils.jar:	$(API_PKG_PATH)/HrtStopwatch.class \
		$(API_PKG_PATH)/HrtProfiler.class
	$(JAR) cvf $@ $?

utils_HrtStopwatch.h:	$(API_PKG_PATH)/HrtStopwatch.class
	$(COMPILE.javah) utils.HrtStopwatch

libutils.a:	libutils.a(hrt_utils.o hrt_stopwatch.o hrt_gstopwatch.o) \
	libutils.a(utils_HrtStopwatch.o)

libutils.so:	libutils.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libutils.so \
	utils_HrtStopwatch.o \
	libutils.a

libutils.dylib:	libutils.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	$(LDLIBS) \
	-o libutils.dylib \
	utils_HrtStopwatch.o \
	libutils.a

# hrt_gstopwatch.o hrt_stopwatch.o hrt_utils.o

#------------------------------------------------------------------------------

hrt_utils_test:		libutils.a

.PHONY: run.hrt_utils_test
run.hrt_utils_test:	hrt_utils_test
	./hrt_utils_test

hrt_stopwatch_test:	libutils.a

.PHONY: run.hrt_stopwatch_test
run.hrt_stopwatch_test:	hrt_stopwatch_test
	./hrt_stopwatch_test

Properties_test:	libutils.a

.PHONY: run.Properties_test
run.Properties_test:	Properties_test
	./Properties_test -p Properties_test.properties

#------------------------------------------------------------------------------

$(API_PKG_PATH)/HrtStopwatchTest.class:	$(API_PKG_PATH)/HrtStopwatchTest.java
	$(COMPILE.java) $<

.PHONY: run.HrtStopwatchTest
run.HrtStopwatchTest:	$(API_PKG_PATH)/HrtStopwatchTest.class
	$(JAVA) $(JAVAFLAGS) -classpath .. -Djava.library.path="." utils.HrtStopwatchTest

#------------------------------------------------------------------------------

$(API_PKG_PATH)/JniInstrumentationTest.class:	\
		$(API_PKG_PATH)/JniInstrumentationTest.java
	$(COMPILE.java) $<

#utils_JniInstrumentationTest.c:	utils_JniInstrumentationTest.h

utils_JniInstrumentationTest.h:	$(API_PKG_PATH)/JniInstrumentationTest.class
	$(COMPILE.javah) utils.JniInstrumentationTest

libjnitest.so:	utils_JniInstrumentationTest.o libutils.a
	$(LINK.cpp) \
	-fPIC -shared \
	$(LDLIBS) \
	-o libjnitest.so \
	utils_JniInstrumentationTest.o \
	-Wl,-rpath -Wl,. libutils.so

libjnitest.dylib:	utils_JniInstrumentationTest.o libutils.a
	$(LINK.cpp) \
	-fPIC -shared -dynamiclib \
	$(LDLIBS) \
	-o libjnitest.dylib \
	utils_JniInstrumentationTest.o \
	-Wl,-rpath -Wl,. libutils.dylib

# undefined references to be resolved by LD_LIBRARY_PATH:
#	libutils.so
# or
#	-L. \
#	-lutils \
# ldd libjnitest.so
#       libutils.so => not found

# symbolical references with default library path:
#
#	-Wl,-rpath -Wl,/home/md/martins_little_helpers/src/utils \
# ldd libjnitest.so
#        libutils.so => /home/md/martins_little_helpers/src/utils/libutils.so (0xb7f8d000)
#
#	-Wl,-rpath -Wl,. \
# ldd libjnitest.so
#        libutils.so => ./libutils.so (0xb7edd000)

# statically linked references:
#	libutils.a
# ldd libjnitest.so
#       ...

.PHONY: run.jitest
run.jitest:	$(API_PKG_PATH)/JniInstrumentationTest.class
	$(JAVA) $(JAVAFLAGS) -classpath .. -Djava.library.path="." utils.JniInstrumentationTest

#------------------------------------------------------------------------------

check:	run.Properties_test \
	run.hrt_utils_test run.hrt_stopwatch_test run.HrtStopwatchTest \
	run.jitest

#------------------------------------------------------------------------------

.depend:
	touch $@

# read local dependencies
-include	.depend

#------------------------------------------------------------------------------
