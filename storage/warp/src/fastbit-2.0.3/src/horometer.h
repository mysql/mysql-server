// $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_HOROMETER_H
#define IBIS_HOROMETER_H
#include <stdio.h>
#include <time.h> // clock, clock_gettime
#if defined(__sun) || defined(__linux__) || defined(__HOS_AIX__) || \
    defined(__CYGWIN__) || defined(__APPLE__) || defined(__FreeBSD__)
#   include <limits.h> // CLK_TCK
#   include <sys/time.h> // gettimeofday, timeval
#   include <sys/times.h> // times, struct tms
#   include <sys/resource.h> // getrusage
#   ifndef RUSAGE_SELF
#       define RUSAGE_SELF 0
#   endif
#   ifndef RUSAGE_CHILDREN
#       define RUSAGE_CHILDRED -1
#   endif
#elif defined(CRAY)
#   include <sys/times.h> // times
#elif defined(sgi)
#   include <limits.h> // CLK_TCK
#   define RUSAGE_SELF      0         /* calling process */
#   define RUSAGE_CHILDREN  -1        /* terminated child processes */
#   include <sys/times.h> // times
//#   include <sys/types.h> // struct tms
#   include <sys/time.h> // gettimeofday, getrusage
#   include <sys/resource.h> // getrusage
#elif defined(__MINGW32__)
#   include <limits.h> // CLK_TCK
#   include <sys/time.h> // gettimeofday, timeval
#elif defined(_WIN32)
#   include <windows.h>
#elif defined(VMS)
#   include <unistd.h>
#endif

/// @file Defines a simple timer class.  All functions are in this single
/// header file.
namespace ibis {
    class horometer;
}

/// \brief Horometer -- a primitive timing instrument.
///
/// This is intented to be a simple timer that measures a single duration.
/// It must be explicitly started by calling the function start.  The same
/// function start may be called to restart the timer which will discard
/// the previous starting point.  The function stop must be called before
/// functions realTime and CPUTime can report correct time values.  After a
/// horometer is stopped, it may continue by calling start to count a new
/// duration, or it may add to the existing duration by calling resume.
///
/// The timing accuracy depends on the underlying implementation.  On most
/// unix systems, the CPU time resolution is about 0.01 seconds, while the
/// elapsed time may be accurate to 0.0001 seconds.  The timing function
/// itself may take ~10,000 clock cycles to execute, which is about 25
/// microseconds on a 400 MHz machine.  This can become a significant source
/// of error if a timer is stopped and resumed at a high frequency.

class ibis::horometer {
public:
    horometer() : startRealTime(0), totalRealTime(0),
		  startCPUTime(0), totalCPUTime(0) {
#if defined(_WIN32) && defined(_MSC_VER)
	// the frequency of the high-resolution performance counter
	LARGE_INTEGER lFrequency;
	BOOL ret = QueryPerformanceFrequency(&lFrequency);
	if (ret != 0 && lFrequency.QuadPart != 0)
	    countPeriod = 1.0/static_cast<double>(lFrequency.QuadPart);
	else
	    countPeriod = 0.0;
#endif
    };
    /// Start the timer.  Clear the internal counters.
    void start() {
	startRealTime = readWallClock();
	startCPUTime = readCPUClock();
	totalRealTime = 0.0;
	totalCPUTime = 0.0;
    };
    /// Stop the timer.  Record the duration.  May resume later.
    void stop() {
	double tmpr = readWallClock() - startRealTime;
	double tmpc = readCPUClock() - startCPUTime;
	if (tmpr > 0.0)
	    totalRealTime += tmpr;
	if (tmpc > 0.0)
	    totalCPUTime += tmpc;
    };
    /// Continue after being stopped.
    void resume() {
	startRealTime = readWallClock();
	startCPUTime = readCPUClock();
    }
    /// Return the elapsed time in seconds.
    double realTime() const {return totalRealTime;}
    /// Return the CPU time in seconds.
    double CPUTime() const {return totalCPUTime;}

private:
    double startRealTime;   // wall clock start time
    double totalRealTime;   // total real time
    double startCPUTime;    // cpu start time
    double totalCPUTime;    // total cpu time
#if defined(_WIN32) && defined(_MSC_VER)
    double countPeriod;     // time of one high-resolution count
#endif

    inline double readWallClock();
    inline double readCPUClock();
};

/// Read the system's wallclock timer.  It tries to use clock_gettime if it
/// is available, otherwise it falls back to gettimeofday and clock.
inline double ibis::horometer::readWallClock() {
#if defined(CLOCK_MONOTONIC) && !defined(__CYGWIN__)
    struct timespec tb;
    if (0 == clock_gettime(CLOCK_MONOTONIC, &tb)) {
	return static_cast<double>(tb.tv_sec) + (1e-9 * tb.tv_nsec);
    }
    else {
	struct timeval cpt;
	gettimeofday(&cpt, 0);
	return static_cast<double>(cpt.tv_sec) + (1e-6 * cpt.tv_usec);
    }
#elif defined(HAVE_GETTIMEOFDAY) || defined(__unix__) || defined(CRAY) || \
    defined(__linux__) || defined(__HOS_AIX__) || defined(__APPLE__) || \
    defined(__FreeBSD__)
    struct timeval cpt;
    gettimeofday(&cpt, 0);
    return static_cast<double>(cpt.tv_sec) + (1e-6 * cpt.tv_usec);
#elif defined(_WIN32) && defined(_MSC_VER)
    double ret = 0.0;
    if (countPeriod != 0) {
	LARGE_INTEGER cnt;
	if (QueryPerformanceCounter(&cnt)) {
	    ret = countPeriod * cnt.QuadPart;
	}
    }
    if (ret == 0.0) { // fallback option -- use GetSystemTime
	union {
	    FILETIME ftFileTime;
	    __int64  ftInt64;
	} ftRealTime;
	GetSystemTimeAsFileTime(&ftRealTime.ftFileTime);
	ret = (double) ftRealTime.ftInt64 * 1e-7;
    }
    return ret;
#elif defined(VMS)
    return (double) clock() * 0.001;
#else
    return (double) clock() / CLOCKS_PER_SEC;
#endif
} //  ibis::horometer::readWallClock

/// Read the CPU timer.  It tries to use getrusage first, if not available,
/// it falls back to times and clock.
inline double ibis::horometer::readCPUClock() {
#if defined(__sun) || defined(sgi) || defined(__linux__) || defined(__APPLE__) \
    || defined(__HOS_AIX__) || defined(__CYGWIN__) || defined(__FreeBSD__)
    // on sun and linux, we can access getrusage to get more accurate time
    double time=0;
    struct rusage ruse;
    if (0 == getrusage(RUSAGE_SELF, &ruse)) {
	time = (ruse.ru_utime.tv_usec + ruse.ru_stime.tv_usec) * 1e-6 +
	    ruse.ru_utime.tv_sec + ruse.ru_stime.tv_sec;
    }
    else {
	fputs("Warning -- horometer::readCPUClock(): getrusage failed "
	      "on RUSAGE_SELF", stderr);
    }
    if (0 == getrusage(RUSAGE_CHILDREN, &ruse)) {
	time += (ruse.ru_utime.tv_usec + ruse.ru_stime.tv_usec) * 1e-6 +
	    ruse.ru_utime.tv_sec + ruse.ru_stime.tv_sec;
    }
    else {
	fputs("Warning -- horometer::readCPUClock(): getrusage failed on "
	      "RUSAGE_CHILDRED", stderr);
    }
    return time;
#elif defined(__unix__) || defined(CRAY)
#if defined(__STDC__)
    struct tms cpt;
    times(&cpt);
    return (cpt.tms_utime + cpt.tms_stime + cpt.tms_cutime +
	    (double)cpt.tms_cstime) / CLK_TCK;
#else
    return (double) times() / CLK_TCK;
#endif
#elif defined(_WIN32)
    return (double) clock() / CLOCKS_PER_SEC;
#elif defined(VMS)
    return (double) clock() * 0.001;
#else
    return (double) clock() / CLOCKS_PER_SEC;
#endif
} // ibis::horometer::readCPUClock
#endif // IBIS_HOROMETER_H
