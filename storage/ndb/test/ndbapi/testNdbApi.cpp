/*
   Copyright (c) 2003, 2018 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <random.h>
#include <NdbTick.h>
#include <my_sys.h>
#include "../../src/ndbapi/SignalSender.hpp"
#include <GlobalSignalNumbers.h>

#define MAX_NDB_OBJECTS 32678

#define CHECK(b) if (!(b)) { \
  g_err.println("ERR: failed on line %u", __LINE__); \
  return -1; } 

#define CHECKE(b,obj) if (!(b)) {                          \
    g_err.println("ERR:failed on line %u with err %u %s",  \
                  __LINE__,                                \
                  obj.getNdbError().code,                 \
                  obj.getNdbError().message); \
    return -1; }

static const char* ApiFailTestRun = "ApiFailTestRun";
static const char* ApiFailTestComplete = "ApiFailTestComplete";
static const char* ApiFailTestsRunning = "ApiFailTestsRunning";
static const char* ApiFailNumberPkSteps = "ApiFailNumberPkSteps";
static const int MAX_STEPS = 10;
static Ndb_cluster_connection* otherConnection = NULL;
static Ndb* stepNdbs[MAX_STEPS];


int runTestMaxNdb(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 loops = ctx->getNumLoops();
  Uint32 l = 0;
  int oldi = 0;
  int result = NDBT_OK;

  while (l < loops && result == NDBT_OK){
    ndbout_c("loop %d", l + 1);
    int errors = 0;
    
    Vector<Ndb*> ndbVector;
    int i = 0;
    int init = 0;
    do {      
      
      Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
      if (pNdb == NULL){
	ndbout << "pNdb == NULL" << endl;      
	errors++;
	continue;
	
      }
      i++;

      ndbVector.push_back(pNdb);
      
      if (pNdb->init()){
	NDB_ERR(pNdb->getNdbError());
	errors++;
	continue;
      }
      
      init++;

    } while (errors == 0);
    
    ndbout << i << " ndb objects created" << endl;
    
    if (l > 0 && i != oldi && init != MAX_NDB_OBJECTS){
      ndbout << l << ": not as manyNdb objects created" << endl
	     << i << " != " << oldi << endl;
      result =  NDBT_FAILED;
    }

    oldi = i;
      
    
    for(unsigned j = 0;  j < ndbVector.size(); j++){
      delete ndbVector[j];
      if(((j+1) % 250) == 0){
	ndbout << "Deleted " << (Uint64) j << " ndb objects " << endl;
      }
    }
    ndbVector.clear();

    l++;
  }

  return result;
}

int runTestMaxTransaction(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 loops = ctx->getNumLoops();
  Uint32 l = 0;
  int oldi = 0;
  int result = NDBT_OK;

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* pTab = ctx->getTab();
  if (pTab == 0) abort();

  while (l < loops && result == NDBT_OK){
    int errors = 0;
    int maxErrors = 5;
    
    Vector<NdbConnection*> conVector;


    int i = 0;
    do {      

      NdbConnection* pCon;
      
      int type = i%2;
      switch (type){
      case 0:
	pCon = pNdb->startTransaction();
	break;
      case 1:
      {
	BaseString key;
	key.appfmt("DATA-%d", i);
	ndbout_c("%s", key.c_str());
	pCon = pNdb->startTransaction(pTab,
				      key.c_str(),
				      key.length());
      }
      break;
      default:
	abort();
      }
      
      if (pCon == NULL){
	NDB_ERR(pNdb->getNdbError());
	errors++;
	continue;
      }
	  
      conVector.push_back(pCon);
	        
      i++;      
    } while (errors < maxErrors);

    ndbout << i << " connections created" << endl;

    if (l > 0 && i != oldi){
      ndbout << l << ": not as many transactions created" << endl
	     << i << " != " << oldi << endl;
      result =  NDBT_FAILED;
    }

    oldi = i;
      
    
    for(unsigned j = 0; j < conVector.size(); j++){
      pNdb->closeTransaction(conVector[j]);
    }
    conVector.clear();
    l++;

  }

  // BONUS Test closeTransaction with null trans
  pNdb->closeTransaction(NULL);

  delete pNdb;


  return result;
}

int runTestMaxOperations(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 l = 1;
  int result = NDBT_OK;
  int maxOpsLimit = 1;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    ndbout << "pNdb.init() failed" << endl;      
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);

  bool endTest = false;
  while (!endTest){
    int errors = 0;
    const int maxErrors = 5;

    maxOpsLimit = l*1000;    
       
    if (hugoOps.startTransaction(pNdb) != NDBT_OK){
      delete pNdb;
      ndbout << "startTransaction failed, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    
    int i = 0;
    do
    {
      i++;      

      const int rowNo = (i % 256);
      if(hugoOps.pkReadRecord(pNdb, rowNo, 1) != NDBT_OK){
        errors++;
        ndbout << "ReadRecord failed at line: " << __LINE__ << ", row: " << rowNo << endl;
        if (errors >= maxErrors){
          result = NDBT_FAILED;
          maxOpsLimit = i;
        }
      }

      // Avoid Transporter overload by executing after max 1000 ops.
      int execResult = 0;
      if (i >= maxOpsLimit)
        execResult = hugoOps.execute_Commit(pNdb); //Commit after last op
      else if ((i%1000) == 0)
        execResult = hugoOps.execute_NoCommit(pNdb);
      else
        continue;

      switch(execResult){
      case NDBT_OK:
        break;

      default:
        result = NDBT_FAILED;
        // Fall through - to '233' which also terminate test, but not 'FAILED'
      case 233:  // Out of operation records in transaction coordinator  
      case 1217: // Out of operation records in local data manager
        // OK - end test
        endTest = true;
        maxOpsLimit = i;		
        ndbout << "execute failed at line: " << __LINE__
	       << ", with execResult: " << execResult << endl;
        break;
      }
    } while (i < maxOpsLimit);

    ndbout << i << " operations used" << endl;

    hugoOps.closeTransaction(pNdb);

    l++;
  }

  /**
   * After the peak usage of NdbOperations comes a cool down periode
   * with lower usage. Check that the NdbOperations free list manager
   * will gradually reduce number of free NdbOperations kept for 
   * later reuse.
   */
  Uint32 hiFreeOperations = 0;
  Uint32 freeOperations = 0;
  {
    Ndb::Free_list_usage usage_stat;
    usage_stat.m_name= NULL;
    while (pNdb->get_free_list_usage(&usage_stat))
    {
      if (strcmp(usage_stat.m_name, "NdbOperation") == 0)
      {
        hiFreeOperations = usage_stat.m_free;
        break;
      }
    }
  }

  maxOpsLimit = 100;
  Uint32 coolDownLoops = 25;
  while (coolDownLoops-- > 0){
    int errors = 0;
    const int maxErrors = 5;

    if (hugoOps.startTransaction(pNdb) != NDBT_OK){
      delete pNdb;
      ndbout << "startTransaction failed, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    
    for (int rowNo = 0; rowNo < 100; rowNo++)
    {
      if(hugoOps.pkReadRecord(pNdb, rowNo, 1) != NDBT_OK){
        errors++;
        ndbout << "ReadRecord failed at line: " << __LINE__ << ", row: " << rowNo << endl;
        if (errors >= maxErrors){
          result = NDBT_FAILED;
          break;
        }
      }
    }

    const int execResult = hugoOps.execute_Commit(pNdb);
    if (execResult != NDBT_OK)
    {
      ndbout << "execute failed at line: " << __LINE__
	     << ", with execResult: " << execResult << endl;
      result = NDBT_FAILED;
    }
    hugoOps.closeTransaction(pNdb);

    {
      Ndb::Free_list_usage usage_stat;
      usage_stat.m_name= NULL;
      while (pNdb->get_free_list_usage(&usage_stat))
      {
        if (strcmp(usage_stat.m_name, "NdbOperation") == 0)
        {
          freeOperations = usage_stat.m_free;
          ndbout << usage_stat.m_name << ", free: " << usage_stat.m_free
                 << endl;
          break;
        }
      }
    }
  } //while (coolDownLoops...

  /**
   * It is a pass criteria that cool down periode
   * reduced the number of free NdbOperations kept.
   */
  if (freeOperations >= hiFreeOperations)
  {
    ndbout << "Cool down periode didn't shrink NdbOperation free-list" << endl;
    result = NDBT_FAILED;
  }
  
  if (result != NDBT_OK)
    ndbout << "Test case failed with result: " << result << endl;
  
  delete pNdb;
  return result;
}

int runTestGetValue(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);
  
  for (int m = 1; m < 100; m++){
    int errors = 0;
    int maxErrors = 5;
      
    NdbConnection* pCon = pNdb->startTransaction();
    if (pCon == NULL){
      delete pNdb;
      return NDBT_FAILED;
    }
      
    NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
    if (pOp == NULL){
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }
      
    if (pOp->readTuple() != 0){
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }
      
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      if (pTab->getColumn(a)->getPrimaryKey() == true){
	if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	  NDB_ERR(pCon->getNdbError());
	  pNdb->closeTransaction(pCon);
	  delete pNdb;
	  return NDBT_FAILED;
	}
      }
    }
      
    int i = 0;
    int maxLimit = 1000*m;
    do {      
	
      if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL) {
	const NdbError err = pCon->getNdbError();
	NDB_ERR(err);
	if (err.code == 0)
	  result = NDBT_FAILED;	
	errors++;
	continue;
      }
	
      i++;             
	
    } while (errors < maxErrors && i < maxLimit);
      
    ndbout << i << " getValues called" << endl;

      
    if (pCon->execute(Commit) != 0){
      const NdbError err = pCon->getNdbError();
      switch(err.code){
      case 880: // TUP - Read too much
      case 823: // TUP - Too much AI
      case 4257: // NDBAPI - Too much AI
      case 4002: // NDBAPI - send problem
	// OK errors
	NDB_ERR(pCon->getNdbError());
	break;
      default:
	NDB_ERR(pCon->getNdbError());
	ndbout << "Illegal error" << endl;
	result= NDBT_FAILED;
	break;
      }
    }
      
    pNdb->closeTransaction(pCon);

  }// m


  delete pNdb;

  return result;
}

int runTestEqual(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 loops = ctx->getNumLoops();
  Uint32 l = 0;
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);
  
  while (l < loops){
    for(int m = 1; m < 10; m++){
      int errors = 0;
      int maxErrors = 5;
      
      NdbConnection* pCon = pNdb->startTransaction();
      if (pCon == NULL){
	ndbout << "Could not start transaction" << endl;
	delete pNdb;
	return NDBT_FAILED;
      }
      
      NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
      if (pOp == NULL){
	NDB_ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);
	delete pNdb;
	return NDBT_FAILED;
      }
      
      if (pOp->readTuple() != 0){
	NDB_ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);
	delete pNdb;
	return NDBT_FAILED;
      }
      
      int i = 0;
      int maxLimit = 1000*m;      
      do {      
	
	if ((l%2)!=0){
	  // Forward
	  for(int a = 0; a<pTab->getNoOfColumns(); a++){
	    if (pTab->getColumn(a)->getPrimaryKey() == true){
	      if(hugoOps.equalForAttr(pOp, a, 1) != 0){
		const NdbError err = pCon->getNdbError();
		NDB_ERR(err);
		if (err.code == 0)
		  result = NDBT_FAILED;
		errors++;
	      }
	    }
	  }
	} else {
	  // Backward
	  for(int a = pTab->getNoOfColumns()-1; a>=0; a--){
	    if (pTab->getColumn(a)->getPrimaryKey() == true){
	      if(hugoOps.equalForAttr(pOp, a, 1) != 0){
		const NdbError err = pCon->getNdbError();
		NDB_ERR(err);
		if (err.code == 0)
		  result = NDBT_FAILED;
		errors++;
	      }
	    }
	  }
	}
	
	i++;      
	
      } while (errors < maxErrors && i < maxLimit);
      
      if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL) {
        const NdbError err = pCon->getNdbError();
	NDB_ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);
	delete pNdb;
        if (err.code == 4225) {
          return NDBT_OK;
        } else {
          return NDBT_FAILED;
        }//if
      }
      
      ndbout << i << " equal called" << endl;
      
      
      int check = pCon->execute(Commit);
      if (check != 0){
	NDB_ERR(pCon->getNdbError());
      }
      
      pNdb->closeTransaction(pCon);
      
    }// m
    l++;
    
  }// l
  
  delete pNdb;
  return result;
}

int runTestDeleteNdb(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 loops = ctx->getNumLoops();
  Uint32 l = 0;
  int result = NDBT_OK;
  NdbRestarts restarts;
  Vector<Ndb*> ndbVector;
  const NdbDictionary::Table* pTab = ctx->getTab();
  HugoTransactions hugoTrans(*pTab);
  int records = ctx->getNumRecords();
  
  while (l < loops && result == NDBT_OK){
    
    // Create 5 ndb objects
    for( int i = 0; i < 5; i++){
      Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
      if (pNdb == NULL){
	ndbout << "pNdb == NULL" << endl;      
	result = NDBT_FAILED;	
	goto end_test;
      }
      ndbVector.push_back(pNdb);
      
      if (pNdb->init()){
	NDB_ERR(pNdb->getNdbError());
	result = NDBT_FAILED;	
	goto end_test;
      }
      if (pNdb->waitUntilReady() != 0){
	NDB_ERR(pNdb->getNdbError());
	result = NDBT_FAILED;	
	goto end_test;
      }
      if (hugoTrans.pkReadRecords(pNdb, records) != 0){
	result = NDBT_FAILED;	
	goto end_test;
      }
    }
    
    if ((l % 2) == 0){
      // Restart random node 
      ndbout << "Restart random node " << endl;
      if(restarts.executeRestart(ctx, "RestartRandomNodeAbort", 120) != 0){
	g_err << "Failed to executeRestart(RestartRandomNode)"<<endl;
	result = NDBT_FAILED;
	goto end_test;
      }
    } else {
      // Restart all nodes
      ndbout << "Restart all nodes " << endl;
      if(restarts.executeRestart(ctx, "RestartAllNodesAbort", 120) != 0){
	g_err << "Failed to executeRestart(RestartAllNodes)"<<endl;
	result = NDBT_FAILED;
	goto end_test;
      }
    }
    
    // Delete the ndb objects
    for(unsigned j = 0;  j < ndbVector.size(); j++)
      delete ndbVector[j];
    ndbVector.clear();
    l++;
  }
  
  
 end_test:
  
  for(unsigned i = 0;  i < ndbVector.size(); i++)
    delete ndbVector[i];
  ndbVector.clear();
  
  return result;
}


int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}
int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runTestWaitUntilReady(NDBT_Context* ctx, NDBT_Step* step){

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");

  // Forget about calling pNdb->init();

  if (pNdb->waitUntilReady() == 0){
    ndbout << "waitUntilReady returned OK" << endl;
    delete pNdb;
    return NDBT_FAILED;
  }
  const NdbError err = pNdb->getNdbError();
  delete pNdb;

  NDB_ERR(err);
  if (err.code != 4256)
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int runGetNdbOperationNoTab(NDBT_Context* ctx, NDBT_Step* step){

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    delete pNdb;
    return NDBT_FAILED;
  }
  
  // Call getNdbOperation on an unknown table
  NdbOperation* pOp = pCon->getNdbOperation("HUPP76");
  if (pOp == NULL){
    NdbError err = pCon->getNdbError();
    NDB_ERR(err);
    if (err.code == 0){
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }    
  }
        
  pNdb->closeTransaction(pCon);
    
  delete pNdb;

  return NDBT_OK;
}

int runBadColNameHandling(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();


  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  const int CASES= 5;
  int i;

  for (i= 0; i < CASES; i++)
  {
    ndbout << "Case " << i << endl;
    NdbConnection* pCon = pNdb->startTransaction();
    if (pCon == NULL){
      pNdb->closeTransaction(pCon);  
      delete pNdb;
      return NDBT_FAILED;
    }
    
    /* Cases 0-3 use PK ops, 4 + use scans */ 
    NdbOperation* pOp = (i < 4 ? pCon->getNdbOperation(pTab->getName()):
                         pCon->getNdbScanOperation(pTab->getName()));
    if (pOp == NULL){
      NDB_ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);  
      delete pNdb;
      return NDBT_FAILED;
    }

    bool failed= false;
    int expectedError= 0;
    HugoOperations hugoOps(*pTab);

    switch(i) {
    case 0:
      if (pOp->readTuple() != 0){
        NDB_ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }
      
      // getValue should fail, we check that we get correct errors
      // in expected places.
      expectedError= 4004;
      failed= (pOp->getValue("MOST_IMPROBABLE2") == NULL);
      break;

    case 1:
      if (pOp->readTuple() != 0){
        NDB_ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }
      
      // equal should fail, we check that we get correct errors
      // in expected places.
      expectedError= 4004;
      failed= (pOp->equal("MOST_IMPROBABLE2", 0) != 0);
      break;

    case 2:
      if (pOp->writeTuple() != 0){
        NDB_ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }

      // set equality on pk columns
      for(int a = 0; a<pTab->getNoOfColumns(); a++){
        if (pTab->getColumn(a)->getPrimaryKey() == true){
          if(hugoOps.equalForAttr(pOp, a, 1) != 0){
            const NdbError err = pCon->getNdbError();
            NDB_ERR(err);
            pNdb->closeTransaction(pCon);
            delete pNdb;
            return NDBT_FAILED;
          }
        }
      }
      
      // setValue should fail, we check that we get correct errors
      // in expected places.
      expectedError= 4004;
      failed= (pOp->setValue("MOST_IMPROBABLE2", 0) != 0);
      break;

    case 3:
      if (pOp->readTuple() != 0){
        NDB_ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }
      
      // getBlobHandle should fail, we check that we get correct errors
      // in expected places.
      expectedError= 4004;
      failed= (pOp->getBlobHandle("MOST_IMPROBABLE2") == NULL);
      break;

    case 4:
    {
      NdbScanOperation* sop= (NdbScanOperation*) pOp;
      if (sop->readTuples() != 0){
        NDB_ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }
      
      // getBlobHandle should fail, we check that we get correct errors
      // in expected places.
      expectedError= 4004;
      ndbout << "About to call getBlobHandle" << endl;
      failed= (sop->getBlobHandle("MOST_IMPROBABLE2") == NULL);

      sop->close();
      break;
    } 
    
    default:
      break;
    }

    if (failed)
    {
      const NdbError opErr= pOp->getNdbError();
      const NdbError transErr = pCon->getNdbError();
      NDB_ERR(opErr);
      NDB_ERR(transErr);
      if (opErr.code != transErr.code) {
        ndbout << "Error reporting mismatch, expected " 
               << expectedError << endl;
        result = NDBT_FAILED;
      }
      if (opErr.code != expectedError){
        ndbout << "No or bad error detected, expected " 
               << expectedError << endl;
        result = NDBT_FAILED;	
      }
    } else {
      ndbout << "Case " << i << " did not fail" << endl;
      result = NDBT_FAILED;
    }

    pNdb->closeTransaction(pCon);

    if (result == NDBT_FAILED)
      break;
  } // for
  
  delete pNdb;

  return result;
}

int runMissingOperation(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();


  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  // Forget about calling pOp->insertTuple();
  
  // Call getValue should not work
  if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL) {
    const NdbError err = pCon->getNdbError();
    NDB_ERR(err);
    if (err.code == 0){
      ndbout << "hupp" << endl;
      result = NDBT_FAILED;	
    }
  } else {
      ndbout << "hupp2" << endl;
    result = NDBT_FAILED;
  }
      
  pNdb->closeTransaction(pCon);  
  delete pNdb;

  return result;
}

int runGetValueInUpdate(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  if (pOp->updateTuple() != 0){
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }
  
  // Call getValue should not work
  if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL) {
    // It didn't work
    const NdbError err = pCon->getNdbError();
    NDB_ERR(err);
    if (err.code == 0){
      pNdb->closeTransaction(pCon);  
      delete pNdb;
      return NDBT_FAILED;	
    }
  } else {
    // It worked, not good!
    pNdb->closeTransaction(pCon);  
    delete pNdb;    
    return NDBT_FAILED;
  }

  int check = pCon->execute(Commit);
  if (check != 0){
    NDB_ERR(pCon->getNdbError());
  }
  
  pNdb->closeTransaction(pCon);  
  delete pNdb;

  return NDBT_OK;
}

int runUpdateWithoutValues(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  HugoOperations hugoOps(*pTab);

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  if (pOp->updateTuple() != 0){
    pNdb->closeTransaction(pCon);
    NDB_ERR(pOp->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	NDB_ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);
	delete pNdb;
	return NDBT_FAILED;
      }
    }
  }

  // Dont' call any setValues

  // Execute should work
  int check = pCon->execute(Commit);
  if (check == 0){
    ndbout << "execute worked" << endl;
  } else {
    NDB_ERR(pCon->getNdbError());
    result = NDBT_FAILED;
  }
  
  pNdb->closeTransaction(pCon);  
  delete pNdb;

  return result;
}

int runUpdateWithoutKeys(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();


  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
    
  NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  if (pOp->updateTuple() != 0){
    pNdb->closeTransaction(pCon);
    NDB_ERR(pOp->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  // Dont' call any equal or setValues

  // Execute should not work
  int check = pCon->execute(Commit);
  if (check == 0){
    ndbout << "execute worked" << endl;
    result = NDBT_FAILED;
  } else {
    NDB_ERR(pCon->getNdbError());
  }
  
  pNdb->closeTransaction(pCon);  
  delete pNdb;

  return result;
}


int runReadWithoutGetValue(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  HugoOperations hugoOps(*pTab);

  Ndb* pNdb = GETNDB(step);
  Uint32 lm;

  for(Uint32 cm= 0; cm < 2; cm++)
  {
    for(lm= 0; lm <= NdbOperation::LM_CommittedRead; lm++)
    {
      NdbConnection* pCon = pNdb->startTransaction();
      if (pCon == NULL){
	pNdb->closeTransaction(pCon);  
	return NDBT_FAILED;
      }
    
      NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
      if (pOp == NULL){
	NDB_ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);  
	return NDBT_FAILED;
      }
  
      if (pOp->readTuple((NdbOperation::LockMode)lm) != 0){
	pNdb->closeTransaction(pCon);
	NDB_ERR(pOp->getNdbError());
	return NDBT_FAILED;
      }
    
      for(int a = 0; a<pTab->getNoOfColumns(); a++){
	if (pTab->getColumn(a)->getPrimaryKey() == true){
	  if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	    NDB_ERR(pCon->getNdbError());
	    pNdb->closeTransaction(pCon);
	    return NDBT_FAILED;
	  }
	}
      }
    
      // Dont' call any getValues
    
      // Execute should work
      int check = pCon->execute(cm == 0 ? NoCommit : Commit);
      if (check == 0){
	ndbout << "execute worked" << endl;
      } else {
	NDB_ERR(pCon->getNdbError());
	result = NDBT_FAILED;
      }
    
      pNdb->closeTransaction(pCon);  
    }
  }

  /**
   * Now test scans
   */
  for(lm= 0; lm <= NdbOperation::LM_CommittedRead; lm++)
  {
    NdbConnection* pCon = pNdb->startTransaction();
    if (pCon == NULL){
      pNdb->closeTransaction(pCon);  
      return NDBT_FAILED;
    }
    
    NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
    if (pOp == NULL){
      NDB_ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);  
      return NDBT_FAILED;
    }
    
    if ((pOp->readTuples((NdbOperation::LockMode)lm)) != 0){
      pNdb->closeTransaction(pCon);
      NDB_ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    
    // Dont' call any getValues
    
    // Execute should work
    int check = pCon->execute(NoCommit);
    if (check == 0){
      ndbout << "execute worked" << endl;
    } else {
      NDB_ERR(pCon->getNdbError());
      result = NDBT_FAILED;
    }
  
    int res;
    while((res = pOp->nextResult()) == 0);
    pNdb->closeTransaction(pCon);  
    
    if(res != 1)
      result = NDBT_FAILED;
  }
  
  return result;
}


int runCheckGetNdbErrorOperation(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);
  
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    ndbout << "Could not start transaction" << endl;
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }
  
  // Dont call readTuple here
  // That's the error!
  
  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	// An error has occured, check that 
	// it's possible to get the NdbErrorOperation
	const NdbError err = pCon->getNdbError();
	NDB_ERR(err);
	if (err.code == 0)
	  result = NDBT_FAILED;

	NdbOperation* pOp2 = pCon->getNdbErrorOperation();
	if (pOp2 == NULL)
	  result = NDBT_FAILED;
	else {
	  const NdbError err2 = pOp2->getNdbError();
	  NDB_ERR(err2);
	  if (err.code == 0)
	    result = NDBT_FAILED;
	}
      }
    }
  }
  
  pNdb->closeTransaction(pCon);
    
  delete pNdb;
  return result;
}

#define C2(x) { int _x= (x); if(_x == 0){ ndbout << "line: " << __LINE__ << endl;  return NDBT_FAILED;} }

int runBug_11133(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  HugoOperations hugoOps(*pTab);

  Ndb* pNdb = GETNDB(step);
  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkInsertRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkInsertRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkInsertRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkReadRecord(pNdb, 0, 1, NdbOperation::LM_Exclusive) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkWriteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);

  Ndb ndb2(&ctx->m_cluster_connection, "TEST_DB");
  C2(ndb2.init() == 0);
  C2(ndb2.waitUntilReady() == 0);
  HugoOperations hugoOps2(*pTab);  

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkInsertRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps2.startTransaction(&ndb2) == 0);
  C2(hugoOps2.pkWritePartialRecord(&ndb2, 0) == 0);
  C2(hugoOps2.execute_async(&ndb2, NdbTransaction::NoCommit) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps2.wait_async(&ndb2) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);
  C2(hugoOps2.closeTransaction(&ndb2) == 0);  

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps2.startTransaction(&ndb2) == 0);
  C2(hugoOps2.pkWriteRecord(&ndb2, 0, 1) == 0);
  C2(hugoOps2.execute_async(&ndb2, NdbTransaction::NoCommit) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps2.wait_async(&ndb2) == 0);
  C2(hugoOps2.execute_Commit(pNdb) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);
  C2(hugoOps2.closeTransaction(&ndb2) == 0);  

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkUpdateRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps2.startTransaction(&ndb2) == 0);
  C2(hugoOps2.pkWritePartialRecord(&ndb2, 0) == 0);
  C2(hugoOps2.execute_async(&ndb2, NdbTransaction::NoCommit) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps2.wait_async(&ndb2) == 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);
  C2(hugoOps2.closeTransaction(&ndb2) == 0);  

  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkDeleteRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_NoCommit(pNdb) == 0);
  C2(hugoOps2.startTransaction(&ndb2) == 0);
  C2(hugoOps2.pkWritePartialRecord(&ndb2, 0) == 0);
  C2(hugoOps2.execute_async(&ndb2, NdbTransaction::NoCommit) == 0);
  C2(hugoOps.execute_Commit(pNdb) == 0);
  C2(hugoOps2.wait_async(&ndb2) != 0);
  C2(hugoOps.closeTransaction(pNdb) == 0);
  C2(hugoOps2.closeTransaction(&ndb2) == 0);  

  return result;
}

int runBug_WritePartialIgnoreError(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  HugoOperations hugoOps(*pTab);

  Ndb* pNdb = GETNDB(step);
  C2(hugoOps.startTransaction(pNdb) == 0);
  C2(hugoOps.pkWritePartialRecord(pNdb, 0, 1) == 0);
  C2(hugoOps.execute_Commit(pNdb, AO_IgnoreError) == 839);
  C2(hugoOps.closeTransaction(pNdb) == 0);

  return result;
}

int runScan_4006(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const Uint32 max= 5;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(max)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }
  
  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }

  Uint32 i;
  Vector<NdbScanOperation*> scans;
  for(i = 0; i<10*max; i++)
  {
    NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
    if (pOp == NULL){
      NDB_ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);  
      delete pNdb;
      return NDBT_FAILED;
    }
    
    if (pOp->readTuples() != 0){
      pNdb->closeTransaction(pCon);
      NDB_ERR(pOp->getNdbError());
      delete pNdb;
      return NDBT_FAILED;
    }
    scans.push_back(pOp);
  }

  // Dont' call any equal or setValues

  // Execute should not work
  int check = pCon->execute(NoCommit);
  if (check == 0){
    ndbout << "execute worked" << endl;
  } else {
    NDB_ERR(pCon->getNdbError());
  }
  
  for(i= 0; i<scans.size(); i++)
  {
    NdbScanOperation* pOp= scans[i];
    while((check= pOp->nextResult()) == 0);
    if(check != 1)
    {
      NDB_ERR(pOp->getNdbError());
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }
  }
  
  pNdb->closeTransaction(pCon);  

  Vector<NdbConnection*> cons;
  for(i= 0; i<10*max; i++)
  {
    pCon= pNdb->startTransaction();
    if(pCon)
      cons.push_back(pCon);
    else
      break;
  }
  
  for(i= 0; i<cons.size(); i++)
  {
    cons[i]->close();
  }
  
  if(cons.size() != max)
  {
    result= NDBT_FAILED;
  }
  
  delete pNdb;
  
  return result;
}

char pkIdxName[255];

int createPkIndex(NDBT_Context* ctx, NDBT_Step* step){
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);

  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  bool logged = ctx->getProperty("LoggedIndexes", 1);

  // Create index    
  BaseString::snprintf(pkIdxName, 255, "IDC_PK_%s", pTab->getName());
  if (orderedIndex)
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "ordered index "
	   << pkIdxName << " (";
  else
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "unique index "
	   << pkIdxName << " (";

  NdbDictionary::Index pIdx(pkIdxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey()){
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() <<" ";
    }
  }
  
  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    return NDBT_FAILED;
  }

  ndbout << "OK!" << endl;
  return NDBT_OK;
}

int createPkIndex_Drop(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  // Drop index
  ndbout << "Dropping index " << pkIdxName << " ";
  if (pNdb->getDictionary()->dropIndex(pkIdxName, 
				       pTab->getName()) != 0){
    ndbout << "FAILED!" << endl;
    NDB_ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  
  return NDBT_OK;
}

static
int
op_row(NdbTransaction* pTrans, HugoOperations& hugoOps,
       const NdbDictionary::Table* pTab, int op, int row)
{
  NdbOperation * pOp = 0;
  switch(op){
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 12:
    pOp = pTrans->getNdbOperation(pTab->getName());
    break;
  case 9:
    return 0;
  case 6:
  case 7:
  case 8:
  case 10:
  case 11:
    pOp = pTrans->getNdbIndexOperation(pkIdxName, pTab->getName());
  default:
    break;
  }
  
  switch(op){
  case 0:
  case 6:
    pOp->readTuple();
    break;
  case 1:
  case 7:
    pOp->committedRead();
    break;
  case 2:
  case 8:
    pOp->readTupleExclusive();
    break;
  case 3:
  case 9:
    pOp->insertTuple();
    break;
  case 4:
  case 10:
    pOp->updateTuple();
    break;
  case 5:
  case 11:
    pOp->deleteTuple();
    break;
  case 12:
    CHECK(!pOp->simpleRead());
    break;
  default:
    abort();
  }

  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(pOp, a, row) != 0){
	return NDBT_FAILED;
      }
    }
  }

  switch(op){
  case 0:
  case 1:
  case 2:
  case 6:
  case 7:
  case 8:
  case 12:
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      CHECK(pOp->getValue(a));
    }
    break;
  case 3: 
  case 4:
  case 10:
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      if (pTab->getColumn(a)->getPrimaryKey() == false){
	if(hugoOps.setValueForAttr(pOp, a, row, 2) != 0){
	  return NDBT_FAILED;
	}
      }
    }
    break;
  case 5:
  case 11:
    pOp->deleteTuple();
    break;
  case 9:
  default:
    abort();
  }
  
  return NDBT_OK;
}

static void print(int op)
{
  const char * str = 0;
  switch(op){
  case 0:  str = "pk read-sh"; break;
  case 1:  str = "pk read-nl"; break;
  case 2:  str = "pk read-ex"; break;
  case 3:  str = "pk insert "; break;
  case 4:  str = "pk update "; break;
  case 5:  str = "pk delete "; break;
  case 6:  str = "uk read-sh"; break;
  case 7:  str = "uk read-nl"; break;
  case 8:  str = "uk read-ex"; break;
  case 9:  str = "noop      "; break;
  case 10: str = "uk update "; break;
  case 11: str = "uk delete "; break;
  case 12: str = "pk read-si"; break;

  default:
    abort();
  }
  printf("%s ", str);
}

int
runTestIgnoreError(NDBT_Context* ctx, NDBT_Step* step)
{
  Uint32 loops = ctx->getNumRecords();
  const NdbDictionary::Table* pTab = ctx->getTab();

  HugoOperations hugoOps(*pTab);
  HugoTransactions hugoTrans(*pTab);

  Ndb* pNdb = GETNDB(step);

  struct {
    ExecType et;
    AbortOption ao;
  } tests[] = {
    { Commit, AbortOnError },
    { Commit, AO_IgnoreError },
    { NoCommit, AbortOnError },
    { NoCommit, AO_IgnoreError },
  };

  printf("case: <op1>     <op2>       c/nc ao/ie\n");
  Uint32 tno = 0;
  for (Uint32 op1 = 0; op1 < 13; op1++)
  {
    // NOTE : I get a node crash if the following loop starts from 0!
    for (Uint32 op2 = op1; op2 < 13; op2++)
    {
      int ret;
      NdbTransaction* pTrans = 0;
      
      for (Uint32 i = 0; i<4; i++, tno++)
      {
	if (loops != 1000 && loops != tno)
	  continue;
	ExecType et = tests[i].et;
	AbortOption ao = tests[i].ao;
	
	printf("%.3d : ", tno);
	print(op1);
	print(op2);
	switch(et){
	case Commit: printf("c    "); break;
	case NoCommit: printf("nc   "); break;
        default: printf("bad exectype : %d\n", et); return NDBT_FAILED;
	}
	switch(ao){
	case AbortOnError: printf("aoe  "); break;
	case AO_IgnoreError: printf("ie   "); break;
        default: printf("bad abortoption : %d\n", ao); return NDBT_FAILED;
	}
	printf(": ");
	

	hugoTrans.loadTable(pNdb, 1);
	CHECK(pTrans = pNdb->startTransaction());
	CHECK(!op_row(pTrans, hugoOps, pTab, op1, 0));
	ret = pTrans->execute(et, ao);
	pTrans->close();
	printf("%d ", ret);
	hugoTrans.clearTable(pNdb);

	hugoTrans.loadTable(pNdb, 1);
	CHECK(pTrans = pNdb->startTransaction());
	CHECK(!op_row(pTrans, hugoOps, pTab, op1, 1));
	ret = pTrans->execute(et, ao);
	pTrans->close();
	printf("%d ", ret);
	hugoTrans.clearTable(pNdb);
      
	hugoTrans.loadTable(pNdb, 1);
	CHECK(pTrans = pNdb->startTransaction());
	CHECK(!op_row(pTrans, hugoOps, pTab, op1, 0));
	CHECK(!op_row(pTrans, hugoOps, pTab, op2, 1));
	ret = pTrans->execute(et, ao);
	pTrans->close();
	printf("%d\n", ret);
	hugoTrans.clearTable(pNdb);
	
	hugoTrans.clearTable(pNdb);
      }
    }
  }
  return NDBT_OK;
}

static
Uint32
do_cnt(Ndb_cluster_connection* con)
{
  Uint32 cnt = 0;
  const Ndb* p = 0;
  con->lock_ndb_objects();
  while ((p = con->get_next_ndb_object(p)) != 0) cnt++;
  con->unlock_ndb_objects();
  return cnt;
}

int runCheckNdbObjectList(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb_cluster_connection* con = &ctx->m_cluster_connection;
  
  Uint32 cnt1 = do_cnt(con);
  Vector<Ndb*> objs;
  for (Uint32 i = 0; i<100; i++)
  {
    Uint32 add = 1 + (rand() % 5);
    for (Uint32 j = 0; j<add; j++)
    {
      Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
      if (pNdb == NULL){
	ndbout << "pNdb == NULL" << endl;      
	return NDBT_FAILED;  
      }
      objs.push_back(pNdb);
    }
    if (do_cnt(con) != (cnt1 + objs.size()))
      return NDBT_FAILED;
  }
  
  for (Uint32 i = 0; i<100 && objs.size(); i++)
  {
    Uint32 sub = 1 + rand() % objs.size();
    for (Uint32 j = 0; j<sub && objs.size(); j++)
    {
      Uint32 idx = rand() % objs.size();
      delete objs[idx];
      objs.erase(idx);
    }
    if (do_cnt(con) != (cnt1 + objs.size()))
      return NDBT_FAILED;
  }
  
  for (Uint32 i = 0; i<objs.size(); i++)
    delete objs[i];
  
  return (cnt1 == do_cnt(con)) ? NDBT_OK : NDBT_FAILED;
}


static Ndb_cluster_connection* g_cluster_connection;

int runNdbClusterConnectionDelete_connection_owner(NDBT_Context* ctx,
                                                   NDBT_Step* step)
{
  // Get connectstring from main connection
  char constr[256];
  if (!ctx->m_cluster_connection.get_connectstring(constr,
                                                   sizeof(constr)))
  {
    g_err << "Too short buffer for connectstring" << endl;
    return NDBT_FAILED;
  }

  // Create a new cluster connection, connect it and assign
  // to pointer so the other thread can access it.
  Ndb_cluster_connection* con = new Ndb_cluster_connection(constr);

  const int retries = 12;
  const int retry_delay = 5;
  const int verbose = 1;
  if (con->connect(retries, retry_delay, verbose) != 0)
  {
    delete con;
    g_err << "Ndb_cluster_connection.connect failed" << endl;
    return NDBT_FAILED;
  }

  g_cluster_connection = con;

  // Signal other thread that cluster connection has been creted
  ctx->setProperty("CREATED", 1);

  // Now wait for the other thread to use the connection
  // until it signals this thread to continue and
  // delete the cluster connection(since the
  // other thread still have live Ndb objects created
  // in the connection, this thread should hang in
  // the delete until other thread has finished cleaning up)
  ctx->getPropertyWait("CREATED", 2);

  g_cluster_connection = NULL;
  delete con;

  return NDBT_OK;
}

int runNdbClusterConnectionDelete_connection_user(NDBT_Context* ctx, NDBT_Step* step)
{
  // Wait for the cluster connection to be created by other thread
  ctx->getPropertyWait("CREATED", 1);

  Ndb_cluster_connection* con = g_cluster_connection;

  // Create some Ndb objects and start transactions
  class ActiveTransactions
  {
    Vector<NdbTransaction*> m_transactions;

  public:
    void release()
    {
      while(m_transactions.size())
      {
        NdbTransaction* trans = m_transactions[0];
        Ndb* ndb = trans->getNdb();
        g_info << "Deleting Ndb object " << ndb <<
                  "and transaction " << trans << endl;
        ndb->closeTransaction(trans);
        delete ndb;
        m_transactions.erase(0);
      }
      // The list should be empty
      assert(m_transactions.size() == 0);
    }

    ~ActiveTransactions()
    {
      release();
    }

    void push_back(NdbTransaction* trans)
    {
      m_transactions.push_back(trans);
    }
  } active_transactions;

  g_info << "Creating Ndb objects and transactions.." << endl;
  for (Uint32 i = 0; i<100; i++)
  {
    Ndb* ndb = new Ndb(con, "TEST_DB");
    if (ndb == NULL){
      g_err << "ndb == NULL" << endl;
      return NDBT_FAILED;
    }
    if (ndb->init(256) != 0){
      NDB_ERR(ndb->getNdbError());
      delete ndb;
      return NDBT_FAILED;
    }

    if (ndb->waitUntilReady() != 0){
      NDB_ERR(ndb->getNdbError());
      delete ndb;
      return NDBT_FAILED;
    }

    NdbTransaction* trans = ndb->startTransaction();
    if (trans == NULL){
      g_err << "trans == NULL" << endl;
      NDB_ERR(ndb->getNdbError());
      delete ndb;
      return NDBT_FAILED;
    }

    active_transactions.push_back(trans);
  }
  g_info << "  ok!" << endl;

  // Signal to cluster connection owner that Ndb objects have been created
  ctx->setProperty("CREATED", 2);

  // Delay a little and then start closing transactions and
  // deleting the Ndb objects
  NdbSleep_SecSleep(1);

  g_info << "Releasing transactions and related Ndb objects..." << endl;
  active_transactions.release();
  g_info << "  ok!" << endl;
  return NDBT_OK;
}


  
static void
testExecuteAsynchCallback(int res, NdbTransaction *con, void *data_ptr)
{
  int *res_ptr= (int *)data_ptr;

  *res_ptr= res;
}

int runTestExecuteAsynch(NDBT_Context* ctx, NDBT_Step* step){
  /* Test that NdbTransaction::executeAsynch() works (BUG#27495). */
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init(2048)){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
  if (pOp == NULL){
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }

  if (pOp->readTuples() != 0){
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::FRAGMENT) == 0){
    NDB_ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }
  int res= 42;
  pCon->executeAsynch(NoCommit, testExecuteAsynchCallback, &res);
  while(pNdb->pollNdb(100000) == 0)
    ;
  if (res != 0){
    NDB_ERR(pCon->getNdbError());
    ndbout << "Error returned from execute: " << res << endl;
    result= NDBT_FAILED;
  }

  pNdb->closeTransaction(pCon);

  delete pNdb;

  return result;
}

template class Vector<NdbScanOperation*>;

int 
runBug28443(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  
  NdbRestarter restarter;

  restarter.insertErrorInAllNodes(9003);

  for (int i = 0; i<ctx->getNumLoops(); i++)
  {
    HugoTransactions hugoTrans(*ctx->getTab());
    if (hugoTrans.loadTable(GETNDB(step), records, 2048) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
    if (runClearTable(ctx, step) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
  }
  
done:
  restarter.insertErrorInAllNodes(9003);

  return result;
}

int 
runBug37158(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  Ndb* pNdb = GETNDB(step);

  for (int i = 0; i<ctx->getNumLoops(); i++)
  {
    HugoOperations hugoOps(*ctx->getTab());
    hugoOps.startTransaction(pNdb);
    if (hugoOps.pkWriteRecord(pNdb, 0) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
    

    if (hugoOps.pkWritePartialRecord(pNdb, 1) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
    
    if (hugoOps.pkWriteRecord(pNdb, 2) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
    
    if (hugoOps.pkUpdateRecord(pNdb, 0) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
    
    if (hugoOps.execute_Commit(pNdb, AO_IgnoreError) == 4011)
    {
      result = NDBT_FAILED;
      goto done;
    }
    hugoOps.closeTransaction(pNdb);

    if (runClearTable(ctx, step) != 0)
    {
      result = NDBT_FAILED;
      goto done;
    }
  }
  
done:

  return result;
}

int
simpleReadAbortOnError(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Simple read has some error handling issues
   * Setting the operation to be AbortOnError can expose these
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  HugoOperations hugoOps(*pTab);
  NdbRestarter restarter;

  hugoOps.startTransaction(pNdb);
  CHECK(!hugoOps.pkWriteRecord(pNdb,0));
  CHECK(!hugoOps.execute_Commit(pNdb, AbortOnError));

  NdbTransaction* trans;
  
  CHECK(trans= pNdb->startTransaction());

  /* Insert error 5047 which causes next LQHKEYREQ to fail due
   * to 'transporter overload'
   * Error insert is self-clearing
   */
  restarter.insertErrorInAllNodes(5047);

  /* Create SimpleRead on row 0, which exists (though we'll get
   * 'transporter overload for this'
   */
  NdbOperation* op;
  CHECK(op= trans->getNdbOperation(pTab));

  CHECK(!op->simpleRead());

  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(op, a, 0) != 0){
        restarter.insertErrorInAllNodes(0);  
	return NDBT_FAILED;
      }
    }
  }
  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    CHECK(op->getValue(a));
  }
  
  CHECK(!op->setAbortOption(NdbOperation::AbortOnError));

  /* Create normal read on row 0 which will succeed */
  NdbOperation* op2;
  CHECK(op2= trans->getNdbOperation(pTab));

  CHECK(!op2->readTuple());

  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(op2, a, 0) != 0){
        restarter.insertErrorInAllNodes(0);  
	return NDBT_FAILED;
      }
    }
  }
  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    CHECK(op2->getValue(a));
  }
  
  CHECK(!op2->setAbortOption(NdbOperation::AbortOnError));


  CHECK(trans->execute(NoCommit) == -1);

  CHECK(trans->getNdbError().code == 1218); // Transporter Overload

  restarter.insertErrorInAllNodes(0);  

  return NDBT_OK;
  
}


int
testNdbRecordPkAmbiguity(NDBT_Context* ctx, NDBT_Step* step)
{
  /* NdbRecord Insert and Write can take 2 record and row ptrs
   * In all cases, the AttrInfo sent to TC for PK columns
   * should be the same as the KeyInfo sent to TC to avoid
   * inconsistency
   * Approach :
   *   1) Use Insert/Write to insert tuple with different 
   *      values for pks in attr row
   *   2) Read back all data, including PKs
   *   3) Verify all values.
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  const NdbRecord* tabRec= pTab->getDefaultRecord();
  const Uint32 sizeOfTabRec= NdbDictionary::getRecordRowLength(tabRec);
  char keyRowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
  char attrRowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
  bzero(keyRowBuf, sizeof(keyRowBuf));
  bzero(attrRowBuf, sizeof(attrRowBuf));

  HugoCalculator calc(*pTab);

  const int numRecords= 100;

  for (int optype=0; optype < 2; optype++)
  {
    /* First, let's calculate the correct Hugo values for this row */

    for (int record=0; record < numRecords; record++)
    {
      int updates= 0;
      for (int col=0; col<pTab->getNoOfColumns(); col++)
      {
        char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                                 keyRowBuf,
                                                 col);
        CHECK(valPtr != NULL);
        
        int len= pTab->getColumn(col)->getSizeInBytes();
        Uint32 real_len;
        bool isNull= (calc.calcValue(record, col, updates, valPtr,
                                     len, &real_len) == NULL);
        if (pTab->getColumn(col)->getNullable())
        {
          NdbDictionary::setNull(tabRec,
                                 keyRowBuf,
                                 col,
                                 isNull);
        }
      }
      
      /* Now copy the values to the Attr record */
      memcpy(attrRowBuf, keyRowBuf, sizeOfTabRec);
      
      Uint32 mippleAttempts= 3;
      
      while (memcmp(keyRowBuf, attrRowBuf, sizeOfTabRec) == 0)
      {
        /* Now doctor the PK values in the Attr record */
        for (int col=0; col<pTab->getNoOfColumns(); col++)
        {
          if (pTab->getColumn(col)->getPrimaryKey())
          {
            char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                                     attrRowBuf,
                                                     col);
            CHECK(valPtr != NULL);
            
            int len= pTab->getColumn(col)->getSizeInBytes();
            Uint32 real_len;
            /* We use the PK value for some other record */
            int badRecord= record + (rand() % 1000);
            bool isNull= (calc.calcValue(badRecord, col, updates, valPtr,
                                         len, &real_len) == NULL);
            CHECK(! isNull);
          }
        }
        
        /* Can try to get variance only a limited number of times */
        CHECK(mippleAttempts-- != 0);
      }
      
      /* Ok, now have key and attr records with different values for
       * PK cols, let's try to insert
       */
      NdbTransaction* trans=pNdb->startTransaction();
      CHECK(trans != 0);
      
      const NdbOperation* op= NULL;
      if (optype == 0)
      {
        // ndbout << "Using insertTuple" << endl;
        op= trans->insertTuple(tabRec,
                               keyRowBuf,
                               tabRec,
                               attrRowBuf);
      }
      else
      {
        // ndbout << "Using writeTuple" << endl;
        op= trans->writeTuple(tabRec,
                              keyRowBuf,
                              tabRec,
                              attrRowBuf);
      }
      CHECK(op != 0);
      
      CHECK(trans->execute(Commit) == 0);
      trans->close();
      
      /* Now read back */
      memset(attrRowBuf, 0, sizeOfTabRec);
      
      Uint32 pkVal= 0;
      memcpy(&pkVal, NdbDictionary::getValuePtr(tabRec,
                                                keyRowBuf,
                                                0),
             sizeof(pkVal));

      trans= pNdb->startTransaction();
      op= trans->readTuple(tabRec,
                           keyRowBuf,
                           tabRec,
                           attrRowBuf);
      CHECK(op != 0);
      CHECK(trans->execute(Commit) == 0);
      CHECK(trans->getNdbError().code == 0);
      trans->close();
      
      /* Verify the values read back */
      for (int col=0; col<pTab->getNoOfColumns(); col++)
      {
        const char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                                       attrRowBuf,
                                                       col);
        CHECK(valPtr != NULL);
        
        char calcBuff[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
        int len= pTab->getColumn(col)->getSizeInBytes();
        Uint32 real_len;
        bool isNull= (calc.calcValue(record, col, updates, calcBuff,
                                     len, &real_len) == NULL);
        bool colIsNullable= pTab->getColumn(col)->getNullable();
        if (isNull)
        {
          CHECK(colIsNullable);
          if (!NdbDictionary::isNull(tabRec,
                                     attrRowBuf,
                                     col))
          {
            ndbout << "Error, col " << col 
                   << " (pk=" <<  pTab->getColumn(col)->getPrimaryKey()
                   << ") should be Null, but is not" << endl;
            return NDBT_FAILED;
          }
        }
        else
        {
          if (colIsNullable)
          {
            if (NdbDictionary::isNull(tabRec,
                                      attrRowBuf,
                                      col))
            {
              ndbout << "Error, col " << col 
                     << " (pk=" << pTab->getColumn(col)->getPrimaryKey()
                     << ") should be non-Null but is null" << endl;
              return NDBT_FAILED;
            };
          }
          
          /* Compare actual data read back */
          if( memcmp(calcBuff, valPtr, real_len) != 0 )
          {
            ndbout << "Error, col " << col 
                   << " (pk=" << pTab->getColumn(col)->getPrimaryKey()
                   << ") should be equal, but isn't for record "
                   << record << endl;
            ndbout << "Expected :";
            for (Uint32 i=0; i < real_len; i++)
            {
              ndbout_c("%x ", calcBuff[i]);
            }
            ndbout << endl << "Received :";
            for (Uint32 i=0; i < real_len; i++)
            {
              ndbout_c("%x ", valPtr[i]);
            }
            ndbout << endl;
            
            return NDBT_FAILED;
          }
        }
      }
      
      /* Now delete the tuple */
      trans= pNdb->startTransaction();
      op= trans->deleteTuple(tabRec,
                             keyRowBuf,
                             tabRec);
      CHECK(op != 0);
      CHECK(trans->execute(Commit) == 0);
      
      trans->close();
    }
  }

  return NDBT_OK;
  
}

int
testNdbRecordPKUpdate(NDBT_Context* ctx, NDBT_Step* step)
{
  /* In general, we should be able to update primary key
   * values.  We cannot *change* them, but for cases where
   * a collation maps several discrete values to a single
   * normalised value, it should be possible to modify
   * the discrete value of the key, as the normalised 
   * key value is unchanged.
   * Rather than testing with such a collation here, we 
   * cop out and test for errors with a 'null' change.
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  const NdbRecord* tabRec= pTab->getDefaultRecord();
  char rowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
  char badKeyRowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];

  HugoCalculator calc(*pTab);

  const int numRecords= 100;

  /* First, let's calculate the correct Hugo values for this row */
  for (int record=0; record < numRecords; record++)
  {
    int updates= 0;
    for (int col=0; col<pTab->getNoOfColumns(); col++)
    {
      char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                               rowBuf,
                                               col);
      CHECK(valPtr != NULL);
      
      int len= pTab->getColumn(col)->getSizeInBytes();
      Uint32 real_len;
      bool isNull= (calc.calcValue(record, col, updates, valPtr,
                                   len, &real_len) == NULL);
      if (pTab->getColumn(col)->getNullable())
      {
        NdbDictionary::setNull(tabRec,
                               rowBuf,
                               col,
                               isNull);
      }      
    }

    /* Create similar row, but with different id col (different
     * PK from p.o.v. of PK column update
     */
    memcpy(badKeyRowBuf, rowBuf, NDB_MAX_TUPLE_SIZE_IN_WORDS << 2);
    for (int col=0; col<pTab->getNoOfColumns(); col++)
    {
      if (calc.isIdCol(col))
      {
        char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                                 badKeyRowBuf,
                                                 col);
        Uint32 badId= record+333;
        memcpy(valPtr, &badId, sizeof(badId));
      }
    }

    NdbTransaction* trans=pNdb->startTransaction();
    CHECK(trans != 0);
    
    const NdbOperation* op= trans->insertTuple(tabRec,
                                               rowBuf);
    CHECK(op != 0);
    
    CHECK(trans->execute(Commit) == 0);
    trans->close();
    
    /* Now update the PK columns */
    trans= pNdb->startTransaction();
    op= trans->updateTuple(tabRec,
                           rowBuf,
                           tabRec,
                           rowBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    CHECK(trans->getNdbError().code == 0);
    trans->close();

    /* Now update PK with scan takeover op */
    trans= pNdb->startTransaction();

    NdbScanOperation* scanOp=trans->scanTable(tabRec,
                                              NdbOperation::LM_Exclusive);
    CHECK(scanOp != 0);
    
    CHECK(trans->execute(NoCommit) == 0);
    
    /* Now update PK with lock takeover op */
    const char* rowPtr;
    CHECK(scanOp->nextResult(&rowPtr, true, true) == 0);
    
    op= scanOp->updateCurrentTuple(trans,
                                   tabRec,
                                   rowBuf);
    CHECK(op != NULL);
    
    CHECK(trans->execute(Commit) == 0);
    
    trans->close();

    /* Now attempt bad PK update with lock takeover op 
     * This is interesting as NDBAPI normally takes the
     * value of PK columns in an update from the key
     * row - so it's not possible to pass a 'different'
     * value (except when collations are used).
     * Scan Takeover update takes the PK values from the
     * attribute record and so different values can 
     * be supplied.
     * Here we check that different values result in the
     * kernel complaining.
     */
    trans= pNdb->startTransaction();

    scanOp=trans->scanTable(tabRec,
                            NdbOperation::LM_Exclusive);
    CHECK(scanOp != 0);
    
    CHECK(trans->execute(NoCommit) == 0);
    
    /* Now update PK with lock takeover op */
    CHECK(scanOp->nextResult(&rowPtr, true, true) == 0);
    
    op= scanOp->updateCurrentTuple(trans,
                                   tabRec,
                                   badKeyRowBuf);
    CHECK(op != NULL);
    
    CHECK(trans->execute(Commit) == -1);
    CHECK(trans->getNdbError().code == 897);

    trans->close();

    /* Now delete the tuple */
    trans= pNdb->startTransaction();
    op= trans->deleteTuple(tabRec,
                           rowBuf,
                           tabRec);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    
    trans->close();
  }

  return NDBT_OK;
  
}

static 
BaseString getKeyVal(int record, bool upper)
{
  /* Create VARCHAR format key with upper or
   * lower case leading char
   */
  BaseString keyData;
  char c= 'a' + (record % ('z' - 'a'));
  
  keyData.appfmt("%cblahblah%d", c, record);
  
  if (upper)
    keyData.ndb_toupper();

  BaseString varCharKey;
  varCharKey.appfmt("%c%s", keyData.length(), keyData.c_str());
  
  return varCharKey;
}

int
testNdbRecordCICharPKUpdate(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Test a change to a CHAR primary key with a case insensitive
   * collation.
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  
  /* Run as a 'T1' testcase - do nothing for other tables */
  if (strcmp(pTab->getName(), "T1") != 0)
    return NDBT_OK;

  CHARSET_INFO* charset= NULL;
  const char* csname="latin1_general_ci";
  charset= get_charset_by_name(csname, MYF(0));
  
  if (charset == NULL)
  {
    ndbout << "Couldn't get charset " << csname << endl;
    return NDBT_FAILED;
  }

  /* Create table with required schema */
  NdbDictionary::Table tab;
  tab.setName("TAB_CICHARPKUPD");
  
  NdbDictionary::Column pk;
  pk.setName("PK");
  pk.setType(NdbDictionary::Column::Varchar);
  pk.setLength(20);
  pk.setNullable(false);
  pk.setPrimaryKey(true);
  pk.setCharset(charset);
  tab.addColumn(pk);

  NdbDictionary::Column data;
  data.setName("DATA");
  data.setType(NdbDictionary::Column::Unsigned);
  data.setNullable(false);
  data.setPrimaryKey(false);
  tab.addColumn(data);

  pNdb->getDictionary()->dropTable(tab.getName());
  if(pNdb->getDictionary()->createTable(tab) != 0)
  {
    ndbout << "Create table failed with error : "
           << pNdb->getDictionary()->getNdbError().code
           << pNdb->getDictionary()->getNdbError().message
           << endl;
    return NDBT_FAILED;
  }
  
  ndbout << (NDBT_Table&)tab << endl;

  pTab= pNdb->getDictionary()->getTable(tab.getName());
  
  const NdbRecord* tabRec= pTab->getDefaultRecord();
  const Uint32 rowLen= NDB_MAX_TUPLE_SIZE_IN_WORDS << 2;
  char ucRowBuf[ rowLen ];
  char lcRowBuf[ rowLen ];
  char readBuf[ rowLen ];
  char* ucPkPtr= NdbDictionary::getValuePtr(tabRec,
                                            ucRowBuf,
                                            0);
  Uint32* ucDataPtr= (Uint32*) NdbDictionary::getValuePtr(tabRec,
                                                          ucRowBuf,
                                                          1);
  char* lcPkPtr= NdbDictionary::getValuePtr(tabRec,
                                            lcRowBuf,
                                            0);
  Uint32* lcDataPtr= (Uint32*) NdbDictionary::getValuePtr(tabRec,
                                                          lcRowBuf,
                                                          1);

  char* readPkPtr= NdbDictionary::getValuePtr(tabRec,
                                              readBuf,
                                              0);
  Uint32* readDataPtr= (Uint32*) NdbDictionary::getValuePtr(tabRec,
                                                            readBuf,
                                                            1);
    

  const int numRecords= 100;
  BaseString upperKey;
  BaseString lowerKey;

  for (int record=0; record < numRecords; record++)
  {
    upperKey.assign(getKeyVal(record, true).c_str());
    lowerKey.assign(getKeyVal(record, false).c_str());
    
    memcpy(ucPkPtr, upperKey.c_str(), upperKey.length());
    memcpy(lcPkPtr, lowerKey.c_str(), lowerKey.length());
    memcpy(ucDataPtr, &record, sizeof(record));
    memcpy(lcDataPtr, &record, sizeof(record));

    /* Insert with upper case */
    NdbTransaction* trans=pNdb->startTransaction();
    CHECK(trans != 0);
    
    const NdbOperation* op= trans->insertTuple(tabRec,
                                               ucRowBuf);
    CHECK(op != 0);
    
    int rc= trans->execute(Commit);
    if (rc != 0)
      ndbout << "Error " << trans->getNdbError().message << endl;
    CHECK(rc == 0);
    trans->close();

    /* Read with upper case */
    trans=pNdb->startTransaction();
    CHECK(trans != 0);
    op= trans->readTuple(tabRec,
                         ucRowBuf,
                         tabRec,
                         readBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    trans->close();

    /* Check key and data read */
    CHECK(memcmp(ucPkPtr, readPkPtr, ucPkPtr[0]) == 0);
    CHECK(memcmp(ucDataPtr, readDataPtr, sizeof(int)) == 0);
    
    memset(readBuf, 0, NDB_MAX_TUPLE_SIZE_IN_WORDS << 2);

    /* Read with lower case */
    trans=pNdb->startTransaction();
    CHECK(trans != 0);
    op= trans->readTuple(tabRec,
                         lcRowBuf,
                         tabRec,
                         readBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    trans->close();

    /* Check key and data read */
    CHECK(memcmp(ucPkPtr, readPkPtr, ucPkPtr[0]) == 0);
    CHECK(memcmp(ucDataPtr, readDataPtr, sizeof(int)) == 0);
    
    memset(readBuf, 0, NDB_MAX_TUPLE_SIZE_IN_WORDS << 2);

    /* Now update just the PK column to lower case */
    trans= pNdb->startTransaction();
    unsigned char mask[1];
    mask[0]= 1;
    op= trans->updateTuple(tabRec,
                           lcRowBuf,
                           tabRec,
                           lcRowBuf,
                           mask);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    CHECK(trans->getNdbError().code == 0);
    trans->close();

    /* Now check that we can read with the upper case key */
    memset(readBuf, 0, NDB_MAX_TUPLE_SIZE_IN_WORDS << 2);
    
    trans=pNdb->startTransaction();
    CHECK(trans != 0);
    op= trans->readTuple(tabRec,
                         ucRowBuf,
                         tabRec,
                         readBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    trans->close();

    /* Check key and data read */
    CHECK(memcmp(lcPkPtr, readPkPtr, lcPkPtr[0]) == 0);
    CHECK(memcmp(lcDataPtr, readDataPtr, sizeof(int)) == 0);

    /* Now check that we can read with the lower case key */
    memset(readBuf, 0, NDB_MAX_TUPLE_SIZE_IN_WORDS << 2);
    
    trans=pNdb->startTransaction();
    CHECK(trans != 0);
    op= trans->readTuple(tabRec,
                         lcRowBuf,
                         tabRec,
                         readBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    trans->close();

    /* Check key and data read */
    CHECK(memcmp(lcPkPtr, readPkPtr, lcPkPtr[0]) == 0);
    CHECK(memcmp(lcDataPtr, readDataPtr, sizeof(int)) == 0);


    /* Now delete the tuple */
    trans= pNdb->startTransaction();
    op= trans->deleteTuple(tabRec,
                           ucRowBuf,
                           tabRec);
     CHECK(op != 0);
     CHECK(trans->execute(Commit) == 0);
   
     trans->close();
  }

  pNdb->getDictionary()->dropTable(tab.getName());

  return NDBT_OK;
  
}

int
testNdbRecordRowLength(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Bug#43891 ignored null bits at the end of an row
   * when calculating the row length, leading to various
   * problems
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  int numCols= pTab->getNoOfColumns();
  const NdbRecord* defaultRecord= pTab->getDefaultRecord();

  /* Create an NdbRecord structure with all the Null
   * bits at the end - to test that they are included
   * correctly in row length calculations.
   */
  NdbDictionary::RecordSpecification rsArray[ NDB_MAX_ATTRIBUTES_IN_TABLE ];

  bool hasNullable= false;
  Uint32 highestUsed= 9000;
  for (int attrId=0; attrId< numCols; attrId++)
  {
    NdbDictionary::RecordSpecification& rs= rsArray[attrId];
    
    rs.column= pTab->getColumn(attrId);
    CHECK(NdbDictionary::getOffset(defaultRecord,
                                   attrId,
                                   rs.offset));
    CHECK(NdbDictionary::getNullBitOffset(defaultRecord,
                                          attrId,
                                          rs.nullbit_byte_offset,
                                          rs.nullbit_bit_in_byte));
    if (rs.column->getNullable())
    {
      /* Shift null bit(s) to bytes beyond the end of the record */
      hasNullable= true;
      rs.nullbit_byte_offset= highestUsed++;
      rs.nullbit_bit_in_byte= 0;
    }
  }
  
  if (hasNullable)
  {
    printf("Testing");
    const NdbRecord* myRecord= pNdb->getDictionary()->createRecord(pTab,
                                                                   rsArray,
                                                                   numCols,
                                                                   sizeof(NdbDictionary::RecordSpecification));
    CHECK(myRecord != 0);
    Uint32 rowLength= NdbDictionary::getRecordRowLength(myRecord);
    if (rowLength != highestUsed)
    {
      ndbout << "Failure, expected row length " << highestUsed
             << " got row length " << rowLength
             << endl;
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}

int
runBug44015(NDBT_Context* ctx, NDBT_Step* step)
{
  /* testNdbApi -n WeirdAssertFail
   * Generates phrase "here2" on 6.3 which is 
   * output by DbtupExecQuery::handleReadReq()
   * detecting that the record's tuple checksum
   * is incorrect.
   * Later can generate assertion failure in 
   * prepare_read
   *         ndbassert(src_len >= (dynstart - src_data));
   * resulting in node failure
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  
  int numIterations= 100;
  int numRecords= 1024;
  
  NdbTransaction* trans;
  HugoOperations hugoOps(*pTab);
  
  for (int iter=0; iter < numIterations; iter++)
  {
    ndbout << "Iter : " << iter << endl;
    CHECK((trans= pNdb->startTransaction()) != 0);
    
    CHECK(hugoOps.setTransaction(trans) == 0);
    
    CHECK(hugoOps.pkInsertRecord(pNdb,
                                 0,
                                 numRecords) == 0);
    
    /* Now execute the transaction */
    if ((trans->execute(NdbTransaction::NoCommit) != 0))
    {
      ndbout << "Execute failed, error is " 
             << trans->getNdbError().code << " "
             << trans->getNdbError().message << endl;
      CHECK(0);
    }

    CHECK(trans->getNdbError().code == 0);
    
    /* Now delete the records in the same transaction
     * Need to do this manually as Hugo doesn't support it
     */
    CHECK(hugoOps.pkDeleteRecord(pNdb,
                                 0,
                                 numRecords) == 0);
    
    CHECK(trans->execute(NdbTransaction::NoCommit) == 0);
    CHECK(trans->getNdbError().code == 0);
    
    /* Now abort the transaction by closing it */
    trans->close();

    /* Force Hugo Transaction back to NULL */
    hugoOps.setTransaction(NULL, true);
  }

  ctx->stopTest();

  return NDBT_OK;
}

int runScanReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int i = 0;
  int scan_flags = NdbScanOperation::SF_TupScan;
  NdbOperation::LockMode lm = 
    (NdbOperation::LockMode)
    ctx->getProperty("ReadLockMode", (Uint32)NdbOperation::LM_CommittedRead);

  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), 0, 0, 0,
                                  lm, scan_flags) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int
runBug44065_org(NDBT_Context* ctx, NDBT_Step* step)
{
  /* testNdbApi -n WeirdAssertFail2
   * Results in assertion failure in DbtupCommit::execTUP_DEALLOCREQ()
   *   ndbassert(ptr->m_header_bits & Tuple_header::FREE);
   * Results in node failure
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  
  int numOuterIterations= 50;
  int numInnerIterations= 20;
  int numRecords= 200;
  
  NdbTransaction* trans;
  
  for (int outerIter=0; outerIter < numOuterIterations; outerIter++)
  {
    HugoOperations hugoOps(*pTab);

    int offset= (outerIter * numRecords);
    ndbout << "Outer Iter : " << outerIter 
           << " " << offset << "-" << (offset + numRecords - 1) << endl;

    {
      HugoTransactions trans(*pTab);
      CHECK(trans.loadTableStartFrom(pNdb, offset, numRecords) == 0);
    }

    for (int iter=0; iter < numInnerIterations; iter++)
    {
      //ndbout << "Inner Iter : " << iter << endl;
      CHECK((trans= pNdb->startTransaction()) != 0);
      
      CHECK(hugoOps.setTransaction(trans) == 0);
      
      /* Delete the records */
      CHECK(hugoOps.pkDeleteRecord(pNdb,
                                   offset,
                                   numRecords) == 0);
      
      /* Re-insert them */
      CHECK(hugoOps.pkInsertRecord(pNdb,
                                   offset,
                                   numRecords) == 0);
      
      /* Now execute the transaction, with IgnoreError */
      if ((trans->execute(NdbTransaction::NoCommit,
                          NdbOperation::AO_IgnoreError) != 0))
      {
        NdbError err = trans->getNdbError();
        ndbout << "Execute failed, error is " 
               << err.code << " " << endl;
        CHECK((err.classification == NdbError::TemporaryResourceError ||
               err.classification == NdbError::OverloadError ||
               err.classification == NdbError::TimeoutExpired));
        NdbSleep_MilliSleep(50);
      }
      
      /* Now abort the transaction by closing it without committing */
      trans->close();
      
      /* Force Hugo Transaction back to NULL */
      hugoOps.setTransaction(NULL, true);
    }
  }

  ctx->stopTest();

  return NDBT_OK;
}

static volatile int aValue = 0;

void
a_callback(int, NdbTransaction*, void*)
{
  ndbout_c("callback received!");
  aValue = 1;
}

int
runBug44065(NDBT_Context* ctx, NDBT_Step* step)
{
  /* testNdbApi -n WeirdAssertFail2
   * Results in assertion failure in DbtupCommit::execTUP_DEALLOCREQ()
   *   ndbassert(ptr->m_header_bits & Tuple_header::FREE);
   * Results in node failure
   */
  int rowno = 0;
  aValue = 0;
  Ndb* pNdb = GETNDB(step);
  Ndb * pNdb2 = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  pNdb2->init();
  pNdb2->waitUntilReady();

  const NdbDictionary::Table* pTab= ctx->getTab();
  
  HugoOperations hugoOps1(*pTab);
  CHECK(hugoOps1.startTransaction(pNdb) == 0);
  CHECK(hugoOps1.pkInsertRecord(pNdb, rowno) == 0);
  CHECK(hugoOps1.execute_NoCommit(pNdb) == 0);

  {
    HugoOperations hugoOps2(*pTab);
    CHECK(hugoOps2.startTransaction(pNdb2) == 0);
    
    CHECK(hugoOps2.pkDeleteRecord(pNdb2, rowno) == 0);
    CHECK(hugoOps2.pkInsertRecord(pNdb2, rowno) == 0);
    
    NdbTransaction* trans = hugoOps2.getTransaction();
    aValue = 0;
    
    trans->executeAsynch(NdbTransaction::NoCommit, a_callback, 0);
    pNdb2->sendPreparedTransactions(1);
    CHECK(hugoOps1.execute_Commit(pNdb) == 0);
    ndbout_c("waiting for callback");
    while (aValue == 0)
    {
      pNdb2->pollNdb();
      NdbSleep_MilliSleep(100);
    }
    CHECK(hugoOps2.execute_Rollback(pNdb2) == 0);
  }

  delete pNdb2; // need to delete hugoOps2 before pNdb2
  ctx->stopTest();

  return NDBT_OK;
}

int testApiFailReqImpl(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Setup a separate connection for running PK updates
   * with that will be disconnected without affecting
   * the test framework
   */
  if (otherConnection != NULL)
  {
    ndbout << "Connection not null" << endl;
    return NDBT_FAILED;
  }
  
  char connectString[256];
  ctx->m_cluster_connection.get_connectstring(connectString,
                                              sizeof(connectString));
  
  otherConnection= new Ndb_cluster_connection(connectString);
  
  if (otherConnection == NULL)
  {
    ndbout << "Connection is null" << endl;
    return NDBT_FAILED;
  }
  
  int rc= otherConnection->connect();
  
  if (rc!= 0)
  {
    ndbout << "Connect failed with rc " << rc << endl;
    return NDBT_FAILED;
  }
  
  /* Check that all nodes are alive - if one has failed
   * then probably we exposed bad API_FAILREQ handling
   */
  if (otherConnection->wait_until_ready(10,10) != 0)
  {
    ndbout << "Cluster connection was not ready" << endl;
    return NDBT_FAILED;
  }
  
  for (int i=0; i < MAX_STEPS; i++)
  {
    /* We must create the Ndb objects here as we 
     * are still single threaded
     */
    stepNdbs[i]= new Ndb(otherConnection,
                         "TEST_DB");
    stepNdbs[i]->init();
    int rc= stepNdbs[i]->waitUntilReady(10);
    
    if (rc != 0)
    {
      ndbout << "Ndb " << i << " was not ready" << endl;
      return NDBT_FAILED;
    }
    
  }
  
  /* Now signal the 'worker' threads to start sending Pk
   * reads
   */
  ctx->setProperty(ApiFailTestRun, 1);
  
  /* Wait until all of them are running before proceeding */
  ctx->getPropertyWait(ApiFailTestsRunning, 
                       ctx->getProperty(ApiFailNumberPkSteps));

  if (ctx->isTestStopped())
  {
    return NDBT_OK;
  }
  
  /* Clear the test-run flag so that they'll wait after
   * they hit an error
   */
  ctx->setProperty(ApiFailTestRun, (Uint32)0);

  /* Wait a little */
  sleep(1);

  /* Active more stringent checking of behaviour after
   * API_FAILREQ
   */
  NdbRestarter restarter;
    
  /* Activate 8078 - TCs will abort() if they get a TCKEYREQ
   * from the failed API after an API_FAILREQ message
   */
  ndbout << "Activating 8078" << endl;
  restarter.insertErrorInAllNodes(8078);
  
  /* Wait a little longer */
  sleep(1);
  
  /* Now cause our connection to disconnect
   * This results in TC receiving an API_FAILREQ
   * If there's an issue with API_FAILREQ 'cleanly'
   * stopping further signals, there should be
   * an assertion failure in TC 
   */
  int otherNodeId = otherConnection->node_id();
  
  ndbout << "Forcing disconnect of node " 
         << otherNodeId << endl;
  
  /* All dump 900 <nodeId> */
  int args[2]= {900, otherNodeId};
  
  restarter.dumpStateAllNodes( args, 2 );
  

  /* Now wait for all workers to finish
   * (Running worker count to get down to zero
   */
  ctx->getPropertyWait(ApiFailTestsRunning, (Uint32)0);

  if (ctx->isTestStopped())
  {
    return NDBT_OK;
  }
  
  /* Clean up error insert */
  restarter.insertErrorInAllNodes(0);
  
  /* Clean up allocated resources */
  for (int i= 0; i < MAX_STEPS; i++)
  {
    delete stepNdbs[i];
    stepNdbs[i]= NULL;
  }
  
  delete otherConnection;
  otherConnection= NULL;
  
  return NDBT_OK;
}


int testApiFailReq(NDBT_Context* ctx, NDBT_Step* step)
{  
  /* Perform a number of iterations, connecting,
   * sending lots of PK updates, inserting error
   * and then causing node failure
   */
  Uint32 iterations = 10;
  int rc = NDBT_OK;

  while (iterations --)
  {
    rc= testApiFailReqImpl(ctx, step);
    
    if (rc == NDBT_FAILED)
    {
      break;
    }
  } // while(iterations --)
    
  /* Avoid PkRead worker threads getting stuck */
  ctx->setProperty(ApiFailTestComplete, (Uint32) 1);

  return rc;
}

int runBulkPkReads(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Run batched Pk reads */

  while(true)
  {
    /* Wait to be signalled to start running */
    while ((ctx->getProperty(ApiFailTestRun) == 0) &&
           (ctx->getProperty(ApiFailTestComplete) == 0) &&
           !ctx->isTestStopped())
    {
      ctx->wait_timeout(500); /* 500 millis */
    }

    if (ctx->isTestStopped() ||
        (ctx->getProperty(ApiFailTestComplete) != 0))
    {
      /* Asked to stop by main test thread */
      return NDBT_OK;
    }
    /* Indicate that we're underway */
    ctx->incProperty(ApiFailTestsRunning);
      
    Ndb* otherNdb = stepNdbs[step->getStepNo()];
    HugoOperations hugoOps(*ctx->getTab());
    Uint32 numRecords = ctx->getNumRecords();
    Uint32 batchSize = (1000 < numRecords)? 1000 : numRecords;
    
    ndbout << "Step number " << step->getStepNo()
           << " reading batches of " << batchSize 
           << " rows " << endl;
    
    while(true)
    {
      if (hugoOps.startTransaction(otherNdb) != 0)
      {
        if (otherNdb->getNdbError().code == 4009) 
        {
          /* Api disconnect sometimes manifests as Cluster failure
           * from API's point of view as it cannot seize() a 
           * transaction from any Ndbd node
           * We treat this the same way as the later error cases
           */
          break;
        }
          
        ndbout << "Failed to start transaction.  Error : "
               << otherNdb->getNdbError().message << endl;
        return NDBT_FAILED;
      }
      
      for (Uint32 op = 0; op < batchSize; op++)
      {
        if (hugoOps.pkReadRecord(otherNdb,
                                 op) != 0)
        {
          ndbout << "Failed to define read of record number " << op << endl;
          ndbout << "Error : " << hugoOps.getTransaction()->getNdbError().message 
                 << endl;
          return NDBT_FAILED;
        }
      }
      
      if (hugoOps.execute_Commit(otherNdb) != 0)
      {
        NdbError err = hugoOps.getTransaction()->getNdbError();
        ndbout << "Execute failed with Error : " 
               << err.message
               << endl;
        
        hugoOps.closeTransaction(otherNdb);
        
        if ((err.code == 4002) || // send failed
            (err.code == 4010) || // Node failure
            (err.code == 4025) || // Node failure
            (err.code == 1218))   // Send buffer overload (reading larger tables)
        {
          /* Expected scenario due to injected Api disconnect 
           * If there was a node failure due to assertion failure
           * then we'll detect it when we try to setup a new
           * connection
           */
          break; 
        }
        return NDBT_FAILED;
      }
      
      hugoOps.closeTransaction(otherNdb);
    }

    /* Signal that we've finished running this iteration */
    ctx->decProperty(ApiFailTestsRunning);
  }
 
  return NDBT_OK;
}
  
int runReadColumnDuplicates(NDBT_Context* ctx, NDBT_Step* step){

  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();
  HugoCalculator hc(*pTab);
  Uint32 numRecords = ctx->getNumRecords();

  Ndb* pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
  if (pNdb == NULL){
    ndbout << "pNdb == NULL" << endl;      
    return NDBT_FAILED;  
  }
  if (pNdb->init()){
    NDB_ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);
  
  for (int m = 1; m < 100; m++){
    Uint32 record = (100 - m) % numRecords;
    NdbConnection* pCon = pNdb->startTransaction();
    if (pCon == NULL){
      delete pNdb;
      return NDBT_FAILED;
    }
      
    NdbOperation* pOp = pCon->getNdbOperation(pTab->getName());
    if (pOp == NULL){
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }
      
    if (pOp->readTuple() != 0){
      pNdb->closeTransaction(pCon);
      delete pNdb;
      return NDBT_FAILED;
    }
    
    int numCols= pTab->getNoOfColumns();

    for(int a = 0; a < numCols; a++){
      if (pTab->getColumn(a)->getPrimaryKey() == true){
	if(hugoOps.equalForAttr(pOp, a, record) != 0){
	  NDB_ERR(pCon->getNdbError());
	  pNdb->closeTransaction(pCon);
	  delete pNdb;
	  return NDBT_FAILED;
	}
      }
    }
      
    int dupColNum = m % numCols;
    int numReads = m + 1;
    
    NdbRecAttr* first = NULL;
    ndbout << "Reading record " 
           << record << " Column "
           << dupColNum << " " << numReads
           << " times" << endl;
    while (numReads--)
    {
      NdbRecAttr* recAttr = pOp->getValue(dupColNum);
      if (recAttr == NULL) {
	const NdbError err = pCon->getNdbError();
	NDB_ERR(err);
        result = NDBT_FAILED;
        pNdb->closeTransaction(pCon);	
	break;
      }
      first = (first == NULL) ? recAttr : first;
    };
    
    if (result == NDBT_FAILED)
      break;

    if (pCon->execute(Commit) != 0){
      const NdbError err = pCon->getNdbError();
      NDB_ERR(err);
      result = NDBT_FAILED;
      pNdb->closeTransaction(pCon);
      break;
    }

    if (pCon->getNdbError().code != 0)
    {
      NdbError err = pCon->getNdbError();
      if (err.code == 880)
      {
        /* Tried to read too much error - this column
         * is probably too large.
         * Skip to next iteration
         */
        ndbout << "Reading too much in one op, skipping..." << endl;
        pNdb->closeTransaction(pCon);
        continue;
      }
      ndbout << "Error at execute time : " << err.code
             << ":" << err.message << endl;
      pNdb->closeTransaction(pCon);
      result = NDBT_FAILED;
      break;
    }

    /* Let's check the results */

    
    const NdbRecAttr* curr = first;

    for (int c= 0; c < (m+1); c++)
    {
      if (hc.verifyRecAttr(record,
                           0,
                           curr))
      {
        ndbout << "Mismatch on record "
                 << record << " column "
                 << dupColNum << " read number "
                 << c+1 << endl;
        result =  NDBT_FAILED;
        break;
      }

      ndbout << "/";
      
      curr = curr->next();
    }

    ndbout << endl;

    pNdb->closeTransaction(pCon);

    if (result == NDBT_FAILED)
      break;

    if (curr != NULL)
    {
      ndbout << "Error - extra RecAttr(s) found" << endl;
      result = NDBT_FAILED;
      break;
    }

  }// m

  delete pNdb;

  return result;
}

class TransGuard
{
  NdbTransaction* pTrans;
public:
  TransGuard(NdbTransaction * p) : pTrans(p) {}
  ~TransGuard() { if (pTrans) pTrans->close(); pTrans = 0; }
};

int
runBug51775(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);

  NdbTransaction * pTrans1 = pNdb->startTransaction();
  if (pTrans1 == NULL)
  {
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }
  TransGuard g1(pTrans1);

  NdbTransaction * pTrans2 = pNdb->startTransaction();
  if (pTrans2 == NULL)
  {
    pTrans1->close();
    NDB_ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  TransGuard g2(pTrans2);

  {
    NdbOperation * pOp = pTrans1->getNdbOperation(ctx->getTab()->getName());
    if (pOp == NULL)
    {
      NDB_ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    if (pOp->insertTuple() != 0)
    {
      NDB_ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    HugoOperations hugoOps(* ctx->getTab());
    hugoOps.setValues(pOp, 0, 0);
  }

  {
    NdbOperation * pOp = pTrans2->getNdbOperation(ctx->getTab()->getName());
    if (pOp == NULL)
    {
      NDB_ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    if (pOp->readTuple() != 0)
    {
      NDB_ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    HugoOperations hugoOps(* ctx->getTab());
    hugoOps.equalForRow(pOp, 0);
    pOp->getValue(NdbDictionary::Column::FRAGMENT);
  }


  pTrans1->execute(NoCommit); // We now have un uncommitted insert

  /**
   * Now send a read...which will get 266
   */
  pTrans2->executeAsynch(NoCommit, 0, 0);
  int res = pNdb->pollNdb(1, 1000);
  ndbout_c("res: %u", res);
  
  NdbSleep_SecSleep(10);
  ndbout_c("pollNdb()");
  while (pNdb->pollNdb() + res == 0);

  return NDBT_OK;
}

int setupOtherConnection(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Setup a separate connection for running operations
   * that can be disconnected without affecting
   * the test framework
   */
  if (otherConnection != NULL)
  {
    g_err.println("otherConnection not null");
    return NDBT_FAILED;
  }
  
  char connectString[256];
  ctx->m_cluster_connection.get_connectstring(connectString,
                                              sizeof(connectString));
  
  otherConnection= new Ndb_cluster_connection(connectString);
  
  if (otherConnection == NULL)
  {
    g_err.println("otherConnection is null");
    return NDBT_FAILED;
  }
  
  int rc= otherConnection->connect();
  
  if (rc!= 0)
  {
    g_err.println("Connect failed with rc %d", rc);
    return NDBT_FAILED;
  }
  
  /* Check that all nodes are alive */
  if (otherConnection->wait_until_ready(10,10) != 0)
  {
    g_err.println("Cluster connection was not ready");
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int tearDownOtherConnection(NDBT_Context* ctx, NDBT_Step* step)
{
  if (otherConnection == NULL)
  {
    g_err << "otherConnection is NULL" << endl;
    return NDBT_OK;
  }

  delete otherConnection;
  otherConnection = NULL;

  return NDBT_OK;
}


int testFragmentedApiFailImpl(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Setup a separate connection for running scan operations
   * that will be disconnected without affecting
   * the test framework
   */
  if (setupOtherConnection(ctx, step) != NDBT_OK)
  {
    return NDBT_FAILED;
  }
  
  for (int i=0; i < MAX_STEPS; i++)
  {
    /* We must create the Ndb objects here as we 
     * are still single threaded
     */
    stepNdbs[i]= new Ndb(otherConnection,
                         "TEST_DB");
    stepNdbs[i]->init();
    int rc= stepNdbs[i]->waitUntilReady(10);
    
    if (rc != 0)
    {
      g_err.println("FragApiFail : Ndb %d was not ready", i);
      return NDBT_FAILED;
    }
    
  }
  
  /* Now signal the 'worker' threads to start sending Pk
   * reads
   */
  ctx->setProperty(ApiFailTestRun, 1);
  
  /* Wait until all of them are running before proceeding */
  ctx->getPropertyWait(ApiFailTestsRunning, 
                       ctx->getProperty(ApiFailNumberPkSteps));

  if (ctx->isTestStopped())
  {
    return NDBT_OK;
  }
  
  /* Clear the test-run flag so that they'll wait after
   * they hit an error
   */
  ctx->setProperty(ApiFailTestRun, (Uint32)0);

  /* Wait a little */
  sleep(1);

  /* Now cause our connection to disconnect
   * This results in NDBD running API failure
   * code and cleaning up any in-assembly fragmented
   * signals
   */
  int otherNodeId = otherConnection->node_id();
  
  g_info.println("FragApiFail : Forcing disconnect of node %u", otherNodeId);
  
  /* All dump 900 <nodeId> */
  int args[2]= {900, otherNodeId};
  
  NdbRestarter restarter;
  restarter.dumpStateAllNodes( args, 2 );
  
  /* Now wait for all workers to finish
   * (Running worker count to get down to zero
   */
  ctx->getPropertyWait(ApiFailTestsRunning, (Uint32)0);

  if (ctx->isTestStopped())
  {
    return NDBT_OK;
  }
  
  /* Clean up allocated resources */
  for (int i= 0; i < MAX_STEPS; i++)
  {
    delete stepNdbs[i];
    stepNdbs[i]= NULL;
  }
  
  tearDownOtherConnection(ctx, step);
  
  return NDBT_OK;
}

int testFragmentedApiFail(NDBT_Context* ctx, NDBT_Step* step)
{  
  /* Perform a number of iterations, connecting,
   * sending lots of PK updates, inserting error
   * and then causing node failure
   */
  Uint32 iterations = 10;
  int rc = NDBT_OK;

  while (iterations --)
  {
    rc= testFragmentedApiFailImpl(ctx, step);
    
    if (rc == NDBT_FAILED)
    {
      break;
    }
  } // while(iterations --)
    
  /* Avoid scan worker threads getting stuck */
  ctx->setProperty(ApiFailTestComplete, (Uint32) 1);

  return rc;
}

int runFragmentedScanOtherApi(NDBT_Context* ctx, NDBT_Step* step)
{
  /* We run a loop sending large scan requests that will be
   * fragmented.
   * The requests are so large that they actually fail on 
   * arrival at TUP as there is too much ATTRINFO
   * That doesn't affect this testcase though, as it is
   * testing TC cleanup of fragmented signals from a 
   * failed API
   */
  /* SEND > ((2 * MAX_SEND_MESSAGE_BYTESIZE) + SOME EXTRA) 
   * This way we get at least 3 fragments
   * However, as this is generally > 64kB, it's too much AttrInfo for
   * a ScanTabReq, so the 'success' case returns error 874
   */
  const Uint32 PROG_WORDS= 16500; 
  
  /* Use heap rather than stack as stack is too small in
   * STEP thread
   */
  Uint32* buff= new Uint32[ PROG_WORDS + 10 ]; // 10 extra for final 'return' etc.
  Uint32 stepNo = step->getStepNo();

  while(true)
  {
    /* Wait to be signalled to start running */
    while ((ctx->getProperty(ApiFailTestRun) == 0) &&
           (ctx->getProperty(ApiFailTestComplete) == 0) &&
           !ctx->isTestStopped())
    {
      ctx->wait_timeout(500); /* 500 millis */
    }

    if (ctx->isTestStopped() ||
        (ctx->getProperty(ApiFailTestComplete) != 0))
    {
      g_info.println("%u: Test stopped, exiting thread", stepNo);
      /* Asked to stop by main test thread */
      delete[] buff;
      return NDBT_OK;
    }
    /* Indicate that we're underway */
    ctx->incProperty(ApiFailTestsRunning);

    Ndb* otherNdb = stepNdbs[stepNo];
    
    while (true)
    {
      /* Start a transaction */
      NdbTransaction* trans= otherNdb->startTransaction();
      if (!trans)
      {
        const NdbError err = otherNdb->getNdbError();
        
        /* During this test, if we attempt to get a transaction
         * when the API is disconnected, we can get error 4009
         * (Cluster failure) or 4035 (Cluster temporarily unavailable).
         * We treat this similarly to the
         * "Node failure caused abort of transaction" case
         */
        if (err.code == 4009 || err.code == 4035)
        {
          g_info.println("%u: Failed to start transaction from Ndb object Error : %u %s",
                   stepNo, err.code, err.message);
          break;
        }
        g_err.println("ERR: %u: %u: Failed to start transaction from Ndb object Error : %u %s",
                      __LINE__, stepNo, err.code, err.message);
        delete[] buff;
        return NDBT_FAILED;
      }
      
      NdbScanOperation* scan= trans->getNdbScanOperation(ctx->getTab());
      
      if (scan == NULL)
      {
        /* getNdbScanOperation can fail in same way as startTransaction
         * since it starts a buddy transaction for scan operations.
         */
        const NdbError err = trans->getNdbError();
        if (err.code == 4009 || err.code == 4035)
        {
          g_info.println("%u: Failed to get scan operation transaction Error : %u %s",
                   stepNo, err.code, err.message);
          trans->close();
          break;
        }
        g_err.println("ERR: %u: %u: Failed to get scan operation transaction Error : %u %s",
                 __LINE__, stepNo, err.code, err.message);
        trans->close();
        delete[] buff;
        return NDBT_FAILED;
      }
      
      CHECK(0 == scan->readTuples());
      
      /* Create a large program, to give a large SCANTABREQ */
      NdbInterpretedCode prog(ctx->getTab(), 
                              buff, PROG_WORDS + 10);
      
      for (Uint32 w=0; w < PROG_WORDS; w++)
        CHECK(0 == prog.load_const_null(1));
    
      CHECK(0 == prog.interpret_exit_ok());
      CHECK(0 == prog.finalise());
      
      CHECK(0 == scan->setInterpretedCode(&prog));
      
      int ret = trans->execute(NdbTransaction::NoCommit);

      const NdbError execError= trans->getNdbError();

      if (ret != 0)
      {
        /* Transaction was aborted.  Should be due to node disconnect. */
        if(execError.classification != NdbError::NodeRecoveryError)
        {
          g_err.println("ERR: %u: %u: Execute aborted transaction with invalid error code: %u",
                   __LINE__, stepNo, execError.code);
          NDB_ERR_OUT(g_err, execError);
          trans->close();
          delete[] buff;
          return NDBT_FAILED;
        }
        g_info.println("%u: Execute aborted transaction with NR error code: %u",
                 stepNo, execError.code);
        trans->close();
        break;
      }

      /* Can get success (0), or 874 for too much AttrInfo, depending
       * on timing
       */
      if ((execError.code != 0) &&
          (execError.code != 874) &&
          (execError.code != 4002))
      {
        g_err.println("ERR: %u: %u: incorrect error code: %u", __LINE__, stepNo, execError.code);
        NDB_ERR_OUT(g_err, execError);
        trans->close();
        delete[] buff;
        return NDBT_FAILED;
      }

      /* nextResult will always fail */  
      CHECK(-1 == scan->nextResult());
      
      NdbError scanError= scan->getNdbError();
      
      /* 'Success case' is 874 for too much AttrInfo */
      if (scanError.code != 874)
      {
       /* When disconnected, we get should get a node failure related error */
        if (scanError.classification == NdbError::NodeRecoveryError)
        {
          g_info.println("%u: Scan failed due to node failure/disconnect with error code %u",
                         stepNo, scanError.code);
          trans->close();
          break;
        }
        else
        {
          g_err.println("ERR: %u: %u: incorrect error code: %u", __LINE__, stepNo, scanError.code);
          NDB_ERR_OUT(g_err, scanError);
          trans->close();
          delete[] buff;
          return NDBT_FAILED;
        }
      }
      
      scan->close();
      
      trans->close();
    } // while (true)
    
    /* Node failure case - as expected */
    g_info.println("%u: Scan thread finished iteration", stepNo);

    /* Signal that we've finished running this iteration */
    ctx->decProperty(ApiFailTestsRunning);
  } 

  delete[] buff;
  return NDBT_OK;
}
  
void outputLockMode(NdbOperation::LockMode lm)
{
  switch(lm)
  {
  case NdbOperation::LM_Exclusive:
    ndbout << "LM_Exclusive";
    break;
  case NdbOperation::LM_Read:
    ndbout << "LM_Read";
    break;
  case NdbOperation::LM_SimpleRead:
    ndbout << "LM_SimpleRead";
    break;
  case NdbOperation::LM_CommittedRead:
    ndbout << "LM_CommittedRead";
    break;
  }
}

NdbOperation::LockMode chooseLockMode(bool onlyRealLocks = false)
{
  Uint32 choice;
  
  if (onlyRealLocks)
  {
    choice = rand() % 2;
  }
  else
  {
    choice = rand() % 4;
  }

  NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;

  switch(choice)
  {
  case 0:
    lm = NdbOperation::LM_Exclusive;
    break;
  case 1:
    lm = NdbOperation::LM_Read;
    break;
  case 2:
    lm = NdbOperation::LM_SimpleRead;
    break;
  case 3:
  default:
    lm = NdbOperation::LM_CommittedRead;
    break;
  }

  outputLockMode(lm);
  ndbout << endl;

  return lm;
}

NdbOperation::LockMode chooseConflictingLockMode(NdbOperation::LockMode lm)
{
  NdbOperation::LockMode conflicting = NdbOperation::LM_Exclusive;

  switch (lm) 
  {
  case NdbOperation::LM_Exclusive:
    conflicting = (((rand() % 2) == 0) ? 
                   NdbOperation::LM_Exclusive :
                   NdbOperation::LM_Read);

    break;
  case NdbOperation::LM_Read:
    conflicting = NdbOperation::LM_Exclusive;
    break;
  default:
    abort(); // SimpleRead + CommittedRead can't conflict reliably
  }

  ndbout << "conflicting with ";
  outputLockMode(lm);
  ndbout << " using ";
  outputLockMode(conflicting);
  ndbout << endl;
  return conflicting;
}   

#define CHECKN(c, o, e) { if (!(c)) {                     \
    ndbout << "Failed on line " << __LINE__ << endl;    \
    ndbout << (o)->getNdbError() << endl;               \
    return e; } }

NdbOperation* defineReadAllColsOp(HugoOperations* hugoOps,
                                  NdbTransaction* trans,
                                  const NdbDictionary::Table* pTab,
                                  NdbOperation::LockMode lm,
                                  Uint32 rowNum)
{
  NdbOperation* op = trans->getNdbOperation(pTab);
  CHECKN(op != NULL, trans, NULL);
    
  CHECKN(op->readTuple(lm) == 0, op, NULL);
  
  hugoOps->equalForRow(op, rowNum);
  
  for(int c = 0; c < pTab->getNoOfColumns(); c++)
  {
    if(!pTab->getColumn(c)->getPrimaryKey())
    {
      CHECKN(op->getValue(pTab->getColumn(c)->getName()) != NULL, op, NULL);
    }
  }
  
  return op;
}

bool checkReadRc(HugoOperations* hugoOps,
                 Ndb* ndb,
                 const NdbDictionary::Table* pTab,
                 NdbOperation::LockMode lm,
                 Uint32 rowNum,
                 int expectedRc)
{
  NdbTransaction* trans = ndb->startTransaction();
  CHECKN(trans != NULL, ndb, false);
  
  NdbOperation* readOp = defineReadAllColsOp(hugoOps,
                                             trans,
                                             pTab,
                                             lm,
                                             rowNum);
  CHECKN(readOp != NULL, trans, false);

  int execRc = trans->execute(Commit);
  
  if (expectedRc)
  {
    /* Here we assume that the error is on the transaction
     * which may not be the case for some errors
     */
    if (trans->getNdbError().code != expectedRc)
    {
      ndbout << "Expected " << expectedRc << " at " << __LINE__ << endl;
      ndbout << "Got " << trans->getNdbError() << endl;
      return false;
    }
  }
  else
  {
    CHECKN(execRc == 0, trans, false);
    CHECKN(readOp->getNdbError().code == 0, readOp, false);
  }
  
  trans->close();

  return true;
}

bool checkReadDeadlocks(HugoOperations* hugoOps,
                        Ndb* ndb,
                        const NdbDictionary::Table* pTab,
                        NdbOperation::LockMode lm,
                        Uint32 rowNum)
{
  return checkReadRc(hugoOps, ndb, pTab, lm, rowNum, 266);
}

bool checkReadSucceeds(HugoOperations* hugoOps,
                       Ndb* ndb,
                       const NdbDictionary::Table* pTab,
                       NdbOperation::LockMode lm,
                       Uint32 rowNum)
{
  return checkReadRc(hugoOps, ndb, pTab, lm, rowNum, 0);
}

int runTestUnlockBasic(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Basic tests that we can lock and unlock rows
   * using the unlock mechanism
   * Some minor side-validation that the API rejects
   * readLockInfo for non Exclusive / Shared lock modes
   * and that double-release of the lockhandle is caught
   */
  const NdbDictionary::Table* pTab = ctx->getTab();
  
  HugoOperations hugoOps(*pTab);
  
  const Uint32 iterations = 200;

  for (Uint32 iter = 0; iter < iterations; iter++)
  {
    Uint32 rowNum = iter % ctx->getNumRecords();

    NdbTransaction* trans = GETNDB(step)->startTransaction();
    CHECKN(trans != NULL, GETNDB(step), NDBT_FAILED);
    
    ndbout << "First transaction operation using ";
    NdbOperation::LockMode lm = chooseLockMode();

    NdbOperation* op = defineReadAllColsOp(&hugoOps,
                                           trans,
                                           pTab,
                                           lm,
                                           rowNum);
    CHECKN(op != NULL, trans, NDBT_FAILED);
    
    if (op->getLockHandle() == NULL)
    {
      if ((lm == NdbOperation::LM_CommittedRead) ||
          (lm == NdbOperation::LM_SimpleRead))
      {
        if (op->getNdbError().code == 4549)
        {
          /* As expected, go to next iteration */
          ndbout << "Definition error as expected, moving to next" << endl;
          trans->close();
          continue;
        }
        ndbout << "Expected 4549, got :" << endl;
      }
      ndbout << op->getNdbError() << endl;
      ndbout << " at "<<__FILE__ << ":" <<__LINE__ << endl;
      return NDBT_FAILED;
    }
    
    CHECKN(trans->execute(NoCommit) == 0, trans, NDBT_FAILED);
    
    const NdbLockHandle* lh = op->getLockHandle();
    CHECKN(lh != NULL, op, NDBT_FAILED);

    /* Ok, let's use another transaction to try and get a
     * lock on the row (exclusive or shared)
     */
    NdbTransaction* trans2 = GETNDB(step)->startTransaction();
    CHECKN(trans2 != NULL, GETNDB(step), NDBT_FAILED);


    ndbout << "Second transaction operation using ";
    NdbOperation::LockMode lm2 = chooseLockMode();

    NdbOperation* op2 = defineReadAllColsOp(&hugoOps,
                                            trans2,
                                            pTab,
                                            lm2,
                                            rowNum);
    CHECKN(op2 != NULL, trans2, NDBT_FAILED);

    /* Execute can succeed if both lock modes are LM read
     * otherwise we'll deadlock (266)
     */
    bool expectOk = ((lm2 == NdbOperation::LM_CommittedRead) ||
                     ((lm == NdbOperation::LM_Read) &&
                      ((lm2 == NdbOperation::LM_Read) ||
                       (lm2 == NdbOperation::LM_SimpleRead))));

    /* Exclusive read locks primary only, and SimpleRead locks
     * Primary or Backup, so SimpleRead may or may not succeed
     */
    bool unknownCase = ((lm == NdbOperation::LM_Exclusive) &&
                        (lm2 == NdbOperation::LM_SimpleRead));
    
    if (trans2->execute(NoCommit) != 0)
    {
      if (expectOk ||
          (trans2->getNdbError().code != 266))
      {
        ndbout << trans2->getNdbError() << endl;
        ndbout << " at "<<__FILE__ << ":" <<__LINE__ << endl;
        return NDBT_FAILED;
      }
    }
    else
    {
      if (!expectOk  && !unknownCase)
      {
        ndbout << "Expected deadlock but had success!" << endl;
        return NDBT_FAILED;
      }
    }
    trans2->close();

    /* Now let's try to create an unlockRow operation, and
     * execute it 
     */
    const NdbOperation* unlockOp = trans->unlock(lh);
    
    CHECKN(unlockOp != NULL, trans, NDBT_FAILED);

    CHECKN(trans->execute(NoCommit) == 0, trans, NDBT_FAILED);

    /* Now let's try to get an exclusive lock on the row from
     * another transaction which can only be possible if the
     * original lock has been removed.
     */
    CHECK(checkReadSucceeds(&hugoOps,
                            GETNDB(step),
                            pTab,
                            NdbOperation::LM_Exclusive,
                            rowNum));
    ndbout << "Third transaction operation using LM_Exclusive succeeded" << endl;

    Uint32 choice = rand() % 3;
    switch(choice)
    {
    case 0:
      ndbout << "Closing transaction" << endl;
      trans->close();
      break;
    case 1:
      ndbout << "Releasing handle and closing transaction" << endl;
      CHECKN(trans->releaseLockHandle(lh) == 0, trans, NDBT_FAILED);
      trans->close();
      break;
    case 2:
      ndbout << "Attempting to release the handle twice" << endl;
      CHECKN(trans->releaseLockHandle(lh) == 0, trans, NDBT_FAILED);
      
      if ((trans->releaseLockHandle(lh) != -1) ||
          (trans->getNdbError().code != 4551))
      {
        ndbout << "Expected 4551, but got no error " << endl;
        ndbout << " at "<<__FILE__ << ":" <<__LINE__ << endl;
        return NDBT_FAILED;
      }
      
      trans->close();
      break;
    default:
      abort();
      break;
    } 
  } // for (Uint32 iter

  return NDBT_OK;
}

int runTestUnlockRepeat(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Transaction A locks 2 rows
   * It repeatedly unlocks and re-locks one row, but leaves
   * the other locked
   * Transaction B verifies that it can only lock the unlocked
   * row when it is unlocked, and can never lock the row which
   * is never unlocked!
   */

  const NdbDictionary::Table* pTab = ctx->getTab();
  
  HugoOperations hugoOps(*pTab);

  const Uint32 outerLoops = 2;
  const Uint32 iterations = 10;

  Ndb* ndb = GETNDB(step);

  /* Transaction A will take a lock on otherRowNum and hold it
   * throughout.
   * RowNum will be locked and unlocked each iteration
   */
  Uint32 otherRowNum = ctx->getNumRecords() - 1;
  
  for (Uint32 outerLoop = 0; outerLoop < outerLoops; outerLoop ++)
  {
    NdbTransaction* transA = ndb->startTransaction();
    CHECKN(transA != NULL, ndb, NDBT_FAILED);

    NdbOperation::LockMode lockAOtherMode;
    ndbout << "TransA : Try to lock otherRowNum in mode ";

    switch (outerLoop % 2) {
    case 0:
      ndbout << "LM_Exclusive" << endl;
      lockAOtherMode = NdbOperation::LM_Exclusive;
      break;
    default:
      ndbout << "LM_Read" << endl;
      lockAOtherMode = NdbOperation::LM_Read;
      break;
    }
  
    NdbOperation* lockAOtherRowNum = defineReadAllColsOp(&hugoOps,
                                                         transA,
                                                         pTab,
                                                         lockAOtherMode,
                                                         otherRowNum);
    CHECKN(lockAOtherRowNum != NULL, transA, NDBT_FAILED);

    CHECKN(transA->execute(NoCommit) == 0, transA, NDBT_FAILED);

    ndbout << "TransA : Got initial lock on otherRowNum" << endl;

    for (Uint32 iter = 0; iter < iterations; iter++)
    {
      Uint32 rowNum = iter % (ctx->getNumRecords() - 1);
  
      ndbout << "  TransA : Try to lock rowNum with mode ";
      NdbOperation::LockMode lockAMode = chooseLockMode(true); // Exclusive or LM_Read
  
      /* Transaction A takes a lock on rowNum */
      NdbOperation* lockARowNum = defineReadAllColsOp(&hugoOps,
                                                      transA,
                                                      pTab,
                                                      lockAMode,
                                                      rowNum);
      CHECKN(lockARowNum != NULL, transA, NDBT_FAILED);
    
      const NdbLockHandle* lockAHandle = lockARowNum->getLockHandle();
      CHECKN(lockAHandle != NULL, lockARowNum, NDBT_FAILED);

      CHECKN(transA->execute(NoCommit) == 0, transA, NDBT_FAILED);

      ndbout << "    TransA : Got lock on rowNum" << endl; 

      /* Now transaction B checks that it cannot get a conflicting lock 
       * on rowNum 
       */
      ndbout << "  TransB : Try to lock rowNum by ";

      CHECK(checkReadDeadlocks(&hugoOps,
                               ndb,
                               pTab,
                               chooseConflictingLockMode(lockAMode),
                               rowNum));

      ndbout << "    TransB : Failed to get lock on rowNum as expected" << endl;

      /* Now transaction A unlocks rowNum */
      const NdbOperation* unlockOpA = transA->unlock(lockAHandle);
      CHECKN(unlockOpA != NULL, transA, NDBT_FAILED);

      CHECKN(transA->execute(NoCommit) == 0, transA, NDBT_FAILED);

      ndbout << "  TransA : Unlocked rowNum" << endl;
    
      /* Now transaction B attempts to gain a lock on RowNum */
      NdbTransaction* transB = ndb->startTransaction();
      CHECKN(transB != NULL, ndb, NDBT_FAILED);

      ndbout << "  TransB : Try to lock rowNum with mode ";
      NdbOperation::LockMode lockBMode = chooseLockMode(true);

      NdbOperation* tryLockBRowNum2 = defineReadAllColsOp(&hugoOps,
                                                          transB,
                                                          pTab,
                                                          lockBMode,
                                                          rowNum);
      CHECKN(tryLockBRowNum2 != NULL, transB, NDBT_FAILED);

      CHECKN(transB->execute(NoCommit) == 0, transB, NDBT_FAILED);
    
      ndbout << "    TransB : Got lock on rowNum" << endl;

      ndbout << "  TransB : Try to lock other row by ";
      NdbOperation::LockMode lockBOtherMode = chooseConflictingLockMode(lockAOtherMode);

      /* Now transaction B attempts to gain a lock on OtherRowNum
       * which should fail as transaction A still has it locked
       */
      NdbOperation* tryLockBOtherRowNum = defineReadAllColsOp(&hugoOps,
                                                              transB,
                                                              pTab,
                                                              lockBOtherMode,
                                                              otherRowNum);
      CHECKN(tryLockBOtherRowNum != NULL, transB, NDBT_FAILED);

      CHECKN(transB->execute(NoCommit) == -1, transB, NDBT_FAILED);
    
      if (transB->getNdbError().code != 266)
      {
        ndbout << "Error was expecting 266, but got " << transB->getNdbError() << endl;
        ndbout << "At line " << __LINE__ << endl;
        return NDBT_FAILED;
      }

      ndbout << "    TransB : Failed to get lock on otherRowNum as expected" << endl;

      transB->close();
    }

    transA->close();
  }

  return NDBT_OK;
}


int runTestUnlockMulti(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();

  /* Verifies that a single transaction (or multiple
   * transactions) taking multiple locks on the same
   * row using multiple operations behaves correctly
   * as the operations unlock their locks.
   * 
   * Transaction A will lock the row to depth A
   * Transaction A may use an exclusive lock as its first lock
   * Transaction B will lock the row to depth B
   *   iff transaction A did not use exclusive locks
   * 
   * Once all locks are in place, the locks placed are
   * removed.
   * The code checks that the row remains locked until
   * all locking operations are unlocked
   * The code checks that the row is unlocked when all
   * locking operations are unlocked.
   *
   * Depth A and B and whether A uses exclusive or not
   * are varied.
   */
  
  HugoOperations hugoOps(*pTab);

  const Uint32 MinLocks = 3;
  const Uint32 MaxLocksPerTrans = 20;
  Uint32 rowNum = ctx->getNumRecords() - 1;
  Uint32 numLocksInTransA = rand() % MaxLocksPerTrans;
  numLocksInTransA = (numLocksInTransA > MinLocks) ?
    numLocksInTransA : MinLocks;
  bool useExclusiveInA = ((rand() % 2) == 0);

  Uint32 numLocksInTransB = useExclusiveInA ? 0 :
    (rand() % MaxLocksPerTrans);
  
  Uint32 maxLocks = (numLocksInTransA > numLocksInTransB) ?
    numLocksInTransA : numLocksInTransB;
  
  ndbout << "NumLocksInTransA " << numLocksInTransA 
         << " NumLocksInTransB " << numLocksInTransB
         << " useExclusiveInA " << useExclusiveInA
         << endl;

  NdbOperation* transAOps[ MaxLocksPerTrans ];
  NdbOperation* transBOps[ MaxLocksPerTrans ];

  /* First the lock phase when transA and transB
   * claim locks (with LockHandles)
   * As this occurs, transC attempts to obtain
   * a conflicting lock and fails.
   */
  Ndb* ndb = GETNDB(step);

  NdbTransaction* transA = ndb->startTransaction();
  CHECKN(transA != NULL, ndb, NDBT_FAILED);
  
  NdbTransaction* transB = ndb->startTransaction();
  CHECKN(transB != NULL, ndb, NDBT_FAILED);
  
  ndbout << "Locking phase" << endl << endl;
  for(Uint32 depth=0; depth < maxLocks; depth++)
  {
    ndbout << "Depth " << depth << endl;
    NdbOperation::LockMode lmA;
    /* TransA */
    if (depth < numLocksInTransA)
    {
      ndbout << "  TransA : Locking with mode ";
      if ((depth == 0) && useExclusiveInA)
      {
        lmA = NdbOperation::LM_Exclusive;
        ndbout << "LM_Exclusive" << endl;
      }
      else if (!useExclusiveInA)
      {
        lmA = NdbOperation::LM_Read;
        ndbout << "LM_Read" << endl;
      }
      else
      {
        lmA = chooseLockMode(true); // LM_Exclusive or LM_Read;
      }
      
      NdbOperation* lockA = defineReadAllColsOp(&hugoOps,
                                                transA,
                                                pTab,
                                                lmA,
                                                rowNum);
      CHECKN(lockA != NULL, transA, NDBT_FAILED);
      CHECKN(lockA->getLockHandle() != NULL, lockA, NDBT_FAILED);
      
      transAOps[ depth ] = lockA;
      
      CHECKN(transA->execute(NoCommit) == 0, transA, NDBT_FAILED);
      ndbout << "  TransA : Succeeded" << endl;
    }
    
    /* TransB */
    if (depth < numLocksInTransB)
    {
      ndbout << "  TransB : Locking with mode LM_Read" << endl;
      
      NdbOperation* lockB = defineReadAllColsOp(&hugoOps,
                                                transB,
                                                pTab,
                                                NdbOperation::LM_Read,
                                                rowNum);
      CHECKN(lockB != NULL, transB, NDBT_FAILED);
      CHECKN(lockB->getLockHandle() != NULL, lockB, NDBT_FAILED);
      
      transBOps[ depth ] = lockB;
      
      CHECKN(transB->execute(NoCommit) == 0, transB, NDBT_FAILED);
      ndbout << "  TransB : Succeeded" << endl;
    }
  }

  ndbout << "Unlocking phase" << endl << endl;

  for(Uint32 depth = 0; depth < maxLocks; depth++)
  {
    Uint32 level = maxLocks - depth - 1;

    ndbout << "Depth " << level << endl;

    ndbout << "  TransC : Trying to lock row with lockmode ";
    NdbOperation::LockMode lmC;
    if (useExclusiveInA)
    {
      lmC = chooseLockMode(true); // LM_Exclusive or LM_Read;
    }
    else
    {
      ndbout << "LM_Exclusive" << endl;
      lmC = NdbOperation::LM_Exclusive;
    }

    CHECK(checkReadDeadlocks(&hugoOps,
                             ndb,
                             pTab,
                             lmC,
                             rowNum));

    ndbout << "  TransC failed as expected" << endl;

    if (level < numLocksInTransB)
    {
      const NdbLockHandle* lockHandleB = transBOps[ level ]->getLockHandle();
      CHECKN(lockHandleB != NULL, transBOps[ level ], NDBT_FAILED);

      const NdbOperation* unlockB = transB->unlock(lockHandleB);
      CHECKN(unlockB != NULL, transB, NDBT_FAILED);
      
      CHECKN(transB->execute(NoCommit) == 0, transB, NDBT_FAILED);
      ndbout << "  TransB unlock succeeded" << endl;
    }

    if (level < numLocksInTransA)
    {
      const NdbLockHandle* lockHandleA = transAOps[ level ]->getLockHandle();
      CHECKN(lockHandleA != NULL, transAOps[ level ], NDBT_FAILED);
      
      const NdbOperation* unlockA = transA->unlock(lockHandleA);
      CHECKN(unlockA != NULL, transA, NDBT_FAILED);
      
      CHECKN(transA->execute(NoCommit) == 0, transA, NDBT_FAILED);
      ndbout << "  TransA unlock succeeded" << endl;
    }
  }


  /* Finally, all are unlocked and transC can successfully
   * obtain a conflicting lock
   */
  CHECK(checkReadSucceeds(&hugoOps,
                          ndb,
                          pTab,
                          NdbOperation::LM_Exclusive,
                          rowNum));

  ndbout << "TransC LM_Exclusive lock succeeded" << endl;
  
  transA->close();
  transB->close();

  return NDBT_OK;
}
                    

int runTestUnlockScan(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Performs a table scan with LM_Read or LM_Exclusive 
   * and lock takeovers for a number of the rows returned
   * Validates that some of the taken-over locks are held
   * before unlocking them and validating that they 
   * are released.
   */
  const NdbDictionary::Table* pTab = ctx->getTab();
  
  HugoCalculator calc(*pTab);
  HugoOperations hugoOps(*pTab);

  /* 
     1) Perform scan of the table with LM_Read / LM_Exclusive
     2) Takeover some of the rows with read and lockinfo
     3) Unlock the rows
     4) Check that they are unlocked
  */
  Ndb* ndb = GETNDB(step);
  
  const int iterations = 2;

  const int maxNumTakeovers = 15;
  NdbOperation* takeoverOps[ maxNumTakeovers ];
  Uint32 takeoverColIds[ maxNumTakeovers ];
  
  int numTakeovers = MIN(maxNumTakeovers, ctx->getNumRecords());
  int takeoverMod = ctx->getNumRecords() / numTakeovers;

  ndbout << "numTakeovers is " << numTakeovers
         << " takeoverMod is " << takeoverMod << endl;

  for (int iter = 0; iter < iterations; iter++)
  {
    ndbout << "Scanning table with lock mode : ";
    NdbOperation::LockMode lmScan = chooseLockMode(true); // LM_Exclusive or LM_Read

    NdbTransaction* trans = ndb->startTransaction();
    CHECKN(trans != NULL, ndb, NDBT_FAILED);
    
    /* Define scan */
    NdbScanOperation* scan = trans->getNdbScanOperation(pTab);
    CHECKN(scan != NULL, trans, NDBT_FAILED);

    Uint32 scanFlags = NdbScanOperation::SF_KeyInfo;

    CHECKN(scan->readTuples(lmScan, scanFlags) == 0, scan, NDBT_FAILED);

    NdbRecAttr* idColRecAttr = NULL;

    for(int c = 0; c < pTab->getNoOfColumns(); c++)
    {
      NdbRecAttr* ra = scan->getValue(pTab->getColumn(c)->getName());
      CHECKN(ra != NULL, scan, NDBT_FAILED);
      if (calc.isIdCol(c))
      {
        CHECK(idColRecAttr == NULL);
        idColRecAttr = ra;
      }
    }
    CHECK(idColRecAttr != NULL);

    CHECKN(trans->execute(NoCommit) == 0, trans, NDBT_FAILED);
    
    int rowsRead = 0;
    int rowsTakenover = 0;
    while (scan->nextResult(true) == 0)
    {      
      if ((rowsTakenover < maxNumTakeovers) &&
          (0 == (rowsRead % takeoverMod)))
      {
        /* We're going to take the lock for this row into 
         * a separate operation
         */
        Uint32 rowId = idColRecAttr->u_32_value();
        ndbout << "  Taking over lock on result num " << rowsRead 
               << " row (" << rowId << ")" << endl;
        NdbOperation* readTakeoverOp = scan->lockCurrentTuple();
        CHECKN(readTakeoverOp != NULL, scan, NDBT_FAILED);
        
        CHECKN(readTakeoverOp->getLockHandle() != NULL, readTakeoverOp, NDBT_FAILED);
        takeoverOps[ rowsTakenover ] = readTakeoverOp;
        takeoverColIds[ rowsTakenover ] = rowId;

        CHECKN(trans->execute(NoCommit) == 0, trans, NDBT_FAILED);

        CHECKN(readTakeoverOp->getNdbError().code == 0, readTakeoverOp, NDBT_FAILED);

// // Uncomment to check that takeover keeps lock.
//         if (0 == (rowsTakenover % 7))
//         {
//           ndbout << "  Validating taken-over lock holds on rowid "
//                  << takeoverColIds[ rowsTakenover ] 
//                  << " by ";
//           /* Occasionally validate the lock held by the scan */
//           CHECK(checkReadDeadlocks(&hugoOps,
//                                    ndb,
//                                    pTab,
//                                    chooseConflictingLockMode(lmScan),
//                                    takeoverColIds[ rowsTakenover ]));
//         }
        
        rowsTakenover ++;

      }

      rowsRead ++;
    }
    
    scan->close();

    ndbout << "Scan complete : rows read : " << rowsRead 
           << " rows locked : " << rowsTakenover << endl;

    ndbout << "Now unlocking rows individually" << endl;
    for (int lockedRows = 0; lockedRows < rowsTakenover; lockedRows ++)
    {
      if (0 == (lockedRows % 3))
      {
        ndbout << "  First validating that lock holds on rowid "
               << takeoverColIds[ lockedRows ]
               << " by ";
        /* Occasionally check that the lock held by the scan still holds */
        CHECK(checkReadDeadlocks(&hugoOps,
                                 ndb,
                                 pTab,
                                 chooseConflictingLockMode(lmScan),
                                 takeoverColIds[ lockedRows ]));
        ndbout << "  Lock is held" << endl;
      }

      /* Unlock the row */
      const NdbLockHandle* lockHandle = takeoverOps[ lockedRows ]->getLockHandle();
      CHECKN(lockHandle != NULL, takeoverOps[ lockedRows ], NDBT_FAILED);

      const NdbOperation* unlockOp = trans->unlock(lockHandle);
      CHECKN(unlockOp, trans, NDBT_FAILED);

      CHECKN(trans->execute(NoCommit) == 0, trans, NDBT_FAILED);
      
      /* Now check that the row's unlocked */
      CHECK(checkReadSucceeds(&hugoOps,
                              ndb,
                              pTab,
                              NdbOperation::LM_Exclusive,
                              takeoverColIds[ lockedRows ]));
      ndbout << "  Row " << takeoverColIds[ lockedRows ] 
             << " unlocked successfully" << endl;
    }

    /* Lastly, verify that scan with LM_Exclusive in separate transaction 
     * can scan whole table without locking on anything
     */
    ndbout << "Validating unlocking code with LM_Exclusive table scan" << endl;

    NdbTransaction* otherTrans = ndb->startTransaction();
    CHECKN(otherTrans != NULL, ndb, NDBT_FAILED);

    NdbScanOperation* otherScan = otherTrans->getNdbScanOperation(pTab);
    CHECKN(otherScan != NULL, otherTrans, NDBT_FAILED);

    CHECKN(otherScan->readTuples(NdbOperation::LM_Exclusive) == 0, otherScan, NDBT_FAILED);

    for(int c = 0; c < pTab->getNoOfColumns(); c++)
    {
      NdbRecAttr* ra = otherScan->getValue(pTab->getColumn(c)->getName());
      CHECKN(ra != NULL, otherScan, NDBT_FAILED);
    }

    CHECKN(otherTrans->execute(NoCommit) == 0, trans, NDBT_FAILED);
    
    int nextRc = 0;
    while (0 == (nextRc = otherScan->nextResult(true)))
    {};

    if (nextRc != 1)
    {
      ndbout << "Final scan with lock did not complete successfully" << endl;
      ndbout << otherScan->getNdbError() << endl;
      ndbout << "at line " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    otherScan->close();
    otherTrans->close();

    ndbout << "All locked rows unlocked" << endl;

    trans->close();
  }

  return NDBT_OK;
}

#include <NdbMgmd.hpp>

class NodeIdReservations {
  bool m_ids[MAX_NODES];
  NdbMutex m_mutex;
public:
  void lock(unsigned id)
  {
    require(id < NDB_ARRAY_SIZE(m_ids));
    NdbMutex_Lock(&m_mutex);
    //ndbout  << "locking nodeid: " << id << endl;
    if (m_ids[id])
    {
      //already locked!
      g_err << "Nodeid " << id << " is already locked! Crashing!" << endl;
      abort();
    }
    m_ids[id] = true;
    NdbMutex_Unlock(&m_mutex);
  }

  void unlock(unsigned id)
  {
    require(id < NDB_ARRAY_SIZE(m_ids));
    NdbMutex_Lock(&m_mutex);
    //ndbout  << "unlocking nodeid: " << id << endl;
    if (!m_ids[id])
    {
      //already unlocked!
      abort();
    }
    m_ids[id] = false;
    NdbMutex_Unlock(&m_mutex);
  }

  NodeIdReservations() {
    bzero(m_ids, sizeof(m_ids));
    NdbMutex_Init(&m_mutex);
  }

  class Reserve {
    unsigned m_id;
    NodeIdReservations& m_res;

    Reserve(); // Not impl.
    Reserve(const Reserve&); // Not impl.
  public:
    Reserve(NodeIdReservations& res, unsigned id) :
        m_id(id), m_res(res) {
      m_res.lock(m_id);
    }

    void unlock() {
      m_res.unlock(m_id);
      m_id = 0;
    }

    ~Reserve(){
      if (m_id)
      {
        m_res.unlock(m_id);
      }
    }
  };
};

NodeIdReservations g_reservations;


int runNdbClusterConnectInit(NDBT_Context* ctx, NDBT_Step* step)
{
  // Find number of unconnected API nodes slot to use for test
  Uint32 api_nodes = 0;
  {
    NdbMgmd mgmd;

    if (!mgmd.connect())
      return NDBT_FAILED;

    ndb_mgm_node_type
      node_types[2] = { NDB_MGM_NODE_TYPE_API,
                        NDB_MGM_NODE_TYPE_UNKNOWN };

    ndb_mgm_cluster_state *cs = ndb_mgm_get_status2(mgmd.handle(), node_types);
    if (cs == NULL)
    {
      printf("ndb_mgm_get_status2 failed, error: %d - %s\n",
             ndb_mgm_get_latest_error(mgmd.handle()),
             ndb_mgm_get_latest_error_msg(mgmd.handle()));
      return NDBT_FAILED;
    }

    for(int i = 0; i < cs->no_of_nodes; i++ )
    {
      ndb_mgm_node_state *ns = cs->node_states + i;
      require(ns->node_type == NDB_MGM_NODE_TYPE_API);
      if (ns->node_status == NDB_MGM_NODE_STATUS_CONNECTED)
      {
        // Node is already connected, don't use in test
        continue;
      }
      api_nodes++;
    }
    free(cs);
  }

  if (api_nodes <= 1)
  {
    ndbout << "Too few API node slots available, failing test" << endl;
    return NDBT_FAILED;
  }
  // Don't try to use nodeid allocated by main cluster connection
  api_nodes--;

  ndbout << "Found " << api_nodes << " unconnected API nodes" << endl;
  ctx->setProperty("API_NODES", api_nodes);
  return NDBT_OK;
}


int runNdbClusterConnect(NDBT_Context* ctx, NDBT_Step* step)
{
  const Uint32 api_nodes = ctx->getProperty("API_NODES");
  const Uint32 step_no = step->getStepNo();
  const Uint32 timeout_after_first_alive = ctx->getProperty("TimeoutAfterFirst",
                                                            30);
  if (step_no > api_nodes)
  {
    // Don't run with more threads than API node slots
    return NDBT_OK;
  }

  // Get connectstring from main connection
  char constr[256];
  if (!ctx->m_cluster_connection.get_connectstring(constr,
                                                   sizeof(constr)))
  {
    g_err << "Too short buffer for connectstring" << endl;
    return NDBT_FAILED;
  }

  Uint32 l = 0;
  const Uint32 loops = ctx->getNumLoops();
  while (l < loops && !ctx->isTestStopped())
  {
    g_info << "loop: " << l << endl;
    if (ctx->getProperty("WAIT") > 0)
    {
      ndbout_c("thread %u waiting", step_no);
      ctx->incProperty("WAITING");
      while (ctx->getProperty("WAIT") > 0 && !ctx->isTestStopped())
        NdbSleep_MilliSleep(10);
      ndbout_c("thread %u waiting complete", step_no);
    }
    Ndb_cluster_connection con(constr);

    const int retries = 12;
    const int retry_delay = 5;
    const int verbose = 1;
    if (con.connect(retries, retry_delay, verbose) != 0)
    {
      g_err << "Ndb_cluster_connection.connect failed" << endl;
      g_err << "Error code: "
            << con.get_latest_error()
            << " message: "
            << con.get_latest_error_msg()
            << endl;
      return NDBT_FAILED;
    }

    // Check that the connection got a unique nodeid
    NodeIdReservations::Reserve res(g_reservations, con.node_id());

    const int timeout = 30;
    int ret = con.wait_until_ready(timeout, timeout_after_first_alive);
    if (! (ret == 0 || (timeout_after_first_alive == 0 && ret > 0)))
    {
      g_err << "Cluster connection was not ready, nodeid: "
            << con.node_id() << endl;
      g_err << "Error code: "
            << con.get_latest_error()
            << " message: "
            << con.get_latest_error_msg()
            << endl;
      abort();
      return NDBT_FAILED;
    }

    // Create and init Ndb object
    Ndb ndb(&con, "TEST_DB");
    if (ndb.init() != 0)
    {
      NDB_ERR(ndb.getNdbError());
      return NDBT_FAILED;
    }

    const int max_sleep = 25;
    NdbSleep_MilliSleep(10 + rand() % max_sleep);

    l++;
    res.unlock(); // make sure it's called before ~Ndb_cluster_connection
  }

  ctx->incProperty("runNdbClusterConnect_FINISHED");

  return NDBT_OK;
}

int
runRestarts(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  Uint32 threads = ctx->getProperty("API_NODES", (unsigned)0);
  Uint32 sr = ctx->getProperty("ClusterRestart", (unsigned)0);
  Uint32 master = ctx->getProperty("Master", (unsigned)0);
  Uint32 slow = ctx->getProperty("SlowNR", (unsigned)0);
  Uint32 slowNoStart = ctx->getProperty("SlowNoStart", (unsigned)0);
  NdbRestarter restarter;

  if (restarter.waitClusterStarted() != 0)
  {
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  if (sr == 0 && restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  while (ctx->getProperty("runNdbClusterConnect_FINISHED") < threads
         && !ctx->isTestStopped())
  {
    ndbout_c("%u %u",
             ctx->getProperty("runNdbClusterConnect_FINISHED"),
             threads);
    if (sr == 0)
    {
      int id = rand() % restarter.getNumDbNodes();
      int nodeId = restarter.getDbNodeId(id);
      if (master == 1)
      {
        nodeId = restarter.getMasterNodeId();
      }
      else if (master == 2)
      {
        nodeId = restarter.getRandomNotMasterNodeId(rand());
      }
      ndbout << "Restart node " << nodeId
             << "(master: " << restarter.getMasterNodeId() << ")"
             << endl;
      if (restarter.restartOneDbNode(nodeId, false, true, true) != 0)
      {
        g_err << "Failed to restartNextDbNode" << endl;
        result = NDBT_FAILED;
        break;
      }

      if (restarter.waitNodesNoStart(&nodeId, 1))
      {
        g_err << "Failed to waitNodesNoStart" << endl;
        result = NDBT_FAILED;
        break;
      }

      if (slowNoStart)
      {
        /**
         * Spend some time in the NOT_STARTED state, as opposed
         * to some substate of STARTING
         */
        Uint32 blockTime = 3 * 60 * 1000;
        Uint64 end = NdbTick_CurrentMillisecond() + blockTime;
        while (ctx->getProperty("runNdbClusterConnect_FINISHED") < threads
               && !ctx->isTestStopped() &&
               NdbTick_CurrentMillisecond() < end)
        {
          NdbSleep_MilliSleep(100);
        }
      }

      if (slow)
      {
        /**
         * Block starting node in sp4
         */
        int dump[] = { 71, 4 };
        restarter.dumpStateOneNode(nodeId, dump, NDB_ARRAY_SIZE(dump));
      }

      if (restarter.startNodes(&nodeId, 1))
      {
        g_err << "Failed to start node" << endl;
        result = NDBT_FAILED;
        break;
      }

      if (slow)
      {
        Uint32 blockTime = 3 * 60 * 1000;
        Uint64 end = NdbTick_CurrentMillisecond() + blockTime;
        while (ctx->getProperty("runNdbClusterConnect_FINISHED") < threads
               && !ctx->isTestStopped() &&
               NdbTick_CurrentMillisecond() < end)
        {
          NdbSleep_MilliSleep(100);
        }

        // unblock
        int dump[] = { 71 };
        restarter.dumpStateOneNode(nodeId, dump, NDB_ARRAY_SIZE(dump));
      }
    }
    else
    {
      ndbout << "Blocking threads" << endl;
      ctx->setProperty("WAITING", Uint32(0));
      ctx->setProperty("WAIT", 1);
      while (ctx->getProperty("WAITING") <
             (threads - ctx->getProperty("runNdbClusterConnect_FINISHED")) &&
             !ctx->isTestStopped())
      {
        NdbSleep_MilliSleep(10);
      }

      ndbout << "Restart cluster" << endl;
      if (restarter.restartAll2(Uint32(NdbRestarter::NRRF_NOSTART |
                                       NdbRestarter::NRRF_ABORT)) != 0)
      {
        g_err << "Failed to restartAll" << endl;
        result = NDBT_FAILED;
        break;
      }

      ctx->setProperty("WAITING", Uint32(0));
      ctx->setProperty("WAIT", Uint32(0));

      ndbout << "Starting cluster" << endl;
      restarter.startAll();
    }

    if (restarter.waitClusterStarted() != 0)
    {
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }
  }

  return result;
}

int runCheckAllNodesStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if (restarter.waitClusterStarted(1) != 0)
  {
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}



static bool
check_connect_no_such_host()
{
  for (int i = 0; i < 3; i++)
  {
    const char* no_such_host = "no_such_host:1186";
    Ndb_cluster_connection con(no_such_host);

    const int verbose = 1;
    int res = con.connect(i, i, verbose);
    if (res != 1)
    {
      g_err << "Ndb_cluster_connection.connect(" << i << "," << i
            << ", 1) to '" << no_such_host << "' returned " << res
            << " instead of expected 1" << endl;
      return false;
    }
    g_info << "Ndb_cluster_connection.connect(" << i << "," << i
           << ", 1) to '" << no_such_host << "' returned " << res
           << " and message '" << con.get_latest_error_msg() << "'"<< endl;
  }
  return true;
}


static bool
check_connect_until_no_more_nodeid(const char* constr)
{
  bool result = true;
  Vector<Ndb_cluster_connection*> connections;
  while(true)
  {
    Ndb_cluster_connection* con = new Ndb_cluster_connection(constr);
    if (!con)
    {
      g_err << "Failed to create another Ndb_cluster_connection" << endl;
      result = false;
      break;
    }
    connections.push_back(con);
    g_info << "connections: " << connections.size() << endl;

    const int verbose = 1;
    int res = con->connect(0, 0, verbose);
    if (res != 0)
    {
      g_info << "Ndb_cluster_connection.connect(0,0,1) returned " << res
             << " and error message set to : '" << con->get_latest_error_msg()
             << "'" << endl;

      if (res != 1)
      {
        // The error returned should be 1
        g_err << "Unexpected return code " << res << " returned" << endl;
        result = false;
      }
      else if (strstr(con->get_latest_error_msg(),
                      "No free node id found for mysqld(API)") == NULL)
      {
        // The error message should end with "No free node id
        // found for mysqld(API)" since this host is configured in the config
        g_err << "Unexpected error message " << con->get_latest_error_msg()
              << " returned" << endl;
        result = false;
      }
      else
      {
        ndbout << "check_connect_until_no_more_nodeid OK!" << endl;
      }
      break;
    }
  }

  while(connections.size())
  {
    Ndb_cluster_connection* con = connections[0];
    g_info << "releasing connection, size: " << connections.size() << endl;
    delete con;
    connections.erase(0);
  }
  require(connections.size() == 0);

  return result;
}


int runNdbClusterConnectionConnect(NDBT_Context* ctx, NDBT_Step* step)
{
  // Get connectstring from main connection
  char constr[256];
  if(!ctx->m_cluster_connection.get_connectstring(constr,
                                                  sizeof(constr)))
  {
    g_err << "Too short buffer for connectstring" << endl;
    return NDBT_FAILED;
  }

  if (!check_connect_no_such_host() ||
      !check_connect_until_no_more_nodeid(constr))
  {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/* Testing fragmented signal send/receive */

/*
  SectionStore

  Abstraction of long section storage api.
  Used by FragmentAssembler to assemble received long sections
*/
class SectionStore
{
public:
  virtual ~SectionStore() {};
  virtual int appendToSection(Uint32 secId, LinearSectionPtr ptr) = 0;
};

/*
  Basic Section Store

  Naive implementation using malloc.  Real usage might use something better.
*/
class BasicSectionStore : public SectionStore
{
public:
  BasicSectionStore()
  {
    init();
  };

  ~BasicSectionStore()
  {
    freeStorage();
  };

  void init()
  {
    ptrs[0].p = NULL;
    ptrs[0].sz = 0;

    ptrs[2] = ptrs[1] = ptrs[0];
  }

  void freeStorage()
  {
    free(ptrs[0].p);
    free(ptrs[1].p);
    free(ptrs[2].p);
  }

  virtual int appendToSection(Uint32 secId, LinearSectionPtr ptr)
  {
    /* Potentially expensive re-alloc + copy */
    require(secId < 3);
    
    Uint32 existingSz = ptrs[secId].sz;
    Uint32* existingBuff = ptrs[secId].p;

    Uint32 newSize = existingSz + ptr.sz;
    Uint32* newBuff = (Uint32*) realloc(existingBuff, newSize * 4);

    if (!newBuff)
      return -1;
    
    memcpy(newBuff + existingSz, ptr.p, ptr.sz * 4);
    
    ptrs[secId].p = newBuff;
    ptrs[secId].sz = existingSz + ptr.sz;

    return 0;
  }
    
  LinearSectionPtr ptrs[3];
};



/*
  FragmentAssembler

  Used to assemble sections from multiple fragment signals, and 
  produce a 'normal' signal.
  
  Requires a SectionStore implementation to accumulate the section
  fragments

  Might be useful generic utility, or not.

  Usage : 
    FragmentAssembler fa(ss);
    while (!fa.isComplete())
    {
      sig = waitSignal();
      ss.handleSignal(sig, sections);
    }

    fa.getSignalHeader();
    fa.getSignalBody();
    fa.getSectionStore(); ..

*/
class FragmentAssembler
{
public:
  enum AssemblyError
  {
    NoError = 0,
    FragmentSequence = 1,
    FragmentSource = 2,
    FragmentIdentity = 3,
    SectionAppend = 4
  };

  FragmentAssembler(SectionStore* _secStore):
    secsReceived(0),
    secStore(_secStore),
    complete(false),
    fragId(0),
    sourceNode(0),
    error(NoError)
  {}

  int handleSignal(const SignalHeader* sigHead,
                   const Uint32* sigBody,
                   LinearSectionPtr* sections)
  {
    Uint32 sigLen = sigHead->theLength;
    
    if (fragId == 0)
    {
      switch (sigHead->m_fragmentInfo)
      {
      case 0:
      {
        /* Not fragmented, pass through */
        sh = *sigHead;
        memcpy(signalBody, sigBody, sigLen * 4);
        Uint32 numSecs = sigHead->m_noOfSections;
        for (Uint32 i=0; i<numSecs; i++)
        {
          if (secStore->appendToSection(i, sections[i]) != 0)
          {
            error = SectionAppend;
            return -1;
          }
        }
        complete = true;
        break;
      }
      case 1:
      {
        /* Start of fragmented signal */
        Uint32 incomingFragId;
        Uint32 incomingSourceNode;
        Uint32 numSecsInFragment;
        
        if (handleFragmentSections(sigHead, sigBody, sections,
                                   &incomingFragId, &incomingSourceNode,
                                   &numSecsInFragment) != 0)
          return -1;
        
        require(incomingFragId != 0);
        fragId = incomingFragId;
        sourceNode = incomingSourceNode;
        require(numSecsInFragment > 0);
        
        break;
      }
      default:
      {
        /* Error, out of sequence fragment */
        error = FragmentSequence;
        return -1;
        break;
      }
      }
    }
    else
    {
      /* FragId != 0 */
      switch (sigHead->m_fragmentInfo)
      {
      case 0:
      case 1:
      {
        /* Error, out of sequence fragment */
        error = FragmentSequence;
        return -1;
      }
      case 2:
        /* Fall through */
      case 3:
      {
        /* Body fragment */
        Uint32 incomingFragId;
        Uint32 incomingSourceNode;
        Uint32 numSecsInFragment;
        
        if (handleFragmentSections(sigHead, sigBody, sections,
                                   &incomingFragId, &incomingSourceNode,
                                   &numSecsInFragment) != 0)
          return -1;

        if (incomingSourceNode != sourceNode)
        {
          /* Error in source node */
          error = FragmentSource;
          return -1;
        }
        if (incomingFragId != fragId)
        {
          error = FragmentIdentity;
          return -1;
        }
        
        if (sigHead->m_fragmentInfo == 3)
        {
          /* Final fragment, contains actual signal body */
          memcpy(signalBody,
                 sigBody,
                 sigLen * 4);
          sh = *sigHead;
          sh.theLength = sigLen - (numSecsInFragment + 1);
          sh.m_noOfSections = 
            ((secsReceived & 4)? 1 : 0) +
            ((secsReceived & 2)? 1 : 0) +
            ((secsReceived & 1)? 1 : 0);
          sh.m_fragmentInfo = 0;
          
          complete=true;
        }
        break;
      }
      default:
      {
        /* Bad fragmentinfo field */
        error = FragmentSequence;
        return -1;
      }
      }
    }

    return 0;
  }

  int handleSignal(NdbApiSignal* signal,
                   LinearSectionPtr* sections)
  {
    return handleSignal(signal, signal->getDataPtr(), sections);
  }

  bool isComplete()
  {
    return complete;
  }

  /* Valid if isComplete() */
  SignalHeader getSignalHeader()
  {
    return sh;
  }
  
  /* Valid if isComplete() */
  Uint32* getSignalBody()
  {
    return signalBody;
  }

  /* Valid if isComplete() */
  Uint32 getSourceNode()
  {
    return sourceNode;
  }

  SectionStore* getSectionStore()
  {
    return secStore;
  }

  AssemblyError getError() const
  {
    return error;
  }
  
private:
  int handleFragmentSections(const SignalHeader* sigHead,
                             const Uint32* sigBody,
                             LinearSectionPtr* sections,
                             Uint32* incomingFragId,
                             Uint32* incomingSourceNode,
                             Uint32* numSecsInFragment)
  {
    Uint32 sigLen = sigHead->theLength;
    
    *numSecsInFragment = sigHead->m_noOfSections;
    require(sigLen >= (1 + *numSecsInFragment));
           
    *incomingFragId = sigBody[sigLen - 1];
    *incomingSourceNode = refToNode(sigHead->theSendersBlockRef);
    const Uint32* secIds = &sigBody[sigLen - (*numSecsInFragment) - 1];
    
    for (Uint32 i=0; i < *numSecsInFragment; i++)
    {
      secsReceived |= (1 < secIds[i]);
      
      if (secStore->appendToSection(secIds[i], sections[i]) != 0)
      {
        error = SectionAppend;
        return -1;
      }
    }
    
    return 0;
  }

  Uint32 secsReceived;
  SectionStore* secStore;
  bool complete;
  Uint32 fragId;
  Uint32 sourceNode;
  SignalHeader sh;
  Uint32 signalBody[NdbApiSignal::MaxSignalWords];
  AssemblyError error;
};                 

static const Uint32 MAX_SEND_BYTES=32768; /* Align with TransporterDefinitions.hpp */
static const Uint32 MAX_SEND_WORDS=MAX_SEND_BYTES/4;
static const Uint32 SEGMENT_WORDS= 60; /* Align with SSPool etc */
static const Uint32 SEGMENT_BYTES = SEGMENT_WORDS * 4;
//static const Uint32 MAX_SEGS_PER_SEND=64; /* 6.3 */
static const Uint32 MAX_SEGS_PER_SEND = (MAX_SEND_BYTES / SEGMENT_BYTES) - 2; /* Align with TransporterFacade.cpp */
static const Uint32 MAX_WORDS_PER_SEND = MAX_SEGS_PER_SEND * SEGMENT_WORDS;
static const Uint32 HALF_MAX_WORDS_PER_SEND = MAX_WORDS_PER_SEND / 2;
static const Uint32 THIRD_MAX_WORDS_PER_SEND = MAX_WORDS_PER_SEND / 3;
static const Uint32 MEDIUM_SIZE = 5000;

/* Most problems occurred with sections lengths around the boundary
 * of the max amount sent - MAX_WORDS_PER_SEND, so we define interesting
 * sizes so that we test behavior around these boundaries
 */
static Uint32 interestingSizes[] = 
{
  0,
  1, 
  MEDIUM_SIZE,
  THIRD_MAX_WORDS_PER_SEND -1,
  THIRD_MAX_WORDS_PER_SEND,
  THIRD_MAX_WORDS_PER_SEND +1,
  HALF_MAX_WORDS_PER_SEND -1,
  HALF_MAX_WORDS_PER_SEND,
  HALF_MAX_WORDS_PER_SEND + 1,
  MAX_WORDS_PER_SEND -1, 
  MAX_WORDS_PER_SEND, 
  MAX_WORDS_PER_SEND + 1,
  (2* MAX_SEND_WORDS) + 1,
  1234 /* Random */
};


/* 
   FragSignalChecker

   Class for testing fragmented signal send + receive
*/
class FragSignalChecker
{
public:

  Uint32* buffer;

  FragSignalChecker()
  {
    buffer= NULL;
    init();
  }

  ~FragSignalChecker()
  {
    free(buffer);
  }

  void init()
  {
    buffer = (Uint32*) malloc(getBufferSize());

    if (buffer)
    {
      /* Init to a known pattern */
      for (Uint32 i = 0; i < (getBufferSize()/4); i++)
      {
        buffer[i] = i;
      }
    }
  }

  static Uint32 getNumInterestingSizes()
  {
    return sizeof(interestingSizes) / sizeof(Uint32);
  }

  static Uint32 getNumIterationsRequired()
  {
    /* To get combinatorial coverage, need each of 3
     * sections with each of the interesting sizes
     */
    Uint32 numSizes = getNumInterestingSizes();
    return numSizes * numSizes * numSizes;
  }

  static Uint32 getSecSz(Uint32 secNum, Uint32 iter)
  {
    require(secNum < 3);
    Uint32 numSizes = getNumInterestingSizes();
    Uint32 divisor = (secNum == 0 ? 1 : 
                      secNum == 1 ? numSizes :
                      numSizes * numSizes);
    /* offset ensures only end sections are 0 length */
    Uint32 index = (iter / divisor) % numSizes;
    if ((index == 0) && (iter >= (divisor * numSizes)))
      index = 1; /* Avoid lower numbered section being empty */
    Uint32 value = interestingSizes[index];
    if(value == 1234)
    {
      value = 1 + (rand() % (2* MAX_WORDS_PER_SEND));
    }
    return value;
  }

  static Uint32 getBufferSize()
  {
    const Uint32 MaxSectionWords = (2 * MAX_SEND_WORDS) + 1;
    const Uint32 MaxTotalSectionsWords = MaxSectionWords * 3;
    return MaxTotalSectionsWords * 4;
  }

  int sendRequest(SignalSender* ss, 
                  Uint32* sizes)
  {
    /* 
     * We want to try out various interactions between the
     * 3 sections and the length of the data sent
     * - All fit in one 'chunk'
     * - None fit in one 'chunk'
     * - Each ends on a chunk boundary
     *
     * Max send size is ~ 32kB
     * Segment size is 60 words / 240 bytes
     *  -> 136 segments / chunk
     *  -> 134 segments / chunk 'normally' sent
     *  -> 32160 bytes
     */
    g_err << "Sending "
          << sizes[0]
          << " " << sizes[1]
          << " " << sizes[2]
          << endl;
    
    const Uint32 numSections = 
      (sizes[0] ? 1 : 0) + 
      (sizes[1] ? 1 : 0) + 
      (sizes[2] ? 1 : 0);
    const Uint32 testType = 40;
    const Uint32 fragmentLength = 1;
    const Uint32 print = 0;
    const Uint32 len = 5 + numSections;
    SimpleSignal request(false);
    
    Uint32* signalBody = request.getDataPtrSend();
    signalBody[0] = ss->getOwnRef();
    signalBody[1] = testType;
    signalBody[2] = fragmentLength;
    signalBody[3] = print;
    signalBody[4] = 0; /* Return count */
    signalBody[5] = sizes[0];
    signalBody[6] = sizes[1];
    signalBody[7] = sizes[2];
    
    
    request.ptr[0].sz = sizes[0];
    request.ptr[0].p = &buffer[0];
    request.ptr[1].sz = sizes[1];
    request.ptr[1].p = &buffer[sizes[0]];
    request.ptr[2].sz = sizes[2];
    request.ptr[2].p = &buffer[sizes[0] + sizes[1]];
    
    request.header.m_noOfSections= numSections;
    
    int rc = 0;
    ss->lock();
    rc = ss->sendFragmentedSignal(ss->get_an_alive_node(),
                                  request,
                                  CMVMI,
                                  GSN_TESTSIG,
                                  len);
    ss->unlock();
    
    if (rc != 0)
    {
      g_err << "Error sending signal" << endl;
      return rc;
    }
    
    return 0;
  }

  int waitResponse(SignalSender* ss,
                   Uint32* expectedSz)
  {
    /* Here we need to wait for all of the signals which
     * comprise a fragmented send, and check that
     * the data is as expected
     */
    BasicSectionStore bss;
    FragmentAssembler fa(&bss);
    
    while(true)
    {
      ss->lock();
      SimpleSignal* response = ss->waitFor(10000);
      ss->unlock();
      
      if (!response)
      {
        g_err << "Timed out waiting for response" << endl;
        return -1;
      }
      
      //response->print();
      
      if (response->header.theVerId_signalNumber == GSN_TESTSIG)
      {
        if (fa.handleSignal(&response->header,
                            response->getDataPtr(),
                            response->ptr) != 0)
        {
          g_err << "Error assembling fragmented signal."
                << "  Error is "
                << (Uint32) fa.getError()
                << endl;
          return -1;
        }
        
        if (fa.isComplete())
        {
          Uint32 expectedWord = 0;
          for (Uint32 i=0; i < 3; i++)
          {
            if (bss.ptrs[i].sz != expectedSz[i])
            {
              g_err << "Wrong size for section : "
                    << i
                    << " expected " << expectedSz[i]
                    << " but received " << bss.ptrs[i].sz
                    << endl;
              return -1;
            }
            
            for (Uint32 d=0; d < expectedSz[i]; d++)
            {
              if (bss.ptrs[i].p[d] != expectedWord)
              {
                g_err << "Bad data in section "
                      << i
                      << " at word number "
                      << d
                      << ".  Expected "
                      << expectedWord
                      << " but found "
                      << bss.ptrs[i].p[d]
                      << endl;
                return -1;
              }
              expectedWord++;
            }
          }
          
          break;
        }
        
      }
    }
    
    return 0;
  }
  
  int runTest(SignalSender* ss)
  {
    for (Uint32 iter=0; 
         iter < getNumIterationsRequired(); 
         iter++)
    {
      int rc;
      Uint32 sizes[3];
      sizes[0] = getSecSz(0, iter);
      sizes[1] = getSecSz(1, iter);
      sizes[2] = getSecSz(2, iter);
      
      /* Build request, including sections */
      rc = sendRequest(ss, sizes);
      if (rc != 0)
      {
        g_err << "Failed sending request on iteration " << iter 
              << " with rc " << rc << endl;
        return NDBT_FAILED;
      }
      
      /* Wait for response */
      rc = waitResponse(ss, sizes);
      if (rc != 0)
      {
        g_err << "Failed waiting for response on iteration " << iter
              << " with rc " << rc << endl;
        return NDBT_FAILED;
      }
    }
    
    return NDBT_OK;
  }
};


int testFragmentedSend(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb= GETNDB(step);
  Ndb_cluster_connection* conn = &pNdb->get_ndb_cluster_connection();
  SignalSender ss(conn);
  FragSignalChecker fsc;
  
  return fsc.runTest(&ss);
}

static int
runReceiveTRANSIDAIAfterRollback(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* const ndb = GETNDB(step);
  NdbRestarter restarter;

  do { 
    // fill table with 10 rows.
    const NdbDictionary::Table * pTab = ctx->getTab();
    HugoTransactions hugoTrans(*pTab);
    if(hugoTrans.loadTable(ndb, 10) != 0) {
      g_err << "Failed to load table" << endl;
      break;
    }
    // do error injection in data nodes
    if (restarter.insertErrorInAllNodes(8107) != 0){
      g_err << "Failed to insert error 8107" << endl;
      break;
    }
    if (restarter.insertErrorInAllNodes(4037) != 0){
      g_err << "Failed to insert error 4037" << endl;
      break;
    }
  
    // do error injection in ndbapi
    DBUG_SET_INITIAL("+d,ndb_delay_close_txn,ndb_delay_transid_ai");
  
    // start transaction
    NdbTransaction* const trans = ndb->startTransaction();
    if (trans == NULL)
    {
      g_err << "ndb->startTransaction() gave unexpected error : "
            << ndb->getNdbError() << endl;
      break;
    }
    NdbOperation* const op = trans->getNdbOperation(pTab);
    if (op == NULL)
    {
      g_err << "trans->getNdbOperation() gave unexpected error : "
            << trans->getNdbError() << endl;
      break;
    }
  
    // start primary key read with shared lock
    HugoOperations hugoOps(*ctx->getTab());
    if(hugoOps.startTransaction(ndb)) {
      g_err << "hugoOps.startTransaction() gave unexpected error : " 
            << hugoOps.getTransaction()->getNdbError() << endl;
      break;
    }
    if(hugoOps.pkReadRecord(ndb, 1, 1, NdbOperation::LM_Read)) {
      g_err << "hugoOps.pkReadRecord() gave unexpected error : " 
            << hugoOps.getTransaction()->getNdbError() << endl;
      break;
    }
    if(hugoOps.execute_Commit(ndb) != 0) {
      g_err << "hugoOps.execute_Commit() gave unexpected error : " 
            << hugoOps.getTransaction()->getNdbError() << endl;
      break;
    }
  
    // all ok, test passes 
    ndb->closeTransaction(trans);
  
    // clean up 
    DBUG_SET_INITIAL("-d,ndb_delay_close_txn,ndb_delay_transid_ai");
    restarter.insertErrorInAllNodes(0);
    return NDBT_OK;
  } while(0);

  // clean up for error path 
  DBUG_SET_INITIAL("-d,ndb_delay_close_txn,ndb_delay_transid_ai");
  restarter.insertErrorInAllNodes(0);
  return NDBT_FAILED;
}

int
testNdbRecordSpecificationCompatibility(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Test for checking the compatibility of RecordSpecification
   * when compiling old code with newer header.
   * Create an instance of RecordSpecification_v1 and try to pass
   * it to the NdbApi createRecord.
   */

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab= ctx->getTab();
  int numCols= pTab->getNoOfColumns();
  const NdbRecord* defaultRecord= pTab->getDefaultRecord();

  NdbDictionary::RecordSpecification_v1 rsArray[ NDB_MAX_ATTRIBUTES_IN_TABLE ];

  for (int attrId=0; attrId< numCols; attrId++)
  {
    NdbDictionary::RecordSpecification_v1& rs= rsArray[attrId];

    rs.column= pTab->getColumn(attrId);
    rs.offset= 0;
    rs.nullbit_byte_offset= 0;
    rs.nullbit_bit_in_byte= 0;
    CHECK(NdbDictionary::getOffset(defaultRecord,
                                   attrId,
                                   rs.offset));
    CHECK(NdbDictionary::getNullBitOffset(defaultRecord,
                                          attrId,
                                          rs.nullbit_byte_offset,
                                          rs.nullbit_bit_in_byte));
  }
  const NdbRecord* tabRec= pNdb->getDictionary()->createRecord(pTab,
                              (NdbDictionary::RecordSpecification*)rsArray,
                              numCols,
                              sizeof(NdbDictionary::RecordSpecification_v1));
  CHECK(tabRec != 0);

  char keyRowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
  char attrRowBuf[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
  bzero(keyRowBuf, sizeof(keyRowBuf));
  bzero(attrRowBuf, sizeof(attrRowBuf));

  HugoCalculator calc(*pTab);

  const int numRecords= 100;

  for (int record=0; record < numRecords; record++)
  {
    int updates= 0;
    /* calculate the Hugo values for this row */
    for (int col=0; col<pTab->getNoOfColumns(); col++)
    {
      char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                               keyRowBuf,
                                               col);
      CHECK(valPtr != NULL);
      int len= pTab->getColumn(col)->getSizeInBytes();
      Uint32 real_len;
      bool isNull= (calc.calcValue(record, col, updates, valPtr,
                                   len, &real_len) == NULL);
      if (pTab->getColumn(col)->getNullable())
      {
        NdbDictionary::setNull(tabRec,
                               keyRowBuf,
                               col,
                               isNull);
      }
    }

    /* insert the row */
    NdbTransaction* trans=pNdb->startTransaction();
    CHECK(trans != 0);
    CHECK(trans->getNdbError().code == 0);

    const NdbOperation* op= NULL;
    op= trans->insertTuple(tabRec,
                           keyRowBuf);
    CHECK(op != 0);

    CHECK(trans->execute(Commit) == 0);
    trans->close();

    /* Now read back */
    Uint32 pkVal= 0;
    memcpy(&pkVal, NdbDictionary::getValuePtr(tabRec,
                                              keyRowBuf,
                                              0),
           sizeof(pkVal));

    trans= pNdb->startTransaction();
    op= trans->readTuple(tabRec,
                         keyRowBuf,
                         tabRec,
                         attrRowBuf);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);
    CHECK(trans->getNdbError().code == 0);
    trans->close();

    /* Verify the values read back */
    for (int col=0; col<pTab->getNoOfColumns(); col++)
    {
      const char* valPtr= NdbDictionary::getValuePtr(tabRec,
                                                     attrRowBuf,
                                                     col);
      CHECK(valPtr != NULL);

      char calcBuff[ NDB_MAX_TUPLE_SIZE_IN_WORDS << 2 ];
      int len= pTab->getColumn(col)->getSizeInBytes();
      Uint32 real_len;
      bool isNull= (calc.calcValue(record, col, updates, calcBuff,
                                   len, &real_len) == NULL);
      bool colIsNullable= pTab->getColumn(col)->getNullable();
      if (isNull)
      {
        CHECK(colIsNullable);
        if (!NdbDictionary::isNull(tabRec,
                                   attrRowBuf,
                                   col))
        {
          ndbout << "Error, col " << col
              << " (pk=" <<  pTab->getColumn(col)->getPrimaryKey()
                  << ") should be Null, but is not" << endl;
          return NDBT_FAILED;
        }
      }
      else
      {
        if (colIsNullable)
        {
          if (NdbDictionary::isNull(tabRec,
                                    attrRowBuf,
                                    col))
          {
            ndbout << "Error, col " << col
                << " (pk=" << pTab->getColumn(col)->getPrimaryKey()
                << ") should be non-Null but is null" << endl;
            return NDBT_FAILED;
          };
        }

        /* Compare actual data read back */
        if( memcmp(calcBuff, valPtr, real_len) != 0 )
        {
          ndbout << "Error, col " << col
              << " (pk=" << pTab->getColumn(col)->getPrimaryKey()
              << ") should be equal, but isn't for record "
              << record << endl;
          ndbout << "Expected :";
          for (Uint32 i=0; i < real_len; i++)
          {
            ndbout_c("%x ", calcBuff[i]);
          }
          ndbout << endl << "Received :";
          for (Uint32 i=0; i < real_len; i++)
          {
            ndbout_c("%x ", valPtr[i]);
          }
          ndbout << endl;

          return NDBT_FAILED;
        }
      }
    }

    /* Now delete the tuple */
    trans= pNdb->startTransaction();
    op= trans->deleteTuple(tabRec,
                           keyRowBuf,
                           tabRec);
    CHECK(op != 0);
    CHECK(trans->execute(Commit) == 0);

    trans->close();
  }

  return NDBT_OK;
}

int testSchemaObjectOwnerCheck(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* const ndb = GETNDB(step);
  Ndb *otherNdb = NULL;
  NdbDictionary::Dictionary* const dict = ndb->getDictionary();
  NdbTransaction *trans = ndb->startTransaction();
  NdbRestarter restarter;
  int result = NDBT_OK;

  do
  {
    ndbout << "Creating table with index" << endl;
    NdbDictionary::Table tab;
    NdbDictionary::Index idx;
    tab.setName("SchemaObjOwnerCheck_tab");
    tab.setLogging(true);

    // create column
    NdbDictionary::Column col("col1");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);

    // create index on column
    idx.setTable("SchemaObjOwnerCheck_tab");
    idx.setName("SchemaObjOwnerCheck_idx");
    idx.setType(NdbDictionary::Index::UniqueHashIndex);
    idx.setLogging(false);
    idx.addColumnName("col1");

    NdbError error;
    if(tab.validate(error) == -1)
    {
      ndbout << "Failed to create table" << endl;
      break;
    }
    
    if (dict->createTable(tab) == -1) {
      g_err << "Failed to create SchemaObjOwnerCheck_tab table." << endl;
      result = NDBT_FAILED;
      break;
    }
    if (dict->createIndex(idx) == -1) {
      g_err << "Failed to create index, error: " << dict->getNdbError() << endl;
      result = NDBT_FAILED;
      break;
    }

    ndbout << "Setting up other connection to acquire schema objects." << endl;
    char connectString[256];
    ctx->m_cluster_connection.get_connectstring(connectString,
                                                sizeof(connectString));
    otherConnection= new Ndb_cluster_connection(connectString);
    if (otherConnection == NULL)
    {
      ndbout << "otherConnection is null" << endl;
      result = NDBT_FAILED;
      break;
    }
    int rc= otherConnection->connect();
    if (rc != 0)
    {
      ndbout << "Connect of otherConnection failed with rc " << rc << endl;
      result = NDBT_FAILED;
      break;
    }
    if (otherConnection->wait_until_ready(10,10) != 0)
    {
      ndbout << "Cluster connection otherConnection was not ready" << endl;
      result = NDBT_FAILED;
      break;
    }
    otherNdb = new Ndb(otherConnection, "TEST_DB");
    if(!otherNdb)
    {
      ndbout << "Failed to acquire Ndb object from otherConnection" << endl;
      result = NDBT_FAILED;
      break;
    }
    otherNdb->init();
    if(otherNdb->waitUntilReady(10) != 0)
    {
      ndbout << "Failed to init Ndb object from otherConnection" << endl;
      result = NDBT_FAILED;
      break;
    }
    const NdbDictionary::Table *otherTable = otherNdb->getDictionary()->getTable("SchemaObjOwnerCheck_tab");
    if(!otherTable)
    {
      ndbout << "Failed to get Ndb table from otherConnection" << endl;
      result = NDBT_FAILED;
      break;
    }
    const NdbDictionary::Index *otherIndex = otherNdb->getDictionary()->getIndex("SchemaObjOwnerCheck_idx", "SchemaObjOwnerCheck_tab");
    if(!otherIndex)
    {
      ndbout << "Failed to get Ndb index from otherConnection" << endl;
      result = NDBT_FAILED;
      break;
    }
  
    ndbout << "Enabling schema object ownership check on ctx connection" << endl;
    trans->setSchemaObjOwnerChecks(true);
  
    ndbout << "Attempting to acquire Ndb*Operations on schema objects ";
    ndbout << "which belong to other connection" << endl;
    NdbOperation *op = trans->getNdbOperation(otherTable);
    const NdbError err1 = trans->getNdbError();
    if(err1.code != 1231)
    {
      ndbout << "Failed to detect Table with wrong owner for NdbOperation" << endl;
      result = NDBT_FAILED;
      break;
    } 
    NdbScanOperation *scanop = trans->getNdbScanOperation(otherTable);
    const NdbError err2 = trans->getNdbError();
    if(err2.code != 1231)
    {
      ndbout << "Failed to detect Table with wrong owner for NdbScanOperation" << endl;
      result = NDBT_FAILED;
      break;
    } 
    NdbIndexScanOperation *idxscanop = trans->getNdbIndexScanOperation(otherIndex, otherTable);
    const NdbError err3 = trans->getNdbError();
    if(err3.code != 1231)
    {
      ndbout << "Failed to detect Table/Index with wrong owner for NdbIndexScanOperation" << endl;
      result = NDBT_FAILED;
      break;
    } 
    NdbIndexOperation *idxop = trans->getNdbIndexOperation(otherIndex);
    const NdbError err4 = trans->getNdbError();
    if(err4.code != 1231)
    {
      ndbout << "Failed to detect Index with wrong owner for NdbIndexOperation" << endl;
      result = NDBT_FAILED;
      break;
    } 
    ndbout << "Success: ownership check detected wrong owner" << endl;   
 
    ndbout << "Disabling schema object ownership check on valid connection" << endl;
    trans->setSchemaObjOwnerChecks(false);
  
    ndbout << "Attempting to acquire Ndb*Operations ";
    ndbout << "on valid schema objects from other connection" << endl;
    op = trans->getNdbOperation(otherTable);
    scanop = trans->getNdbScanOperation(otherTable);
    idxscanop = trans->getNdbIndexScanOperation(otherIndex, otherTable);
    idxop = trans->getNdbIndexOperation(otherIndex);
    
    if(!op || !scanop || !idxscanop || !idxop)  // failure to acquire at least one op
    {
      ndbout << "Failed to acquire ";
      if(!op)        ndbout << "NdbOperation, ";
      if(!scanop)    ndbout << "NdbScanOperation, ";
      if(!idxscanop) ndbout << "NdbIndexScanOperation, ";
      if(!idxop)     ndbout << "NdbIndexOperation, ";
      ndbout << "error: " << trans->getNdbError().message << endl;
      result = NDBT_FAILED;
      break;
    }
    ndbout << "Success: ownership check skipped, wrong owner not detected" << endl;   

    ndbout << "Enabling schema object ownership check on valid connection" << endl;
    trans->setSchemaObjOwnerChecks(true);
 
    ndbout << "Acquiring schema objects from current connection" << endl;
    const NdbDictionary::Table *table = ndb->getDictionary()->getTable("SchemaObjOwnerCheck_tab");
    if(!table)
    {
      ndbout << "Failed to get Ndb table from connection" << endl;
      result = NDBT_FAILED;
      break;
    }
    const NdbDictionary::Index *index = ndb->getDictionary()->getIndex("SchemaObjOwnerCheck_idx", "SchemaObjOwnerCheck_tab");
    if(!index)
    {
      ndbout << "Failed to get Ndb index from connection" << endl;
      result = NDBT_FAILED;
      break;
    }
 
    ndbout << "Attempting to acquire Ndb*Operations ";
    ndbout << "on owned schema objects with different db" << endl;
    ndb->setDatabaseName("notexist");
    NdbOperation *op2 = trans->getNdbOperation(table);
    NdbScanOperation *scanop2 = trans->getNdbScanOperation(table);
    NdbIndexScanOperation *idxscanop2 = trans->getNdbIndexScanOperation(index, table);
    NdbIndexOperation *idxop2 = trans->getNdbIndexOperation(index, table);

    if(!op2 || !scanop2 || !idxscanop2 || !idxop2)  // failure to acquire at least one op
    {
      ndbout << "Failed to acquire ";
      if(!op)        ndbout << "NdbOperation, ";
      if(!scanop)    ndbout << "NdbScanOperation, ";
      if(!idxscanop) ndbout << "NdbIndexScanOperation, ";
      if(!idxop)     ndbout << "NdbIndexOperation, ";
      ndbout << "error: " << trans->getNdbError().message << endl;
      result = NDBT_FAILED;
      break;
    }
    ndbout << "Success: acquired Ndb*Operations on owned schema objects" << endl;   
  } while(false);

  ndbout << "Cleanup" << endl; 
  ndb->setDatabaseName("TEST_DB");
  if (dict->dropIndex("SchemaObjOwnerCheck_idx", "SchemaObjOwnerCheck_tab") == -1) 
  {
    g_err << "Failed to drop SchemaObjOwnerCheck_idx index." << endl;
    result = NDBT_FAILED;
  }
  if (dict->dropTable("SchemaObjOwnerCheck_tab") == -1) 
  {
    g_err << "Failed to drop SchemaObjOwnerCheck_tab table." << endl;
    result = NDBT_FAILED;
  }

  trans->setSchemaObjOwnerChecks(false);
  ndb->closeTransaction(trans);

  if(otherNdb)
  {
    delete otherNdb;
    otherNdb = NULL;
  }
  if(otherConnection)
  {
    delete otherConnection;
    otherConnection = NULL;
  }
  return result;
}

int 
testMgmdSendBufferExhaust(NDBT_Context* ctx, NDBT_Step* step)
{
  /* 1 : Get MGMD node id
   * 2 : Get a data node node id
   * 3 : Consume most SB in MGMD
   * 4 : Block sending from MGMD -> data node
   * 5 : Observe whether MGMD is alive + well
   * 6 : Unblock sending
   * 7 : Release SB
   * 8 : Completed
   */
  NdbRestarter restarter;
  int result = NDBT_OK;
  
  int dataNodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
  int mgmdNodeId = ndb_mgm_get_mgmd_nodeid(restarter.handle);

  ndbout << "MGMD node id : " << mgmdNodeId << endl;
  ndbout << "Data node id : " << dataNodeId << endl;
  
  ndbout << "Reducing MGMD SB memory + blocking send to data node" << endl;
  const int leftSbBytes = 96 * 1024;
  const int dumpCodeConsumeSb [] = {9996, leftSbBytes};
  const int dumpCodeBlockSend [] = {9994, dataNodeId};
  CHECK(restarter.dumpStateOneNode(mgmdNodeId, dumpCodeConsumeSb, 2) == 0);
  CHECK(restarter.dumpStateOneNode(mgmdNodeId, dumpCodeBlockSend, 2) == 0);

  ndbout << "Checking ability of MGMD to respond to requests" << endl;
  
  Uint32 count = 30;

  while (count--)
  {
    ndbout << "  - Getting node status " << count;
    ndb_mgm_cluster_state* state = ndb_mgm_get_status(restarter.handle);
    if (state == NULL)
    {
      ndbout << "ndb_mgm_get_status failed"
	     << ", error: " << ndb_mgm_get_latest_error(restarter.handle)
             << " - " <<  ndb_mgm_get_latest_error_msg(restarter.handle)
	     << endl;
      result = NDBT_FAILED;
      break;
    }
    
    ndbout << " - ok." << endl;
    free(state);
    NdbSleep_MilliSleep(1000);
  }

  ndbout << "Cleaning up" << endl;
  const int dumpCodeUnblockSend [] = {9995, dataNodeId};
  const int dumpCodeReleaseSb [] = {9997};
  CHECK(restarter.dumpStateOneNode(mgmdNodeId, dumpCodeUnblockSend, 2) == 0);
  CHECK(restarter.dumpStateOneNode(mgmdNodeId, dumpCodeReleaseSb, 1) == 0);
  CHECK(ndb_mgm_get_latest_error(restarter.handle) == 0);

  return result;
}

/*
 * Create Unique Index in the given table using ndbapi
 * Returns
 *   NDBT_OK     if index creation was successful
 *   NDBT_FAILED if the index creation failed
 * */
int
createUniqueIndex(NdbDictionary::Dictionary* pDict,
                  const char* tableName,
                  const char* indexName,
                  const char* columnName)
{
  /* create a new index on the table */
  NdbDictionary::Index tmpIndex;
  tmpIndex.setName(indexName);
  tmpIndex.setTable(tableName);
  tmpIndex.setType(NdbDictionary::Index::UniqueHashIndex);
  tmpIndex.setLogging(false);
  tmpIndex.addIndexColumn(columnName);

  /* create an index on the table */
  ndbout << "Creating index " << indexName
         << " on " << tableName << endl;
  CHECKN(pDict->createIndex(tmpIndex) == 0, pDict, NDBT_FAILED);
  return NDBT_OK;
}

/**
 * Runs a transaction using the passed index data.
 * Returns the errorCode on failure. 0 on success.
 */
int
runTransactionUsingNdbIndexOperation(Ndb* pNdb,
                                     Vector<const NdbDictionary::Index*> pIndexes,
                                     const NdbDictionary::Table *tab)
{
  /*
   * 1. Start a transaction and fetch NdbIndexOperations using the
   *    sent indexes.
   * 2. Execute the transaction.
   * 3. Return the error-code or 0
   */
  /* start a transaction */
  NdbTransaction *pTransaction= pNdb->startTransaction();
  CHECKN(pTransaction != NULL, pNdb, pNdb->getNdbError().code);

  for(uint i = 0; i < pIndexes.size(); i++){
    /* use the obsolete index to fetch a NdbIndexOperation */
    NdbIndexOperation *pIndexOperation=
        pTransaction->getNdbIndexOperation(pIndexes[i], tab);
    CHECKN(pIndexOperation != NULL, pTransaction,
           pTransaction->getNdbError().code);

    /* add where field */
    pIndexOperation->readTuple(NdbOperation::LM_Read);
    pIndexOperation->equal(pIndexes[i]->getColumn(0)->getName(), 10);

    /* add select field */
    NdbRecAttr *pRecAttr= pIndexOperation->getValue(1, NULL);
    CHECKN(pRecAttr != NULL, pTransaction, pTransaction->getNdbError().code);
  }

  /* execute the transaction */
  ndbout << "Executing the transaction." << endl;
  if(pTransaction->execute( NdbTransaction::Commit,
                            NdbOperation::AbortOnError) == -1)
  {
    /* Transaction failed. */
    NdbError ndbError = pTransaction->getNdbError();
    /* Ignore - Tuple did not exist errors */
    if(ndbError.code != 626)
    {
      pNdb->closeTransaction(pTransaction);
      NDB_ERR(ndbError);
      return ndbError.code;
    }
  }
  pNdb->closeTransaction(pTransaction);
  ndbout << "Transaction ran successfully." << endl;
  return NDBT_OK;
}

int
runGetNdbIndexOperationTest(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * 1. Obtain the index using getIndex()
   * 2. Drop that index from that table.
   * 3. Execute transaction using that index
   * 5. Verify that the transaction returns error code 284.
   * 6. Create another index - this will take the same index id as the
   *    one previously dropped.
   * 7. Repeat 3 the previously dropped index object.
   * 8. Verify that the transaction returns error code 241.
   */
  Ndb* pNdb = GETNDB(step);
  const char* tableName = "I3";
  const char* indexName = "I3$NDBT_IDX0";
  const NdbDictionary::Table *tab = ctx->getTab();
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  /* load the index */
  const NdbDictionary::Index *pIndex;
  Vector<const NdbDictionary::Index*> pIndexes;
  CHECKN((pIndex = pDict->getIndex(indexName,tableName)) != NULL,
         pDict, NDBT_FAILED);
  pIndexes.push_back(pIndex);
  /* drop the index from the table */
  ndbout << "Dropping index " << indexName << " from " << tableName << endl;
  CHECKN(pDict->dropIndexGlobal(*pIndex) == 0, pDict, NDBT_FAILED);
  /*
    perform a transaction using the dropped index
    Expected Error : 284 - Table not defined in transaction coordinator
   */
  if(runTransactionUsingNdbIndexOperation(pNdb, pIndexes, tab) != 284)
  {
    ndberr << "Transaction was supposed to fail with error 284 but didn't."
           << endl;
    return NDBT_FAILED;
  }

  /* create a new index on the table */
  CHECK(createUniqueIndex(pDict, tableName, indexName,
                          pIndex->getColumn(0)->getName()) != NDBT_FAILED);

  /*
    perform a transaction using the dropped index
    Expected Error : 241 - Invalid schema object version
   */
  if(runTransactionUsingNdbIndexOperation(pNdb, pIndexes, tab) != 241)
  {
    ndberr << "Transaction was supposed to fail with error 241 but didn't."
           << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runCreateIndexesOnI3(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Create indexes on table I3 */
  Ndb* pNdb = GETNDB(step);
  const char* tableName = "I3";
  const uint numOfIndexes = 4;
  const char* columnNames[] = {"PORT", "MAC", "HOSTNAME", "GW"};
  ctx->setProperty("numOfIndexes", numOfIndexes);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  /* create the indexes */
  Vector<const NdbDictionary::Index*> pIndexes;
  for(uint i = 0; i < numOfIndexes; i++)
  {
    BaseString name;
    name.assfmt("I3$NDBT_UIDX%d", i);
    CHECK(createUniqueIndex(pDict, tableName, name.c_str(),
                            columnNames[i]) != NDBT_FAILED);
  }
  return NDBT_OK;
}

int
runGetNdbIndexOperationBatchTest(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * 1. In a loop, use all the indexes to perform batch transactions
   *    but, drop an index at every turn at different positions.
   * 2. Verify that the transactions fail with expected error.
   */
  Ndb* pNdb = GETNDB(step);
  const char* tableName = "I3";
  const NdbDictionary::Table *tab = ctx->getTab();
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const uint numOfIndexes = ctx->getProperty("numOfIndexes");

  /* load the indexes */
  Vector<const NdbDictionary::Index*> pIndexes;
  for(uint i = 0; i < numOfIndexes; i++)
  {
    BaseString name;
    const NdbDictionary::Index *pIndex;
    name.assfmt("I3$NDBT_UIDX%d", i);
    CHECKN((pIndex = pDict->getIndex(name.c_str(),tableName)) != NULL,
           pDict, NDBT_FAILED);
    pIndexes.push_back(pIndex);
  }

  /* start batch operations */
  ndbout << "Starting batch transactions." << endl;
  for(uint i=0; i < numOfIndexes; i++)
  {
    /* drop ith index */
    ndbout << "Dropping index " << pIndexes[i]->getName()
           << " from " << tableName << endl;
    CHECKN(pDict->dropIndexGlobal(*pIndexes[i]) == 0, pDict, NDBT_FAILED);

    /* run batch operations in a loop,
     * changing the position of dropped indexes every time */
    for(uint loops = 0; loops < numOfIndexes; loops++)
    {
      /*
      perform a transaction using the dropped index
      Expected Error : 284 - Table not defined in transaction coordinator
       */
      if(runTransactionUsingNdbIndexOperation(pNdb, pIndexes, tab) != 284)
      {
        ndberr << "Transaction was supposed to fail with error 284 but didn't."
               << endl;
        return NDBT_FAILED;
      }

      /* rotate positions of obsolete indexes */
      pIndexes.push_back(pIndexes[0]);
      pIndexes.erase(0);
    }
  }

  return NDBT_OK;
}

int
runGetNdbIndexOperationTransactions(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * 1. In a loop, use all the indexes to perform batch transactions
   * 2. Verify that the transactions fail with one of the expected error.
   */
  Ndb* pNdb = GETNDB(step);
  const char* tableName = "I3";
  const NdbDictionary::Table *tab = ctx->getTab();
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const uint numOfIndexes = ctx->getProperty("numOfIndexes");

  /* start batch operations */
  ndbout << "Starting batch transactions." << endl;
  uint l = 0;
  while(ctx->getProperty("StopTransactions") == 0)
  {
    Vector<const NdbDictionary::Index*> pIndexes;
    if(l++ % 50 == 0)
    {
      /* load the indexes every 50th loop */
      pIndexes.clear();
      for(uint i = 0; i < numOfIndexes; i++)
      {
        BaseString name;
        const NdbDictionary::Index *pIndex;
        name.assfmt("I3$NDBT_UIDX%d", i);
        pIndex = pDict->getIndex(name.c_str(),tableName);
        if(pIndex != NULL)
          pIndexes.push_back(pIndex);
      }
    }

    /*
      perform a transaction
      Expected Errors : 284 - Table not defined in transaction coordinator (or)
                        241 - Invalid schema object version (or)
                   283/1226 - Table is being dropped
    */
    int result = runTransactionUsingNdbIndexOperation(pNdb, pIndexes, tab);
    if(result != NDBT_OK && result != 241 && result != 284 &&
       result != 283 && result != 1226)
    {
      /* Transaction failed with an unexpected error */
      ndberr << "Transaction failed with an unexpected error : " << result << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int
runDropIndexesOnI3(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const char* tableName = "I3";
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const uint numOfIndexes = ctx->getProperty("numOfIndexes");
  uint loops = ctx->getNumLoops();

  while(loops-- > 0)
  {
    for(uint i =0; i < numOfIndexes; i++)
    {
      BaseString name;
      name.assfmt("I3$NDBT_UIDX%d", i);
      /* drop the index. */
      ndbout << "Dropping index " << name.c_str()
                 << " from " << tableName << endl;
      CHECKN(pDict->dropIndex(name.c_str(), tableName) == 0,
             pDict, NDBT_FAILED);

      /* sleep for a random ms */
      const int max_sleep = 100;
      NdbSleep_MilliSleep(rand() % max_sleep);
    }

    /* recreate the indexes and start again */
    runCreateIndexesOnI3(ctx, step);
  }
  ctx->setProperty("StopTransactions", 1);

  return NDBT_OK;
}

static void unusedCallback(int, NdbTransaction*, void*)
{}

/**
 * Test that Ndb::closeTransaction() and/or Ndb-d'tor is
 * able to do propper cleanup of NdbTransactions which
 * are in some 'incomplete' states:
 *  - Transactions being closed before executed.
 *  - Transactions being closed without, or only partially
 *    defined operations.
 *  - Transactions being closed with prepared async operations
 *    not yet executed.
 *  - Ndb instance destructed with NdbTransactions still open
 *    or in 'incomplete' states as described above.
 *
 * Pass verification is no unexpected errors being returned,
 * no asserts hit (Normally found in Ndb::free_list's), and
 * no datanode crashed. (All of these used to be a problem!)
 */
int runTestNoExecute(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table* pTab = ctx->getTab();

  {
    Ndb ndb(&ctx->m_cluster_connection, "TEST_DB");
  }
  {
    Ndb ndb(&ctx->m_cluster_connection, "TEST_DB");
    if (ndb.init()){
      NDB_ERR(ndb.getNdbError());
      return NDBT_FAILED;
    }
  }

  Ndb* pNdb = NULL;
  NdbConnection* pCon = NULL;
  for (int i = 0; i < 1000; i++)
  {
    if (pNdb == NULL)
    {
      pNdb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
      if (pNdb == NULL){
        ndbout << "pNdb == NULL" << endl;      
        return NDBT_FAILED;  
      }
      if (pNdb->init()){
        NDB_ERR(pNdb->getNdbError());
        delete pNdb;
        return NDBT_FAILED;
      }
    }
    pCon = pNdb->startTransaction();
    if (pCon == NULL){
      NDB_ERR(pNdb->getNdbError());
      delete pNdb;
      return NDBT_FAILED;
    }

    const int testcase = ((i >> 2) % 10);
    switch (testcase)
    {
      case 0:   //Do nothing
        break;
 
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      {
        NdbOperation *pOp = pCon->getNdbOperation(pTab->getName());
        if (pOp == NULL){
          NDB_ERR(pCon->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 1)
          break;

        if (pOp->readTuple() != 0){
          NDB_ERR(pOp->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 2)
          break;

        if (pOp->getLockHandle() == NULL){
          NDB_ERR(pOp->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 3)
          break;

        pCon->executeAsynchPrepare(NdbTransaction::Commit, &unusedCallback, NULL);
        if (testcase == 4)
          break;

        pNdb->sendPollNdb(0, 0);
        break;
      }

      case 6:
      case 7:
      case 8:
      case 9:
      {
        NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
        if (pOp == NULL){
          NDB_ERR(pCon->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 6)
          break;

        if (pOp->readTuples() != 0){
          NDB_ERR(pOp->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 7)
          break;

        if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL){
          NDB_ERR(pOp->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        if (testcase == 8)
          break;

        if (pCon->execute(Commit) != 0){
          NDB_ERR(pCon->getNdbError());
          delete pNdb;
          return NDBT_FAILED;
        }
        break;
      }
    }

    if ((i >> 0) & 0x01)
    {
      pNdb->closeTransaction(pCon);
      pCon = NULL;
    }
    if ((i >> 1) & 0x01)
    {
      delete pNdb;
      pNdb = NULL;
      pCon = NULL;
    }
  }
  delete pNdb;

  return result;
}


int
runCheckTransId(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* stepNdb = GETNDB(step);
  Ndb_cluster_connection* ncc = &stepNdb->get_ndb_cluster_connection();

  /**
   * Coverage of problem in bug#23709232
   *
   * Shared 'max transid' concept assumes that when a block
   * reference is reused, the old Ndb's 'max transid' is passed 
   * to the new Ndb.
   * However this had a bug, exposed by interleaving of
   * Ndb(), Ndb->init(), and ~Ndb(), which might be expected
   * to occur in any multithreaded environment.
   */
  
  Ndb* ndb1 = new Ndb(ncc); // Init transid from connection

  ndb1->init(); // Determine block-ref

  NdbTransaction* trans1 = ndb1->startTransaction();
  Uint64 transId1 = trans1->getTransactionId();
  trans1->close();

  ndbout << "Transid1 : " << transId1 << endl;

  Ndb* ndb2 = new Ndb(ncc); // Init transid from connection

  delete ndb1;  // Free block-ref

  ndb2->init(); // Determine block-ref

  NdbTransaction* trans2 = ndb2->startTransaction();
  Uint64 transId2 = trans2->getTransactionId();
  trans2->close();

  ndbout << "Transid2 : " << transId2 << endl;
  
  delete ndb2;
  
  if (transId1 == transId2)
  {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

/* CheckTransIdMt
 * Can control threading + iterations here
 */
const int CheckTransIdSteps = 8;
const Uint32 CheckTransIdIterations = 10000;
const Uint32 CheckTransIdEntries = CheckTransIdSteps * CheckTransIdIterations;

static Uint64* g_checkTransIdArrays;

int
runInitCheckTransIdMt(NDBT_Context* ctx, NDBT_Step* step)
{
  g_checkTransIdArrays = new Uint64[CheckTransIdEntries];

  ndbout << "Running" << endl;

  return NDBT_OK;
}

int
runCheckTransIdMt(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* stepNdb = GETNDB(step);
  Ndb_cluster_connection* ncc = &stepNdb->get_ndb_cluster_connection();
  
  Uint32 stepIdx = step->getStepNo() - 1;
  Uint64* myIds = g_checkTransIdArrays + (stepIdx * CheckTransIdIterations);
  
  for (Uint32 i=0; i<CheckTransIdIterations; i++)
  {
    /* New Ndb, create a transaction, get id, close it, delete Ndb */
    Ndb newNdb(ncc);
    newNdb.init();
    
    NdbTransaction* newTrans = newNdb.startTransaction();
    myIds[i] = newTrans->getTransactionId();
    newTrans->close();
  }

  return NDBT_OK;
}

int cmpUint64(const void* a, const void* b)
{
  Uint64 va = *((const Uint64*)a);
  Uint64 vb = *((const Uint64*)b);
  
  return ((va > vb)? 1 :
          (vb > va)? -1 :
          0);
} 

int
runVerifyCheckTransIdMt(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Look for duplicates */
  ndbout << "Checking" << endl;
  
  /* First sort */
  qsort(g_checkTransIdArrays, CheckTransIdEntries, sizeof(Uint64), cmpUint64);
  
  int result = NDBT_OK;
  Uint32 contigCount = 0;
  Uint32 errorCount = 0;
  Uint32 maxContigError = 0;
  Uint32 contigErrorCount = 0;

  /* Then check */
  for (Uint32 i=1; i<CheckTransIdEntries; i++)
  {
    //ndbout << g_checkTransIdArrays[i-1] << endl;
    if (g_checkTransIdArrays[i] == g_checkTransIdArrays[i-1])
    {
      ndbout << "Error : Duplicate transid found "
             << " (" << g_checkTransIdArrays[i]
             << ")" << endl;
      errorCount ++;
      contigErrorCount++;
      
      result = NDBT_FAILED;
    }
    else
    {
      if (contigErrorCount > 0)
      {
        if (contigErrorCount > maxContigError)
        {
          maxContigError = contigErrorCount;
        }
        contigErrorCount = 0;
      }
      if (g_checkTransIdArrays[i] == g_checkTransIdArrays[i-1] + 1)
      {
        contigCount++;
      }
    }
  }

  ndbout << CheckTransIdEntries << " transaction ids of which "
         << contigCount << " are contiguous, giving "
         << CheckTransIdEntries - contigCount << " gaps." << endl;

  ndbout << errorCount << " duplicates found, with max of "
         << maxContigError + 1 << " uses of the same transaction id" << endl;

  return result;
}

int
runFinaliseCheckTransIdMt(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Free the storage */
  delete g_checkTransIdArrays;

  return NDBT_OK;
}

int
runTestColumnNameLookupPerf(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table *tab = ctx->getTab();
  
  ndbout_c("Table lookups on columns in table %s",
           tab->getName());

  const char* colNames[512];
  for (int c=0; c<tab->getNoOfColumns(); c++)
  {
    colNames[c] = tab->getColumn(c)->getName();
    ndbout_c("  %d %s",
             c, colNames[c]);
  }

  const Uint32 iterations=10000000;
  for (int c=0; c < tab->getNoOfColumns(); c++)
  {
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    const char* name = colNames[c];
    for (Uint32 i=0; i < iterations; i++)
    {
      const NdbDictionary::Column* col = tab->getColumn(colNames[c]);
      (void) col;
    };
    const Uint64 time = NdbTick_Elapsed(start, NdbTick_getCurrentTicks()).milliSec();
    ndbout_c("Col %u %s : %u iterations in %llu millis",
             c, name, iterations, time);
  }

  return NDBT_OK;
}

int runMaybeRestartMaster(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * Psuedo-randomly restart the current master node
   * Often in test runs the Master node is the lowest
   * numbered node id due to nodes being iterated.
   *
   * Randomly restarting the Master prior to running a
   * test is one way to avoid tests with do [not] restart
   * the master for always [never] restarting the
   * lowest node id
   */
  NdbRestarter restarter;
  int masterNodeId = restarter.getMasterNodeId();
  const bool restartMaster = ((rand() % 2) == 0);

  if (restartMaster)
  {
    ndbout << "Restarting Master node "
           << masterNodeId
           << endl;

    if (restarter.restartOneDbNode(masterNodeId,
                                   false,        // Initial
                                   true) != 0)   // NOSTART
    {
      g_err << "Failed to restart node" << endl;
      return NDBT_FAILED;
    }

    if (restarter.waitNodesNoStart(&masterNodeId, 1) != 0)
    {
      g_err << "Failed to wait for NoStart" << endl;
      return NDBT_FAILED;
    }

    if (restarter.startNodes(&masterNodeId, 1) != 0)
    {
      g_err << "Failed to start node" << endl;
      return NDBT_FAILED;
    }

    if (restarter.waitClusterStarted() != 0)
    {
      g_err << "Failed waiting for node to start" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Master node restarted" << endl;
  }
  else
  {
    ndbout << "Not restarting Master node "
           << masterNodeId
           << endl;
  }
  return NDBT_OK;
}

void
asyncCallback(int res, NdbTransaction* trans, void* obj)
{
  
}

int
runTestOldApiScanFinalise(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table *tab = ctx->getTab();

  /**
   * Test behaviour of 'old api' scan prepare + send 
   * without subsequent execAsynchPrepare()
   * Note that use of async API with scans is not 
   * currently documented, but it is possible.
   */
  {
    NdbTransaction * trans = pNdb->startTransaction();
    CHECK(trans != NULL);
    
    /**
     *  Prepare transaction, so that it is considered for
     * sending
     */
    trans->executeAsynchPrepare(NdbTransaction::NoCommit,
                                asyncCallback,
                                NULL);

    /**
     * Now define a scan, which is not prepared
     */

    NdbScanOperation* scanOp = trans->getNdbScanOperation(tab);
    CHECK(scanOp != NULL);

    CHECK(scanOp->readTuples(NdbScanOperation::LM_CommittedRead,
                             0,
                             16) == 0);
    
    for(int a = 0; a<tab->getNoOfColumns(); a++)
    {
      CHECK(scanOp->getValue(tab->getColumn(a)) != 0);
    }

    /**
     * Now call send and check behaviour
     * Expect : 
     *   send will finalise + send the scan
     *   scan will proceed as expected (no rows in resultset)
     */

    CHECK(pNdb->sendPollNdb() != 0);

    ndbout_c("Trans error : %u %s\n"
             "Scan error : %u %s\n",
             trans->getNdbError().code,
             trans->getNdbError().message,
             scanOp->getNdbError().code,
             scanOp->getNdbError().message);

    /* Specific error for this case now */
    CHECK(trans->getNdbError().code == 4342);
    CHECK(scanOp->getNdbError().code == 4342);

    /**
     * Now attempt nextResult
     */
    int nextRes = scanOp->nextResult();
    
    ndbout_c("Next result : %d\n"
             "ScanError : %u %s",
             nextRes,
             scanOp->getNdbError().code,
             scanOp->getNdbError().message);
    CHECK(nextRes == -1);
    CHECK(scanOp->getNdbError().code == 4342); /* Scan defined but not prepared */

    trans->close();
  }

/* Test requires DBUG error injection */
#ifndef DBUG_OFF
  /**
   * Test behaviour of 'old api' scan finalisation
   * failure
   */
  {
    NdbTransaction * trans = pNdb->startTransaction();
    CHECK(trans != NULL);

    NdbScanOperation* scanOp = trans->getNdbScanOperation(tab);
    CHECK(scanOp != NULL);

    CHECK(scanOp->readTuples(NdbScanOperation::LM_CommittedRead,
                             0,
                             16) == 0);
    
    for(int a = 0; a<tab->getNoOfColumns(); a++)
    {
      CHECK(scanOp->getValue(tab->getColumn(a)) != 0);
    }

    /* Force failure in finalisation via error-insert */
    DBUG_SET_INITIAL("+d,ndb_scanbuff_oom");

    int execRes = trans->execute(NdbTransaction::NoCommit,
                                 NdbOperation::AbortOnError);

    DBUG_SET_INITIAL("-d,ndb_scanbuff_oom");

    NdbError transError = trans->getNdbError();
    NdbError scanError1 = scanOp->getNdbError();

    int nextRes = scanOp->nextResult();
    
    NdbError scanError2 = scanOp->getNdbError();

    ndbout_c("execRes : %d\n"
             "transError : %u %s\n"
             "scanError : %u %s\n"
             "nextRes + scanError : %d %u %s",
             execRes, 
             transError.code, transError.message,
             scanError1.code, scanError1.message,
             nextRes,
             scanError2.code, scanError2.message);
    
    CHECK(execRes == 0);
    CHECK(transError.code == 4000);
    CHECK(scanError1.code == 4000);
    CHECK(nextRes == -1);
    CHECK(scanError2.code == 4000);

    trans->close();
  }
#endif

  return NDBT_OK;
}




static int reCreateTableHook(Ndb* ndb,
                             NdbDictionary::Table & table,
                             int when,
                             void* arg)
{
  if (when == 0)
  {
    NDBT_Context* ctx = (NDBT_Context*) arg;
    
    bool readBackup = (ctx->getProperty("CreateRB", Uint32(0)) != 0);
    bool fullyReplicated = (ctx->getProperty("CreateFR", Uint32(0)) != 0);

    /* Add others as necessary... */

    if (readBackup)
    {
      ndbout << "rCTH : Setting ReadBackup property" << endl;
    }
    table.setReadBackupFlag(readBackup);

    if (fullyReplicated)
    {
      ndbout << "rCTH : Setting Fully Replicated property" << endl;
    }
    table.setFullyReplicated(fullyReplicated);
  }

  return 0;

}

int
runReCreateTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);

  /* Drop table by name if it exists */
  NdbDictionary::Table tab = * ctx->getTab();
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();
  
  BaseString tabName(tab.getName());

  ndbout << "Dropping table " << tabName << endl;
  
  pDict->dropTable(tabName.c_str());
  
  ndbout << "Recreating table " << tabName << endl;
                   
  /* Now re-create, perhaps with different options */
  if (NDBT_Tables::createTable(pNdb,
                               tabName.c_str(),
                               false,
                               false,
                               reCreateTableHook,
                               ctx) != 0)
  {
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* newTab = pDict->getTable(tabName.c_str());

  if (newTab == NULL)
  {
    return NDBT_FAILED;
  }
  
  ctx->setTab(newTab);

  return NDBT_OK;
}

int runDropTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Table tab = * ctx->getTab();
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();

  ndbout << "Dropping table " << tab.getName() << endl;
  
  pDict->dropTable(tab.getName());

  return NDBT_OK;
}

int runCheckLateDisconnect(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* tab = ctx->getTab();
  HugoTransactions hugoTrans(*tab);
  NdbRestarter restarter;
  //Ndb* pNdb = GETNDB(step);
  
  Ndb otherNdb(otherConnection, "TEST_DB");
  otherNdb.init();
  int rc = otherNdb.waitUntilReady(10);
  
  if (rc != 0)
  {
    ndbout << "Ndb was not ready" << endl;
    
    return NDBT_FAILED;
  }

  ndbout << "Loading data" << endl;
  /* Put some data into the table */
  if (hugoTrans.loadTable(&otherNdb,
                          1024) != NDBT_OK)
  {
    ndbout << "Data load failed " << endl;
    return NDBT_FAILED;
  }
  
  const Uint32 code = ctx->getProperty("ErrorCode", Uint32(0));

  ndbout << "Setting error insert : " << code << endl;
    
  /* TC error insert causing API disconnection 
   * at some point
   */
  
  if (restarter.insertErrorInAllNodes(code) != 0)
  {
    ndbout << "Failed to insert error" << endl;
  }
  
  ndbout << "Updating data, expect disconnection" << endl;
  /* Perform a bulk update */
  /* We expect to be disconnected at the end of this... */
  rc = hugoTrans.pkUpdateRecords(&otherNdb,
                                 1024);
  
  
  restarter.insertErrorInAllNodes(0);
  
  /* We rely on the test framework to detect a problem
   * if the data nodes failed here
   */
  
  return NDBT_OK;
}

int
runCheckWriteTransaction(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();
  
  HugoOperations hugoOps(*pTab);
  Ndb* pNdb = GETNDB(step);
  
  CHECKE((hugoOps.startTransaction(pNdb) == NDBT_OK),
         hugoOps);
  
  CHECKE((hugoOps.pkWriteRecord(pNdb,
                                0) == NDBT_OK),
         hugoOps);
  CHECKE((hugoOps.execute_Commit(pNdb) == NDBT_OK),
         hugoOps);
  CHECKE((hugoOps.closeTransaction(pNdb) == NDBT_OK),
         hugoOps);  
  
  return NDBT_OK;
}


int runCheckSlowCommit(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  /* Want to test the 'slow' commit protocol behaves
   * correctly for various table types
   */
  for (int table_type = 0; table_type < 3; table_type++)
  {
    switch (table_type)
    {
    case 0:
    {
      ndbout << "Normal table" << endl;
      ctx->setProperty("CreateRB", Uint32(0));
      ctx->setProperty("CreateFR", Uint32(0));
      break;
    }
    case 1:
    {
      ndbout << "ReadBackup table" << endl;
      ctx->setProperty("CreateRB", Uint32(1));
      ctx->setProperty("CreateFR", Uint32(0));
      break;
    }
    case 2:
    {
      ndbout << "FullyReplicated" << endl;
      /* Need RB set, as can create !RB FR table... */
      ctx->setProperty("CreateRB", Uint32(1));
      ctx->setProperty("CreateFR", Uint32(1));
      break;
    }
    }
    
    if (runReCreateTable(ctx, step) != NDBT_OK)
    {
      return NDBT_FAILED;
    }
    
    for (int test_type=0; test_type < 3; test_type++)
    {
      Uint32 errorCode = 0;
      switch (test_type)
      {
      case 0:
        /* As normal */
        break;
      case 1:
        /* Timeout during commit phase */
        errorCode = 8113; 
        break;
      case 2:
        /* Timeout during complete phase */
        errorCode = 8114;
        break;
      }
      ndbout << "Inserting error " << errorCode 
             << " in all nodes." << endl;
      
      restarter.insertErrorInAllNodes(errorCode);
        
      int ret = runCheckWriteTransaction(ctx,step);
      
      restarter.insertErrorInAllNodes(0);
      if (ret != NDBT_OK)
      {
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}


NDBT_TESTSUITE(testNdbApi);
TESTCASE("MaxNdb", 
	 "Create Ndb objects until no more can be created\n"){ 
  INITIALIZER(runTestMaxNdb);
}
TESTCASE("MaxTransactions", 
	 "Start transactions until no more can be created\n"){ 
  INITIALIZER(runTestMaxTransaction);
}
TESTCASE("MaxOperations", 
	"Get operations until no more can be created\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestMaxOperations);
  FINALIZER(runClearTable);
}
TESTCASE("MaxGetValue", 
	"Call getValue loads of time\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestGetValue);
  FINALIZER(runClearTable);
}
TESTCASE("MaxEqual", 
	"Call equal loads of time\n"){ 
  INITIALIZER(runTestEqual);
}
TESTCASE("DeleteNdb", 
	"Make sure that a deleted Ndb object is properly deleted\n"
	"and removed from transporter\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestDeleteNdb);
  FINALIZER(runClearTable);
}
TESTCASE("WaitUntilReady", 
	"Make sure you get an error message when calling waitUntilReady\n"
	"without an init'ed Ndb\n"){ 
  INITIALIZER(runTestWaitUntilReady);
}
TESTCASE("GetOperationNoTab", 
	"Call getNdbOperation on a table that does not exist\n"){ 
  INITIALIZER(runGetNdbOperationNoTab);
}
TESTCASE("BadColNameHandling",
         "Call methods with an invalid column name and check error handling\n"){
  INITIALIZER(runBadColNameHandling);
}
TESTCASE("MissingOperation", 
	"Missing operation request(insertTuple) should give an error code\n"){ 
  INITIALIZER(runMissingOperation);
}
TESTCASE("GetValueInUpdate", 
	"Test that it's not possible to perform getValue in an update\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runGetValueInUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutKeys", 
	"Test that it's not possible to perform update without setting\n"
	 "PKs"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutKeys);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutValues", 
	"Test that it's not possible to perform update without setValues\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutValues);
  FINALIZER(runClearTable);
}
TESTCASE("NdbErrorOperation", 
	 "Test that NdbErrorOperation is properly set"){
  INITIALIZER(runCheckGetNdbErrorOperation);
}
TESTCASE("ReadWithoutGetValue", 
	 "Test that it's possible to perform read wo/ getvalue's\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runReadWithoutGetValue);
  FINALIZER(runClearTable);
}
TESTCASE("Bug_11133", 
	 "Test ReadEx-Delete-Write\n"){ 
  INITIALIZER(runBug_11133);
  FINALIZER(runClearTable);
}
TESTCASE("Bug_WritePartialIgnoreError", 
	 "Test WritePartialIgnoreError\n"){ 
  INITIALIZER(runBug_WritePartialIgnoreError);
  FINALIZER(runClearTable);
}
TESTCASE("Scan_4006", 
	 "Check that getNdbScanOperation does not get 4006\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runScan_4006);
  FINALIZER(runClearTable);
}
TESTCASE("IgnoreError", ""){
  INITIALIZER(createPkIndex);
  STEP(runTestIgnoreError);
  FINALIZER(runClearTable);
  FINALIZER(createPkIndex_Drop);
}
TESTCASE("CheckNdbObjectList", 
	 ""){ 
  INITIALIZER(runCheckNdbObjectList);
}
TESTCASE("DeleteClusterConnectionWhileUsed",
         "Make sure that deleting of Ndb_cluster_connection will"
         "not return until all it's Ndb objects has been deleted."){
  STEP(runNdbClusterConnectionDelete_connection_owner)
  STEP(runNdbClusterConnectionDelete_connection_user);
}
TESTCASE("ExecuteAsynch", 
	 "Check that executeAsync() works (BUG#27495)\n"){ 
  INITIALIZER(runTestExecuteAsynch);
}
TESTCASE("Bug28443", 
	 ""){ 
  INITIALIZER(runBug28443);
}
TESTCASE("Bug37158", 
	 ""){ 
  INITIALIZER(runBug37158);
}
TESTCASE("SimpleReadAbortOnError",
         "Test behaviour of Simple reads with Abort On Error"){
  INITIALIZER(simpleReadAbortOnError);
}
TESTCASE("NdbRecordPKAmbiguity",
         "Test behaviour of NdbRecord insert with ambig. pk values"){
  INITIALIZER(testNdbRecordPkAmbiguity);
}
TESTCASE("NdbRecordPKUpdate",
         "Verify that primary key columns can be updated"){
  INITIALIZER(testNdbRecordPKUpdate);
}
TESTCASE("NdbRecordCICharPKUpdate",
         "Verify that a case-insensitive char pk column can be updated"){
  INITIALIZER(testNdbRecordCICharPKUpdate);
}
TESTCASE("NdbRecordRowLength",
         "Verify that the record row length calculation is correct") {
  INITIALIZER(testNdbRecordRowLength);
}
TESTCASE("Bug44015",
         "Rollback insert followed by delete to get corruption") {
  STEP(runBug44015);
  STEPS(runScanReadUntilStopped, 10);
}
TESTCASE("Bug44065_org",
         "Rollback no-change update on top of existing data") {
  INITIALIZER(runBug44065_org);
}
TESTCASE("Bug44065",
         "Rollback no-change update on top of existing data") {
  INITIALIZER(runBug44065);
}
TESTCASE("ApiFailReqBehaviour",
         "Check ApiFailReq cleanly marks Api disconnect") {
  // Some flags to enable the various threads to cooperate
  TC_PROPERTY(ApiFailTestRun, (Uint32)0);
  TC_PROPERTY(ApiFailTestComplete, (Uint32)0);
  TC_PROPERTY(ApiFailTestsRunning, (Uint32)0);
  TC_PROPERTY(ApiFailNumberPkSteps, (Uint32)5); // Num threads below
  INITIALIZER(runLoadTable);
  // 5 threads to increase probability of pending
  // TCKEYREQ after API_FAILREQ
  STEP(runBulkPkReads);
  STEP(runBulkPkReads);
  STEP(runBulkPkReads);
  STEP(runBulkPkReads);
  STEP(runBulkPkReads);
  STEP(testApiFailReq);
  FINALIZER(runClearTable);
}
TESTCASE("ReadColumnDuplicates",
         "Check NdbApi behaves ok when reading same column multiple times") {
  INITIALIZER(runLoadTable);
  STEP(runReadColumnDuplicates);
  FINALIZER(runClearTable);
}
TESTCASE("Bug51775", "")
{
  INITIALIZER(runBug51775);
}
TESTCASE("FragmentedApiFailure",
         "Test in-assembly fragment cleanup code for API failure") {
  // We reuse some of the infrastructure from ApiFailReqBehaviour here
  TC_PROPERTY(ApiFailTestRun, (Uint32)0);
  TC_PROPERTY(ApiFailTestComplete, (Uint32)0);
  TC_PROPERTY(ApiFailTestsRunning, (Uint32)0);
  TC_PROPERTY(ApiFailNumberPkSteps, (Uint32)5); // Num threads below
  // 5 threads to increase probability of fragmented signal being
  // in-assembly when disconnect occurs
  STEP(runFragmentedScanOtherApi);
  STEP(runFragmentedScanOtherApi);
  STEP(runFragmentedScanOtherApi);
  STEP(runFragmentedScanOtherApi);
  STEP(runFragmentedScanOtherApi);
  STEP(testFragmentedApiFail);
};
TESTCASE("UnlockBasic",
         "Check basic op unlock behaviour") {
  INITIALIZER(runLoadTable);
  STEP(runTestUnlockBasic);
  FINALIZER(runClearTable);
}
TESTCASE("UnlockRepeat",
         "Check repeated lock/unlock behaviour") {
  INITIALIZER(runLoadTable);
  STEP(runTestUnlockRepeat);
  FINALIZER(runClearTable);
}
TESTCASE("UnlockMulti",
         "Check unlock behaviour with multiple operations") {
  INITIALIZER(runLoadTable);
  STEP(runTestUnlockMulti);
  FINALIZER(runClearTable);
}
TESTCASE("UnlockScan",
         "Check unlock behaviour with scan lock-takeover") {
  INITIALIZER(runLoadTable);
  STEP(runTestUnlockScan);
  FINALIZER(runClearTable);
}
TESTCASE("NdbClusterConnect",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
}
TESTCASE("NdbClusterConnectionConnect",
         "Test Ndb_cluster_connection::connect()")
{
  INITIALIZER(runNdbClusterConnectionConnect);
}
TESTCASE("NdbClusterConnectNR",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  TC_PROPERTY("TimeoutAfterFirst", (Uint32)0);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("NdbClusterConnectNR_master",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  TC_PROPERTY("Master", 1);
  TC_PROPERTY("TimeoutAfterFirst", (Uint32)0);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("NdbClusterConnectNR_non_master",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  TC_PROPERTY("Master", 2);
  TC_PROPERTY("TimeoutAfterFirst", (Uint32)0);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("NdbClusterConnectNR_slow",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  TC_PROPERTY("Master", 2);
  TC_PROPERTY("TimeoutAfterFirst", (Uint32)0);
  TC_PROPERTY("SlowNR", 1);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("NdbClusterConnectSR",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  TC_PROPERTY("ClusterRestart", (Uint32)1);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("NdbClusterConnectNR_slow_nostart",
         "Make sure that every Ndb_cluster_connection get a unique nodeid")
{
  // Test ability for APIs to connect while some node in NOT_STARTED state
  // Limit to non-master nodes due to uniqueness failing when master
  // restarted
  // (Bug #27484475 NDB : NODEID ALLOCATION UNIQUENESS NOT GUARANTEED
  //  OVER MASTER NODE FAILURE)
  // Use randomised initial master restart to avoid always testing
  // the same node id restart behaviour
  TC_PROPERTY("Master", 2);
  TC_PROPERTY("TimeoutAfterFirst", (Uint32)0);
  TC_PROPERTY("SlowNoStart", 1);
  INITIALIZER(runMaybeRestartMaster);
  INITIALIZER(runNdbClusterConnectInit);
  STEPS(runNdbClusterConnect, MAX_NODES);
  STEP(runRestarts); // Note after runNdbClusterConnect or else counting wrong
}
TESTCASE("TestFragmentedSend",
         "Test fragmented send behaviour"){
  INITIALIZER(testFragmentedSend);
}
TESTCASE("ReceiveTRANSIDAIAfterRollback",
         "Delay the delivery of TRANSID_AI results from the data node." \
         "Abort a transaction with a timeout so that the "\
         "transaction closing and TRANSID_AI processing are interleaved." \
         "Confirm that this interleaving does not result in a core."
){
  STEP(runReceiveTRANSIDAIAfterRollback);
  FINALIZER(runClearTable);
}
TESTCASE("RecordSpecificationBackwardCompatibility",
         "Test RecordSpecification struct's backward compatibility"){
  STEP(testNdbRecordSpecificationCompatibility);
}
TESTCASE("SchemaObjectOwnerCheck",
         "Test use of schema objects with non-owning connections"){
  STEP(testSchemaObjectOwnerCheck);
}
TESTCASE("MgmdSendbufferExhaust",
         "")
{
  INITIALIZER(testMgmdSendBufferExhaust);
}
TESTCASE("GetNdbIndexOperationTest",
         "Send an obsolete index into getNdbIndexOperation and execute." \
         "Confirm that this doesn't crash the ndbd.")
{
  //To be run only on Table I3
  INITIALIZER(runLoadTable);
  STEP(runGetNdbIndexOperationTest);
  VERIFIER(runCheckAllNodesStarted);
  FINALIZER(runClearTable)
}
TESTCASE("GetNdbIndexOperationBatchTest",
         "Send an obsolete index into getNdbIndexOperation in a batch" \
         "and execute. Confirm that this doesn't crash the ndbd.")
{
  //To be run only on Table I3
  INITIALIZER(runCreateIndexesOnI3);
  INITIALIZER(runLoadTable);
  STEP(runGetNdbIndexOperationBatchTest);
  VERIFIER(runCheckAllNodesStarted);
  FINALIZER(runClearTable)
}
TESTCASE("GetNdbIndexOperationParallelDroppingTest",
         "1. Start transactions batch/normal in a step" \
         "2. Start dropping/creating indexes in a parallel thread " \
         "Confirm that this doesn't crash the ndbd.")
{
  //To be run only on Table I3
  INITIALIZER(runCreateIndexesOnI3);
  INITIALIZER(runLoadTable);
  STEPS(runGetNdbIndexOperationTransactions, 100);
  STEP(runDropIndexesOnI3);
  VERIFIER(runCheckAllNodesStarted);
  FINALIZER(runClearTable)
}
TESTCASE("CloseBeforeExecute", 
	 "Check that objects allocated within a Ndb/NdbTransaction " \
         "is released even if Txn is not executed"){ 
  INITIALIZER(runTestNoExecute);
}
TESTCASE("CheckTransId",
         "Check transid uniqueness across multiple Ndb instances")
{
  INITIALIZER(runCheckTransId);
}
TESTCASE("CheckTransIdMt",
         "Check transid uniqueness across multiple threads")
{
  INITIALIZER(runInitCheckTransIdMt);
  STEPS(runCheckTransIdMt, CheckTransIdSteps);
  VERIFIER(runVerifyCheckTransIdMt);
  FINALIZER(runFinaliseCheckTransIdMt);
}
TESTCASE("OldApiScanFinalise",
         "Test error during finalise behaviour")
{
  VERIFIER(runTestOldApiScanFinalise);
}
TESTCASE("TestColumnNameLookupPerf",
         "")
{
  INITIALIZER(runTestColumnNameLookupPerf);
}
TESTCASE("CheckDisconnectCommit",
         "Check commit post API disconnect")
{
  TC_PROPERTY("CreateRB", Uint32(1)); // ReadBackup
  TC_PROPERTY("ErrorCode", Uint32(8110)); // API disconnect during COMMIT
  INITIALIZER(runReCreateTable);
  INITIALIZER(setupOtherConnection);
  STEP(runCheckLateDisconnect);
  FINALIZER(runDropTable);
  FINALIZER(tearDownOtherConnection);
}
TESTCASE("CheckDisconnectComplete",
         "Check complete post API disconnect")
{
  TC_PROPERTY("CreateRB", Uint32(1)); // ReadBackup
  TC_PROPERTY("ErrorCode", Uint32(8111)); // API disconnect during COMPLETE
  INITIALIZER(runReCreateTable);
  INITIALIZER(setupOtherConnection);
  STEP(runCheckLateDisconnect);
  FINALIZER(runDropTable);
  FINALIZER(tearDownOtherConnection);
}
TESTCASE("CheckSlowCommit",
         "Check slow commit protocol + table types")
{
  STEP(runCheckSlowCommit);
  FINALIZER(runDropTable);
}


NDBT_TESTSUITE_END(testNdbApi);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNdbApi);
  //  TABLE("T1");
  return testNdbApi.execute(argc, argv);
}

template class Vector<Ndb*>;
template class Vector<NdbConnection*>;
template class Vector<Ndb_cluster_connection*>;
