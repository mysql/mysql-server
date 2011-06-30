/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
       FLEXHAMMER
       Hammer ndb with read, insert, update and delete transactions. 

       Arguments:
        -t Number of threads to start, default 1
        -o Number of operations per hammering-round, default 500
	-l Number of loops to run, default 1, 0=infinite
        -a Number of attributes, default 25
        -c Number of tables, default 1
	-s Size of each attribute, default 1
	-simple Use simple read to read from database
        -dirty Use dirty read to read from database
	-write Use writeTuple to write to db
	-r Number of records to Hammer
        -no_table_create Don't create tables in db
        -regulate To be able to regulate the load flexHammer produces.
        -stdtables Use standard table names
	-sleep Sleep a number of seconds before running the test, this 
	       can be used so that another flexProgram have tome to create tables

       Returns:
        0 - Test passed
       -1 - Test failed
        1 - Invalid arguments

Revision history:
  1.7  020208 epesson: Adapted to use NDBT
  1.10 020222 epesson: Finalised handling of thread results
  1.11 020222 epesson: Bug in checking results during delete fixed

 * *************************************************** */

#include <ndb_global.h>
#include <NdbApi.hpp>

#include <NdbMain.h>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <NdbTimer.hpp>
#include <NdbTick.h>
#include <NdbTest.hpp>
#include <NDBT_Error.hpp>
#include <NdbSchemaCon.hpp>

ErrorData * flexHammerErrorData;


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
#define MAXATTRSIZE 100
// Max number of retries if something fails
#define MaxNoOfAttemptsC 10 

enum StartType {
  stIdle,
  stHammer,
  stStop,
  stLast};

enum MyOpType {
  otInsert,
  otRead,
  otDelete,
  otUpdate,
  otLast};

struct ThreadNdb {
  int threadNo;
  NdbThread* threadLife;
  int threadReady;
  StartType threadStart;
  int threadResult;};

extern "C" void* flexHammerThread(void*);
static int setAttrNames(void);
static int setTableNames(void);
static int readArguments(int, const char**);
static int createTables(Ndb*);
static int dropTables(Ndb*);
static void sleepBeforeStartingTest(int seconds);
static int checkThreadResults(ThreadNdb *threadArrayP, const char* phase);

//enum OperationType {
//  otInsert,
//  otRead,
//  otUpdate,
//  otDelete,
//  otVerifyDelete,
//  otLast };

enum ReadyType {
	stReady, 
	stRunning
} ;
static int			tNoOfThreads;
static int			tNoOfAttributes;
static int			tNoOfTables;
static int			tNoOfBackups;
static int			tAttributeSize;
static int			tNoOfOperations;
static int			tNoOfRecords;
static int			tNoOfLoops;
static char			tableName[MAXTABLES][MAXSTRLEN];
static char			attrName[MAXATTR][MAXSTRLEN];
static int			theSimpleFlag = 0;
static int			theWriteFlag = 0;
static int			theDirtyFlag = 0;
static int			theTableCreateFlag = 0;
static int			theStandardTableNameFlag = 0;
static unsigned int tSleepTime = 0;

#define START_TIMER { NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define PRINT_TIMER(text, trans, opertrans) timer.printTransactionStatistics(text, trans, opertrans); }; 


// Initialise thread data
void 
resetThreads(ThreadNdb *threadArrayP) {

  for (int i = 0; i < tNoOfThreads ; i++)
    {
      threadArrayP[i].threadReady = 0;
      threadArrayP[i].threadResult = 0;
      threadArrayP[i].threadStart = stIdle;
    }
} // resetThreads

void 
waitForThreads(ThreadNdb *threadArrayP)
{
  int cont = 1;

  while (cont) {
    NdbSleep_MilliSleep(100);
    cont = 0;
    for (int i = 0; i < tNoOfThreads ; i++) {
      if (threadArrayP[i].threadReady == 0) {
	cont = 1;
      } // if
    } // for
  } // while
} // waitForThreads

void 
tellThreads(ThreadNdb* threadArrayP, const StartType what)
{
  for (int i = 0; i < tNoOfThreads ; i++) 
    {
    threadArrayP[i].threadStart = what;
    } // for
} // tellThreads

static Ndb_cluster_connection *g_cluster_connection= 0;
 
NDB_COMMAND(flexHammer, "flexHammer", "flexHammer", "flexHammer", 65535)
//main(int argc, const char** argv)
{
  ndb_init();
  ThreadNdb* pThreads = NULL; // Pointer to thread data array
  Ndb* pMyNdb = NULL;	      // Pointer to Ndb object
  int tLoops = 0;
  int returnValue = 0;
  int check = 0;
  
  flexHammerErrorData = new ErrorData;

  flexHammerErrorData->resetErrorCounters();

  if (readArguments(argc, argv) != 0) {
    ndbout << "Wrong arguments to flexHammer" << endl;
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  } // if

  /* print Setting */
  flexHammerErrorData->printSettings(ndbout);

  check = setAttrNames();
  if (check == -1) {
    ndbout << "Couldn't set attribute names" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  } // if
  check = setTableNames();
  if (check == -1) {
    ndbout << "Couldn't set table names" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  } // if

  // Create thread data array
  pThreads = new ThreadNdb[tNoOfThreads];
  // NdbThread_SetConcurrencyLevel(tNoOfThreads + 2);

  // Create and init Ndb object
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  g_cluster_connection= &con;
  pMyNdb = new Ndb(g_cluster_connection, "TEST_DB");
  pMyNdb->init();

  // Wait for Ndb to become ready
  if (pMyNdb->waitUntilReady(10000) != 0) {
    ndbout << "NDB is not ready" << endl << "Benchmark failed" << endl;
    returnValue = NDBT_FAILED;
  }

  else {
    check = createTables(pMyNdb);
    if (check != 0) {
      returnValue = NDBT_FAILED;
    } // if
    else {
      sleepBeforeStartingTest(tSleepTime);
      
      // Create threads.                                           *
      resetThreads(pThreads);
      for (int i = 0; i < tNoOfThreads ; i++) {  
	pThreads[i].threadNo = i;
	pThreads[i].threadLife = NdbThread_Create(flexHammerThread,
						  (void**)&pThreads[i],
						  65535,
						  "flexHammerThread",
                                                  NDB_THREAD_PRIO_LOW);
      } // for
      
      // And wait until they are ready
      waitForThreads(pThreads);
      if (checkThreadResults(pThreads, "init") != 0) {
        returnValue = NDBT_FAILED;
      } // if
      
      
      if (returnValue == NDBT_OK) {
	ndbout << endl <<  "All threads started" << endl << endl;
	
	for(;;) {
	  
	  // Check if it's time to exit program
	  if((tNoOfLoops != 0) && (tNoOfLoops <= tLoops))
	    break;
	  
	  // Tell all threads to start hammer
	  ndbout << "Hammering..." << endl;
	  
	  resetThreads(pThreads);
	  
	  START_TIMER;
	  tellThreads(pThreads, stHammer);
	  
	  waitForThreads(pThreads);
	  ndbout << "Threads ready to continue..." << endl;
	  STOP_TIMER;
	  
	  // Check here if anything went wrong
	  if (checkThreadResults(pThreads, "hammer") != 0) {
	    ndbout << "Thread(s) failed." << endl;
	    returnValue = NDBT_FAILED;
	  } // if
	  
	  PRINT_TIMER("hammer", tNoOfOperations*tNoOfThreads, tNoOfTables*6);
	  
	  ndbout << endl;
	  
	  tLoops++;
	  
	} // for
      } // if

      // Signaling threads to stop 
      resetThreads(pThreads);
      tellThreads(pThreads, stStop);
      
      // Wait for threads to stop
      waitForThreads(pThreads);
      
      ndbout << "----------------------------------------------" << endl << endl;
      ndbout << "Benchmark completed" << endl;
    } // else
  } // else
  // Clean up 

  flexHammerErrorData->printErrorCounters(ndbout);

  // Kill them all! 
  void* tmp;
  for(int i = 0; i < tNoOfThreads; i++){
    NdbThread_WaitFor(pThreads[i].threadLife, &tmp);
    NdbThread_Destroy(&pThreads[i].threadLife);
  }

  dropTables(pMyNdb);

  delete flexHammerErrorData;
  delete [] pThreads;
  delete pMyNdb;

  // Exit via NDBT
  return NDBT_ProgramExit(returnValue);
  
} //main

extern "C"
void*
flexHammerThread(void* pArg)
{
  ThreadNdb* pThreadData = (ThreadNdb*)pArg;
  //unsigned int threadNo = pThreadData->threadNo;
  Ndb* pMyNdb = NULL ;
  NdbConnection *pMyTransaction = NULL ;
  //  NdbOperation*	pMyOperation[MAXTABLES] = {NULL};
  NdbOperation*	pMyOperation[MAXTABLES];
  int check = 0;
  int loop_count_ops = 0;
  int loop_count_tables = 0;
  int loop_count_attributes = 0;
  int count_round = 0;
  int count = 0;
  int count_tables = 0;
  int count_attributes = 0;
  int i = 0;
  int tThreadResult = 0;
  MyOpType tMyOpType = otLast;
  int pkValue = 0;
  int readValue[MAXATTR][MAXATTRSIZE]; bzero(readValue, sizeof(readValue));
  int attrValue[MAXATTRSIZE];
  NdbRecAttr* tTmp = NULL;
  int tNoOfAttempts = 0;
 
  for (i = 0; i < MAXATTRSIZE; i++)
    attrValue[i] = 0; 
  // Ndb object for each thread
  pMyNdb = new Ndb(g_cluster_connection, "TEST_DB" );
  pMyNdb->init();
  if (pMyNdb->waitUntilReady(10000) != 0) {
    // Error, NDB is not ready
    tThreadResult = 99;
    // Go to idle directly
    pThreadData->threadStart = stIdle;
  } // if
  
  for(;;) {
    pThreadData->threadResult = tThreadResult;
    pThreadData->threadReady = 1; // Signalling ready to main
    
    // If Idle just wait to be stopped from main
    while (pThreadData->threadStart == stIdle) {
      NdbSleep_MilliSleep(100);   
    } // while
    
    // Check if signal to exit is received
    if (pThreadData->threadStart == stStop) {
      pThreadData->threadReady = 1;
      // break out of eternal loop
      break;
    } // if
    
    // Set to Idle to prepare for possible error break
    pThreadData->threadStart = stIdle;
    
    // Prepare transaction
    loop_count_ops = tNoOfOperations;
    loop_count_tables = tNoOfTables;
    loop_count_attributes = tNoOfAttributes;
    
    for (count=0 ; count < loop_count_ops ; count++) {
      
      //pkValue = (int)(count + thread_base);
      // This limits the number of records used in this test
      pkValue = count % tNoOfRecords; 
      
      for (count_round = 0; count_round < 5; ) {
	switch (count_round) {
	case 0:       // Insert
	  tMyOpType = otInsert;
	  // Increase attrValues
	  for (i=0; i < MAXATTRSIZE; i ++) {
	    attrValue[i]++;
	  }
	  break;
	case 1: 
	case 3:       // Read and verify
	  tMyOpType = otRead;
	  break;
	case 2:       // Update
	  // Increase attrValues
	  for(i=0; i < MAXATTRSIZE; i ++) {
	    attrValue[i]++;
	  }
	  tMyOpType = otUpdate;
	  break;
	case 4:       // Delete
	  tMyOpType = otDelete;
	  break;
	default:
	  assert(false);
	  break;
	} // switch
	    
	// Get transaction object
	pMyTransaction = pMyNdb->startTransaction();
	if (pMyTransaction == NULL) {
	  // Fatal error
	  tThreadResult = 1;
	  // break out of for count_round loop waiting to be stopped by main
	  break;
	} // if

	for (count_tables = 0; count_tables < loop_count_tables; 
	     count_tables++) {
	  pMyOperation[count_tables] = 
	    pMyTransaction->getNdbOperation(tableName[count_tables]);	
	  if (pMyOperation[count_tables] == NULL) {
	    //Fatal error
	    tThreadResult = 2;
	    // break out of inner for count_tables loop
	    break;
	  } // if
		
	  switch (tMyOpType) {
	  case otInsert:			// Insert case
	    if (theWriteFlag == 1 && theDirtyFlag == 1) {
	      check = pMyOperation[count_tables]->dirtyWrite();
	    } else if (theWriteFlag == 1) {
	      check = pMyOperation[count_tables]->writeTuple();
	    } else {
	      check = pMyOperation[count_tables]->insertTuple();
	    } // if else
	    break;
	  case otRead:			// Read Case
	    if (theSimpleFlag == 1) {
	      check = pMyOperation[count_tables]->simpleRead();
	    } else if (theDirtyFlag == 1) {
	      check = pMyOperation[count_tables]->dirtyRead();
	    } else {
	      check = pMyOperation[count_tables]->readTuple();
	    } // if else
	    break;
	  case otUpdate:			// Update Case
	    if (theWriteFlag == 1 && theDirtyFlag == 1) {
	      check = pMyOperation[count_tables]->dirtyWrite();
	    } else if (theWriteFlag == 1) {
	      check = pMyOperation[count_tables]->writeTuple();
	    } else if (theDirtyFlag == 1) {
	      check = pMyOperation[count_tables]->dirtyUpdate();
	    } else {
	      check = pMyOperation[count_tables]->updateTuple();
	    } // if else
	    break;
	  case otDelete:			// Delete Case
	    check = pMyOperation[count_tables]->deleteTuple();
	    break;
	  default:
	    assert(false);
	    break;
	  } // switch
	  if (check == -1) {
	    // Fatal error
	    tThreadResult = 3;
	    // break out of inner for count_tables loop
	    break;
	  } // if

	  check = pMyOperation[count_tables]->equal( (char*)attrName[0], 
						    (char*)&pkValue );

	  if (check == -1) {
	    // Fatal error
	    tThreadResult = 4;
	    ndbout << "pMyOperation equal failed" << endl;
	    // break out of inner for count_tables loop
	    break;
	  } // if
	  
	  check = -1;
	  tTmp = NULL;
	  switch (tMyOpType) {
	  case otInsert:			// Insert case
	  case otUpdate:			// Update Case
	    for (count_attributes = 1; count_attributes < loop_count_attributes;
		 count_attributes++) {
	      check = 
		pMyOperation[count_tables]->setValue((char*)attrName[count_attributes], (char*)&attrValue[0]);
	    } // for
	    break;
	  case otRead:			// Read Case
	    for (count_attributes = 1; count_attributes < loop_count_attributes; 
		 count_attributes++) {
	      tTmp = pMyOperation[count_tables]->
		getValue( (char*)attrName[count_attributes], 
			  (char*)&readValue[count_attributes][0] );
	    } // for
	    break;
	  case otDelete:			// Delete Case
	    break;
	  default:
	    assert(false);
	    break;
	  } // switch
	  if (check == -1 && tTmp == NULL && tMyOpType != otDelete) {
	    // Fatal error
	    tThreadResult = 5;
	    break;
	  } // if
	} // for count_tables
	    
	// Only execute if everything is OK
	if (tThreadResult != 0) {
	  // Close transaction (below)
	  // and continue with next count_round
	  count_round++;
	  tNoOfAttempts = 0;
	} // if
	else {
	  check = pMyTransaction->execute(Commit);
	  if (check == -1 ) {
	    const NdbError & err = pMyTransaction->getNdbError();

	// Add complete error handling here

              int retCode = flexHammerErrorData->handleErrorCommon(pMyTransaction->getNdbError());
              if (retCode == 1) {
		//if (strcmp(pMyTransaction->getNdbError().message, "Tuple did not exist") != 0 && strcmp(pMyTransaction->getNdbError().message,"Tuple already existed when attempting to insert") != 0) ndbout_c("execute: %s", pMyTransaction->getNdbError().message);

		if (pMyTransaction->getNdbError().code != 626 && pMyTransaction->getNdbError().code != 630){
     ndbout_c("Error code = %d", pMyTransaction->getNdbError().code);
     ndbout_c("execute: %s", pMyTransaction->getNdbError().message);}

              } else if (retCode == 2) {
                ndbout << "4115 should not happen in flexHammer" << endl;
              } else if (retCode == 3) {
// --------------------------------------------------------------------
// We are not certain if the transaction was successful or not.
// We must reexecute but might very well find that the transaction
// actually was updated. Updates and Reads are no problem here. Inserts
// will not cause a problem if error code 630 arrives. Deletes will
// not cause a problem if 626 arrives.
// --------------------------------------------------------------------
		/* What can we do here? */
		ndbout_c("execute: %s", pMyTransaction->getNdbError().message);
                 }//if(retCode == 3)
	// End of adding complete error handling

	    switch( err.classification) {
	    case NdbError::ConstraintViolation:	// Tuple already existed
	      count_round++;
	      tNoOfAttempts = 0;
	      break;
	    case NdbError::TimeoutExpired:
	    case NdbError::NodeRecoveryError:
	    case NdbError::TemporaryResourceError:
	    case NdbError::OverloadError:
	      if (tNoOfAttempts <= MaxNoOfAttemptsC) {
		// Retry
		tNoOfAttempts++;
	      } else {
		// Too many retries, continue with next
		count_round++;
		tNoOfAttempts = 0;
	      } // else if
	      break;
	      // Fatal, just continue
	    default:
	      count_round++;
	      tNoOfAttempts = 0;
	      break;
	    } // switch
	  } // if
	  else {
	    // Execute commit was OK
	    // This is verifying read values
	    //switch (tMyOpType) {
	    //case otRead:  // Read case
	    //for (j = 0; j < tNoOfAttributes; j++) {
	    //for(i = 1; i < tAttributeSize; i++) {
	    //if ( readValue[j][i] != attrValue[i]) {
	    //ndbout << "pkValue = " << pkValue << endl;
	    //ndbout << "readValue != attrValue" << endl;
	    //ndbout << readValue[j][i] << " != " << attrValue[i] << endl;
	    //} // if
	    //  } // for
	    //} // for
	    //break;
	    //} // switch
	    count_round++;
	    tNoOfAttempts = 0;
	  } // else if
	} // else if
	pMyNdb->closeTransaction(pMyTransaction);
      } // for count_round
    } // for count
  } // for (;;)

  // Clean up
  delete pMyNdb; 
  pMyNdb = NULL;

  flexHammerErrorData->resetErrorCounters();

  return  NULL; // thread exits
  
} // flexHammerThread


int
readArguments (int argc, const char** argv)
{
  int i = 1;
  
  tNoOfThreads = 5;		// Default Value
  tNoOfOperations = 500;	// Default Value
  tNoOfRecords = 1;             // Default Value
  tNoOfLoops = 1;	        // Default Value
  tNoOfAttributes = 25;		// Default Value
  tNoOfTables = 1;		// Default Value
  tNoOfBackups = 0;		// Default Value
  tAttributeSize = 1;		// Default Value
  theTableCreateFlag = 0;
  
  while (argc > 1) {
    if (strcmp(argv[i], "-t") == 0) {
      tNoOfThreads = atoi(argv[i+1]);
      if ((tNoOfThreads < 1) || (tNoOfThreads > NDB_MAXTHREADS))
	return(1);
    }
    else if (strcmp(argv[i], "-o") == 0) {
      tNoOfOperations = atoi(argv[i+1]);
      if (tNoOfOperations < 1)
	return(1);
    }
    else if (strcmp(argv[i], "-r") == 0) {
      tNoOfRecords = atoi(argv[i+1]);
      if (tNoOfRecords < 1)
	return(1);
    }
    else if (strcmp(argv[i], "-a") == 0) {
      tNoOfAttributes = atoi(argv[i+1]);
      if ((tNoOfAttributes < 2) || (tNoOfAttributes > MAXATTR))
	return(1);
    }
    else if (strcmp(argv[i], "-c") == 0) {
      tNoOfTables = atoi(argv[i+1]);
      if ((tNoOfTables < 1) || (tNoOfTables > MAXTABLES))
	return(1);
    }
    else if (strcmp(argv[i], "-l") == 0) {
      tNoOfLoops = atoi(argv[i+1]);
      if ((tNoOfLoops < 0) || (tNoOfLoops > 100000))
	return(1);
    }
    else if (strcmp(argv[i], "-s") == 0) {
      tAttributeSize = atoi(argv[i+1]);
      if ((tAttributeSize < 1) || (tAttributeSize > MAXATTRSIZE))
	return(1);
    }
    else if (strcmp(argv[i], "-sleep") == 0) {
      tSleepTime = atoi(argv[i+1]);
      if ((tSleepTime < 1) || (tSleepTime > 3600))
	exit(-1);
    }
    else if (strcmp(argv[i], "-simple") == 0) {
      theSimpleFlag = 1;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-write") == 0) {
      theWriteFlag = 1;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-dirty") == 0) {
      theDirtyFlag = 1;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-no_table_create") == 0) {
      theTableCreateFlag = 1;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-stdtables") == 0) {
      theStandardTableNameFlag = 1;
      argc++;
      i--;
    } // if
    else {
      return(1);
    }
    
    argc -= 2;
    i = i + 2;
  } // while
  
  ndbout << endl << "FLEXHAMMER - Starting normal mode" << endl;
  ndbout << "Hammer ndb with read, insert, update and delete transactions"<< endl << endl;
  
  ndbout << "  " << tNoOfThreads << " thread(s) " << endl;
  ndbout << "  " << tNoOfLoops << " iterations " << endl;
  ndbout << "  " << tNoOfTables << " table(s) and " << 1 << " operation(s) per transaction " << endl;
  ndbout << "  " << tNoOfRecords << " records to hammer(limit this with the -r option)" << endl;
  ndbout << "  " << tNoOfAttributes << " attributes per table " << endl;
  ndbout << "  " << tNoOfOperations << " transaction(s) per thread and round " << endl;
  ndbout << "  " << tAttributeSize << " is the number of 32 bit words per attribute " << endl << endl;
  return 0;
} // readArguments


void sleepBeforeStartingTest(int seconds)
{
  if (seconds > 0) {
    ndbout << "Sleeping(" << seconds << ")...";
    NdbSleep_SecSleep(seconds);
    ndbout << " done!" << endl;
  } // if
} // sleepBeforeStartingTest

static int
createTables(Ndb* pMyNdb)
{
  int i = 0;
  int j = 0;
  int check = 0;
  NdbSchemaCon *MySchemaTransaction = NULL;
  NdbSchemaOp *MySchemaOp = NULL;

  //	Create Table and Attributes. 	                          
  if (theTableCreateFlag == 0) {

    for (i = 0; i < tNoOfTables; i++) {

      ndbout << "Creating " << tableName[i] << "...";
      // Check if table exists already
      const void * p = pMyNdb->getDictionary()->getTable(tableName[i]);
      if (p != 0) {
	ndbout << " already exists." << endl;
	// Continue with next table at once
	continue;
      } // if
      ndbout << endl;
      
      MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pMyNdb);
      if (MySchemaTransaction == NULL) {
	return(-1);
      } // if
      
      MySchemaOp = MySchemaTransaction->getNdbSchemaOp();	
      if (MySchemaOp == NULL) {
	// Clean up opened schema transaction
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return(-1);
      } // if
      
      // Create tables, rest of parameters are default right now
      check = MySchemaOp->createTable(tableName[i],
				      8,		// Table Size
				      TupleKey,	// Key Type
				      40);		// Nr of Pages

      if (check == -1) { 
	// Clean up opened schema transaction
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return(-1);
      } // if
      
      // Primary key
      //ndbout << "  pk " << (char*)&attrName[0] << "..." << endl;
      check = MySchemaOp->createAttribute( (char*)attrName[0], TupleKey, 32,
					   1, UnSigned, MMBased,
					   NotNullAttribute );
      if (check == -1) { 
	// Clean up opened schema transaction
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return(-1);
      } // if

      // Rest of attributes
      for (j = 1; j < tNoOfAttributes ; j++) {
	//ndbout << "    " << (char*)attrName[j] << "..." << endl;
	check = MySchemaOp->createAttribute( (char*)attrName[j], NoKey, 32, 
					     tAttributeSize, UnSigned, MMBased, 
					     NotNullAttribute );
	if (check == -1) {
	  // Clean up opened schema transaction
	  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	  return(-1);
	} // if
      } // for
  
      // Execute creation
      check = MySchemaTransaction->execute();
      if (check == -1) {
	// Clean up opened schema transaction
	NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
	return(-1);
      } // if
      
      NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
    } // for
  } // if

  return(0);

} // createTables 

static int
dropTables(Ndb* pMyNdb)
{
  int i = 0;

  if (theTableCreateFlag == 0)
  {
    for (i = 0; i < tNoOfTables; i++)
    {
      ndbout << "Dropping " << tableName[i] << "...";
      pMyNdb->getDictionary()->dropTable(tableName[i]);
      ndbout << "done" << endl;
    }
  }

  return(0);

} // createTables 


static int setAttrNames()
{
  int i = 0;
  int retVal = 0;

  for (i = 0; i < MAXATTR ; i++) {
    retVal = BaseString::snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
    if (retVal < 0) {
      // Error in conversion
      return(-1);
    } // if
  } // for

  return (0);
} // setAttrNames

static int setTableNames()
{
  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables wits SQL
  int i = 0;
  int retVal = 0;

  for (i = 0; i < MAXTABLES ; i++) {
    if (theStandardTableNameFlag == 0) {
      retVal = BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d_%u", i, 
                                    (Uint32)(NdbTick_CurrentMillisecond()/1000));
    } // if 
    else {
      retVal = BaseString::snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
    } // else
    if (retVal < 0) {
      // Error in conversion
      return(-1);
    } // if
  } // for

  return(0);
} // setTableNames

static int checkThreadResults(ThreadNdb *threadArrayP, const char* phase)
{
  int i = 0;

  for (i = 0; i < tNoOfThreads; i++) {
    if (threadArrayP[i].threadResult != 0) {
      ndbout << "Thread " << i << " reported fatal error " 
	     << threadArrayP[i].threadResult << " during " << phase << endl;
      return(-1);
    } // if
  } // for

  return(0);
}

