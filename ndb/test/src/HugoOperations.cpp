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

#include <HugoOperations.hpp>


int HugoOperations::startTransaction(Ndb* pNdb){
  
  if (pTrans != NULL){
    ndbout << "HugoOperations::startTransaction, pTrans != NULL" << endl;
    return NDBT_FAILED;
  }
  pTrans = pNdb->startTransaction();
  if (pTrans == NULL) {
    const NdbError err = pNdb->getNdbError();
    ERR(err);
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int HugoOperations::closeTransaction(Ndb* pNdb){

  if (pTrans != NULL){
    pNdb->closeTransaction(pTrans);
    pTrans = NULL;
  }
  pTrans = NULL;

  m_result_sets.clear();
  m_executed_result_sets.clear();

  return NDBT_OK;
}

NdbConnection* HugoOperations::getTransaction(){
  return pTrans;
}

int HugoOperations::pkReadRecord(Ndb* pNdb,
				 int recordNo,
				 int numRecords,
				 NdbOperation::LockMode lm){
  int a;  
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
rand_lock_mode:
    switch(lm){
    case NdbOperation::LM_Read:
      check = pOp->readTuple();
      break;
    case NdbOperation::LM_Exclusive:
      check = pOp->readTupleExclusive();
      break;
    case NdbOperation::LM_CommittedRead:
      check = pOp->dirtyRead();
      break;
    default:
      lm = (NdbOperation::LockMode)((rand() >> 16) & 3);
      goto rand_lock_mode;
    }
    
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((rows[r]->attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    } 
  }
  return NDBT_OK;
}

int HugoOperations::pkUpdateRecord(Ndb* pNdb,
				   int recordNo,
				   int numRecords,
				   int updatesValue){
  int a; 
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->updateTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == false){
	if(setValueForAttr(pOp, a, recordNo+r, updatesValue ) != 0){ 
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    } 
  }
  return NDBT_OK;
}

int HugoOperations::pkInsertRecord(Ndb* pNdb,
				   int recordNo,
				   int numRecords,
				   int updatesValue){
  
  int a, check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->insertTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == false){
	if(setValueForAttr(pOp, a, recordNo+r, updatesValue ) != 0){ 
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    } 
  }
  return NDBT_OK;
}

int HugoOperations::pkDeleteRecord(Ndb* pNdb,
				   int recordNo,
				   int numRecords){
  
  int a, check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->deleteTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
  }
  return NDBT_OK;
}
#if 0
NdbResultSet*
HugoOperations::scanReadRecords(Ndb* pNdb, ScanLock lock){
  
  NDBT_ResultRow * m_tmpRow = new NDBT_ResultRow(tab);

  NdbScanOperation* pOp = pTrans->getNdbScanOperation(tab.getName());	
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    return 0;
  }
  

  int check = 0;
  NdbResultSet * rs = 0;
  switch(lock){
  case SL_ReadHold:
    rs = pOp->readTuples(NdbScanOperation::LM_Read, 1, 1);
    break;
  case SL_Exclusive:
    rs = pOp->readTuples(NdbScanOperation::LM_Exclusive, 1, 1);
    break;
  case SL_Read:
  default:
    rs = pOp->readTuples(NdbScanOperation::LM_Dirty, 1, 1);
  }
  
  if( rs == 0) {
    ERR(pTrans->getNdbError());
    return 0;
  }
  
  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    return 0;
  }
  
  // Define attributes to read  
  for(int a = 0; a<tab.getNoOfColumns(); a++){
    if((m_tmpRow->attributeStore(a) = 
	pOp->getValue(tab.getColumn(a)->getName())) == 0) {
      ERR(pTrans->getNdbError());
      return 0;
    }
  } 
  return rs;
}

int
HugoOperations::readTuples(NdbResultSet* rs){
  int res = 0;
  while((res = rs->nextResult()) == 0){
  }
  if(res != 1)
    return NDBT_FAILED;
  return NDBT_OK;
}
#endif

int HugoOperations::execute_Commit(Ndb* pNdb,
				   AbortOption eao){

  int check = 0;
  check = pTrans->execute(Commit, eao);   

  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    ERR(err);
    NdbOperation* pOp = pTrans->getNdbErrorOperation();
    if (pOp != NULL){
      const NdbError err2 = pOp->getNdbError();
      ERR(err2);
    }
    if (err.code == 0)
      return NDBT_FAILED;
    return err.code;
  }

  for(int i = 0; i<m_result_sets.size(); i++){
    m_executed_result_sets.push_back(m_result_sets[i]);

    int rows = m_result_sets[i].records;
    NdbResultSet* rs = m_result_sets[i].m_result_set;
    int res = rs->nextResult();
    switch(res){
    case 1:
      return 626;
    case -1:
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      return (err.code > 0 ? err.code : NDBT_FAILED);
    }

    // A row found

    switch(rows){
    case 0:
      return 4000;
    default:
      m_result_sets[i].records--;
      break;
    }
  }

  m_result_sets.clear();
  
  return NDBT_OK;
}

int HugoOperations::execute_NoCommit(Ndb* pNdb, AbortOption eao){

  int check;
  check = pTrans->execute(NoCommit, eao);   

  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    ERR(err);
    NdbOperation* pOp;
    while ((pOp = pTrans->getNdbErrorOperation()) != NULL){
      const NdbError err2 = pOp->getNdbError();
      ERR(err2);
    }
    if (err.code == 0)
      return NDBT_FAILED;
    return err.code;
  }

  for(int i = 0; i<m_result_sets.size(); i++){
    m_executed_result_sets.push_back(m_result_sets[i]);

    int rows = m_result_sets[i].records;
    NdbResultSet* rs = m_result_sets[i].m_result_set;
    int res = rs->nextResult();
    switch(res){
    case 1:
      return 626;
    case -1:
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      return (err.code > 0 ? err.code : NDBT_FAILED);
    }

    // A row found

    switch(rows){
    case 0:
      return 4000;
    default:
    case 1:
      break;
    }
  }

  m_result_sets.clear();

  return NDBT_OK;
}

int HugoOperations::execute_Rollback(Ndb* pNdb){
  int check;
  check = pTrans->execute(Rollback);   
  if( check == -1 ) {
    const NdbError err = pTrans->getNdbError();
    ERR(err);
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

HugoOperations::HugoOperations(const NdbDictionary::Table& _tab):
  UtilTransactions(_tab),
  calc(_tab),
  pTrans(NULL){

}

HugoOperations::~HugoOperations(){
  deallocRows();
}


int HugoOperations::equalForAttr(NdbOperation* pOp,
				   int attrId, 
				   int rowId){
  int check = 0;
  const NdbDictionary::Column* attr = tab.getColumn(attrId);  
  if (attr->getPrimaryKey() == false){
    g_info << "Can't call equalForAttr on non PK attribute" << endl;
    return NDBT_FAILED;
  }
    
  switch (attr->getType()){
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Binary:
  case NdbDictionary::Column::Varbinary:{
    char buf[8000];
    memset(buf, 0, sizeof(buf));
    check = pOp->equal( attr->getName(), calc.calcValue(rowId, attrId, 0, buf));
    break;
  }
  case NdbDictionary::Column::Int:
    check = pOp->equal( attr->getName(), (Int32)calc.calcValue(rowId, attrId, 0));
    break;
  case NdbDictionary::Column::Unsigned:
    check = pOp->equal( attr->getName(), (Uint32)calc.calcValue(rowId, attrId, 0));
    break;
  case NdbDictionary::Column::Bigint:
    check = pOp->equal( attr->getName(), (Int64)calc.calcValue(rowId, attrId, 0));
    break;
  case NdbDictionary::Column::Bigunsigned:    
    check = pOp->equal( attr->getName(), (Uint64)calc.calcValue(rowId, attrId, 0));
    break;
  case NdbDictionary::Column::Float:
    g_info << "Float not allowed as PK value" << endl;
    check = -1;
    break;
    
  default:
    g_info << "default" << endl;
    check = -1;
    break;
  }
  return check;
}

int HugoOperations::setValueForAttr(NdbOperation* pOp,
				      int attrId, 
				      int rowId,
				      int updateId){
  int check = 0;
  const NdbDictionary::Column* attr = tab.getColumn(attrId);     

  if (attr->getTupleKey()){
    // Don't set values for TupleId PKs
    return check;
  }
  
  switch (attr->getType()){
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Binary:
  case NdbDictionary::Column::Varbinary:{
    char buf[8000];
    check = pOp->setValue( attr->getName(), 
			   calc.calcValue(rowId, attrId, updateId, buf));
    break;
  }
  case NdbDictionary::Column::Int:{
    Int32 val = calc.calcValue(rowId, attrId, updateId);
    check = pOp->setValue( attr->getName(), val);
  }
    break;
  case NdbDictionary::Column::Bigint:{
    Int64 val = calc.calcValue(rowId, attrId, updateId);
    check = pOp->setValue( attr->getName(), 
			   val);
  }
    break;
  case NdbDictionary::Column::Unsigned:{
    Uint32 val = calc.calcValue(rowId, attrId, updateId);
    check = pOp->setValue( attr->getName(), val);
  }
    break;
  case NdbDictionary::Column::Bigunsigned:{
    Uint64 val = calc.calcValue(rowId, attrId, updateId);
    check = pOp->setValue( attr->getName(), 
			   val);
  }
    break;
  case NdbDictionary::Column::Float:
    check = pOp->setValue( attr->getName(), 
			   (float)calc.calcValue(rowId, attrId, updateId));
    break;
  default:
    check = -1;
    break;
  }
  return check;
}

int
HugoOperations::verifyUpdatesValue(int updatesValue, int _numRows){
  _numRows = (_numRows == 0 ? rows.size() : _numRows);
  
  int result = NDBT_OK;
  
  for(int i = 0; i<_numRows; i++){
    if(calc.verifyRowValues(rows[i]) != NDBT_OK){
      g_err << "Inconsistent row" 
	    << endl << "\t" << rows[i]->c_str().c_str() << endl;
      result = NDBT_FAILED;
      continue;
    }

    if(calc.getUpdatesValue(rows[i]) != updatesValue){
      result = NDBT_FAILED;
      g_err << "Invalid updates value for row " << i << endl
	    << " updatesValue: " << updatesValue << endl
	    << " calc.getUpdatesValue: " << calc.getUpdatesValue(rows[i]) << endl 
	    << rows[i]->c_str().c_str() << endl;
      continue;
    }
  }

  if(_numRows == 0){
    g_err << "No rows -> Invalid updates value" << endl;
    return NDBT_FAILED;
  }

  return result;
}

void HugoOperations::allocRows(int _numRows){
  deallocRows();

  if(_numRows <= 0){
    g_info << "Illegal value for num rows : " << _numRows << endl;
    abort();
  }
  
  for(int b=0; b<_numRows; b++){
    rows.push_back(new NDBT_ResultRow(tab));
  }
}

void HugoOperations::deallocRows(){
  while(rows.size() > 0){
    delete rows.back();
    rows.erase(rows.size() - 1);
  }
}

int HugoOperations::saveCopyOfRecord(int numRecords ){

  if (numRecords > (int)rows.size())
    return NDBT_FAILED;

  for (int i = 0; i < numRecords; i++){
    savedRecords.push_back(rows[i]->c_str());    
  }
  return NDBT_OK;  
}

BaseString HugoOperations::getRecordStr(int recordNum){
  if (recordNum > (int)rows.size())
    return NULL;
  return rows[recordNum]->c_str();
}

int HugoOperations::getRecordGci(int recordNum){
  return pTrans->getGCI();
}


int HugoOperations::compareRecordToCopy(int numRecords ){
  if (numRecords > (int)rows.size())
    return NDBT_FAILED;
  if ((unsigned)numRecords > savedRecords.size())
    return NDBT_FAILED;

  int result = NDBT_OK;
  for (int i = 0; i < numRecords; i++){
    BaseString str = rows[i]->c_str();
    ndbout << "row["<<i<<"]: " << str << endl;
    ndbout << "sav["<<i<<"]: " << savedRecords[i] << endl;
    if (savedRecords[i] == str){
      ;
    } else {
      result = NDBT_FAILED;
    }    
  }
  return result;
}

void
HugoOperations::refresh() {
  NdbConnection* t = getTransaction(); 
  if(t)
    t->refresh();
}

int HugoOperations::indexReadRecords(Ndb*, const char * idxName, int recordNo,
				     bool exclusive,
				     int numRecords){
    
  int a;
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    if (exclusive == true)
      check = pOp->readTupleExclusive();
    else
      check = pOp->readTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((rows[r]->attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    } 
  }
  return NDBT_OK;
}

int 
HugoOperations::indexUpdateRecord(Ndb*,
				  const char * idxName, 
				  int recordNo,
				  int numRecords,
				  int updatesValue){
  int a; 
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->updateTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == false){
	if(setValueForAttr(pOp, a, recordNo+r, updatesValue ) != 0){ 
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    } 
  }
  return NDBT_OK;
}

int 
HugoOperations::scanReadRecords(Ndb* pNdb, NdbScanOperation::LockMode lm,
				int records){

  allocRows(records);
  NdbScanOperation * pOp = pTrans->getNdbScanOperation(tab.getName());
  
  if(!pOp)
    return -1;

  NdbResultSet * rs = pOp->readTuples(lm, 1, 1);
  
  if(!rs){
    return -1;
  }

  for(int a = 0; a<tab.getNoOfColumns(); a++){
    if((rows[0]->attributeStore(a) = 
	pOp->getValue(tab.getColumn(a)->getName())) == 0) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
  } 

  RsPair p = {rs, records};
  m_result_sets.push_back(p);
  
  return 0;
}

template class Vector<HugoOperations::RsPair>;
