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

#define MAX_PARTS 4 
#define MAX_SEEK 16 
#define MAXSTRLEN 16 
#define MAXATTR 64
#define MAXTABLES 64
#define MAXTHREADS 128
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

extern "C" { static void* threadLoop(void*); }
static void setAttrNames(void);
static void setTableNames(void);
static int readArguments(int argc, const char** argv);
static int createTables(Ndb*);
static void defineOperation(NdbConnection* aTransObject, StartType aType, 
                            Uint32 base, Uint32 aIndex);
static void execute(StartType aType);
static bool executeThread(StartType aType, Ndb* aNdbObject, unsigned int);
static void executeCallback(int result, NdbConnection* NdbObject,
                            void* aObject);
static bool error_handler(const NdbError & err);
static Uint32 getKey(Uint32, Uint32) ;
static void input_error();


static int                              retry_opt = 3 ;
static int                              failed = 0 ;
                
ErrorData * flexAsynchErrorData;                        

struct ThreadNdb
{
  int NoOfOps;
  int ThreadNo;
};

static NdbThread*               threadLife[MAXTHREADS];
static int                              tNodeId;
static int                              ThreadReady[MAXTHREADS];
static StartType                ThreadStart[MAXTHREADS];
static char                             tableName[MAXTABLES][MAXSTRLEN+1];
static char                             attrName[MAXATTR][MAXSTRLEN+1];

// Program Parameters
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

//Program Flags
static int                              theTestFlag = 0;
static int                              theSimpleFlag = 0;
static int                              theDirtyFlag = 0;
static int                              theWriteFlag = 0;
static int                              theStdTableNameFlag = 0;
static int                              theTableCreateFlag = 0;

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

NDB_COMMAND(flexAsynch, "flexAsynch", "flexAsynch", "flexAsynch", 65535)
{
  ThreadNdb*            pThreadData;
  int                   tLoops=0, i;
  int                   returnValue = NDBT_OK;

  flexAsynchErrorData = new ErrorData;
  flexAsynchErrorData->resetErrorCounters();

  if (readArguments(argc, argv) != 0){
    input_error();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  pThreadData = new ThreadNdb[MAXTHREADS];

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

  Ndb * pNdb = new Ndb("TEST_DB");      
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

  if(returnValue == NDBT_OK){
    /****************************************************************
     *  Create NDB objects.                                   *
     ****************************************************************/
    resetThreads();
    for (int i = 0; i < tNoOfThreads ; i++) {
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
      PRINT_TIMER("insert", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
        i = retry_opt ;
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

      START_TIMER;
      execute(stRead);
      STOP_TIMER;
      PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
        i = retry_opt ;
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
      PRINT_TIMER("update", noOfTransacts, tNoOfOpsPerTrans) ;

      if (0 < failed) {
        i = retry_opt ;
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
          
      START_TIMER;
      execute(stRead);
      STOP_TIMER;
      PRINT_TIMER("read", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
        i = retry_opt ;
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
      PRINT_TIMER("delete", noOfTransacts, tNoOfOpsPerTrans);

      if (0 < failed) {
        i = retry_opt ;
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
    for(i = 0; i<tNoOfThreads; i++){
      NdbThread_WaitFor(threadLife[i], &tmp);
      NdbThread_Destroy(&threadLife[i]);
    }
  } 
  delete [] pThreadData;
  delete pNdb;

  //printing errorCounters
  flexAsynchErrorData->printErrorCounters(ndbout);

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
  localNdb = new Ndb("TEST_DB");
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
    if(!executeThread(tType, localNdb, threadBase)){
      break;
    }
    ThreadReady[threadNo] = 1;
  }//for

  delete localNdb;
  ThreadReady[threadNo] = 1;

  NdbThread_Exit(0);
  return NULL; // Just to keep compiler happy
}//threadLoop()

static 
bool
executeThread(StartType aType, Ndb* aNdbObject, unsigned int threadBase) {
  int i, j, k;
  NdbConnection* tConArray[1024];
  unsigned int tBase;
  unsigned int tBase2;

  for (i = 0; i < tNoOfTransactions; i++) {
    if (tLocal == false) {
      tBase = i * tNoOfParallelTrans * tNoOfOpsPerTrans;
    } else {
      tBase = i * tNoOfParallelTrans * MAX_SEEK;
    }//if
    START_REAL_TIME;
    for (j = 0; j < tNoOfParallelTrans; j++) {
      if (tLocal == false) {
        tBase2 = tBase + (j * tNoOfOpsPerTrans);
      } else {
        tBase2 = tBase + (j * MAX_SEEK);
        tBase2 = getKey(threadBase, tBase2);
      }//if
      if (startTransGuess == true) {
        Uint64 Tkey64;
        Uint32* Tkey32 = (Uint32*)&Tkey64;
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
      
      for (k = 0; k < tNoOfOpsPerTrans; k++) {
        //-------------------------------------------------------
        // Define the operation, but do not execute it yet.
        //-------------------------------------------------------
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
    while (Tcomp < tNoOfParallelTrans) {
      int TlocalComp = aNdbObject->pollNdb(3000, 0);
      Tcomp += TlocalComp;
    }//while
    for (j = 0 ; j < tNoOfParallelTrans ; j++) {
      aNdbObject->closeTransaction(tConArray[j]);
    }//for
  }//for
  return true;
}//executeThread()

static 
Uint32
getKey(Uint32 aBase, Uint32 anIndex) {
  Uint32 Tfound = anIndex;
  Uint64 Tkey64;
  Uint32* Tkey32 = (Uint32*)&Tkey64;
  Tkey32[0] = aBase;
  Uint32 hash;
  for (Uint32 i = anIndex; i < (anIndex + MAX_SEEK); i++) {
    Tkey32[1] = (Uint32)i;
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
  for (int k = 2; k < loopCountAttributes; k++) {
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

static void setAttrNames()
{
  int i;

  for (i = 0; i < MAXATTR ; i++){
    snprintf(attrName[i], MAXSTRLEN, "COL%d", i);
  }
}


static void setTableNames()
{
  // Note! Uses only uppercase letters in table name's
  // so that we can look at the tables wits SQL
  int i;
  for (i = 0; i < MAXTABLES ; i++){
    if (theStdTableNameFlag==0){
      snprintf(tableName[i], MAXSTRLEN, "TAB%d_%d", i, 
               (int)(NdbTick_CurrentMillisecond()/1000));
    } else {
      snprintf(tableName[i], MAXSTRLEN, "TAB%d", i);
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
      for (int j = 1; j < tNoOfAttributes ; j++){
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
  }
  return false ; // return false to abort
}
static
bool error_handler(const char* error_string, int error_int) {
  ndbout << error_string << endl ;
  if ((4008 == error_int) ||
      (721 == error_int) ||
      (266 == error_int)){
    ndbout << endl << "Attempting to recover and continue now..." << endl ;
    return true ; // return true to retry
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
      if ((tNoOfThreads < 1) || (tNoOfThreads > MAXTHREADS)){
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
  ndbout_c("");
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
}
  


