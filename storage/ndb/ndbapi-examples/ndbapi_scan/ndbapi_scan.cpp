/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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
 *  NdbScanOperation
 *       getValue() 
 *       readTuples()
 *       nextResult()
 *       deleteCurrentTuple()
 *       updateCurrentTuple()
 *
 *  const NdbDictionary::Dictionary
 *       getTable()
 *
 *  const NdbDictionary::Table
 *       getColumn()
 *
 *  const NdbDictionary::Column
 *       getLength()
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

#include "config.h"

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>
// Used for cout
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <config.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/**
 * Helper sleep function
 */
static void
milliSleep(int milliseconds){
  struct timeval sleeptime;
  sleeptime.tv_sec = milliseconds / 1000;
  sleeptime.tv_usec = (milliseconds - (sleeptime.tv_sec * 1000)) * 1000000;
  select(0, 0, 0, 0, &sleeptime);
}


/**
 * Helper debugging macros
 */
#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }
#define APIERROR(error) { \
  PRINT_ERROR(error.code,error.message); \
  exit(-1); }

struct Car 
{
  /**
   * Note memset, so that entire char-fields are cleared
   *   as all 20 bytes are significant (as type is char)
   */
  Car() { memset(this, 0, sizeof(* this)); }
  
  unsigned int reg_no;
  char brand[20];
  char color[20];
};

/**
 * Function to drop table
 */
void drop_table(MYSQL &mysql)
{
  if (mysql_query(&mysql, "DROP TABLE IF EXISTS api_scan"))
    MYSQLERROR(mysql);
}


/**
 * Function to create table
 */
void create_table(MYSQL &mysql) 
{
  while (mysql_query(&mysql, 
		  "CREATE TABLE"
		  "  api_scan"
		  "    (REG_NO INT UNSIGNED NOT NULL,"
		  "     BRAND CHAR(20) NOT NULL,"
		  "     COLOR CHAR(20) NOT NULL,"
		  "     PRIMARY KEY USING HASH (REG_NO))"
                  "  ENGINE=NDB CHARSET=latin1"))
  {
    if (mysql_errno(&mysql) != ER_TABLE_EXISTS_ERROR)
      MYSQLERROR(mysql);
    std::cout << "MySQL Cluster already has example table: api_scan. "
	      << "Dropping it..." << std::endl; 
    drop_table(mysql);
  }
}

int populate(Ndb * myNdb)
{
  int i;
  Car cars[15];

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_scan");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

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
    NdbOperation* myNdbOperation = myTrans->getNdbOperation(myTable);
    if (myNdbOperation == NULL) 
      APIERROR(myTrans->getNdbError());
    myNdbOperation->insertTuple();
    myNdbOperation->equal("REG_NO", cars[i].reg_no);
    myNdbOperation->setValue("BRAND", cars[i].brand);
    myNdbOperation->setValue("COLOR", cars[i].color);
  }

  int check = myTrans->execute(NdbTransaction::Commit);

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

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_scan");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY errors
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
    myScanOp = myTrans->getNdbScanOperation(myTable);	
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
     * Use NdbScanFilter to define search criteria
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.cmp(NdbScanFilter::COND_EQ, column, color, 20) < 0 ||
       filter.end() < 0)
    {
      std::cout <<  myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }    
    
    /**
     * Start scan    (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NdbTransaction::NoCommit) != 0){      
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
       * NoCommit when all cached tuple have been marked for deletion
       */    
      if(check != -1)
      {
	check = myTrans->execute(NdbTransaction::NoCommit);
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
    /**
     * Commit all prepared operations
     */
    if(myTrans->execute(NdbTransaction::Commit) == -1)
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

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_scan");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY errors
   *
   * Exit loop;
   *  retryMax reached
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
    myScanOp = myTrans->getNdbScanOperation(myTable);	
    if (myScanOp == NULL) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }

    /**
     * Define a result set for the scan.
     */ 
    if( myScanOp->readTuples(NdbOperation::LM_Exclusive) ) 
    {
      std::cout << myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    } 

    /**
     * Use NdbScanFilter to define search criteria
     */ 
    NdbScanFilter filter(myScanOp) ;   
    if(filter.begin(NdbScanFilter::AND) < 0  || 
       filter.cmp(NdbScanFilter::COND_EQ, update_column, before_color, 20) <0||
       filter.end() <0)
    {
      std::cout <<  myTrans->getNdbError().message << std::endl;
      myNdb->closeTransaction(myTrans);
      return -1;
    }    
    
    /**
     * Start scan    (NoCommit since we are only reading at this stage);
     */     
    if(myTrans->execute(NdbTransaction::NoCommit) != 0)
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
	check = myTrans->execute(NdbTransaction::NoCommit);   
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
    if(myTrans->execute(NdbTransaction::Commit) == -1)
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

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_scan");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  /**
   * Loop as long as :
   *  retryMax not reached
   *  failed operations due to TEMPORARY errors
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
    myScanOp = myTrans->getNdbScanOperation(myTable);	
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
     * No data exists in myRecAttr until transaction has committed!
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
    if(myTrans->execute(NdbTransaction::NoCommit) != 0){      
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

void mysql_connect_and_create(MYSQL & mysql, const char *socket)
{
  bool ok;

  ok = mysql_real_connect(&mysql, "localhost", "root", "", "", 0, socket, 0);
  if(ok) {
    mysql_query(&mysql, "CREATE DATABASE ndb_examples");
    ok = ! mysql_select_db(&mysql, "ndb_examples");
  }
  if(ok) {
    create_table(mysql);
  }

  if(! ok) MYSQLERROR(mysql);
}

void ndb_run_scan(const char * connectstring)
{

  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/

  Ndb_cluster_connection cluster_connection(connectstring);
  if (cluster_connection.connect(4, 5, 1))
  {
    std::cout << "Unable to connect to cluster within 30 secs." << std::endl;
    exit(-1);
  }
  // Optionally connect and wait for the storage nodes (ndbd's)
  if (cluster_connection.wait_until_ready(30,0) < 0)
  {
    std::cout << "Cluster was not ready within 30 secs.\n";
    exit(-1);
  }

  Ndb myNdb(&cluster_connection,"ndb_examples");
  if (myNdb.init(1024) == -1) {      // Set max 1024  parallel transactions
    APIERROR(myNdb.getNdbError());
    exit(-1);
  }

  /*******************************************
   * Check table definition                  *
   *******************************************/
  int column_color;
  {
    const NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
    const NdbDictionary::Table *t= myDict->getTable("api_scan");
    if(t == NULL) 
    {
      std::cout << "Dictionary::getTable() failed.";
      exit(-1);
    }
    Car car;
    if (t->getColumn("COLOR")->getLength() != sizeof(car.color) ||
	t->getColumn("BRAND")->getLength() != sizeof(car.brand))
    {
      std::cout << "Wrong table definition" << std::endl;
      exit(-1);
    }
    column_color= t->getColumn("COLOR")->getColumnNo();
  }

  if(populate(&myNdb) > 0)
    std::cout << "populate: Success!" << std::endl;
  
  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  std::cout << "Going to delete all pink cars!" << std::endl;
  
  {
    /**
     * Note! color needs to be of exact the same size as column defined
     */
    Car tmp;
    sprintf(tmp.color, "Pink");
    if(scan_delete(&myNdb, column_color, tmp.color) > 0)
      std::cout << "scan_delete: Success!" << std::endl  << std::endl;
  }

  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
  
  {
    /**
     * Note! color1 & 2 need to be of exact the same size as column defined
     */
    Car tmp1, tmp2;
    sprintf(tmp1.color, "Blue");
    sprintf(tmp2.color, "Black");
    std::cout << "Going to update all " << tmp1.color 
	      << " cars to " << tmp2.color << " cars!" << std::endl;
    if(scan_update(&myNdb, column_color, tmp1.color, tmp2.color) > 0) 
      std::cout << "scan_update: Success!" << std::endl  << std::endl;
  }
  if(scan_print(&myNdb) > 0)
    std::cout << "scan_print: Success!" << std::endl  << std::endl;
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cout << "Arguments are <socket mysqld> <connect_string cluster>.\n";
    exit(-1);
  }
  char * mysqld_sock  = argv[1];
  const char *connectstring = argv[2];
  MYSQL mysql;

  mysql_init(& mysql);
  mysql_connect_and_create(mysql, mysqld_sock);

  ndb_init();
  ndb_run_scan(connectstring);
  ndb_end(0);

  mysql_close(&mysql);

  return 0;
}
