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
       NODEREC
       Perform benchmark of insert, update and delete transactions

       Arguments:
        -t Number of threads to start, default 1
        -o Number of loops per thread, default 100000
	

 * *************************************************** */

#include <ndb_global.h>

#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <NdbMain.h>
#include <NdbTimer.hpp>
#include <NdbTick.h>
#include <random.h>

#define MAX_TIMERS 4 
#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 64
#define MAXTHREADS 256
#define MAXATTRSIZE 8000
#define START_TIMER NdbTimer timer; timer.doStart();
#define STOP_TIMER timer.doStop();
#define START_TIMER_TOP NdbTimer timer_top; timer_top.doStart();
#define STOP_TIMER_TOP timer_top.doStop();

void* ThreadExec(void*);
struct ThreadNdb
{
  int NoOfOps;
  int ThreadNo;
  Ndb* NdbRef;
};

static NdbThread* threadLife[MAXTHREADS];
static unsigned int tNoOfThreads;
static unsigned int tNoOfOpsPerExecute;
static unsigned int tNoOfRecords;
static unsigned int tNoOfOperations;
static int ThreadReady[MAXTHREADS];
static int ThreadStart[MAXTHREADS];

NDB_COMMAND(benchronja, "benchronja", "benchronja", "benchronja", 65535){
  ndb_init();

  ThreadNdb		tabThread[MAXTHREADS];
  int			i = 0 ;
  int			cont = 0 ;
  Ndb*			pMyNdb = NULL ; //( "TEST_DB" );	
  int           tmp = 0 ;
  int			nTest = 0 ;
  char inp[100] ;

  tNoOfThreads = 1;			// Default Value
  tNoOfOpsPerExecute = 1;	// Default Value
  tNoOfOperations = 100000;	// Default Value
  tNoOfRecords = 500 ;		// Default Value <epaulsa: changed from original 500,000 to match 'initronja's' default  
  i = 1;
  while (argc > 1)
  {
    if (strcmp(argv[i], "-t") == 0){
      tNoOfThreads = atoi(argv[i+1]);
      if ((tNoOfThreads < 1) || (tNoOfThreads > MAXTHREADS)) goto error_input;
    }else if (strcmp(argv[i], "-o") == 0){
      tNoOfOperations = atoi(argv[i+1]);
      if (tNoOfOperations < 1) goto error_input;
    }else if (strcmp(argv[i], "-r") == 0){
      tNoOfRecords = atoi(argv[i+1]);
      if ((tNoOfRecords < 1) || (tNoOfRecords > 1000000000)) goto error_input;
	}else if (strcmp(argv[i], "-p") == 0){
		nTest = atoi(argv[i+1]) ;
		if (0 > nTest || 18 < nTest) goto error_input ;
    }else if (strcmp(argv[i], "-c") == 0){
      tNoOfOpsPerExecute = atoi(argv[i+1]);
      if ((tNoOfOpsPerExecute < 1) || (tNoOfOpsPerExecute > 1024)) goto error_input;
    }else{
      goto error_input;
    }
    argc -= 2;
    i = i + 2;
  }

  ndbout << "Initialisation started. " << endl;
  pMyNdb = new Ndb("TEST_DB") ;
  pMyNdb->init();
  ndbout << "Initialisation completed. " << endl;

  ndbout << endl << "Execute Ronja Benchmark" << endl;
  ndbout << "  NdbAPI node with id = " << pMyNdb->getNodeId() << endl;
  ndbout << "  " << tNoOfThreads << " thread(s) " << endl;
  ndbout << "  " << tNoOfOperations << " transaction(s) per thread and round " << endl;

  if (pMyNdb->waitUntilReady(120) != 0) {
    ndbout << "Benchmark failed - NDB is not ready" << endl;
	delete pMyNdb ;
    return NDBT_ProgramExit(NDBT_FAILED);
  }//if

  NdbThread_SetConcurrencyLevel(2 + tNoOfThreads);

  for (i = 0; i < tNoOfThreads ; i++) {
    ThreadReady[i] = 0;
    ThreadStart[i] = 0;
  }//for

  for (i = 0; i < tNoOfThreads ; i++) {
    tabThread[i].ThreadNo = i;
    tabThread[i].NdbRef = NULL;
    tabThread[i].NoOfOps = tNoOfOperations;
    threadLife[i] = NdbThread_Create(ThreadExec,
                                       (void**)&tabThread[i],
                                       32768,
                                       "RonjaThread",
                                       NDB_THREAD_PRIO_LOW);
  }//for
  
  cont = 1;
  while (cont) {
	NdbSleep_MilliSleep(10);
    cont = 0;
    for (i = 0; i < tNoOfThreads ; i++)
      if (!ThreadReady[i]) cont = 1;
  }//while

  ndbout << "All threads started" << endl;
  
  if(!nTest){

	  for (;;){

		  inp[0] = 0;
		  ndbout << endl << "What to do next:" << endl;
		  ndbout << "1 \t=> Perform lookups in short table" << endl;
		  ndbout << "2 \t=> Perform lookups in long table" << endl;
		  ndbout << "3 \t=> Perform updates in short table" << endl;
		  ndbout << "4 \t=> Perform updates in long table" << endl;
		  ndbout << "5 \t=> Perform 50% lookups/50% updates in short table" << endl;
		  ndbout << "6 \t=> Perform 50% lookups/50% updates in long table" << endl;
		  ndbout << "7 \t=> Perform 80% lookups/20% updates in short table" << endl;
		  ndbout << "8 \t=> Perform 80% lookups/20% updates in long table" << endl;
		  ndbout << "9 \t=> Perform 25% lookups short/25% lookups long/25% updates short/25% updates long" << endl;
		  ndbout << "10\t=> Test bug with replicated interpreted updates, short table" << endl;
		  ndbout << "11\t=> Test interpreter functions, short table" << endl;
		  ndbout << "12\t=> Test bug with replicated interpreted updates, long table" << endl;
		  ndbout << "13\t=> Test interpreter functions, long table" << endl;
		  ndbout << "14\t=> Perform lookups in short table, no guess of TC" << endl;
		  ndbout << "15\t=> Perform lookups in long table, no guess of TC" << endl;
		  ndbout << "16\t=> Perform updates in short table, no guess of TC" << endl;
		  ndbout << "17\t=> Perform updates in long table, no guess of TC" << endl;
		  ndbout << "18\t=> Multi record updates of transactions" << endl;
		  ndbout << "All other responses will exit" << endl;
		  ndbout << "_____________________________" << endl << endl ;
		  
		  int inp_i = 0;
		  do {
		    inp[inp_i] = (char) fgetc(stdin);		    
		    if (inp[inp_i] == '\n' || inp[inp_i] == EOF) {
		      inp[inp_i] ='\0';
		      break;
		    }		
		    inp_i++;

		  } while (inp[inp_i - 1] != '\n' && inp[inp_i - 1] != EOF);
		  
		  tmp = atoi(inp);
		  
		  if ((tmp > 18) || (tmp <= 0)) break;
		  
		  ndbout << "Starting test " << tmp << "..." << endl;

		  for (i = 0; i < tNoOfThreads ; i++){ ThreadStart[i] = tmp; }
		  
		  cont = 1;
		  while (cont) {
			  NdbSleep_MilliSleep(10);
			  cont = 0;
			  for (i = 0; i < tNoOfThreads ; i++){
				  if (!ThreadReady[i]) cont = 1;
			  }
		  }//while
	  }//for(;;)

  }else{

	  if(19 == nTest){
		  ndbout << "Executing all 18 available tests..." << endl << endl;
		  for (int count = 1; count < nTest; count++){
			  ndbout << "Test " << count << endl ;
			  ndbout << "------" << endl << endl ;
			  for (i = 0; i < tNoOfThreads ; i++) { ThreadStart[i] = count ; }
			  cont = 1;
			  while (cont) {
				  NdbSleep_MilliSleep(10);
				  cont = 0;
				  for (i = 0; i < tNoOfThreads ; i++){
					  if (!ThreadReady[i]) cont = 1;
				  }
			  }
		  }//for
	  }else{
		  ndbout << endl << "Executing test " << nTest << endl << endl;
		  for (i = 0; i < tNoOfThreads ; i++) { ThreadStart[i] = nTest ; }
		  cont = 1;
		  while (cont) {
			  NdbSleep_MilliSleep(10);
			  cont = 0;
			  for (i = 0; i < tNoOfThreads ; i++){
				  if (!ThreadReady[i]) cont = 1;
			  }
		  }
	  }//if(18 == nTest)
  } //if(!nTest)

  ndbout << "--------------------------------------------------" << endl;

  for (i = 0; i < tNoOfThreads ; i++) ThreadReady[i] = 0;
  // Signaling threads to stop 
  for (i = 0; i < tNoOfThreads ; i++) ThreadStart[i] = 999;

    // Wait for threads to stop
  cont = 1;
  do {
     NdbSleep_MilliSleep(1);
	 cont = 0;
	 for (i = 0; i < tNoOfThreads ; i++){
      if (ThreadReady[i] == 0) cont = 1;
	 }
  } while (cont == 1);

  delete pMyNdb ;
  ndbout << endl << "Ronja Benchmark completed" << endl;
  return NDBT_ProgramExit(NDBT_OK) ;

error_input:
  ndbout << endl << "  Ivalid parameter(s)" << endl;
  ndbout <<         "  Usage: benchronja [-t threads][-r rec] [-o ops] [-c ops_per_exec] [-p test], where:" << endl;
  ndbout <<			"  threads - the number of threads to start; default: 1" << endl;
  ndbout <<			"  rec - the number of records in the tables; default: 500" << endl;
  ndbout <<			"  ops - the number of operations per transaction; default: 100000" << endl;
  ndbout <<			"  ops_per_exec - the number of operations per execution; default: 1" << endl ;
  ndbout <<			"  test - the number of test to execute; 19 executes all available tests; default: 0"<< endl ;
  ndbout <<			"  which enters a loop expecting manual input of test number to execute." << endl << endl ;
  delete pMyNdb ;
  return NDBT_ProgramExit(NDBT_WRONGARGS) ;

  }
////////////////////////////////////////

void commitTrans(Ndb* aNdb, NdbConnection* aCon)
{
  int ret = aCon->execute(Commit);
  assert (ret != -1);
  aNdb->closeTransaction(aCon);
}

void rollbackTrans(Ndb* aNdb, NdbConnection* aCon)
{
  int ret = aCon->execute(Rollback);
  assert (ret != -1);
  aNdb->closeTransaction(aCon);
}

void updateNoCommit(NdbConnection* aCon, Uint32* flip, unsigned int key)
{
  NdbOperation* theOperation;

  *flip = *flip + 1;
  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->updateTuple();
  theOperation->equal((Uint32)0, key);
  theOperation->setValue((Uint32)1, (char*)flip);
  int ret = aCon->execute(NoCommit);
  assert (ret != -1);
}

void updateNoCommitFail(NdbConnection* aCon, unsigned int key)
{
  NdbOperation* theOperation;

  Uint32 flip = 0;
  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->updateTuple();
  theOperation->equal((Uint32)0, key);
  theOperation->setValue((Uint32)1, (char*)flip);
  int ret = aCon->execute(NoCommit);
  assert (ret == -1);
}

void deleteNoCommit(NdbConnection* aCon, Uint32* flip, unsigned int key)
{
  NdbOperation* theOperation;

  *flip = 0;
  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->deleteTuple();
  theOperation->equal((Uint32)0, key);
  int ret = aCon->execute(NoCommit);
  assert (ret != -1);
}

void insertNoCommit(NdbConnection* aCon, Uint32* flip, unsigned int key)
{
  NdbOperation* theOperation;
  Uint32 placeholder[100];

  *flip = *flip + 1;
  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->insertTuple();
  theOperation->equal((Uint32)0, key);
  theOperation->setValue((Uint32)1, (char*)flip);
  theOperation->setValue((Uint32)2, (char*)&placeholder[0]);
  theOperation->setValue((Uint32)3, (char*)&placeholder[0]);
  int ret = aCon->execute(NoCommit);
  assert (ret != -1);
}

void writeNoCommit(NdbConnection* aCon, Uint32* flip, unsigned int key)
{
  NdbOperation* theOperation;
  Uint32 placeholder[100];

  *flip = *flip + 1;
  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->writeTuple();
  theOperation->equal((Uint32)0, key);
  theOperation->setValue((Uint32)1, (char*)flip);
  theOperation->setValue((Uint32)2, (char*)&placeholder[0]);
  theOperation->setValue((Uint32)3, (char*)&placeholder[0]);
  int ret = aCon->execute(NoCommit);
  assert (ret != -1);
}

void readNoCommit(NdbConnection* aCon, Uint32* flip, Uint32 key, int expected_ret)
{
  NdbOperation* theOperation;
  Uint32 readFlip;

  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->readTuple();
  theOperation->equal((Uint32)0, key);
  theOperation->getValue((Uint32)1, (char*)&readFlip);
  int ret = aCon->execute(NoCommit);
  assert (ret == expected_ret);
  if (ret == 0) 
    assert (*flip == readFlip);
}

void readDirtyNoCommit(NdbConnection* aCon, Uint32* flip, Uint32 key, int expected_ret)
{
  NdbOperation* theOperation;
  Uint32 readFlip;

  theOperation = aCon->getNdbOperation("SHORT_REC");
  theOperation->committedRead();
  theOperation->equal((Uint32)0, key);
  theOperation->getValue((Uint32)1, (char*)&readFlip);
  int ret = aCon->execute(NoCommit);
  assert (ret == expected_ret);
  if (ret == 0) 
    assert (*flip == readFlip);
}

void readVerify(Ndb* aNdb, Uint32* flip, Uint32 key, int expected_ret)
{
  NdbConnection* theTransaction;
  theTransaction = aNdb->startTransaction();
  readNoCommit(theTransaction, flip, key, expected_ret);
  commitTrans(aNdb, theTransaction);
}

void readDirty(Ndb* aNdb, Uint32* flip, Uint32 key, int expected_ret)
{
  NdbOperation* theOperation;
  NdbConnection* theTransaction;
  Uint32 readFlip;

  theTransaction = aNdb->startTransaction();
  theOperation = theTransaction->getNdbOperation("SHORT_REC");
  theOperation->committedRead();
  theOperation->equal((Uint32)0, key);
  theOperation->getValue((Uint32)1, (char*)&readFlip);
  int ret = theTransaction->execute(Commit);
  assert (ret == expected_ret);
  if (ret == 0) 
    assert (*flip == readFlip);
  aNdb->closeTransaction(theTransaction);
}

int multiRecordTest(Ndb* aNdb, unsigned int key)
{
  NdbConnection* theTransaction;
  Uint32 flip = 0;
  Uint32 save_flip;
  ndbout << "0" << endl;

  theTransaction = aNdb->startTransaction();

  updateNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);

  updateNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);

  commitTrans(aNdb, theTransaction);

  ndbout << "1 " << endl;

  readVerify(aNdb, &flip, key, 0);
  readDirty(aNdb, &flip, key, 0);
  save_flip = flip;
  ndbout << "1.1 " << endl;

  theTransaction = aNdb->startTransaction();

  deleteNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, -1);
  readDirty(aNdb, &save_flip, key, 0);           // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, -1);
  ndbout << "1.2 " << endl;

  insertNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);
  readDirty(aNdb, &save_flip, key, 0);           // COMMITTED READ!!!
  ndbout << "1.3 " << endl;

  updateNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);
  readDirty(aNdb, &save_flip, key, 0);           // COMMITTED READ!!!
  ndbout << "1.4 " << endl;

  commitTrans(aNdb, theTransaction);

  ndbout << "2 " << endl;

  readDirty(aNdb, &flip, key, 0);           // COMMITTED READ!!!
  readVerify(aNdb, &flip, key, 0);

  save_flip = flip;
  theTransaction = aNdb->startTransaction();

  deleteNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, -1);       // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, -1);

  insertNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);

  updateNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, 0);        // COMMITTED READ!!!

  deleteNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, -1);
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, -1);

  rollbackTrans(aNdb, theTransaction);

  ndbout << "3 " << endl;

  flip = save_flip;
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readVerify(aNdb, &flip, key, 0);

  theTransaction = aNdb->startTransaction();

  updateNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, 0);
  readNoCommit(theTransaction, &flip, key, 0);

  deleteNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, -1);
  readDirtyNoCommit(theTransaction, &flip, key, -1);
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!

  insertNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!

  updateNoCommit(theTransaction, &flip, key);

  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);
  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!

  deleteNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, 0);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, -1);
  readDirtyNoCommit(theTransaction, &flip, key, -1);

  commitTrans(aNdb, theTransaction);

  ndbout << "4 " << endl;

  readVerify(aNdb, &flip, key, -1);

  theTransaction = aNdb->startTransaction();

  insertNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, -1);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);

  deleteNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, -1);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, -1);
  readDirtyNoCommit(theTransaction, &flip, key, -1);

  insertNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, -1);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);

  updateNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, -1);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, 0);
  readDirtyNoCommit(theTransaction, &flip, key, 0);

  deleteNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &save_flip, key, -1);                     // COMMITTED READ!!!
  readNoCommit(theTransaction, &flip, key, -1);
  readDirtyNoCommit(theTransaction, &flip, key, -1);

  commitTrans(aNdb, theTransaction);

  ndbout << "5 " << endl;

  readDirty(aNdb, &flip, key, -1);                         // COMMITTED READ!!!
  readVerify(aNdb, &flip, key, -1);

  theTransaction = aNdb->startTransaction();

  insertNoCommit(theTransaction, &flip, key);

  readDirty(aNdb, &flip, key, -1);                         // COMMITTED READ!!!
  readDirtyNoCommit(theTransaction, &flip, key, 0);        // COMMITTED READ!!!

  commitTrans(aNdb, theTransaction);
  readDirty(aNdb, &flip, key, 0);                          // COMMITTED READ!!!

  ndbout << "6 " << endl;

  theTransaction = aNdb->startTransaction();

  deleteNoCommit(theTransaction, &flip, key);
  updateNoCommitFail(theTransaction, key);
  rollbackTrans(aNdb, theTransaction);
  return 0;
}

int lookup(Ndb* aNdb, unsigned int key, unsigned int long_short, int guess){

  int placeholder[500];
  unsigned int flip, count;
  int ret_value, i;
  NdbConnection* theTransaction;
  NdbOperation* theOperation;
  if ( !aNdb ) return -1 ;
	
  if (guess != 0)
    theTransaction = aNdb->startTransaction((Uint32)0, (const char*)&key, (Uint32)4);
  else
    theTransaction = aNdb->startTransaction();

  for (i = 0; i < tNoOfOpsPerExecute; i++) {
    if (long_short == 0)
      theOperation = theTransaction->getNdbOperation("SHORT_REC");
    else
      theOperation = theTransaction->getNdbOperation("LONG_REC");
    if (theOperation == NULL) {
      ndbout << "Table missing" << endl;
	  aNdb->closeTransaction(theTransaction) ;
	  return -1;
    }//if
    theOperation->simpleRead();
    theOperation->equal((Uint32)0, key);
    theOperation->getValue((Uint32)1, (char*)&flip);
    theOperation->getValue((Uint32)2, (char*)&count);
    if (theOperation->getValue((Uint32)3, (char*)&placeholder[0]) == NULL) {
      ndbout << "Error in definition phase = " << theTransaction->getNdbError() << endl;  
      aNdb->closeTransaction(theTransaction);
      return -1;
    }//if
  }//for
  ret_value = theTransaction->execute(Commit);
  if (ret_value == -1)
    ndbout << "Error in lookup:" << theTransaction->getNdbError() << endl;
    aNdb->closeTransaction(theTransaction);
	return ret_value;
}//lookup()

int update(Ndb* aNdb, unsigned int key, unsigned int long_short, int guess)
{
  int placeholder[500];
  int ret_value, i;
  unsigned int flip, count;
  NdbConnection* theTransaction;
  NdbOperation* theOperation;

  if ( !aNdb ) return -1 ;

  if (guess != 0)
    theTransaction = aNdb->startTransaction((Uint32)0, (const char*)&key, (Uint32)4);
  else
    theTransaction = aNdb->startTransaction();

  for (i = 0; i < tNoOfOpsPerExecute; i++) {
    if (long_short == 0)
      theOperation = theTransaction->getNdbOperation("SHORT_REC"); // Use table SHORT_REC
    else
      theOperation = theTransaction->getNdbOperation("LONG_REC"); // Use table LONG_REC
    if (theOperation == NULL) {
      ndbout << "Table missing" << endl;
	  aNdb->closeTransaction(theTransaction) ;
	  delete aNdb ;
      return -1;
    }//if
    theOperation->interpretedUpdateTuple();                       // Send interpreted program to NDB kernel
    theOperation->equal((Uint32)0, key);                          // Search key
    theOperation->getValue((Uint32)1, (char*)&flip);              // Read value of flip
    theOperation->getValue((Uint32)2, (char*)&count);             // Read value of count
    theOperation->getValue((Uint32)3, (char*)&placeholder[0]);    // Read value of placeholder
    theOperation->load_const_u32((Uint32)1, (Uint32)0);           // Load register 1 with 0
    theOperation->read_attr((Uint32)1, (Uint32)2);                // Read Flip value into register 2
    theOperation->branch_eq((Uint32)1, (Uint32)2, (Uint32)0);     // If Flip (register 2) == 0 (register 1) goto label 0
    theOperation->branch_label((Uint32)1);                        // Goto label 1
    theOperation->def_label((Uint32)0);                           // Define label 0
    theOperation->load_const_u32((Uint32)1, (Uint32)1);           // Load register 1 with 1
    theOperation->def_label((Uint32)1);                           // Define label 0
    theOperation->write_attr((Uint32)1, (Uint32)1);               // Write 1 (register 1) into Flip
    ret_value = theOperation->incValue((Uint32)2, (Uint32)1);     // Increment Count by 1
    if (ret_value == -1) {
      ndbout << "Error in definition phase " << endl;  
      aNdb->closeTransaction(theTransaction);
      return ret_value;
    }//if
  }//for
  ret_value = theTransaction->execute(Commit);                  // Perform the actual read and update
  if (ret_value == -1) {
    ndbout << "Error in update:" << theTransaction->getNdbError() << endl;
    aNdb->closeTransaction(theTransaction); // < epaulsa
	return ret_value ;
  }//if
  aNdb->closeTransaction(theTransaction);
  return ret_value;
}//update()

int update_bug(Ndb* aNdb, unsigned int key, unsigned int long_short)
{
  int placeholder[500];
  int ret_value, i;
  unsigned int flip, count;
  NdbConnection* theTransaction;
  NdbOperation* theOperation;

  if ( !aNdb ) return -1 ;

  theTransaction = aNdb->startTransaction();
  for (i = 0; i < tNoOfOpsPerExecute; i++) {
    if (long_short == 0)
      theOperation = theTransaction->getNdbOperation("SHORT_REC"); // Use table SHORT_REC
    else
      theOperation = theTransaction->getNdbOperation("LONG_REC"); // Use table LONG_REC
    if (theOperation == NULL) {
      ndbout << "Table missing" << endl;
  	  aNdb->closeTransaction(theTransaction) ;
      return -1;
    }//if
    theOperation->interpretedUpdateTuple();                       // Send interpreted program to NDB kernel
    theOperation->equal((Uint32)0, key);                          // Search key
    theOperation->getValue((Uint32)1, (char*)&flip);              // Read value of flip
    theOperation->getValue((Uint32)2, (char*)&count);             // Read value of count
    theOperation->getValue((Uint32)3, (char*)&placeholder[0]);    // Read value of placeholder
    theOperation->load_const_u32((Uint32)1, (Uint32)0);           // Load register 1 with 0
    theOperation->read_attr((Uint32)1, (Uint32)2);                // Read Flip value into register 2
    theOperation->branch_eq((Uint32)1, (Uint32)2, (Uint32)0);     // If Flip (register 2) == 0 (register 1) goto label 0
    theOperation->branch_label((Uint32)1);                        // Goto label 1
    theOperation->def_label((Uint32)0);                           // Define label 0
    theOperation->load_const_u32((Uint32)1, (Uint32)1);           // Load register 1 with 1
    theOperation->def_label((Uint32)1);                           // Define label 0
    theOperation->write_attr((Uint32)1, (Uint32)1);               // Write 1 (register 1) into Flip
    ret_value = theOperation->incValue((Uint32)2, (Uint32)1);     // Increment Count by 1
    if (ret_value == -1) {
      ndbout << "Error in definition phase " << endl;  
      aNdb->closeTransaction(theTransaction);
      return ret_value;
    }//if
  }//for
  ret_value = theTransaction->execute(NoCommit);                  // Perform the actual read and update
  if (ret_value == -1) {
    ndbout << "Error in update:" << theTransaction->getNdbError() << endl;
    aNdb->closeTransaction(theTransaction);
	return ret_value ;
  }//if
  aNdb->closeTransaction(theTransaction);
  return ret_value;
}//update_bug()

int update_interpreter_test(Ndb* aNdb, unsigned int key, unsigned int long_short)
{
  int placeholder[500];
  int ret_value, i;
  unsigned int flip, count;
  NdbConnection* theTransaction;
  NdbOperation* theOperation;
  Uint32 Tlabel = 0;

  if ( !aNdb ) return -1 ; 

//------------------------------------------------------------------------------
// Start the transaction and get a unique transaction id
//------------------------------------------------------------------------------
  theTransaction = aNdb->startTransaction();
  for (i = 0; i < tNoOfOpsPerExecute; i++) {
//------------------------------------------------------------------------------
// Get the proper table object and load schema information if not already
// present.
//------------------------------------------------------------------------------
    if (long_short == 0)
      theOperation = theTransaction->getNdbOperation("SHORT_REC"); // Use table SHORT_REC
    else
      theOperation = theTransaction->getNdbOperation("LONG_REC"); // Use table LONG_REC
    if (theOperation == NULL) {
      ndbout << "Table missing" << endl;
  	  aNdb->closeTransaction(theTransaction) ;
      return -1;
    }//if
//------------------------------------------------------------------------------
// Define the operation type and the tuple key (primary key in this case).
//------------------------------------------------------------------------------
    theOperation->interpretedUpdateTuple();                       // Send interpreted program to NDB kernel
    theOperation->equal((Uint32)0, key);                          // Search key

//------------------------------------------------------------------------------
// Perform initial read of attributes before updating them
//------------------------------------------------------------------------------
    theOperation->getValue((Uint32)1, (char*)&flip);              // Read value of flip
    theOperation->getValue((Uint32)2, (char*)&count);             // Read value of count
    theOperation->getValue((Uint32)3, (char*)&placeholder[0]);    // Read value of placeholder

//------------------------------------------------------------------------------
// Test that the various branch operations can handle things correctly.
// Test first 2 + 3 = 5 with 32 bit registers
// Next test the same with 32 bit + 64 bit = 64 
//------------------------------------------------------------------------------
    theOperation->load_const_u32((Uint32)4, (Uint32)0);           // Load register 4 with 0

    theOperation->load_const_u32((Uint32)0, (Uint32)0);
    theOperation->load_const_u32((Uint32)1, (Uint32)3);
    theOperation->load_const_u32((Uint32)2, (Uint32)5);
    theOperation->load_const_u32((Uint32)3, (Uint32)1);
    theOperation->def_label(Tlabel++);
    theOperation->def_label(Tlabel++);
    theOperation->sub_reg((Uint32)2, (Uint32)3, (Uint32)2);
    theOperation->branch_ne((Uint32)2, (Uint32)0, (Uint32)0);
    theOperation->load_const_u32((Uint32)2, (Uint32)5);
    theOperation->sub_reg((Uint32)1, (Uint32)3, (Uint32)1);
    theOperation->branch_ne((Uint32)1, (Uint32)0, (Uint32)1);  

    theOperation->load_const_u32((Uint32)1, (Uint32)2);           // Load register 1 with 2
    theOperation->load_const_u32((Uint32)2, (Uint32)3);           // Load register 2 with 3
    theOperation->add_reg((Uint32)1, (Uint32)2, (Uint32)1);       // 2+3 = 5 into reg 1
    theOperation->load_const_u32((Uint32)2, (Uint32)5);           // Load register 2 with 5

    theOperation->def_label(Tlabel++);

    theOperation->branch_eq((Uint32)1, (Uint32)2, Tlabel);
    theOperation->interpret_exit_nok((Uint32)6001);

    theOperation->def_label(Tlabel++);
    theOperation->branch_ne((Uint32)1, (Uint32)2, Tlabel);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->interpret_exit_nok((Uint32)6002);

    theOperation->def_label(Tlabel++);
    theOperation->branch_lt((Uint32)1, (Uint32)2, Tlabel);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->interpret_exit_nok((Uint32)6003);

    theOperation->def_label(Tlabel++);
    theOperation->branch_gt((Uint32)1, (Uint32)2, Tlabel);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->interpret_exit_nok((Uint32)6005);

    theOperation->def_label(Tlabel++);
    theOperation->branch_eq_null((Uint32)1, Tlabel);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->interpret_exit_nok((Uint32)6006);

    theOperation->def_label(Tlabel++);
    theOperation->branch_ne_null((Uint32)1,Tlabel);
    theOperation->interpret_exit_nok((Uint32)6007);

    theOperation->def_label(Tlabel++);
    theOperation->branch_ge((Uint32)1, (Uint32)2, Tlabel);
    theOperation->interpret_exit_nok((Uint32)6008);

    theOperation->def_label(Tlabel++);
    theOperation->branch_eq_null((Uint32)6,Tlabel);
    theOperation->interpret_exit_nok((Uint32)6009);

    theOperation->def_label(Tlabel++);
    theOperation->branch_ne_null((Uint32)6, Tlabel);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->interpret_exit_nok((Uint32)6010);

    theOperation->def_label(Tlabel++);

    theOperation->load_const_u32((Uint32)5, (Uint32)1);
    theOperation->add_reg((Uint32)4, (Uint32)5, (Uint32)4);

    theOperation->load_const_u32((Uint32)5, (Uint32)1);
    theOperation->branch_eq((Uint32)4, (Uint32)5, Tlabel);


    theOperation->load_const_u32((Uint32)5, (Uint32)2);
    theOperation->branch_eq((Uint32)4, (Uint32)5, (Tlabel + 1));

    theOperation->load_const_u32((Uint32)5, (Uint32)3);
    theOperation->branch_eq((Uint32)4, (Uint32)5, (Tlabel + 2));

    theOperation->load_const_u32((Uint32)5, (Uint32)4);
    theOperation->branch_eq((Uint32)4, (Uint32)5, (Tlabel + 3));
       
    theOperation->branch_label(Tlabel + 4);

    theOperation->def_label(Tlabel++);
    theOperation->load_const_u32((Uint32)1, (Uint32)200000);
    theOperation->load_const_u32((Uint32)2, (Uint32)300000);
    theOperation->add_reg((Uint32)1, (Uint32)2, (Uint32)1);
    theOperation->load_const_u32((Uint32)2, (Uint32)500000);
    theOperation->branch_label((Uint32)2);

    theOperation->def_label(Tlabel++);
    theOperation->load_const_u32((Uint32)1, (Uint32)200000);
    theOperation->load_const_u32((Uint32)2, (Uint32)300000);
    theOperation->add_reg((Uint32)1, (Uint32)2, (Uint32)1);
    theOperation->load_const_u32((Uint32)2, (Uint32)500000);
    theOperation->branch_label((Uint32)2);

    theOperation->def_label(Tlabel++);
    theOperation->load_const_u32((Uint32)1, (Uint32)2);
    Uint64 x = 0;
    theOperation->load_const_u64((Uint32)2, (Uint64)(x - 1));
    theOperation->add_reg((Uint32)1, (Uint32)2, (Uint32)1);
    theOperation->load_const_u32((Uint32)2, (Uint32)1);
    theOperation->branch_label((Uint32)2);

    theOperation->def_label(Tlabel++);
    theOperation->load_const_u32((Uint32)1, (Uint32)2);
    theOperation->load_const_u64((Uint32)2, (Uint64)(x - 1));
    theOperation->add_reg((Uint32)1, (Uint32)2, (Uint32)1);
    theOperation->load_const_u64((Uint32)2, (Uint64)1);
    theOperation->branch_label((Uint32)2);

    theOperation->def_label(Tlabel++);
    theOperation->read_attr((Uint32)1, (Uint32)2);
    theOperation->branch_eq((Uint32)1, (Uint32)2, Tlabel);
    theOperation->load_const_u32((Uint32)1, (Uint32)0);
    theOperation->branch_label(Tlabel + 1);
    theOperation->def_label(Tlabel++);
    theOperation->load_const_u32((Uint32)1, (Uint32)1);
    theOperation->def_label(Tlabel++);
    theOperation->write_attr((Uint32)1, (Uint32)1);
    ret_value = theOperation->incValue((Uint32)2, (Uint32)1);
    if (ret_value == -1) {
      ndbout << "Error in definition phase " << endl;
      ndbout << "Error = " << theOperation->getNdbError() << " on line = " << theOperation->getNdbErrorLine() << endl;
      aNdb->closeTransaction(theTransaction);
      return ret_value;
    }//if
  }//for
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
  ret_value = theTransaction->execute(Commit);                  // Perform the actual read and update
  if (ret_value == -1) {
    ndbout << "Error in update:" << theTransaction->getNdbError() << endl;
    aNdb->closeTransaction(theTransaction); // < epaulsa
	return ret_value ;
  }//if
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
  aNdb->closeTransaction(theTransaction);
  return ret_value;
}//update_interpreter_test()

void* ThreadExec(void* ThreadData){

  ThreadNdb* tabThread = (ThreadNdb*)ThreadData;
  Ndb*		   pMyNdb = NULL ;
  myRandom48Init(NdbTick_CurrentMillisecond());

  int		   Tsuccess = 0 ;
  int          check = 0 ;
  int          loop_count_ops = 0;
  int		   count, i, Ti;
  int		   tType =  0 ;
  int          remType = 0 ;
  unsigned int thread_no = 0 ;
  unsigned long total_milliseconds;
  unsigned int key = 0 ;
  unsigned int prob = 0 ;
  unsigned long transaction_time = 0 ;
  unsigned long transaction_max_time = 0 ;
  unsigned long min_time, max_time[MAX_TIMERS];
  double	   mean_time, mean_square_time, std_time; 
  
  thread_no = tabThread->ThreadNo;
  pMyNdb = tabThread->NdbRef;
  if (!pMyNdb) {
    pMyNdb = new Ndb( "TEST_DB" );
    pMyNdb->init();
  }//if

  for (;;){

    min_time = 0xFFFFFFFF;
    //for (Ti = 0; Ti < MAX_TIMERS ; Ti++) max_time[Ti] = 0;
	memset(&max_time, 0, sizeof max_time) ;
    mean_time = 0;
    mean_square_time = 0;
    ThreadReady[thread_no] = 1;

    while (!ThreadStart[thread_no]){
		NdbSleep_MilliSleep(1);
	}

    // Check if signal to exit is received
    if (ThreadStart[thread_no] == 999){
		delete pMyNdb;
		pMyNdb = NULL ;
		ThreadReady[thread_no] = 1;
		NdbThread_Exit(0) ;
		return 0 ;
    }//if

    tType = ThreadStart[thread_no];
    remType = tType;
    ThreadStart[thread_no] = 0;
	ThreadReady[thread_no] = 0 ;

    // Start transaction, type of transaction
    // is received in the array ThreadStart
    loop_count_ops = tNoOfOperations;

    START_TIMER_TOP
    for (count=0 ; count < loop_count_ops ; count++) {
      
		Tsuccess = 0;
//----------------------------------------------------
// Generate a random key between 0 and tNoOfRecords - 1
//----------------------------------------------------
      key = myRandom48(tNoOfRecords); 
//----------------------------------------------------
// Start time measurement of transaction.
//----------------------------------------------------
      START_TIMER
      //do {
        switch (remType){
          case 1:
//----------------------------------------------------
// Only lookups in short record table
//----------------------------------------------------
            Tsuccess = lookup(pMyNdb, key, 0, 1);
            break;

          case 2:
//----------------------------------------------------
// Only lookups in long record table
//----------------------------------------------------
            Tsuccess = lookup(pMyNdb, key, 1, 1);
            break;
          case 3:
//----------------------------------------------------
// Only updates in short record table
//----------------------------------------------------
            Tsuccess = update(pMyNdb, key, 0, 1);
            break;
          case 4:
//----------------------------------------------------
// Only updates in long record table
//----------------------------------------------------
            Tsuccess = update(pMyNdb, key, 1, 1);
            break;
          case 5:
//----------------------------------------------------
// 50% read/50 % update in short record table
//----------------------------------------------------
            prob = myRandom48(100);
            if (prob < 50)
              Tsuccess = update(pMyNdb, key, 0, 1);
            else
              Tsuccess = lookup(pMyNdb, key, 0, 1);
            break;
          case 6:
//----------------------------------------------------
// 50% read/50 % update in long record table
//----------------------------------------------------
            prob = myRandom48(100);
            if (prob < 50)
              Tsuccess = update(pMyNdb, key, 1, 1);
            else
              Tsuccess = lookup(pMyNdb, key, 1, 1);
            break;
          case 7:
//----------------------------------------------------
// 80 read/20 % update in short record table
//----------------------------------------------------
            prob = myRandom48(100);
            if (prob < 20)
              Tsuccess = update(pMyNdb, key, 0, 1);
            else
              Tsuccess = lookup(pMyNdb, key, 0, 1);
            break;
          case 8:
//----------------------------------------------------
// 80 read/20 % update in long record table
//----------------------------------------------------
            prob = myRandom48(100);
            if (prob < 20)
              Tsuccess = update(pMyNdb, key, 1, 1);
            else
              Tsuccess = lookup(pMyNdb, key, 1, 1);
            break;
          case 9:
//----------------------------------------------------
// 25 read short/25 % read long/25 % update short/25 % update long
//----------------------------------------------------
            prob = myRandom48(100);
            if (prob < 25)
              Tsuccess = update(pMyNdb, key, 0, 1);
            else if (prob < 50)
              Tsuccess = update(pMyNdb, key, 1, 1);
            else if (prob < 75)
              Tsuccess = lookup(pMyNdb, key, 0, 1);
            else
              Tsuccess = lookup(pMyNdb, key, 1, 1);
            break;
          case 10:
//----------------------------------------------------
// Test bug with replicated interpreted update, short table
//----------------------------------------------------
            Tsuccess = update_bug(pMyNdb, key, 0);
            break;
          case 11:
//----------------------------------------------------
// Test interpreter functions, short table
//----------------------------------------------------
            Tsuccess = update_interpreter_test(pMyNdb, key, 0);
            break;
          case 12:
//----------------------------------------------------
// Test bug with replicated interpreted update, long table
//----------------------------------------------------
            Tsuccess = update_bug(pMyNdb, key, 1);
            break;
          case 13:
//----------------------------------------------------
// Test interpreter functions, long table
//----------------------------------------------------
            Tsuccess = update_interpreter_test(pMyNdb, key, 1);
            break;
          case 14:
//----------------------------------------------------
// Only lookups in short record table
//----------------------------------------------------
            Tsuccess = lookup(pMyNdb, key, 0, 0);
            break;
          case 15:
//----------------------------------------------------
// Only lookups in long record table
//----------------------------------------------------
            Tsuccess = lookup(pMyNdb, key, 1, 0);
            break;
          case 16:
//----------------------------------------------------
// Only updates in short record table
//----------------------------------------------------
            Tsuccess = update(pMyNdb, key, 0, 0);
            break;
          case 17:
//----------------------------------------------------
// Only updates in long record table
//----------------------------------------------------
            Tsuccess = update(pMyNdb, key, 1, 0);
            break;
          case 18:
            Tsuccess = multiRecordTest(pMyNdb, key);
            break;
          default:
            break;
        }//switch
      //} while (0);//
	  if(-1 == Tsuccess) {
		  NDBT_ProgramExit(NDBT_FAILED);
		  exit(-1);
	  } // for
//----------------------------------------------------
// Stop time measurement of transaction.
//----------------------------------------------------     
	  STOP_TIMER	  
	  transaction_time = (unsigned long)timer.elapsedTime() ;//stopTimer(&theStartTime);
//----------------------------------------------------
// Perform calculations of time measurements.
//----------------------------------------------------
      transaction_max_time = transaction_time;
      for (Ti = 0; Ti < MAX_TIMERS; Ti++) {
        if (transaction_max_time > max_time[Ti]) {
          Uint32 tmp = max_time[Ti];
          max_time[Ti] = transaction_max_time;
          transaction_max_time = tmp;
        }//if
      }//if
      if (transaction_time < min_time) min_time = transaction_time;
      mean_time = (double)transaction_time + mean_time;
      mean_square_time = (double)(transaction_time * transaction_time) + mean_square_time;
    }//for
//----------------------------------------------------
// Calculate mean and standard deviation
//----------------------------------------------------
    STOP_TIMER_TOP
    total_milliseconds = (unsigned long)timer_top.elapsedTime() ;//stopTimer(&total_time);
    mean_time = mean_time / loop_count_ops;
    mean_square_time = mean_square_time / loop_count_ops;
    std_time = sqrt(mean_square_time - (mean_time * mean_time));
//----------------------------------------------------
// Report statistics
//----------------------------------------------------
	ndbout << "Thread = " << thread_no << " reporting:" << endl ;
	ndbout << "------------------------------" << endl ;
    ndbout << "Total time is " << (unsigned int)(total_milliseconds /1000);
    ndbout << " seconds and " << (unsigned int)(total_milliseconds % 1000);
    ndbout << " milliseconds" << endl;
    ndbout << "Minimum time = " << (unsigned int)min_time << " milliseconds" << endl;
    for (Ti = 0; Ti < MAX_TIMERS; Ti++) {
      ndbout << "Maximum timer " << Ti << " = " << (unsigned int)max_time[Ti] << " milliseconds" << endl;
      ndbout << "Mean time = " << (unsigned int)mean_time << " milliseconds" << endl;
      ndbout << "Standard deviation on time = " << (unsigned int)std_time;
      ndbout << " milliseconds" << endl << endl ;
    }//for
    ndbout << endl ;
  
  } // for(;;)
  
  delete pMyNdb ;
  NdbThread_Exit(0) ;
  return 0 ; // Compiler is happy now
}

