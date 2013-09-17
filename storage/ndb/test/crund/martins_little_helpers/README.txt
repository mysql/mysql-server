
Martin's Little Helpers
-----------------------

A collection of C++ utility classes useful for benchmarking:

- helpers.hpp           some tracing and debugging macros
- string_helpers.hpp    a few string convenience functions
- Properties.hpp        C++ implementation of java.util.Properties

- hrt_utils.h           high-resolution time measurement utilities
- hrt_stopwatch.h       high-resolution time stopwatch utility
- hrt_gstopwatch.h      global (=singleton) high-resolution time stopwatch

- HrtStopwatch.java     Java high-resolution time stopwatch utility
- HrtProfiler.java	simple utility for manual Java code instrumentation


How to Install:
---------------

0) Required: [g]make, JDK, C/C++ compiler

1) Edit the file ../env.properties

    Configure the property: JAVA_INCLUDEOPTS

2) Edit the file ./src/utils/Makefile

    Configure the macro: DDEFINES

    MacOS X:
    DDEFINES    = \
                 -DHRT_REALTIME_METHOD=HRT_USE_GETTIMEOFDAY \
                 -DHRT_CPUTIME_METHOD=HRT_USE_GETRUSAGE \

    Linux (try compiling with these methods for higher clock resolution):
    DDEFINES    = \
                 -DHRT_REALTIME_METHOD=HRT_USE_CLOCK_GETTIME \
                 -DHRT_CPUTIME_METHOD=HRT_USE_CLOCK_GETTIME \

    LDLIBS	= -lrt

3) Build the library

    Run once:
        $ make dep

    Build either optimized or debug objects:
        $ make opt
        $ make dbg

4) Run tests (optional)

        $ make check
   or
        $ cd src/utils/
        $ make run.Properties_test
        $ make run.hrt_utils_test
        $ make run.hrt_stopwatch_test
        $ make run.HrtStopwatchTest
