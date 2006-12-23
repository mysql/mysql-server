/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
 *  NdbPortLibTest.cpp
 *  Test the functionality of portlib
 *  TODO - Add tests for NdbMem
 */

#include <ndb_global.h>

#include "NdbOut.hpp"
#include "NdbThread.h"
#include "NdbMutex.h"
#include "NdbCondition.h"
#include "NdbSleep.h"
#include "NdbTick.h"
#include "NdbEnv.h"
#include "NdbHost.h"
#include "NdbMain.h"

int TestHasFailed;
int verbose = 0;

static void fail(const char* test, const char* cause)
{
  TestHasFailed = 1;
  ndbout << test << " failed, " << cause << endl;
}

// test 1 variables and funcs

extern "C"  void* thread1func(void* arg)
{
  int arg1;
  int returnvalue = 8;
  arg1 = *(int*)arg;
  ndbout << "thread1: thread1func called with arg = " << arg1 << endl;

  //  delay(1000);
  if (arg1 != 7)
    fail("TEST1", "Wrong arg");

  return returnvalue;
}

// test 2 variables and funcs

NdbMutex* test2mutex;

extern "C" void* test2func(void* arg)
{

  int arg1;
  arg1 = *(int*)arg;
  ndbout << "thread" << arg1 << " started in test2func" << endl;

  if (NdbMutex_Lock(test2mutex) != 0)
    fail("TEST2", "Failed to lock mutex");

  ndbout << "thread" << arg1 << ", test2func " << endl;

  if (NdbMutex_Unlock(test2mutex) != 0)
    fail("TEST2", "Failed to unlock mutex");

  int returnvalue = arg1;
  return returnvalue;
}


// test 3 and 7 variables and funcs

NdbMutex* testmutex;
NdbCondition* testcond;
int testthreadsdone;

extern "C" void* testfunc(void* arg)
{
  int tmpVar;
  int threadno;
  int result;

  threadno = *(int*)arg;

  ndbout << "Thread" << threadno << " started in testfunc" << endl;
  do 
    {

      if ((threadno % 2) == 0)
	result = NdbSleep_SecSleep(1);
      else
	result = NdbSleep_MilliSleep(100);

      if (result != 0)
	fail("TEST3", "Wrong result from sleep function");

      if (NdbMutex_Lock(testmutex) != 0)
	fail("TEST3", "Wrong result from NdbMutex_Lock function");
  
      ndbout << "thread" << threadno << ", testfunc " << endl;
      testthreadsdone++;
      tmpVar = testthreadsdone;

      if (NdbCondition_Signal(testcond) != 0)
	fail("TEST3", "Wrong result from NdbCondition_Signal function");
  
      if (NdbMutex_Unlock(testmutex) != 0)
	fail("TEST3", "Wrong result from NdbMutex_Unlock function");

    }
  while(tmpVar<100);
  
  return 0;
}

extern "C" void* testTryLockfunc(void* arg)
{
  int tmpVar = 0;
  int threadno;
  int result;

  threadno = *(int*)arg;

  ndbout << "Thread" << threadno << " started" << endl;
  do 
    {

      if ((threadno % 2) == 0)
	result = NdbSleep_SecSleep(1);
      else
	result = NdbSleep_MilliSleep(100);

      if (result != 0)
	fail("TEST3", "Wrong result from sleep function");

      if (NdbMutex_Trylock(testmutex) == 0){
	 
	ndbout << "thread" << threadno << ", testTryLockfunc locked" << endl;
  	testthreadsdone++;
	tmpVar = testthreadsdone;
	
	if (NdbCondition_Signal(testcond) != 0)
	  fail("TEST3", "Wrong result from NdbCondition_Signal function");
  
	if (NdbMutex_Unlock(testmutex) != 0)
	  fail("TEST3", "Wrong result from NdbMutex_Unlock function");
      }

    }
  while(tmpVar<100);
  
  return 0;
}



void testMicros(int count);
Uint64 time_diff(Uint64 s1, Uint64 s2, Uint32 m1, Uint32 m2);

NDB_COMMAND(PortLibTest, "portlibtest", "portlibtest", "Test the portable function layer", 4096){

  ndbout << "= TESTING ARGUMENT PASSING ============" << endl;
  ndbout << "ARGC: " << argc << endl;
  for(int i = 1; i < argc; i++){
    ndbout << " ARGV"<<i<<": " << (char*)argv[i] << endl;
  }
  ndbout << endl << endl;
  

  struct NdbThread* thread1var;
  void *status = 0;
  int arg = 7;

  TestHasFailed = 0;
  // create one thread and wait for it to return
  ndbout << "= TEST1 ===============================" << endl;

  thread1var = NdbThread_Create(thread1func, // Function 
				(void**)&arg,// Arg
				2048,        // Stacksize
				(char*)"thread1",  // Thread name
				NDB_THREAD_PRIO_MEAN); // Thread priority

  
  if(NdbThread_WaitFor(thread1var, &status) != 0)
    fail("TEST1", "NdbThread_WaitFor failed");
  // NOTE! thread return value is not yet used in Ndb and thus not tested(does not work)
  //ndbout << "thread1 returned, status = " << status << endl;
  //if (status != 8) 
  // fail("TEST1", "Wrong status");
  ndbout << "TEST1 completed" << endl;


  NdbThread_Destroy(&thread1var);

  // Create 10 threads that will wait for a mutex before printing it's message to screen
  ndbout << "= TEST2 ===============================" << endl;
#define T2_THREADS 10
  NdbThread* threads[T2_THREADS];
  int   args[T2_THREADS];
  void *status2 = 0;
  test2mutex = NdbMutex_Create();
  NdbMutex_Lock(test2mutex);

  for (int i = 0; i < T2_THREADS; i++)
    {
      args[i] = i;
    threads[i] = NdbThread_Create(test2func, // Function 
				  (void**)&args[i],// Arg
				  2048,        // Stacksize
				  (char*)"test2thread",  // Thread name
				  NDB_THREAD_PRIO_MEAN); // Thread priority
    if (threads[i] == NULL)
      fail("TEST2", "NdbThread_Create failed");
    }

  ndbout << "All threads created" << endl;

  NdbMutex_Unlock(test2mutex);

  for (int i = 0; i < T2_THREADS; i++)
  {
    if (NdbThread_WaitFor(threads[i], &status2))
      fail("TEST2", "NdbThread_WaitFor failed");      

    NdbThread_Destroy(&threads[i]);
    // Don't test return values
    //    ndbout << "thread" << i << " returned, status = " << status2 << endl;
    //    if (status2 != i)
    //      fail("TEST2", "Wrong status");
  }

  if (NdbMutex_Lock(test2mutex) != 0)
    fail("TEST2", "NdbMutex_Lock failed");
  if (NdbMutex_Unlock(test2mutex) != 0)
    fail("TEST2", "NdbMutex_Unlock failed");
  if (NdbMutex_Destroy(test2mutex) != 0)
    fail("TEST2", "NdbMutex_Destroy failed");
  ndbout << "TEST2 completed" << endl;

  ndbout << "= TEST3 ===============================" << endl;
  // Create 10 threads that will by synchronised by a condition
  // When they are awakened and have the mutex they will increment a global variable
#define T3_THREADS 10
  NdbThread* t3threads[T3_THREADS];
  int   t3args[T3_THREADS];
  void *status3 = 0;

  testmutex = NdbMutex_Create();
  testcond = NdbCondition_Create();
  testthreadsdone = 0;
  
  for (int i = 0; i < T3_THREADS; i++)
    {
      t3args[i] = i;
      t3threads[i] = NdbThread_Create(testfunc, // Function 
				      (void**)&t3args[i],// Arg
				      2048,        // Stacksize
				      (char*)"test3thread",  // Thread name
				      NDB_THREAD_PRIO_MEAN); // Thread priority
    }

  ndbout << "All threads created" << endl;

  if (NdbMutex_Lock(testmutex) != 0)
    fail("TEST3", "NdbMutex_Lock failed");
  
  while (testthreadsdone < T3_THREADS*10)
    {
      if(NdbCondition_Wait(testcond, testmutex) != 0)
	fail("TEST3", "NdbCondition_Wait failed");
      ndbout << "Condition signaled, there are " << testthreadsdone << " completed threads" << endl;
    }
  if (NdbMutex_Unlock(testmutex) != 0)
    fail("TEST3", "NdbMutex_Unlock failed");

  for (int i = 0; i < T3_THREADS; i++)
  {
    if (NdbThread_WaitFor(t3threads[i], &status3) != 0)
      fail("TEST3", "NdbThread_WaitFor failed");

    NdbThread_Destroy(&t3threads[i]);
    //ndbout << "thread" << i << " returned, status = " << status3 << endl;
    //if (status3 != i)
    //  fail("TEST3", "Wrong status");
  }

  NdbMutex_Destroy(testmutex);
  NdbCondition_Destroy(testcond);
  ndbout << "TEST3 completed" << endl;

  ndbout << "= TEST4 ===============================" << endl;
  // Check tick functions

  //#if 0

  int sleeptimes[] = {78, 12, 199, 567, 899};


  for (int i = 0; i < 5; i++)
  {
  ndbout << "*------------------------------- Measure" << i << endl;

  NDB_TICKS millisec_now; 
  NDB_TICKS millisec_now2;

  millisec_now = NdbTick_CurrentMillisecond();
  NdbSleep_MilliSleep(sleeptimes[i]);
  millisec_now2 = NdbTick_CurrentMillisecond();

  ndbout << "  Time before sleep = " << millisec_now << endl;
  ndbout << "  Time after sleep =  " << millisec_now2 << endl;
  ndbout << "  Tried to sleep "<<sleeptimes[i]<<" milliseconds." << endl;
  ndbout << "  Sleep time was " << millisec_now2 -millisec_now <<" milliseconds." << endl;

  }

  ndbout << "TEST4 completed" << endl;

  ndbout << "= TEST5 ===============================" << endl;
  // Check NdbOut

  ndbout << "Testing hex and dec functions of NdbOut" << endl;

  for (int i = 0; i<= 0xFF; i++)
    {
      ndbout << i << "=" <<hex << i << "="<<dec << i << ", ";
    }

  ndbout << endl<< "Testing that hex is reset to dec by endl" << endl;
  ndbout << hex << 67 << endl;
  ndbout << 67 << endl;
  
  ndbout << "TEST5 completed" << endl;


  ndbout << "= TEST6 ===============================" << endl;
  const char* theEnvHostNamePtr;
  char buf[255];
  char theHostHostName[256];
  theEnvHostNamePtr = NdbEnv_GetEnv("HOSTNAME", buf, 255);
  if(theEnvHostNamePtr == NULL)
    fail("TEST6", "Could not get HOSTNAME from env");
  else{
    ndbout << "HOSTNAME from GetEnv" <<  theEnvHostNamePtr << endl;
 
    NdbHost_GetHostName(theHostHostName);
  
    ndbout << "HOSTNAME from GetHostName" <<theHostHostName << endl;

    if (strcmp(theEnvHostNamePtr, theHostHostName) != 0)
      fail("TEST6", "NdbHost_GetHostName or NdbEnv_GetEnv failed");
  }

  ndbout << "= TEST7 ===============================" << endl;

  testmutex = NdbMutex_Create();
  testcond = NdbCondition_Create();
  testthreadsdone = 0;
  
  for (int i = 0; i < T3_THREADS; i++)
    {
      t3args[i] = i;
      t3threads[i] = NdbThread_Create(testfunc, // Function 
				      (void**)&t3args[i],// Arg
				      2048,        // Stacksize
				      (char*)"test7thread",  // Thread name
				      NDB_THREAD_PRIO_MEAN); // Thread priority
    }

  ndbout << "All threads created" << endl;

  if (NdbMutex_Lock(testmutex) != 0)
    fail("TEST7", "NdbMutex_Lock failed");

  while (testthreadsdone < T3_THREADS*10)
    {
      // just testing the functionality without timing out, therefor 20 sec.
      if(NdbCondition_WaitTimeout(testcond, testmutex, 20000) != 0)
	fail("TEST7", "NdbCondition_WaitTimeout failed");
      ndbout << "Condition signaled, there are " << testthreadsdone << " completed threads" << endl;
    }
  if (NdbMutex_Unlock(testmutex) != 0)
    fail("TEST7", "NdbMutex_Unlock failed");

  for (int i = 0; i < T3_THREADS; i++)
  {
    if (NdbThread_WaitFor(t3threads[i], &status3) != 0)
      fail("TEST7", "NdbThread_WaitFor failed");

    NdbThread_Destroy(&t3threads[i]);
  }

  NdbMutex_Destroy(testmutex);
  NdbCondition_Destroy(testcond);

  ndbout << "TEST7 completed" << endl;


  ndbout << "= TEST8 ===============================" << endl;
  ndbout << "         NdbCondition_WaitTimeout" << endl;
  testmutex = NdbMutex_Create();
  testcond = NdbCondition_Create();

  for (int i = 0; i < 5; i++)
  {
    ndbout << "*------------------------------- Measure" << i << endl;

  NDB_TICKS millisec_now; 
  NDB_TICKS millisec_now2;

  millisec_now = NdbTick_CurrentMillisecond();
  if (NdbCondition_WaitTimeout(testcond, testmutex, sleeptimes[i]) != 0)
    fail("TEST8", "NdbCondition_WaitTimeout failed");
  millisec_now2 = NdbTick_CurrentMillisecond();

  ndbout << "  Time before WaitTimeout = " << millisec_now << endl;
  ndbout << "  Time after WaitTimeout =  " << millisec_now2 << endl;
  ndbout << "  Tried to wait "<<sleeptimes[i]<<" milliseconds." << endl;
  ndbout << "  Wait time was " << millisec_now2 -millisec_now <<" milliseconds." << endl;

  }

  ndbout << "TEST8 completed" << endl;

 
  ndbout << "= TEST9 ===============================" << endl;
  ndbout << "         NdbTick_CurrentXXXXXsecond compare" << endl;

  for (int i = 0; i < 5; i++)
  {
    ndbout << "*------------------------------- Measure" << i << endl;

  NDB_TICKS millisec_now; 
  NDB_TICKS millisec_now2;
  Uint32 usec_now, usec_now2;
  Uint64 msec_now, msec_now2;


  millisec_now = NdbTick_CurrentMillisecond();
  NdbTick_CurrentMicrosecond( &msec_now, &usec_now);

  NdbSleep_MilliSleep(sleeptimes[i]);

  millisec_now2 = NdbTick_CurrentMillisecond();
  NdbTick_CurrentMicrosecond( &msec_now2, &usec_now2);

  Uint64 usecdiff = time_diff(msec_now,msec_now2,usec_now,usec_now2);
  NDB_TICKS msecdiff = millisec_now2 -millisec_now;

  ndbout << "     Slept "<<sleeptimes[i]<<" milliseconds." << endl;
  ndbout << "  Measured " << msecdiff <<" milliseconds with milli function ." << endl;
  ndbout << "  Measured " << usecdiff/1000 << "," << usecdiff%1000<<" milliseconds with micro function ." << endl;
  }

  ndbout << "TEST9 completed" << endl;


  const int iter = 20;
  ndbout << "Testing microsecond timer - " << iter << " iterations" << endl;
  testMicros(iter);
  ndbout << "Testing microsecond timer - COMPLETED" << endl;

  ndbout << "= TEST10 ===============================" << endl;

  testmutex = NdbMutex_Create();
  testcond = NdbCondition_Create();
  testthreadsdone = 0;
  
  for (int i = 0; i < T3_THREADS; i++)
    {
      t3args[i] = i;
      t3threads[i] = NdbThread_Create(testTryLockfunc, // Function 
				      (void**)&t3args[i],// Arg
				      2048,        // Stacksize
				      (char*)"test10thread",  // Thread name
				      NDB_THREAD_PRIO_MEAN); // Thread priority
    }

  ndbout << "All threads created" << endl;

  if (NdbMutex_Lock(testmutex) != 0)
    fail("TEST10", "NdbMutex_Lock failed");

  while (testthreadsdone < T3_THREADS*10)
    {
      if(NdbCondition_Wait(testcond, testmutex) != 0)
	fail("TEST10", "NdbCondition_WaitTimeout failed");
      ndbout << "Condition signaled, there are " << testthreadsdone << " completed threads" << endl;
    }
  if (NdbMutex_Unlock(testmutex) != 0)
    fail("TEST10", "NdbMutex_Unlock failed");

  for (int i = 0; i < T3_THREADS; i++)
  {
    if (NdbThread_WaitFor(t3threads[i], &status3) != 0)
      fail("TEST10", "NdbThread_WaitFor failed");

    NdbThread_Destroy(&t3threads[i]);
  }

  NdbMutex_Destroy(testmutex);
  NdbCondition_Destroy(testcond);

  ndbout << "TEST10 completed" << endl;


  // Check total status of test

  if (TestHasFailed == 1)
    ndbout << endl << "TEST FAILED!" << endl;
  else
    ndbout << endl << "TEST PASSED!" << endl;

  return TestHasFailed;

};

Uint64 time_diff(Uint64 s1, Uint64 s2, Uint32 m1, Uint32 m2){

  Uint64 diff = 0;
  diff += (s2 - s1) * 1000000;
  if(m2 >= m1)
    diff += (m2 - m1);
  else {
    diff += m2;
    diff -= m1;
  }

  //  if(0)
  // ndbout("(s1,m1) = (%d, %d) (s2,m2) = (%d, %d) -> diff = %d\n",
  //   (Uint32)s1,m1,(Uint32)s2,m2, (Uint32)diff);
  
  return diff;
};

void 
testMicros(int count){
  Uint32 avg = 0;
  Uint32 sum2 = 0;

  for(int i = 0; i<count; i++){
    Uint64 s1, s2;
    Uint32 m1, m2;
    if(NdbTick_CurrentMicrosecond(&s1, &m1) != 0){
      ndbout << "Failed to get current micro" << endl;
      TestHasFailed = 1; 
      return;
    }
    Uint32 r = (rand() % 1000) + 1;
    NdbSleep_MilliSleep(r);
    if(NdbTick_CurrentMicrosecond(&s2, &m2) != 0){
      ndbout << "Failed to get current micro" << endl;
      TestHasFailed = 1; 
      return;
    }
    Uint64 m = time_diff(s1,s2,m1,m2);
    if(verbose)
      ndbout << "Slept for " << r << " ms" 
	     << " - Measured  " << m << " us" << endl;
    
    if(m > (r*1000)){
      avg += (m - (r*1000));
      sum2 += (m - (r*1000)) * (m - (r*1000));
    } else {
      avg += ((r*1000) - m);
      sum2 += ((r*1000) - m) * ((r*1000) - m);
    }
#if 0
    m /= 1000;
    if(m > r && ((m - r) > 10)){
      ndbout << "Difference to big: " << (m - r) << " - Test failed" << endl;
      TestHasFailed = 1;
    }
    if(m < r && ((r - m) > 10)){
      ndbout << "Difference to big: " << (r - m) << " - Test failed" << endl;
      TestHasFailed = 1;
    }
#endif
  }

  Uint32 dev = (avg * avg - sum2) / count; dev /= count;
  avg /= count;

  Uint32 t = 0;
  while((t*t)<dev) t++;
  ndbout << "NOTE - measure are compared to NdbSleep_MilliSleep(...)" << endl;
  ndbout << "Average error = " << avg << " us" << endl;
  ndbout << "Stddev  error = " << t << " us" << endl;
}
