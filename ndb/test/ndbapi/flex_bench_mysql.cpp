/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
    -use_ndb Use NDB API, otherwise use mysql client
#ifdef CEBIT_STAT
    -statserv host:port  statistics server to report to
    -statfreq ops        report every ops operations (default 100)
#endif
    Returns:
    0 - Test passed
    1 - Test failed
    2 - Invalid arguments

* *************************************************** */

#define USE_MYSQL
#ifdef USE_MYSQL
#include <mysql.h>
#endif

#include "NdbApi.hpp"

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbTimer.hpp>
#include <NdbThread.h>
#include <NdbAutoPtr.hpp>

#include <NdbTest.hpp>

#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 128
#define MAXATTRSIZE 1000
#define MAXNOLONGKEY 16 // Max number of long keys.
#define MAXLONGKEYTOTALSIZE 1023 // words = 4092 bytes

extern "C" { static void* flexBenchThread(void*); }
static int readArguments(int argc, const char** argv);
#ifdef USE_MYSQL
static int createTables(MYSQL*);
static int dropTables(MYSQL*);
#endif
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
static unsigned int         t_instances = 1;

//Program Flags
static int                  theSimpleFlag = 0;
static int                  theDirtyFlag = 0;
static int                  theWriteFlag = 0;
static int                  theStdTableNameFlag = 0;
static int                  theTableCreateFlag = 0;
static bool                 theTempTable = false;
static bool                 VerifyFlag = true;
static bool                 useLongKeys = false;
static bool                 verbose = false;
#ifdef USE_MYSQL
static bool                 use_ndb = false;
static int                  engine_id = 0;
static int                  sockets[16];
static int                  n_sockets = 0;
static char*                engine[] =
  { 
    " ENGINE = NDBCLUSTER ", // use default engine
    " ENGINE = MEMORY ",
    " ENGINE = MYISAM ",
    " ENGINE = INNODB "
  };
#else
static bool                 use_ndb = true;
#endif

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

NDB_COMMAND(flexBench, "flexBench", "flexBench", "flexbench", 65535)
{
  ndb_init();
  ThreadData*           pThreadsData;
  int                   tLoops = 0;
  int                   returnValue = NDBT_OK;
  if (readArguments(argc, argv) != 0){
    input_error();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  NdbAutoPtr<char> p10;
  if(useLongKeys){
    int e1 = sizeof(char*) * tNoOfLongPK;
    int e2_1 = strlen("KEYATTR  ") + 1;
    int e2 = e2_1 * tNoOfLongPK;
    char *tmp = (char *) malloc(e1 + e2);
    p10.reset(tmp);
    longKeyAttrName = (char **) tmp;
    tmp += e1;
    for (Uint32 i = 0; i < tNoOfLongPK; i++) {
      //      longKeyAttrName[i] = (char *) malloc(strlen("KEYATTR  ") + 1);
      longKeyAttrName[i] = tmp;
      tmp += e2_1;
      memset(longKeyAttrName[i], 0, e2_1);
      sprintf(longKeyAttrName[i], "KEYATTR%i", i);
    }
  }

  NdbAutoObjArrayPtr<ThreadData>
    p12( pThreadsData = new ThreadData[tNoOfThreads] );

 
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
  if (use_ndb) {
    ndbout << "Use NDB API with NdbPool in this test case" << endl;
    ndbout << "Pool size = " << t_instances << endl;
  } else {
    ndbout << "Use mysql client with " << engine[engine_id];
    ndbout << " as engine" << endl;
  }
  theErrorData.printSettings(ndbout);
  
  NdbThread_SetConcurrencyLevel(tNoOfThreads + 2);

#ifdef USE_MYSQL
  MYSQL mysql;
  if (!use_ndb) {
    if ( mysql_thread_safe() == 0 ) {
      ndbout << "Not thread safe mysql library..." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    ndbout << "Connecting to MySQL..." <<endl;

    mysql_init(&mysql);
    {
      int the_socket = sockets[0];
      char the_socket_name[1024];
      sprintf(the_socket_name, "%s%u%s", "/tmp/mysql.",the_socket,".sock");
      //    sprintf(the_socket_name, "%s", "/tmp/mysql.sock");
      ndbout << the_socket_name << endl;
      if ( mysql_real_connect(&mysql,
			    "localhost",
			    "root",
			    "",
			    "test",
			    the_socket,
			    the_socket_name,
			    0) == NULL ) {
        ndbout << "Connect failed" <<endl;
        returnValue = NDBT_FAILED;
      }
    }
    if(returnValue == NDBT_OK){
      mysql_set_server_option(&mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON);
      if (createTables(&mysql) != 0){
        returnValue = NDBT_FAILED;
      }
    }
  }
#endif
  if (use_ndb) {
    Uint32 ndb_id = 0;
    if (!create_instance(t_instances, 1, t_instances)) {
      ndbout << "Creation of the NdbPool failed" << endl;
      returnValue = NDBT_FAILED;
    } else {
      Ndb* pNdb = get_ndb_object(ndb_id, "test", "def");
      if (pNdb == NULL) {
        ndbout << "Failed to get a NDB object" << endl;
        returnValue = NDBT_FAILED;
      } else {
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
        return_ndb_object(pNdb, ndb_id);
      }
    }
  }
  if(returnValue == NDBT_OK){

    sleepBeforeStartingTest(tSleepTime);
    
    /****************************************************************
     *  Create threads.                                           *
     ****************************************************************/
    resetThreads(pThreadsData);
    
    for (unsigned int i = 0; i < tNoOfThreads; i++){  
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
      // Give update-command to all threads
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
       * Perform delete.                                              *
       ****************************************************************/
      // Reset and start timer
      START_TIMER;
      // Give delete-command to all threads
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
    for(Uint32 i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(pThreadsData[i].threadLife, &tmp);
      NdbThread_Destroy(&pThreadsData[i].threadLife);
    }
  }
#ifdef USE_MYSQL
  if (!use_ndb) {
    dropTables(&mysql);
    mysql_close(&mysql);
  }
#endif
  if (use_ndb) {
    drop_instance();
  }
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
  Uint32            ndb_id = 0;
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
  unsigned***           longKeyAttrValue = NULL;


  threadNo = pThreadData->threadNo ;

#ifdef USE_MYSQL
  MYSQL mysql;
  int the_socket = sockets[threadNo % n_sockets];
  char the_socket_name[1024];
  //sprintf(the_socket_name, "%s", "/tmp/mysql.sock");
  sprintf(the_socket_name, "%s%u%s", "/tmp/mysql.",the_socket,".sock");
  if (!use_ndb) {
    ndbout << the_socket_name << endl;
    ndbout << "Thread connecting to MySQL... " << endl;
    mysql_init(&mysql);
    
    if ( mysql_real_connect(&mysql,
			    "localhost",
			    "root",
			    "",
			    "test",
			    the_socket,
			    the_socket_name,
			    0) == NULL ) {
      ndbout << "failed" << endl;
      NdbThread_Exit(0) ;
    }
    ndbout << "ok" << endl;

    int r;
    if (tNoOfTables > 1)
      r = mysql_autocommit(&mysql, 0);
    else
      r = mysql_autocommit(&mysql, 1);

    if (r) {
      ndbout << "autocommit on/off failed" << endl;
      NdbThread_Exit(0) ;
    }
  }
#endif

  NdbAutoPtr<int> p00( attrValue= (int*)malloc(nReadBuffSize) ) ;
  NdbAutoPtr<int> p01( attrRefValue= (int*)malloc(nRefBuffSize) );
  if (use_ndb) {
    pOps = (NdbOperation**)malloc(tNoOfTables*sizeof(NdbOperation*)) ;
  }
  NdbAutoPtr<NdbOperation*> p02( pOps );

  if( !attrValue || !attrRefValue ||
      ( use_ndb && ( !pOps) ) ){
    // Check allocations to make sure we got all the memory we asked for
    ndbout << "One or more memory allocations failed when starting thread #";
    ndbout << threadNo << endl ;
    ndbout << "Thread #" << threadNo << " will now exit" << endl ;
    tResult = 13 ;
    NdbThread_Exit(0) ;
  }
  
  if (use_ndb) {
    pNdb = get_ndb_object(ndb_id, "test", "def");
    if (pNdb == NULL) {
      ndbout << "Failed to get an NDB object" << endl;
      ndbout << "Thread #" << threadNo << " will now exit" << endl ;
      tResult = 13;
      NdbThread_Exit(0) ;
    }
    pNdb->waitUntilReady();
    return_ndb_object(pNdb, ndb_id);
    pNdb = NULL;
  }

  // To make sure that two different threads doesn't operate on the same record
  // Calculate an "unique" number to use as primary key
  threadBase = (threadNo * 2000000) + (tNodeId * 260000000);

  NdbAutoPtr<char> p22;
  if(useLongKeys){
    // Allocate and populate the longkey array.
    int e1 = sizeof(unsigned**) * tNoOfOperations;
    int e2 = sizeof(unsigned*) * tNoOfLongPK * tNoOfOperations;
    int e3 = sizeof(unsigned) * tSizeOfLongPK * tNoOfLongPK * tNoOfOperations;
    char* tmp;
    p22.reset(tmp = (char*)malloc(e1+e2+e3));

    longKeyAttrValue = (unsigned ***) tmp;
    tmp += e1;
    for (Uint32 n = 0; n < tNoOfOperations; n++) {
      longKeyAttrValue[n] = (unsigned **) tmp;
      tmp += sizeof(unsigned*) * tNoOfLongPK;
    }

    for (Uint32 n = 0; n < tNoOfOperations; n++){
      for (Uint32 i = 0; i < tNoOfLongPK ; i++) {
	longKeyAttrValue[n][i] = (unsigned *) tmp;
	tmp += sizeof(unsigned) * tSizeOfLongPK;
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

#ifdef USE_MYSQL
  // temporary buffer to store prepared statement text
  char buf[2048];
  MYSQL_STMT** prep_read   = NULL;
  MYSQL_STMT** prep_delete = NULL;
  MYSQL_STMT** prep_update = NULL;
  MYSQL_STMT** prep_insert = NULL;
  MYSQL_BIND* bind_delete = NULL;
  MYSQL_BIND* bind_read   = NULL;
  MYSQL_BIND* bind_update = NULL;
  MYSQL_BIND* bind_insert = NULL;
  int* mysql_data = NULL;

  NdbAutoPtr<char> p21;

  if (!use_ndb) {
    // data array to which prepared statements are bound
    char* tmp;
    int e1 = sizeof(int)*tAttributeSize*tNoOfAttributes;
    int e2 = sizeof(MYSQL_BIND)*tNoOfAttributes;
    int e3 = sizeof(MYSQL_BIND)*tNoOfAttributes;
    int e4 = sizeof(MYSQL_BIND)*tNoOfAttributes;
    int e5 = sizeof(MYSQL_BIND)*1;
    int e6 = sizeof(MYSQL_STMT*)*tNoOfTables;
    int e7 = sizeof(MYSQL_STMT*)*tNoOfTables;
    int e8 = sizeof(MYSQL_STMT*)*tNoOfTables;
    int e9 = sizeof(MYSQL_STMT*)*tNoOfTables;
    p21.reset(tmp = (char*)malloc(e1+e2+e3+e4+e5+e6+e7+e8+e9));

    mysql_data  = (int*)tmp;         tmp += e1;
    bind_insert = (MYSQL_BIND*)tmp;  tmp += e2;
    bind_update = (MYSQL_BIND*)tmp;  tmp += e3;
    bind_read   = (MYSQL_BIND*)tmp;  tmp += e4;
    bind_delete = (MYSQL_BIND*)tmp;  tmp += e5;
    prep_insert = (MYSQL_STMT**)tmp; tmp += e6;
    prep_update = (MYSQL_STMT**)tmp; tmp += e7;
    prep_read   = (MYSQL_STMT**)tmp; tmp += e8;
    prep_delete = (MYSQL_STMT**)tmp;

    for (Uint32 ca = 0; ca < tNoOfAttributes; ca++){
      MYSQL_BIND& bi = bind_insert[ca];
      bi.buffer_type = MYSQL_TYPE_LONG;
      bi.buffer = (char*)&mysql_data[ca*tAttributeSize];
      bi.buffer_length = 0;
      bi.length = NULL;
      bi.is_null = NULL;
    }//for
    
    for (Uint32 ca = 0; ca < tNoOfAttributes; ca++){
      MYSQL_BIND& bi = bind_update[ca];
      bi.buffer_type = MYSQL_TYPE_LONG;
      if ( ca == tNoOfAttributes-1 ) // the primary key comes last in statement
	bi.buffer = (char*)&mysql_data[0];
      else
	bi.buffer = (char*)&mysql_data[(ca+1)*tAttributeSize];
      bi.buffer_length = 0;
      bi.length = NULL;
      bi.is_null = NULL;
    }//for
    
    for (Uint32 ca = 0; ca < tNoOfAttributes; ca++){
      MYSQL_BIND& bi = bind_read[ca];
      bi.buffer_type = MYSQL_TYPE_LONG;
      bi.buffer = (char*)&mysql_data[ca*tAttributeSize];
      bi.buffer_length = 4;
      bi.length = NULL;
      bi.is_null = NULL;
    }//for
    
    for (Uint32 ca = 0; ca < 1; ca++){
      MYSQL_BIND& bi = bind_delete[ca];
      bi.buffer_type = MYSQL_TYPE_LONG;
      bi.buffer = (char*)&mysql_data[ca*tAttributeSize];
      bi.buffer_length = 0;
      bi.length = NULL;
      bi.is_null = NULL;
    }//for
    
    for (Uint32 i = 0; i < tNoOfTables; i++) {
      int pos = 0;
      pos += sprintf(buf+pos, "%s%s%s",
		     "INSERT INTO ",
		     tableName[i],
		     " VALUES(");
      pos += sprintf(buf+pos, "%s", "?");
      for (Uint32 j = 1; j < tNoOfAttributes; j++) {
	pos += sprintf(buf+pos, "%s", ",?");
      }
      pos += sprintf(buf+pos, "%s", ")");
      if (verbose)
	ndbout << buf << endl;
      prep_insert[i] = mysql_prepare(&mysql, buf, pos);
      if (prep_insert[i] == 0) {
	ndbout << "mysql_prepare: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
      if (mysql_bind_param(prep_insert[i], bind_insert)) {
	ndbout << "mysql_bind_param: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
    }
    
    for (Uint32 i = 0; i < tNoOfTables; i++) {
      int pos = 0;
      pos += sprintf(buf+pos, "%s%s%s",
		     "UPDATE ",
		     tableName[i],
		     " SET ");
      for (Uint32 j = 1; j < tNoOfAttributes; j++) {
	if (j != 1)
	  pos += sprintf(buf+pos, "%s", ",");
	pos += sprintf(buf+pos, "%s%s", attrName[j],"=?");
      }
      pos += sprintf(buf+pos, "%s%s%s", " WHERE ", attrName[0], "=?");
      
      if (verbose)
	ndbout << buf << endl;
      prep_update[i] = mysql_prepare(&mysql, buf, pos);
      if (prep_update[i] == 0) {
	ndbout << "mysql_prepare: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
      if (mysql_bind_param(prep_update[i], bind_update)) {
	ndbout << "mysql_bind_param: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
    }
    
    for (Uint32 i = 0; i < tNoOfTables; i++) {
      int pos = 0;
      pos += sprintf(buf+pos, "%s", "SELECT ");
      for (Uint32 j = 1; j < tNoOfAttributes; j++) {
	if (j != 1)
	  pos += sprintf(buf+pos, "%s", ",");
	pos += sprintf(buf+pos, "%s", attrName[j]);
      }
      pos += sprintf(buf+pos, "%s%s%s%s%s",
		     " FROM ",
		     tableName[i],
		     " WHERE ",
		     attrName[0],
		     "=?");
      if (verbose)
	ndbout << buf << endl;
      prep_read[i] = mysql_prepare(&mysql, buf, pos);
      if (prep_read[i] == 0) {
	ndbout << "mysql_prepare: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
      if (mysql_bind_param(prep_read[i], bind_read)) {
	ndbout << "mysql_bind_param: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
      if (mysql_bind_result(prep_read[i], &bind_read[1])) {
	ndbout << "mysql_bind_result: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
    }
    
    for (Uint32 i = 0; i < tNoOfTables; i++) {
      int pos = 0;
      pos += sprintf(buf+pos, "%s%s%s%s%s",
		     "DELETE FROM ",
		     tableName[i],
		     " WHERE ",
		     attrName[0],
		     "=?");
      if (verbose)
	ndbout << buf << endl;
      prep_delete[i] = mysql_prepare(&mysql, buf, pos);
      if (prep_delete[i] == 0) {
	ndbout << "mysql_prepare: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
      if (mysql_bind_param(prep_delete[i], bind_delete)) {
	ndbout << "mysql_bind_param: " << mysql_error(&mysql) << endl;
	NdbThread_Exit(0) ;
      }
    }
  }
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

      if (use_ndb) {
        pNdb = get_ndb_object(ndb_id, "test", "def");
        if (pNdb == NULL) {
          ndbout << "Could not get Ndb object in thread" << threadNo;
          ndbout << endl;
          tResult = 1; //Indicate fatal error
          break;
        }
	pTrans = pNdb->startTransaction();
	if (pTrans == NULL) {
	  // This is a fatal error, abort program
	  ndbout << "Could not start transaction in thread" << threadNo;
	  ndbout << endl;
	  ndbout << pNdb->getNdbError() << endl;
	  tResult = 1; // Indicate fatal error
	  break; // Break out of for loop
	}
      }

      // Calculate the current operation offset in the reference array
      nRefLocalOpOffset = tAttributeSize*tNoOfAttributes*(count - 1) ;
      int* tmpAttrRefValue = attrRefValue + nRefLocalOpOffset;

      for (int countTables = 0;
	   countTables < loopCountTables && tResult == 0;
	   countTables++) {

	int nTableOffset = tAttributeSize *
	  loopCountAttributes *
	  countTables ;

	int* tmpAttrValue = attrValue + nTableOffset;

	if (use_ndb) {
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
				       (char *)longKeyAttrValue[count - 1][i],
				       tSizeOfLongPK*4); 
	  }
	  else 
	    pOps[countTables]->equal((char*)attrName[0], 
				     (char*)&tmpAttrRefValue[0]);

	  if (tType == stInsert) {
	    for (int ca = 1; ca < loopCountAttributes; ca++){
	      pOps[countTables]->setValue((char*)attrName[ca],
					  (char*)&tmpAttrRefValue[tAttributeSize*ca]);
	    }//for
	  } else if (tType == stUpdate) {
	    for (int ca = 1; ca < loopCountAttributes; ca++){
	      int* tmp = (int*)&tmpAttrRefValue[tAttributeSize*ca];
	      if (countTables == 0)
		(*tmp)++;
	      pOps[countTables]->setValue((char*)attrName[ca],(char*)tmp);
	    }//for
	  } else if (tType == stRead || stVerify == tType) {
	    for (int ca = 1; ca < loopCountAttributes; ca++) {
	      tTmp =
		pOps[countTables]->getValue((char*)attrName[ca], 
					    (char*)&tmpAttrValue[tAttributeSize*ca]);
	    }//for
	  } else if (stVerifyDelete == tType) {
	    if(useLongKeys){
	      tTmp = pOps[countTables]->getValue(longKeyAttrName[0], 
						 (char*)&tmpAttrValue[0]);
	    } else {
	      tTmp = pOps[countTables]->getValue((char*)attrName[0], 
						 (char*)&tmpAttrValue[0]);
	    }
	  }//if
	} else { // !use_ndb
#ifndef USE_MYSQL
	  assert(false);
#else
	  switch (tType)
	    {
	    case stInsert:
	      for (int ca = 0; ca < loopCountAttributes; ca++){
		mysql_data[ca] = tmpAttrRefValue[tAttributeSize*ca];
	      }//for
	      if (mysql_execute(prep_insert[countTables])) {
		ndbout << tableName[countTables];  
		ndbout << " mysql_execute: " << mysql_error(&mysql) << endl;
		tResult = 1 ;
	      }
	      break;
	    case stUpdate:          // Update Case
	      mysql_data[0] = tmpAttrRefValue[0];
	      for (int ca = 1; ca < loopCountAttributes; ca++){
		int* tmp = (int*)&tmpAttrRefValue[tAttributeSize*ca];
		if (countTables == 0)
		  (*tmp)++;
		mysql_data[ca] = *tmp;
	      }//for
	      if (mysql_execute(prep_update[countTables])) {
		ndbout << tableName[countTables];  
		ndbout << " mysql_execute: " << mysql_error(&mysql) << endl;
		tResult = 2 ;
	      }
	      break;
	    case stVerify:
	    case stRead:            // Read Case
	      mysql_data[0] = tmpAttrRefValue[0];
	      if (mysql_execute(prep_read[countTables])) {
		ndbout << tableName[countTables];  
		ndbout << " mysql_execute: " << mysql_error(&mysql) << endl;
		tResult = 3 ;
		break;
	      }
	      if (mysql_stmt_store_result(prep_read[countTables])) {
		ndbout << tableName[countTables];  
		ndbout << " mysql_stmt_store_result: "
		       << mysql_error(&mysql) << endl;
		tResult = 4 ;
		break;
	      }
	      {
		int rows= 0;
		int r;
		while ( (r= mysql_fetch(prep_read[countTables])) == 0 ){
		  rows++;
		}
		if ( r == 1 ) {
		  ndbout << tableName[countTables];  
		  ndbout << " mysql_fetch: " << mysql_error(&mysql) << endl;
		  tResult = 5 ;
		  break;
		}
		if ( rows != 1 ) {
		  ndbout << tableName[countTables];  
		  ndbout << " mysql_fetch: rows = " << rows << endl;
		  tResult = 6 ;
		  break;
		}
	      }
	      {
		for (int ca = 1; ca < loopCountAttributes; ca++) {
		  tmpAttrValue[tAttributeSize*ca] = mysql_data[ca];
		}
	      }
	      break;
	    case stDelete:          // Delete Case
	      mysql_data[0] = tmpAttrRefValue[0];
	      if (mysql_execute(prep_delete[countTables])) {
		ndbout << tableName[countTables];  
		ndbout << " mysql_execute: " << mysql_error(&mysql) << endl;
		tResult = 7 ;
		break;
	      }
	      break;
	    case stVerifyDelete:
	      {
		sprintf(buf, "%s%s%s",
			"SELECT COUNT(*) FROM ",tableName[countTables],";");
		if (mysql_query(&mysql, buf)) {
		  ndbout << buf << endl;
		  ndbout << "Error: " << mysql_error(&mysql) << endl;
		  tResult = 8 ;
		  break;
		}
		MYSQL_RES *res = mysql_store_result(&mysql);
		if ( res == NULL ) {
		  ndbout << "mysql_store_result: "
			 << mysql_error(&mysql) << endl
			 << "errno: " << mysql_errno(&mysql) << endl;
		  tResult = 9 ;
		  break;
		}
		int num_fields = mysql_num_fields(res);
		int num_rows   = mysql_num_rows(res);
		if ( num_rows != 1 || num_fields != 1 ) {
		  ndbout << tableName[countTables];  
		  ndbout << " mysql_store_result: num_rows = " << num_rows
			 << " num_fields = " << num_fields << endl;
		  tResult = 10 ;
		  break;
		}
		MYSQL_ROW row = mysql_fetch_row(res);
		if ( row == NULL ) {
		  ndbout << "mysql_fetch_row: "
			 << mysql_error(&mysql) << endl;
		  tResult = 11 ;
		  break;
		}
		if ( *(char*)row[0] != '0' ) {
		  ndbout << tableName[countTables];  
		  ndbout << " mysql_fetch_row: value = "
			 << (char*)(row[0]) << endl;
		  tResult = 12 ;
		  break;
		}
		mysql_free_result(res);
	      }
	      break;
	    default:
	      assert(false);
	    }
#endif
	}
      }//for Tables loop

      if (tResult != 0)
	break;

      if (use_ndb){
	check = pTrans->execute(Commit);
      } else {
#ifdef USE_MYSQL
	if (tNoOfTables > 1)
	  if (mysql_commit(&mysql)) {
	    ndbout << " mysql_commit: " << mysql_error(&mysql) << endl;
	    tResult = 13;
	  } else 
	    check = 0;
#endif
      }

      if (use_ndb) {
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
      }

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
	    nTableOffset = tables*loopCountAttributes*tAttributeSize;
	    int ov =*(int*)&attrValue[nTableOffset + tAttributeSize*a];
	    int nv =*(int*)&tmpAttrRefValue[tAttributeSize*a];
	    if (ov != nv){
	      ndbout << "Error in verify ";
	      ndbout << "pk = " << tmpAttrRefValue[0] << ":" << endl;
	      ndbout << "attrValue[" << nTableOffset + tAttributeSize*a << "] = " << ov << endl ;
	      ndbout << "attrRefValue[" << nRefLocalOpOffset + tAttributeSize*a << "]" << nv << endl ;
	      tResult = 11 ;
	      break ;
	    }//if
	  }//for
	}//for
      }// if(stVerify ... )
      if (use_ndb) {
	pNdb->closeTransaction(pTrans);
        return_ndb_object(pNdb, ndb_id);
        pNdb = NULL;
      }
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
    if (pNdb) {
      pNdb->closeTransaction(pTrans);
      return_ndb_object(pNdb, ndb_id);
      pNdb = NULL;
    }
  }

#ifdef USE_MYSQL
  if (!use_ndb) {
    mysql_close(&mysql);
    for (Uint32 i = 0; i < tNoOfTables; i++) {
      mysql_stmt_close(prep_insert[i]);
      mysql_stmt_close(prep_update[i]);
      mysql_stmt_close(prep_delete[i]);
      mysql_stmt_close(prep_read[i]);
    }
  }
#endif
  if (use_ndb && pNdb) {
    ndbout << "I got here " << endl;
    return_ndb_object(pNdb, ndb_id);
  }
  NdbThread_Exit(0);
  return NULL; // Just to keep compiler happy
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
    }else if (strcmp(argv[i], "-pool_size") == 0){
      t_instances = atoi(argv[i+1]);
      if ((t_instances < 1) || (t_instances > 240)) 
        return -1;
      argc -= 1;
      i++;
#ifdef USE_MYSQL
    }else if (strcmp(argv[i], "-engine") == 0){
      engine_id = atoi(argv[i+1]);
      if ((engine_id < 0) || (engine_id > 3)) 
        return -1;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-socket") == 0){
      sockets[n_sockets] = atoi(argv[i+1]);
      if (sockets[n_sockets] <= 0)
        return -1;
      n_sockets++;
      argc -= 1;
      i++;
    }else if (strcmp(argv[i], "-use_ndb") == 0){
      use_ndb = true;
#endif
    }else if (strcmp(argv[i], "-s") == 0){
      tAttributeSize = atoi(argv[i+1]);
      if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE)) 
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
    }else if (strcmp(argv[i], "-simple") == 0){
      theSimpleFlag = 1;
    }else if (strcmp(argv[i], "-write") == 0){
      theWriteFlag = 1;
    }else if (strcmp(argv[i], "-dirty") == 0){
      theDirtyFlag = 1;
    }else if (strcmp(argv[i], "-sleep") == 0){
      tSleepTime = atoi(argv[i+1]);
      if ((tSleepTime < 1) || (tSleepTime > 3600)) 
        return -1;
      argc -= 1;
      i++;
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
      snprintf(statHost, sizeof(statHost), "%.*s", q-p, p);
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
#ifdef USE_MYSQL
  if (n_sockets == 0) {
    n_sockets = 1;
    sockets[0] = 3306;
  }
#endif
  return 0;
}

static void sleepBeforeStartingTest(int seconds){
  if (seconds > 0){
      ndbout << "Sleeping(" <<seconds << ")...";
      NdbSleep_SecSleep(seconds);
      ndbout << " done!" << endl;
    }
}


#ifdef USE_MYSQL
static int
dropTables(MYSQL* mysqlp){
  char buf[2048];
  for(unsigned i = 0; i < tNoOfTables; i++){
    int pos = 0;
    ndbout << "Dropping " << tableName[i] << "... ";
    pos += sprintf(buf+pos, "%s", "DROP TABLE ");
    pos += sprintf(buf+pos, "%s%s", tableName[i], ";");
    if (verbose)
      ndbout << endl << buf << endl;
    if (mysql_query(mysqlp, buf) != 0){
      ndbout << "Failed!"<<endl
	     <<mysql_error(mysqlp)<<endl
	     <<buf<<endl;
    } else
      ndbout << "OK!" << endl;
  }
  
  return 0;
}
#endif

#ifdef USE_MYSQL
static int
createTables(MYSQL* mysqlp){

  for (Uint32 i = 0; i < tNoOfAttributes; i++){
    snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
  }

  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables with SQL
  for (Uint32 i = 0; i < tNoOfTables; i++){
    if (theStdTableNameFlag == 0){
      snprintf(tableName[i], MAXSTRLEN, "TAB%d_%d", i, 
	       (int)(NdbTick_CurrentMillisecond() / 1000));
    } else {
      snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    }
  }
  
  char buf[2048];
  for(unsigned i = 0; i < tNoOfTables; i++){
    int pos = 0;
    ndbout << "Creating " << tableName[i] << "... ";
    
    pos += sprintf(buf+pos, "%s", "CREATE TABLE ");
    pos += sprintf(buf+pos, "%s%s", tableName[i], " ");
    if(useLongKeys){
      for(Uint32 i = 0; i < tNoOfLongPK; i++) {
      }
    } else {
      pos += sprintf(buf+pos, "%s%s%s",
		     "(", attrName[0], " int unsigned primary key");
    }
    for (unsigned j = 1; j < tNoOfAttributes; j++)
      pos += sprintf(buf+pos, "%s%s%s", ",", attrName[j], " int unsigned");
    pos += sprintf(buf+pos, "%s%s%s", ")", engine[engine_id], ";");
    if (verbose)
      ndbout << endl << buf << endl;
    if (mysql_query(mysqlp, buf) != 0)
      return -1;
    ndbout << "done" << endl;
  }
  return 0;
}
#endif

static int
createTables(Ndb* pMyNdb){

  for (Uint32 i = 0; i < tNoOfAttributes; i++){
    snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
  }

  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables with SQL
  for (Uint32 i = 0; i < tNoOfTables; i++){
    if (theStdTableNameFlag == 0){
      snprintf(tableName[i], MAXSTRLEN, "TAB%d_%d", i, 
	       (int)(NdbTick_CurrentMillisecond() / 1000));
    } else {
      snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    }
  }
  
  for(unsigned i = 0; i < tNoOfTables; i++){
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
  ndbout << "   -use_ndb Use NDB API (otherwise use mysql client)" << endl ;
  ndbout << "   -pool_size Number of Ndb objects in pool" << endl ;
  theErrorData.printCmdLineArgs(ndbout);
  ndbout << endl <<"Returns:" << endl;
  ndbout << "\t 0 - Test passed" << endl;
  ndbout << "\t 1 - Test failed" << endl;
  ndbout << "\t 2 - Invalid arguments" << endl << endl;
}

// vim: set sw=2:
