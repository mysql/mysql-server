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

#include <NdbApi.hpp>
#include <NdbSchemaCon.hpp>
#include <NdbMain.h>
#include <md5_hash.hpp>

#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <NdbTimer.hpp>

#include <NdbTest.hpp>
#include <NDBT_Error.hpp>

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
#define PKSIZE 1


#ifdef NDB_WIN32
inline long lrand48(void) { return rand(); };
#endif


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
  int threadNo;
  Ndb* threadNdb;
  Uint32 threadBase;
  Uint32 threadLoopCounter;
  Uint32 threadNextStart;
  Uint32 threadStop;
  Uint32 threadLoopStop;
  Uint32 threadIncrement;
  Uint32 threadNoCompleted;
  bool   threadCompleted;
  StartType threadStartType;
};

struct TransNdb
{
  char transRecord[128];
  Ndb* transNdb;
  StartType  transStartType;
  Uint32     vpn_number;
  Uint32     vpn_identity;
  Uint32     transErrorCount;
  NdbOperation* transOperation;
  ThreadNdb* transThread;
};

extern "C" { static void* threadLoop(void*); }
static void setAttrNames(void);
static void setTableNames(void);
static int readArguments(int argc, const char** argv);
static int createTables(Ndb*);
static bool defineOperation(NdbConnection* aTransObject, TransNdb*,
                            Uint32 vpn_nb, Uint32 vpn_id);
static bool executeTransaction(TransNdb* transNdbRef);
static StartType random_choice();
static void execute(StartType aType);
static bool executeThread(ThreadNdb*, TransNdb*);
static void executeCallback(int result, NdbConnection* NdbObject,
                            void* aObject);
static bool error_handler(const NdbError & err) ;
static Uint32 getKey(Uint32, Uint32) ;
static void input_error();
                                      
ErrorData * flexTTErrorData;

static NdbThread*                       threadLife[NDB_MAXTHREADS];
static int                              tNodeId;
static int                              ThreadReady[NDB_MAXTHREADS];
static StartType                        ThreadStart[NDB_MAXTHREADS];
static char                             tableName[1][MAXSTRLEN+1];
static char                             attrName[5][MAXSTRLEN+1];

// Program Parameters
static bool                             tInsert = false;
static bool                             tDelete = false;
static bool                             tReadUpdate = true;
static int                              tUpdateFreq = 20;
static bool                             tLocal = false;
static int                              tLocalPart = 0;
static int                              tMinEvents = 0;
static int                              tSendForce = 0;
static int                              tNoOfLoops = 1;
static Uint32                           tNoOfThreads = 1;
static Uint32                           tNoOfParallelTrans = 32;
static Uint32                           tNoOfTransactions = 500;
static Uint32                           tLoadFactor = 80;
static bool                             tempTable = false;
static bool                             startTransGuess = true;

//Program Flags
static int                              theSimpleFlag = 0;
static int                              theDirtyFlag = 0;
static int                              theWriteFlag = 0;
static int                              theTableCreateFlag = 1;

#define START_REAL_TIME
#define STOP_REAL_TIME
#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); }; 

static void 
resetThreads(){

  for (int i = 0; i < tNoOfThreads ; i++) {
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
    for (int i = 0; i < tNoOfThreads ; i++) {
      if (ThreadReady[i] == 0) {
        cont = 1;
      }//if
    }//for
  } while (cont == 1);
}

static void 
tellThreads(StartType what)
{
  for (int i = 0; i < tNoOfThreads ; i++) 
    ThreadStart[i] = what;
}

static Ndb_cluster_connection *g_cluster_connection= 0;

NDB_COMMAND(flexTT, "flexTT", "flexTT", "flexTT", 65535)
{
  ndb_init();
  ThreadNdb*            pThreadData;
  int                   returnValue = NDBT_OK;
  int i;
  flexTTErrorData = new ErrorData;
  flexTTErrorData->resetErrorCounters();

  if (readArguments(argc, argv) != 0){
    input_error();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  pThreadData = new ThreadNdb[NDB_MAXTHREADS];

  ndbout << endl << "FLEXTT - Starting normal mode" << endl;
  ndbout << "Perform TimesTen benchmark" << endl;
  ndbout << "  " << tNoOfThreads << " number of concurrent threads " << endl;
  ndbout << "  " << tNoOfParallelTrans;
  ndbout << " number of parallel transaction per thread " << endl;
  ndbout << "  " << tNoOfTransactions << " transaction(s) per round " << endl;
  ndbout << "  " << tNoOfLoops << " iterations " << endl;
  ndbout << "  " << "Update Frequency is " << tUpdateFreq << "%" << endl;
  ndbout << "  " << "Load Factor is " << tLoadFactor << "%" << endl;
  if (tLocal == true) {
    ndbout << "  " << "We only use Local Part = ";
    ndbout << tLocalPart << endl;
  }//if
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

  /* print Setting */
  flexTTErrorData->printSettings(ndbout);

  NdbThread_SetConcurrencyLevel(2 + tNoOfThreads);

  setAttrNames();
  setTableNames();

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  g_cluster_connection= &con;

  Ndb * pNdb = new Ndb(g_cluster_connection, "TEST_DB");      
  pNdb->init();
  tNodeId = pNdb->getNodeId();

  ndbout << "  NdbAPI node with id = " << pNdb->getNodeId() << endl;
  ndbout << endl;
  
  ndbout << "Waiting for ndb to become ready..." <<endl;
  if (pNdb->waitUntilReady(2000) != 0){
    ndbout << "NDB is not ready" << endl;
    ndbout << "Benchmark failed!" << endl;
    returnValue = NDBT_FAILED;
  }

  if(returnValue == NDBT_OK){
    if (createTables(pNdb) != 0){
      returnValue = NDBT_FAILED;
    }
  }

  if(returnValue == NDBT_OK){
    /****************************************************************
     *  Create NDB objects.                                   *
     ****************************************************************/
    resetThreads();
    for (i = 0; i < tNoOfThreads ; i++) {
      pThreadData[i].threadNo = i;
      threadLife[i] = NdbThread_Create(threadLoop,
                                       (void**)&pThreadData[i],
                                       32768,
                                       "flexAsynchThread",
                                       NDB_THREAD_PRIO_LOW);
    }//for
    ndbout << endl <<  "All NDB objects and table created" << endl << endl;
    int noOfTransacts = tNoOfParallelTrans * tNoOfTransactions *
                        tNoOfThreads * tNoOfLoops;
    /****************************************************************
     * Execute program.                                             *
     ****************************************************************/
    /****************************************************************
     * Perform inserts.                                             *
     ****************************************************************/
          
    if (tInsert == true) {
      tInsert = false;
      tReadUpdate = false;
      START_TIMER;
      execute(stInsert);
      STOP_TIMER;
      PRINT_TIMER("insert", noOfTransacts, 1);
    }//if
    /****************************************************************
     * Perform read + updates.                                      *
     ****************************************************************/
      
    if (tReadUpdate == true) {
      START_TIMER;
      execute(stRead);
      STOP_TIMER;
      PRINT_TIMER("update + read", noOfTransacts, 1);
    }//if  
    /****************************************************************
     * Perform delete.                                              *
     ****************************************************************/
                
    if (tDelete == true) {
      tDelete = false;
      START_TIMER;
      execute(stDelete);
      STOP_TIMER;
      PRINT_TIMER("delete", noOfTransacts, 1);
    }//if
    ndbout << "--------------------------------------------------" << endl;
        
    execute(stStop);
    void * tmp;
    for(i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(threadLife[i], &tmp);
      NdbThread_Destroy(&threadLife[i]);
    }
  } 
  delete [] pThreadData;
  delete pNdb;

  //printing errorCounters
  flexTTErrorData->printErrorCounters(ndbout);

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
  ThreadNdb* tabThread = (ThreadNdb*)ThreadData;
  int loc_threadNo = tabThread->threadNo;

  void * mem = malloc(sizeof(TransNdb)*tNoOfParallelTrans);
  TransNdb* pTransData = (TransNdb*)mem;

  localNdb = new Ndb(g_cluster_connection, "TEST_DB");
  localNdb->init(1024);
  localNdb->waitUntilReady();

  if (tLocal == false) {
    tabThread->threadIncrement = 1;
  } else {
    tabThread->threadIncrement = MAX_SEEK;
  }//if
  tabThread->threadBase = (loc_threadNo << 16) + tNodeId;
  tabThread->threadNdb = localNdb;
  tabThread->threadStop = tNoOfParallelTrans * tNoOfTransactions;
  tabThread->threadStop *= tabThread->threadIncrement;
  tabThread->threadLoopStop = tNoOfLoops;
  Uint32 i, j;
  for (i = 0; i < tNoOfParallelTrans; i++) {
    pTransData[i].transNdb = localNdb;    
    pTransData[i].transThread = tabThread;    
    pTransData[i].transOperation = NULL;    
    pTransData[i].transStartType = stIdle;    
    pTransData[i].vpn_number = tabThread->threadBase;    
    pTransData[i].vpn_identity = 0;
    pTransData[i].transErrorCount = 0;
    for (j = 0; j < 128; j++) {
      pTransData[i].transRecord[j] = 0x30;
    }//for
  }//for

  for (;;){
    while (ThreadStart[loc_threadNo] == stIdle) {
      NdbSleep_MilliSleep(10);
    }//while

    // Check if signal to exit is received
    if (ThreadStart[loc_threadNo] == stStop) {
      break;
    }//if

    tabThread->threadStartType = ThreadStart[loc_threadNo];  
    tabThread->threadLoopCounter = 0;
    tabThread->threadCompleted = false;  
    tabThread->threadNoCompleted = 0;
    tabThread->threadNextStart = 0;

    ThreadStart[loc_threadNo] = stIdle;
    if(!executeThread(tabThread, pTransData)){
      break;
    }
    ThreadReady[loc_threadNo] = 1;
  }//for

  free(mem);
  delete localNdb;
  ThreadReady[loc_threadNo] = 1;

  return NULL; // Thread exits
}//threadLoop()

static 
bool
executeThread(ThreadNdb* tabThread, TransNdb* atransDataArrayPtr) {
  Uint32 i;
  for (i = 0; i < tNoOfParallelTrans; i++) {
    TransNdb* transNdbPtr = &atransDataArrayPtr[i];
    transNdbPtr->vpn_identity = i * tabThread->threadIncrement;
    transNdbPtr->transStartType = tabThread->threadStartType;
    if (executeTransaction(transNdbPtr) == false) {
      return false;
    }//if
  }//for
  tabThread->threadNextStart = tNoOfParallelTrans * tabThread->threadIncrement;
  do {
    tabThread->threadNdb->sendPollNdb(3000, tMinEvents, tSendForce);
  } while (tabThread->threadCompleted == false);
  return true;
}//executeThread()

static
bool executeTransaction(TransNdb* transNdbRef)
{
  NdbConnection* MyTrans;
  ThreadNdb* tabThread = transNdbRef->transThread;
  Ndb* aNdbObject = transNdbRef->transNdb;
  Uint32 threadBase = tabThread->threadBase;
  Uint32 startKey = transNdbRef->vpn_identity;
  if (tLocal == true) {
    startKey = getKey(startKey, threadBase);
  }//if
  if (startTransGuess == true) {
    Uint32 tKey[2];
    tKey[0] = startKey;
    tKey[1] = threadBase;
    MyTrans = aNdbObject->startTransaction((Uint32)0, //Priority
                                         (const char*)&tKey[0],   //Main PKey
                                         (Uint32)8);           //Key Length
  } else {
   MyTrans = aNdbObject->startTransaction();
  }//if
  if (MyTrans == NULL) {
    error_handler(aNdbObject->getNdbError());
    ndbout << endl << "Unable to recover! Quiting now" << endl ;
    return false;
  }//if
  //-------------------------------------------------------
  // Define the operation, but do not execute it yet.
  //-------------------------------------------------------
  if (!defineOperation(MyTrans, transNdbRef, startKey, threadBase))
    return false;

  return true;
}//executeTransaction()


static 
Uint32
getKey(Uint32 aBase, Uint32 aThreadBase) {
  Uint32 Tfound = aBase;
  Uint32 hash;
  Uint64 Tkey64;
  Uint32* tKey32 = (Uint32*)&Tkey64;
  tKey32[0] = aThreadBase;
  for (int i = aBase; i < (aBase + MAX_SEEK); i++) {
    tKey32[1] = (Uint32)i;
    hash = md5_hash((Uint64*)&Tkey64, (Uint32)2);
    hash = (hash >> 6) & (MAX_PARTS - 1);
    if (hash == tLocalPart) {
      Tfound = i;
      break;
    }//if
  }//for
  return Tfound;
}//getKey()

static void
executeCallback(int result, NdbConnection* NdbObject, void* aObject)
{
  TransNdb* transNdbRef = (TransNdb*)aObject;
  ThreadNdb* tabThread = transNdbRef->transThread;
  Ndb* tNdb = transNdbRef->transNdb;
  Uint32 vpn_id = transNdbRef->vpn_identity;
  Uint32 vpn_nb = tabThread->threadBase;

  if (result == -1) {
// Add complete error handling here
    int retCode = flexTTErrorData->handleErrorCommon(NdbObject->getNdbError());
    if (retCode == 1) {
      if (NdbObject->getNdbError().code != 626 &&
          NdbObject->getNdbError().code != 630) {
        ndbout_c("execute: %s", NdbObject->getNdbError().message);
        ndbout_c("Error code = %d", NdbObject->getNdbError().code);
      }
    } else if (retCode == 2) {
      ndbout << "4115 should not happen in flexTT" << endl;
    } else if (retCode == 3) {
      /* What can we do here? */
      ndbout_c("execute: %s", NdbObject->getNdbError().message);
    }//if(retCode == 3)
    transNdbRef->transErrorCount++;
    const NdbError & err = NdbObject->getNdbError();
    switch (err.classification) {
    case NdbError::NoDataFound:
    case NdbError::ConstraintViolation:
      ndbout << "Error with vpn_id = " << vpn_id << " and vpn_nb = ";
      ndbout << vpn_nb << endl;
      ndbout << err << endl;
      goto checkCompleted;
    case NdbError::OverloadError:
      NdbSleep_MilliSleep(10);
    case NdbError::NodeRecoveryError:
    case NdbError::UnknownResultError:
    case NdbError::TimeoutExpired:
      break;
    default:
      goto checkCompleted;
    }//if
    if ((transNdbRef->transErrorCount > 10) ||
        (tabThread->threadNoCompleted > 0)) {
      goto checkCompleted;
    }//if
  } else {
    if (tabThread->threadNoCompleted == 0) {
      transNdbRef->transErrorCount = 0;
      transNdbRef->vpn_identity = tabThread->threadNextStart;
      if (tabThread->threadNextStart == tabThread->threadStop) {
        tabThread->threadLoopCounter++;
        transNdbRef->vpn_identity = 0;
        tabThread->threadNextStart = 0;
        if (tabThread->threadLoopCounter == tNoOfLoops) {
          goto checkCompleted;
        }//if
      }//if
      tabThread->threadNextStart += tabThread->threadIncrement;
    } else {
      goto checkCompleted;
    }//if
  }//if
  tNdb->closeTransaction(NdbObject);
  executeTransaction(transNdbRef);
  return;

checkCompleted:
  tNdb->closeTransaction(NdbObject);
  tabThread->threadNoCompleted++;
  if (tabThread->threadNoCompleted == tNoOfParallelTrans) {
    tabThread->threadCompleted = true;
  }//if
  return;      
}//executeCallback()

static
StartType
random_choice()
{
//----------------------------------------------------
// Generate a random key between 0 and tNoOfRecords - 1
//----------------------------------------------------
   UintR random_number = lrand48() % 100;
   if (random_number < tUpdateFreq)
    return stUpdate;
  else
    return stRead;
}//random_choice()

static bool
defineOperation(NdbConnection* localNdbConnection, TransNdb* transNdbRef,
                unsigned int vpn_id, unsigned int vpn_nb)
{
  NdbOperation*  localNdbOperation;
  StartType      TType = transNdbRef->transStartType;

  //-------------------------------------------------------
  // Set-up the attribute values for this operation.
  //-------------------------------------------------------
  localNdbOperation = localNdbConnection->getNdbOperation(tableName[0]);        
  if (localNdbOperation == NULL) {
    error_handler(localNdbConnection->getNdbError());
    return false;
  }//if
  switch (TType) {
  case stInsert:   // Insert case
    if (theWriteFlag == 1 && theDirtyFlag == 1) {
      localNdbOperation->dirtyWrite();
    } else if (theWriteFlag == 1) {
      localNdbOperation->writeTuple();
    } else {
      localNdbOperation->insertTuple();
    }//if
    break;
  case stRead:     // Read Case
    TType = random_choice();
    if (TType == stRead) {
      if (theSimpleFlag == 1) {
        localNdbOperation->simpleRead();
      } else if (theDirtyFlag == 1) {
        localNdbOperation->dirtyRead();
      } else {
        localNdbOperation->readTuple();
      }//if
    } else {
      if (theWriteFlag == 1 && theDirtyFlag == 1) {
        localNdbOperation->dirtyWrite();
      } else if (theWriteFlag == 1) {
        localNdbOperation->writeTuple();
      } else if (theDirtyFlag == 1) {
        localNdbOperation->dirtyUpdate();
      } else {
        localNdbOperation->updateTuple();
      }//if
    }//if
    break;
  case stDelete:  // Delete Case
    localNdbOperation->deleteTuple();
    break;
  default:
    error_handler(localNdbOperation->getNdbError());
  }//switch
  localNdbOperation->equal((Uint32)0,vpn_id);
  localNdbOperation->equal((Uint32)1,vpn_nb);
  char* attrValue = &transNdbRef->transRecord[0];
  switch (TType) {
  case stInsert:      // Insert case
    localNdbOperation->setValue((Uint32)2, attrValue);
    localNdbOperation->setValue((Uint32)3, attrValue);
    localNdbOperation->setValue((Uint32)4, attrValue);
    break;
  case stUpdate:      // Update Case
    localNdbOperation->setValue((Uint32)3, attrValue);
    break;
  case stRead:    // Read Case
    localNdbOperation->getValue((Uint32)2, attrValue);
    localNdbOperation->getValue((Uint32)3, attrValue);
    localNdbOperation->getValue((Uint32)4, attrValue);
    break;
  case stDelete:  // Delete Case
    break;
  default:
    error_handler(localNdbOperation->getNdbError());
  }//switch
  localNdbConnection->executeAsynchPrepare(Commit, &executeCallback, 
                                           (void*)transNdbRef);
  return true;
}//defineOperation()


static void setAttrNames()
{
  BaseString::snprintf(attrName[0], MAXSTRLEN, "VPN_ID");
  BaseString::snprintf(attrName[1], MAXSTRLEN, "VPN_NB");
  BaseString::snprintf(attrName[2], MAXSTRLEN, "DIRECTORY_NB");
  BaseString::snprintf(attrName[3], MAXSTRLEN, "LAST_CALL_PARTY");
  BaseString::snprintf(attrName[4], MAXSTRLEN, "DESCR");
}


static void setTableNames()
{
  BaseString::snprintf(tableName[0], MAXSTRLEN, "VPN_USERS");
}

static
int 
createTables(Ndb* pMyNdb){

  NdbSchemaCon          *MySchemaTransaction;
  NdbSchemaOp           *MySchemaOp;
  int                   check;

  if (theTableCreateFlag == 0) {
    ndbout << "Creating Table: vpn_users " << "..." << endl;
    MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pMyNdb);
      
    if(MySchemaTransaction == NULL && 
       (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    MySchemaOp = MySchemaTransaction->getNdbSchemaOp();       
    if(MySchemaOp == NULL &&
       (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    check = MySchemaOp->createTable( tableName[0]
                                       ,8                       // Table Size
                                       ,TupleKey                // Key Type
                                       ,40                      // Nr of Pages
                                       ,All
                                       ,6
                                       ,(tLoadFactor - 5)
                                       ,tLoadFactor
                                       ,1
                                       ,!tempTable
                                       );
      
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    check = MySchemaOp->createAttribute( (char*)attrName[0],
                                         TupleKey,
                                         32,
                                         1,
                                         UnSigned,
                                         MMBased,
                                         NotNullAttribute );
      
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
    check = MySchemaOp->createAttribute( (char*)attrName[1],
                                         TupleKey,
                                         32,
                                         1,
                                         UnSigned,
                                         MMBased,
                                         NotNullAttribute );
      
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
    check = MySchemaOp->createAttribute( (char*)attrName[2],
                                             NoKey,
                                             8,
                                             10,
                                             UnSigned,
                                             MMBased,
                                             NotNullAttribute );
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    check = MySchemaOp->createAttribute( (char*)attrName[3],
                                             NoKey,
                                             8,
                                             10,
                                             UnSigned,
                                             MMBased,
                                             NotNullAttribute );
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    check = MySchemaOp->createAttribute( (char*)attrName[4],
                                             NoKey,
                                             8,
                                             100,
                                             UnSigned,
                                             MMBased,
                                             NotNullAttribute );
    if (check == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    if (MySchemaTransaction->execute() == -1 &&
        (!error_handler(MySchemaTransaction->getNdbError())))
      return -1;
      
    NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  }//if
  
  return 0;
}

bool error_handler(const NdbError& err){
  ndbout << err << endl ;
  switch(err.classification){
  case NdbError::NodeRecoveryError:
  case NdbError::SchemaError:
  case NdbError::TimeoutExpired:
    ndbout << endl << "Attempting to recover and continue now..." << endl ;
    return true ; // return true to retry
  }
  return false;
}
#if 0
bool error_handler(const char* error_string, int error_int) {
  ndbout << error_string << endl ;
  if ((4008 == error_int) ||
      (677 == error_int) ||
      (891 == error_int) ||
      (1221 == error_int) ||
      (721 == error_int) ||
      (266 == error_int)) {
    ndbout << endl << "Attempting to recover and continue now..." << endl ;
    return true ; // return true to retry
  }
  return false ; // return false to abort
}
#endif

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
    } else if (strcmp(argv[i], "-o") == 0) {
      tNoOfTransactions = atoi(argv[i+1]);
      if (tNoOfTransactions < 1){
	ndbout_c("Invalid no of transactions");
        return -1;
      }
    } else if (strcmp(argv[i], "-l") == 0){
      tNoOfLoops = atoi(argv[i+1]);
      if (tNoOfLoops < 1) {
	ndbout_c("Invalid no of loops");
        return -1;
      }
    } else if (strcmp(argv[i], "-e") == 0){
      tMinEvents = atoi(argv[i+1]);
      if ((tMinEvents < 1) || (tMinEvents > tNoOfParallelTrans)) {
	ndbout_c("Invalid no of loops");
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
    } else if (strcmp(argv[i], "-ufreq") == 0){
      tUpdateFreq = atoi(argv[i+1]);
      if ((tUpdateFreq < 0) || (tUpdateFreq > 100)){
	ndbout_c("Invalid Update Frequency");
        return -1;
      }
    } else if (strcmp(argv[i], "-load_factor") == 0){
      tLoadFactor = atoi(argv[i+1]);
      if ((tLoadFactor < 40) || (tLoadFactor >= 100)){
	ndbout_c("Invalid LoadFactor");
        return -1;
      }
    } else if (strcmp(argv[i], "-d") == 0){
      tDelete = true;
      argc++;
      i--;
    } else if (strcmp(argv[i], "-i") == 0){
      tInsert = true;
      argc++;
      i--;
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
    } else if (strcmp(argv[i], "-table_create") == 0){
      theTableCreateFlag = 0;
      tInsert = true;
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
    } else {
      return -1;
    }
    
    argc -= 2;
    i = i + 2;
  }//while
  if (tLocal == true) {
    if (startTransGuess == false) {
      ndbout_c("Not valid to use no_hint with local");
    }//if
  }//if
  return 0;
}

static
void
input_error(){
  
  ndbout_c("FLEXTT");
  ndbout_c("   Perform benchmark of insert, update and delete transactions");
  ndbout_c("");
  ndbout_c("Arguments:");
  ndbout_c("   -t Number of threads to start, default 1");
  ndbout_c("   -p Number of parallel transactions per thread, default 32");
  ndbout_c("   -o Number of transactions per loop, default 500");
  ndbout_c("   -ufreq Number Update Frequency in percent (0 -> 100), rest is read");
  ndbout_c("   -load_factor Number Fill level in index in percent (40 -> 99)");
  ndbout_c("   -l Number of loops to run, default 1, 0=infinite");
  ndbout_c("   -i Start by inserting all records");
  ndbout_c("   -d End by deleting all records (only one loop)");
  ndbout_c("   -simple Use simple read to read from database");
  ndbout_c("   -dirty Use dirty read to read from database");
  ndbout_c("   -write Use writeTuple in insert and update");
  ndbout_c("   -n Use standard table names");
  ndbout_c("   -table_create Create tables in db");
  ndbout_c("   -temp Create table(s) without logging");
  ndbout_c("   -no_hint Don't give hint on where to execute transaction coordinator");
  ndbout_c("   -adaptive Use adaptive send algorithm (default)");
  ndbout_c("   -force Force send when communicating");
  ndbout_c("   -non_adaptive Send at a 10 millisecond interval");
  ndbout_c("   -local Number of part, only use keys in one part out of 16");
}
