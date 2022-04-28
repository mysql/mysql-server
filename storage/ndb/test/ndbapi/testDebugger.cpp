/*
   Copyright (c) 2013, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <EventLogger.hpp>


static
Uint64
subtract_two_values(Uint64 a, Uint64 b)
{
  Uint64 r;
  if (b < a)
    r = a - b;
  else
    r = b - a;
  return r;
}

static
const char*
val2string(Uint32 a)
{
  const char *r;
  switch (a) {
  case 37:
    r = "Moving the lawn";
    break;
  case 28:
    r = "Cleaning junk";
    break;
  default:
    r = NULL;
    break;
  }
  return r;
}

int runTestEventLogger(NDBT_Context* ctx, NDBT_Step* step)
{
  const int loops = ctx->getNumLoops();
  const int n = step->getStepNo();

  g_eventLogger->info("%d, Starting test of EventLogger", n);

  for(int l = 0; l < loops * 10; l++)
  {
    g_eventLogger->alert("%d testing EventLogger, loop: %d", n, l);
    g_eventLogger->critical("%d testing EventLogger, loop: %d", n, l);
    g_eventLogger->error("%d testing EventLogger, loop: %d", n, l);
    g_eventLogger->warning("%d testing EventLogger, loop: %d", n, l);
    g_eventLogger->info("%d testing EventLogger, loop: %d", n, l);
    g_eventLogger->debug("%d testing EventLogger, loop: %d", n, l);

    {
      // Similar to usage in "the dog"
      const unsigned int val1 = 73;
      const Uint64 val2 = 37*1000*1000;
      const Uint64 val3 = 38*1000*1000;
      g_eventLogger->warning("TestDog: Warning overslept %llu ms, "
                             "expected %u ms.",
                             subtract_two_values(val2, val3)/1000,
                             val1);
    }

    {
      // Another usage similar to "the dog"
      const Uint32 counter_values[] = {37, 28, 19};
      const Uint32 thread_ids[] = {56, 47, 36};
      const Uint32 elapsed[] = {97, 86, 75};

      for (size_t i = 0; i < NDB_ARRAY_SIZE(counter_values); i++)
      {
        const char *str = val2string(counter_values[i]);
        if (str)
        {
          g_eventLogger->warning("TestDog: some kernel thread %u is stuck "
                                 "in: %s elapsed=%u",
                                 thread_ids[i], str, elapsed[i]);
        }
        else
        {
          g_eventLogger->warning("TestDog: some kernel thread %u is stuck "
                                 "in: Unknown place %u elapsed=%u",
                                 thread_ids[i], counter_values[i],
                                 elapsed[i]);
        }
      }
    }
  }

  g_eventLogger->info("%d, Finished test of EventLogger", n);

  return NDBT_OK;
}

NDBT_TESTSUITE(testDebugger);
DRIVER(DummyDriver); /* turn off use of NdbApi */
TESTCASE("TestEventLogger",
         "Using EventLogger from single thread")
{
  STEP(runTestEventLogger);
}
TESTCASE("TestEventLogger10",
         "Using EventLogger from 10 threads to ensure its thread safety")
{
  STEPS(runTestEventLogger, 10);
}
NDBT_TESTSUITE_END(testDebugger)


int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testDebugger);
  testDebugger.setCreateTable(false);
  testDebugger.setRunAllTables(true);
  testDebugger.setEnsureIndexStatTables(false);

  int res = testDebugger.execute(argc, argv);

  ndb_end(0);
  return res;
}
