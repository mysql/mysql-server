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

#include <NdbSleep.h>
#include <HugoAsynchTransactions.hpp>

HugoAsynchTransactions::HugoAsynchTransactions(const NdbDictionary::Table& _tab):
  HugoTransactions(_tab),
  transactionsCompleted(0),
  numTransactions(0),
  transactions(NULL){
}

HugoAsynchTransactions::~HugoAsynchTransactions(){
  deallocTransactions();
}

void asynchCallback(int result, NdbConnection* pTrans, 
		    void* anObject) {
  HugoAsynchTransactions* pHugo = (HugoAsynchTransactions*) anObject;
  
  pHugo->transactionCompleted();

  if (result == -1) {
    const NdbError err = pTrans->getNdbError();
    switch(err.status) {
    case NdbError::Success:
      ERR(err);
      g_info << "ERROR: NdbError reports success when transcaction failed"
	     << endl;
      break;
      
    case NdbError::TemporaryError:      
      ERR(err);
      break;

#if 0      
    case 626: // Tuple did not exist
      g_info << (unsigned int)pHugo->getTransactionsCompleted() << ": " 
	     << err.code << " " << err.message << endl;
      break;
#endif
 
    case NdbError::UnknownResult:
      ERR(err);
      break;
      
    case NdbError::PermanentError:
      switch (err.classification) {
      case NdbError::ConstraintViolation:
	// Tuple already existed, OK in this application, 
	// but should be reported
	g_info << (unsigned int)pHugo->getTransactionsCompleted() 
	       << ": " << err.code << " " << err.message << endl;
	break;
      default:
	ERR(err);
	break;
      }
      break;
    }
  } else {// if (result == -1)
    /*
    ndbout << (unsigned int)pHugo->getTransactionsCompleted() << " completed" 
	   << endl;
    */
  }
}

int
HugoAsynchTransactions::loadTableAsynch(Ndb* pNdb, 
				  int records,
				  int batch,
				  int trans,
				  int operations){

  int result = executeAsynchOperation(pNdb, records, batch, trans, operations, 
				      NO_INSERT);
  g_info << (unsigned int)transactionsCompleted * operations 
	 << "|- inserted..." << endl;

  return result;
} 

void
HugoAsynchTransactions::transactionCompleted() {
  transactionsCompleted++;
}

long
HugoAsynchTransactions::getTransactionsCompleted() {
  return transactionsCompleted;
}

int 
HugoAsynchTransactions::pkDelRecordsAsynch(Ndb* pNdb, 
				     int records,
				     int batch,
				     int trans,
				     int operations) {
  
  g_info << "|- Deleting records asynchronous..." << endl;

  int result =  executeAsynchOperation(pNdb, records, batch, trans, 
				       operations, 
				       NO_DELETE);
  g_info << "|- " << (unsigned int)transactionsCompleted * operations 
	 << " deleted..." << endl;

  return result;  
}

int 
HugoAsynchTransactions::pkReadRecordsAsynch(Ndb* pNdb, 
				      int records,
				      int batch,
				      int trans,
				      int operations) {

  g_info << "|- Reading records asynchronous..." << endl;

  allocRows(trans*operations);
  int result = executeAsynchOperation(pNdb, records, batch, trans, operations, 
				      NO_READ);

  g_info << "|- " << (unsigned int)transactionsCompleted * operations 
	 << " read..."
	 << endl;

  deallocRows();

  return result;
}

int 
HugoAsynchTransactions::pkUpdateRecordsAsynch(Ndb* pNdb, 
					int records,
					int batch,
					int trans,
					int operations) {

  g_info << "|- Updating records asynchronous..." << endl;

  int             check = 0;
  int             cTrans = 0;
  int             cReadRecords = 0;
  int             cReadIndex = 0;
  int             cRecords = 0;
  int             cIndex = 0;

  transactionsCompleted = 0;

  allocRows(trans*operations);
  allocTransactions(trans);
  int a, t, r;

  for (int i = 0; i < batch; i++) { // For each batch
    while (cRecords < records*batch) {
      cTrans = 0;
      cReadIndex = 0;
      for (t = 0; t < trans; t++) { // For each transaction
	transactions[t] = pNdb->startTransaction();
	if (transactions[t] == NULL) {
	  ERR(pNdb->getNdbError());
	  return NDBT_FAILED;
	}	
	for (int k = 0; k < operations; k++) { // For each operation
	  NdbOperation* pOp = transactions[t]->getNdbOperation(tab.getName());
	  if (pOp == NULL) { 
	    ERR(transactions[t]->getNdbError());
	    pNdb->closeTransaction(transactions[t]);
	    return NDBT_FAILED;
	  }
	  
	  // Read
	  // Define primary keys
	  check = pOp->readTupleExclusive();
	  for (a = 0; a < tab.getNoOfColumns(); a++) {
	    if (tab.getColumn(a)->getPrimaryKey() == true) {
	      if (equalForAttr(pOp, a, cReadRecords) != 0){
		ERR(transactions[t]->getNdbError());
		pNdb->closeTransaction(transactions[t]);
		return NDBT_FAILED;
	      }
	    }
	  }	    
	  // Define attributes to read  
	  for (a = 0; a < tab.getNoOfColumns(); a++) {
	    if ((rows[cReadIndex]->attributeStore(a) = 
		 pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	      ERR(transactions[t]->getNdbError());
	      pNdb->closeTransaction(transactions[t]);
	      return NDBT_FAILED;
	    }
	  }	    	  
	  cReadIndex++;
	  cReadRecords++;
	  
	} // For each operation
	
	// Let's prepare...
	transactions[t]->executeAsynchPrepare(NoCommit, &asynchCallback, 
					this);
	cTrans++;

	if (cReadRecords >= records) {
	  // No more transactions needed
	  break;
	}      
      } // For each transaction

      // Wait for all outstanding transactions
      pNdb->sendPollNdb(3000, 0, 0);

      // Verify the data!
      for (r = 0; r < trans*operations; r++) {
	if (calc.verifyRowValues(rows[r]) != 0) {
	  g_info << "|- Verify failed..." << endl;
	  // Close all transactions
	  for (int t = 0; t < cTrans; t++) {
	    pNdb->closeTransaction(transactions[t]);
	  }
	  return NDBT_FAILED;
	}
      }	

      // Update
      cTrans = 0;
      cIndex = 0;
      for (t = 0; t < trans; t++) { // For each transaction
	for (int k = 0; k < operations; k++) { // For each operation
	  NdbOperation* pOp = transactions[t]->getNdbOperation(tab.getName());
	  if (pOp == NULL) { 
	    ERR(transactions[t]->getNdbError());
	    pNdb->closeTransaction(transactions[t]);
	    return NDBT_FAILED;
	  }
	  
	  int updates = calc.getUpdatesValue(rows[cIndex]) + 1;

	  check = pOp->updateTuple();
	  if (check == -1) {
	    ERR(transactions[t]->getNdbError());
	    pNdb->closeTransaction(transactions[t]);
	      return NDBT_FAILED;
	  }

	  // Set search condition for the record
	  for (a = 0; a < tab.getNoOfColumns(); a++) {
	    if (tab.getColumn(a)->getPrimaryKey() == true) {
	      if (equalForAttr(pOp, a, cRecords) != 0) {
		ERR(transactions[t]->getNdbError());
		pNdb->closeTransaction(transactions[t]);
		return NDBT_FAILED;
	      }
	    }
	  }

	  // Update the record
	  for (a = 0; a < tab.getNoOfColumns(); a++) {
	    if (tab.getColumn(a)->getPrimaryKey() == false) {
	      if (setValueForAttr(pOp, a, cRecords, updates) != 0) {
		ERR(transactions[t]->getNdbError());
		pNdb->closeTransaction(transactions[t]);
		return NDBT_FAILED;
	      }
	    }
	  }	  
	  cIndex++;
	  cRecords++;
	  
	} // For each operation
	
	// Let's prepare...
	transactions[t]->executeAsynchPrepare(Commit, &asynchCallback, 
					this);
	cTrans++;

	if (cRecords >= records) {
	  // No more transactions needed
	  break;
	}      
      } // For each transaction

      // Wait for all outstanding transactions
      pNdb->sendPollNdb(3000, 0, 0);

      // Close all transactions
      for (t = 0; t < cTrans; t++) {
	pNdb->closeTransaction(transactions[t]);
      }

    } // while (cRecords < records*batch)

  } // For each batch

  deallocTransactions();
  deallocRows();
  
  g_info << "|- " << ((unsigned int)transactionsCompleted * operations)/2 
	 << " updated..." << endl;
  return NDBT_OK;
}

void 
HugoAsynchTransactions::allocTransactions(int trans) {
  if (transactions != NULL) {
    deallocTransactions(); 
  }
  numTransactions = trans;
  transactions = new NdbConnection*[numTransactions];  
}

void 
HugoAsynchTransactions::deallocTransactions() {
  if (transactions != NULL){
    delete[] transactions;
  }
  transactions = NULL;
}

int 
HugoAsynchTransactions::executeAsynchOperation(Ndb* pNdb,		      
					 int records,
					 int batch,
					 int trans,
					 int operations,
					 NDB_OPERATION theOperation,
					 ExecType theType) {

  int             check = 0;
  //  int             retryAttempt = 0;  // Not used at the moment
  //  int             retryMax = 5;      // Not used at the moment
  int             cTrans = 0;
  int             cRecords = 0;
  int             cIndex = 0;
  int a,t,r;

  transactionsCompleted = 0;
  allocTransactions(trans);

  for (int i = 0; i < batch; i++) { // For each batch
    while (cRecords < records*batch) {
      cTrans = 0;
      cIndex = 0;
      for (t = 0; t < trans; t++) { // For each transaction
	transactions[t] = pNdb->startTransaction();
	if (transactions[t] == NULL) {
	  ERR(pNdb->getNdbError());
	  return NDBT_FAILED;
	}	
	for (int k = 0; k < operations; k++) { // For each operation
	  NdbOperation* pOp = transactions[t]->getNdbOperation(tab.getName());
	  if (pOp == NULL) { 
	    ERR(transactions[t]->getNdbError());
	    pNdb->closeTransaction(transactions[t]);
	    return NDBT_FAILED;
	  }
	  
	  switch (theOperation) {
	  case NO_INSERT: 
	    // Insert
	    check = pOp->insertTuple();
	    if (check == -1) { 
	      ERR(transactions[t]->getNdbError());
	      pNdb->closeTransaction(transactions[t]);
	      return NDBT_FAILED;
	    }
	    
	    // Set a calculated value for each attribute in this table	 
	    for (a = 0; a < tab.getNoOfColumns(); a++) {
	      if (setValueForAttr(pOp, a, cRecords, 0 ) != 0) {	  
		ERR(transactions[t]->getNdbError());
		pNdb->closeTransaction(transactions[t]);	  
		return NDBT_FAILED;
	      }
	    } // For each attribute
	    break;
	  case NO_UPDATE:
	    // This is a special case and is handled in the calling client...
	    break;
	  break;
	  case NO_READ:
	    // Define primary keys
	    check = pOp->readTuple();
	    for (a = 0; a < tab.getNoOfColumns(); a++) {
	      if (tab.getColumn(a)->getPrimaryKey() == true) {
		if (equalForAttr(pOp, a, cRecords) != 0){
		  ERR(transactions[t]->getNdbError());
		  pNdb->closeTransaction(transactions[t]);
		  return NDBT_FAILED;
		}
	      }
	    }	    
	    // Define attributes to read  
	    for (a = 0; a < tab.getNoOfColumns(); a++) {
	      if ((rows[cIndex]->attributeStore(a) = 
		   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
		ERR(transactions[t]->getNdbError());
		pNdb->closeTransaction(transactions[t]);
		return NDBT_FAILED;
	      }
	    }	    	  
	    break;
	  case NO_DELETE:
	    // Delete
	    check = pOp->deleteTuple();
	    if (check == -1) { 
	      ERR(transactions[t]->getNdbError());
	      pNdb->closeTransaction(transactions[t]);
	      return NDBT_FAILED;
	    }

	    // Define primary keys
	    for (a = 0; a < tab.getNoOfColumns(); a++) {
	      if (tab.getColumn(a)->getPrimaryKey() == true){
		if (equalForAttr(pOp, a, cRecords) != 0) {
		  ERR(transactions[t]->getNdbError());
		  pNdb->closeTransaction(transactions[t]);		
		  return NDBT_FAILED;
		}
	      }
	    }
	    break;
	  default:
	    // Should not happen...
	    pNdb->closeTransaction(transactions[t]);		
	    return NDBT_FAILED;
	  }

	  cIndex++;
	  cRecords++;

	} // For each operation
    
	// Let's prepare...
	transactions[t]->executeAsynchPrepare(theType, &asynchCallback, 
					this);
	cTrans++;

	if (cRecords >= records) {
	  // No more transactions needed
	  break;
	}      
      } // For each transaction

      // Wait for all outstanding transactions
      pNdb->sendPollNdb(3000, 0, 0);

      // ugly... it's starts to resemble flexXXX ...:(
      switch (theOperation) {
      case NO_READ:
	// Verify the data!
	for (r = 0; r < trans*operations; r++) {
	  if (calc.verifyRowValues(rows[r]) != 0) {
	    g_info << "|- Verify failed..." << endl;
	    // Close all transactions
	    for (int t = 0; t < cTrans; t++) {
	      pNdb->closeTransaction(transactions[t]);
	    }
	    return NDBT_FAILED;
	  }
	}	
	break;
      case NO_INSERT:
      case NO_UPDATE:
      case NO_DELETE:
	break;
      }

      // Close all transactions
      for (t = 0; t < cTrans; t++) {
	pNdb->closeTransaction(transactions[t]);
      }

    } // while (cRecords < records*batch)

  } // For each batch

  deallocTransactions();

  return NDBT_OK;

}
