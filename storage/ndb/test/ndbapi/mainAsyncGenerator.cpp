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

#include <ndb_global.h>

#include <NdbHost.h>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbEnv.h>
#include <NdbTest.hpp>

#include "userInterface.h"
#include "dbGenerator.h"

static int   numProcesses;
static int   numSeconds;
static int   numWarmSeconds;
static int   parallellism;
static int   millisSendPoll;
static int   minEventSendPoll;
static int   forceSendPoll;

static ThreadData *data;

static void usage(const char *prog)
{
  const char  *progname;

   /*--------------------------------------------*/
   /* Get the name of the program (without path) */
   /*--------------------------------------------*/
   progname = strrchr(prog, '/');

   if (progname == 0)
     progname = prog;
   else
     ++progname;

   ndbout_c(
           "Usage: %s [-proc <num>] [-warm <num>] [-time <num>] [ -p <num>] " 
	   "[-t <num> ] [ -e <num> ] [ -f <num>] \n"
           "  -proc <num>    Specifies that <num> is the number of\n"
           "                 threads. The default is 1.\n"
           "  -time <num>    Specifies that the test will run for <num> sec.\n"
           "                 The default is 10 sec\n"
           "  -warm <num>    Specifies the warm-up/cooldown period of <num> "
	   "sec.\n"
           "                 The default is 10 sec\n"
	   "  -p <num>       The no of parallell transactions started by "
	   "one thread\n"
	   "  -e <num>       Minimum no of events before wake up in call to "
	   "sendPoll\n"
	   "                 Default is 1\n"
	   "  -f <num>       force parameter to sendPoll\n"
	   "                 Default is 0\n",
           progname);
}

static
int
parse_args(int argc, const char **argv)
{
   int i;

   numProcesses     = 1;
   numSeconds       = 10;
   numWarmSeconds   = 10;
   parallellism     = 1;
   millisSendPoll   = 10000;
   minEventSendPoll = 1;
   forceSendPoll    = 0;
   

   i = 1;
   while (i < argc){
     if (strcmp("-proc",argv[i]) == 0) {
       if (i + 1 >= argc) {
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &numProcesses) == -1 ||
	   numProcesses <= 0 || numProcesses > 127) {
	 ndbout_c("-proc flag requires a positive integer argument [1..127]");
	 return 1;
       }
       i += 2;
     } else if (strcmp("-p", argv[i]) == 0){
       if(i + 1 >= argc){
	 usage(argv[0]);
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &parallellism) == -1 ||
	   parallellism <= 0){
	 ndbout_c("-p flag requires a positive integer argument");
	 return 1;
       }
       i += 2;
     }
     else if (strcmp("-time",argv[i]) == 0) {
       if (i + 1 >= argc) {
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &numSeconds) == -1 ||
	   numSeconds < 0) {
	 ndbout_c("-time flag requires a positive integer argument");
	 return 1;
       }
       i += 2;
     }
     else if (strcmp("-warm",argv[i]) == 0) {
       if (i + 1 >= argc) {
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &numWarmSeconds) == -1 ||
	   numWarmSeconds < 0) {
	 ndbout_c("-warm flag requires a positive integer argument");
	 return 1;
       }
       i += 2;
     }
     else if (strcmp("-e",argv[i]) == 0) {
       if (i + 1 >= argc) {
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &minEventSendPoll) == -1 ||
	   minEventSendPoll < 0) {
	 ndbout_c("-e flag requires a positive integer argument");
	 return 1;
       }
       i += 2;
     }
     else if (strcmp("-f",argv[i]) == 0) {
       if (i + 1 >= argc) {
	 usage(argv[0]);
	 return 1;
       }
       if (sscanf(argv[i+1], "%d", &forceSendPoll) == -1 ||
	   forceSendPoll < 0) {
	 ndbout_c("-f flag requires a positive integer argument");
	 return 1;
       }
       i += 2;
     }
     else {
       return 1;
     }
   }

   if(minEventSendPoll > parallellism){
     ndbout_c("minEventSendPoll(%d) > parallellism(%d)",
	     minEventSendPoll, parallellism);
     ndbout_c("not very good...");
     ndbout_c("very bad...");
     ndbout_c("exiting...");
     return 1;
   }
   return 0;
}

static 
void 
print_transaction(const char            *header,
		  unsigned long          totalCount,
		  TransactionDefinition *trans,
		  unsigned int           printBranch,
		  unsigned int           printRollback)
{
  double f;
  
  ndbout_c("  %s: %d (%.2f%%) "
	   "Latency(ms) avg: %d min: %d max: %d std: %d n: %d",
	   header,
	   trans->count,
	   (double)trans->count / (double)totalCount * 100.0,
	   (int)trans->latency.getMean(),
	   (int)trans->latency.getMin(),
	   (int)trans->latency.getMax(),
	   (int)trans->latency.getStddev(),
	   (int)trans->latency.getCount()
	   );
  
  if( printBranch ){
    if( trans->count == 0 )
      f = 0.0;
    else
      f = (double)trans->branchExecuted / (double)trans->count * 100.0;
    ndbout_c("      Branches Executed: %d (%.2f%%)", trans->branchExecuted, f);
  }
  
  if( printRollback ){
    if( trans->count == 0 )
      f = 0.0;
    else
      f = (double)trans->rollbackExecuted / (double)trans->count * 100.0;
    ndbout_c("      Rollback Executed: %d (%.2f%%)",trans->rollbackExecuted,f);
  }
}

void 
print_stats(const char       *title,
	    unsigned int      length,
	    unsigned int      transactionFlag,
	    GeneratorStatistics *gen,
	    int numProc, int parallellism)
{
  int    i;
  char buf[10];
  char name[MAXHOSTNAMELEN];
  
  name[0] = 0;
  NdbHost_GetHostName(name);
  
  ndbout_c("\n------ %s ------",title);
  ndbout_c("Length        : %d %s",
	 length,
	 transactionFlag ? "Transactions" : "sec");
  ndbout_c("Processor     : %s", name);
  ndbout_c("Number of Proc: %d",numProc);
  ndbout_c("Parallellism  : %d", parallellism);
  ndbout_c("\n");

  if( gen->totalTransactions == 0 ) {
    ndbout_c("   No Transactions for this test");
  }
  else {
    for(i = 0; i < 5; i++) {
      sprintf(buf, "T%d",i+1);
      print_transaction(buf,
			gen->totalTransactions,
			&gen->transactions[i],
			i >= 2,
			i >= 3 );
    }
    
    ndbout_c("\n");
    ndbout_c("  Overall Statistics:");
    ndbout_c("     Transactions: %d", gen->totalTransactions);
    ndbout_c("     Outer       : %.0f TPS",gen->outerTps);
    ndbout_c("\n");
  }
}

static 
void *
threadRoutine(void *arg)
{
  int i;
  ThreadData *data = (ThreadData *)arg;
  Ndb * pNDB;

  pNDB = asyncDbConnect(parallellism);		      
  /* NdbSleep_MilliSleep(rand() % 10); */

  for(i = 0; i<parallellism; i++){
    data[i].pNDB = pNDB;
  }
  millisSendPoll = 30000;
  asyncGenerator(data, parallellism,
		 millisSendPoll, minEventSendPoll, forceSendPoll);

  asyncDbDisconnect(pNDB);

  return NULL;
}

NDB_COMMAND(DbAsyncGenerator, "DbAsyncGenerator",
	    "DbAsyncGenerator", "DbAsyncGenerator", 65535)
{
  ndb_init();
  int i;
  int j;
  int k;
  struct NdbThread* pThread = NULL;
  GeneratorStatistics  stats;
  GeneratorStatistics *p;
  char threadName[32];
  int rc = NDBT_OK;
  void* tmp = NULL;
  if(parse_args(argc,argv) != 0){
    usage(argv[0]);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
    

  ndbout_c("\nStarting Test with %d process(es) for %d %s parallellism %d",
	   numProcesses,
	   numSeconds,
	   "sec",
	   parallellism);

  ndbout_c("   WarmUp/coolDown = %d sec", numWarmSeconds);
  
  data = (ThreadData*)malloc((numProcesses*parallellism)*sizeof(ThreadData));
 
  for(i = 0; i < numProcesses; i++) {
    for(j = 0; j<parallellism; j++){
      data[i*parallellism+j].warmUpSeconds   = numWarmSeconds;
      data[i*parallellism+j].testSeconds     = numSeconds;
      data[i*parallellism+j].coolDownSeconds = numWarmSeconds;
      data[i*parallellism+j].randomSeed      = 
	NdbTick_CurrentMillisecond()+i+j;
      data[i*parallellism+j].changedTime     = 0;
      data[i*parallellism+j].runState        = Runnable;
    }
    sprintf(threadName, "AsyncThread[%d]", i);
    pThread = NdbThread_Create(threadRoutine, 
			      (void**)&data[i*parallellism], 
			      65535, 
			      threadName,
                              NDB_THREAD_PRIO_LOW);
    if(pThread != 0 && pThread != NULL){
      (&data[i*parallellism])->pThread = pThread;
    } else {      
      perror("Failed to create thread");
      rc = NDBT_FAILED;
    }
  }

  showTime();

  /*--------------------------------*/
  /* Wait for all processes to exit */
  /*--------------------------------*/
  for(i = 0; i < numProcesses; i++) {
    NdbThread_WaitFor(data[i*parallellism].pThread, &tmp);
    NdbThread_Destroy(&data[i*parallellism].pThread);
  }
   
  ndbout_c("All threads have finished");
  
  /*-------------------------------------------*/
  /* Clear all structures for total statistics */
  /*-------------------------------------------*/
  stats.totalTransactions = 0;
  stats.outerTps          = 0.0;
  
  for(i = 0; i < NUM_TRANSACTION_TYPES; i++ ) {
    stats.transactions[i].count            = 0;
    stats.transactions[i].branchExecuted   = 0;
    stats.transactions[i].rollbackExecuted = 0;
    stats.transactions[i].latency.reset();
  }
  
  /*--------------------------------*/
  /* Add the values for all Threads */
  /*--------------------------------*/
  for(i = 0; i < numProcesses; i++) {
    for(k = 0; k<parallellism; k++){
      p = &data[i*parallellism+k].generator;
      
      stats.totalTransactions += p->totalTransactions;
      stats.outerTps          += p->outerTps;
      
      for(j = 0; j < NUM_TRANSACTION_TYPES; j++ ) {
	stats.transactions[j].count += 
	  p->transactions[j].count;
	stats.transactions[j].branchExecuted += 
	  p->transactions[j].branchExecuted;
	stats.transactions[j].rollbackExecuted += 
	  p->transactions[j].rollbackExecuted;
	stats.transactions[j].latency += 
	  p->transactions[j].latency;
      }
    }
  }

  print_stats("Test Results", 
	      numSeconds,
	      0,
	      &stats,
	      numProcesses,
	      parallellism);

  free(data);
  
  NDBT_ProgramExit(rc);
}
