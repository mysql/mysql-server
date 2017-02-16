/* Copyright (c) 2006, 2010, Oracle and/or its affiliates.
   Copyright (c) 2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

   Library for providing TAP support for testing C and C++ was written
   by Mats Kindahl <mats@mysql.com>.
*/

#include "tap.h"

#include "my_global.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static ulong start_timer(void);
static void end_timer(ulong start_time,char *buff);
static void nice_time(double sec,char *buff,my_bool part_second);

/*
  Visual Studio 2003 does not know vsnprintf but knows _vsnprintf.
  We don't put this #define elsewhere because we prefer my_vsnprintf
  everywhere instead, except when linking with libmysys is not
  desirable - the case here.
*/
#if defined(_MSC_VER) && ( _MSC_VER == 1310 )
#define vsnprintf _vsnprintf
#endif

/**
   @defgroup MyTAP_Internal MyTAP Internals

   Internal functions and data structures for the MyTAP implementation.
*/

/**
   Test data structure.

   Data structure containing all information about the test suite.

   @ingroup MyTAP_Internal
 */
static TEST_DATA g_test = { NO_PLAN, 0, 0, "" };

/**
   Output stream for test report message.

   The macro is just a temporary solution.

   @ingroup MyTAP_Internal
 */
#define tapout stdout

/**
  Emit the beginning of a test line, that is: "(not) ok", test number,
  and description.

  To emit the directive, use the emit_dir() function

  @ingroup MyTAP_Internal

  @see emit_dir

  @param pass  'true' if test passed, 'false' otherwise
  @param fmt   Description of test in printf() format.
  @param ap    Vararg list for the description string above.
 */
static void
vemit_tap(int pass, char const *fmt, va_list ap)
{
  fprintf(tapout, "%sok %d%s",
          pass ? "" : "not ",
          ++g_test.last,
          (fmt && *fmt) ? " - " : "");
  if (fmt && *fmt)
    vfprintf(tapout, fmt, ap);
  fflush(tapout);
}


/**
   Emit a TAP directive.

   TAP directives are comments after that have the form:

   @code
   ok 1 # skip reason for skipping
   not ok 2 # todo some text explaining what remains
   @endcode

   @ingroup MyTAP_Internal

   @param dir  Directive as a string
   @param why  Explanation string
 */
static void
emit_dir(const char *dir, const char *why)
{
  fprintf(tapout, " # %s %s", dir, why);
  fflush(tapout);
}


/**
   Emit a newline to the TAP output stream.

   @ingroup MyTAP_Internal
 */
static void
emit_endl()
{
  fprintf(tapout, "\n");
  fflush(tapout);
}

static void
handle_core_signal(int signo)
{
  BAIL_OUT("Signal %d thrown\n", signo);
}

void
BAIL_OUT(char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(tapout, "Bail out! ");
  vfprintf(tapout, fmt, ap);
  diag("%d tests planned,  %d failed,  %d was last executed",
       g_test.plan, g_test.failed, g_test.last);
  emit_endl();
  va_end(ap);
  exit(255);
}


void
diag(char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(tapout, "# ");
  vfprintf(tapout, fmt, ap);
  emit_endl();
  va_end(ap);
}

typedef struct signal_entry {
  int signo;
  void (*handler)(int);
} signal_entry;

static signal_entry install_signal[]= {
  { SIGINT,  handle_core_signal },
  { SIGQUIT, handle_core_signal },
  { SIGILL,  handle_core_signal },
  { SIGABRT, handle_core_signal },
  { SIGFPE,  handle_core_signal },
  { SIGSEGV, handle_core_signal }
#ifdef SIGBUS
  , { SIGBUS,  handle_core_signal }
#endif
#ifdef SIGXCPU
  , { SIGXCPU, handle_core_signal }
#endif
#ifdef SIGXCPU
  , { SIGXFSZ, handle_core_signal }
#endif
#ifdef SIGXCPU
  , { SIGSYS,  handle_core_signal }
#endif
#ifdef SIGXCPU
  , { SIGTRAP, handle_core_signal }
#endif
};

int skip_big_tests= 1;
ulong start_time= 0;

void
plan(int count)
{
  char *config= getenv("MYTAP_CONFIG");
  size_t i;

  start_time= start_timer();

  if (config)
    skip_big_tests= strcmp(config, "big");

  setvbuf(tapout, 0, _IONBF, 0);  /* provide output at once */
  /*
    Install signal handler
  */

  for (i= 0; i < sizeof(install_signal)/sizeof(*install_signal); ++i)
    signal(install_signal[i].signo, install_signal[i].handler);

  g_test.plan= count;
  switch (count)
  {
  case NO_PLAN:
    break;
  default:
    if (count > 0)
    {
      fprintf(tapout, "1..%d\n", count);
      fflush(tapout);
    }
    break;
  }
}


void
skip_all(char const *reason, ...)
{
  va_list ap;
  va_start(ap, reason);
  fprintf(tapout, "1..0 # skip ");
  vfprintf(tapout, reason, ap);
  fflush(tapout);
  va_end(ap);
  exit(0);
}

void
ok(int pass, char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  if (!pass && *g_test.todo == '\0')
    ++g_test.failed;

  vemit_tap(pass, fmt, ap);
  va_end(ap);
  if (*g_test.todo != '\0')
    emit_dir("todo", g_test.todo);
  emit_endl();
}

void
ok1(int const pass)
{
  va_list ap;

  memset(&ap, 0, sizeof(ap));

  if (!pass && *g_test.todo == '\0')
    ++g_test.failed;

  vemit_tap(pass, NULL, ap);

  if (*g_test.todo != '\0')
    emit_dir("todo", g_test.todo);

  emit_endl();
}

void
skip(int how_many, char const * const fmt, ...)
{
  char reason[80];
  if (fmt && *fmt)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(reason, sizeof(reason), fmt, ap);
    va_end(ap);
  }
  else
    reason[0] = '\0';

  while (how_many-- > 0)
  {
    va_list ap;
    memset((char*) &ap, 0, sizeof(ap));         /* Keep compiler happy */
    vemit_tap(1, NULL, ap);
    emit_dir("skip", reason);
    emit_endl();
  }
}


void
todo_start(char const *message, ...)
{
  va_list ap;
  va_start(ap, message);
  vsnprintf(g_test.todo, sizeof(g_test.todo), message, ap);
  va_end(ap);
}

void
todo_end()
{
  *g_test.todo = '\0';
}

int exit_status()
{
  char buff[60];

  /*
    If there were no plan, we write one last instead.
  */
  if (g_test.plan == NO_PLAN)
    plan(g_test.last);

  if (g_test.plan != g_test.last)
  {
    diag("%d tests planned but%s %d executed",
         g_test.plan, (g_test.plan > g_test.last ? " only" : ""), g_test.last);
    return EXIT_FAILURE;
  }

  if (g_test.failed > 0)
  {
    diag("Failed %d tests!", g_test.failed);
    return EXIT_FAILURE;
  }
  if (start_time)
  {
    end_timer(start_time, buff);
    printf("Test took %s\n", buff);
    fflush(stdout);
  }

  return EXIT_SUCCESS;
}

#if defined(__WIN__) || defined(__NETWARE__)
#include <time.h>
#else
#include <sys/times.h>
#ifdef _SC_CLK_TCK				// For mit-pthreads
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
#endif
#endif

static ulong start_timer(void)
{
#if defined(__WIN__) || defined(__NETWARE__)
 return clock();
#else
  struct tms tms_tmp;
  return times(&tms_tmp);
#endif
}


/** 
  Write as many as 52+1 bytes to buff, in the form of a legible
  duration of time.

  len("4294967296 days, 23 hours, 59 minutes, 60.00 seconds")  ->  52
*/

static void nice_time(double sec,char *buff, my_bool part_second)
{
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) (sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff+= sprintf(buff, "%ld %s", tmp, tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) (sec/3600.0);
    sec-=3600.0*tmp;
    buff+= sprintf(buff, "%ld %s", tmp, tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) (sec/60.0);
    sec-=60.0*tmp;
    buff+= sprintf(buff, "%ld min ", tmp);
  }
  if (part_second)
    sprintf(buff,"%.2f sec",sec);
  else
    sprintf(buff,"%d sec",(int) sec);
}


static void end_timer(ulong start_time,char *buff)
{
  nice_time((double) (start_timer() - start_time) /
	    CLOCKS_PER_SEC,buff,1);
}


/**
   @mainpage Testing C and C++ using MyTAP

   @section IntroSec Introduction

   Unit tests are used to test individual components of a system. In
   contrast, functional tests usually test the entire system.  The
   rationale is that each component should be correct if the system is
   to be correct.  Unit tests are usually small pieces of code that
   tests an individual function, class, a module, or other unit of the
   code.

   Observe that a correctly functioning system can be built from
   "faulty" components.  The problem with this approach is that as the
   system evolves, the bugs surface in unexpected ways, making
   maintenance harder.

   The advantages of using unit tests to test components of the system
   are several:

   - The unit tests can make a more thorough testing than the
     functional tests by testing correctness even for pathological use
     (which shouldn't be present in the system).  This increases the
     overall robustness of the system and makes maintenance easier.

   - It is easier and faster to find problems with a malfunctioning
     component than to find problems in a malfunctioning system.  This
     shortens the compile-run-edit cycle and therefore improves the
     overall performance of development.

   - The component has to support at least two uses: in the system and
     in a unit test.  This leads to more generic and stable interfaces
     and in addition promotes the development of reusable components.

   For example, the following are typical functional tests:
   - Does transactions work according to specifications?
   - Can we connect a client to the server and execute statements?

   In contrast, the following are typical unit tests:

   - Can the 'String' class handle a specified list of character sets?
   - Does all operations for 'my_bitmap' produce the correct result?
   - Does all the NIST test vectors for the AES implementation encrypt
     correctly?


   @section UnitTest Writing unit tests

   The purpose of writing unit tests is to use them to drive component
   development towards a solution that passes the tests.  This means that the
   unit tests has to be as complete as possible, testing at least:

   - Normal input
   - Borderline cases
   - Faulty input
   - Error handling
   - Bad environment

   @subsection NormalSubSec Normal input

   This is to test that the component have the expected behaviour.
   This is just plain simple: test that it works.  For example, test
   that you can unpack what you packed, adding gives the sum, pincing
   the duck makes it quack.

   This is what everybody does when they write tests.


   @subsection BorderlineTests Borderline cases

   If you have a size anywhere for your component, does it work for
   size 1? Size 0? Sizes close to <code>UINT_MAX</code>?

   It might not be sensible to have a size 0, so in this case it is
   not a borderline case, but rather a faulty input (see @ref
   FaultyInputTests).


   @subsection FaultyInputTests Faulty input

   Does your bitmap handle 0 bits size? Well, it might not be designed
   for it, but is should <em>not</em> crash the application, but
   rather produce an error.  This is called defensive programming.

   Unfortunately, adding checks for values that should just not be
   entered at all is not always practical: the checks cost cycles and
   might cost more than it's worth.  For example, some functions are
   designed so that you may not give it a null pointer.  In those
   cases it's not sensible to pass it <code>NULL</code> just to see it
   crash.

   Since every experienced programmer add an <code>assert()</code> to
   ensure that you get a proper failure for the debug builds when a
   null pointer passed (you add asserts too, right?), you will in this
   case instead have a controlled (early) crash in the debug build.


   @subsection ErrorHandlingTests Error handling

   This is testing that the errors your component is designed to give
   actually are produced.  For example, testing that trying to open a
   non-existing file produces a sensible error code.


   @subsection BadEnvironmentTests Environment

   Sometimes, modules has to behave well even when the environment
   fails to work correctly.  Typical examples are when the computer is
   out of dynamic memory or when the disk is full.  You can emulate
   this by replacing, e.g., <code>malloc()</code> with your own
   version that will work for a while, but then fail.  Some things are
   worth to keep in mind here:

   - Make sure to make the function fail deterministically, so that
     you really can repeat the test.

   - Make sure that it doesn't just fail immediately.  The unit might
     have checks for the first case, but might actually fail some time
     in the near future.


   @section UnitTest How to structure a unit test

   In this section we will give some advice on how to structure the
   unit tests to make the development run smoothly.  The basic
   structure of a test is:

   - Plan
   - Test
   - Report


   @subsection TestPlanning Plan the test

   Planning the test means telling how many tests there are.  In the
   event that one of the tests causes a crash, it is then possible to
   see that there are fewer tests than expected, and print a proper
   error message.

   To plan a test, use the @c plan() function in the following manner:

   @code
   int main(int argc, char *argv[])
   {
     plan(5);
         .
         .
         .
   }
   @endcode

   If you don't call the @c plan() function, the number of tests
   executed will be printed at the end.  This is intended to be used
   while developing the unit and you are constantly adding tests.  It
   is not indented to be used after the unit has been released.


   @subsection TestRunning Execute the test

   To report the status of a test, the @c ok() function is used in the
   following manner:

   @code
   int main(int argc, char *argv[])
   {
     plan(5);
     ok(ducks == paddling_ducks,
        "%d ducks did not paddle", ducks - paddling_ducks);
             .
             .
             .
   }
   @endcode

   This will print a test result line on the standard output in TAP
   format, which allows TAP handling frameworks (like Test::Harness)
   to parse the status of the test.

   @subsection TestReport  Report the result of the test

   At the end, a complete test report should be written, with some
   statistics. If the test returns EXIT_SUCCESS, all tests were
   successfull, otherwise at least one test failed.

   To get a TAP complient output and exit status, report the exit
   status in the following manner:

   @code
   int main(int argc, char *argv[])
   {
     plan(5);
     ok(ducks == paddling_ducks,
        "%d ducks did not paddle", ducks - paddling_ducks);
             .
             .
             .
     return exit_status();
   }
   @endcode

   @section DontDoThis Ways to not do unit testing

   In this section, we'll go through some quite common ways to write
   tests that are <em>not</em> a good idea.

   @subsection BreadthFirstTests Doing breadth-first testing

   If you're writing a library with several functions, don't test all
   functions using size 1, then all functions using size 2, etc.  If a
   test for size 42 fails, you have no easy way of tracking down why
   it failed.

   It is better to concentrate on getting one function to work at a
   time, which means that you test each function for all sizes that
   you think is reasonable.  Then you continue with the next function,
   doing the same. This is usually also the way that a library is
   developed (one function at a time) so stick to testing that is
   appropriate for now the unit is developed.

   @subsection JustToBeSafeTest Writing unnecessarily large tests

   Don't write tests that use parameters in the range 1-1024 unless
   you have a very good reason to belive that the component will
   succeed for 562 but fail for 564 (the numbers picked are just
   examples).

   It is very common to write extensive tests "just to be safe."
   Having a test suite with a lot of values might give you a warm
   fuzzy feeling, but it doesn't really help you find the bugs.  Good
   tests fail; seriously, if you write a test that you expect to
   succeed, you don't need to write it.  If you think that it
   <em>might</em> fail, <em>then</em> you should write it.

   Don't take this as an excuse to avoid writing any tests at all
   "since I make no mistakes" (when it comes to this, there are two
   kinds of people: those who admit they make mistakes, and those who
   don't); rather, this means that there is no reason to test that
   using a buffer with size 100 works when you have a test for buffer
   size 96.

   The drawback is that the test suite takes longer to run, for little
   or no benefit.  It is acceptable to do a exhaustive test if it
   doesn't take too long to run and it is quite common to do an
   exhaustive test of a function for a small set of values.
   Use your judgment to decide what is excessive: your milage may
   vary.
*/

/**
   @example simple.t.c

   This is an simple example of how to write a test using the
   library.  The output of this program is:

   @code
   1..1
   # Testing basic functions
   ok 1 - Testing gcs()
   @endcode

   The basic structure is: plan the number of test points using the
   plan() function, perform the test and write out the result of each
   test point using the ok() function, print out a diagnostics message
   using diag(), and report the result of the test by calling the
   exit_status() function.  Observe that this test does excessive
   testing (see @ref JustToBeSafeTest), but the test point doesn't
   take very long time.
*/

/**
   @example todo.t.c

   This example demonstrates how to use the <code>todo_start()</code>
   and <code>todo_end()</code> function to mark a sequence of tests to
   be done.  Observe that the tests are assumed to fail: if any test
   succeeds, it is considered a "bonus".
*/

/**
   @example skip.t.c

   This is an example of how the <code>SKIP_BLOCK_IF</code> can be
   used to skip a predetermined number of tests. Observe that the
   macro actually skips the following statement, but it's not sensible
   to use anything than a block.
*/

/**
   @example skip_all.t.c

   Sometimes, you skip an entire test because it's testing a feature
   that doesn't exist on the system that you're testing. To skip an
   entire test, use the <code>skip_all()</code> function according to
   this example.
 */
