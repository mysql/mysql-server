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

  return NDBT_OK;
}

NdbConnection* HugoOperations::getTransaction(){
  return pTrans;
}

int HugoOperations::pkReadRecord(Ndb* pNdb,
				 int recordNo,
				 bool exclusive,
				 int numRecords){
  
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if((rows[r]->attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    } 
  }
  return NDBT_OK;
}

int HugoOperations::pkDirtyReadRecord(Ndb* pNdb,
				      int recordNo,
				      int numRecords){
  
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->dirtyRead();

    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if((rows[r]->attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	return NDBT_FAILED;
      }
    } 
  }
  return NDBT_OK;
}

int HugoOperations::pkSimpleReadRecord(Ndb* pNdb,
				       int recordNo,
				       int numRecords){
  
  allocRows(numRecords);
  int check;
  for(int r=0; r < numRecords; r++){
    NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    check = pOp->simpleRead();
    
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      return NDBT_FAILED;
    }
    
    // Define primary keys
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
  
  int check;
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
  
  int check;
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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

int HugoOperations::scanReadRecords(Ndb* pNdb, 
				    Uint32 parallelism, ScanLock lock){

  NdbConnection * pCon = pNdb->hupp(pTrans);
  NDBT_ResultRow * m_tmpRow = new NDBT_ResultRow(tab);
  ScanTmp tmp(pCon, m_tmpRow);
  tmp.m_op = ScanTmp::READ;

  NdbOperation* pOp = pCon->getNdbOperation(tab.getName());	
  if (pOp == NULL) {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }
  
  int check = 0;
  switch(lock){
  case SL_ReadHold:
    check = pOp->openScanReadHoldLock(parallelism);
    break;
  case SL_Exclusive:
    check = pOp->openScanExclusive(parallelism);
    break;
  case SL_Read:
  default:
    check = pOp->openScanRead(parallelism);
  }
  
  if( check == -1 ) {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }
  
  check = pOp->interpret_exit_ok();
  if( check == -1 ) {
    ERR(pCon->getNdbError());
    return NDBT_FAILED;
  }
  
  // Define attributes to read  
  for(int a = 0; a<tab.getNoOfColumns(); a++){
    if((m_tmpRow->attributeStore(a) = 
	pOp->getValue(tab.getColumn(a)->getName())) == 0) {
      ERR(pCon->getNdbError());
      return NDBT_FAILED;
    }
  } 

  check = tmp.pTrans->executeScan();   
  if( check == -1 ) {
    NdbError err = tmp.pTrans->getNdbError();
    ERR(err);
    return err.code;
  }

  tmp.m_delete = false;
  m_scans.push_back(tmp);

  return 0;
}

int HugoOperations::executeScanRead(Ndb* pNdb){

  int check = 0;
  for(Uint32 i = 0; i<m_scans.size(); i++){
    ScanTmp & tmp = m_scans[i];
    check = run(tmp);
    if(check != 0){
      return check;
    }
  }
  while(m_scans.size() > 0){
    ScanTmp & tmp = m_scans[m_scans.size() - 1];
    if(tmp.m_op != ScanTmp::DONE)
      abort();
    
    tmp.pTrans->close();
    delete tmp.m_tmpRow;
    m_scans.erase(m_scans.size() - 1);
  }
  if(check != 0){
    return check;
  }
  
  return NDBT_OK;
}

int HugoOperations::execute_Commit(Ndb* pNdb,
				   AbortOption eao){

  int check = 0;
  while(m_scans.size() > 0){
    ScanTmp & tmp = m_scans[m_scans.size() - 1];
    if(tmp.m_op != ScanTmp::DONE)
      abort();
    
    tmp.pTrans->close();
    delete tmp.m_tmpRow;
    m_scans.erase(m_scans.size() - 1);
  }
  if(check != 0){
    return check;
  }
  
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
  return NDBT_OK;
}

int
HugoOperations::run(ScanTmp & tmp){
  int count = 0;
  if(tmp.m_op == ScanTmp::DONE)
    abort();

  int eof = tmp.pTrans->nextScanResult(true) ;
  while(eof == 0){
    count++;
    switch(tmp.m_op){
    case ScanTmp::READ:
    case ScanTmp::UPDATE:
    case ScanTmp::DELETE:
      break;
    case ScanTmp::DONE:
      abort();
    }
    rows.push_back(tmp.m_tmpRow->clone());
    eof = tmp.pTrans->nextScanResult(false);
  }

  tmp.m_op = ScanTmp::DONE;
  if (eof == -1) {
    deallocRows();
    NdbError err = tmp.pTrans->getNdbError();
    ERR(err);
    return err.code;
  }    

  if(count == 0)
    return 626;

  return 0;
}

int HugoOperations::execute_NoCommit(Ndb* pNdb, AbortOption eao){

  int check;
  for(Uint32 i = 0; i<m_scans.size(); i++){
    ScanTmp & tmp = m_scans[i];
    check = run(tmp);
    if(check != 0){
      return check;
    }
  }

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
  for(Uint32 i = 0; i<m_scans.size(); i++){
    if(m_scans[i].pTrans)
      m_scans[i].pTrans->refresh();
  }
}

int HugoOperations::indexReadRecords(Ndb*, const char * idxName, int recordNo,
				     bool exclusive,
				     int numRecords){
    
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to read  
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey() == true){
	if(equalForAttr(pOp, a, r+recordNo) != 0){
	  ERR(pTrans->getNdbError());
	  return NDBT_FAILED;
	}
      }
    }
    
    // Define attributes to update
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
