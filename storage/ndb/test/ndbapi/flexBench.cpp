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


/* ***************************************************
FLEXBENCH
Perform benchmark of insert, update and delete transactions

Arguments:
    -t Number of threads to start, default 1
    -o Number of operations per loop, default 500
    -l Number of loops to run, default 1, 0=infinite
    -a Number of attributes, default 25
    -c Number of tables, default 1
    -s Size of each attribute, default 1 (Primary Key is always of size 1,
    independent of this value)
    -lkn Number of long primary keys, default 1
    -lks Size of each long primary key, default 1
    -simple Use simple read to read from database
    -dirty Use dirty read to read from database
    -write Use writeTuple in insert and update
    -stdtables Use standard table names
    -no_table_create Don't create tables in db
    -sleep Sleep a number of seconds before running the test, this 
    can be used so that another flexBench have time to create tables
    -temp Use tables without logging
    -verify Verify inserts, updates and deletes
#ifdef CEBIT_STAT
    -statserv host:port  statistics server to report to
    -statfreq ops        report every ops operations (default 100)
#endif
    Returns:
    0 - Test passed
    1 - Test failed
    2 - Invalid arguments

* *************************************************** */

#include <ndb_global.h>
#include "NdbApi.hpp"

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbTimer.hpp>
#include <NdbThread.h>

#include <NdbTest.hpp>

#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 128
#define MAXATTRSIZE 1000
#define MAXNOLONGKEY 16 // Max number of long keys.
#define MAXLONGKEYTOTALSIZE 1023 // words = 4092 bytes

extern "C" { static void* flexBenchThread(void*); }
static int readArguments(int argc, const char** argv);
static int createTables(Ndb*);
static void sleepBeforeStartingTest(int seconds);
static void input_error();

enum StartType { 
  stIdle,
  stInsert,
  stVerify,
  stRead,
  stUpdate,
  stDelete,
  stTryDelete,
  stVerifyDelete,
  stStop 
};

struct ThreadData
{
  int threadNo;
  NdbThread* threadLife;
  int threadReady;  
  StartType threadStart;
  int threadResult;
};

static int                  tNodeId = 0 ;
static char                 tableName[MAXTABLES][MAXSTRLEN+1];
static char                 attrName[MAXATTR][MAXSTRLEN+1];
static char**               longKeyAttrName;

// Program Parameters
static int                  tNoOfLoops = 1;
static int                  tAttributeSize = 1;
static unsigned int         tNoOfThreads = 1;
static unsigned int         tNoOfTables = 1;
static unsigned int         tNoOfAttributes = 25;
static unsigned int         tNoOfOperations = 500;
static unsigned int         tSleepTime = 0;
static unsigned int         tNoOfLongPK = 1;
static unsigned int         tSizeOfLongPK = 1;

//Program Flags
static int                  theSimpleFlag = 0;
static int                  theDirtyFlag = 0;
static int                  theWriteFlag = 0;
static int                  theStdTableNameFlag = 0;
static int                  theTableCreateFlag = 0;
static bool                 theTempTable = false;
static bool                 VerifyFlag = true;
static bool                 useLongKeys = false;


static ErrorData theErrorData; // Part of flexBench-program

#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); };

#include <NdbTCP.h>

#ifdef CEBIT_STAT
#include <NdbMutex.h>
static bool statEnable = false;
static char statHost[100];
static int statFreq = 100;
static int statPort = 0;
static int statSock = -1;
static enum { statError = -1, statClosed, statOpen } statState;
static NdbMutex statMutex = NDB_MUTEX_INITIALIZER;
#endif

//-------------------------------------------------------------------
// Statistical Reporting routines
//-------------------------------------------------------------------
#ifdef CEBIT_STAT
// Experimental client-side statistic for CeBIT

static void
statReport(enum StartType st, int ops)
{
  if (!statEnable)
    return;
  if (NdbMutex_Lock(&statMutex) < 0) {
    if (statState != statError) {
      ndbout_c("stat: lock mutex failed: %s", strerror(errno));
      statState = statError;
    }
    return;
  }
  static int nodeid;
  // open connection
  if (statState != statOpen) {
    char *p = getenv("NDB_NODEID");		// ndbnet sets NDB_NODEID
    nodeid = p == 0 ? 0 : atoi(p);
    if ((statSock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
      if (statState != statError) {
	ndbout_c("stat: create socket failed: %s", strerror(errno));
	statState = statError;
      }
      (void)NdbMutex_Unlock(&statMutex);
      return;
    }
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(statPort);
    if (Ndb_getInAddr(&saddr.sin_addr, statHost) < 0) {
      if (statState != statError) {
	ndbout_c("stat: host %s not found", statHost);
	statState = statError;
      }
      (void)close(statSock);
      (void)NdbMutex_Unlock(&statMutex);
      return;
    }
    if (connect(statSock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
      if (statState != statError) {
	ndbout_c("stat: connect failed: %s", strerror(errno));
	statState = statError;
      }
      (void)close(statSock);
      (void)NdbMutex_Unlock(&statMutex);
      return;
    }
    statState = statOpen;
    ndbout_c("stat: connection to %s:%d opened", statHost, (int)statPort);
  }
  const char *text;
  switch (st) {
  case stInsert:
    text = "insert";
    break;
  case stVerify:
    text = "verify";
    break;
  case stRead:
    text = "read";
    break;
  case stUpdate:
    text = "update";
    break;
  case stDelete:
    text = "delete";
    break;
  case stVerifyDelete:
    text = "verifydelete";
    break;
  default:
    text = "unknown";
    break;
  }
  char buf[100];
  sprintf(buf, "%d %s %d\n", nodeid, text, ops);
  int len = strlen(buf);
  // assume SIGPIPE already ignored
  if (write(statSock, buf, len) != len) {
    if (statState != statError) {
      ndbout_c("stat: write failed: %s", strerror(errno));
      statState = statError;
    }
    (void)close(statSock);
    (void)NdbMutex_Unlock(&statMutex);
    return;
  }
  (void)NdbMutex_Unlock(&statMutex);
}
#endif	// CEBIT_STAT

static void 
resetThreads(ThreadData* pt){
  for (unsigned int i = 0; i < tNoOfThreads; i++){
    pt[i].threadReady = 0;
    pt[i].threadResult = 0;
    pt[i].threadStart = stIdle;
  }
}

static int 
checkThreadResults(ThreadData* pt){
  for (unsigned int i = 0; i < tNoOfThreads; i++){
    if(pt[i].threadResult != 0){
      ndbout_c("Thread%d reported fatal error %d", i, pt[i].threadResult);
      return -1;
    }
  }
  return 0;
}

static
void 
waitForThreads(ThreadData* pt)
{
  int cont = 1;
  while (cont){
    NdbSleep_MilliSleep(100);
    cont = 0;
    for (unsigned int i = 0; i < tNoOfThreads; i++){
      if (pt[i].threadReady == 0) 
    cont = 1;
    }
  }
}

static void 
tellThreads(ThreadData* pt, StartType what)
{
  for (unsigned int i = 0; i < tNoOfThreads; i++) 
    pt[i].threadStart = what;
}

static Ndb_cluster_connection *g_cluster_connection= 0;

NDB_COMMAND(flexBench, "flexBench", "flexBench", "flexbench", 65535)
{
  ndb_init();
  ThreadData*           pThreadsData;
  int                   tLoops = 0, i;
  int                   returnValue = NDBT_OK;
    
  if (readArguments(argc, argv) != 0){
    input_error();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  if(useLongKeys){
    longKeyAttrName = (char **) malloc(sizeof(char*) * tNoOfLongPK);
    for (Uint32 i = 0; i < tNoOfLongPK; i++) {
      longKeyAttrName[i] = (char *) malloc(strlen("KEYATTR  ") + 1);
      memset(longKeyAttrName[i], 0, strlen("KEYATTR  ") + 1);
      sprintf(longKeyAttrName[i], "KEYATTR%i", i);
    }
  }
  
  pThreadsData = new ThreadData[tNoOfThreads];
 
  ndbout << endl << "FLEXBENCH - Starting normal mode" << endl;
  ndbout << "Perform benchmark of insert, update and delete transactions"<< endl;
  ndbout << "  " << tNoOfThreads << " thread(s) " << endl;
  ndbout << "  " << tNoOfLoops << " iterations " << endl;
  ndbout << "  " << tNoOfTables << " table(s) and " << 1 << " operation(s) per transaction " <<endl;
  ndbout << "  " << tNoOfAttributes << " attributes per table " << endl;
  ndbout << "  " << tNoOfOperations << " transaction(s) per thread and round " << endl;
  ndbout << "  " << tAttributeSize << " is the number of 32 bit words per attribute "<< endl;
  ndbout << "  " << "Table(s) without logging: " << (Uint32)theTempTable << endl;
  
  if(useLongKeys)
    ndbout << "  " << "Using long keys with " << tNoOfLongPK << " keys a' " << 
      tSizeOfLongPK * 4 << " bytes each." << endl;
  
  ndbout << "  " << "Verification is " ; 
  if(VerifyFlag) {
      ndbout << "enabled" << endl ;
  }else{
      ndbout << "disabled" << endl ;
  }
  theErrorData.printSettings(ndbout);
  
  NdbThread_SetConcurrencyLevel(tNoOfThreads + 2);

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  g_cluster_connection= &con;

  Ndb* pNdb;
  pNdb = new Ndb(&con, "TEST_DB" );  
  pNdb->init();

  tNodeId = pNdb->getNodeId();
  ndbout << "  NdbAPI node with id = " << tNodeId << endl;
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

    sleepBeforeStartingTest(tSleepTime);
    
    /****************************************************************
     *  Create threads.                                           *
     ****************************************************************/
    resetThreads(pThreadsData);
    
    for (i = 0; i < tNoOfThreads; i++){  
      pThreadsData[i].threadNo = i;
      pThreadsData[i].threadLife = NdbThread_Create(flexBenchThread,
                                                    (void**)&pThreadsData[i],
                                                    32768,
                                                    "flexBenchThread",
                                                    NDB_THREAD_PRIO_LOW);
    }
    
    waitForThreads(pThreadsData);
    
    ndbout << endl <<  "All threads started" << endl << endl;
    
    /****************************************************************
     * Execute program.                                             *
     ****************************************************************/
  
    for(;;){

      int loopCount = tLoops + 1;
      ndbout << endl << "Loop # " << loopCount  << endl << endl;
      
      /****************************************************************
       * Perform inserts.                                             *
       ****************************************************************/
      // Reset and start timer
      START_TIMER;
      // Give insert-command to all threads
      resetThreads(pThreadsData);
      tellThreads(pThreadsData, stInsert);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed in performing insert" << endl;
        returnValue = NDBT_FAILED;
        break;
      }        
      // stop timer and print results.
      STOP_TIMER;
      PRINT_TIMER("insert", tNoOfOperations*tNoOfThreads, tNoOfTables);
      /****************************************************************
      * Verify inserts.                                             *
      ****************************************************************/
      if (VerifyFlag) {
      resetThreads(pThreadsData);
      ndbout << "Verifying inserts...\t" ;
      tellThreads(pThreadsData, stVerify);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed while verifying inserts" << endl;
        returnValue = NDBT_FAILED;
        break;
      }else{
          ndbout << "\t\tOK" << endl << endl ;
      }
      }
      
      /****************************************************************
       * Perform read.                                                *
       ****************************************************************/
      // Reset and start timer 
      START_TIMER;
      // Give read-command to all threads
      resetThreads(pThreadsData);
      tellThreads(pThreadsData, stRead);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed in performing read" << endl;
        returnValue = NDBT_FAILED;
        break;
      }
      // stop timer and print results.
      STOP_TIMER;
      PRINT_TIMER("read", tNoOfOperations*tNoOfThreads, tNoOfTables);
      
      /****************************************************************
       * Perform update.                                              *
       ****************************************************************/
      // Reset and start timer
      START_TIMER;
      // Give insert-command to all threads
      resetThreads(pThreadsData);
      tellThreads(pThreadsData, stUpdate);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed in performing update" << endl;
        returnValue = NDBT_FAILED;
        break;
      }
      // stop timer and print results.
      STOP_TIMER;
      PRINT_TIMER("update", tNoOfOperations*tNoOfThreads, tNoOfTables);
      
      /****************************************************************
      * Verify updates.                                             *
      ****************************************************************/
      if (VerifyFlag) {
      resetThreads(pThreadsData);
      ndbout << "Verifying updates...\t" ;
      tellThreads(pThreadsData, stVerify);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed while verifying updates" << endl;
        returnValue = NDBT_FAILED;
        break;
      }else{
          ndbout << "\t\tOK" << endl << endl ;
      }
      }
      
      /****************************************************************
       * Perform read.                                             *
       ****************************************************************/
      // Reset and start timer
      START_TIMER;
      // Give insert-command to all threads
      resetThreads(pThreadsData);
      tellThreads(pThreadsData, stRead);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed in performing read" << endl;
        returnValue = NDBT_FAILED;
        break;
      }
      // stop timer and print results.
      STOP_TIMER;
      PRINT_TIMER("read", tNoOfOperations*tNoOfThreads, tNoOfTables);

      /****************************************************************
       * Perform delete.                                              *
       ****************************************************************/
      // Reset and start timer
      START_TIMER;
      // Give insert-command to all threads
      resetThreads(pThreadsData);
      tellThreads(pThreadsData, stDelete);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
        ndbout << "Error: Threads failed in performing delete" << endl;
        returnValue = NDBT_FAILED;
        break;
      }
      // stop timer and print results.
      STOP_TIMER;
      PRINT_TIMER("delete", tNoOfOperations*tNoOfThreads, tNoOfTables);

      /****************************************************************
      * Verify deletes.                                              *
      ****************************************************************/
      if (VerifyFlag) {
      resetThreads(pThreadsData);
      ndbout << "Verifying tuple deletion..." ;
      tellThreads(pThreadsData, stVerifyDelete);
      waitForThreads(pThreadsData);
      if (checkThreadResults(pThreadsData) != 0){
          ndbout << "Error: Threads failed in verifying deletes" << endl;
          returnValue = NDBT_FAILED;
          break;
      }else{ 
          ndbout << "\t\tOK" << endl << endl ;
      }
      }

      ndbout << "--------------------------------------------------" << endl;

      tLoops++;

      if ( 0 != tNoOfLoops && tNoOfLoops <= tLoops ) 
        break;
      theErrorData.printErrorCounters();
    }
    
    resetThreads(pThreadsData);
    tellThreads(pThreadsData, stStop);
    waitForThreads(pThreadsData);

    void * tmp;
    for(i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(pThreadsData[i].threadLife, &tmp);
      NdbThread_Destroy(&pThreadsData[i].threadLife);
    }
  }

  if (useLongKeys == true) {
    // Only free these areas if they have been allocated
    // Otherwise cores will happen
    for (i = 0; i < tNoOfLongPK; i++)
      free(longKeyAttrName[i]);
    free(longKeyAttrName);
  } // if

  delete [] pThreadsData;
  delete pNdb;
  theErrorData.printErrorCounters();
  return NDBT_ProgramExit(returnValue);
}
////////////////////////////////////////


unsigned long get_hash(unsigned long * hash_key, int len)
{
  unsigned long hash_value = 147;
  unsigned h_key;
  int i;
  for (i = 0; i < len; i++)
    {
      h_key = hash_key[i];
      hash_value = (hash_value << 5) + hash_value + (h_key & 255);
      hash_value = (hash_value << 5) + hash_value + ((h_key >> 8) & 255);
      hash_value = (hash_value << 5) + hash_value + ((h_key >> 16) & 255);
      hash_value = (hash_value << 5) + hash_value + ((h_key >> 24) & 255);
    }
  return hash_value;
}

// End of warming up phase



static void* flexBenchThread(void* pArg)
{
  ThreadData*       pThreadData = (ThreadData*)pArg;
  unsigned int      threadNo, threadBase;
  Ndb*              pNdb = NULL ;
  NdbConnection     *pTrans = NULL ;
  NdbOperation**    pOps = NULL ;
  StartType         tType ;
  StartType         tSaveType ;
  NdbRecAttr*       tTmp = NULL ;
  int*              attrValue = NULL ;
  int*              attrRefValue = NULL ;
  int               check = 0 ;
  int               loopCountOps, loopCountTables, loopCountAttributes;
  int               tAttemptNo = 0;
  int               tRetryAttempts = 20;
  int               tResult = 0;
  int               tSpecialTrans = 0;
  int               nRefLocalOpOffset = 0 ;
  int               nReadBuffSize = 
    tNoOfTables * tNoOfAttributes * sizeof(int) * tAttributeSize ;
  int               nRefBuffSize = 
    tNoOfOperations * tNoOfAttributes * sizeof(int) * tAttributeSize ;
  unsigned***           longKeyAttrValue;


  threadNo = pThreadData->threadNo ;

  attrValue = (int*)malloc(nReadBuffSize) ;
  attrRefValue = (int*)malloc(nRefBuffSize) ;
  pOps = (NdbOperation**)malloc(tNoOfTables*sizeof(NdbOperation*)) ;
  pNdb = new Ndb(g_cluster_connection, "TEST_DB" );
  
  if(!attrValue || !attrRefValue || !pOps || !pNdb){
    // Check allocations to make sure we got all the memory we asked for
    ndbout << "One or more memory allocations failed when starting thread #";
    ndbout << threadNo << endl ;
    ndbout << "Thread #" << threadNo << " will now exit" << endl ;
    tResult = 13 ;
    free(attrValue) ;
    free(attrRefValue) ;
    free(pOps) ;
    delete pNdb ;
    return 0; // thread exits
  }
  
  pNdb->init();
  pNdb->waitUntilReady();

  // To make sure that two different threads doesn't operate on the same record
  // Calculate an "unique" number to use as primary key
  threadBase = (threadNo * 2000000) + (tNodeId * 260000000);
  
  if(useLongKeys){
    // Allocate and populate the longkey array.
    longKeyAttrValue = (unsigned ***) malloc(sizeof(unsigned**) * tNoOfOperations );
    Uint32 n;
    for (n = 0; n < tNoOfOperations; n++)
      longKeyAttrValue[n] = (unsigned **) malloc(sizeof(unsigned*) * tNoOfLongPK );
    for (n = 0; n < tNoOfOperations; n++){
      for (Uint32 i = 0; i < tNoOfLongPK ; i++) {
	longKeyAttrValue[n][i] = (unsigned *) malloc(sizeof(unsigned) * tSizeOfLongPK);
	memset(longKeyAttrValue[n][i], 0, sizeof(unsigned) * tSizeOfLongPK);
	for(Uint32 j = 0; j < tSizeOfLongPK; j++) {
	  // Repeat the unique value to fill up the long key.
	  longKeyAttrValue[n][i][j] = threadBase + n; 
	}
      }
    }
  }

  int nRefOpOffset = 0 ;
  //Assign reference attribute values to memory
  for(Uint32 ops = 1 ; ops < tNoOfOperations ; ops++){
    // Calculate offset value before going into the next loop
    nRefOpOffset = tAttributeSize*tNoOfAttributes*(ops-1) ; 
    for(Uint32 a = 0 ; a < tNoOfAttributes ; a++){
      *(int*)&attrRefValue[nRefOpOffset + tAttributeSize*a] = 
	(int)(threadBase + ops + a) ;
    }
  }

#ifdef CEBIT_STAT
  // ops not yet reported
  int statOps = 0;
#endif
  for (;;) {
    pThreadData->threadResult = tResult; // Report error to main thread, 
    // normally tResult is set to 0
    pThreadData->threadReady = 1;

    while (pThreadData->threadStart == stIdle){
      NdbSleep_MilliSleep(100);
    }//while

    // Check if signal to exit is received
    if (pThreadData->threadStart == stStop){
      pThreadData->threadReady = 1;
      // ndbout_c("Thread%d is stopping", threadNo);
      // In order to stop this thread, the main thread has signaled
      // stStop, break out of the for loop so that destructors
      // and the proper exit functions are called
      break;
    }//if

    tType = pThreadData->threadStart;
    tSaveType = tType;
    pThreadData->threadStart = stIdle;

    // Start transaction, type of transaction
    // is received in the array ThreadStart
    loopCountOps = tNoOfOperations;
    loopCountTables = tNoOfTables;
    loopCountAttributes = tNoOfAttributes;

    for (int count = 1; count < loopCountOps && tResult == 0;){

      pTrans = pNdb->startTransaction();
      if (pTrans == NULL) {
	// This is a fatal error, abort program
	ndbout << "Could not start transaction in thread" << threadNo;
	ndbout << endl;
	ndbout << pNdb->getNdbError() << endl;
	tResult = 1; // Indicate fatal error
	break; // Break out of for loop
      }

      // Calculate the current operation offset in the reference array
      nRefLocalOpOffset = tAttributeSize*tNoOfAttributes*(count - 1) ;

      for (int countTables = 0;
	   countTables < loopCountTables && tResult == 0;
	   countTables++) {

	pOps[countTables] = pTrans->getNdbOperation(tableName[countTables]);  
	if (pOps[countTables] == NULL) {
	  // This is a fatal error, abort program
	  ndbout << "getNdbOperation: " << pTrans->getNdbError();
	  tResult = 2; // Indicate fatal error
	  break;
	}//if

	switch (tType) {
	case stInsert:          // Insert case
	  if (theWriteFlag == 1 && theDirtyFlag == 1)
	    pOps[countTables]->dirtyWrite();
	  else if (theWriteFlag == 1)
	    pOps[countTables]->writeTuple();
	  else
	    pOps[countTables]->insertTuple();
	  break;
	case stRead:            // Read Case
	  if (theSimpleFlag == 1)
	    pOps[countTables]->simpleRead();
	  else if (theDirtyFlag == 1)
	    pOps[countTables]->dirtyRead();
	  else
	    pOps[countTables]->readTuple();
	  break;
	case stUpdate:          // Update Case
	  if (theWriteFlag == 1 && theDirtyFlag == 1)
	    pOps[countTables]->dirtyWrite();
	  else if (theWriteFlag == 1)
	    pOps[countTables]->writeTuple();
	  else if (theDirtyFlag == 1)
	    pOps[countTables]->dirtyUpdate();
	  else
	    pOps[countTables]->updateTuple();
	  break;
	case stDelete:          // Delete Case
	  pOps[countTables]->deleteTuple();
	  break;
	case stVerify:
	  pOps[countTables]->readTuple();
	  break;
	case stVerifyDelete:
	  pOps[countTables]->readTuple();
	  break;
	default:
	  assert(false);
	}//switch

	
	if(useLongKeys){
	  // Loop the equal call so the complete key is send to the kernel.
	  for(Uint32 i = 0; i < tNoOfLongPK; i++) 
	    pOps[countTables]->equal(longKeyAttrName[i], 
				     (char *)longKeyAttrValue[count - 1][i], tSizeOfLongPK*4); 
	}
	else 
	  pOps[countTables]->equal((Uint32)0, 
				   (char*)&attrRefValue[nRefLocalOpOffset]);
	
	if (tType == stInsert || tType == stUpdate){
	  for (int ca = 1; ca < loopCountAttributes; ca++){
	    pOps[countTables]->setValue((Uint32)ca,
					(char*)&attrRefValue[nRefLocalOpOffset + tAttributeSize*ca]);
	  }//for
	} else if (tType == stRead || stVerify == tType) {
	  int nTableOffset = tAttributeSize * 
	    loopCountAttributes *
	    countTables ;
	  for (int ca = 1; ca < loopCountAttributes; ca++) {
	    tTmp = pOps[countTables]->getValue((Uint32)ca, 
					       (char*)&attrValue[nTableOffset + tAttributeSize*ca]);
	  }//for
	} else if (stVerifyDelete == tType) {
	  if(useLongKeys){
	    int nTableOffset = tAttributeSize *
	      loopCountAttributes *
	      countTables ;
	    tTmp = pOps[countTables]->getValue(longKeyAttrName[0], 
					       (char*)&attrValue[nTableOffset]);
	  } else {
	    int nTableOffset = tAttributeSize *
	      loopCountAttributes *
	      countTables ;
	    tTmp = pOps[countTables]->getValue((Uint32)0, 
					       (char*)&attrValue[nTableOffset]);
	  }
	}//if
      }//for Tables loop

      if (tResult != 0)
	break;
      check = pTrans->execute(Commit);

      // Decide what kind of error this is
      if ((tSpecialTrans == 1) &&
	  (check == -1)) {
	// --------------------------------------------------------------------
	// A special transaction have been executed, change to check = 0 in
	// certain situations.
	// --------------------------------------------------------------------
	switch (tType) {
	case stInsert:          // Insert case
	  if (630 == pTrans->getNdbError().code ) {
	    check = 0;
	    ndbout << "Insert with 4007 was successful" << endl;
	  }//if
	  break;
	case stDelete:          // Delete Case
	  if (626 == pTrans->getNdbError().code ) {
	    check = 0;
	    ndbout << "Delete with 4007 was successful" << endl;
	  }//if
	  break;
	default:
	  assert(false);
	}//switch
      }//if
      tSpecialTrans = 0;
      if (check == -1) {
	if ((stVerifyDelete == tType) && 
	    (626 == pTrans->getNdbError().code)) {
	  // ----------------------------------------------
	  // It's good news - the deleted tuple is gone, 
	  // so reset "check" flag
	  // ----------------------------------------------
	  check = 0 ;
	} else {
	  int retCode = 
	    theErrorData.handleErrorCommon(pTrans->getNdbError());
	  if (retCode == 1) {
	    ndbout_c("execute: %d, %d, %s", count, tType, 
		     pTrans->getNdbError().message );
	    ndbout_c("Error code = %d", pTrans->getNdbError().code );
	    tResult = 20;
	  } else if (retCode == 2) {
	    ndbout << "4115 should not happen in flexBench" << endl;
	    tResult = 20;
	  } else if (retCode == 3) {
	    // --------------------------------------------------------------------
	    // We are not certain if the transaction was successful or not.
	    // We must reexecute but might very well find that the transaction
	    // actually was updated. Updates and Reads are no problem here. Inserts
	    // will not cause a problem if error code 630 arrives. Deletes will
	    // not cause a problem if 626 arrives.
	    // --------------------------------------------------------------------
	    if ((tType == stInsert) || (tType == stDelete)) {
	      tSpecialTrans = 1;
	    }//if
	  }//if
	}//if
      }//if
      // Check if retries should be made
      if (check == -1 && tResult == 0) {
	if (tAttemptNo < tRetryAttempts){
	  tAttemptNo++;
	} else {
	  // --------------------------------------------------------------------
	  // Too many retries have been made, report error and break out of loop
	  // --------------------------------------------------------------------
	  ndbout << "Thread" << threadNo;
	  ndbout << ": too many errors reported" << endl;
	  tResult = 10;
	  break;
	}//if            
      }//if

      if (check == 0){
	// Go to the next record
	count++;
	tAttemptNo = 0;
#ifdef CEBIT_STAT
	// report successful ops
	if (statEnable) {
	  statOps += loopCountTables;
	  if (statOps >= statFreq) {
	    statReport(tType, statOps);
	    statOps = 0;
	  }//if
	}//if
#endif
      }//if

      if (stVerify == tType && 0 == check){
	int nTableOffset = 0 ;
	for (int a = 1 ; a < loopCountAttributes ; a++){
	  for (int tables = 0 ; tables < loopCountTables ; tables++){
	    nTableOffset = tables*loopCountAttributes*tAttributeSize ;
	    if (*(int*)&attrValue[nTableOffset + tAttributeSize*a] != *(int*)&attrRefValue[nRefLocalOpOffset + tAttributeSize*a]){
	      ndbout << "Error in verify:" << endl ;
	      ndbout << "attrValue[" << nTableOffset + tAttributeSize*a << "] = " << attrValue[a] << endl ;
	      ndbout << "attrRefValue[" << nRefLocalOpOffset + tAttributeSize*a << "]" << attrRefValue[nRefLocalOpOffset + tAttributeSize*a] << endl ;
	      tResult = 11 ;
	      break ;
	    }//if
	  }//for
	}//for
      }// if(stVerify ... )
      pNdb->closeTransaction(pTrans) ;  
    }// operations loop
#ifdef CEBIT_STAT
    // report remaining successful ops
    if (statEnable) {
      if (statOps > 0) {
	statReport(tType, statOps);
	statOps = 0;
      }//if
    }//if
#endif
  }
  delete pNdb;
  free(attrValue) ;
  free(attrRefValue) ;
  free(pOps) ;

  if (useLongKeys == true) {
    // Only free these areas if they have been allocated
    // Otherwise cores will occur
    for (Uint32 n = 0; n < tNoOfOperations; n++){
      for (Uint32 i = 0; i < tNoOfLongPK; i++) {
	free(longKeyAttrValue[n][i]);
      }
      free(longKeyAttrValue[n]);
    }
    free(longKeyAttrValue);
  } // if

  return NULL; // Thread exits
}


static int readArguments(int argc, const char** argv)
{

  int i = 1;
  while (argc > 1){
    if (strcmp(argv[i], "-t") == 0){
      tNoOfThreads = atoi(argv[i+1]);
      if ((tNoOfThreads < 1)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-o") == 0){
      tNoOfOperations = atoi(argv[i+1]);
      if (tNoOfOperations < 1) 
        return -1;;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-a") == 0){
      tNoOfAttributes = atoi(argv[i+1]);
      if ((tNoOfAttributes < 2) || (tNoOfAttributes > MAXATTR)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-lkn") == 0){
     tNoOfLongPK = atoi(argv[i+1]);
     useLongKeys = true;
      if ((tNoOfLongPK < 1) || (tNoOfLongPK > MAXNOLONGKEY) || 
	  (tNoOfLongPK * tSizeOfLongPK) > MAXLONGKEYTOTALSIZE){
      	ndbout << "Argument -lkn is not in the proper range." << endl;  
	return -1;
      }
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-lks") == 0){
      tSizeOfLongPK = atoi(argv[i+1]);
      useLongKeys = true;
      if ((tSizeOfLongPK < 1) || (tNoOfLongPK * tSizeOfLongPK) > MAXLONGKEYTOTALSIZE){
	ndbout << "Argument -lks is not in the proper range 1 to " << 
	  MAXLONGKEYTOTALSIZE << endl;
        return -1;
      }
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-c") == 0){
      tNoOfTables = atoi(argv[i+1]);
      if ((tNoOfTables < 1) || (tNoOfTables > MAXTABLES)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-stdtables") == 0){
      theStdTableNameFlag = 1;
    }else if (strcmp(argv[i], "-l") == 0){
      tNoOfLoops = atoi(argv[i+1]);
      if ((tNoOfLoops < 0) || (tNoOfLoops > 100000)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-s") == 0){
      tAttributeSize = atoi(argv[i+1]);
      if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-sleep") == 0){
      tSleepTime = atoi(argv[i+1]);
      if ((tSleepTime < 1) || (tSleepTime > 3600)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-simple") == 0){
      theSimpleFlag = 1;
    }else if (strcmp(argv[i], "-write") == 0){
      theWriteFlag = 1;
    }else if (strcmp(argv[i], "-dirty") == 0){
      theDirtyFlag = 1;
    }else if (strcmp(argv[i], "-no_table_create") == 0){
      theTableCreateFlag = 1;
    }else if (strcmp(argv[i], "-temp") == 0){
      theTempTable = true;
    }else if (strcmp(argv[i], "-noverify") == 0){
      VerifyFlag = false ;
    }else if (theErrorData.parseCmdLineArg(argv, i) == true){
      ; //empty, updated in errorArg(..)
    }else if (strcmp(argv[i], "-verify") == 0){
      VerifyFlag = true ;
#ifdef CEBIT_STAT
    }else if (strcmp(argv[i], "-statserv") == 0){
      if (! (argc > 2))
	return -1;
      const char *p = argv[i+1];
      const char *q = strrchr(p, ':');
      if (q == 0)
	return -1;
      BaseString::snprintf(statHost, sizeof(statHost), "%.*s", q-p, p);
      statPort = atoi(q+1);
      statEnable = true;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-statfreq") == 0){
      if (! (argc > 2))
	return -1;
      statFreq = atoi(argv[i+1]);
      if (statFreq < 1)
	return -1;
      argc -= 1;
      i++;
#endif
    }else{       
      return -1;
    }
    argc -= 1;
    i++;
  }
  return 0;
}

static void sleepBeforeStartingTest(int seconds){
  if (seconds > 0){
      ndbout << "Sleeping(" <<seconds << ")...";
      NdbSleep_SecSleep(seconds);
      ndbout << " done!" << endl;
    }
}


static int
createTables(Ndb* pMyNdb){
  int i;
  for (i = 0; i < tNoOfAttributes; i++){
    BaseString::snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
  }

  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables with SQL
  for (i = 0; i < tNoOfTables; i++){
    if (theStdTableNameFlag == 0){
      BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d_%d", i, 
	       (int)(NdbTick_CurrentMillisecond() / 1000));
    } else {
      BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    }
  }
  
  for(i = 0; i < tNoOfTables; i++){
    ndbout << "Creating " << tableName[i] << "... ";
    
    NdbDictionary::Table tmpTable(tableName[i]);
    
    tmpTable.setStoredTable(!theTempTable);
    
    if(useLongKeys){
      for(Uint32 i = 0; i < tNoOfLongPK; i++) {
	NdbDictionary::Column col(longKeyAttrName[i]);
	col.setType(NdbDictionary::Column::Unsigned);
	col.setLength(tSizeOfLongPK);
	col.setPrimaryKey(true);
	tmpTable.addColumn(col);
      }
    } else {
      NdbDictionary::Column col(attrName[0]);
      col.setType(NdbDictionary::Column::Unsigned);
      col.setLength(1);
      col.setPrimaryKey(true);
      tmpTable.addColumn(col);
    }
    
    
    NdbDictionary::Column col;
    col.setType(NdbDictionary::Column::Unsigned);
    col.setLength(tAttributeSize);
    for (unsigned j = 1; j < tNoOfAttributes; j++){
      col.setName(attrName[j]);
      tmpTable.addColumn(col);
    }
    
    if(pMyNdb->getDictionary()->createTable(tmpTable) == -1){
      return -1;
    }
    ndbout << "done" << endl;
  }
  
  return 0;
}

      
static void input_error(){
  ndbout << endl << "Invalid argument!" << endl;
  ndbout << endl << "Arguments:" << endl;
  ndbout << "   -t Number of threads to start, default 1" << endl;
  ndbout << "   -o Number of operations per loop, default 500" << endl;
  ndbout << "   -l Number of loops to run, default 1, 0=infinite" << endl;
  ndbout << "   -a Number of attributes, default 25" << endl;
  ndbout << "   -c Number of tables, default 1" << endl;
  ndbout << "   -s Size of each attribute, default 1 (Primary Key is always of size 1," << endl;
  ndbout << "      independent of this value)" << endl;
  ndbout << "   -lkn Number of long primary keys, default 1" << endl;
  ndbout << "   -lks Size of each long primary key, default 1" << endl;

  ndbout << "   -simple Use simple read to read from database" << endl;
  ndbout << "   -dirty Use dirty read to read from database" << endl;
  ndbout << "   -write Use writeTuple in insert and update" << endl;
  ndbout << "   -stdtables Use standard table names" << endl;
  ndbout << "   -no_table_create Don't create tables in db" << endl;
  ndbout << "   -sleep Sleep a number of seconds before running the test, this" << endl;
  ndbout << "    can be used so that another flexBench have time to create tables" << endl;
  ndbout << "   -temp Use tables without logging" << endl;
  ndbout << "   -verify Verify inserts, updates and deletes" << endl ;
  theErrorData.printCmdLineArgs(ndbout);
  ndbout << endl <<"Returns:" << endl;
  ndbout << "\t 0 - Test passed" << endl;
  ndbout << "\t 1 - Test failed" << endl;
  ndbout << "\t 2 - Invalid arguments" << endl << endl;
}

// vim: set sw=2:
