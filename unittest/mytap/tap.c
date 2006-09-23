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

#include "tap.h"

#include "my_config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

/**
   Test data structure.

   Data structure containing all information about the test suite.

   @ingroup MyTAP
 */
static TEST_DATA g_test = { 0, 0, 0, "" };

/**
   Output stream for test report message.

   The macro is just a temporary solution.
 */
#define tapout stdout

/**
  Emit the beginning of a test line, that is: "(not) ok", test number,
  and description.

  To emit the directive, use the emit_dir() function

  @ingroup MyTAP

  @see emit_dir

  @param pass  'true' if test passed, 'false' otherwise
  @param fmt   Description of test in printf() format.
  @param ap    Vararg list for the description string above.
 */
static void
emit_tap(int pass, char const *fmt, va_list ap)
{
  fprintf(tapout, "%sok %d%s",
          pass ? "" : "not ",
          ++g_test.last,
          (fmt && *fmt) ? " - " : "");
  if (fmt && *fmt)
    vfprintf(tapout, fmt, ap);
}


/**
   Emit a TAP directive.

   TAP directives are comments after that have the form:

   @code
   ok 1 # skip reason for skipping
   not ok 2 # todo some text explaining what remains
   @endcode

   @param dir  Directive as a string
   @param exp  Explanation string
 */
static void
emit_dir(const char *dir, const char *exp)
{
  fprintf(tapout, " # %s %s", dir, exp);
}


/**
   Emit a newline to the TAP output stream.
 */
static void
emit_endl()
{
  fprintf(tapout, "\n");
}

static void
handle_core_signal(int signo)
{
  BAIL_OUT("Signal %d thrown", signo);
}

void
BAIL_OUT(char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(tapout, "Bail out! ");
  vfprintf(tapout, fmt, ap);
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
  { SIGQUIT, handle_core_signal },
  { SIGILL,  handle_core_signal },
  { SIGABRT, handle_core_signal },
  { SIGFPE,  handle_core_signal },
  { SIGSEGV, handle_core_signal },
  { SIGBUS,  handle_core_signal },
  { SIGXCPU, handle_core_signal },
  { SIGXFSZ, handle_core_signal },
  { SIGSYS,  handle_core_signal },
  { SIGTRAP, handle_core_signal }
};

void
plan(int const count)
{
  /*
    Install signal handler
  */
  size_t i;
  for (i= 0; i < sizeof(install_signal)/sizeof(*install_signal); ++i)
    signal(install_signal[i].signo, install_signal[i].handler);

  g_test.plan= count;
  switch (count)
  {
  case NO_PLAN:
    break;
  default:
    if (count > 0)
      fprintf(tapout, "1..%d\n", count);
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
  va_end(ap);
  exit(0);
}

void
ok(int const pass, char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  if (!pass && *g_test.todo == '\0')
    ++g_test.failed;

  emit_tap(pass, fmt, ap);
  va_end(ap);
  if (*g_test.todo != '\0')
    emit_dir("todo", g_test.todo);
  emit_endl();
}


void
skip(int how_many, char const *const fmt, ...)
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
    emit_tap(1, NULL, ap);
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

int exit_status() {
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

  return EXIT_SUCCESS;
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
   development towards a solution that the tests.  This means that the
   unit tests has to be as complete as possible, testing at least:

   - Normal input
   - Borderline cases
   - Faulty input
   - Error handling
   - Bad environment

   We will go over each case and explain it in more detail.

   @subsection NormalSSec Normal input

   @subsection BorderlineSSec Borderline cases

   @subsection FaultySSec Faulty input

   @subsection ErrorSSec Error handling

   @subsection EnvironmentSSec Environment

   Sometimes, modules has to behave well even when the environment
   fails to work correctly.  Typical examples are: out of dynamic
   memory, disk is full,

   @section UnitTestSec How to structure a unit test

   In this section we will give some advice on how to structure the
   unit tests to make the development run smoothly.

   @subsection PieceSec Test each piece separately

   Don't test all functions using size 1, then all functions using
   size 2, etc.
 */
