/*
   Copyright (c) 2003, 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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
#include "NdbApi.hpp"
#include <NdbSchemaCon.hpp>
#include <NdbMain.h>
#include <md5_hash.hpp>

#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <NdbTimer.hpp>
#include <NDBT_Error.hpp>

#include <NdbTest.hpp>
#include <NDBT_Stats.hpp>

#define MAX_PARTS 4 
#define MAX_SEEK 16 
#define MAXSTRLEN 16 
#define MAXATTR 511 
#define MAXTABLES 1
#define NDB_MAXTHREADS 128
#define MAX_EXECUTOR_THREADS 128
#define MAX_DEFINER_THREADS 32
#define MAX_REAL_THREADS 160
#define NDB_MAX_NODES 48
/*
  NDB_MAXTHREADS used to be just MAXTHREADS, which collides with a
  #define from <sys/thread.h> on AIX (IBM compiler).  We explicitly
  #undef it here lest someone use it by habit and get really funny
  results.  K&R says we may #undef non-existent symbols, so let's go.
*/
#undef MAXTHREADS
#define MAXPAR 1024
#define MAXATTRSIZE 1000
#define PKSIZE 2

enum StartType { 
  stIdle = 0,
  stInsert = 1,
  stRead = 2,
  stUpdate = 3,
  stDelete = 4,
  stStop = 5
} ;

enum RunType {
  RunInsert = 1,
  RunRead = 2,
  RunUpdate = 3,
  RunDelete = 4,
  RunCreateTable = 5,
  RunDropTable = 6,
  RunAll = 7
};

struct ThreadNdb
{
  int NoOfOps;
  int ThreadNo;
  char * record;
};

extern "C" { static void* threadLoop(void*); }
static void setAttrNames(void);
static void setTableNames(void);
static int readArguments(int argc, const char** argv);
static void dropTables(Ndb* pMyNdb);
static int createTables(Ndb*);
static void defineOperation(NdbConnection* aTransObject, StartType aType, 
                            Uint32 base, Uint32 aIndex);
static void defineNdbRecordOperation(char*, NdbConnection* aTransObject, StartType aType,
                            Uint32 base, Uint32 aIndex);
static void execute(StartType aType);
static bool executeThread(ThreadNdb*, StartType aType, Ndb* aNdbObject, unsigned int);
static bool executeTransLoop(ThreadNdb* pThread, StartType aType, Ndb* aNdbObject,
                             unsigned int threadBase, int threadNo);
static void executeCallback(int result, NdbConnection* NdbObject,
                            void* aObject);
static bool error_handler(const NdbError & err);
static void input_error();
static Uint32 get_my_node_id(Uint32 tableNo, Uint32 threadNo);

static void main_thread(RunType run_type, NdbTimer & timer);
static Uint64 get_total_transactions();
static void run_old_flexAsynch(ThreadNdb *pThreadData, NdbTimer & timer);

static int                              retry_opt = 3 ;
static int                              failed = 0 ;
                
ErrorData * flexAsynchErrorData;                        

static NdbThread*                       threadLife[MAX_REAL_THREADS];
static int                              tNodeId;
static int                              ThreadReady[MAX_REAL_THREADS];
static longlong                         ThreadExecutions[MAX_REAL_THREADS];
static StartType                        ThreadStart[NDB_MAXTHREADS];
static char                             tableName[MAXTABLES][MAXSTRLEN+1];
static const NdbDictionary::Table *     tables[MAXTABLES];
static char                             attrName[MAXATTR][MAXSTRLEN+1];
static bool                             nodeTableArray[MAXTABLES][NDB_MAX_NODES + 1];
static Uint32                           numberNodeTable[MAXTABLES];
static RunType                          tRunType = RunAll;
static int                              tStdTableNum = 0;
static int                              tWarmupTime = 10; //Seconds
static int                              tExecutionTime = 30; //Seconds
static int                              tCooldownTime = 10; //Seconds

// Program Parameters
static NdbRecord * g_record[MAXTABLES];
static bool tNdbRecord = false;

static int                              tLocal = 0;
static int                              tSendForce = 0;
static int                              tNoOfLoops = 1;
static int                              tAttributeSize = 1;
static unsigned int             tNoOfThreads = 1;
static unsigned int             tNoOfParallelTrans = 32;
static unsigned int             tNoOfAttributes = 25;
static unsigned int             tNoOfTransactions = 500;
static unsigned int             tNoOfOpsPerTrans = 1;
static unsigned int             tLoadFactor = 80;
static bool                     tempTable = false;
static bool                     startTransGuess = true;
static int                      tExtraReadLoop = 0;
static bool                     tNew = false;
static bool                     tImmediate = false;

//Program Flags
static int                              theTestFlag = 0;
static int                              theSimpleFlag = 0;
static int                              theDirtyFlag = 0;
static int                              theWriteFlag = 0;
static int                              theStdTableNameFlag = 0;
static int                              theTableCreateFlag = 0;
static int                              tConnections = 1;

#define START_REAL_TIME
#define STOP_REAL_TIME
#define DEFINE_TIMER NdbTimer timer
#define START_TIMER timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans)

NDBT_Stats a_i, a_u, a_d, a_r;

static
void
print(const char * name, NDBT_Stats& s)
{
  printf("%s average: %u/s min: %u/s max: %u/s stddev: %u%%\n",
         name,
         (unsigned)s.getMean(),
         (unsigned)s.getMin(),
         (unsigned)s.getMax(),
         (unsigned)(100*s.getStddev() / s.getMean()));
}

static void 
resetThreads(){

  for (unsigned i = 0; i < tNoOfThreads ; i++) {
    ThreadReady[i] = 0;
    ThreadStart[i] = stIdle;
  }//for
}

static void 
waitForThreads(Uint32 num_threads_to_wait_for)
{
  int cont = 0;
  do {
    cont = 0;
    NdbSleep_MilliSleep(20);
    for (unsigned i = 0; i < num_threads_to_wait_for ; i++) {
      if (ThreadReady[i] == 0) {
        cont = 1;
      }//if
    }//for
  } while (cont == 1);
}

static void 
tellThreads(StartType what)
{
  for (unsigned i = 0; i < tNoOfThreads ; i++) 
    ThreadStart[i] = what;
}

static Ndb_cluster_connection * g_cluster_connection = 0;

NDB_COMMAND(flexAsynch, "flexAsynch", "flexAsynch", "flexAsynch", 65535)
{
  ndb_init();
  ThreadNdb*            pThreadData;
  DEFINE_TIMER;
  int                   returnValue = NDBT_OK;

  flexAsynchErrorData = new ErrorData;
  flexAsynchErrorData->resetErrorCounters();

  if (readArguments(argc, argv) != 0){
    input_error();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  pThreadData = new ThreadNdb[NDB_MAXTHREADS];

  ndbout << endl << "FLEXASYNCH - Starting normal mode" << endl;
  ndbout << "Perform benchmark of insert, update and delete transactions";
  ndbout << endl;
  ndbout << "  " << tNoOfThreads << " number of concurrent threads " << endl;
  ndbout << "  " << tNoOfParallelTrans;
  ndbout << " number of parallel operation per thread " << endl;
  ndbout << "  " << tNoOfTransactions << " transaction(s) per round " << endl;
  if (tRunType == RunAll){
    ndbout << "  " << tNoOfLoops << " iterations " << endl;
  } else if (tRunType == RunRead || tRunType == RunUpdate){
    ndbout << "  Warmup time is " << tWarmupTime << endl;
    ndbout << "  Execution time is " << tExecutionTime << endl;
    ndbout << "  Cooldown time is " << tCooldownTime << endl;
  }
  ndbout << "  " << "Load Factor is " << tLoadFactor << "%" << endl;
  ndbout << "  " << tNoOfAttributes << " attributes per table " << endl;
  ndbout << "  " << tAttributeSize;
  ndbout << " is the number of 32 bit words per attribute " << endl;
  if (tempTable == true) {
    ndbout << "  Tables are without logging " << endl;
  } else {
    ndbout << "  Tables are with logging " << endl;
  }//if
  if (startTransGuess == true) {
    ndbout << "  Transactions are executed with hint provided" << endl;
  } else {
    ndbout << "  Transactions are executed with round robin scheme" << endl;
  }//if
  if (tSendForce == 0) {
    ndbout << "  No force send is used, adaptive algorithm used" << endl;
  } else if (tSendForce == 1) {
    ndbout << "  Force send used" << endl;
  } else {
    ndbout << "  No force send is used, adaptive algorithm disabled" << endl;
  }//if

  ndbout << endl;

  NdbThread_SetConcurrencyLevel(2 + (tNoOfThreads * 5 / 4));

  /* print Setting */
  flexAsynchErrorData->printSettings(ndbout);

  setAttrNames();
  setTableNames();

  g_cluster_connection = new Ndb_cluster_connection [tConnections];
  if (tConnections > 1)
  {
    printf("Creating %u connections...", tConnections);
    fflush(stdout);
  }
  for (int i = 0; i < tConnections; i++)
  {
    if(g_cluster_connection[i].connect(12, 5, 1) != 0)
      return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (tConnections > 1)
  {
    printf("\n");
    fflush(stdout);
  }
  
  Ndb * pNdb = new Ndb(g_cluster_connection+0, "TEST_DB");      
  pNdb->init();
  tNodeId = pNdb->getNodeId();

  ndbout << "  NdbAPI node with id = " << pNdb->getNodeId() << endl;
  ndbout << endl;
  
  ndbout << "Waiting for ndb to become ready..." <<endl;
  if (pNdb->waitUntilReady(10000) != 0){
    ndbout << "NDB is not ready" << endl;
    ndbout << "Benchmark failed!" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (tRunType == RunCreateTable)
  {
    if (createTables(pNdb) != 0){
      returnValue = NDBT_FAILED;
    }
  }
  else if (tRunType == RunDropTable)
  {
    dropTables(pNdb);
  }
  else if(returnValue == NDBT_OK){
    if (createTables(pNdb) != 0){
      returnValue = NDBT_FAILED;
    }
  }

  if (tNdbRecord && !tNew)
  {
    Uint32 sz = NdbDictionary::getRecordRowLength(g_record[0]);
    sz += 3;
    for (Uint32 i = 0; i<tNoOfThreads; i++)
    {
      pThreadData[i].record = (char*)malloc(sz);
      bzero(pThreadData[i].record, sz);
    }
  }

  if(returnValue == NDBT_OK &&
     tRunType != RunCreateTable &&
     tRunType != RunDropTable){
    if (tNew)
    {
      main_thread(tRunType, timer);
    }
    else
    {
      run_old_flexAsynch(pThreadData, timer);
    }
  }

  if (tRunType == RunAll)
  {
    dropTables(pNdb);
  }
  delete [] pThreadData;
  delete pNdb;

  if (tRunType == RunAll ||
      tRunType == RunInsert ||
      tRunType == RunDelete ||
      tRunType == RunUpdate ||
      tRunType == RunRead)
  {
    //printing errorCounters
    flexAsynchErrorData->printErrorCounters(ndbout);
    if (tRunType == RunAll) {
      print("insert", a_i);
      print("update", a_u);
      print("delete", a_d);
      print("read  ", a_r);
    }
  }
  if (tRunType == RunInsert ||
      tRunType == RunRead ||
      tRunType == RunUpdate ||
      tRunType == RunDelete)
  {
    Uint64 total_transactions = 0;
    Uint64 exec_time;

    if (tNew)
    {
      total_transactions = get_total_transactions();
    }
    else
    {
      if (tRunType == RunInsert || tRunType == RunDelete)
      {
        total_transactions = (longlong)tNoOfTransactions;
        total_transactions *= (longlong)tNoOfThreads;
        total_transactions *= (longlong)tNoOfParallelTrans;
      }
      else
      {
        for (Uint32 i = 0; i < tNoOfThreads; i++)
        {
          total_transactions += ThreadExecutions[i];
        }
      }
    }
    if (tRunType == RunInsert || tRunType == RunDelete) {
      exec_time = (Uint64)timer.elapsedTime();
    } else {
      exec_time = (Uint64)tExecutionTime * 1000;
    }
    ndbout << "Total number of transactions is " << total_transactions;
    ndbout << endl;
    ndbout << "Execution time is " << exec_time << " milliseconds" << endl;

    if (!exec_time)
    {
      exec_time = 1; /* Avoid floating point exception */
      ndbout_c("Zero execution time!!!");
    }
    total_transactions = (total_transactions * 1000) / exec_time;
    int trans_per_sec = (int)total_transactions;
    ndbout << "Total transactions per second " << trans_per_sec << endl;
  }

  delete [] g_cluster_connection;

  return NDBT_ProgramExit(returnValue);
}//main()


static void execute(StartType aType)
{
  resetThreads();
  tellThreads(aType);
  waitForThreads(tNoOfThreads);
}//execute()

static void*
threadLoop(void* ThreadData)
{
  Ndb* localNdb;
  StartType tType;
  ThreadNdb* tabThread = (ThreadNdb*)ThreadData;
  int threadNo = tabThread->ThreadNo;
  localNdb = new Ndb(g_cluster_connection+(threadNo % tConnections), "TEST_DB");
  localNdb->init(MAXPAR);
  localNdb->waitUntilReady(10000);
  unsigned int threadBase = threadNo;

  
  for (;;){
    while (ThreadStart[threadNo] == stIdle) {
      NdbSleep_MilliSleep(10);
    }//while

    // Check if signal to exit is received
    if (ThreadStart[threadNo] == stStop) {
      break;
    }//if

    tType = ThreadStart[threadNo];
    ThreadStart[threadNo] = stIdle;
    if (tRunType == RunAll || tRunType == RunInsert || tRunType == RunDelete){
      if(!executeThread(tabThread,
                        tType,
                        localNdb,
                        threadBase)){
        break;
      }
    } else {
      if(!executeTransLoop(tabThread,
                           tType,
                           localNdb,
                           threadBase,
                           threadNo)){
        break;
      }
    }
    ThreadReady[threadNo] = 1;
  }//for

  delete localNdb;
  ThreadReady[threadNo] = 1;

  return NULL;
}//threadLoop()

static int error_count = 0;

static bool
update_num_ops(Uint32 *num_ops, NdbConnection **tConArray)
{
  /*
    Move num_ops forward to next unused position, can be old
    transactions still outstanding
  */
  for ( ; *num_ops < tNoOfParallelTrans; (*num_ops)++)
  {
    if (tConArray[*num_ops])
      continue;
    else
      break;
  }
  if (*num_ops == tNoOfParallelTrans)
    return true;
  return false;
}

static int
executeTrans(ThreadNdb* pThread,
             StartType aType,
             Ndb* aNdbObject,
             unsigned int threadBase,
             unsigned int record,
             Uint32 nodeId,
             NdbConnection **tConArray,
             bool execute_all)
{
  unsigned int tBase;
  unsigned int tBase2;
  Uint32 threadBaseLoc, threadBaseLoc2;
  Uint32 num_ops = 0;
  Uint32 i, loops;

  START_REAL_TIME;
  for (i = record, loops = 0;
       i < tNoOfTransactions &&
       loops < 16 &&
       num_ops < tNoOfParallelTrans;
       i++, loops++)
  {
    tBase = i * tNoOfParallelTrans * tNoOfOpsPerTrans;
    threadBaseLoc = (threadBase * tNoOfTransactions * tNoOfParallelTrans) +
                    (i * tNoOfParallelTrans);
    for (unsigned int j = 0; j < tNoOfParallelTrans; j++) {
      if (update_num_ops(&num_ops, tConArray))
        break;
      threadBaseLoc2 = threadBaseLoc + j;
      tBase2 = tBase + (j * tNoOfOpsPerTrans);
      if (startTransGuess == true) {
        union {
          Uint64 _align;
          Uint32 Tkey32[2];
        };
        (void)_align;

        Tkey32[0] = threadBaseLoc2;
        Tkey32[1] = tBase2;
        Ndb::Key_part_ptr hint[2];
        hint[0].ptr = Tkey32+0;
        hint[0].len = 4;
        hint[1].ptr = 0;
        hint[1].len = 0;

        tConArray[num_ops] = aNdbObject->startTransaction(tables[0], hint);
      }
      else
      {
        tConArray[num_ops] = aNdbObject->startTransaction();
      }

      if (tConArray[num_ops] == NULL){
        error_handler(aNdbObject->getNdbError());
        ndbout << endl << "Unable to recover! Quitting now" << endl ;
        return -1;
      }//if
   
      if (nodeId != 0 &&
          tConArray[num_ops]->getConnectedNodeId() != nodeId)
      {
        /*
          We're running only local operations, this won't be local,
          ignore this record
        */
        aNdbObject->closeTransaction(tConArray[num_ops]);
        tConArray[num_ops] = NULL;
        continue;
      }
      for (unsigned int k = 0; k < tNoOfOpsPerTrans; k++) {
        //-------------------------------------------------------
        // Define the operation, but do not execute it yet.
        //-------------------------------------------------------
        if (tNdbRecord)
          defineNdbRecordOperation(pThread->record,
                                   tConArray[num_ops],
                                   aType,
                                   threadBaseLoc2,
                                   (tBase2+k));
        else
          defineOperation(tConArray[num_ops],
                          aType,
                          threadBaseLoc2,
                          (tBase2 + k));
      }//for
    
      tConArray[num_ops]->executeAsynchPrepare(Commit,
                                               &executeCallback,
                                               (void*)&tConArray[num_ops]);
      num_ops++;
    }//for
  }//for
  STOP_REAL_TIME;
  if (num_ops == 0)
    return 0;
  //-------------------------------------------------------
  // Now we have defined a set of operations, it is now time
  // to execute all of them. If execute_all isn't set, we
  // only execute at least half of them. In this manner we
  // can cater for different execution speeds in different
  // parts of the system.
  //-------------------------------------------------------
  int min_execs = execute_all ? (int)num_ops :
                     (num_ops > 1 ? (int)(num_ops / 2) : 1);
  int Tcomp = aNdbObject->sendPollNdb(3000,
                                      min_execs,
                                      tSendForce);
  while (Tcomp < min_execs) {
    int TlocalComp = aNdbObject->pollNdb(3000, min_execs - Tcomp);
    Tcomp += TlocalComp;
  }//while
  if (aNdbObject->getNdbError().code != 0 && error_count < 10000){
    error_count++;
    ndbout << "i = " << i << ", error = ";
    ndbout << aNdbObject->getNdbError().code << ", threadBase = ";
    ndbout << hex << threadBase << endl;
  }
  return Tcomp;
}

static 
bool
executeTransLoop(ThreadNdb* pThread, 
                 StartType aType,
                 Ndb* aNdbObject,
                 unsigned int threadBase,
                 int threadNo) {
  bool continue_flag = true;
  int time_expired;
  longlong executions = 0;
  unsigned int i = 0;
  Uint32 nodeId;
  int ops = 0;
  int record;
  Uint32 local_count = 0;
  bool execute_all = true;
  DEFINE_TIMER;
  NdbConnection* tConArray[MAXPAR];

  for (Uint32 i = 0; i < MAXPAR; i++) tConArray[i] = NULL;
  if (tLocal > 0)
  {
    nodeId = get_my_node_id((Uint32)0, threadBase);
  }
  else
    nodeId = 0;
  ThreadExecutions[threadNo] = 0;
  START_TIMER;
  do
  {
    if (tLocal == 2)
    {
      /* Select node on round robin basis */
      local_count++;
      nodeId = get_my_node_id((Uint32)0, local_count);
    }
    else if (tLocal == 3)
    {
      /* Select node on random basis */
      local_count = (Uint32)(rand() % numberNodeTable[0]);
      nodeId = get_my_node_id((Uint32)0, local_count);
    }
    record = rand() % tNoOfTransactions;
    if ((ops = executeTrans(pThread,
                            aType,
                            aNdbObject,
                            threadBase,
                            (Uint32)record,
                            nodeId,
                            tConArray,
                            execute_all)) < 0)
      return false;
    STOP_TIMER;
    if (!continue_flag)
      break;
    time_expired = (int)(timer.elapsedTime() / 1000);
    if (time_expired < tWarmupTime)
      ; //Do nothing
    else if (time_expired < (tWarmupTime + tExecutionTime)){
      executions += ops; //Count measurement
    }
    else if (time_expired < (tWarmupTime + tExecutionTime + tCooldownTime))
      ; //Do nothing
    else
    {
      execute_all = true;
      continue_flag = false; //Time expired
    }
    if (i == tNoOfTransactions) /* Make sure the record exists */
      i = 0;
  } while (1);
  ThreadExecutions[threadNo] = executions;
  return true;
}//executeTransLoop()

static 
bool
executeThread(ThreadNdb* pThread, 
	      StartType aType,
              Ndb* aNdbObject,
              unsigned int threadBase) {
  NdbConnection* tConArray[MAXPAR];

  for (Uint32 i = 0; i < MAXPAR; i++) tConArray[i] = NULL;
  for (unsigned int i = 0; i < tNoOfTransactions; i++) {
    if ((executeTrans(pThread,
                      aType,
                      aNdbObject,
                      threadBase,
                      i,
                      (Uint32)0,
                      tConArray,
                      true)) < 0)
      return false;
  }//for
  return true;
}//executeThread()

static void
executeCallback(int result, NdbConnection* NdbObject, void* aObject)
{
  NdbConnection **array_ref = (NdbConnection**)aObject;
  assert(NdbObject == *array_ref);
  *array_ref = NULL;
  if (result == -1 && failed < 100)
  {

    // Add complete error handling here

    int retCode = flexAsynchErrorData->handleErrorCommon(NdbObject->getNdbError());
    if (retCode == 1)
    {
      if (NdbObject->getNdbError().code != 626 && NdbObject->getNdbError().code != 630)
      {
        ndbout_c("execute: %s", NdbObject->getNdbError().message);
        ndbout_c("Error code = %d", NdbObject->getNdbError().code);
      }
    }
    else if (retCode == 2)
    {
      ndbout << "4115 should not happen in flexAsynch" << endl;
    }
    else if (retCode == 3)
    {
      /* What can we do here? */
      ndbout_c("execute: %s", NdbObject->getNdbError().message);
    }//if(retCode == 3)
    //    ndbout << "Error occured in poll:" << endl;
    //    ndbout << NdbObject->getNdbError() << endl;
    failed++ ;
  }//if
  NdbObject->close(); /* Close transaction */
  return;
}//executeCallback()



static void
defineOperation(NdbConnection* localNdbConnection, StartType aType,
                Uint32 threadBase, Uint32 aIndex)
{
  NdbOperation*  localNdbOperation;
  unsigned int   loopCountAttributes = tNoOfAttributes;
  unsigned int   countAttributes;
  Uint32         attrValue[MAXATTRSIZE];

  //-------------------------------------------------------
  // Set-up the attribute values for this operation.
  //-------------------------------------------------------
  attrValue[0] = threadBase;
  attrValue[1] = aIndex;
  for (unsigned k = 2; k < loopCountAttributes; k++) {
    attrValue[k] = aIndex;
  }//for
  localNdbOperation = localNdbConnection->getNdbOperation(tableName[0]);        
  if (localNdbOperation == NULL) {
    error_handler(localNdbConnection->getNdbError());
  }//if
  switch (aType) {
  case stInsert: {   // Insert case
    if (theWriteFlag == 1 && theDirtyFlag == 1) {
      localNdbOperation->dirtyWrite();
    } else if (theWriteFlag == 1) {
      localNdbOperation->writeTuple();
    } else {
      localNdbOperation->insertTuple();
    }//if
    break;
  }//case
  case stRead: {     // Read Case
    if (theSimpleFlag == 1) {
      localNdbOperation->simpleRead();
    } else if (theDirtyFlag == 1) {
      localNdbOperation->dirtyRead();
    } else {
      localNdbOperation->readTuple();
    }//if
    break;
  }//case
  case stUpdate: {    // Update Case
    if (theWriteFlag == 1 && theDirtyFlag == 1) {
      localNdbOperation->dirtyWrite();
    } else if (theWriteFlag == 1) {
      localNdbOperation->writeTuple();
    } else if (theDirtyFlag == 1) {
      localNdbOperation->dirtyUpdate();
    } else {
      localNdbOperation->updateTuple();
    }//if
    break;
  }//case
  case stDelete: {   // Delete Case
    localNdbOperation->deleteTuple();
    break;
  }//case
  default: {
    error_handler(localNdbOperation->getNdbError()); 
  }//default
  }//switch

  localNdbOperation->equal((Uint32)0, (char*)(attrValue + 0));
  localNdbOperation->equal((Uint32)1, (char*)(attrValue + 1));
  switch (aType) {
  case stInsert:      // Insert case
  case stUpdate:      // Update Case
    {
      for (countAttributes = 1;
           countAttributes < loopCountAttributes; countAttributes++) {
        localNdbOperation->setValue(countAttributes + 1,
                                    (char*)&attrValue[0]);
      }//for
      break;
    }//case
  case stRead: {      // Read Case
    for (countAttributes = 1;
         countAttributes < loopCountAttributes; countAttributes++) {
      localNdbOperation->getValue(countAttributes + 1,
                                  (char*)&attrValue[0]);
    }//for
    break;
  }//case
  case stDelete: {    // Delete Case
    break;
  }//case
  default: {
    //goto error_handler; < epaulsa
    error_handler(localNdbOperation->getNdbError());
  }//default
  }//switch
  return;
}//defineOperation()


static void
defineNdbRecordOperation(char *record,
                         NdbConnection* pTrans,
                         StartType aType,
                         Uint32 threadBase,
                         Uint32 aIndex)
{
  Uint32 offset;
  NdbDictionary::getOffset(g_record[0], 0, offset);
  * (Uint32*)(record + offset) = threadBase;
  * (Uint32*)(record + offset + 4) = aIndex;
  
  //-------------------------------------------------------
  // Set-up the attribute values for this operation.
  //-------------------------------------------------------
  if (aType != stRead && aType != stDelete)
  {
    for (unsigned k = 1; k < tNoOfAttributes; k++) {
      NdbDictionary::getOffset(g_record[0], k, offset);
      * (Uint32*)(record + offset) = aIndex;    
    }//for
  }
  
  const NdbOperation* op;
  switch (aType) {
  case stInsert: {   // Insert case
    if (theWriteFlag == 1)
    {
      op = pTrans->writeTuple(g_record[0],record,g_record[0],record);
    }
    else
    {
      op = pTrans->insertTuple(g_record[0],record,g_record[0],record);
    }
    break;
  }//case
  case stRead: {     // Read Case
    op = pTrans->readTuple(g_record[0],
                           record,
                           g_record[0],
                           record,
                           NdbOperation::LM_CommittedRead);
    break;
  }//case
  case stUpdate:{    // Update Case
    op = pTrans->updateTuple(g_record[0],
                             record,
                             g_record[0],
                             record);
    break;
  }//case
  case stDelete: {   // Delete Case
    op = pTrans->deleteTuple(g_record[0],
                             record,
                             g_record[0]);
    break;
  }//case
  default: {
    abort();
  }//default
  }//switch

  if (op == NULL)
  {
    ndbout << "Operation is null " << pTrans->getNdbError() << endl;
    abort();
  }
    
  assert(op != 0);
}

static void setAttrNames()
{
  int i;

  for (i = 0; i < MAXATTR ; i++){
    BaseString::snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
  }
}


static void setTableNames()
{
  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables wits SQL
  int i;
  for (i = 0; i < MAXTABLES ; i++){
    if (theStdTableNameFlag==0){
      BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d_%u", i, 
               (unsigned)(NdbTick_CurrentMillisecond()+rand()));
    } else {
      BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d", tStdTableNum);
    }
    ndbout << "Using table name " << tableName[0] << endl;
  }
}

static void
dropTables(Ndb* pMyNdb)
{
  int i;
  for (i = 0; i < MAXTABLES; i++)
  {
    ndbout << "Dropping table " << tableName[i] << "..." << endl;
    pMyNdb->getDictionary()->dropTable(tableName[i]);
  }
}

/*
  Set up nodeTableArray with a boolean true for all nodes that
  contains the table.
*/
static int
setUpNodeTableArray(Uint32 tableNo, const NdbDictionary::Table *pTab)
{
  Uint32 numFragments = pTab->getFragmentCount();
  Uint32 nodeId;
  for (Uint32 i = 1; i <= NDB_MAX_NODES; i++)
    nodeTableArray[tableNo][i] = false;
  for (Uint32 i = 0; i < numFragments; i++)
  {
    if ((pTab->getFragmentNodes(i, &nodeId, (Uint32)1)) == 0)
    {
      return 1;
    }
    nodeTableArray[tableNo][nodeId] = true;
  }
  numberNodeTable[tableNo] = 0;
  for (Uint32 i = 1; i <= NDB_MAX_NODES; i++)
  {
    if (nodeTableArray[tableNo][i])
      numberNodeTable[tableNo]++;
  }
  return 0;
}

static Uint32
get_node_relative_id(Uint32 tableNo, Uint32 node_id)
{
  Uint32 rel_id = 0;

  for (Uint32 i = 1; i < node_id; i++)
  {
    if (nodeTableArray[tableNo][i])
      rel_id++;
  }
  return rel_id;
}

static Uint32
get_node_count(Uint32 tableNo)
{
  return get_node_relative_id(tableNo, NDB_MAX_NODES + 1);
}

static Uint32
get_my_node_id(Uint32 tableNo, Uint32 threadNo)
{
  Uint32 count = 0;
  Uint32 n = threadNo % numberNodeTable[tableNo];
  for (Uint32 i = 1; i <= NDB_MAX_NODES; i++)
  {
    if (nodeTableArray[tableNo][i])
    {
      if (count == n)
        return i;
      count++;
    }
  }
  return 0;
}

static
int 
createTables(Ndb* pMyNdb){

  NdbDictionary::Dictionary* pDict = pMyNdb->getDictionary();
  if (theTableCreateFlag == 0 || tRunType == RunCreateTable)
  {
    for(int i=0; i < MAXTABLES ;i++)
    {
      ndbout << "Creating " << tableName[i] << "..." << endl;

      NdbDictionary::Table tab;
      tab.setName(tableName[i]);
      if (tempTable)
      {
        tab.setLogging(false);
      }

      {
        NdbDictionary::Column distkey;
        distkey.setName("DISTKEY");
        distkey.setType(NdbDictionary::Column::Unsigned);
        distkey.setPrimaryKey(true);
        distkey.setDistributionKey(true);
        tab.addColumn(distkey);
      }

      {
        NdbDictionary::Column pk;
        pk.setName(attrName[0]);
        pk.setType(NdbDictionary::Column::Unsigned);
        pk.setPrimaryKey(true);
        tab.addColumn(pk);
      }

      for (unsigned j = 1; j < tNoOfAttributes ; j++)
      {
        NdbDictionary::Column col;
        col.setName(attrName[j]);
        col.setType(NdbDictionary::Column::Unsigned);
        col.setLength(tAttributeSize);
        tab.addColumn(col);
      }

      int res = pDict->createTable(tab);
      if (res != 0)
      {
        ndbout << pDict->getNdbError() << endl;
        return -1;
      }
    }
  }

  for(int i=0; i < MAXTABLES ;i++)
  {
    const NdbDictionary::Table * pTab = pDict->getTable(tableName[i]);
    if (pTab == NULL)
    {
      error_handler(pDict->getNdbError());
      return -1;
    }
    tables[i] = pTab;
    if (setUpNodeTableArray(i, pTab))
    {
      error_handler(pDict->getNdbError());
      return -1;
    }
  }

  if (tNdbRecord)
  {
    for(int i=0; i < MAXTABLES ;i++)
    {
      const NdbDictionary::Table * pTab = tables[i];

      int off = 0;
      Vector<NdbDictionary::RecordSpecification> spec;
      for (Uint32 j = 0; j<unsigned(pTab->getNoOfColumns()); j++)
      {
        NdbDictionary::RecordSpecification r0;
        r0.column = pTab->getColumn(j);
        r0.offset = off;
        off += (r0.column->getSizeInBytes() + 3) & ~(Uint32)3;
        spec.push_back(r0);
      }
      g_record[i] = 
	  pDict->createRecord(pTab, spec.getBase(), 
			      spec.size(),
			      sizeof(NdbDictionary::RecordSpecification));
      assert(g_record[i]);
    }
  }
  return 0;
}

static
bool error_handler(const NdbError & err){
  ndbout << err << endl ;
  switch(err.classification){
  case NdbError::TemporaryResourceError:
  case NdbError::OverloadError:
  case NdbError::SchemaError:
    ndbout << endl << "Attempting to recover and continue now..." << endl ;
    return true;
  default:
    break;
  }
  return false ; // return false to abort
}

static void
setAggregateRun(void)
{
  tNoOfLoops = 1;
  tExtraReadLoop = 0;
  theTableCreateFlag = 1;
}

/* Start NEW Module */

/**
 * This part contains the code used for the case --local 4 which is using
 * the design pattern that could be used for asynchronous applications of
 * the NDB API.
 *
 * This variant will always use transaction hints, it will always the
 * NDB Record format in the NDB API.
 */

static void* definer_thread(void *data);
static void* executor_thread(void *data);

static Uint32 tNoOfExecutorThreads = 0;
static Uint32 tNoOfDefinerThreads = 0;

enum RunState
{
  WARMUP = 0,
  EXECUTING = 1,
  COOLDOWN = 2
};

RunState tRunState = WARMUP;

typedef struct KeyOperation KEY_OPERATION;
struct KeyOperation
{
  Uint32 first_key;
  Uint32 second_key;
  Uint32 definer_thread_id;
  Uint32 executor_thread_id;
  RunType operation_type;
  KEY_OPERATION *next_key_op;
};

typedef struct key_list_header KEY_LIST_HEADER;
struct key_list_header
{
  KEY_OPERATION *first_in_list;
  KEY_OPERATION *last_in_list;
  Uint32 num_in_list;
};


typedef struct thread_data_struct THREAD_DATA;
struct thread_data_struct
{
  KEY_LIST_HEADER list_header;
  Uint32 thread_id;
  bool ready;
  bool stop;
  bool start;

  char *record;
  NdbMutex *transport_mutex;
  struct NdbCondition *transport_cond;
  struct NdbCondition *main_cond;
  struct NdbCondition *start_cond;
  char not_used[52];
};
THREAD_DATA thread_data_array[MAX_DEFINER_THREADS + MAX_EXECUTOR_THREADS];

static Uint64
get_total_transactions()
{
  Uint64 total_transactions = 0;

  for (Uint32 i = tNoOfDefinerThreads; i < tNoOfThreads; i++)
  {
    total_transactions += ThreadExecutions[i];
  }
  return total_transactions;
}

static void
init_list_headers(KEY_LIST_HEADER *list_header,
                  Uint32 num_list_headers)
{
  Uint32 i;
  KEY_LIST_HEADER *list_header_ref;
  char *list_header_ptr = (char*)list_header;
  for (i = 0;
       i < num_list_headers;
       i++, list_header_ptr += sizeof(KEY_LIST_HEADER))
  {
    list_header_ref = (KEY_LIST_HEADER*)list_header_ptr;
    list_header_ref->first_in_list = NULL;
    list_header_ref->last_in_list = NULL;
    list_header_ref->num_in_list = 0;
  }
}

static void
wait_thread_ready(THREAD_DATA *my_thread_data)
{
  NdbMutex_Lock(my_thread_data->transport_mutex);
  while (1)
  {
    if (my_thread_data->ready)
      break;
    NdbCondition_Wait(my_thread_data->main_cond,
                      my_thread_data->transport_mutex);
  }
  NdbMutex_Unlock(my_thread_data->transport_mutex);
}

static void
wait_for_threads_ready(Uint32 num_threads)
{
  for (Uint32 i = 0; i < num_threads; i++)
    wait_thread_ready(&thread_data_array[i]);
}

static void
signal_thread_to_start(THREAD_DATA *my_thread_data)
{
  NdbMutex_Lock(my_thread_data->transport_mutex);
  my_thread_data->start = true;
  my_thread_data->ready = false;
  NdbCondition_Signal(my_thread_data->start_cond);
  NdbMutex_Unlock(my_thread_data->transport_mutex);
}

static void
signal_definer_threads_to_start()
{
  for (Uint32 i = 0; i < tNoOfDefinerThreads; i++)
    signal_thread_to_start(&thread_data_array[i]);
}

static void
signal_executor_threads_to_start()
{
  for (Uint32 i = 0; i < tNoOfExecutorThreads; i++)
    signal_thread_to_start(&thread_data_array[tNoOfDefinerThreads + i]);
}

static void
signal_thread_ready_wait_for_start(THREAD_DATA *my_thread_data)
{
  NdbMutex_Lock(my_thread_data->transport_mutex);
  my_thread_data->ready = true;
  NdbCondition_Signal(my_thread_data->main_cond);
  while (1)
  {
    if (my_thread_data->start)
      break;
    NdbCondition_Wait(my_thread_data->start_cond,
                      my_thread_data->transport_mutex);
  }
  my_thread_data->start = false;
  NdbMutex_Unlock(my_thread_data->transport_mutex);
}

static void
signal_thread_to_stop(THREAD_DATA *my_thread_data)
{
  NdbMutex_Lock(my_thread_data->transport_mutex);
  my_thread_data->stop = true;
  NdbCondition_Signal(my_thread_data->transport_cond);
  NdbMutex_Unlock(my_thread_data->transport_mutex);
}

static void
signal_definer_threads_to_stop()
{
  for (Uint32 i = 0; i < tNoOfDefinerThreads; i++)
    signal_thread_to_stop(&thread_data_array[i]);
}

static void
signal_executor_threads_to_stop()
{
  for (Uint32 i = tNoOfDefinerThreads; i < tNoOfThreads; i++)
    signal_thread_to_stop(&thread_data_array[i]);
}

static void
destroy_thread_data(THREAD_DATA *my_thread_data)
{
  free(my_thread_data->record);
  NdbMutex_Destroy(my_thread_data->transport_mutex);
  NdbCondition_Destroy(my_thread_data->transport_cond);
  NdbCondition_Destroy(my_thread_data->start_cond);
  NdbCondition_Destroy(my_thread_data->main_cond);
}

static void
init_thread_data(THREAD_DATA *my_thread_data, Uint32 thread_id)
{
  Uint32 sz = NdbDictionary::getRecordRowLength(g_record[0]);
  my_thread_data->record = (char*)malloc(sz);
  memset(my_thread_data->record, 0, sz);
  init_list_headers(&my_thread_data->list_header, 1);
  my_thread_data->stop = false;
  my_thread_data->ready = false;
  my_thread_data->start = false;
  my_thread_data->transport_mutex = NdbMutex_Create();
  my_thread_data->transport_cond = NdbCondition_Create();
  my_thread_data->main_cond = NdbCondition_Create();
  my_thread_data->start_cond = NdbCondition_Create();
  my_thread_data->thread_id = thread_id;
}

static void
create_definer_thread(THREAD_DATA *my_thread_data, Uint32 thread_id)
{
  init_thread_data(my_thread_data, thread_id);
  threadLife[thread_id] = NdbThread_Create(definer_thread,
                                           (void**)my_thread_data,
                                           1024 * 1024,
                                           "flexAsynchThread",
                                           NDB_THREAD_PRIO_LOW);
}

static void
create_definer_threads()
{
  for (Uint32 i = 0; i < tNoOfDefinerThreads; i++)
  {
    Uint32 thread_id = i;
    create_definer_thread(&thread_data_array[thread_id], thread_id);
  }
}

static void
create_executor_thread(THREAD_DATA *my_thread_data, Uint32 thread_id)
{
  init_thread_data(my_thread_data, thread_id);
  threadLife[thread_id] = NdbThread_Create(executor_thread,
                                           (void**)my_thread_data,
                                           1024 * 1024,
                                           "flexAsynchThread",
                                           NDB_THREAD_PRIO_LOW);
}

static void
create_executor_threads()
{
  for (Uint32 i = 0; i < tNoOfExecutorThreads; i++)
  {
    Uint32 thread_id = tNoOfDefinerThreads + i;
    create_executor_thread(&thread_data_array[thread_id], thread_id);
  }
}

static void
main_thread(RunType start_type, NdbTimer & timer)
{
  bool insert_delete;

  tNoOfExecutorThreads = tNoOfThreads;
  if (tNoOfDefinerThreads == 0)
  {
    tNoOfDefinerThreads = (tNoOfThreads + 3)/4;
  }
  tNoOfThreads = tNoOfExecutorThreads + tNoOfDefinerThreads;

  if (start_type == RunInsert ||
      start_type == RunDelete)
    insert_delete = true;
  else
    insert_delete = false;

  create_definer_threads();
  create_executor_threads();

  wait_for_threads_ready(tNoOfThreads);

  /**
   * Start threads, start with execution threads to ensure they are
   * up and running before definer threads starts sending data to
   * them
   */
  START_TIMER;
  signal_definer_threads_to_start();
  signal_executor_threads_to_start();

  if (!insert_delete)
  {
    sleep(tWarmupTime);
    tRunState = EXECUTING;
    sleep(tExecutionTime);
    tRunState = COOLDOWN;
    sleep(tCooldownTime);
    signal_definer_threads_to_stop();
  }
  wait_for_threads_ready(tNoOfDefinerThreads);
  STOP_TIMER;

  signal_executor_threads_to_stop();
  wait_for_threads_ready(tNoOfThreads);

  /**
   * Now all threads are stopped and prepared to be destroyed,
   * now start them just to destroy themselves
   */
  signal_definer_threads_to_start();
  signal_executor_threads_to_start();

  void * tmp;
  for (Uint32 i = 0; i < tNoOfThreads; i++)
  {
    NdbThread_WaitFor(threadLife[i], &tmp);
    NdbThread_Destroy(&threadLife[i]);
  }
}

static NdbConnection*
get_trans_object(Uint32 first_key,
                 Uint32 second_key,
                 Ndb *my_ndb)
{
  union {
    Uint64 _align;
    Uint32 Tkey32[2];
  };
  (void)_align;

  Tkey32[0] = first_key;
  Tkey32[1] = second_key;
  Ndb::Key_part_ptr hint[2];
  hint[0].ptr = Tkey32+0;
  hint[0].len = 4;
  hint[1].ptr = 0;
  hint[1].len = 0;

  return my_ndb->startTransaction(tables[0], hint);
}

static Ndb*
get_ndb_object(Uint32 my_thread_id)
{
  Ndb *my_ndb = new Ndb(g_cluster_connection+(my_thread_id % tConnections),
                        "TEST_DB");
  my_ndb->init(MAXPAR);
  my_ndb->waitUntilReady(10000);
  return my_ndb;
}

static void
insert_list(KEY_LIST_HEADER *list_header,
            KEY_OPERATION *insert_op)
{
  KEY_OPERATION *current_last = list_header->last_in_list;
  insert_op->next_key_op = NULL;
  list_header->last_in_list = insert_op;
  if (current_last)
    current_last->next_key_op = insert_op;
  else
    list_header->first_in_list = insert_op;
  list_header->num_in_list++;
}

static KEY_OPERATION*
get_first_free(KEY_LIST_HEADER *list_header)
{
  assert(list_header->first_in_list);
  KEY_OPERATION *key_op = list_header->first_in_list;
  list_header->first_in_list = key_op->next_key_op;
  list_header->num_in_list--;
  if (!list_header->first_in_list)
  {
    list_header->last_in_list = NULL;
  }
  key_op->next_key_op = NULL;
  return key_op;
}

static void
move_list(KEY_LIST_HEADER *src_list_header,
          KEY_LIST_HEADER *dst_list_header)
{
  KEY_OPERATION *last_completed_op = dst_list_header->last_in_list;
  KEY_OPERATION *first_in_list = src_list_header->first_in_list;
  if (!first_in_list)
    return;
  if (last_completed_op)
  {
    last_completed_op->next_key_op = first_in_list;
  }
  else
  {
    dst_list_header->first_in_list = first_in_list;
  }
  dst_list_header->last_in_list = src_list_header->last_in_list;
  dst_list_header->num_in_list += src_list_header->num_in_list;
  src_list_header->num_in_list = 0;
  src_list_header->first_in_list = NULL;
  src_list_header->last_in_list = NULL;
}

/**
 * Retrieve a linked list of prepared operations. If no operations
 * prepared we wait on a condition until operations are defined for
 * us to execute.
 */
static void
receive_operations(THREAD_DATA *my_thread_data,
                   KEY_LIST_HEADER *list_header,
                   bool wait)
{
  bool first = true;
  KEY_LIST_HEADER *thread_list_header = &my_thread_data->list_header;
  list_header->first_in_list = NULL;
  list_header->last_in_list = NULL;
  list_header->num_in_list = 0;

recheck:
  NdbMutex_Lock(my_thread_data->transport_mutex);
  while (!my_thread_data->stop &&
         (first || thread_list_header->first_in_list))
  {
    move_list(thread_list_header, list_header);
    if (list_header->first_in_list)
      break;
    NdbCondition_Wait(my_thread_data->transport_cond,
                      my_thread_data->transport_mutex);
    first = false;
  }
  NdbMutex_Unlock(my_thread_data->transport_mutex);
  if (first && wait &&
      thread_list_header->num_in_list < ((tNoOfParallelTrans + 1) / 2))
  {
    /**
     * We will wait for at least 200 microseconds if we haven't yet received
     * at least half of the number of records we desire to execute.
     */
    NdbSleep_MicroSleep(200);
    first = false;
    goto recheck;
  }
}

static void
send_operations(Uint32 thread_id,
                KEY_LIST_HEADER *list_header)
{
  THREAD_DATA *recv_thread = &thread_data_array[thread_id];

  NdbMutex_Lock(recv_thread->transport_mutex);
  /**
   * We are moving operations into the list, thus we need
   * to wake any threads waiting for operations to execute.
   */
  move_list(list_header,
            &recv_thread->list_header);
  NdbCondition_Signal(recv_thread->transport_cond);
  NdbMutex_Unlock(recv_thread->transport_mutex);
}

static void
init_key_op_list(char *key_op_ptr,
                 KEY_LIST_HEADER *list_header,
                 Uint32 max_outstanding,
                 Uint32 my_thread_id,
                 RunType my_run_type)
{
  KEY_OPERATION *key_op;

  list_header->first_in_list = (KEY_OPERATION*)key_op_ptr;
  for (Uint32 i = 0;
       i < max_outstanding;
       i++, key_op_ptr += sizeof(KEY_OPERATION))
  {
    key_op = (KEY_OPERATION*)key_op_ptr;
    key_op->next_key_op = (KEY_OPERATION*)(key_op_ptr + sizeof(KEY_OPERATION));
    key_op->definer_thread_id = my_thread_id;
    key_op->executor_thread_id = MAX_EXECUTOR_THREADS;
    key_op->operation_type = my_run_type;
  }
  key_op->next_key_op = NULL; /* Last key operation */
  list_header->last_in_list = key_op;
  list_header->num_in_list = max_outstanding;
}

static Uint32
get_thread_id_for_record(Uint32 record_id,
                         Uint32 node_count,
                         Uint32 thread_count,
                         Uint32 thread_group,
                         Uint32 num_thread_groups,
                         Ndb *my_ndb)
{
  Uint32 thread_id;
  NdbConnection *trans = get_trans_object(record_id, record_id, my_ndb);
  Uint32 node_id = trans->getConnectedNodeId();
  trans->close();
  Uint32 node_rel_id = get_node_relative_id((Uint32)0, node_id);
  if (node_count >= thread_count)
  {
    thread_id = node_rel_id % thread_count;
    return thread_id;
  }

recalculate:
  thread_id = thread_group * node_count + node_rel_id;
  if (thread_id >= thread_count)
  {
    /**
     * Only the last thread group may have less than
     * node_count threads, so choosing a random group
     * except the last will always give a valid
     * thread_id.
     */
    thread_group = rand() % (num_thread_groups - 1);
    goto recalculate;
  }
  return thread_id;
}

void init_thread_id_mem(char *thread_id_mem,
                        Uint32 first_record,
                        Uint32 total_records,
                        Ndb *my_ndb)
{
  Uint32 node_count = get_node_count((Uint32)0);
  Uint32 thread_count = tNoOfExecutorThreads;
  Uint32 num_thread_groups = (thread_count + node_count - 1) / node_count;
  Uint32 thread_group = 0;
  for (Uint32 record_id = first_record, i = 0;
       i < total_records;
       i++, record_id++)
  {
    thread_id_mem[i] = (char)get_thread_id_for_record(record_id,
                                                      node_count,
                                                      thread_count,
                                                      thread_group,
                                                      num_thread_groups,
                                                      my_ndb);
    thread_group++;
    if (thread_group == num_thread_groups)
      thread_group = 0;
  }
}

static bool
check_for_outstanding(Uint32 *thread_state)
{
  for (Uint32 i = 0; i < tNoOfExecutorThreads; i++)
  {
    if (thread_state[i])
      return true;
  }
  return false;
}

static void
update_thread_state(KEY_LIST_HEADER *list_header,
                    Uint32 *thread_state)
{
  KEY_OPERATION *key_op = list_header->first_in_list;

  while (key_op)
  {
    thread_state[key_op->executor_thread_id]--;
    key_op->executor_thread_id = MAX_EXECUTOR_THREADS;
    key_op = key_op->next_key_op;
  }
}

static void
wait_until_all_completed(THREAD_DATA *my_thread_data,
                         Uint32 *thread_state,
                         KEY_LIST_HEADER *free_list_header)
{
  KEY_LIST_HEADER list_header;
  bool outstanding = true;
  while (outstanding && !my_thread_data->stop)
  {
    receive_operations(my_thread_data, &list_header, false);
    update_thread_state(&list_header, thread_state);
    move_list(&list_header, free_list_header);
    outstanding = check_for_outstanding(thread_state);
  }
}

static Uint32
prepare_operations(char *thread_id_mem,
                   KEY_LIST_HEADER *free_list_header,
                   Uint32 *thread_state,
                   Uint32 first_record_to_define,
                   Uint32 num_records_to_define,
                   Uint32 first_record,
                   Uint32 last_record,
                   Uint32 max_per_thread)
{
  KEY_LIST_HEADER thread_list_headers[MAX_EXECUTOR_THREADS];
  Uint32 record_id, i, num_records;

  init_list_headers(&thread_list_headers[0], tNoOfExecutorThreads);
  for (record_id = first_record_to_define, i = 0;
       record_id <= last_record && i < num_records_to_define;
       record_id++, i++)
  {
    KEY_OPERATION *define_op = get_first_free(free_list_header);
    Uint32 thread_id = (Uint32)thread_id_mem[record_id - first_record];
    define_op->first_key = record_id;
    define_op->second_key = record_id;
    define_op->executor_thread_id = thread_id;
    thread_state[thread_id]++;
    KEY_LIST_HEADER *thread_list_header = &thread_list_headers[thread_id];
    insert_list(thread_list_header, define_op);
    if (thread_list_header->num_in_list >= max_per_thread)
    {
      /**
       * One thread has max number of records, we won't define any
       * more to keep the code simple.
       */
      break;
    }
  }
  num_records = i;
  for (i = 0; i < tNoOfExecutorThreads; i++)
  {
    KEY_LIST_HEADER *thread_list_header = &thread_list_headers[i];
    if (thread_list_header->num_in_list)
    {
      send_operations(tNoOfDefinerThreads + i, thread_list_header);
    }
  }
  return num_records;
}

static void*
definer_thread(void *data)
{
  THREAD_DATA *my_thread_data = (THREAD_DATA*)data;
  Uint32 my_thread_id = my_thread_data->thread_id;
  RunType  run_type = tRunType;
  Uint32 thread_state[MAX_EXECUTOR_THREADS];
  Uint32 max_outstanding = (tNoOfExecutorThreads * tNoOfParallelTrans) /
                            tNoOfDefinerThreads;
  Uint32 max_per_thread = 1000 / tNoOfDefinerThreads;
  Uint32 total_records = max_outstanding * tNoOfTransactions;
  Uint32 first_record = total_records * my_thread_id;
  Uint32 my_last_record = first_record + total_records - 1;
  Uint32 current_record = first_record;
  KEY_LIST_HEADER free_list_header;
  void *key_op_mem = malloc(sizeof(KEY_OPERATION) * max_outstanding);
  char *thread_id_mem = (char*)malloc(total_records);
  memset((char*)&thread_state[0], 0, sizeof(thread_state));

  init_key_op_list((char*)key_op_mem,
                   &free_list_header,
                   max_outstanding,
                   my_thread_id,
                   run_type);
  Ndb *my_ndb = get_ndb_object(my_thread_id);
  init_thread_id_mem(thread_id_mem,
                     first_record,
                     total_records,
                     my_ndb);
  delete my_ndb;
  ThreadExecutions[my_thread_id] = 0;
  signal_thread_ready_wait_for_start(my_thread_data);

  while (!my_thread_data->stop)
  {
    Uint32 defined_ops = prepare_operations(thread_id_mem,
                                            &free_list_header,
                                            &thread_state[0],
                                            current_record,
                                            max_outstanding,
                                            first_record,
                                            my_last_record,
                                            max_per_thread);
    current_record += defined_ops;
    if (defined_ops)
    {
      wait_until_all_completed(my_thread_data,
                               &thread_state[0],
                               &free_list_header);
    }
    if (current_record > my_last_record)
    {
      if (run_type != RunRead &&
          run_type != RunUpdate)
      {
        /**
         * Inserts and deletes are done when first round is
         * completed. Reads and updates proceed until time is
         * completed.
         */
        break;
      }
      current_record = first_record;
    }
  }
  signal_thread_ready_wait_for_start(my_thread_data);
  free(key_op_mem);
  free(thread_id_mem);
  destroy_thread_data(my_thread_data);
  return NULL;
}

/**
 * This method receives a linked list of key operations and executes
 * all of them.
 *
 * Return Value: >= 0 means successful completion of this many operations
 *               -1 Failure, stop test
 */
static int
execute_operations(char *record,
                   Ndb* my_ndb,
                   KEY_OPERATION *key_op)
{
  NdbConnection* ndb_conn_array[MAXPAR];
  Uint32 num_ops = 0;

  while (key_op)
  {
    ndb_conn_array[num_ops] = get_trans_object(key_op->first_key,
                                               key_op->second_key,
                                               my_ndb);
    if (ndb_conn_array[num_ops] == NULL){
      error_handler(my_ndb->getNdbError());
      ndbout << endl << "Unable to recover! Quitting now" << endl ;
      return -1;
    }
    //-------------------------------------------------------
    // Define the operation, but do not execute it yet.
    //-------------------------------------------------------
    defineNdbRecordOperation(record,
                             ndb_conn_array[num_ops],
                             (StartType)key_op->operation_type,
                             key_op->first_key,
                             key_op->second_key);

    ndb_conn_array[num_ops]->executeAsynchPrepare(Commit,
                                        &executeCallback,
                                        (void*)&ndb_conn_array[num_ops]);
    num_ops++;
    key_op = key_op->next_key_op;
  }
  if (num_ops == 0)
    return 0;

  /**
   * Now execute each defined operation and wait for all of them to
   * complete.
   */
  int Tcomp = my_ndb->sendPollNdb(3000,
                                  num_ops,
                                  tSendForce);
  if (Tcomp != (int)num_ops &&
      my_ndb->getNdbError().code != 0)
  {
    /* Error handling */
    if (error_count > 100)
      return -1;

    error_count++;
    ndbout << "error = " << my_ndb->getNdbError().code << endl;
  }
  return Tcomp;
}

static void
report_back_operations(KEY_OPERATION *first_defined_op)
{
  KEY_LIST_HEADER thread_list_header[MAX_DEFINER_THREADS];
  KEY_OPERATION *next_op, *executed_op;

  init_list_headers(&thread_list_header[0], tNoOfDefinerThreads);
  executed_op = first_defined_op;
  while (executed_op)
  {
    next_op = executed_op->next_key_op;
    insert_list(&thread_list_header[executed_op->definer_thread_id],
                executed_op);
    executed_op = next_op;
  }
  for (Uint32 i = 0; i < tNoOfDefinerThreads; i++)
  {
    if (thread_list_header[i].first_in_list)
    {
      send_operations(i, &thread_list_header[i]);
    }
  }
}

/**
 * This is the main function of the executor threads, these threads
 * receive linked lists of operations to execute from the definer
 * threads. The definer threads stops these threads by simply
 * sending a stop operation.
 */
static void*
executor_thread(void *data)
{
  THREAD_DATA *my_thread_data = (THREAD_DATA*)data;
  Uint32 my_thread_id = my_thread_data->thread_id;
  Uint64 exec_count = 0;
  Uint32 error_count = 0;
  Uint32 executions = 0;
  Uint32 error_flag = false;
  int ret_code;
  KEY_LIST_HEADER list_header;

  Ndb *my_ndb = get_ndb_object(my_thread_id);
  ThreadExecutions[my_thread_id] = 0;

  signal_thread_ready_wait_for_start(my_thread_data);

  while (!my_thread_data->stop)
  {
    receive_operations(my_thread_data, &list_header, !tImmediate);
    if (list_header.num_in_list == 0)
    {
      break;
    }
    ret_code = 0;
    if (!error_flag)
    {
      /* Ignore to execute after errors to simplify error handling */
      ret_code = execute_operations(my_thread_data->record,
                                    my_ndb,
                                    list_header.first_in_list);
    }
    report_back_operations(list_header.first_in_list);
    if (ret_code < 0)
    {
      ndbout_c("executor thread id = %u received error", my_thread_id);
      error_flag = true;
    }
    else if (!error_flag &&
             (tRunType == RunInsert ||
              tRunType == RunDelete ||
              tRunState == EXECUTING))
    {
      executions++;
      exec_count += (Uint64)ret_code;
    }
  }

  ThreadExecutions[my_thread_id] = exec_count;
  if (error_count)
  {
    ndbout_c("Received %u errors in executor thread, id = %u",
             error_count, my_thread_id);
  }
  signal_thread_ready_wait_for_start(my_thread_data);
  delete my_ndb;
  destroy_thread_data(my_thread_data);
  return NULL;
}

/* End NEW Module */

static
int 
readArguments(int argc, const char** argv){
  
  int i = 1;
  while (argc > 1){
    if (strcmp(argv[i], "-t") == 0){
      tNoOfThreads = atoi(argv[i+1]);
      if ((tNoOfThreads < 1) || (tNoOfThreads > NDB_MAXTHREADS)){
	ndbout_c("Invalid no of threads");
        return -1;
      }
    }
    else if (strcmp(argv[i], "-d") == 0)
    {
      tNoOfDefinerThreads = atoi(argv[i+1]);
      if (tNoOfDefinerThreads > NDB_MAXTHREADS)
      {
        ndbout_c("Invalid no of definer threads");
        return -1;
      }
    } else if (strcmp(argv[i], "-p") == 0){
      tNoOfParallelTrans = atoi(argv[i+1]);
      if ((tNoOfParallelTrans < 1) || (tNoOfParallelTrans > MAXPAR)){
	ndbout_c("Invalid no of parallell transactions");
        return -1;
      }
    } else if (strcmp(argv[i], "-load_factor") == 0){
      tLoadFactor = atoi(argv[i+1]);
      if ((tLoadFactor < 40) || (tLoadFactor > 99)){
	ndbout_c("Invalid load factor");
        return -1;
      }
    } else if (strcmp(argv[i], "-c") == 0) {
      tNoOfOpsPerTrans = atoi(argv[i+1]);
      if (tNoOfOpsPerTrans < 1){
	ndbout_c("Invalid no of operations per transaction");
        return -1;
      }
    } else if (strcmp(argv[i], "-o") == 0) {
      tNoOfTransactions = atoi(argv[i+1]);
      if (tNoOfTransactions < 1){
	ndbout_c("Invalid no of transactions");
        return -1;
      }
    } else if (strcmp(argv[i], "-a") == 0){
      tNoOfAttributes = atoi(argv[i+1]);
      if ((tNoOfAttributes < 2) || (tNoOfAttributes > MAXATTR)){
	ndbout_c("Invalid no of attributes");
        return -1;
      }
    } else if (strcmp(argv[i], "-n") == 0){
      theStdTableNameFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-l") == 0){
      tNoOfLoops = atoi(argv[i+1]);
      if ((tNoOfLoops < 0) || (tNoOfLoops > 100000)){
	ndbout_c("Invalid no of loops");
        return -1;
      }
    } else if (strcmp(argv[i], "-s") == 0){
      tAttributeSize = atoi(argv[i+1]);
      if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE)){
	ndbout_c("Invalid attributes size");
        return -1;
      }
    } else if (strcmp(argv[i], "-local") == 0){
      tLocal = atoi(argv[i+1]);
      if (tLocal < 1 || (tLocal > 3)){
        ndbout_c("Invalid local value, only 1,2 or 3 allowed");
        return -1;
      }
      startTransGuess = true;
    } else if (strcmp(argv[i], "-simple") == 0){
      theSimpleFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-adaptive") == 0){
      tSendForce = 0;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-force") == 0){
      tSendForce = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-non_adaptive") == 0){
      tSendForce = 2;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-write") == 0){
      theWriteFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-dirty") == 0){
      theDirtyFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-test") == 0){
      theTestFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-no_table_create") == 0){
      theTableCreateFlag = 1;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-temp") == 0){
      tempTable = true;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-no_hint") == 0){
      startTransGuess = false;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-ndbrecord") == 0){
      tNdbRecord = true;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-r") == 0){
      tExtraReadLoop = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-con") == 0){
      tConnections = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-insert") == 0){
      setAggregateRun();
      tRunType = RunInsert;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-read") == 0){
      setAggregateRun();
      tRunType = RunRead;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-update") == 0){
      setAggregateRun();
      tRunType = RunUpdate;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-delete") == 0){
      setAggregateRun();
      tRunType = RunDelete;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-create_table") == 0){
      tRunType = RunCreateTable;
      argc++;
    }
    else if (strcmp(argv[i], "-new") == 0)
    {
      tNew = true;
      tNdbRecord = true;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-immediate") == 0)
    {
      tImmediate = true;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-drop_table") == 0){
      tRunType = RunDropTable;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-warmup_time") == 0){
      tWarmupTime = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-execution_time") == 0){
      tExecutionTime = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-cooldown_time") == 0){
      tCooldownTime = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-table") == 0){
      tStdTableNum = atoi(argv[i+1]);
      theStdTableNameFlag = 1;
    } else {
      return -1;
    }
    
    argc -= 2;
    i = i + 2;
  }//while
  if (tLocal > 0) {
    if (tNoOfOpsPerTrans != 1) {
      ndbout_c("Not valid to have more than one op per trans with local");
    }//if
    if (startTransGuess == false) {
      ndbout_c("Not valid to use no_hint with local");
    }//if
  }//if
  return 0;
}

static
void
input_error(){
  ndbout_c("FLEXASYNCH");
  ndbout_c("   Perform benchmark of insert, update and delete transactions");
  ndbout_c(" ");
  ndbout_c("Arguments:");
  ndbout_c("   -t Number of threads to start, default 1");
  ndbout_c("   -p Number of parallel transactions per thread, default 32");
  ndbout_c("   -o Number of transactions per loop, default 500");
  ndbout_c("   -l Number of loops to run, default 1, 0=infinite");
  ndbout_c("   -load_factor Number Load factor in index in percent (40 -> 99)");
  ndbout_c("   -a Number of attributes, default 25");
  ndbout_c("   -c Number of operations per transaction");
  ndbout_c("   -s Size of each attribute, default 1 ");
  ndbout_c("      (PK is always of size 1, independent of this value)");
  ndbout_c("   -simple Use simple read to read from database");
  ndbout_c("   -dirty Use dirty read to read from database");
  ndbout_c("   -write Use writeTuple in insert and update");
  ndbout_c("   -n Use standard table names");
  ndbout_c("   -no_table_create Don't create tables in db");
  ndbout_c("   -temp Create table(s) without logging");
  ndbout_c("   -no_hint Don't give hint on where to execute transaction coordinator");
  ndbout_c("   -adaptive Use adaptive send algorithm (default)");
  ndbout_c("   -force Force send when communicating");
  ndbout_c("   -non_adaptive Send at a 10 millisecond interval");
  ndbout_c("   -local 1 = each thread its own node, 2 = round robin on node per parallel trans 3 = random node per parallel trans");
  ndbout_c("   -ndbrecord Use NDB Record");
  ndbout_c("   -r Number of extra loops");
  ndbout_c("   -insert Only run inserts on standard table");
  ndbout_c("   -read Only run reads on standard table");
  ndbout_c("   -update Only run updates on standard table");
  ndbout_c("   -delete Only run deletes on standard table");
  ndbout_c("   -create_table Only run Create Table of standard table");
  ndbout_c("   -drop_table Only run Drop Table on standard table");
  ndbout_c("   -warmup_time Warmup Time before measurement starts");
  ndbout_c("   -execution_time Execution Time where measurement is done");
  ndbout_c("   -cooldown_time Cooldown time after measurement completed");
  ndbout_c("   -table Number of standard table, default 0");
}
  
static void
run_old_flexAsynch(ThreadNdb *pThreadData,
                   NdbTimer & timer)
{
  int                   tLoops=0;
  /****************************************************************
   *  Create NDB objects.                                   *
  ****************************************************************/
  resetThreads();
  for (Uint32 i = 0; i < tNoOfThreads ; i++)
  {
    pThreadData[i].ThreadNo = i;
    threadLife[i] = NdbThread_Create(threadLoop,
                                     (void**)&pThreadData[i],
                                     32768,
                                     "flexAsynchThread",
                                     NDB_THREAD_PRIO_LOW);
  }//for
  ndbout << endl <<  "All NDB objects and table created" << endl << endl;
  int noOfTransacts = tNoOfParallelTrans*tNoOfTransactions*tNoOfThreads;
  /****************************************************************
   * Execute program.                                             *
   ****************************************************************/

  for (;;)
  {

    int loopCount = tLoops + 1 ;
    ndbout << endl << "Loop # " << loopCount  << endl << endl ;

    /****************************************************************
     * Perform inserts.                                             *
     ****************************************************************/

    failed = 0 ;
    if (tRunType == RunAll || tRunType == RunInsert)
    {
      ndbout << "Executing inserts" << endl;
      START_TIMER;
      execute(stInsert);
      STOP_TIMER;
    }
    if (tRunType == RunAll)
    {
      a_i.addObservation((1000*noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
      PRINT_TIMER("insert", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed)
      {
        int i = retry_opt ;
        int ci = 1 ;
        while (0 < failed && 0 < i)
        {
          ndbout << failed << " of the transactions returned errors!"
                 << endl << endl;
          ndbout << "Attempting to redo the failed transactions now..."
                 << endl ;
          ndbout << "Redo attempt " << ci <<" out of " << retry_opt
                 << endl << endl;
          failed = 0 ;
          START_TIMER;
          execute(stInsert);
          STOP_TIMER;
          PRINT_TIMER("insert", noOfTransacts, tNoOfOpsPerTrans);
          i-- ;
          ci++;
        }
        if (0 == failed )
        {
          ndbout << endl <<"Redo attempt succeeded" << endl << endl;
        }
        else
        {
          ndbout << endl <<"Redo attempt failed, moving on now..." << endl
                 << endl;
        }//if
      }//if
    }//if
    /****************************************************************
     * Perform read.                                                *
     ****************************************************************/

    failed = 0 ;

    if (tRunType == RunAll || tRunType == RunRead)
    {
      for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
      {
        ndbout << "Executing reads" << endl;
        START_TIMER;
        execute(stRead);
        STOP_TIMER;
        if (tRunType == RunAll)
        {
          a_r.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
          PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
        }//if
      }//for
    }//if

    if (tRunType == RunAll)
    {
      if (0 < failed)
      {
        int i = retry_opt ;
        int cr = 1;
        while (0 < failed && 0 < i)
        {
          ndbout << failed << " of the transactions returned errors!"<<endl ;
          ndbout << endl;
          ndbout <<"Attempting to redo the failed transactions now..." << endl;
          ndbout << endl;
          ndbout <<"Redo attempt " << cr <<" out of ";
          ndbout << retry_opt << endl << endl;
          failed = 0 ;
          START_TIMER;
          execute(stRead);
          STOP_TIMER;
          PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
          i-- ;
          cr++ ;
        }//while
        if (0 == failed )
        {
          ndbout << endl <<"Redo attempt succeeded" << endl << endl ;
        }
        else
        {
          ndbout << endl <<"Redo attempt failed, moving on now..." << endl << endl ;
        }//if
      }//if
    }//if


    /****************************************************************
     * Perform update.                                              *
     ****************************************************************/

    failed = 0 ;

    if (tRunType == RunAll || tRunType == RunUpdate)
    {
      ndbout << "Executing updates" << endl;
      START_TIMER;
      execute(stUpdate);
      STOP_TIMER;
    }//if
    if (tRunType == RunAll)
    {
      a_u.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
      PRINT_TIMER("update", noOfTransacts, tNoOfOpsPerTrans) ;

      if (0 < failed)
      {
        int i = retry_opt ;
        int cu = 1 ;
        while (0 < failed && 0 < i)
        {
          ndbout << failed << " of the transactions returned errors!"<<endl ;
          ndbout << endl;
          ndbout <<"Attempting to redo the failed transactions now..." << endl;
          ndbout << endl <<"Redo attempt " << cu <<" out of ";
          ndbout << retry_opt << endl << endl;
          failed = 0 ;
          START_TIMER;
          execute(stUpdate);
          STOP_TIMER;
          PRINT_TIMER("update", noOfTransacts, tNoOfOpsPerTrans);
          i-- ;
          cu++ ;
        }//while
        if (0 == failed )
        {
          ndbout << endl <<"Redo attempt succeeded" << endl << endl;
        }
        else
        {
          ndbout << endl;
          ndbout <<"Redo attempt failed, moving on now..." << endl << endl;
        }//if
      }//if
    }//if

    /****************************************************************
     * Perform read.                                             *
     ****************************************************************/

    failed = 0 ;

    if (tRunType == RunAll)
    {
      for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
      {
        ndbout << "Executing reads" << endl;
        START_TIMER;
        execute(stRead);
        STOP_TIMER;
        a_r.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
        PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
      }

      if (0 < failed)
      {
        int i = retry_opt ;
        int cr2 = 1 ;
        while (0 < failed && 0 < i)
        {
          ndbout << failed << " of the transactions returned errors!"<<endl ;
          ndbout << endl;
          ndbout <<"Attempting to redo the failed transactions now..." << endl;
          ndbout << endl <<"Redo attempt " << cr2 <<" out of ";
          ndbout << retry_opt << endl << endl;
          failed = 0 ;
          START_TIMER;
          execute(stRead);
          STOP_TIMER;
          PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
          i-- ;
          cr2++ ;
        }//while
        if (0 == failed )
        {
          ndbout << endl <<"Redo attempt succeeded" << endl << endl;
        }
        else
        {
          ndbout << endl;
          ndbout << "Redo attempt failed, moving on now..." << endl << endl;
        }//if
      }//if
    }//if


    /****************************************************************
     * Perform delete.                                              *
     ****************************************************************/

    failed = 0 ;

    if (tRunType == RunAll || tRunType == RunDelete)
    {
      ndbout << "Executing deletes" << endl;
      START_TIMER;
      execute(stDelete);
      STOP_TIMER;
    }//if
    if (tRunType == RunAll)
    {
      a_d.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
      PRINT_TIMER("delete", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
        int i = retry_opt ;
        int cd = 1 ;
        while (0 < failed && 0 < i)
        {
          ndbout << failed << " of the transactions returned errors!"<< endl ;
          ndbout << endl;
          ndbout <<"Attempting to redo the failed transactions now:" << endl ;
          ndbout << endl <<"Redo attempt " << cd <<" out of ";
          ndbout << retry_opt << endl << endl;
          failed = 0 ;
          START_TIMER;
          execute(stDelete);
          STOP_TIMER;
          PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
          i-- ;
          cd++ ;
        }//while
        if (0 == failed )
        {
          ndbout << endl <<"Redo attempt succeeded" << endl << endl ;
        }
        else
        {
          ndbout << endl;
          ndbout << "Redo attempt failed, moving on now..." << endl << endl;
        }//if
      }//if
    }//if

    tLoops++;
    ndbout << "--------------------------------------------------" << endl;

    if (tNoOfLoops != 0)
    {
      if (tNoOfLoops <= tLoops)
        break ;
    }
  }//for

  execute(stStop);
  void * tmp;
  for (Uint32 i = 0; i < tNoOfThreads; i++)
  {
    NdbThread_WaitFor(threadLife[i], &tmp);
    NdbThread_Destroy(&threadLife[i]);
  }
}
template class Vector<NdbDictionary::RecordSpecification>;
