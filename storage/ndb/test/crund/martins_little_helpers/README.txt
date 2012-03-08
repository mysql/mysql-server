
Martin's Little Helpers
-----------------------

A collection of C++ utility classes and headers I found useful writing.

Sources and libs are currently all in src/utils :

- helpers.hpp           some tracing and debugging macros
- string_helpers.hpp    a few string convenience functions
- Properties.hpp        a C++ implementation of java.util.Properties

- hrt_utils.h           high-resolution time measurement utilities
- hrt_stopwatch.h       high-resolution time stopwatch utility
- hrt_gstopwatch.h      a global (=singleton) high-resolution time stopwatch

- HrtStopwatch.java     a Java high-resolution time stopwatch utility
- HrtProfiler.java	a crude utility for manual Java code instrumentation


How to Install:
---------------


0) Have Gnu gcc, gmake, and Sun's JDK (>= 1.5) installed


1) Edit the file ../env.properties

    Configure the property: JAVA_INCLUDEOPTS

    MacOS X:
    JAVA_INCLUDEOPTS=-I"/System/Library/Frameworks/JavaVM.framework/Versions/1.5.0/Headers"

    Linux:
    JAVA_INCLUDEOPTS=-I"/usr/lib/jvm/java-6-sun-1.6.0.07/include" -I"/usr/lib/jvm/java-6-sun-1.6.0.07/include/linux"


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

    Build either of optimized or debug objects:
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


Comments or questions appreciated: martin.zaun@oracle.com
