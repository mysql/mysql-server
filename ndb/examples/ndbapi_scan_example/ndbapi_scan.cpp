
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


/*
 * ndbapi_scan.cpp: 
 * Illustrates how to use the scan api in the NDBAPI.
 * The example shows how to do scan, scan for update and scan for delete
 * using NdbScanFilter and NdbScanOperation
 *
 * Classes and methods used in this example:
 *
 *  Ndb
 *       init()
 *       waitUntilRead()
 *       getDictionary()
 *       startTransaction()
 *       closeTransaction()
 *       sendPreparedTransactions()
 *       pollNdb()
 *
 *  NdbConnection
 *       getNdbOperation()
 *       executeAsynchPrepare()
 *       getNdbError()
 *       executeScan()
 *       nextScanResult()
 *
 *  NdbDictionary::Dictionary
 *       getTable()
 *       dropTable()
 *       createTable()
 *
 *  NdbDictionary::Column
 *       setName()
 *       setType()
 *       setLength()
 *       setPrimaryKey()
 *       setNullable()
 *
 *  NdbDictionary::Table
 *       setName()
 *       addColumn()
 *
 *  NdbOperation
 *       insertTuple()
 *       equal()
 *       setValue()
 *       openScanRead()
 *       openScanExclusive()
 *
 *  NdbRecAttr
 *       aRef()
 *       u_32_value()
 *
 *  NdbResultSet
 *       nextResult()
 *       deleteTuple()
 *       updateTuple()
 *
 *  NdbScanOperation
 *       getValue() 
 *       readTuplesExclusive()
 *
 *  NdbScanFilter
 *       begin()
 *	 eq()
 *	 end()
 *
 *       
 */


#include <ndb_global.h>

#include <NdbApi.hpp>
#include <NdbScanFilter.hpp>
// Used for cout
#include <iostream>

/**
 * Helper sleep function
 */
int
milliSleep(int milliseconds){
  int result = 0;
  struct timespec sleeptime;
  sleeptime.tv_sec = milliseconds / 1000;
  sleeptime.tv_nsec = (milliseconds - (sleeptime.tv_sec * 1000)) * 1000000;
  result = nanosleep(&sleeptime, NULL);
  return result;
}


/**
 * Helper sleep function
 */
#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbConnection object, and 
 *   - A pointer to an arbitrary object.)
 */
static void
callback(int result, NdbConnection* myTrans, void* aObject)
{
  if (result == -1) {
    std::cout << "In callback: " << std::endl;     
    /**
     * Put error checking code here (see ndb_async_example)
     */
    APIERROR(myTrans->getNdbError());
  } else {
    /**
     * Ok!
     */
    return;
  }
}

/**
 * Function to create table
 */
int create_table(Ndb * myNdb) 
{
  NdbDictionary::Table myTable;
  NdbDictionary::Column myColumn;
  
  NdbDictionary::Dictionary* myDict = myNdb->getDictionary();
  
  /*********************************************************
   * Create a table named GARAGE if it does not exist *
   *********************************************************/
  if (myDict->getTable("GARAGE") != NULL) {
    std::cout << "NDB already has example table: GARAGE. "
	      << "Dropping it..." << std::endl; 
    if(myDict->dropTable("GARAGE") == -1)
    {
      std::cout << "Failed to drop: GARAGE." << std::endl; 
      exit(1);
    }
  } 

  myTable.setName("GARAGE");
  
  myColumn.setName("REG_NO");
  myColumn.setType(NdbDictionary::Column::Unsigned);
  myColumn.setLength(1);
  myColumn.setPrimaryKey(true);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  myColumn.setName("BRAND");
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(20);
  myColumn.setPrimaryKey(false);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);


  myColumn.setName("COLOR");
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(20);
  myColumn.setPrimaryKey(false);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  if (myDict->createTable(myTable) == -1) {
      APIERROR(myDict->getNdbError());
      return -1;
  }
  return 1;
}


int populate(Ndb * myNdb)
{
  NdbConnection*  myNdbConnection[15];   // For transactions
  NdbOperation*   myNdbOperation;       // For operations
  /******************************************************
   * Insert (we do 15 insert transactions in parallel) *
   ******************************************************/
  /**
   * Five blue mercedes
   */
  for (int i = 0; i < 5; i++) 
  {
    myNdbConnection[i] = myNdb->startTransaction();
    if (myNdbConnection[i] == NULL) 
      APIERROR(myNdb->getNdbError());
    myNdbOperation = myNdbConnection[i]->getNdbOperation("GARAGE");
    // Error check. If error, then maybe table GARAGE is not in database
    if (myNdbOperation == NULL) 
      APIERROR(myNdbConnection[i]->getNdbError());
    myNdbOperation->insertTuple();
    myNdbOperation->equal("REG_NO", i);
    myNdbOperation->setValue("BRAND", "Mercedes");
    myNdbOperation->setValue("COLOR", "Blue");
    // Prepare transaction (the transaction is NOT yet sent to NDB)
    myNdbConnection[i]->executeAsynchPrepare(Commit, &callback, NULL);
  }


  /**
   * Five black bmw
   */
  for (int i = 5; i < 10; i++) 
  {
    myNdbConnection[i] = myNdb->startTransaction();
    if (myNdbConnection[i] == NULL)
      APIERROR(myNdb->getNdbError());
    myNdbOperation = myNdbConnection[i]->getNdbOperation("GARAGE");
    // Error check. If error, then maybe table MYTABLENAME is not in database
    if (myNdbOperation == NULL) 
      APIERROR(myNdbConnection[i]->getNdbError());
    myNdbOperation->insertTuple();
    myNdbOperation->equal("REG_NO", i);
    myNdbOperation->setValue("BRAND", "BMW");
    myNdbOperation->setValue("COLOR", "Black");
    // Prepare transaction (the transaction is NOT yet sent to NDB)
    myNdbConnection[i]->executeAsynchPrepare(Commit, &callback, NULL);
  }

  /**
   * Five pink toyotas
   */
  for (int i = 10; i < 15; i++) {
    myNdbConnection[i] = myNdb->startTransaction();
    if (myNdbConnection[i] == NULL) APIERROR(myNdb->getNdbError());
    myNdbOperation = myNdbConnection[i]->getNdbOperation("GARAGE");
    // Error check. If error, then maybe table MYTABLENAME is not in database
    if (myNdbOperation == NULL) APIERROR(myNdbConnection[i]->getNdbError());
    myNdbOperation->insertTuple();
    myNdbOperation->equal("REG_NO", i);
    myNdbOperation->setValue("BRAND", "Toyota");
    myNdbOperation->setValue("COLOR", "Pink");
    // Prepare transaction (the transaction is NOT yet sent to NDB)
    myNdbConnection[i]->executeAsynchPrepare(Commit, &callback, NULL);
  }

  // Send all transactions to NDB 
  myNdb->sendPreparedTransactions(0);
  // Poll all transactions
  myNdb->pollNdb(3000, 0);

  //  it is also possible to use sendPollNdb instead of
  //  myNdb->sendPreparedTransactions(0); and myNdb->pollNdb(3000, 15);  above.
  //  myNdb->sendPollNdb(3000,0);
  //  Note! Neither sendPollNdb or pollNdb returs until all 15 callbacks have 
  //  executed.

  //  Close all transactions. It is also possible to close transactions
  //  in the callback.
  for (int i = 0; i < 15; i++) 
    myNdb->closeTransaction(myNdbConnection[i]);
  return 1;
}

int scan_delete(Ndb* myNdb, 
		int parallelism,
		int column,
		int column_len,
		const char * color)
		
{
  
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbError              err;
  NdbConnection		*myTrans;
  NdbScanOperation	*myScanOp;

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY erros
   *
   * Exit loop;
   *  retyrMax reached
   *  Permanent error (return -1)
   */
  while (true)
  {
    if (retryAttempt >= retryMax)
    {
      std::cout << "ERROR: has retried this operation " << retryAttempt 
		<< " times, failing!" << std::endl;
      return -1;
    }

    myTrans = myNdb->startTransaction();
    if (myTrans == NULL) 
    {
      const NdbError err = myNdb->getNdbError();

      if (err.status == NdbError::TemporaryError)
      {
	milliSleep(50);
	retryAttempt++;
	continue;
      }
      std::cout <<  err.message << std::endl;
      return -1;
    }

   /**
    * Get a scan operation.
    */
    myScanOp = myTrans->getNdbScanOperation("GARAGE");	
    if (myScanOp == NULL) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }

    /**
     * Define a result set for the scan.
     */ 
    NdbResultSet * rs = myScanOp->readTuplesExclusive(parallelism);
    if( rs == 0 ) {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 

    /**
     * Use NdbScanFilter to define a search critera
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.eq(column, color, column_len, false) <0||
       filter.end() <0)
    {
      std::cout <<  myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }    
    
    /**
     * Start scan    (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NoCommit) != 0){      
      err = myTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	std::cout << myTrans->getNdbError().message << std::endl;
	myNdb->closeTransaction(myTrans);
	milliSleep(50);
	continue;
      }
      std::cout << err.code << std::endl;
      std::cout << myTrans->getNdbError().code << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }


   /**
    * start of loop: nextResult(true) means that "parallelism" number of
    * rows are fetched from NDB and cached in NDBAPI
    */    
    while((check = rs->nextResult(true)) == 0){
      do {
	if (rs->deleteTuple() != 0){
	  std::cout << myTrans->getNdbError().message << std::endl;
	  myNdb->closeTransaction(myTrans);
	  return -1;
	}
	deletedRows++;
	
	/**
	 * nextResult(false) means that the records 
	 * cached in the NDBAPI are modified before
	 * fetching more rows from NDB.
	 */    
      } while((check = rs->nextResult(false)) == 0);
      
      /**
       * Commit when all cached tuple have been marked for deletion
       */    
      if(check != -1){
	check = myTrans->execute(Commit);   
	myTrans->releaseCompletedOperations();
      }
      /**
       * Check for errors
       */
      err = myTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  std::cout << myTrans->getNdbError().message << std::endl;
	  myNdb->closeTransaction(myTrans);
	  milliSleep(50);
	  continue;
	}	
      }
      /**
       * End of loop 
       */
    }
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
    return 0;

    
  }
  if(myTrans!=0) {
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
  }
  return -1;
}


int scan_update(Ndb* myNdb, 
		int parallelism,
		int column_len,
		int update_column,
		const char * column_name,
		const char * before_color,
		const char * after_color)
		
{
  
  // Scan all records exclusive and update
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int updatedRows = 0;
  int check;
  NdbError              err;
  NdbConnection		*myTrans;
  NdbScanOperation	*myScanOp;

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY erros
   *
   * Exit loop;
   *  retyrMax reached
   *  Permanent error (return -1)
   */
  while (true)
  {

    if (retryAttempt >= retryMax)
    {
      std::cout << "ERROR: has retried this operation " << retryAttempt 
		<< " times, failing!" << std::endl;
      return -1;
    }

    myTrans = myNdb->startTransaction();
    if (myTrans == NULL) 
    {
      const NdbError err = myNdb->getNdbError();

      if (err.status == NdbError::TemporaryError)
      {
	milliSleep(50);
	retryAttempt++;
	continue;
      }
      std::cout <<  err.message << std::endl;
      return -1;
    }

   /**
    * Get a scan operation.
    */
    myScanOp = myTrans->getNdbScanOperation("GARAGE");	
    if (myScanOp == NULL) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }

    /**
     * Define a result set for the scan.
     */ 
    NdbResultSet * rs = myScanOp->readTuplesExclusive(parallelism);
    if( rs == 0 ) {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 

    /**
     * Use NdbScanFilter to define a search critera
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.eq(update_column, before_color, column_len, false) <0||
       filter.end() <0)
    {
      std::cout <<  myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }    
    
    /**
     * Start scan    (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NoCommit) != 0){      
      err = myTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	std::cout << myTrans->getNdbError().message << std::endl;
	myNdb->closeTransaction(myTrans);
	milliSleep(50);
	continue;
      }
      std::cout << myTrans->getNdbError().code << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }

   /**
    * Define an update operation
    */    
    NdbOperation * myUpdateOp;
   /**
    * start of loop: nextResult(true) means that "parallelism" number of
    * rows are fetched from NDB and cached in NDBAPI
    */    
    while((check = rs->nextResult(true)) == 0){
      do {
	/**
	 * Get update operation
	 */    
	myUpdateOp = rs->updateTuple();
	if (myUpdateOp == 0){
	  std::cout << myTrans->getNdbError().message << std::endl;
	  myNdb->closeTransaction(myTrans);
	  return -1;
	}
	updatedRows++;
	/**
	 * do the update
	 */    
	myUpdateOp->setValue(update_column,after_color);
	/**
	 * nextResult(false) means that the records 
	 * cached in the NDBAPI are modified before
	 * fetching more rows from NDB.
	 */    
      } while((check = rs->nextResult(false)) == 0);
      
      /**
       * Commit when all cached tuple have been updated
       */    
      if(check != -1){
	check = myTrans->execute(Commit);   
	myTrans->releaseCompletedOperations();
      }
      /**
       * Check for errors
       */
      err = myTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  std::cout << myTrans->getNdbError().message << std::endl;
	  myNdb->closeTransaction(myTrans);
	  milliSleep(50);
	  continue;
	}	
      }
      /**
       * End of loop 
       */
    }
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
    return 0;

    
  }
  if(myTrans!=0) {
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
  }
  return -1;
}



int scan_print(Ndb * myNdb, int parallelism, 
	       int column_len_brand, 
	       int column_len_color) 
{
// Scan all records exclusive and update
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int fetchedRows = 0;
  int check;
  NdbError              err;
  NdbConnection		*myTrans;
  NdbScanOperation	*myScanOp;
  /* Result of reading attribute value, three columns:
     REG_NO, BRAND, and COLOR
   */
  NdbRecAttr *    	myRecAttr[3];   

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY erros
   *
   * Exit loop;
   *  retyrMax reached
   *  Permanent error (return -1)
   */
  while (true)
  {

    if (retryAttempt >= retryMax)
    {
      std::cout << "ERROR: has retried this operation " << retryAttempt 
		<< " times, failing!" << std::endl;
      return -1;
    }

    myTrans = myNdb->startTransaction();
    if (myTrans == NULL) 
    {
      const NdbError err = myNdb->getNdbError();

      if (err.status == NdbError::TemporaryError)
      {
	milliSleep(50);
	retryAttempt++;
	continue;
      }
     std::cout << err.message << std::endl;
      return -1;
    }
    /*
     * Define a scan operation. 
     * NDBAPI.
     */
    myScanOp = myTrans->getNdbScanOperation("GARAGE");	
    if (myScanOp == NULL) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }

    /**
     * Define a result set for the scan.
     */ 
    NdbResultSet * rs = myScanOp->readTuplesExclusive(parallelism);
    if( rs == 0 ) {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 

    /**
     * Define storage for fetched attributes.
     * E.g., the resulting attributes of executing
     * myOp->getValue("REG_NO") is placed in myRecAttr[0].
     * No data exists in myRecAttr until transaction has commited!
     */
    myRecAttr[0] = myScanOp->getValue("REG_NO");
    myRecAttr[1] = myScanOp->getValue("BRAND");
    myRecAttr[2] = myScanOp->getValue("COLOR");
    if(myRecAttr[0] ==NULL || myRecAttr[1] == NULL || myRecAttr[2]==NULL) 
    {
	std::cout << myTrans->getNdbError().message << std::endl;
	myNdb->closeTransaction(myTrans);
	return -1;
    }
    /**
     * Start scan   (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NoCommit) != 0){      
      err = myTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	std::cout << myTrans->getNdbError().message << std::endl;
	myNdb->closeTransaction(myTrans);
	milliSleep(50);
	continue;
      }
      std::cout << err.code << std::endl;
      std::cout << myTrans->getNdbError().code << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }
    
    /**
     * start of loop: nextResult(true) means that "parallelism" number of
     * rows are fetched from NDB and cached in NDBAPI
     */    
    while((check = rs->nextResult(true)) == 0){
      do {
	
	fetchedRows++;
	/**
	 * print  REG_NO unsigned int
	 */
	std::cout << myRecAttr[0]->u_32_value() << "\t";
	char * buf_brand = new char[column_len_brand+1];
	char * buf_color = new char[column_len_color+1];
	/**
	 * print  BRAND character string
	 */
	memcpy(buf_brand, myRecAttr[1]->aRef(), column_len_brand);
	buf_brand[column_len_brand] = 0;
	std::cout << buf_brand << "\t";
	delete [] buf_brand;
	/**
	 * print  COLOR character string
	 */
	memcpy(buf_color, myRecAttr[2]->aRef(), column_len_color);
	buf_brand[column_len_color] = 0;
	std::cout << buf_color << std::endl;
	delete [] buf_color;	
	/**
	 * nextResult(false) means that the records 
	 * cached in the NDBAPI are modified before
	 * fetching more rows from NDB.
	 */    
      } while((check = rs->nextResult(false)) == 0);

    }    
    myNdb->closeTransaction(myTrans);
    return 1;
  }
  return -1;

}


int main()
{
  ndb_init();
  Ndb* myNdb = new Ndb( "TEST_DB" );  // Object representing the database

  
  
  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb->init(1024) == -1) {          // Set max 1024  parallel transactions
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  create_table(myNdb);
  
  NdbDictionary::Dictionary* myDict = myNdb->getDictionary();
  int column_color = myDict->getTable("GARAGE")->getColumn("COLOR")->getColumnNo();
  int column_len_color = 
    myDict->getTable("GARAGE")->getColumn("COLOR")->getLength();
  int column_len_brand = 
    myDict->getTable("GARAGE")->getColumn("BRAND")->getLength();
  int parallelism = 16;
  

  if(populate(myNdb) > 0)
    std::cout << "populate: Success!" << std::endl;

  if(scan_print(myNdb, parallelism, column_len_brand, column_len_color) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  std::cout << "Going to delete all pink cars!" << std::endl;
  if(scan_delete(myNdb, parallelism, column_color,
		 column_len_color, "Pink") > 0)
    std::cout << "scan_delete: Success!" << std::endl  << std::endl;

  if(scan_print(myNdb, parallelism, column_len_brand, column_len_color) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  std::cout << "Going to update all blue cars to black cars!" << std::endl;
  if(scan_update(myNdb, parallelism, column_len_color, column_color, 
		 "COLOR", "Blue", "Black") > 0) 
  {
    std::cout << "scan_update: Success!" << std::endl  << std::endl;
  }
  if(scan_print(myNdb, parallelism, column_len_brand, column_len_color) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;

  delete myNdb; 
}
