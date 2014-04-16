/*
   Copyright (C) 2003-2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

/* ***************************************************
       FLEXSCAN
       Perform benchmark of:
         insert
	 read
	 scan read
	 update
	 scan update
	 read
	 scan delete
	 verify delete

       Arguments:
        -f Location of my.cnf file, default my.cnf
        -t Number of threads to start, default 1
        -o Number of operations per loop, default 500	-l Number of loops to run, default 1, 0=infinite
        -a Number of attributes, default 25
        -c Number of tables, default 1
	-s Size of each attribute, default 1
	-stdtables Use standard table names
        -no_table_create Don't create tables in db
	-sleep Sleep a number of seconds before running the test, this 
	       can be used so that another flexBench hav etome to create tables
	-p Parallellism to use 1-32, default:1
	-abort <number> Test scan abort after a number of tuples
	-h Print help text
	-no_scan_update Don't do scan updates
	-no_scan_delete Don't do scan deletes
	
       Returns:
        NDBT_OK - Test passed
        NDBT_FAILED - Test failed

  Revision history:
    1.12 020222 epesson: Rewritten to use NDBT. Major bugs fixed

 * *************************************************** */

#include "NdbApi.hpp"

#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <NdbTimer.hpp>
#include <NdbMain.h>
#include <NdbTest.hpp>
#include <NDBT_Error.hpp>
#include <NdbSchemaCon.hpp>

#define PKSIZE 1
#define FOREVER 1
#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 64
#define NDB_MAXTHREADS 256
/*
  NDB_MAXTHREADS used to be just MAXTHREADS, which collides with a
  #define from <sys/thread.h> on AIX (IBM compiler).  We explicitly
  #undef it here lest someone use it by habit and get really funny
  results.  K&R says we may #undef non-existent symbols, so let's go.
*/
#undef MAXTHREADS
#define MAXATTRSIZE 64

enum StartType { 
  stIdle, 
  stInsert,
  stRead, 
  stScanRead,
  stUpdate,
  stScanUpdate,
  stDelete,
  stVerifyDelete, 
  stScanDelete,
  stStop,
  stLast} ;


struct ThreadNdb
{
  int ThreadNo;
  NdbThread* threadLife;
  StartType threadStart;
  int threadResult;
  int threadReady;
};

extern "C" void* flexScanThread(void*);
static int setAttrNames(void);
static int setTableNames(void);
static int createTables(Ndb* pMyNdb);
static void sleepBeforeStartingTest(int seconds);
static int readArguments(int argc, const char** argv);
static void setAttrValues(int* attrValue, 
			  int* readValue, 
			  int Offset);
static int insertRows(Ndb* pNdb, int* pkValue, int* attrValue, StartType tType);
static int readRows(Ndb* pNdb, int* pkValue, int* readValue);
static int deleteRows(Ndb* pNdb, int* pkValue);
static int scanReadRows(Ndb* pNdb, int* readValue);
static int scanUpdateRows(Ndb* pNdb, int* readValue, int* attrValue);
static int scanDeleteRows(Ndb* pNdb, int* readValue);
static int verifyDeleteRows(Ndb* pNdb, int* pkValue, int* readValue);
static void Compare(int* attrValue, int* readValue);
static void UpdateArray(int *attrValue);

static int tNoOfThreads = 1;
static int tNoOfAttributes = 25;
static int tNoOfTables = 1;
static int tAttributeSize = 1;
static int tNodeId = 0;
static int tNoOfOperations = 500;
static int tNoOfLoops = 1;
static int tAbortAfter = 0;
static int tParallellism = 1;

static char tableName[MAXTABLES][MAXSTRLEN];
static char attrName[MAXATTR][MAXSTRLEN];

static unsigned int tSleepTime = 0;

static int theStdTableNameFlag = 0;
static int theTableCreateFlag = 0;
static int theScanAbortTestFlag = 0;
static int theNoScanUpdateFlag = 0;
static int theNoScanDeleteFlag = 0;

//flexScanErrorData = new ErrorData;
ErrorData * flexScanErrorData;
NdbError  * anerror;

//static errorData            theErrorData; 
//static unsigned int         tErrorCounter[6000]; 

#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); }; 

static void UpdateArray(int *attrValue)
{
  int tableCount = 0;
  int attrCount = 0;
  int opCount = 0;
  int sizeCount = 0;
  int* pValue = attrValue;

  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    for (attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
      for (opCount = 0; opCount < tNoOfOperations; opCount++) {
	for (sizeCount = 0; sizeCount < tAttributeSize; sizeCount++) {
	  // Update value in array
	  (*pValue)++;
	  //ndbout << "attrValue[" << tableCount*tNoOfAttributes*tNoOfOperations*tAttributeSize +
	  //attrCount*tNoOfOperations*tAttributeSize + opCount*tAttributeSize + sizeCount <<
	  //"] = " << attrValue[tableCount*tNoOfAttributes*tNoOfOperations*tAttributeSize +
	  //attrCount*tNoOfOperations*tAttributeSize + opCount*tAttributeSize + sizeCount] << endl;
	  // Increment pointer
	  pValue++;
	} // sizeCount
      } // for opCount
    } // for attrCount
  } // for tableCount

} // Update

static void Compare(int* attrValue, int* readValue)
{
  int tableCount = 0;
  int attrCount = 0;
  int OpCount = 0;
  int first = 0;

  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    for (attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
      for (OpCount = 0; OpCount < tNoOfOperations; OpCount++) {
	if (memcmp(&(attrValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
			      attrCount*tNoOfOperations + OpCount]),
		   &(readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
			      attrCount*tNoOfOperations + OpCount]), 
		   tAttributeSize) != 0) {
	  // Values mismatch
	  if (first == 0) {
	    //ndbout << "Read and set values differ for:" << endl;
	    first = 1;
	    ndbout << "Mismatch found.";
	  } // if
	  // Comparision of values after scan update is meaningless right now
	  //ndbout << "  table " << tableName[tableCount] << 
	  //" - attr " << attrName[attrCount+1];
	  //for (sizeCount = 0; sizeCount < tAttributeSize; sizeCount++) {
	  //ndbout << ": set " << 
	  //attrValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations*tAttributeSize + 
	  //attrCount*tNoOfOperations*tAttributeSize + 
	  //tNoOfOperations*tAttributeSize + sizeCount] << " read " << 
	  //readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations*tAttributeSize + 
	  //attrCount*tNoOfOperations*tAttributeSize + 
	  //tNoOfOperations*tAttributeSize + sizeCount] << endl;
	  //} // for
	} // if
      } // for OpCount
    } // for attrCount
  } // for tableCount

  // A final pretty-print
  if (first == 1) {
    ndbout << endl;
  } // if
} // Compare

static void printInfo()
{
  ndbout << endl << "FLEXSCAN - Starting normal mode" << endl;
  ndbout << "Perform benchmark of insert, update and delete transactions"<< endl;
  ndbout << "  NdbAPI node with id = " << tNodeId << endl;
  ndbout << "  " << tNoOfThreads << " thread(s) " << endl;
  ndbout << "  " << tNoOfLoops << " iterations " << endl;
  ndbout << "  " << tNoOfTables << " table(s) and " << 1 << " operation(s) per transaction " 
	 << endl;
  ndbout << "  " << tNoOfAttributes << " attributes per table incl. pk" << endl;
  ndbout << "  " << tNoOfOperations << " transaction(s) per thread and round " << endl;
  if (theScanAbortTestFlag == 1) {
    ndbout << "  Scan abort test after " << tAbortAfter << " tuples" << endl;
  } // if
  ndbout << "  " << tParallellism << " parallellism in scans" << endl;
  ndbout << "  " << tAttributeSize << " is the number of 32 bit words per attribute " << 
    endl << endl;
  
} // printInfo

static void tellThreads(ThreadNdb *threadArrayP, StartType what)
{
  int i = 0;

  for (i = 0; i < tNoOfThreads ; i++)
    threadArrayP[i].threadStart = what;
} // tellThreads

static void waitForThreads(ThreadNdb *threadArrayP)
{
  int i = 0;
  int cont = 1;

  while (cont == 1){

    NdbSleep_MilliSleep(10);
    cont = 0;
    
    for (i = 0; i < tNoOfThreads ; i++) {
      if (threadArrayP[i].threadReady == 0) {
//	ndbout << "Main is reporting thread " << i << " not ready" << endl;
	cont = 1;
      } // if
    } // for
  } // while
} // waitForThreads


static void resetThreads(ThreadNdb *threadArrayP)
{
  int i = 0;

  for (i = 0; i < tNoOfThreads ; i++) {
    threadArrayP[i].threadReady = 0;
    threadArrayP[i].threadResult = 0;
    threadArrayP[i].threadStart = stIdle;
    //ndbout << "threadStart[" << i << "]=" << 
    //threadArrayP[i].threadStart << endl;
  } // for
} // resetThreads

static int checkThreadResults(ThreadNdb *threadArrayP, char *action)
{
  int i = 0;
  int retValue = 0;

  for (i = 0; i < tNoOfThreads; i++) {
    if (threadArrayP[i].threadResult != 0) {
      ndbout << "Thread " << i << " reported fatal error " 
	     << threadArrayP[i].threadResult << " during " << action << endl;
      retValue = -1;
      break;
    } // if
  } // for

  return(retValue);
} // checkThreadResults

NDB_COMMAND(flexScan, "flexScan", "flexScan", "flexScan", 65535)
{
  ndb_init();
  ThreadNdb*		pThreads = NULL;
  Ndb*			pMyNdb = NULL;	
  int                   tLoops = 0;
  int                   check = 0;
  int                   returnValue = NDBT_OK;
  int                   every2ndScanDelete = 0; // Switch between scan delete and normal delete

  flexScanErrorData = new ErrorData;

  flexScanErrorData->resetErrorCounters();

  if (readArguments(argc, argv) != 0) {
    ndbout << "Wrong arguments to flexScan" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  } // if

  /* print Setting */
  flexScanErrorData->printSettings(ndbout);

  check = setAttrNames();
  if (check != 0) {
    ndbout << "Couldn't set attribute names" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  } // if
  check = setTableNames();
  if (check != 0) {
    ndbout << "Couldn't set table names" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  } // if

  pMyNdb = new Ndb ("TEST_DB");
  pMyNdb->init();
  tNodeId = pMyNdb->getNodeId();

  printInfo();
                                  
  NdbThread_SetConcurrencyLevel(tNoOfThreads + 2);
  //NdbThread_SetConcurrencyLevel(tNoOfThreads + 8);
  
  pThreads = new ThreadNdb[tNoOfThreads];

  if (pMyNdb->waitUntilReady(10000) != 0) {
    ndbout << "NDB is not ready" << endl << "Benchmark failed"	 << endl;
    returnValue = NDBT_FAILED;
  } // if
  
  else {
    
    if (createTables(pMyNdb) != 0) {
      ndbout << "Could not create tables" << endl;
      returnValue = NDBT_FAILED;
    } // if
    else {
      sleepBeforeStartingTest(tSleepTime);
      
      resetThreads(pThreads);
      // Create threads
      for (int i = 0; i < tNoOfThreads ; i++){  
	pThreads[i].ThreadNo = i;
	// Ignore the case that thread creation may fail
	pThreads[i].threadLife = NdbThread_Create(flexScanThread,
						  (void**)&pThreads[i],
						  327680,
						  "flexScanThread", NDB_THREAD_PRIO_LOW);
	if (pThreads[i].threadLife == NULL) {
	  ndbout << "Could not create thread " << i << endl;
	  returnValue = NDBT_FAILED;
	  // Use the number of threads that were actually created
	  tNoOfThreads = i;
	  break; // break for loop
	} // if
      } // for
      
      waitForThreads(pThreads);
      if (checkThreadResults(pThreads, "init") != 0) {
	returnValue = NDBT_FAILED;
      } // if
      
      if (returnValue == NDBT_OK) {
	ndbout << "All threads started" << endl;
	
	while (FOREVER) {
	  
	  resetThreads(pThreads);
	  
	  if ((tNoOfLoops != 0) && (tNoOfLoops <= tLoops)) {
	    break;
	  } // if
	  
	  // Insert
	  START_TIMER;
	  
	  tellThreads(pThreads, stInsert);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "insert") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("insert", tNoOfOperations*tNoOfThreads, tNoOfTables);
	  
	  resetThreads(pThreads);
	  
	  // Read
	  START_TIMER;
	  
	  tellThreads(pThreads, stRead);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "read") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("read", tNoOfOperations*tNoOfThreads, tNoOfTables);
	  
	  resetThreads(pThreads);
	  
	  // Update
	  START_TIMER;
	  
	  tellThreads(pThreads, stUpdate);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "update") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("update", tNoOfOperations*tNoOfThreads, tNoOfTables);
	  
	  resetThreads(pThreads);
	  
	  // Scan read
	  START_TIMER;
	  
	  tellThreads(pThreads, stScanRead);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "scanread") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("scanread", tNoOfTables*tNoOfThreads, 1);
	  
	  resetThreads(pThreads);
	  
	  // Update
	  START_TIMER;
	  
	  tellThreads(pThreads, stUpdate);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "update") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("update", tNoOfOperations*tNoOfThreads, tNoOfTables);
	  
	  resetThreads(pThreads);
	  
	  // Read
	  START_TIMER;
	  
	  tellThreads(pThreads, stRead);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "read") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("read", tNoOfOperations*tNoOfThreads, tNoOfTables);
	  
	  resetThreads(pThreads);
	  
	  // Only do scan update if told to do so
	  if (theNoScanUpdateFlag == 0) {
	    // Scan update
	    START_TIMER;
	    
	    tellThreads(pThreads, stScanUpdate);
	    waitForThreads(pThreads);
	    
	    STOP_TIMER;
	    if (checkThreadResults(pThreads, "scanupdate") != 0) {
	      returnValue = NDBT_FAILED;
	      break;
	    } // if
	    PRINT_TIMER("scanupdate", tNoOfTables*tNoOfThreads, 1);
	    
	    resetThreads(pThreads);
	    
	    // Read
	    START_TIMER;
	    
	    tellThreads(pThreads, stRead);
	    // tellThreads(pThreads, stScanRead);
	    waitForThreads(pThreads);
	    
	    STOP_TIMER;
	    if (checkThreadResults(pThreads, "read") != 0) {
	      returnValue = NDBT_FAILED;
	      break;
	    } // if
	    PRINT_TIMER("read", tNoOfOperations*tNoOfThreads, tNoOfTables);
	    
	    resetThreads(pThreads);
	  } // if theNoScanUpdateFlag

	  // Shift between delete and scan delete
	  if ((every2ndScanDelete % 2 == 0) || (theNoScanDeleteFlag == 1)){
	    // Delete
	    START_TIMER;
	    tellThreads(pThreads, stDelete);
	    waitForThreads(pThreads);
	    
	    STOP_TIMER;
	    if (checkThreadResults(pThreads, "delete") != 0) {
	      returnValue = NDBT_FAILED;
	      break;
	    } // if
	    PRINT_TIMER("delete", tNoOfOperations*tNoOfThreads, tNoOfTables);
	    resetThreads(pThreads);
	  } // if
	  else {
	    resetThreads(pThreads);	  // Scan delete
	    START_TIMER;
	    tellThreads(pThreads, stScanDelete);
	    waitForThreads(pThreads);
	    
	    STOP_TIMER;
	    if (checkThreadResults(pThreads, "scandelete") != 0) {
	      returnValue = NDBT_FAILED;
	      break;
	    } // if
	    PRINT_TIMER("scandelete", tNoOfTables*tNoOfThreads, 1);
	    
	    resetThreads(pThreads);
	  } // else
	  every2ndScanDelete++;

	  resetThreads(pThreads);	  // Verify delete
	  START_TIMER;
	  tellThreads(pThreads, stVerifyDelete);
	  waitForThreads(pThreads);
	  
	  STOP_TIMER;
	  if (checkThreadResults(pThreads, "verifydelete") != 0) {
	    returnValue = NDBT_FAILED;
	    break;
	  } // if
	  PRINT_TIMER("verifydelete", tNoOfOperations*tNoOfThreads*tNoOfTables, 1);
	  
	  resetThreads(pThreads);
	  
	  ndbout << "--------------------------------------------------" << endl;
	  tLoops++;
	  
	} // while
      } // if
    } // else
  } // else

  // Stop threads in a nice way
  tellThreads(pThreads, stStop);
  waitForThreads(pThreads);

  // Clean up
  delete [] pThreads;
  delete pMyNdb;

  flexScanErrorData->printErrorCounters(ndbout);

  if (returnValue == NDBT_OK) {
    ndbout << endl << "Benchmark completed successfully" << endl;
  } // if
  else {
    ndbout << endl << "Benchmark failed" << endl;
  } // else

  // Exit via NDBT
  return NDBT_ProgramExit(returnValue);;
} // main

void*
flexScanThread(void* ThreadData)
{
  ThreadNdb* pThreadData = (ThreadNdb*)ThreadData;
  unsigned int thread_no = pThreadData->ThreadNo;
  unsigned int thread_base = (thread_no * 2000000) + (tNodeId * 26000);
  int tThreadResult = 0;
  Ndb*			MyNdb = NULL;
  int                   check = 0;
  StartType	      	tType = stLast;
  int*                  pkValue = NULL;
  int*			attrValue = NULL;
  int*			readValue = NULL;
  int                   AllocSize = 0;
  
  AllocSize = tNoOfTables * (tNoOfAttributes-1) * tNoOfOperations * 
    tAttributeSize * sizeof(int);
  attrValue = (int*)malloc(AllocSize);
  readValue = (int*)malloc(AllocSize);
  pkValue = (int*)malloc(tNoOfOperations * sizeof(int));
  if ((attrValue == NULL) || (readValue == NULL) || (pkValue == NULL)) {
    tThreadResult = 98;
    pThreadData->threadStart = stIdle;
  } // if
  
  setAttrValues(attrValue, readValue, thread_base);
  
  MyNdb = new Ndb( "TEST_DB" );
  MyNdb->init();
  if (MyNdb->waitUntilReady(10000) != 0) {
    tThreadResult = 99;
    pThreadData->threadStart = stIdle;
  } // if
  
  // Set primary key value, same for all tables
  for (int c = 0; c < tNoOfOperations; c++) {
    pkValue[c] = (int)(c + thread_base);
  } // for
  
  while (FOREVER) {
    pThreadData->threadResult = tThreadResult;
    pThreadData->threadReady = 1;
    
    while (pThreadData->threadStart == stIdle) {
      NdbSleep_MilliSleep(10);
    } // while
    
    // Check if signal to exit is received
    if (pThreadData->threadStart >= stStop){
      pThreadData->threadReady = 1;
      break;
    } // if
    tType = pThreadData->threadStart;
    pThreadData->threadStart = stIdle;
    
    switch (tType) {
    case stInsert:
      check = insertRows(MyNdb, pkValue, attrValue, tType);
      break;
    case stRead:
      check = readRows(MyNdb, pkValue, readValue);
      Compare(attrValue, readValue);
      break;
    case stUpdate:
      UpdateArray(attrValue);
      check = insertRows(MyNdb, pkValue, attrValue, tType);
      break;
    case stScanRead:
      //check = readRows(MyNdb, pkValue, readValue);
      check = scanReadRows(MyNdb, readValue);
      Compare(attrValue, readValue);
      break;
    case stScanUpdate:
      UpdateArray(attrValue);
      //tType = stUpdate;
      //check = insertRows(MyNdb, pkValue, attrValue, tType);
      check = scanUpdateRows(MyNdb, readValue, attrValue);
      break;
    case stDelete:
      check = deleteRows(MyNdb, pkValue);
      break;
    case stScanDelete:
      check = scanDeleteRows(MyNdb, readValue);
      break;
    case stVerifyDelete:
      check = verifyDeleteRows(MyNdb, pkValue, readValue);
      break;
    default:
      ndbout << "tType is " << tType << endl;
      require(false);
      break;
    } // switch
    
    tThreadResult = check;
    
    if (tThreadResult != 0) {
      // Check if error is fatak or not
    } // if
    else {
      continue;
    } // else
  } // while

  // Clean up
  delete MyNdb;
  if (attrValue != NULL) {
    free(attrValue);
  } //if
  if (readValue != NULL) {
    free(readValue);
  } // if
  if (pkValue != NULL) {
    free(pkValue);
  } // if
  
  return NULL; // thread exits

} // flexScanThread


static int setAttrNames()
{
  int i = 0;
  int  retVal = 0;
  
  for (i = 0; i < MAXATTR ; i++) {
    retVal = BaseString::snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
    if (retVal < 0) {
      return(-1);
    } // if
  } // for

  return(0);
} // setAttrNames


static int setTableNames()
{
  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables with SQL
  int i = 0;
  int retVal = 0;

  for (i = 0; i < MAXTABLES ; i++) {

    if (theStdTableNameFlag == 0) {
      retVal = BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d_%d", i, 
	       (int)(NdbTick_CurrentMillisecond() / 1000));
    } // if 
    else {
      retVal = BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    } // if else

    if (retVal < 0) {
      return(-1);
    } // if
  } // for

  return(0);
} // setTableNames


//	Create Table and Attributes. 	
static int createTables(Ndb* pMyNdb)
{
  
  NdbSchemaCon		*MySchemaTransaction = NULL;
  NdbSchemaOp		*MySchemaOp = NULL;
  int i = 0;
  int j = 0;
  int check = 0;
  
  if (theTableCreateFlag == 0) {
    
    i = 0;
    do {
      i++;
      ndbout << endl << "Creating " << tableName[i - 1] << "..." << endl;

      MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pMyNdb);      
      if( MySchemaTransaction == NULL ) {
	return (-1);
      } // if

      MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
      if( MySchemaOp == NULL ) {
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return (-1);
      } // if

      check = MySchemaOp->createTable(tableName[i - 1]
				      ,8		// Table Size
				      ,TupleKey	        // Key Type
				      ,40);		// Nr of Pages

      if (check == -1) {
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return -1;
      } // if
      
      check = MySchemaOp->createAttribute( (char*)attrName[0], TupleKey, 32, PKSIZE,
					   UnSigned, MMBased, NotNullAttribute );
      if (check == -1) {
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return -1;
      } // if
      
      for (j = 1; j < tNoOfAttributes ; j++) {
	check = MySchemaOp->createAttribute( (char*)attrName[j], NoKey, 32, tAttributeSize,
					     UnSigned, MMBased, NotNullAttribute );
	if (check == -1) {
	  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	  return -1;
	} // if
      } // for
      
      if (MySchemaTransaction->execute() == -1) {
	ndbout <<  MySchemaTransaction->getNdbError().message << endl;
	ndbout << "Probably, " << tableName[i - 1] << " already exist" << endl;
      } // if
      
      NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
    } while (tNoOfTables > i);
  }

  return 0;
} // createTables

static void printUsage()
{
  ndbout << "Usage of flexScan:" << endl;
  ndbout << "-f <path> Location of my.cnf file, default: my.cnf" << endl;
  ndbout << "-t <int>  Number of threads to start, default 1" << endl;
  ndbout << "-o <int>  Number of operations per loop, default 500" << endl;
  ndbout << "-l <int>  Number of loops to run, default 1, 0=infinite" << endl;
  ndbout << "-a <int>  Number of attributes, default 25" << endl;
  ndbout << "-c <int>  Number of tables, default 1" << endl;
  ndbout << "-s <int>  Size of each attribute, default 1" << endl;
  ndbout << "-stdtables        Use standard table names" << endl;
  ndbout << "-no_table_create  Don't create tables in db" << endl;
  ndbout << "-sleep <int>      Sleep a number of seconds before running the test" << endl;
  ndbout << "-p <int>          Parallellism to use 1-32, default:1" << endl;
  ndbout << "-abort <int>      Test scan abort after a number of tuples" << endl;
  ndbout << "-no_scan_update   Don't do scan updates" << endl;
  ndbout << "-no_scan_delete   Don't do scan deletes" << endl;
  ndbout << "-h                Print this text" << endl;
  //  inputErrorArg();
  flexScanErrorData->printCmdLineArgs(ndbout);
}

static int readArguments(int argc, const char** argv)
{
  int i = 1;
  int retValue = 0;
  int printFlag = 0;

  tNoOfThreads = 1;		// Set default Value
  tNoOfTables = 1;		// Default Value

  while (argc > 1) {
    if (strcmp(argv[i], "-t") == 0) {
      if (argv[i + 1] != NULL) {
	tNoOfThreads = atoi(argv[i + 1]);
	if ((tNoOfThreads < 1) || (tNoOfThreads > NDB_MAXTHREADS)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // if
    else if (strcmp(argv[i], "-o") == 0) {
      if (argv[i + 1] != NULL) {
	tNoOfOperations = atoi(argv[i + 1]);
	if (tNoOfOperations < 1) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-a") == 0) {
      if (argv[i + 1] != NULL) {
	tNoOfAttributes = atoi(argv[i + 1]);
	if ((tNoOfAttributes < 2) || (tNoOfAttributes > MAXATTR)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-c") == 0) {
      if (argv[i + 1] != NULL) {
	tNoOfTables = atoi(argv[i+1]);
	if ((tNoOfTables < 1) || (tNoOfTables > MAXTABLES)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-l") == 0) {
      if (argv[i + 1] != NULL) {
	tNoOfLoops = atoi(argv[i+1]);
	if ((tNoOfLoops < 0) || (tNoOfLoops > 100000)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-s") == 0) {
      if (argv[i + 1] != NULL) {
	tAttributeSize = atoi(argv[i+1]);
	if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-no_table_create") == 0) {
      theTableCreateFlag = 1;
      argc++;
      i--;
    } // else if
    else if (strcmp(argv[i], "-stdtables") == 0) {
      theStdTableNameFlag = 1;
      argc++;
      i--;
    } // else if
    else if (strcmp(argv[i], "-sleep") == 0) {
      if (argv[i + 1] != NULL) {
	tSleepTime = atoi(argv[i+1]);
	if ((tSleepTime < 1) || (tSleepTime > 3600)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-abort") == 0) {
      // Test scan abort after a number of tuples
      theScanAbortTestFlag = 1;
      if (argv[i + 1] != NULL) {
	tAbortAfter = atoi(argv[i + 1]);
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-p") == 0) {
      if (argv[i + 1] != NULL) {
	tParallellism = atoi(argv[i + 1]);
	if ((tParallellism < 1) || (tParallellism > 32)) {
	  retValue = -1;
	} // if
      } // if
      else {
	retValue = -1;
      } // else
    } // else if
    else if (strcmp(argv[i], "-h") == 0) {
      printFlag = 1;
      argc++;
      i--;
    } // else if
    else if (strcmp(argv[i], "-no_scan_update") == 0) {
      theNoScanUpdateFlag = 1;
      argc++;
      i--;
    } // else if
    else if (strcmp(argv[i], "-no_scan_delete") == 0) {
      theNoScanDeleteFlag = 1;
      argc++;
      i--;
    } // else if
    else {
      retValue = -1;
    } // else

    argc -= 2;
    i = i + 2;
  }

  if ((retValue != 0) || (printFlag == 1)) {
    printUsage();
  } // if

  return(retValue);
  
} // readArguments

static void sleepBeforeStartingTest(int seconds)
{
  if (seconds > 0) {
    ndbout << "Sleeping(" <<seconds << ")...";
    NdbSleep_SecSleep(seconds);
    ndbout << " done!" << endl;
  } // if
} // sleepBeforeStartingTest

static void setAttrValues(int* attrValue, 
			  int* readValue, 
			  int Offset)
{
  int tableCount = 0;
  int attrCount = 0;
  int OpCount = 0;
  int attrSize = 0;
  int* pAttr = NULL;
  int* pRead = NULL;
  
  // Set attribute values in memory array
  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    for (attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) { 
      for (OpCount = 0; OpCount < tNoOfOperations; OpCount++) {
	pAttr = &(attrValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
			   attrCount*tNoOfOperations + OpCount]);
	pRead = &(readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
			   attrCount*tNoOfOperations + OpCount]);
	for (attrSize = 0; attrSize < tAttributeSize; attrSize++){ 
	  *pAttr = (int)(Offset + tableCount + attrCount + OpCount + attrSize);
	  //ndbout << "attrValue[" << tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
	  //attrCount*tNoOfOperations + OpCount + attrSize << "] = " <<
	  //attrValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations + 
	  //attrCount*tNoOfOperations + OpCount + attrSize] << endl;
	  *pRead = 0;
	  pAttr++;
	  pRead++;
	} // for attrSize
      } // for OpCount
    } // for attrCount
  } // for tableCount
  
} // setAttrValues

static int insertRows(Ndb* pNdb, // NDB object
		      int* pkValue, // Primary key values
		      int* attrValue, // Attribute values
		      StartType tType)
{
  int tResult = 0;
  int check = 0;
  int tableCount = 0;
  int attrCount = 0;
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperations[MAXTABLES] = {NULL};
  int opCount = 0;
  
  for (opCount = 0; opCount < tNoOfOperations; opCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
    } // if
    else {
      for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {	
	
	MyOperations[tableCount] = 
	  MyTransaction->getNdbOperation(tableName[tableCount]);
	if (MyOperations[tableCount] == NULL) {
	  tResult = 2;
	  // Break for tableCount loop
	  break;
	} // if
	
	if (tType == stUpdate) {
	  check = MyOperations[tableCount]->updateTuple();
	} // if
	else if (tType == stInsert) {
	  check = MyOperations[tableCount]->insertTuple();
	} // else if
	else {
	  require(false);
	} // else
	
	if (check == -1) {
	  tResult = 3;
	  break;
	} // if 
	check = MyOperations[tableCount]->equal((char*)attrName[0], 
						(char*)&(pkValue[opCount]));
	if (check == -1) {
	  tResult = 7;
	  break;
	} // if
	
	for (attrCount = 0; attrCount < tNoOfAttributes - 1; attrCount++) {
	  int Index = tableCount * (tNoOfAttributes - 1) * tNoOfOperations * tAttributeSize +
	    attrCount * tNoOfOperations * tAttributeSize + opCount * tAttributeSize;
	  check = MyOperations[tableCount]->
	    setValue((char*)attrName[attrCount + 1],
		     (char*)&(attrValue[Index]));
	  if (check == -1) {
	    tResult = 8;
	    break; // break attrCount loop
	  } // if
	} // for
      } // for tableCount
      
      // Execute transaction with insert one tuple in every table
      check = MyTransaction->execute(Commit);
      if (check == -1) {
	ndbout << MyTransaction->getNdbError().message << endl;

	// Add complete error handling here

              int retCode = flexScanErrorData->handleErrorCommon(MyTransaction->getNdbError());
              if (retCode == 1) {
		if (MyTransaction->getNdbError().code != 626 && MyTransaction->getNdbError().code != 630){
                ndbout_c("execute: %d, %d, %s", opCount, tType, MyTransaction->getNdbError().message);
                ndbout_c("Error code = %d", MyTransaction->getNdbError().code);}
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
		/* What can we do here? */
                ndbout_c("execute: %s", MyTransaction->getNdbError().message);
                 }//if(retCode == 3)

      } // if(check == -1)
      
      pNdb->closeTransaction(MyTransaction);
    } // else
  } // for opCount
  
  return(tResult);
} // insertRows

static int readRows(Ndb* pNdb,
		    int* pkValue,
		    int* readValue)
{
  int tResult = 0;
  int tableCount = 0;
  int attrCount = 0;
  int check = 0;
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperations[MAXTABLES] = {NULL};
  NdbRecAttr* tmp = NULL;
  int Value = 0;
  int Index = 0;
  int opCount = 0;

  for (opCount = 0; opCount < tNoOfOperations; opCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
    } // if
    else {
      for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {	
	
	MyOperations[tableCount] = 
	  MyTransaction->getNdbOperation(tableName[tableCount]);
	if (MyOperations[tableCount] == NULL) {
	  tResult = 2;
	  // Break for tableCount loop
	  break;
	} // if
	
	check = MyOperations[tableCount]->readTuple();
	if (check == -1) {
	  tResult = 3;
	  break;
	} // if
	
	check = MyOperations[tableCount]->
	  equal((char*)attrName[0], (char*)&(pkValue[opCount]));
	if (check == -1) {
	  tResult = 7;
	  break;
	} // if
	
	for (int attrCount = 0; attrCount < tNoOfAttributes - 1; attrCount++) {
	  Index = tableCount * (tNoOfAttributes - 1) * tNoOfOperations * tAttributeSize +
	    attrCount * tNoOfOperations * tAttributeSize + opCount * tAttributeSize;
	  tmp = MyOperations[tableCount]->
	    getValue((char*)attrName[attrCount + 1], (char*)&(readValue[Index]));
	  
	  if (tmp == NULL) {
	    tResult = 9;
	    break;
	} // if
	} // for attrCount
      } // for tableCount
      // Execute transaction reading one tuple in every table
      check = MyTransaction->execute(Commit);
      if (check == -1) {
	ndbout << MyTransaction->getNdbError().message << endl;

	// Add complete error handling here

              int retCode = flexScanErrorData->handleErrorCommon(MyTransaction->getNdbError());
              if (retCode == 1) {
		if (MyTransaction->getNdbError().code != 626 && MyTransaction->getNdbError().code != 630){
                ndbout_c("execute: %d, %s", opCount, MyTransaction ->getNdbError().message );
                ndbout_c("Error code = %d", MyTransaction->getNdbError().code );}
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
		/* What can we do here? */
                ndbout_c("execute: %s", MyTransaction ->getNdbError().message );
                 }//if(retCode == 3)

      } // if
      
      pNdb->closeTransaction(MyTransaction);
    } // else
  } // for opCount

  return(tResult);
} // readRows

static int scanReadRows(Ndb* pNdb, int* readValue)
{
  int tResult = 0;
  int tableCount = 0;
  int attrCount = 0;
  int check = 0;
  int countAbort = 0; // Counts loops until scan abort if requested
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperation = NULL;
  NdbRecAttr* tmp = NULL;
  
  
  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
      break;
    } // if
    MyOperation = MyTransaction->getNdbOperation(tableName[tableCount]);
    if (MyOperation == NULL) {
      tResult = 2;
      break;
    } // if
    
    check = MyOperation->openScanRead(tParallellism);
    if (check == -1) {
      tResult = 10;
      break;
    } // if
    
    for (int attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
      // Get all attributes
      tmp = MyOperation->
	getValue((char*)attrName[attrCount+1],
		 (char*)&(readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations*tAttributeSize +
				   attrCount*tNoOfOperations*tAttributeSize]));
      if (tmp == NULL) {
	tResult = 9;
	break;
      } // if
    } // for attrCount

    check = MyTransaction->executeScan();
    if (check == -1) {
      tResult = 12;
      break;
    } // if

    check = MyTransaction->nextScanResult();
    while (check == 0) {
      // Check if scan abort is requested
      if (theScanAbortTestFlag == 1) {
	if (countAbort == tAbortAfter) {
	  MyTransaction->stopScan();
	  ndbout << "scanread aborted on request after " << countAbort*tParallellism << 
	    " tuples" << endl;
	  break; // break while loop
	} // if
	countAbort++;
      } // if
      check = MyTransaction->nextScanResult();
    } // while

    pNdb->closeTransaction(MyTransaction);
  } // for tableCount
    
    return(tResult);
} // scanReadRows

static int scanUpdateRows(Ndb* pNdb, 
			  int* readValue,
			  int* attrValue)
{
  int tResult = 0;
  int tableCount = 0;
  int attrCount = 0;
  int check = 0;
  int opCount = 0;
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperation = NULL;
  NdbConnection* MyTakeOverTrans = NULL;
  NdbOperation* MyTakeOverOp = NULL;
  NdbRecAttr* tTmp = NULL;

  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
      break; // break tableCount for loop
    } // if
    MyOperation = MyTransaction->getNdbOperation(tableName[tableCount]);
    if (MyOperation == NULL) {
      tResult = 2;
      break;
    } // if

    check = MyOperation->openScanExclusive(tParallellism);
    if (check == -1) {
      tResult = 11;
      break;
    } // if

    // Fetch all attributes
    for (int attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
      tTmp = MyOperation->
	getValue((char*)attrName[attrCount+1],
		 (char*)&(readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations*tAttributeSize +
				   attrCount*tNoOfOperations*tAttributeSize]));
      if (tTmp == NULL) {
	tResult = 9;
	break; // break for loop
      } // if
    } // for
    if (tResult != 0) {
      break; // break while loop also
    } // if

    check = MyTransaction->executeScan();
    if (check == -1) {
      tResult = 12;
      break;
    } // if
    check = MyTransaction->nextScanResult();
    opCount = 0;
    while (check == 0) {
      MyTakeOverTrans = pNdb->startTransaction();
      MyTakeOverOp = MyOperation->takeOverForUpdate(MyTakeOverTrans);
      for (attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
	check = MyTakeOverOp->setValue((char*)attrName[attrCount+1],
				       (char*)&(attrValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations*tAttributeSize +
							 attrCount*tNoOfOperations*tAttributeSize + opCount*tAttributeSize]));
      } // for
      
      check = MyTakeOverTrans->execute(Commit);
      if (check == 0) {
	check = MyTransaction->nextScanResult();
	opCount++;
      } // if
      else {
	tResult = 95;
	
	/* MyTransaction, MyTakeOverTrans, Which one? */
	
	// Any further error handling?
	int retCode = flexScanErrorData->handleErrorCommon(MyTakeOverTrans->getNdbError());
	if (retCode == 1) {
	  if (MyTakeOverTrans->getNdbError().code != 626 && MyTakeOverTrans->getNdbError().code != 630){
	    ndbout_c("execute: %s", MyTakeOverTrans->getNdbError().message);
	    ndbout_c("Error code = %d", MyTakeOverTrans->getNdbError().code);}
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
	  /* What can we do here? */
	  ndbout_c("execute: %s", MyTakeOverTrans->getNdbError().message);
	}//if(retCode == 3)
	
      } // else
      pNdb->closeTransaction(MyTakeOverTrans);
    } // while
    
    pNdb->closeTransaction(MyTransaction);
  } // for

  return(tResult);
} // scanUpdateRows

static int scanDeleteRows(Ndb* pNdb, int* readValue)
{
  int tResult = 0;
  int tableCount = 0;
  int attrCount = 0;
  int check = 0;
  NdbRecAttr* tTmp = NULL;
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperation = NULL;
  NdbConnection* MyTakeOverTrans = NULL;
  NdbOperation* MyTakeOverOp = NULL;

  for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
      break; // break tableCount for loop
    } // if

    MyOperation = MyTransaction->getNdbOperation(tableName[tableCount]);
    if (MyOperation == NULL) {
      tResult = 2;
      break;
    } // if

    check = MyOperation->openScanExclusive(tParallellism);
    if (check == -1) {
      tResult = 11;
      break;
    } // if

    for (int attrCount = 0; attrCount < tNoOfAttributes-1; attrCount++) {
      tTmp = MyOperation->
	getValue((char*)attrName[attrCount+1],
		 (char*)&(readValue[tableCount*(tNoOfAttributes-1)*tNoOfOperations +
				   attrCount*tNoOfOperations]));
      if (tTmp == NULL) {
	tResult = 9;
	break;
      } // if
    } // for

    check = MyTransaction->executeScan();
    if (check == -1) {
      tResult = 12;
      break;
    } // if
    check = MyTransaction->nextScanResult();
    while (check == 0) {
      MyTakeOverTrans = pNdb->startTransaction();
      MyTakeOverOp = MyOperation->takeOverForDelete(MyTakeOverTrans);
      check = MyTakeOverOp->deleteTuple();

      check = MyTakeOverTrans->execute(Commit);

      //Error handling here

              int retCode =flexScanErrorData->handleErrorCommon(MyTakeOverTrans->getNdbError());
              if (retCode == 1) {
		if (MyTakeOverTrans->getNdbError().code != 626 && MyTakeOverTrans->getNdbError().code != 630){
                ndbout_c("execute: %s", MyTakeOverTrans->getNdbError().message );
                ndbout_c("Error code = %d", MyTakeOverTrans->getNdbError().code );}
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
		/* What can we do here? */
                ndbout_c("execute: %s", MyTakeOverTrans->getNdbError().message );
                 }//if(retCode == 3) End of error handling

      pNdb->closeTransaction(MyTakeOverTrans);
      check = MyTransaction->nextScanResult();
    } // while
    pNdb->closeTransaction(MyTransaction);
  } // for tableCount
  return(tResult);
} // scanDeleteRows

static int deleteRows(Ndb* pNdb,
		      int* pkValue)
{
  int tResult = 0;
  NdbConnection* MyTransaction = NULL;
  int tableCount = 0;
  int opCount = 0;
  int check = 0;
  NdbOperation* MyOperations[MAXTABLES] = {NULL};

  for (opCount = 0; opCount < tNoOfOperations; opCount++) {
    MyTransaction = pNdb->startTransaction();
    if (MyTransaction == NULL) {
      tResult = 1;
    } // if
    else {
      for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {	
	
	MyOperations[tableCount] = 
	  MyTransaction->getNdbOperation(tableName[tableCount]);
	if (MyOperations[tableCount] == NULL) {
	  tResult = 2;
	  // Break for tableCount loop
	  break;
	} // if
	
	check = MyOperations[tableCount]->deleteTuple();
	if (check == -1) {
	  tResult = 3;
	  break;
	} // if
	
	check = MyOperations[tableCount]->
	  equal((char*)attrName[0], (char*)&(pkValue[opCount]));
	if (check == -1) {
	  tResult = 7;
	  break;
	} // if
	
      } // for tableCount
      
      // Execute transaction deleting one tuple in every table
      check = MyTransaction->execute(Commit);
      if (check == -1) {
	ndbout << MyTransaction->getNdbError().message << endl;
	// Add complete error handling here

              int retCode = flexScanErrorData->handleErrorCommon(MyTransaction->getNdbError());
              if (retCode == 1) {
		if (MyTransaction->getNdbError().code != 626 && MyTransaction->getNdbError().code != 630){
                ndbout_c("execute: %d, %s", opCount, MyTransaction->getNdbError().message );
                ndbout_c("Error code = %d", MyTransaction->getNdbError().code );}
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
		/* What can we do here? */
                ndbout_c("execute: %s", MyTransaction->getNdbError().message );
                 }//if(retCode == 3)

      } // if
      
      pNdb->closeTransaction(MyTransaction);
    } // else
  } // for opCount

  return(tResult);

} // deleteRows

////////////////////////////////////////
//
// Name: verifyDeleteRows
//
// Purpose: Verifies that all tables are empty by reading every tuple
//          No deletions made here
//
// Returns: 'Standard' error codes
//
/////////////////////////////////////
static int verifyDeleteRows(Ndb* pNdb,
			    int* pkValue,
			    int* readValue)
{
  int tResult = 0;
  int tableCount = 0;
  int attrCount = 0;
  int check = 0;
  NdbConnection* MyTransaction = NULL;
  NdbOperation* MyOperations = NULL;
  NdbRecAttr* tmp = NULL;
  int Value = 0;
  int Index = 0;
  int opCount = 0;

  for (opCount = 0; opCount < tNoOfOperations; opCount++) {
    for (tableCount = 0; tableCount < tNoOfTables; tableCount++) {	
      MyTransaction = pNdb->startTransaction();
      if (MyTransaction == NULL) {
	tResult = 1;
      } // if
      else {
	
	MyOperations = 
	  MyTransaction->getNdbOperation(tableName[tableCount]);
	if (MyOperations == NULL) {
	  tResult = 2;
	  // Break for tableCount loop
	  break;
	} // if
	
	check = MyOperations->readTuple();
	if (check == -1) {
	  tResult = 3;
	  break;
	} // if
	
	check = MyOperations->
	  equal((char*)attrName[0], (char*)&(pkValue[opCount]));
	if (check == -1) {
	  tResult = 7;
	  break;
	} // if
	
	for (int attrCount = 0; attrCount < tNoOfAttributes - 1; attrCount++) {
	  Index = tableCount * (tNoOfAttributes - 1) * tNoOfOperations * tAttributeSize +
	    attrCount * tNoOfOperations * tAttributeSize + opCount * tAttributeSize;
	  tmp = MyOperations->
	    getValue((char*)attrName[attrCount + 1], (char*)&(readValue[Index]));
	  
	  if (tmp == NULL) {
	    tResult = 9;
	    break;
	  } // if
	} // for attrCount
	// Execute transaction reading one tuple in every table
	check = MyTransaction->execute(Commit);
	if ((check == -1) && (MyTransaction->getNdbError().code == 626)){
	  // This is expected because everything should be deleted 
	} // if
	else if (check == 0) {
	  // We have found a tuple that should have been deleted
	  ndbout << "tuple " << tableName[tableCount] << ":" <<
	    opCount << " was never deleted" << endl;
	  tResult = 97;
	} // else if
	else {
	  // Unexpected error
	  ndbout << "Unexpected error during delete" << endl;
	  require(false);
	} // else

	pNdb->closeTransaction(MyTransaction);

      } // else
    } // for tableCount
  } // for opCount

  return(tResult);
} // verifyDeleteRows
