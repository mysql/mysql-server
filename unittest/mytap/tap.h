/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   Library for providing TAP support for testing C and C++ was written
   by Mats Kindahl <mats@mysql.com>.
*/

#ifndef TAP_H
#define TAP_H

/*
  @defgroup MyTAP MySQL support for performing unit tests according to TAP.

*/

#define NO_PLAN  (0)

/**
   Data about test plan.

   @internal We are using the "typedef struct X { ... } X" idiom to
   create class/struct X both in C and C++.
 */
typedef struct TEST_DATA {
  /**
     Number of tests that is planned to execute.

     Can be zero (<code>NO_PLAN</code>) meaning that the plan string
     will be printed at the end of test instead.
  */
  int plan;

  /** Number of last test that was done or skipped. */
  int last;

  /** Number of tests that failed. */
  int failed;

  /** Todo reason. */
  char todo[128];
} TEST_DATA;

#ifdef __cplusplus
extern "C" {
#endif

/**
   Set number of tests that is planned to execute.

   The function also accepts the predefined constant
   <code>NO_PLAN</code>.  If the function is not called, it is as if
   it was called with <code>NO_PLAN</code>, i.e., the test plan will
   be printed after all the test lines.

   The plan() function will install signal handlers for all signals
   that generate a core, so if you want to override these signals, do
   it <em>after</em> you have called the plan() function.

   @param count The planned number of tests to run. 
*/
void plan(int count);


/**
   Report test result as a TAP line.

   Function used to write status of an individual test.  Call this
   function in the following manner:

   @code
   ok(ducks == paddling,
      "%d ducks did not paddle", ducks - paddling);
   @endcode

   @param pass Zero if the test failed, non-zero if it passed.
   @param fmt  Format string in printf() format. NULL is allowed, in
               which case nothing is printed.
*/
void ok(int pass, char const *fmt, ...)
  __attribute__((format(printf,2,3)));

/**
   Skip a determined number of tests.

   Function to print that <em>how_many</em> tests have been
   skipped.  The reason is printed for each skipped test.  Observe
   that this function does not do the actual skipping for you, it just
   prints information that tests have been skipped.  It shall be used
   in the following manner:

   @code
   if (ducks == 0) {
     skip(2, "No ducks in the pond");
   } else {
      int i;
      for (i = 0 ; i < 2 ; ++i)
        ok(duck[i] == paddling, "is duck %d paddling?", i);
   }
   @endcode

   @see SKIP_BLOCK_IF

   @param how_many   Number of tests that are to be skipped.
   @param reason     A reason for skipping the tests
 */
void skip(int how_many, char const *reason, ...)
    __attribute__((format(printf,2,3)));


/**
   Helper macro to skip a block of code.  The macro can be used to
   simplify conditionally skipping a block of code.  It is used in the
   following manner:

   @code
   SKIP_BLOCK_IF(ducks == 0, 2, "No ducks in the pond")
   {
     int i;
     for (i = 0 ; i < 2 ; ++i)
       ok(duck[i] == paddling, "is duck %d paddling?", i);
   }
   @endcode

   @see skip
 */
#define SKIP_BLOCK_IF(SKIP_IF_TRUE, COUNT, REASON) \
  if (SKIP_IF_TRUE) skip((COUNT),(REASON)); else

/**
   Print a diagnostics message.

   @param fmt  Diagnostics message in printf() format.
 */
void diag(char const *fmt, ...)
  __attribute__((format(printf,1,2)));

/**
   Print a bail out message.

   A bail out message can be issued when no further testing can be
   done, e.g., when there are missing dependencies.

   The test will exit with status 255.  This function does not return.

   @note A bail out message is printed if a signal that generates a
   core is raised.

   @param fmt Bail out message in printf() format.
*/

void BAIL_OUT(char const *fmt, ...)
  __attribute__((noreturn, format(printf,1,2)));


/**
   Print summary report and return exit status.

   This function will print a summary report of how many tests passed,
   how many were skipped, and how many remains to do.  The function
   should be called after all tests are executed in the following
   manner:

   @code
   return exit_status();
   @endcode

   @returns EXIT_SUCCESS if all tests passed, EXIT_FAILURE if one or
   more tests failed.
 */
int exit_status(void);


/**
   Skip entire test suite.

   To skip the entire test suite, use this function. It will
   automatically call exit(), so there is no need to have checks
   around it.
 */
void skip_all(char const *reason, ...)
  __attribute__((noreturn, format(printf, 1, 2)));

/**
   Start section of tests that are not yet ready.

   To start a section of tests that are not ready and are expected to
   fail, use this function and todo_end() in the following manner:

   @code
   todo_start("Not ready yet");
   ok(is_rocketeering(duck), "Rocket-propelled ducks");
   ok(is_kamikaze(duck), "Kamikaze ducks");
   todo_end();
   @endcode

   @see todo_end

   @note
   It is not possible to nest todo sections.

   @param message Message that will be printed before the todo tests.
*/
void todo_start(char const *message, ...)
  __attribute__((format (printf, 1, 2)));

/**
   End a section of tests that are not yet ready.
*/
void todo_end();

#ifdef __cplusplus
}
#endif

#endif /* TAP_H */
