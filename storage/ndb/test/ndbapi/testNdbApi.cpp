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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <random.h>
#include <NdbTick.h>

#define MAX_NDB_OBJECTS 32678

#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

#define CHECKE(b) if (!(b)) { \
  errors++; \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 


int runTestMaxNdb(NDBT_Context* ctx, NDBT_Step* step){
  Uint32 loops = ctx->getNumLoops();
  Uint32 l = 0;
  int oldi = 0;
  int result = NDBT_OK;

  while (l < loops && result == NDBT_OK){
    ndbout_c("loop %d", l + 1);
    int errors = 0;
    int maxErrors = 5;
    
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
      if(restarts.executeRestart("RestartRandomNodeAbort", 120) != 0){
	g_err << "Failed to executeRestart(RestartRandomNode)"<<endl;
	result = NDBT_FAILED;
	goto end_test;
      }
    } else {
      // Restart all nodes
      ndbout << "Restart all nodes " << endl;
      if(restarts.executeRestart("RestartAllNodesAbort", 120) != 0){
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
    for(int a = 0; a<pTab->getNoOfColumns(); a++){
      pOp->getValue(a);
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
  default:
    abort();
  }
  printf("%s ", str);
}

int
runTestIgnoreError(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
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
  for (Uint32 op1 = 0; op1 < 12; op1++)
  {
    for (Uint32 op2 = op1; op2 < 12; op2++)
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
	}
	switch(ao){
	case AbortOnError: printf("aoe  "); break;
	case AO_IgnoreError: printf("ie   "); break;
	}
	printf(": ");
	

	hugoTrans.loadTable(pNdb, 1);
	pTrans = pNdb->startTransaction();
	op_row(pTrans, hugoOps, pTab, op1, 0);
	ret = pTrans->execute(et, ao);
	pTrans->close();
	printf("%d ", ret);
	hugoTrans.clearTable(pNdb);

	hugoTrans.loadTable(pNdb, 1);
	pTrans = pNdb->startTransaction();
	op_row(pTrans, hugoOps, pTab, op1, 1);
	ret = pTrans->execute(et, ao);
	pTrans->close();
	printf("%d ", ret);
	hugoTrans.clearTable(pNdb);
      
	hugoTrans.loadTable(pNdb, 1);
	pTrans = pNdb->startTransaction();
	op_row(pTrans, hugoOps, pTab, op1, 0);
	op_row(pTrans, hugoOps, pTab, op2, 1);
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
  const NdbDictionary::Table* pTab = ctx->getTab();

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

  for (Uint32 i = 0; i<ctx->getNumLoops(); i++)
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
NDBT_TESTSUITE_END(testNdbApi);

int main(int argc, const char** argv){
  ndb_init();
  //  TABLE("T1");
  return testNdbApi.execute(argc, argv);
}

template class Vector<Ndb*>;
template class Vector<NdbConnection*>;
