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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>

#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; break;\
} 


struct Attrib {
  bool indexCreated;
  int numAttribs;
  int attribs[1024];
  Attrib(){
    numAttribs = 0;
    indexCreated = false;
  }
};
class AttribList {
public:
  AttribList(){};
  ~AttribList(){
    for(size_t i = 0; i < attriblist.size(); i++){      
      delete attriblist[i];
    }
  };
  void buildAttribList(const NdbDictionary::Table* pTab); 
  Vector<Attrib*> attriblist;
};

void AttribList::buildAttribList(const NdbDictionary::Table* pTab){
  attriblist.clear();

  Attrib* attr;
  // Build attrib definitions that describes which attributes to build index
  // Try to build strange combinations, not just "all" or all PK's

  int i;

  for(i = 1; i <= pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = i;
    for(int a = 0; a<i; a++)
      attr->attribs[a] = a;
    attriblist.push_back(attr);
  }
  int b = 0;
  for(i = pTab->getNoOfColumns()-1; i > 0; i--){
    attr = new Attrib;
    attr->numAttribs = i;
    b++;
    for(int a = 0; a<i; a++)
      attr->attribs[a] = a+b;
    attriblist.push_back(attr);
  }
  for(i = pTab->getNoOfColumns(); i > 0;  i--){
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for(int a = 0; a<pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns()-a-1;
    attriblist.push_back(attr); 
  }  
  for(i = 1; i < pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = pTab->getNoOfColumns() - i;
    for(int a = 0; a<pTab->getNoOfColumns() - i; a++)
      attr->attribs[a] = pTab->getNoOfColumns()-a-1;
    attriblist.push_back(attr); 
  }  
  for(i = 1; i < pTab->getNoOfColumns(); i++){
    attr = new Attrib;
    attr->numAttribs = 2;
    for(int a = 0; a<2; a++){
      attr->attribs[a] = i%pTab->getNoOfColumns();
    }
    attriblist.push_back(attr);
  }

  // Last 
  attr = new Attrib;
  attr->numAttribs = 1;
  attr->attribs[0] = pTab->getNoOfColumns()-1;
  attriblist.push_back(attr);

  // Last and first
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = pTab->getNoOfColumns()-1;
  attr->attribs[1] = 0;
  attriblist.push_back(attr); 

  // First and last
  attr = new Attrib;
  attr->numAttribs = 2;
  attr->attribs[0] = 0;
  attr->attribs[1] = pTab->getNoOfColumns()-1;
  attriblist.push_back(attr);  

#if 0
  for(size_t i = 0; i < attriblist.size(); i++){

    ndbout << attriblist[i]->numAttribs << ": " ;
    for(int a = 0; a < attriblist[i]->numAttribs; a++)
      ndbout << attriblist[i]->attribs[a] << ", ";
    ndbout << endl;
  }
#endif

}

char idxName[255];
char pkIdxName[255];

static const int SKIP_INDEX = 99;

int create_index(NDBT_Context* ctx, int indxNum, 
		 const NdbDictionary::Table* pTab, 
		 Ndb* pNdb, Attrib* attr, bool logged){
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  int result  = NDBT_OK;

  HugoCalculator calc(*pTab);

  if (attr->numAttribs == 1 && 
      calc.isUpdateCol(attr->attribs[0]) == true){
    // Don't create index for the Hugo update column
    // since it's not unique
    return SKIP_INDEX;
  }

  // Create index    
  BaseString::snprintf(idxName, 255, "IDC%d", indxNum);
  if (orderedIndex)
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "ordered index "<<idxName << " (";
  else
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "unique index "<<idxName << " (";
  ndbout << flush;
  NdbDictionary::Index pIdx(idxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c< attr->numAttribs; c++){
    int attrNo = attr->attribs[c];
    pIdx.addIndexColumn(pTab->getColumn(attrNo)->getName());
    ndbout << pTab->getColumn(attrNo)->getName()<<" ";
  }
  
  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    attr->indexCreated = false;
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    ERR(err);
    if(err.classification == NdbError::ApplicationError)
      return SKIP_INDEX;
    
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
    attr->indexCreated = true;
  }
  return result;
}


int drop_index(int indxNum, Ndb* pNdb, 
	       const NdbDictionary::Table* pTab, Attrib* attr){
  int result = NDBT_OK;

  if (attr->indexCreated == false)
    return NDBT_OK;	

  BaseString::snprintf(idxName, 255, "IDC%d", indxNum);
  
  // Drop index
  ndbout << "Dropping index "<<idxName<<"(" << pTab->getName() << ") ";
  if (pNdb->getDictionary()->dropIndex(idxName, pTab->getName()) != 0){
    ndbout << "FAILED!" << endl;
    ERR(pNdb->getDictionary()->getNdbError());
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }	
  return result;
}

int runCreateIndexes(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int l = 0;
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  // NOTE If we need to test creating both logged and non logged indexes
  // this should be divided into two testcases
  // The paramater logged should then be specified 
  // as a TC_PROPERTY. ex TC_PROPERTY("LoggedIndexes", 1);
  // and read into the test step like
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  AttribList attrList;
  attrList.buildAttribList(pTab);


  while (l < loops && result == NDBT_OK){
    unsigned int i;
    for (i = 0; i < attrList.attriblist.size(); i++){
      
      // Try to create index
      if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) == NDBT_FAILED)
	result = NDBT_FAILED;      
    }
    
    // Now drop all indexes that where created
    for (i = 0; i < attrList.attriblist.size(); i++){
            
      // Try to drop index
      if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK)
	result = NDBT_FAILED;      		     
    }
    
    l++;
  }
  
  return result;
}

int createRandomIndex(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  AttribList attrList;
  attrList.buildAttribList(pTab);

  int retries = 100;
  while(retries > 0){
    const Uint32 i = rand() % attrList.attriblist.size();
    int res = create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], 
			   logged);
    if (res == SKIP_INDEX){
      retries--;
      continue;
    }

    if (res == NDBT_FAILED){
      return NDBT_FAILED;
    }
    
    ctx->setProperty("createRandomIndex", i);
    // Now drop all indexes that where created
    
    return NDBT_OK;
  }
  
  return NDBT_FAILED;
}

int createRandomIndex_Drop(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);

  Uint32 i = ctx->getProperty("createRandomIndex");
  
  BaseString::snprintf(idxName, 255, "IDC%d", i);
  
  // Drop index
  ndbout << "Dropping index " << idxName << " ";
  if (pNdb->getDictionary()->dropIndex(idxName, 
				       ctx->getTab()->getName()) != 0){
    ndbout << "FAILED!" << endl;
    ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }	
  
  return NDBT_OK;
}

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

int
runVerifyIndex(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 16);
  const int parallelism = batchSize > 240 ? 240 : batchSize;

  do {
    if (utilTrans.verifyIndex(pNdb, idxName, parallelism, true) != 0){
      g_err << "Inconsistent index" << endl;
      return NDBT_FAILED;
    }
  } while(ctx->isTestStopped() == false);
  return NDBT_OK;
}

int
runTransactions1(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0){
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }    

    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;
    
    if (hugoTrans.scanUpdateRecords(pNdb, rows, batchSize) != 0){
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }    
    
    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int
runTransactions2(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
#if 1
    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchSize) != 0){
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }
#endif
    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;
#if 1
    if (hugoTrans.indexUpdateRecords(pNdb, pkIdxName, rows, batchSize) != 0){
      g_err << "Index update failed" << endl;
      return NDBT_FAILED;
    }
#endif
    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int
runTransactions3(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 32);
  const int parallel = batchSize > 240 ? 240 : batchSize;

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if(hugoTrans.loadTable(pNdb, rows, batchSize, false) != 0){
      g_err << "Load table failed" << endl;
      return NDBT_FAILED;
    }
    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;

    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0){
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }    

    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;
    
    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchSize) != 0){
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }
    
    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;
    
    if (hugoTrans.indexUpdateRecords(pNdb, pkIdxName, rows, batchSize) != 0){
      g_err << "Index update failed" << endl;
      return NDBT_FAILED;
    }
    
    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;

    if (hugoTrans.scanUpdateRecords(pNdb, rows, 5, parallel) != 0){
      g_err << "Scan updated table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;

    if(utilTrans.clearTable(pNdb, rows, parallel) != 0){
      g_err << "Clear table failed" << endl;
      return NDBT_FAILED;
    }

    ctx->sync_down("PauseThreads");
    if(ctx->isTestStopped())
      break;
    
    int count = -1;
    if(utilTrans.selectCount(pNdb, 64, &count) != 0 || count != 0)
      return NDBT_FAILED;
    ctx->sync_down("PauseThreads");
  }
  return NDBT_OK;
}

int runRestarts(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NDBT_TestCase* pCase = ctx->getCase();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;
  int sync_threads = ctx->getProperty("Threads", (unsigned)0);

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    if(restarts.executeRestart("RestartRandomNodeAbort", timeout) != 0){
      g_err << "Failed to executeRestart(" <<pCase->getName() <<")" << endl;
      result = NDBT_FAILED;
      break;
    }    
    ctx->sync_up_and_wait("PauseThreads", sync_threads);
    i++;
  }
  ctx->stopTest();
  return result;
}

int runCreateLoadDropIndex(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int l = 0;
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int batchSize = ctx->getProperty("BatchSize", 1);
  int parallelism = batchSize > 240? 240: batchSize;
  ndbout << "batchSize="<<batchSize<<endl;
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  HugoTransactions hugoTrans(*pTab);
  UtilTransactions utilTrans(*pTab);
  AttribList attrList;
  attrList.buildAttribList(pTab);

  for (unsigned int i = 0; i < attrList.attriblist.size(); i++){
        
    while (l < loops && result == NDBT_OK){

      if ((l % 2) == 0){
	// Create index first and then load
	
	// Try to create index
	if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) == NDBT_FAILED){
	  result = NDBT_FAILED;      
	}
	
	// Load the table with data
	ndbout << "Loading data after" << endl;
	CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
	
	
      } else {
	// Load table then create index
	
	// Load the table with data
	ndbout << "Loading data before" << endl;
	CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
	
	// Try to create index
	if (create_index(ctx, i, pTab, pNdb, attrList.attriblist[i], logged) == NDBT_FAILED)
	  result = NDBT_FAILED;      
	
      }
      
      // Verify that data in index match 
      // table data
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      
      // Do it all...
      ndbout <<"Doing it all"<<endl;
      int count;
      ndbout << "  pkUpdateRecords" << endl;
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(hugoTrans.pkUpdateRecords(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  pkDelRecords half" << endl;
      CHECK(hugoTrans.pkDelRecords(pNdb, records/2, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  scanUpdateRecords" << endl;
      CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2, parallelism) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  clearTable" << endl;
      CHECK(utilTrans.clearTable(pNdb, records/2, parallelism) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == 0);
      ndbout << "  loadTable" << endl;
      CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      ndbout << "  loadTable again" << endl;
      CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
      CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
      CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
      CHECK(count == records);


      if ((l % 2) == 0){
	// Drop index first and then clear
	
	// Try to create index
	if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK){
	  result = NDBT_FAILED;      
	}
	
	// Clear table
	ndbout << "Clearing table after" << endl;
	CHECK(hugoTrans.clearTable(pNdb, records, parallelism) == 0);
	
	
      } else {
	// Clear table then drop index
	
	//Clear table
	ndbout << "Clearing table before" << endl;
	CHECK(hugoTrans.clearTable(pNdb, records, parallelism) == 0);
	
	// Try to drop index
	if (drop_index(i, pNdb, pTab, attrList.attriblist[i]) != NDBT_OK)
	  result = NDBT_FAILED;      	
      }
      
      ndbout << "  Done!" << endl;
      l++;
    }
      
    // Make sure index is dropped
    drop_index(i, pNdb, pTab, attrList.attriblist[i]);

  }

  return result;
}

int runInsertDelete(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int batchSize = ctx->getProperty("BatchSize", 1);
  int parallelism = batchSize > 240? 240: batchSize;
  ndbout << "batchSize="<<batchSize<<endl;
  bool logged = ctx->getProperty("LoggedIndexes", 1);

  HugoTransactions hugoTrans(*pTab);
  UtilTransactions utilTrans(*pTab);  

  AttribList attrList;
  attrList.buildAttribList(pTab);

  for (unsigned int i = 0; i < attrList.attriblist.size(); i++){
    
    Attrib* attr = attrList.attriblist[i]; 
    // Create index
    if (create_index(ctx, i, pTab, pNdb, attr, logged) == NDBT_OK){
      int l = 1;
      while (l <= loops && result == NDBT_OK){
  
	CHECK(hugoTrans.loadTable(pNdb, records, batchSize) == 0);
	CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
	CHECK(utilTrans.clearTable(pNdb, records, parallelism) == 0);
	CHECK(utilTrans.verifyIndex(pNdb, idxName, parallelism) == 0);
	l++;	    
      }            
      
      // Drop index
      if (drop_index(i, pNdb, pTab, attr) != NDBT_OK)
	result = NDBT_FAILED;      		     
    }    
  }
  
  return result;
}
int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  HugoTransactions hugoTrans(*ctx->getTab());
  int batchSize = ctx->getProperty("BatchSize", 1);
  if(hugoTrans.loadTable(GETNDB(step), records, batchSize) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runSystemRestart1(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  const char * name = ctx->getTab()->getName();
  while(i<=loops && result != NDBT_FAILED){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    /*
      1. Load data
      2. Restart cluster and verify records
      3. Update records
      4. Restart cluster and verify records
      5. Delete half of the records
      6. Restart cluster and verify records
      7. Delete all records
      8. Restart cluster and verify records
      9. Insert, update, delete records
      10. Restart cluster and verify records
      11. Insert, update, delete records
      12. Restart cluster with error insert 5020 and verify records
    */
    ndbout << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    
    ndbout << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName, 16, false) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName,  16, false) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName,  16, false) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName,  16, false) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.verifyIndex(pNdb, idxName,  16, false) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 1) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster with error insert 5020..." << endl;
    CHECK(restarter.restartAll(false, true) == 0);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);
    CHECK(restarter.insertErrorInAllNodes(5020) == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    
    i++;
  }

  ctx->stopTest();
  ndbout << "runSystemRestart1 finished" << endl;  

  return result;
}

#define CHECK2(b, t) if(!b){ g_err << __LINE__ << ": " << t << endl; break;}

int
runMixed1(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());


  do {
    // TC1
    g_err << "pkRead, indexRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0, "indexReadRecords");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    // TC1
    g_err << "pkRead, indexRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0, "indexReadRecords");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");
    

    // TC2
    g_err << "pkRead, indexRead, NoCommit, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecord");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0,
	   "indexReadRecords");
    CHECK2(hugoOps.execute_NoCommit(pNdb) == 0, "executeNoCommit");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction");

    // TC3
    g_err << "pkRead, pkRead, Commit" << endl;
    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecords ");
    CHECK2(hugoOps.pkReadRecord(pNdb, 0) == 0, "pkReadRecords ");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");
    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");

    // TC4
    g_err << "indexRead, indexRead, Commit" << endl;

    CHECK2(hugoOps.startTransaction(pNdb) == 0, "startTransaction ");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0, "indexReadRecords");
    CHECK2(hugoOps.indexReadRecords(pNdb, pkIdxName, 0) == 0, "indexReadRecords");
    CHECK2(hugoOps.execute_Commit(pNdb) == 0, "executeCommit");

    CHECK2(hugoOps.closeTransaction(pNdb) == 0, "closeTransaction ");

    return NDBT_OK;
  } while(false);


  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int
runBuildDuring(NDBT_Context* ctx, NDBT_Step* step){
  // Verify that data in index match 
  // table data
  const int Threads = ctx->getProperty("Threads", (Uint32)0);
  const int loops = ctx->getNumLoops();

  for(int i = 0; i<loops; i++){
#if 1
    if(createPkIndex(ctx, step) != NDBT_OK){
      g_err << "Failed to create index" << endl;
      return NDBT_FAILED;
    }
#endif

    if(ctx->isTestStopped())
      break;

#if 1
    if(createRandomIndex(ctx, step) != NDBT_OK){
      g_err << "Failed to create index" << endl;
      return NDBT_FAILED;
    }
#endif

    if(ctx->isTestStopped())
      break;

    ctx->setProperty("pause", 1);
    int count = 0;
    for(int j = 0; count < Threads && !ctx->isTestStopped(); 
	j = (j+1) % Threads){
      char buf[255];
      sprintf(buf, "Thread%d_paused", j);
      int tmp = ctx->getProperty(buf, (Uint32)0);
      count += tmp;
    }
    
    if(ctx->isTestStopped())
      break;

#if 1
    if(createPkIndex_Drop(ctx, step) != NDBT_OK){
      g_err << "Failed to drop index" << endl;
      return NDBT_FAILED;
    }
#endif

    if(ctx->isTestStopped())
      break;
    
#if 1
    if(createRandomIndex_Drop(ctx, step) != NDBT_OK){
      g_err << "Failed to drop index" << endl;
      return NDBT_FAILED;
    }
#endif

    ctx->setProperty("pause", (Uint32)0);
    NdbSleep_SecSleep(2);
  }

  ctx->stopTest();
  return NDBT_OK;
}

static NdbLockable g_lock;
static int threadCounter = 0;

void
wait_paused(NDBT_Context* ctx, int id){
  if(ctx->getProperty("pause", (Uint32)0) == 1){
    char buf[255];
    sprintf(buf, "Thread%d_paused", id);
    ctx->setProperty(buf, 1);
    while(!ctx->isTestStopped() && ctx->getProperty("pause", (Uint32)0) == 1){
      NdbSleep_MilliSleep(250);
    }
    ctx->setProperty(buf, (Uint32)0);
  }
}

int
runTransactions4(NDBT_Context* ctx, NDBT_Step* step){

  g_lock.lock();
  const int ThreadId = threadCounter++;
  g_lock.unlock();
  
  // Verify that data in index match 
  // table data
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 32);
  const int parallel = batchSize > 240 ? 240 : batchSize;

  int rows = ctx->getNumRecords();
  while (ctx->isTestStopped() == false) {
    if(hugoTrans.loadTable(pNdb, rows, batchSize, false) != 0){
      g_err << "Load table failed" << endl;
      return NDBT_FAILED;
    }

    wait_paused(ctx, ThreadId);

    if(ctx->isTestStopped())
      break;

    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0){
      g_err << "Updated table failed" << endl;
      return NDBT_FAILED;
    }    

    wait_paused(ctx, ThreadId);
    
    if(ctx->isTestStopped())
      break;
    
    if (hugoTrans.scanUpdateRecords(pNdb, rows, 5, parallel) != 0){
      g_err << "Scan updated table failed" << endl;
      return NDBT_FAILED;
    }

    wait_paused(ctx, ThreadId);

    if(ctx->isTestStopped())
      break;

    if(utilTrans.clearTable(pNdb, rows, parallel) != 0){
      g_err << "Clear table failed" << endl;
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int
runUniqueNullTransactions(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);

  bool logged = ctx->getProperty("LoggedIndexes", 1);
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  NdbConnection * pTrans = 0;

  const NdbDictionary::Table* pTab = ctx->getTab();
  // Create index    
  char nullIndex[255];
  BaseString::snprintf(nullIndex, 255, "IDC_PK_%s_NULL", pTab->getName());
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
  pIdx.setStoredIndex(logged);
  int c;
  for (c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey()){
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() <<" ";
    }
  }
  
  int colId = -1;
  for (c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getNullable()){
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() <<" ";
      colId = c;
      break;
    }
  }
  ndbout << ") ";

  if(colId == -1){
    ndbout << endl << "No nullable column found -> NDBT_FAILED" << endl; 
    return NDBT_FAILED;
  }
  
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    ERR(err);
    return NDBT_FAILED;
  }
  
  int result = NDBT_OK;

  HugoTransactions hugoTrans(*ctx->getTab());
  const int batchSize = ctx->getProperty("BatchSize", 50);
  int loops = ctx->getNumLoops();
  int rows = ctx->getNumRecords();
  while (loops-- > 0 && ctx->isTestStopped() == false) {
    if (hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) != 0){
      g_err << "Updated table failed" << endl;
      result = NDBT_FAILED;
      goto done;
    }    
  }

  if(ctx->isTestStopped()){
    goto done;
  }

  ctx->stopTest();
  while(ctx->getNoOfRunningSteps() > 1){
    NdbSleep_MilliSleep(100);
  }

  result = NDBT_FAILED;
  pTrans = pNdb->startTransaction();
  NdbScanOperation * sOp;
  NdbOperation * uOp;
  NdbResultSet * rs;
  int eof;
  if(!pTrans) goto done;
  sOp = pTrans->getNdbScanOperation(pTab->getName());
  if(!sOp) goto done;
  rs = sOp->readTuples(NdbScanOperation::LM_Exclusive);
  if(!rs) goto done;
  if(pTrans->execute(NoCommit) == -1) goto done;
  while((eof = rs->nextResult(true)) == 0){
    do {
      NdbOperation * uOp = rs->updateTuple();
      if(uOp == 0) goto done;
      uOp->setValue(colId, 0);
    } while((eof = rs->nextResult(false)) == 0);
    eof = pTrans->execute(Commit);
    if(eof == -1) goto done;
  }
  
 done:
  if(pTrans) pNdb->closeTransaction(pTrans);
  pNdb->getDictionary()->dropIndex(nullIndex, pTab->getName());
  return result;
}

int runLQHKEYREF(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops() * 100;
  NdbRestarter restarter;
  
  myRandom48Init(NdbTick_CurrentMillisecond());

#if 0
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if(restarter.dumpStateAllNodes(&val, 1) != 0){
    g_err << "Failed to dump DihMinTimeBetweenLCP" << endl;
    return NDBT_FAILED;
  }
#endif

  for(int i = 0; i<loops && !ctx->isTestStopped(); i++){
    int randomId = myRandom48(restarter.getNumDbNodes());
    int nodeId = restarter.getDbNodeId(randomId);

    const Uint32 error = 5031 + (i % 3);
    
    if(restarter.insertErrorInNode(nodeId, error) != 0){
      g_err << "Failed to error insert( " << error << ") in node "
	    << nodeId << endl;
      return NDBT_FAILED;
    }
  }
  
  ctx->stopTest();
  return NDBT_OK;
}

int 
runBug21384(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());
  NdbRestarter restarter;
  
  int loops = ctx->getNumLoops();
  const int rows = ctx->getNumRecords();
  const int batchsize = ctx->getProperty("BatchSize", 50);
  
  while (loops--)
  {
    if(restarter.insertErrorInAllNodes(8037) != 0)
    {
      g_err << "Failed to error insert(8037)" << endl;
      return NDBT_FAILED;
    }
    
    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchsize) == 0)
    {
      g_err << "Index succeded (it should have failed" << endl;
      return NDBT_FAILED;
    }
    
    if(restarter.insertErrorInAllNodes(0) != 0)
    {
      g_err << "Failed to error insert(0)" << endl;
      return NDBT_FAILED;
    }
    
    if (hugoTrans.indexReadRecords(pNdb, pkIdxName, rows, batchsize) != 0){
      g_err << "Index read failed" << endl;
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}



NDBT_TESTSUITE(testIndex);
TESTCASE("CreateAll", 
	 "Test that we can create all various indexes on each table\n"
	 "Then drop the indexes\n"){
  INITIALIZER(runCreateIndexes);
}
TESTCASE("CreateAll_O",
	 "Test that we can create all various indexes on each table\n"
	 "Then drop the indexes\n"){
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runCreateIndexes);
}
TESTCASE("InsertDeleteGentle", 
	 "Create one index, then perform insert and delete in the table\n"
	 "loop number of times. Use batch size 1."){
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("InsertDeleteGentle_O",
	 "Create one index, then perform insert and delete in the table\n"
	 "loop number of times. Use batch size 1."){
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);
}
TESTCASE("InsertDelete", 
	 "Create one index, then perform insert and delete in the table\n"
	 "loop number of times. Use batchsize 512 to stress db more"){
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);

}
TESTCASE("InsertDelete_O", 
	 "Create one index, then perform insert and delete in the table\n"
	 "loop number of times. Use batchsize 512 to stress db more"){
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runInsertDelete);
  FINALIZER(runClearTable);

}
TESTCASE("CreateLoadDropGentle", 
	 "Try to create, drop and load various indexes \n"
	 "on table loop number of times.Usa batch size 1.\n"){
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("CreateLoadDropGentle_O", 
	 "Try to create, drop and load various indexes \n"
	 "on table loop number of times.Usa batch size 1.\n"){
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 1);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("CreateLoadDrop", 
	 "Try to create, drop and load various indexes \n"
	 "on table loop number of times. Use batchsize 512 to stress db more\n"){
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("CreateLoadDrop_O", 
	 "Try to create, drop and load various indexes \n"
	 "on table loop number of times. Use batchsize 512 to stress db more\n"){
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", 512);
  INITIALIZER(runCreateLoadDropIndex);
}
TESTCASE("NFNR1", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  //TC_PROPERTY("Threads", 2);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR1_O", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR2", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR2_O", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions2);
  //STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR3", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runRestarts);
  STEP(runTransactions3);
  STEP(runVerifyIndex);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR3_O", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runRestarts);
  STEP(runTransactions3);
  STEP(runVerifyIndex);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR4", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR4_O", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR5", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", (unsigned)1);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runLQHKEYREF);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("NFNR5_O", 
	 "Test that indexes are correctly maintained during node fail and node restart"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("BatchSize", (unsigned)1);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runLQHKEYREF);
  STEP(runTransactions1);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runTransactions2);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("SR1", 
	 "Test that indexes are correctly maintained during SR"){ 
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runSystemRestart1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("MixedTransaction", 
	 "Test mixing of index and normal operations"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runMixed1);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("SR1_O", 
	 "Test that indexes are correctly maintained during SR"){ 
  TC_PROPERTY("OrderedIndex", 1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  STEP(runSystemRestart1);
  FINALIZER(runVerifyIndex);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("BuildDuring", 
	 "Test that index build when running transactions work"){ 
  TC_PROPERTY("OrderedIndex", (unsigned)0);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("Threads", 1); // # runTransactions4
  INITIALIZER(runClearTable);
  STEP(runBuildDuring);
  STEP(runTransactions4);
  //STEP(runTransactions4);
  FINALIZER(runClearTable);
}
TESTCASE("BuildDuring_O", 
	 "Test that index build when running transactions work"){ 
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  TC_PROPERTY("Threads", 1); // # runTransactions4
  INITIALIZER(runClearTable);
  STEP(runBuildDuring);
  STEP(runTransactions4);
  //STEP(runTransactions4);
  FINALIZER(runClearTable);
}
TESTCASE("UniqueNull", 
	 "Test that unique indexes and nulls"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createRandomIndex);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runTransactions1);
  STEP(runTransactions2);
  STEP(runUniqueNullTransactions);
  FINALIZER(runVerifyIndex);
  FINALIZER(createRandomIndex_Drop);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
TESTCASE("Bug21384", 
	 "Test that unique indexes and nulls"){ 
  TC_PROPERTY("LoggedIndexes", (unsigned)0);
  INITIALIZER(runClearTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runLoadTable);
  STEP(runBug21384);
  FINALIZER(createPkIndex_Drop);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testIndex);

int main(int argc, const char** argv){
  ndb_init();
  return testIndex.execute(argc, argv);
}

template class Vector<Attrib*>;
