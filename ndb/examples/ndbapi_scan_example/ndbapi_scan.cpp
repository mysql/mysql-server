
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
 *  Ndb_cluster_connection
 *       connect()
 *       wait_until_ready()
 *
 *  Ndb
 *       init()
 *       getDictionary()
 *       startTransaction()
 *       closeTransaction()
 *
 *  NdbTransaction
 *       getNdbScanOperation()
 *       execute()
 *
 *  NdbResultSet
 *
 *  NdbScanOperation
 *       getValue() 
 *       readTuples()
 *       nextResult()
 *       deleteCurrentTuple()
 *       updateCurrentTuple()
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
 *
 *  NdbScanFilter
 *       begin()
 *	 eq()
 *	 end()
 *
 */


#include <NdbApi.hpp>
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

struct Car 
{
  unsigned int reg_no;
  char brand[20];
  char color[20];
};

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
  
  Car car;

  myTable.setName("GARAGE");
  
  myColumn.setName("REG_NO");
  myColumn.setType(NdbDictionary::Column::Unsigned);
  myColumn.setLength(1);
  myColumn.setPrimaryKey(true);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  myColumn.setName("BRAND");
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(sizeof(car.brand));
  myColumn.setPrimaryKey(false);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);


  myColumn.setName("COLOR");
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(sizeof(car.color));
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
  int i;
  Car cars[15];

  /**
   * Five blue mercedes
   */
  for (i = 0; i < 5; i++)
  {
    cars[i].reg_no = i;
    sprintf(cars[i].brand, "Mercedes");
    sprintf(cars[i].color, "Blue");
  }

  /**
   * Five black bmw
   */
  for (i = 5; i < 10; i++)
  {
    cars[i].reg_no = i;
    sprintf(cars[i].brand, "BMW");
    sprintf(cars[i].color, "Black");
  }

  /**
   * Five pink toyotas
   */
  for (i = 10; i < 15; i++)
  {
    cars[i].reg_no = i;
    sprintf(cars[i].brand, "Toyota");
    sprintf(cars[i].color, "Pink");
  }
  
  NdbTransaction* myTrans = myNdb->startTransaction();
  if (myTrans == NULL)
    APIERROR(myNdb->getNdbError());

  for (i = 0; i < 15; i++) 
  {
    NdbOperation* myNdbOperation = myTrans->getNdbOperation("GARAGE");
    // Error check. If error, then maybe table MYTABLENAME is not in database
    if (myNdbOperation == NULL) 
      APIERROR(myTrans->getNdbError());
    myNdbOperation->insertTuple();
    myNdbOperation->equal("REG_NO", cars[i].reg_no);
    myNdbOperation->setValue("BRAND", cars[i].brand);
    myNdbOperation->setValue("COLOR", cars[i].color);
  }

  int check = myTrans->execute(Commit);

  myTrans->close();

  return check != -1;
}

int scan_delete(Ndb* myNdb, 
		int column,
		const char * color)
  
{
  
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbError              err;
  NdbTransaction	*myTrans;
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
    if(myScanOp->readTuples(NdbOperation::LM_Exclusive) != 0)
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 
    
    /**
     * Use NdbScanFilter to define a search critera
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.cmp(NdbScanFilter::COND_EQ, column, color) < 0 ||
       filter.end() < 0)
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
    while((check = myScanOp->nextResult(true)) == 0){
      do 
      {
	if (myScanOp->deleteCurrentTuple() != 0)
	{
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
      } while((check = myScanOp->nextResult(false)) == 0);
      
      /**
       * Commit when all cached tuple have been marked for deletion
       */    
      if(check != -1)
      {
	check = myTrans->execute(Commit);   
      }

      if(check == -1)
      {
	/**
	 * Create a new transaction, while keeping scan open
	 */
	check = myTrans->restart();
      }

      /**
       * Check for errors
       */
      err = myTrans->getNdbError();    
      if(check == -1)
      {
	if(err.status == NdbError::TemporaryError)
	{
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
  
  if(myTrans!=0) 
  {
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
  }
  return -1;
}


int scan_update(Ndb* myNdb, 
		int update_column,
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
  NdbTransaction	*myTrans;
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
    if( myScanOp->readTuplesExclusive(NdbOperation::LM_Exclusive) ) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 

    /**
     * Use NdbScanFilter to define a search critera
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.cmp(NdbScanFilter::COND_EQ, update_column, before_color) <0||
       filter.end() <0)
    {
      std::cout <<  myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }    
    
    /**
     * Start scan    (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NoCommit) != 0)
    {      
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
     * start of loop: nextResult(true) means that "parallelism" number of
     * rows are fetched from NDB and cached in NDBAPI
     */    
    while((check = myScanOp->nextResult(true)) == 0){
      do {
	/**
	 * Get update operation
	 */    
	NdbOperation * myUpdateOp = myScanOp->updateCurrentTuple();
	if (myUpdateOp == 0)
	{
	  std::cout << myTrans->getNdbError().message << std::endl;
	  myNdb->closeTransaction(myTrans);
	  return -1;
	}
	updatedRows++;

	/**
	 * do the update
	 */    
	myUpdateOp->setValue(update_column, after_color);
	/**
	 * nextResult(false) means that the records 
	 * cached in the NDBAPI are modified before
	 * fetching more rows from NDB.
	 */    
      } while((check = myScanOp->nextResult(false)) == 0);
      
      /**
       * NoCommit when all cached tuple have been updated
       */    
      if(check != -1)
      {
	check = myTrans->execute(NoCommit);   
      }

      /**
       * Check for errors
       */
      err = myTrans->getNdbError();    
      if(check == -1)
      {
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

    /**
     * Commit all prepared operations
     */
    if(myTrans->execute(Commit) == -1)
    {
      if(err.status == NdbError::TemporaryError){
	std::cout << myTrans->getNdbError().message << std::endl;
	myNdb->closeTransaction(myTrans);
	milliSleep(50);
	continue;
      }	
    }

    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
    return 0;    
  }


  if(myTrans!=0) 
  {
    std::cout << myTrans->getNdbError().message << std::endl;
    myNdb->closeTransaction(myTrans);
  }
  return -1;
}



int scan_print(Ndb * myNdb)
{
// Scan all records exclusive and update
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int fetchedRows = 0;
  int check;
  NdbError              err;
  NdbTransaction	*myTrans;
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
     * Read without locks, without being placed in lock queue
     */
    if( myScanOp->readTuples(NdbOperation::LM_CommittedRead) == -1)
    {
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
    while((check = myScanOp->nextResult(true)) == 0){
      do {
	
	fetchedRows++;
	/**
	 * print  REG_NO unsigned int
	 */
	std::cout << myRecAttr[0]->u_32_value() << "\t";

	/**
	 * print  BRAND character string
	 */
	std::cout << myRecAttr[1]->aRef() << "\t";

	/**
	 * print  COLOR character string
	 */
	std::cout << myRecAttr[2]->aRef() << std::endl;

	/**
	 * nextResult(false) means that the records 
	 * cached in the NDBAPI are modified before
	 * fetching more rows from NDB.
	 */    
      } while((check = myScanOp->nextResult(false)) == 0);

    }    
    myNdb->closeTransaction(myTrans);
    return 1;
  }
  return -1;

}


int main()
{
  ndb_init();

  Ndb_cluster_connection cluster_connection;

  if (cluster_connection.connect(12, 5, 1))
  {
    std::cout << "Unable to connect to cluster within 30 secs." << std::endl;
    exit(-1);
  }

  if (cluster_connection.wait_until_ready(30,30))
  {
    std::cout << "Cluster was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  
  Ndb myNdb(&cluster_connection,"TEST_DB" );  
  
  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb.init(1024) == -1) {          // Set max 1024  parallel transactions
    APIERROR(myNdb.getNdbError());
    exit(-1);
  }

  create_table(&myNdb);
  
  NdbDictionary::Dictionary* myDict = myNdb.getDictionary();
  int column_color = myDict->getTable("GARAGE")->getColumn("COLOR")->getColumnNo();
  
  if(populate(&myNdb) > 0)
    std::cout << "populate: Success!" << std::endl;
  
  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  std::cout << "Going to delete all pink cars!" << std::endl;
  
  {
    /**
     * Note! color needs to be of exact the same size as column defined
     */
    char color[20] = "Pink";
    if(scan_delete(&myNdb, column_color, color) > 0)
      std::cout << "scan_delete: Success!" << std::endl  << std::endl;
  }

  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  {
    /**
     * Note! color1 & 2 need to be of exact the same size as column defined
     */
    char color1[20] = "Blue";
    char color2[20] = "Black";
    std::cout << "Going to update all " << color1 
	      << " cars to " << color2 << " cars!" << std::endl;
    if(scan_update(&myNdb, column_color, color1, color2) > 0) 
      std::cout << "scan_update: Success!" << std::endl  << std::endl;
  }
  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
}
