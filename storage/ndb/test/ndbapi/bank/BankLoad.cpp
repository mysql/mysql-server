/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "Bank.hpp"
#include <UtilTransactions.hpp>

/**
 * Default account types
 *
 */
struct AccountTypesStruct {
  int id;
  const char descr[64];
};
const AccountTypesStruct accountTypes[] = {
  { 0, "KASSA"},
  { 1, "BANKOMAT"},
  { 2, "POSTGIRO"},
  { 3, "LONEKONTO"},
  { 4, "SPARKONTO"}
};

const int
accountTypesSize = sizeof(accountTypes)/sizeof(AccountTypesStruct);


const char*  tableNames[] = {
  "GL",
  "ACCOUNT", 
  "SYSTEM_VALUES",
  "TRANSACTION",
  "ACCOUNT_TYPE"
};

const int
tableNamesSize = sizeof(tableNames)/sizeof(const char*);


int Bank::getNumAccountTypes(){
  return accountTypesSize;
}

int Bank::createAndLoadBank(bool ovrWrt, bool disk, int num_accounts){

  m_ndb.init();   
  if (m_ndb.waitUntilReady() != 0)
    return NDBT_FAILED;

  const NdbDictionary::Table* pSysValTab = 
    m_ndb.getDictionary()->getTable("SYSTEM_VALUES");
  if (pSysValTab != NULL){
    // The table exists
    if (ovrWrt == false){
      ndbout << "Bank already exist and overwrite == false" << endl;
      return NDBT_FAILED;
    }
  }
  
  if (!m_skip_create && createTables(disk) != NDBT_OK)
    return NDBT_FAILED;
  
  if (clearTables() != NDBT_OK)
    return NDBT_FAILED;
  
  if (loadAccountType() != NDBT_OK)
    return NDBT_FAILED;
  
  if (loadAccount(num_accounts) != NDBT_OK)
    return NDBT_FAILED;
    
  if (loadSystemValues() != NDBT_OK)
    return NDBT_FAILED;

  if (loadGl() != NDBT_OK)
    return NDBT_FAILED;
  
  return NDBT_OK;

}

int Bank::dropBank(){

  m_ndb.init();   
  if (m_ndb.waitUntilReady() != 0)
    return NDBT_FAILED;

  if (dropTables() != NDBT_OK)
    return NDBT_FAILED;
  
  return NDBT_OK;

}

int Bank::createTables(bool disk){
  for (int i = 0; i < tableNamesSize; i++){
    if (createTable(tableNames[i], disk) != NDBT_OK)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}


int Bank::dropTables(){
  for (int i = 0; i < tableNamesSize; i++){
    if (dropTable(tableNames[i]) != NDBT_OK)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int Bank::clearTables(){
  for (int i = 0; i < tableNamesSize; i++){
    if (clearTable(tableNames[i]) != NDBT_OK)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}
   
int Bank::clearTable(const char* tabName){ 
  UtilTransactions util(&m_ndb, tabName);
  if(util.clearTable(&m_ndb, 64) != 0)
    return NDBT_FAILED;
  return NDBT_OK;
}

int Bank::createTable(const char* tabName, bool disk){    
  ndbout << "createTable " << tabName << endl;

  const NdbDictionary::Table* pTab = NDBT_Tables::getTable(tabName);
  if (pTab == NULL)
    return NDBT_FAILED;
  
  const NdbDictionary::Table* org = 
    m_ndb.getDictionary()->getTable(tabName);
  
  if (org != 0 && (disk || pTab->equal(* org)))
  {
    return NDBT_OK;
  }
  
  if (org != 0){
    ndbout << "Different table with same name exists" << endl;
    return NDBT_FAILED;
  }

  if (disk)
  {
    if (NDBT_Tables::create_default_tablespace(&m_ndb))
    {
      ndbout << "Failed to create tablespaces" << endl;
      return NDBT_FAILED;
    }
    NdbDictionary::Table copy(* pTab);
    copy.setTablespaceName("DEFAULT-TS");
    for (int i = 0; i<copy.getNoOfColumns(); i++)
      copy.getColumn(i)->setStorageType(NdbDictionary::Column::StorageTypeDisk);
    if(m_ndb.getDictionary()->createTable(copy) == -1){
      ndbout << "Failed to create table: " <<
	m_ndb.getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  else
  {
    if(m_ndb.getDictionary()->createTable(* pTab) == -1){
      ndbout << "Failed to create table: " <<
	m_ndb.getNdbError() << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;    
}

int Bank::dropTable(const char* tabName){    
  const NdbDictionary::Table* org = 
    m_ndb.getDictionary()->getTable(tabName);
  
  if (org == NULL)
    return NDBT_OK;
  
  ndbout << "dropTable " <<tabName<<endl;
  if (m_ndb.getDictionary()->dropTable(tabName)  != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;    
}








/**
 * Load SYSTEM_VALUES table
 *  This table keeps track of system wide settings
 *  For example:
 *   - next transaction id
 *
 */
int Bank::loadSystemValues (){
int result;

/**
 * Insert start value for next transaction id
 *
 */
result = writeSystemValue(LastTransactionId, 0);

/**
 * Insert start value for current time
 *
 */
result = writeSystemValue(CurrentTime, 1);

return result;

}


/**
 * Load GL table
 * 
 * Insert GL records for time = 0 with balance 0
 */
int Bank::loadGl(){
  g_info << "loadGl" << endl;
  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    NDB_ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  for (int i = 0; i < getNumAccountTypes(); i++){
      
    NdbOperation* pOp = pTrans->getNdbOperation("GL");
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    check = pOp->insertTuple();
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    Uint64 time = 0;
    check = pOp->equal("TIME", time);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    check = pOp->equal("ACCOUNT_TYPE", i);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    Uint32 balance = 0;
    if (getBalanceForAccountType(i, balance) != NDBT_OK){
      return NDBT_FAILED;
    }

    check = pOp->setValue("BALANCE", balance);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    Uint32 depositCount = 0;
    check = pOp->setValue("DEPOSIT_COUNT", depositCount);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    Uint32 depositSum = 0;
    check = pOp->setValue("DEPOSIT_SUM", depositSum);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    Uint32 withdrawalCount = 0;
    check = pOp->setValue("WITHDRAWAL_COUNT", withdrawalCount);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    Uint32 withdrawalSum = 0;
    check = pOp->setValue("WITHDRAWAL_SUM", withdrawalSum);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    Uint32 purged = 1;
    check = pOp->setValue("PURGED", purged);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
  }
  check = pTrans->execute(Commit, AbortOnError);
  if( check == -1 ) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;
} 


int Bank::getBalanceForAccountType(const Uint32 accountType,
				   Uint32& balance){
  int check;
  g_info << "getBalanceForAccountType: accountType="<<accountType<<endl;
    
  NdbConnection* pScanTrans = m_ndb.startTransaction();
  if (pScanTrans == NULL) {
    NDB_ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
      
  NdbScanOperation* pOp = pScanTrans->getNdbScanOperation("ACCOUNT");	
  if (pOp == NULL) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  if( pOp->readTuples() ) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* accountTypeRec = pOp->getValue("ACCOUNT_TYPE");
  if( accountTypeRec ==NULL ) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  NdbRecAttr* balanceRec = pOp->getValue("BALANCE");
  if( balanceRec ==NULL ) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }

  check = pScanTrans->execute(NoCommit, AbortOnError);
  if( check == -1 ) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  int eof;
  int rows = 0;
  eof = pOp->nextResult();
    
  while(eof == 0){
    rows++;
    Uint32 a = accountTypeRec->u_32_value();
    Uint32 b = balanceRec->u_32_value();

    if (a == accountType){
      // One record found
      balance += b;
    }
		
    eof = pOp->nextResult();
  }
  if (eof == -1) {
    NDB_ERR(pScanTrans->getNdbError());
    m_ndb.closeTransaction(pScanTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pScanTrans);
  // ndbout << rows << " rows have been read" << endl;

  return NDBT_OK;

}
  
/**
 * Load ACCOUNT_TYPE table
 * 
 *
 */
int Bank::loadAccountType(){
  g_info << "loadAccountType" << endl;
  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    NDB_ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  for (int i = 0; i < getNumAccountTypes(); i++){
      
    NdbOperation* pOp = pTrans->getNdbOperation("ACCOUNT_TYPE");
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    check = pOp->insertTuple();
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    check = pOp->equal("ACCOUNT_TYPE_ID", accountTypes[i].id);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->setValue("DESCRIPTION", accountTypes[i].descr);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  }
  check = pTrans->execute(Commit, AbortOnError);
  if( check == -1 ) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;
}
  
/**
 * Load ACCOUNT table
 * 
 *  
 *
 */
int Bank::loadAccount (int numAccounts){
  g_info << "loadAccount" << endl;
  int check;
    
  NdbConnection* pTrans = m_ndb.startTransaction();
  if (pTrans == NULL){
    NDB_ERR(m_ndb.getNdbError());
    return NDBT_FAILED;
  }
    
  for (int i = 0; i < numAccounts; i++){
      
    NdbOperation* pOp = pTrans->getNdbOperation("ACCOUNT");
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    check = pOp->insertTuple();
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    check = pOp->equal("ACCOUNT_ID", i);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int owner;
    if (i == 0)
      owner = 0;
    else
      owner = i + 3000;
    check = pOp->setValue("OWNER", owner);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    // Load balance so that the bank's account = 0 has 10 millions
    // and all other accounts have 10000
    // This set the total balance for the entire bank to 
    // 10000000 + (10000 * numAccounts-1)
    // Since no money should dissapear from to the bank nor
    // any money should be added this is a rule that can be checked when 
    // validating the db
    int balance;
    if (i == 0){
      balance = 10000000;
    } else {
      balance = 10000;
    }
    check = pOp->setValue("BALANCE", balance);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    // TODO - This is how to set a value in a 16, 1 attribute, not so nice?
    // NOTE - its not even possible to set the value 0 in this column
    // since that is equal to NULL when casting to char*
    // check = pOp->setValue("ACCOUNT_TYPE", (const char*)(Uint16)(i/accountTypesSize), 2);
    // NOTE attribute now changed to be a 32 bit

      
    int accountType;
    if (i == 0)
      accountType = 0; // KASSA
    else
      accountType = ((i%accountTypesSize) == 0 ?  1 : (i%getNumAccountTypes()));
    check = pOp->setValue("ACCOUNT_TYPE", accountType);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      m_ndb.closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  }
  check = pTrans->execute(Commit, AbortOnError);
  if( check == -1 ) {
    NDB_ERR(pTrans->getNdbError());
    m_ndb.closeTransaction(pTrans);
    return NDBT_FAILED;
  }
    
  m_ndb.closeTransaction(pTrans);      
  return NDBT_OK;
}


int Bank::getNumAccounts(){
  const NdbDictionary::Table* accountTab = 
    m_ndb.getDictionary()->getTable("ACCOUNT");
  if (accountTab == NULL){
    g_err << "Table ACCOUNT does not exist" << endl;
    return NDBT_FAILED;
  }
  UtilTransactions util(*accountTab);
  if(util.selectCount(&m_ndb, 64, &m_maxAccount) != 0)
    return NDBT_FAILED;    
  return NDBT_OK;
}

int Bank::getMaxAmount(){
  return 10000;
}
