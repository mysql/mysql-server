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
#define MAXATTR 64
#define MAXTABLES 64
#define NDB_MAXTHREADS 128
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
static int createTables(Ndb*);
static void defineOperation(NdbConnection* aTransObject, StartType aType, 
                            Uint32 base, Uint32 aIndex);
static void defineNdbRecordOperation(ThreadNdb*, NdbConnection* aTransObject, StartType aType, 
                            Uint32 base, Uint32 aIndex);
static void execute(StartType aType);
static bool executeThread(ThreadNdb*, StartType aType, Ndb* aNdbObject, unsigned int);
static void executeCallback(int result, NdbConnection* NdbObject,
                            void* aObject);
static bool error_handler(const NdbError & err);
static Uint32 getKey(Uint32, Uint32) ;
static void input_error();


static int                              retry_opt = 3 ;
static int                              failed = 0 ;
                
ErrorData * flexAsynchErrorData;                        

static NdbThread*               threadLife[NDB_MAXTHREADS];
static int                              tNodeId;
static int                              ThreadReady[NDB_MAXTHREADS];
static StartType                ThreadStart[NDB_MAXTHREADS];
static char                             tableName[MAXTABLES][MAXSTRLEN+1];
static char                             attrName[MAXATTR][MAXSTRLEN+1];

// Program Parameters
static NdbRecord * g_record[MAXTABLES];
static bool tNdbRecord = false;

static bool                             tLocal = false;
static int                              tLocalPart = 0;
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
#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); }; 

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
  ndbout << "  " << tNoOfLoops << " iterations " << endl;
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
    returnValue = NDBT_FAILED;
  }

  if(returnValue == NDBT_OK){
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

  if(returnValue == NDBT_OK){
    /****************************************************************
     *  Create NDB objects.                                   *
     ****************************************************************/
    resetThreads();
    for (Uint32 i = 0; i < tNoOfThreads ; i++) {
      pThreadData[i].ThreadNo = i
;
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

      START_TIMER;
      execute(stInsert);
      STOP_TIMER;
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
          
      /****************************************************************
       * Perform read.                                                *
       ****************************************************************/
      
      failed = 0 ;

      for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
      {
        START_TIMER;
        execute(stRead);
        STOP_TIMER;
        a_r.addObservation((1000 * noOfTransacts * tNoOfOpsPerTrans) / timer.elapsedTime());
        PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
      }

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
          
          
      /****************************************************************
       * Perform update.                                              *
       ****************************************************************/
      
      failed = 0 ;
          
      START_TIMER;
      execute(stUpdate);
      STOP_TIMER;
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
          
      /****************************************************************
       * Perform read.                                             *
       ****************************************************************/
      
      failed = 0 ;
          
      for (int ll = 0; ll < 1 + tExtraReadLoop; ll++)
      {
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
          

      /****************************************************************
       * Perform delete.                                              *
       ****************************************************************/
      
      failed = 0 ;
          
      START_TIMER;
      execute(stDelete);
      STOP_TIMER;
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
  delete [] pThreadData;
  delete pNdb;

  //printing errorCounters
  flexAsynchErrorData->printErrorCounters(ndbout);

  print("insert", a_i);
  print("update", a_u);
  print("delete", a_d);
  print("read  ", a_r);

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
  localNdb->init(1024);
  localNdb->waitUntilReady(10000);
  unsigned int threadBase = (threadNo << 16) + tNodeId ;
  
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
    if(!executeThread(tabThread, tType, localNdb, threadBase)){
      break;
    }
    ThreadReady[threadNo] = 1;
  }//for

  delete localNdb;
  ThreadReady[threadNo] = 1;

  return NULL;
}//threadLoop()

static 
bool
executeThread(ThreadNdb* pThread, 
	      StartType aType, Ndb* aNdbObject, unsigned int threadBase) {

  NdbConnection* tConArray[1024];
  unsigned int tBase;
  unsigned int tBase2;

  unsigned int extraLoops= 0; // (aType == stRead) ? 100000 : 0;

  for (unsigned int ex= 0; ex < (1 + extraLoops); ex++)
  {
    for (unsigned int i = 0; i < tNoOfTransactions; i++) {
      if (tLocal == false) {
        tBase = i * tNoOfParallelTrans * tNoOfOpsPerTrans;
      } else {
        tBase = i * tNoOfParallelTrans * MAX_SEEK;
      }//if
      START_REAL_TIME;
      for (unsigned int j = 0; j < tNoOfParallelTrans; j++) {
        if (tLocal == false) {
          tBase2 = tBase + (j * tNoOfOpsPerTrans);
        } else {
          tBase2 = tBase + (j * MAX_SEEK);
          tBase2 = getKey(threadBase, tBase2);
        }//if
        if (startTransGuess == true) {
	  union {
            Uint64 Tkey64;
            Uint32 Tkey32[2];
	  };
          Tkey32[0] = threadBase;
          Tkey32[1] = tBase2;
          tConArray[j] = aNdbObject->startTransaction((Uint32)0, //Priority
                                                      (const char*)&Tkey64, //Main PKey
                                                      (Uint32)4);           //Key Length
        } else {
          tConArray[j] = aNdbObject->startTransaction();
        }//if
        if (tConArray[j] == NULL && 
            !error_handler(aNdbObject->getNdbError()) ){
          ndbout << endl << "Unable to recover! Quiting now" << endl ;
          return false;
        }//if
        
        for (unsigned int k = 0; k < tNoOfOpsPerTrans; k++) {
          //-------------------------------------------------------
          // Define the operation, but do not execute it yet.
          //-------------------------------------------------------
          if (tNdbRecord)
            defineNdbRecordOperation(pThread, 
                                     tConArray[j], aType, threadBase,(tBase2+k));
          else
            defineOperation(tConArray[j], aType, threadBase, (tBase2 + k));
        }//for
        
        tConArray[j]->executeAsynchPrepare(Commit, &executeCallback, NULL);
      }//for
      STOP_REAL_TIME;
      //-------------------------------------------------------
      // Now we have defined a set of operations, it is now time
      // to execute all of them.
      //-------------------------------------------------------
      int Tcomp = aNdbObject->sendPollNdb(3000, 0, 0);
      while (unsigned(Tcomp) < tNoOfParallelTrans) {
        int TlocalComp = aNdbObject->pollNdb(3000, 0);
        Tcomp += TlocalComp;
      }//while
      for (unsigned int j = 0 ; j < tNoOfParallelTrans ; j++) {
        aNdbObject->closeTransaction(tConArray[j]);
      }//for
    }//for
  } // for
  return true;
}//executeThread()

static 
Uint32
getKey(Uint32 aBase, Uint32 anIndex) {
  Uint32 Tfound = anIndex;
  union {
    Uint64 Tkey64;
    Uint32 Tkey32[2];
  };
  Tkey32[0] = aBase;
  Uint32 hash;
  for (Uint32 i = anIndex; i < (anIndex + MAX_SEEK); i++) {
    Tkey32[1] = (Uint32)i;
    hash = md5_hash((Uint64*)&Tkey64, (Uint32)2);
    hash = (hash >> 6) & (MAX_PARTS - 1);
    if (hash == unsigned(tLocalPart)) {
      Tfound = i;
      break;
    }//if
  }//for
  return Tfound;
}//getKey()

static void
executeCallback(int result, NdbConnection* NdbObject, void* aObject)
{
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
    return;
  }//if
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
  localNdbOperation->equal((Uint32)0,(char*)&attrValue[0]);
  switch (aType) {
  case stInsert:      // Insert case
  case stUpdate:      // Update Case
    {
      for (countAttributes = 1;
           countAttributes < loopCountAttributes; countAttributes++) {
        localNdbOperation->setValue(countAttributes, 
                                    (char*)&attrValue[0]);
      }//for
      break;
    }//case
  case stRead: {      // Read Case
    for (countAttributes = 1;
         countAttributes < loopCountAttributes; countAttributes++) {
      localNdbOperation->getValue(countAttributes, 
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
      BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    }
  }
}

static
int 
createTables(Ndb* pMyNdb){

  NdbSchemaCon          *MySchemaTransaction;
  NdbSchemaOp           *MySchemaOp;
  int                   check;

  if (theTableCreateFlag == 0) {
    for(int i=0; i < 1 ;i++) {
      ndbout << "Creating " << tableName[i] << "..." << endl;
      MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pMyNdb);
      
      if(MySchemaTransaction == NULL && 
         (!error_handler(MySchemaTransaction->getNdbError())))
        return -1;
      
      MySchemaOp = MySchemaTransaction->getNdbSchemaOp();       
      if(MySchemaOp == NULL &&
         (!error_handler(MySchemaTransaction->getNdbError())))
        return -1;


      check = MySchemaOp->createTable( tableName[i]
                                       ,8                       // Table Size
                                       ,TupleKey                // Key Type
                                       ,40                      // Nr of Pages
                                       ,All
                                       ,6
                                       ,(tLoadFactor - 5)
                                       ,(tLoadFactor)
                                       ,1
                                       ,!tempTable
                                       );
      
      if (check == -1 &&
          (!error_handler(MySchemaTransaction->getNdbError())))
        return -1;
      
      check = MySchemaOp->createAttribute( (char*)attrName[0],
                                           TupleKey,
                                           32,
                                           PKSIZE,
                                           UnSigned,
                                           MMBased,
                                           NotNullAttribute );
      
      if (check == -1 &&
          (!error_handler(MySchemaTransaction->getNdbError())))
        return -1;
      for (unsigned j = 1; j < tNoOfAttributes ; j++){
        check = MySchemaOp->createAttribute( (char*)attrName[j],
                                             NoKey,
                                             32,
                                             tAttributeSize,
                                             UnSigned,
                                             MMBased,
                                             NotNullAttribute );
        if (check == -1 &&
            (!error_handler(MySchemaTransaction->getNdbError())))
          return -1;
      }
      
      if (MySchemaTransaction->execute() == -1 &&
          (!error_handler(MySchemaTransaction->getNdbError())))
        return -1;
      
      NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);

      if (tNdbRecord)
      {
	NdbDictionary::Dictionary* pDict = pMyNdb->getDictionary();
	const NdbDictionary::Table * pTab = pDict->getTable(tableName[i]);
	
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
      tLocalPart = atoi(argv[i+1]);
      tLocal = true;
      startTransGuess = true;
      if ((tLocalPart < 0) || (tLocalPart > MAX_PARTS)){
	ndbout_c("Invalid local part");
        return -1;
      }
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
    } else {
      return -1;
    }
    
    argc -= 2;
    i = i + 2;
  }//while
  if (tLocal == true) {
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
  ndbout_c("   -local Number of part, only use keys in one part out of 16");
  ndbout_c("   -ndbrecord");
}
  
template class Vector<NdbDictionary::RecordSpecification>;
