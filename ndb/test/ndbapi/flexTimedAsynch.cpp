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
       FLEXTIMEDASYNCH
       Perform benchmark of insert, update and delete transactions.

       Arguments:
        -t Number of threads to start, i.e., number of parallel loops, default 1
        -p Number of transactions in a batch, default 32
        -o Number of batches per loop, default 200
        -i Time between batch starts, default 0
	-l Number of loops to run, default 1, 0=infinite
        -a Number of attributes, default 25
        -c Number of operations per transaction
	-s Size of each attribute in 32 bit word, default 1 (Primary Key is always of size 1,
	                                      independent of this value)
	-simple           Use simple read to read from database
        -dirty            Use dirty read to read from database
	-write            Use writeTuple in insert and update
        -n                Use standard table names
        -no_table_create  Don't create tables in db
	-temp             Use temporary tables, no writing to disk.
	
       Returns:
        0 - Test passed
       -1 - Test failed
        1 - Invalid arguments

 * *************************************************** */

#include "NdbApi.hpp"

#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <NdbTimer.hpp>
#include <string.h>
#include <NdbMain.h>
#include <NdbTest.hpp>

#include <NDBT_Error.hpp>

#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 64
#define MAXTHREADS 256
#define MAXATTRSIZE 1000
#define PKSIZE 1

enum StartType { stIdle, 
	        stInsert,
		stRead,
		stUpdate,
		stDelete, 
                stStop } ;

ErrorData * flexTimedAsynchErrorData;    

struct ThreadNdb
{
  int NoOfOps;
  int ThreadNo;  
  unsigned int threadBase;
  unsigned int transactionCompleted;
};

extern "C" void* threadLoop(void*);
void setAttrNames(void);
void setTableNames(void);
void readArguments(int argc, const char** argv);
void createAttributeSpace();
void createTables(Ndb*);
void defineOperation(NdbConnection* aTransObject, StartType aType, unsigned int key, int *);
void execute(StartType aType);
void executeThread(StartType aType, Ndb* aNdbObject, ThreadNdb* threadInfo);
void executeCallback(int result, NdbConnection* NdbObject, void* aObject);

/* epaulsa > *************************************************************/
bool error_handler(const NdbError &) ; //replaces 'goto' things
static int failed = 0 ; // lame global variable that keeps track of failed transactions
                        // incremented in executeCallback() and reset in main()
/************************************************************* < epaulsa */

static NdbThread* threadLife[MAXTHREADS];
static int tNodeId;
static int ThreadReady[MAXTHREADS];
static StartType ThreadStart[MAXTHREADS];
static char tableName[MAXTABLES][MAXSTRLEN+1];
static char attrName[MAXATTR][MAXSTRLEN+1];
static int *getAttrValueTable;

// Program Parameters
static int tNoOfLoops = 1;
static int tAttributeSize = 1;
static unsigned int tNoOfThreads = 1;
static unsigned int tNoOfTransInBatch = 32;
static unsigned int tNoOfAttributes = 25;
static unsigned int tNoOfBatchesInLoop = 200;
static unsigned int tNoOfOpsPerTrans = 1;
static unsigned int tTimeBetweenBatches = 0;

//Program Flags
static int theTestFlag = 0;
static int theTempFlag = 1;
static int theSimpleFlag = 0;
static int theDirtyFlag = 0;
static int theWriteFlag = 0;
static int theStdTableNameFlag = 0;
static int theTableCreateFlag = 0;

#define START_REAL_TIME  NdbTimer timer; timer.doStart();
#define STOP_REAL_TIME timer.doStop(); 

#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); }; 

void 
resetThreads(){

  for (int i = 0; i < tNoOfThreads ; i++) {
    ThreadReady[i] = 0;
    ThreadStart[i] = stIdle;
  }
}

void 
waitForThreads(void)
{
  int cont;
  do {
    cont = 0;
    NdbSleep_MilliSleep(20);
    for (int i = 0; i < tNoOfThreads ; i++) {
      if (ThreadReady[i] == 0) {
        cont = 1;
      }
    }
  } while (cont == 1);
}

void 
tellThreads(StartType what)
{
  for (int i = 0; i < tNoOfThreads ; i++) 
    ThreadStart[i] = what;
}

void createAttributeSpace(){
  getAttrValueTable = new int[tAttributeSize*
			     tNoOfThreads * 
			     tNoOfAttributes ];

}

void deleteAttributeSpace(){
  delete [] getAttrValueTable;
}

NDB_COMMAND(flexTimedAsynch, "flexTimedAsynch", "flexTimedAsynch [-tpoilcas]", "flexTimedAsynch", 65535)
{
  ndb_init();
  ThreadNdb		tabThread[MAXTHREADS];
  int                   tLoops=0;
  int                   returnValue;
  //NdbOut flexTimedAsynchNdbOut;

  flexTimedAsynchErrorData = new ErrorData;
  flexTimedAsynchErrorData->resetErrorCounters();

  Ndb* pNdb;
  pNdb = new Ndb( "TEST_DB" );
  pNdb->init();

  readArguments(argc, argv);
  
  createAttributeSpace();

  ndbout << endl << "FLEXTIMEDASYNCH - Starting normal mode" << endl;
  ndbout << "Perform benchmark of insert, update and delete transactions" << endl << endl;

  if(theTempFlag == 0)
    ndbout << "  " << "Using temporary tables. " << endl;

  // -t, tNoOfThreads
  ndbout << "  " << tNoOfThreads << " number of concurrent threads " << endl;
  // -c, tNoOfOpsPerTrans
  ndbout << "  " << tNoOfOpsPerTrans << " operations per transaction " << endl;
  // -p, tNoOfTransInBatch
  ndbout << "  " << tNoOfTransInBatch << " number of transactions in a batch per thread " << endl;
  // -o, tNoOfBatchesInLoop
  ndbout << "  " << tNoOfBatchesInLoop << " number of batches per loop " << endl;
  // -i, tTimeBetweenBatches
  ndbout << "  " << tTimeBetweenBatches << " milli seconds at least between batch starts " << endl;
  // -l, tNoOfLoops
  ndbout << "  " << tNoOfLoops << " loops " << endl;
  // -a, tNoOfAttributes
  ndbout << "  " << tNoOfAttributes << " attributes per table " << endl; 
  // -s, tAttributeSize
  ndbout << "  " << tAttributeSize << " is the number of 32 bit words per attribute "  << endl << endl;

  NdbThread_SetConcurrencyLevel(2 + tNoOfThreads);

  /* print Setting */
  flexTimedAsynchErrorData->printSettings(ndbout);

  setAttrNames();
  setTableNames();

  ndbout << "Waiting for ndb to become ready..." <<endl;
  if (pNdb->waitUntilReady() == 0) {
    tNodeId = pNdb->getNodeId();
    ndbout << "  NdbAPI node with id = " << tNodeId << endl;
    createTables(pNdb);

    /****************************************************************
     *	Create NDB objects.                                   *
     ****************************************************************/
    resetThreads();
    for (int i = 0; i < tNoOfThreads ; i++) {
      tabThread[i].ThreadNo = i;

      threadLife[i] = NdbThread_Create(threadLoop,
				       (void**)&tabThread[i],
				       32768,
				       "flexTimedAsynchThread",
                                       NDB_THREAD_PRIO_LOW);
    }
    ndbout << endl <<  "All NDB objects and table created" << endl << endl;
    int noOfTransacts = tNoOfTransInBatch*tNoOfBatchesInLoop*tNoOfThreads;
    
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
      PRINT_TIMER("insert", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
	ndbout << failed << " of the transactions returned errors!, moving on now..."<<endl ;
      }
      
      /****************************************************************
       * Perform read.                                                *
       ****************************************************************/
      
      failed = 0 ; 

      START_TIMER;
      execute(stRead);
      STOP_TIMER;
      PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);
     
      if (0 < failed) {
	ndbout << failed << " of the transactions returned errors!, moving on now..."<<endl ;
      }
 
      

      /****************************************************************
       * Perform update.                                              *
       ***************************************************************/
      
      failed = 0 ; 
	  
      START_TIMER;
      execute(stUpdate);
      STOP_TIMER;
      PRINT_TIMER("update", noOfTransacts, tNoOfOpsPerTrans) ;

      if (0 < failed) {
	ndbout << failed << " of the transactions returned errors!, moving on now..."<<endl ;
      }

      /****************************************************************
       * Perform read after update.                                             
       ****************************************************************/
    
      failed = 0 ; 
	  
      START_TIMER;
      execute(stRead);
      STOP_TIMER;
      PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
	ndbout << failed << " of the transactions returned errors!, moving on now..."<<endl ;
      }
 

      /****************************************************************
       * Perform delete.                                              *
       ****************************************************************/
      
      failed = 0; 
	  
      START_TIMER;
      execute(stDelete);
      STOP_TIMER;
      PRINT_TIMER("delete", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
	ndbout << failed << " of the transactions returned errors!, moving on now..."<<endl ;
      }
     	  
      tLoops++;
      ndbout << "--------------------------------------------------" << endl;
   
      if(tNoOfLoops != 0){
	if(tNoOfLoops <= tLoops)
	  break ;
      }
    }

    ndbout << endl << "Benchmark completed!" << endl;
    returnValue = NDBT_OK;

    execute(stStop);
    void * tmp;
    for(int i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(threadLife[i], &tmp);
      NdbThread_Destroy(&threadLife[i]);
    }
  } else {
    ndbout << "NDB is not ready" << endl;
    ndbout << "Benchmark failed!" << endl;
    returnValue = NDBT_FAILED;
  }

  deleteAttributeSpace();
  delete pNdb;

  //printing errorCounters
  flexTimedAsynchErrorData->printErrorCounters(ndbout);

  return NDBT_ProgramExit(returnValue);
}//main()

////////////////////////////////////////

void execute(StartType aType)
{
  resetThreads();
  tellThreads(aType);
  waitForThreads();
}

void*
threadLoop(void* ThreadData)
{
  // Do work until signaled to stop.

  Ndb* localNdb;
  StartType tType;
  ThreadNdb* threadInfo = (ThreadNdb*)ThreadData;
  int threadNo = threadInfo->ThreadNo;
  localNdb = new Ndb("TEST_DB");
  localNdb->init(512);
  localNdb->waitUntilReady();
  threadInfo->threadBase = (threadNo * 2000000) + (tNodeId * 260000000);

  for (;;) {
    while (ThreadStart[threadNo] == stIdle) {
      NdbSleep_MilliSleep(10);
    }

    // Check if signal to exit is received
    if (ThreadStart[threadNo] == stStop) {
      break;
    }

    tType = ThreadStart[threadNo];
    ThreadStart[threadNo] = stIdle;
    executeThread(tType, localNdb, threadInfo);
    ThreadReady[threadNo] = 1;
  }

  delete localNdb;
  ThreadReady[threadNo] = 1;
  NdbThread_Exit(0);

  return NULL;
}

void executeThread(StartType aType, Ndb* aNdbObject, ThreadNdb* threadInfo)
{
  // Do all batch job in loop with start specified delay 
  int i, j, k;
  NdbConnection* tConArray[1024];
  unsigned int tBase;
  unsigned int tBase2;
  int threadId = threadInfo->ThreadNo;
  int *getValueRowAddress = NULL;

  NdbTimer timer; 
  timer.doStart();
  
  for (i = 0; i < tNoOfBatchesInLoop; i++) {
    //tBase = threadBase + (i * tNoOfTransInBatch * tNoOfOpsPerTrans);
    tBase = threadInfo->threadBase + (i * tNoOfTransInBatch * tNoOfOpsPerTrans);
    //tCompleted = 0;
    threadInfo->transactionCompleted = 0;
  
    for (j = 0; j < tNoOfTransInBatch; j++) {
      tBase2 = tBase + (j * tNoOfOpsPerTrans);
      tConArray[j] = aNdbObject->startTransaction();
      if ( tConArray[j] == NULL && !error_handler(aNdbObject->getNdbError())) {
	ndbout << endl << "Unable to recover! Quiting now" << endl ;
	exit (-1) ;
	return ;
      }

      for (k = 0; k < tNoOfOpsPerTrans; k++) {
	//-------------------------------------------------------
	// Define the operation, but do not execute it yet.
	//-------------------------------------------------------
	if(aType == stRead){
	  getValueRowAddress = getAttrValueTable + 
	    threadId * tNoOfAttributes * tAttributeSize;
	}
	defineOperation(tConArray[j], aType, (tBase2 + k), getValueRowAddress);
      }

      tConArray[j]->executeAsynchPrepare(Commit, &executeCallback, threadInfo);
    }

    //-------------------------------------------------------
    // Now we have defined a set of transactions (= batch), it is now time
    // to execute all of them.
    //-------------------------------------------------------
    aNdbObject->sendPollNdb(3000, 0, 0);

    //while (tCompleted < tNoOfTransInBatch) {
    while (threadInfo->transactionCompleted < tNoOfTransInBatch) {
      aNdbObject->pollNdb(3000, 0);
      ndbout << "threadInfo->transactionCompleted = " << 
	threadInfo->transactionCompleted << endl;
    }

    for (j = 0 ; j < tNoOfTransInBatch ; j++) {
      aNdbObject->closeTransaction(tConArray[j]);
    }

    // Control the elapsed time since the last batch start.
    // Wait at least tTimeBetweenBatches milli seconds.
    timer.doStop();
    while(timer.elapsedTime() < tTimeBetweenBatches){
      NdbSleep_MilliSleep(1);
      timer.doStop();
    }
    // Ready to start new batch
    timer.doStart();
  }
  return;
}

void
executeCallback(int result, NdbConnection* NdbObject, void* aObject)
{
  //tCompleted++;
  ThreadNdb *threadInfo = (ThreadNdb *)aObject;
  threadInfo->transactionCompleted++;

  if (result == -1) {

              // Add complete error handling here

              int retCode = flexTimedAsynchErrorData->handleErrorCommon(NdbObject->getNdbError());
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

	      //    ndbout << "Error occured in poll:" << NdbObject->getNdbError() << 
	      //      " ErrorCode = " << NdbObject->getNdbError() << endl;
    ndbout << "executeCallback threadInfo->transactionCompleted = " << 
      threadInfo->transactionCompleted << endl;  
    failed++ ;
    return;
  }
 return;
}

void
defineOperation(NdbConnection* localNdbConnection, 
		StartType aType, 
		unsigned int threadBase,
		int *pRow )
{
  NdbOperation*  localNdbOperation;
  unsigned int   loopCountAttributes = tNoOfAttributes;
  unsigned int   countAttributes;
  int            attrValue[MAXATTRSIZE];

  //-------------------------------------------------------
  // Set-up the attribute values for this operation.
  //-------------------------------------------------------
  for (int k = 0; k < loopCountAttributes; k++) {
    *(int *)&attrValue[k] = (int)threadBase;
  }
  localNdbOperation = localNdbConnection->getNdbOperation(tableName[0]);	
  if (localNdbOperation == NULL) {
    error_handler(localNdbOperation->getNdbError()) ;
  }

  switch (aType) {
  case stInsert: {   // Insert case
    if (theWriteFlag == 1 && theDirtyFlag == 1) {
      localNdbOperation->dirtyWrite();
    } else if (theWriteFlag == 1) {
      localNdbOperation->writeTuple();
    } else {
      localNdbOperation->insertTuple();
    }
    break;
  }
  case stRead: {     // Read Case
    if (theSimpleFlag == 1) {
      localNdbOperation->simpleRead();
    } else if (theDirtyFlag == 1) {
      localNdbOperation->dirtyRead();
    } else {
      localNdbOperation->readTuple();
    }
     break;
  }
  case stUpdate: {    // Update Case
    if (theWriteFlag == 1 && theDirtyFlag == 1) {
      localNdbOperation->dirtyWrite();
    } else if (theWriteFlag == 1) {
      localNdbOperation->writeTuple();
    } else if (theDirtyFlag == 1) {
      localNdbOperation->dirtyUpdate();
    } else {
      localNdbOperation->updateTuple();
    }
    break;
  }
  case stDelete: {   // Delete Case
    localNdbOperation->deleteTuple();
    break;
  }
  default: {
    error_handler(localNdbOperation->getNdbError());
  }
  }

  localNdbOperation->equal((char*)attrName[0],(char*)&attrValue[0]);

  switch (aType) {
  case stInsert:      // Insert case
  case stUpdate:      // Update Case
    {
      for (countAttributes = 1; countAttributes < loopCountAttributes; countAttributes++) {
        localNdbOperation->setValue( (char*)attrName[countAttributes],(char*)&attrValue[0]);
      }
      break;
    }
  case stRead: {      // Read Case
    for (countAttributes = 1; countAttributes < loopCountAttributes; countAttributes++) {
      //localNdbOperation->getValue((char*)attrName[countAttributes],(char*)&attrValue[0]);
      localNdbOperation->getValue((char*)attrName[countAttributes], 
				  (char *) (pRow + countAttributes*tAttributeSize));
    }
    break;
  }
  case stDelete: {    // Delete Case
    break;
  }
  default: {
    error_handler(localNdbOperation->getNdbError());
  }
  }
  return;
}

void readArguments(int argc, const char** argv)
{
  int i = 1;
  while (argc > 1)
    {
      if (strcmp(argv[i], "-t") == 0)
	{
	  tNoOfThreads = atoi(argv[i+1]);
	  //      if ((tNoOfThreads < 1) || (tNoOfThreads > MAXTHREADS))
	  if ((tNoOfThreads < 1) || (tNoOfThreads > MAXTHREADS))
	    exit(-1);
	}
      else if (strcmp(argv[i], "-i") == 0)
	{
	  tTimeBetweenBatches = atoi(argv[i+1]);
	  if (tTimeBetweenBatches < 0)
	    exit(-1);
	}
      else if (strcmp(argv[i], "-p") == 0)
	{
	  tNoOfTransInBatch = atoi(argv[i+1]);
	  //if ((tNoOfTransInBatch < 1) || (tNoOfTransInBatch > MAXTHREADS))
	  if ((tNoOfTransInBatch < 1) || (tNoOfTransInBatch > 10000))
	    exit(-1);
	}
      else if (strcmp(argv[i], "-c") == 0)
	{
	  tNoOfOpsPerTrans = atoi(argv[i+1]);
	  if (tNoOfOpsPerTrans < 1)
	    exit(-1);
	}
      else if (strcmp(argv[i], "-o") == 0)
	{
	  tNoOfBatchesInLoop = atoi(argv[i+1]);
	  if (tNoOfBatchesInLoop < 1)
	    exit(-1);
	}
      else if (strcmp(argv[i], "-a") == 0)
	{
	  tNoOfAttributes = atoi(argv[i+1]);
	  if ((tNoOfAttributes < 2) || (tNoOfAttributes > MAXATTR))
	    exit(-1);
	}
      else if (strcmp(argv[i], "-n") == 0)
	{
	  theStdTableNameFlag = 1;
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-l") == 0)
	{
	  tNoOfLoops = atoi(argv[i+1]);
	  if ((tNoOfLoops < 0) || (tNoOfLoops > 100000))
	    exit(-1);
	}
      else if (strcmp(argv[i], "-s") == 0)
	{
	  tAttributeSize = atoi(argv[i+1]);
	  if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE))
	    exit(-1);
	}
      else if (strcmp(argv[i], "-simple") == 0)
	{
	  theSimpleFlag = 1;
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-write") == 0)
	{
	  theWriteFlag = 1;
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-dirty") == 0)
	{
	  theDirtyFlag = 1;
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-test") == 0)
	{
	  theTestFlag = 1;
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-temp") == 0)
	{
	  theTempFlag = 0;  // 0 if temporary tables.
	  argc++;
	  i--;
	}
      else if (strcmp(argv[i], "-no_table_create") == 0)
	{
	  theTableCreateFlag = 1;
	  argc++;
	  i--;
	}
      else
	{
	  ndbout << "Arguments: " << endl;
	  ndbout << "-t Number of threads to start, i.e., number of parallel loops, default 1 " << endl;
	  ndbout << "-p Number of transactions in a batch, default 32 " << endl;
	  ndbout << "-o Number of batches per loop, default 200 " << endl;
	  ndbout << "-i Minimum time between batch starts in milli seconds, default 0 " << endl;
	  ndbout << "-l Number of loops to run, default 1, 0=infinite " << endl;
	  ndbout << "-a Number of attributes, default 25 " << endl;
	  ndbout << "-c Number of operations per transaction, default 1 " << endl;
	  ndbout << "-s Size of each attribute in 32 bit word, default 1" 
	    "(Primary Key is always of size 1, independent of this value) " << endl;
	  ndbout << "-simple           Use simple read to read from database " << endl;
	  ndbout << "-dirty            Use dirty read to read from database " << endl;
	  ndbout << "-write            Use writeTuple in insert and update " << endl;
	  ndbout << "-n                Use standard table names " << endl;
	  ndbout << "-no_table_create  Don't create tables in db " << endl;
	  ndbout << "-temp             Use temporary tables, no writing to disk. " << endl;
	  exit(-1);
	}

      argc -= 2;
      i = i + 2;
    }
}

void setAttrNames()
{
  int i;

  for (i = 0; i < MAXATTR ; i++)
    {
      sprintf(attrName[i], "COL%d", i);
    }
}


void setTableNames()
{
  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables with SQL
  int i;
  for (i = 0; i < MAXTABLES ; i++)
    {
      if (theStdTableNameFlag==1)
	{
	  sprintf(tableName[i], "TAB%d_%d", tNoOfAttributes, 
		  NdbTick_CurrentMillisecond()/1000);
	} else {
	  sprintf(tableName[i], "TAB%d_%d", tNoOfAttributes, tAttributeSize*4);
	}
    }
}

void createTables(Ndb* pMyNdb)
{

  NdbSchemaCon		*MySchemaTransaction;
  NdbSchemaOp		*MySchemaOp;
  int                   check;

  if (theTableCreateFlag == 0)
    {
      for(int i=0; i < 1 ;i++)
	{
	  ndbout << "Creating " << tableName[i] << "..." << endl;
	  MySchemaTransaction = pMyNdb->startSchemaTransaction();
	  
	  if( MySchemaTransaction == 
	      NULL && (!error_handler(MySchemaTransaction->getNdbError())))
	    exit(-1) ;/*goto error_handler; <epaulsa*/
	  
	  MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
	  if( MySchemaOp == NULL
	      && (!error_handler(MySchemaTransaction->getNdbError())))
	    exit(-1) ;

	  check = MySchemaOp->createTable( tableName[i],
					   8,		// Table Size
					   TupleKey,	// Key Type
					   40,		// Nr of Pages
					   All,         // FragmentType
					   6,
					   78,
					   80,
					   1,           // MemoryType
					   theTempFlag  // 0 if temporary tables else 1
					   );

	  if ( check == -1 && (!error_handler(MySchemaTransaction->getNdbError())))
	    exit(-1) ; /* epaulsa > goto error_handler; < epaulsa */

	
	  check = MySchemaOp->createAttribute( (char*)attrName[0],
					       TupleKey,
					       32,
					       PKSIZE,
					       UnSigned,
					       MMBased,
					       NotNullAttribute );
	
	  if ( check == -1 &&(!error_handler(MySchemaTransaction->getNdbError()))) 
	    exit(-1) ; /* epaulsa > goto error_handler; < epaulsa */

	  for (int j = 1; j < tNoOfAttributes ; j++)
	    {
	      check = MySchemaOp->createAttribute( (char*)attrName[j],
						   NoKey,
						   32,
						   tAttributeSize,
						   UnSigned,
						   MMBased,
						   NotNullAttribute );
	      if ( check == -1
		   && (!error_handler(MySchemaTransaction->getNdbError())))
		exit(-1) ; /* epaulsa > goto error_handler; < epaulsa */
	    }
  
	  if ( MySchemaTransaction->execute() == -1 
	       &&(!error_handler(MySchemaTransaction->getNdbError())))
	    exit(-1) ; /* epaulsa > goto error_handler; < epaulsa */
	  
	  pMyNdb->closeSchemaTransaction(MySchemaTransaction);
	}
    }

  return;
}

bool error_handler(const NdbError & err) {
	ndbout << err << endl ;
	if ( 4008==err.code || 721==err.code || 266==err.code ){
		ndbout << endl << "Attempting to recover and continue now..." << endl ;
		return true ; // return true to retry
	}
	return false ; // return false to abort
}


//*******************************************************************************************





