/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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
  stIdle, 
  stInsert,
  stRead,
  stUpdate,
  stDelete, 
  stStop 
} ;

enum RunType {
  RunInsert,
  RunRead,
  RunUpdate,
  RunDelete,
  RunCreateTable,
  RunDropTable,
  RunAll
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
static void defineNdbRecordOperation(ThreadNdb*, NdbConnection* aTransObject, StartType aType, 
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


static int                              retry_opt = 3 ;
static int                              failed = 0 ;
                
ErrorData * flexAsynchErrorData;                        

static NdbThread*               threadLife[NDB_MAXTHREADS];
static int                              tNodeId;
static int                              ThreadReady[NDB_MAXTHREADS];
static longlong                 ThreadExecutions[NDB_MAXTHREADS];
static StartType                ThreadStart[NDB_MAXTHREADS];
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
waitForThreads(void)
{
  int cont = 0;
  do {
    cont = 0;
    NdbSleep_MilliSleep(20);
    for (unsigned i = 0; i < tNoOfThreads ; i++) {
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
  int                   tLoops=0;
  int                   returnValue = NDBT_OK;
  DEFINE_TIMER;

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

  NdbThread_SetConcurrencyLevel(2 + tNoOfThreads);

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

  if (tNdbRecord)
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
    /****************************************************************
     *  Create NDB objects.                                   *
     ****************************************************************/
    resetThreads();
    for (Uint32 i = 0; i < tNoOfThreads ; i++) {
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

    for(;;) {

      int loopCount = tLoops + 1 ;
      ndbout << endl << "Loop # " << loopCount  << endl << endl ;

      /****************************************************************
       * Perform inserts.                                             *
       ****************************************************************/
          
      failed = 0 ;
      if (tRunType == RunAll || tRunType == RunInsert){
        ndbout << "Executing inserts" << endl;
        START_TIMER;
        execute(stInsert);
        STOP_TIMER;
      }
      if (tRunType == RunAll){
        a_i.addObservation((1000*noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
        PRINT_TIMER("insert", noOfTransacts, tNoOfOpsPerTrans);

        if (0 < failed) {
          int i = retry_opt ;
          int ci = 1 ;
          while (0 < failed && 0 < i){
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
          if(0 == failed ){
            ndbout << endl <<"Redo attempt succeeded" << endl << endl;
          }else{
            ndbout << endl <<"Redo attempt failed, moving on now..." << endl 
                   << endl;
          }//if
        }//if
      }//if  
      /****************************************************************
       * Perform read.                                                *
       ****************************************************************/
      
      failed = 0 ;

      if (tRunType == RunAll || tRunType == RunRead){
        for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
        {
          ndbout << "Executing reads" << endl;
          START_TIMER;
          execute(stRead);
          STOP_TIMER;
          if (tRunType == RunAll){
            a_r.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
            PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
          }//if
        }//for
      }//if

      if (tRunType == RunAll){
        if (0 < failed) {
          int i = retry_opt ;
          int cr = 1;
          while (0 < failed && 0 < i){
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
          if(0 == failed ) {
            ndbout << endl <<"Redo attempt succeeded" << endl << endl ;
          }else{
            ndbout << endl <<"Redo attempt failed, moving on now..." << endl << endl ;
          }//if
        }//if
      }//if
          
          
      /****************************************************************
       * Perform update.                                              *
       ****************************************************************/
      
      failed = 0 ;

      if (tRunType == RunAll || tRunType == RunUpdate){
        ndbout << "Executing updates" << endl;
        START_TIMER;
        execute(stUpdate);
        STOP_TIMER;
      }//if
      if (tRunType == RunAll){
        a_u.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
        PRINT_TIMER("update", noOfTransacts, tNoOfOpsPerTrans) ;

        if (0 < failed) {
          int i = retry_opt ;
          int cu = 1 ;
          while (0 < failed && 0 < i){
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
          if(0 == failed ){
            ndbout << endl <<"Redo attempt succeeded" << endl << endl;
          } else {
            ndbout << endl;
            ndbout <<"Redo attempt failed, moving on now..." << endl << endl;
          }//if
        }//if
      }//if
          
      /****************************************************************
       * Perform read.                                             *
       ****************************************************************/
      
      failed = 0 ;

      if (tRunType == RunAll){
        for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
        {
          ndbout << "Executing reads" << endl;
          START_TIMER;
          execute(stRead);
          STOP_TIMER;
          a_r.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
          PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
        }        

        if (0 < failed) {
          int i = retry_opt ;
          int cr2 = 1 ;
          while (0 < failed && 0 < i){
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
          if(0 == failed ){
            ndbout << endl <<"Redo attempt succeeded" << endl << endl;
          }else{
            ndbout << endl;
            ndbout << "Redo attempt failed, moving on now..." << endl << endl;
          }//if
        }//if
      }//if
          

      /****************************************************************
       * Perform delete.                                              *
       ****************************************************************/
      
      failed = 0 ;
          
      if (tRunType == RunAll || tRunType == RunDelete){
        ndbout << "Executing deletes" << endl;
        START_TIMER;
        execute(stDelete);
        STOP_TIMER;
      }//if
      if (tRunType == RunAll){
        a_d.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
        PRINT_TIMER("delete", noOfTransacts, tNoOfOpsPerTrans);

        if (0 < failed) {
          int i = retry_opt ;
          int cd = 1 ;
          while (0 < failed && 0 < i){
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
          if(0 == failed ){
            ndbout << endl <<"Redo attempt succeeded" << endl << endl ;
          }else{
            ndbout << endl;
            ndbout << "Redo attempt failed, moving on now..." << endl << endl;
          }//if
        }//if
      }//if
          
      tLoops++;
      ndbout << "--------------------------------------------------" << endl;
    
      if(tNoOfLoops != 0){
        if(tNoOfLoops <= tLoops)
          break ;
      }
    }//for
    
    execute(stStop);
    void * tmp;
    for(Uint32 i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(threadLife[i], &tmp);
      NdbThread_Destroy(&threadLife[i]);
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
    longlong total_transactions = 0;
    longlong exec_time;

    if (tRunType == RunInsert || tRunType == RunDelete) {
      total_transactions = (longlong)tNoOfTransactions;
      total_transactions *= (longlong)tNoOfThreads;
      total_transactions *= (longlong)tNoOfParallelTrans;
    } else {
      for (Uint32 i = 0; i < tNoOfThreads; i++){
        total_transactions += ThreadExecutions[i];
      }
    }
    if (tRunType == RunInsert || tRunType == RunDelete) {
      exec_time = (longlong)timer.elapsedTime();
    } else {
      exec_time = (longlong)tExecutionTime * 1000;
    }
    ndbout << "Total number of transactions is " << total_transactions;
    ndbout << endl;
    ndbout << "Execution time is " << exec_time << " milliseconds" << endl;

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
  waitForThreads();
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
          defineNdbRecordOperation(pThread, 
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
  if (result == -1) {

              // Add complete error handling here

              int retCode = flexAsynchErrorData->handleErrorCommon(NdbObject->getNdbError());
              if (retCode == 1) {
		if (NdbObject->getNdbError().code != 626 && NdbObject->getNdbError().code != 630){
                ndbout_c("execute: %s", NdbObject->getNdbError().message);
                ndbout_c("Error code = %d", NdbObject->getNdbError().code);}
              } else if (retCode == 2) {
                ndbout << "4115 should not happen in flexAsynch" << endl;
              } else if (retCode == 3) {
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
defineNdbRecordOperation(ThreadNdb* pThread, 
			 NdbConnection* pTrans, StartType aType,
			 Uint32 threadBase, Uint32 aIndex)
{
  char * record = pThread->record;
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
    op = pTrans->readTuple(g_record[0],record,g_record[0],record, NdbOperation::LM_CommittedRead);
    break;
  }//case
  case stUpdate:{    // Update Case
    op = pTrans->updateTuple(g_record[0],record,g_record[0],record);    
    break;
  }//case
  case stDelete: {   // Delete Case
    op = pTrans->deleteTuple(g_record[0],record, g_record[0]);
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
  
template class Vector<NdbDictionary::RecordSpecification>;
