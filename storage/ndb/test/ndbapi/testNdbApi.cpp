/* Copyright (c) 2003-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


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

#define MAX_NDB_OBJECTS 32678

#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: failed on line " << __LINE__ << endl; \
  return -1; } 

#define CHECKE(b) if (!(b)) { \
  errors++; \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

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
	ERR(pNdb->getNdbError());
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
      
    
    for(size_t j = 0;  j < ndbVector.size(); j++){
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
    ERR(pNdb->getNdbError());
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
	ERR(pNdb->getNdbError());
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
      
    
    for(size_t j = 0; j < conVector.size(); j++){
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
    ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  HugoOperations hugoOps(*pTab);

  bool endTest = false;
  while (!endTest && result == NDBT_OK){
    int errors = 0;
    int maxErrors = 5;

    maxOpsLimit = l*1000;    
       
    if (hugoOps.startTransaction(pNdb) != NDBT_OK){
      delete pNdb;
      return NDBT_FAILED;
    }
    
    int i = 0;
    while (errors < maxErrors){
      
      if(hugoOps.pkReadRecord(pNdb,1, 1) != NDBT_OK){
	errors++;
	continue;
      }
	        
      i++;      

      if (i >= maxOpsLimit){
	errors = maxErrors;
      }
	
    }

    ndbout << i << " operations used" << endl;

    int execResult = hugoOps.execute_Commit(pNdb);
    switch(execResult){
    case NDBT_OK:
      break;
    case 233: // Out of operation records in transaction coordinator      
      // OK - end test
      endTest = true;
      break;
    default:
      result = NDBT_FAILED;
      break;
    }
    
    hugoOps.closeTransaction(pNdb);

    l++;

  }

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
    ERR(pNdb->getNdbError());
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
	  ERR(pCon->getNdbError());
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
	ERR(err);
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
	ERR(pCon->getNdbError());
	break;
      default:
	ERR(pCon->getNdbError());
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
    ERR(pNdb->getNdbError());
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
	ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);
	delete pNdb;
	return NDBT_FAILED;
      }
      
      if (pOp->readTuple() != 0){
	ERR(pCon->getNdbError());
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
		ERR(err);
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
		ERR(err);
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
	ERR(pCon->getNdbError());
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
	ERR(pCon->getNdbError());
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
	ERR(pNdb->getNdbError());
	result = NDBT_FAILED;	
	goto end_test;
      }
      if (pNdb->waitUntilReady() != 0){
	ERR(pNdb->getNdbError());
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
    for(size_t j = 0;  j < ndbVector.size(); j++)
      delete ndbVector[j];
    ndbVector.clear();
    l++;
  }
  
  
 end_test:
  
  for(size_t i = 0;  i < ndbVector.size(); i++)
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

  ERR(err);
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
    ERR(pNdb->getNdbError());
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
    ERR(err);
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
    ERR(pNdb->getNdbError());
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
      ERR(pCon->getNdbError());
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
        ERR(pCon->getNdbError());
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
        ERR(pCon->getNdbError());
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
        ERR(pCon->getNdbError());
        pNdb->closeTransaction(pCon);
        delete pNdb;
        return NDBT_FAILED;
      }

      // set equality on pk columns
      for(int a = 0; a<pTab->getNoOfColumns(); a++){
        if (pTab->getColumn(a)->getPrimaryKey() == true){
          if(hugoOps.equalForAttr(pOp, a, 1) != 0){
            const NdbError err = pCon->getNdbError();
            ERR(err);
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
        ERR(pCon->getNdbError());
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
        ERR(pCon->getNdbError());
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
      ERR(opErr);
      ERR(transErr);
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
    ERR(pNdb->getNdbError());
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
    ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  // Forget about calling pOp->insertTuple();
  
  // Call getValue should not work
  if (pOp->getValue(pTab->getColumn(1)->getName()) == NULL) {
    const NdbError err = pCon->getNdbError();
    ERR(err);
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
    ERR(pNdb->getNdbError());
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
    ERR(pCon->getNdbError());
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
    ERR(err);
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
    ERR(pCon->getNdbError());
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
    ERR(pNdb->getNdbError());
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
    ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  if (pOp->updateTuple() != 0){
    pNdb->closeTransaction(pCon);
    ERR(pOp->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  for(int a = 0; a<pTab->getNoOfColumns(); a++){
    if (pTab->getColumn(a)->getPrimaryKey() == true){
      if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	ERR(pCon->getNdbError());
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
    ERR(pCon->getNdbError());
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
    ERR(pNdb->getNdbError());
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
    ERR(pCon->getNdbError());
    pNdb->closeTransaction(pCon);  
    delete pNdb;
    return NDBT_FAILED;
  }
  
  if (pOp->updateTuple() != 0){
    pNdb->closeTransaction(pCon);
    ERR(pOp->getNdbError());
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
    ERR(pCon->getNdbError());
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
	ERR(pCon->getNdbError());
	pNdb->closeTransaction(pCon);  
	return NDBT_FAILED;
      }
  
      if (pOp->readTuple((NdbOperation::LockMode)lm) != 0){
	pNdb->closeTransaction(pCon);
	ERR(pOp->getNdbError());
	return NDBT_FAILED;
      }
    
      for(int a = 0; a<pTab->getNoOfColumns(); a++){
	if (pTab->getColumn(a)->getPrimaryKey() == true){
	  if(hugoOps.equalForAttr(pOp, a, 1) != 0){
	    ERR(pCon->getNdbError());
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
	ERR(pCon->getNdbError());
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
      ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);  
      return NDBT_FAILED;
    }
    
    if ((pOp->readTuples((NdbOperation::LockMode)lm)) != 0){
      pNdb->closeTransaction(pCon);
      ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    
    // Dont' call any getValues
    
    // Execute should work
    int check = pCon->execute(NoCommit);
    if (check == 0){
      ndbout << "execute worked" << endl;
    } else {
      ERR(pCon->getNdbError());
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
    ERR(pNdb->getNdbError());
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
    ERR(pCon->getNdbError());
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
	ERR(err);
	if (err.code == 0)
	  result = NDBT_FAILED;

	NdbOperation* pOp2 = pCon->getNdbErrorOperation();
	if (pOp2 == NULL)
	  result = NDBT_FAILED;
	else {
	  const NdbError err2 = pOp2->getNdbError();
	  ERR(err2);
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
    ERR(pNdb->getNdbError());
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
      ERR(pCon->getNdbError());
      pNdb->closeTransaction(pCon);  
      delete pNdb;
      return NDBT_FAILED;
    }
    
    if (pOp->readTuples() != 0){
      pNdb->closeTransaction(pCon);
      ERR(pOp->getNdbError());
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
    ERR(pCon->getNdbError());
  }
  
  for(i= 0; i<scans.size(); i++)
  {
    NdbScanOperation* pOp= scans[i];
    while((check= pOp->nextResult()) == 0);
    if(check != 1)
    {
      ERR(pOp->getNdbError());
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
    ERR(err);
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
    ERR(pNdb->getDictionary()->getNdbError());
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
    ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  NdbConnection* pCon = pNdb->startTransaction();
  if (pCon == NULL){
    ERR(pNdb->getNdbError());
    delete pNdb;
    return NDBT_FAILED;
  }

  NdbScanOperation* pOp = pCon->getNdbScanOperation(pTab->getName());
  if (pOp == NULL){
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }

  if (pOp->readTuples() != 0){
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }

  if (pOp->getValue(NdbDictionary::Column::FRAGMENT) == 0){
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pCon);
    delete pNdb;
    return NDBT_FAILED;
  }
  int res= 42;
  pCon->executeAsynch(NoCommit, testExecuteAsynchCallback, &res);
  while(pNdb->pollNdb(100000) == 0)
    ;
  if (res != 0){
    ERR(pCon->getNdbError());
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
               err.classification == NdbError::OverloadError));
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
  
  const Uint32 MaxConnectStringSize= 256;
  char connectString[ MaxConnectStringSize ];
  ctx->m_cluster_connection.get_connectstring(connectString, 
                                              MaxConnectStringSize);
  
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
        
        if ((err.code == 4002) || // Send failed
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
    ERR(pNdb->getNdbError());
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
	  ERR(pCon->getNdbError());
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
	ERR(err);
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
      ERR(err);
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
    ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }
  TransGuard g1(pTrans1);

  NdbTransaction * pTrans2 = pNdb->startTransaction();
  if (pTrans2 == NULL)
  {
    pTrans1->close();
    ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }

  TransGuard g2(pTrans2);

  {
    NdbOperation * pOp = pTrans1->getNdbOperation(ctx->getTab()->getName());
    if (pOp == NULL)
    {
      ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    if (pOp->insertTuple() != 0)
    {
      ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    HugoOperations hugoOps(* ctx->getTab());
    hugoOps.setValues(pOp, 0, 0);
  }

  {
    NdbOperation * pOp = pTrans2->getNdbOperation(ctx->getTab()->getName());
    if (pOp == NULL)
    {
      ERR(pOp->getNdbError());
      return NDBT_FAILED;
    }
    
    if (pOp->readTuple() != 0)
    {
      ERR(pOp->getNdbError());
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
NDBT_TESTSUITE_END(testNdbApi);

int main(int argc, const char** argv){
  ndb_init();
  //  TABLE("T1");
  return testNdbApi.execute(argc, argv);
}

template class Vector<Ndb*>;
template class Vector<NdbConnection*>;
