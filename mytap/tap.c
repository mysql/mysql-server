
#include "tap.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
   Test data structure.

   Data structure containing all information about the test suite.
 */
static TEST_DATA g_test = { 0 };

/**
   Output stream for test report message.

   The macro is just a temporary solution.
 */
#define tapout stdout

/**
  Emit a TAP result and optionally a description.

  @param pass  'true' if test passed, 'false' otherwise
  @param fmt   Description of test in printf() format.
  @param ap    Vararg list for the description string above.
 */
static int
emit_tap(int pass, char const *fmt, va_list ap)
{
  fprintf(tapout, "%sok %d%s",
          pass ? "" : "not ",
          ++g_test.last,
          (fmt && *fmt) ? " - " : "");
  if (fmt && *fmt)
    vfprintf(tapout, fmt, ap);
}


static int
emit_dir(const char *dir, const char *exp)
{
  fprintf(tapout, " # %s %s", dir, exp);
}


static int
emit_endl()
{
  fprintf(tapout, "\n");
}

void
diag(char const *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fprintf(tapout, "# ");
  vfprintf(tapout, fmt, ap);
  fprintf(tapout, "\n");
  va_end(ap);
}


void
plan(int const count)
{
  g_test.plan= count;
  switch (count)
  {
  case NO_PLAN:
  case SKIP_ALL:
    break;
  default:
    if (plan > 0)
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
  if (!pass && *g_test.todo == '\0')
    ++g_test.failed;

  va_list ap;
  va_start(ap, fmt);
  emit_tap(pass, fmt, ap);
  va_end(ap);
  if (*g_test.todo != '\0')
    emit_dir("TODO", g_test.todo);
  emit_endl();
}


void
skip(int how_many, char const *fmt, ...)
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
    emit_dir("SKIP", reason);
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
    diag("%d tests planned but only %d executed",
         g_test.plan, g_test.last);
    return EXIT_FAILURE;
  }

  if (g_test.failed > 0)
  {
    diag("Failed %d tests!", g_test.failed);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

