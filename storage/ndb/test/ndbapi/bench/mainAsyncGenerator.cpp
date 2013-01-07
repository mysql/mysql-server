/*
   Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
#include "ndb_schema.hpp"


static int   numProcesses;
static int   numSeconds;
static int   numWarmSeconds;
static int   parallellism;
static int   millisSendPoll;
static int   minEventSendPoll;
static int   forceSendPoll;
static bool  useNdbRecord;
static bool  useCombUpd;
int          subscriberCount;
static bool  robustMode;

static ThreadData *data;
static Ndb_cluster_connection *g_cluster_connection= 0;


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
           "Usage: %s [-proc <num>] [-warm <num>] [-time <num>] [ -p <num>]" 
	   "[-t <num> ] [ -e <num> ] [ -f <num>] [ -ndbrecord ] [ -s <num>]\n"
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
	   "                 Default is 0\n"
           "  -ndbrecord     Use NdbRecord Api.\n"
           "                 Default is to use old Api\n"
           "  -combupdread   Use update pre-read operation where possible\n"
           "                 Default is to use separate read+update ops\n"
           "  -s <num>       Number of subscribers to operate on, default is %u.\n"
           "  -r             Whether to be robust to key errors\n",
           progname, NO_OF_SUBSCRIBERS);
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
   useNdbRecord     = false;
   useCombUpd       = false;
   subscriberCount  = NO_OF_SUBSCRIBERS;
   robustMode       = false;

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
     else if (strcmp("-ndbrecord",argv[i]) == 0) {
       useNdbRecord= true;
       i++;
     }
     else if (strcmp("-combupdread",argv[i]) == 0) {
       /* Comb up some dread */
       useCombUpd= true;
       i++;
     }
     else if (strcmp("-s", argv[i]) == 0) {
       if (i + 1 >= argc) {
         return 1;
       }
       if (sscanf(argv[i+1], "%u", &subscriberCount) == -1) {
         ndbout_c("-s flag requires a positive argument.");
         return 1;
       }
       i+=2;
     }
     else if (strcmp("-r", argv[i]) == 0) {
       robustMode= true;
       i++;
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
   if (useNdbRecord && useCombUpd){
     ndbout_c("NdbRecord does not currently support combined update "
              "and read.  Using separate read and update ops");
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
  ndbout_c("UseNdbRecord  : %u", useNdbRecord);
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
    ndbout_c("NDBT_Observation;tps;%.0f", gen->outerTps);
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

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return 0;
  }
  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return 0;
  }

  NdbRecordSharedData* ndbRecordSharedDataPtr= NULL;

  g_cluster_connection= &con;
  data = (ThreadData*)malloc((numProcesses*parallellism)*sizeof(ThreadData));

  NdbInterpretedCode* prog1= 0;
  NdbInterpretedCode* prog2= 0;
  NdbInterpretedCode* prog3= 0;

  if (useNdbRecord)
  {
    /* We'll create NdbRecord structures to match the TransactionData
     * struct
     */

    ndbRecordSharedDataPtr= (NdbRecordSharedData*) 
      malloc(sizeof(NdbRecordSharedData));
    Ndb* tempNdb= asyncDbConnect(1);
    NdbDictionary::Dictionary* dict= tempNdb->getDictionary();

    NdbDictionary::RecordSpecification cols[7];
    
    const NdbDictionary::Table* tab= dict->getTable(SUBSCRIBER_TABLE);
    cols[0].column= tab->getColumn((int) IND_SUBSCRIBER_NUMBER);
    cols[0].offset= offsetof(TransactionData, number);
    cols[0].nullbit_byte_offset= 0;
    cols[0].nullbit_bit_in_byte=  0;
    cols[1].column= tab->getColumn((int) IND_SUBSCRIBER_NAME);
    cols[1].offset= offsetof(TransactionData, name);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;
    cols[2].column= tab->getColumn((int) IND_SUBSCRIBER_GROUP);
    cols[2].offset= offsetof(TransactionData, group_id);
    cols[2].nullbit_byte_offset= 0;
    cols[2].nullbit_bit_in_byte=  0;
    cols[3].column= tab->getColumn((int) IND_SUBSCRIBER_LOCATION);
    cols[3].offset= offsetof(TransactionData, location);
    cols[3].nullbit_byte_offset= 0;
    cols[3].nullbit_bit_in_byte=  0;
    cols[4].column= tab->getColumn((int) IND_SUBSCRIBER_SESSIONS);
    cols[4].offset= offsetof(TransactionData, sessions);
    cols[4].nullbit_byte_offset= 0;
    cols[4].nullbit_bit_in_byte=  0;
    cols[5].column= tab->getColumn((int) IND_SUBSCRIBER_CHANGED_BY);
    cols[5].offset= offsetof(TransactionData, changed_by);
    cols[5].nullbit_byte_offset= 0;
    cols[5].nullbit_bit_in_byte=  0;
    cols[6].column= tab->getColumn((int) IND_SUBSCRIBER_CHANGED_TIME);
    cols[6].offset= offsetof(TransactionData, changed_time);
    cols[6].nullbit_byte_offset= 0;
    cols[6].nullbit_bit_in_byte=  0;
    
    ndbRecordSharedDataPtr->subscriberTableNdbRecord= 
      dict->createRecord(tab, cols, 7, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->subscriberTableNdbRecord == NULL)
    {
      ndbout << "Error creating record 1 : " << dict->getNdbError() << endl;
      return -1;
    }

    tab= dict->getTable(GROUP_TABLE);
    cols[0].column= tab->getColumn((int) IND_GROUP_ID);
    cols[0].offset= offsetof(TransactionData, group_id);
    cols[0].nullbit_byte_offset= 0;
    cols[0].nullbit_bit_in_byte=  0;
    /* GROUP_NAME not used via NdbRecord */
    cols[1].column= tab->getColumn((int) IND_GROUP_ALLOW_READ);
    cols[1].offset= offsetof(TransactionData, permission);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;

    ndbRecordSharedDataPtr->groupTableAllowReadNdbRecord=
      dict->createRecord(tab, cols, 2, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->groupTableAllowReadNdbRecord == NULL)
    {
      ndbout << "Error creating record 2.1: " << dict->getNdbError() << endl;
      return -1;
    }

    cols[1].column= tab->getColumn((int) IND_GROUP_ALLOW_INSERT);
    cols[1].offset= offsetof(TransactionData, permission);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;

    ndbRecordSharedDataPtr->groupTableAllowInsertNdbRecord=
      dict->createRecord(tab, cols, 2, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->groupTableAllowInsertNdbRecord == NULL)
    {
      ndbout << "Error creating record 2.2: " << dict->getNdbError() << endl;
      return -1;
    }

    cols[1].column= tab->getColumn((int) IND_GROUP_ALLOW_DELETE);
    cols[1].offset= offsetof(TransactionData, permission);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;

    ndbRecordSharedDataPtr->groupTableAllowDeleteNdbRecord=
      dict->createRecord(tab, cols, 2, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->groupTableAllowDeleteNdbRecord == NULL)
    {
      ndbout << "Error creating record 2.3: " << dict->getNdbError() << endl;
      return -1;
    }

    tab= dict->getTable(SESSION_TABLE);
    cols[0].column= tab->getColumn((int) IND_SESSION_SUBSCRIBER);
    cols[0].offset= offsetof(TransactionData, number);
    cols[0].nullbit_byte_offset= 0;
    cols[0].nullbit_bit_in_byte=  0;
    cols[1].column= tab->getColumn((int) IND_SESSION_SERVER);
    cols[1].offset= offsetof(TransactionData, server_id);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;
    cols[2].column= tab->getColumn((int) IND_SESSION_DATA);
    cols[2].offset= offsetof(TransactionData, session_details);
    cols[2].nullbit_byte_offset= 0;
    cols[2].nullbit_bit_in_byte=  0;

    ndbRecordSharedDataPtr->sessionTableNdbRecord=
      dict->createRecord(tab, cols, 3, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->sessionTableNdbRecord == NULL)
    {
      ndbout << "Error creating record 3 : " << dict->getNdbError() << endl;
      return -1;
    }

    tab= dict->getTable(SERVER_TABLE);
    cols[0].column= tab->getColumn((int) IND_SERVER_SUBSCRIBER_SUFFIX);
    cols[0].offset= offsetof(TransactionData, suffix);
    cols[0].nullbit_byte_offset= 0;
    cols[0].nullbit_bit_in_byte=  0;
    cols[1].column= tab->getColumn((int) IND_SERVER_ID);
    cols[1].offset= offsetof(TransactionData, server_id);
    cols[1].nullbit_byte_offset= 0;
    cols[1].nullbit_bit_in_byte=  0;
    /* SERVER_NAME not used via NdbRecord*/
    /* SERVER_READS not used via NdbRecord */
    /* SERVER_INSERTS not used via NdbRecord */
    /* SERVER_DELETES not used via NdbRecord */

    ndbRecordSharedDataPtr->serverTableNdbRecord=
      dict->createRecord(tab, cols, 2, sizeof(cols[0]), 0);

    if (ndbRecordSharedDataPtr->serverTableNdbRecord == NULL)
    {
      ndbout << "Error creating record 4 : " << dict->getNdbError() << endl;
      return -1;
    }

    /* Create program to increment server reads column */
    prog1= new NdbInterpretedCode(tab);

    if (prog1->add_val(IND_SERVER_READS, (Uint32)1) ||
        prog1->interpret_exit_ok() ||
        prog1->finalise())
    {
      ndbout << "Program 1 definition failed, exiting." << endl;
      return -1;
    }

    prog2= new NdbInterpretedCode(tab);

    if (prog2->add_val(IND_SERVER_INSERTS, (Uint32)1) ||
        prog2->interpret_exit_ok() ||
        prog2->finalise())
    {
      ndbout << "Program 2 definition failed, exiting." << endl;
      return -1;
    }

    prog3= new NdbInterpretedCode(tab);

    if (prog3->add_val(IND_SERVER_DELETES, (Uint32)1) ||
        prog3->interpret_exit_ok() ||
        prog3->finalise())
    {
      ndbout << "Program 3 definition failed, exiting." << endl;
      return -1;
    }

    ndbRecordSharedDataPtr->incrServerReadsProg= prog1;
    ndbRecordSharedDataPtr->incrServerInsertsProg= prog2;
    ndbRecordSharedDataPtr->incrServerDeletesProg= prog3;

    asyncDbDisconnect(tempNdb);      
  }

  for(i = 0; i < numProcesses; i++) {
    for(j = 0; j<parallellism; j++){
      int tid= i*parallellism + j;
      data[tid].warmUpSeconds         = numWarmSeconds;
      data[tid].testSeconds           = numSeconds;
      data[tid].coolDownSeconds       = numWarmSeconds;
      data[tid].randomSeed            = 
	(unsigned long)(NdbTick_CurrentMillisecond()+i+j);
      data[tid].changedTime           = 0;
      data[tid].runState              = Runnable;
      data[tid].ndbRecordSharedData   = ndbRecordSharedDataPtr;
      data[tid].useCombinedUpdate     = useCombUpd;
      data[tid].robustMode            = robustMode;
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

  if (useNdbRecord)
  {
    free(ndbRecordSharedDataPtr);
    delete(prog1);
    delete(prog2);
    delete(prog3);
  }
  
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
/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "ndb_error.hpp"
#include "userInterface.h"
#include <NdbMutex.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NdbApi.hpp>
#include <NdbOut.hpp>

/***************************************************************
* L O C A L   C O N S T A N T S                                *
***************************************************************/

/***************************************************************
* L O C A L   D A T A   S T R U C T U R E S                    *
***************************************************************/

/***************************************************************
* L O C A L   F U N C T I O N S                                *
***************************************************************/

#ifndef NDB_WIN32
#include <unistd.h>
#endif

Ndb*
asyncDbConnect(int parallellism){
  Ndb * pNDB = new Ndb(g_cluster_connection, "TEST_DB");
  
  pNDB->init(parallellism + 1);
  
  while(pNDB->waitUntilReady() != 0){
  }
  
  return pNDB;
}

void 
asyncDbDisconnect(Ndb* pNDB)
{
  delete pNDB;
}

double
userGetTime(void)
{
  static bool initialized = false;
  static NDB_TICKS initSecs = 0;
  static Uint32 initMicros = 0;
  double timeValue = 0;

  if ( !initialized ) {
    initialized = true;
    NdbTick_CurrentMicrosecond(&initSecs, &initMicros); 
    timeValue = 0.0;
  } else {
    NDB_TICKS secs = 0;
    Uint32 micros = 0;

    NdbTick_CurrentMicrosecond(&secs, &micros);
    double s  = (double)secs  - (double)initSecs;
    double us = (double)micros - (double)initMicros;
    
    timeValue = s + (us / 1000000.0);
  }
  return timeValue;
}

void showTime()
{
  char buf[128];
  struct tm* tm_now;
  time_t now;
  now = ::time((time_t*)NULL);
  tm_now = ::gmtime(&now);

  BaseString::snprintf(buf, 128,
	     "%d-%.2d-%.2d %.2d:%.2d:%.2d", 
	     tm_now->tm_year + 1900, 
	     tm_now->tm_mon, 
	     tm_now->tm_mday,
	     tm_now->tm_hour,
	     tm_now->tm_min,
	     tm_now->tm_sec);

  ndbout_c("Time: %s", buf);
}

