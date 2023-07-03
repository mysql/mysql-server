/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

#include "consumer_restore.hpp"
#include <NdbSleep.h>

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static bool asynchErrorHandler(NdbTransaction * trans, Ndb * ndb);
static void callback(int result, NdbTransaction* trans, void* aObject);

bool
BackupRestore::init()
{

  if (!m_restore && !m_restore_meta)
    return true;

  m_ndb = new Ndb();

  if (m_ndb == NULL)
    return false;
  
  // Turn off table name completion
  m_ndb->useFullyQualifiedNames(false);

  m_ndb->init(1024);
  if (m_ndb->waitUntilReady(30) != 0)
  {
    ndbout << "Failed to connect to ndb!!" << endl;
    return false;
  }
  ndbout << "Connected to ndb!!" << endl;

#if USE_MYSQL
  if(use_mysql) 
  {
    if ( mysql_thread_safe() == 0 ) 
    {
      ndbout << "Not thread safe mysql library..." << endl;
      exit(-1);
    }
    
    ndbout << "Connecting to MySQL..." <<endl;
    
    /**
     * nwe param:
     *  port
     *  host
     *  user
     */
    bool returnValue = true;
    mysql_init(&mysql);
    {
      int portNo = 3306;
      if ( mysql_real_connect(&mysql,
			      ga_host,
			      ga_user,
			      ga_password,
			      ga_database,
			      ga_port,
::			      ga_socket,
			      0) == NULL ) 
      {
	ndbout_c("Connect failed: %s", mysql_error(&mysql));
	returnValue = false;
      }
      mysql.reconnect= 1;
      ndbout << "Connected to MySQL!!!" <<endl;
    }

    /*  if(returnValue){
	mysql_set_server_option(&mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON);
	}
    */
    return returnValue;
  }
#endif

  if (m_callback) {
    delete [] m_callback;
    m_callback = 0;
  }

  m_callback = new restore_callback_t[m_parallelism];

  if (m_callback == 0)
  {
    ndbout << "Failed to allocate callback structs" << endl;
    return false;
  }

  m_free_callback = m_callback;
  for (int i= 0; i < m_parallelism; i++) {
    m_callback[i].restore = this;
    m_callback[i].connection = 0;
    m_callback[i].retries = 0;
    if (i > 0)
      m_callback[i-1].next = &(m_callback[i]);
  }
  m_callback[m_parallelism-1].next = 0;

  return true;
  
}

BackupRestore::~BackupRestore()
{
  if (m_ndb != 0)
    delete m_ndb;

  if (m_callback)
    delete [] m_callback;
}

#ifdef USE_MYSQL
bool
BackupRestore::table(const TableS & table, MYSQL * mysqlp){
  if (!m_restore_meta) 
  {
    return true;
  }
    
  char tmpTabName[MAX_TAB_NAME_SIZE*2];
  sprintf(tmpTabName, "%s", table.getTableName());
  char * database = strtok(tmpTabName, "/");
  char * schema   = strtok( NULL , "/");
  char * tableName    = strtok( NULL , "/");

  /**
   * this means that the user did not specify schema
   * and it is a v2x backup
   */
  if(database == NULL)
    return false;
  if(schema == NULL)
    return false;
  if(tableName==NULL)
    tableName = schema; 
  
  char stmtCreateDB[255];
  sprintf(stmtCreateDB,"CREATE DATABASE %s", database);
  
  /*ignore return value. mysql_select_db will trap errors anyways*/
  if (mysql_query(mysqlp,stmtCreateDB) == 0)
  {
    //ndbout_c("%s", stmtCreateDB);
  }

  if (mysql_select_db(&mysql, database) != 0) 
  {
    ndbout_c("Error: %s", mysql_error(&mysql));
    return false;
  }
  
  char buf [2048];
  /**
   * create table ddl
   */
  if (create_table_string(table, tableName,  buf)) 
  {
    ndbout_c("Unable to create a table definition since the "
	     "backup contains undefined types");
    return false;
  }

  //ndbout_c("%s", buf);
  
  if (mysql_query(mysqlp,buf) != 0) 
  {
      ndbout_c("Error: %s", mysql_error(&mysql));
      return false;
  } else 
  {
    ndbout_c("Successfully restored table %s into database %s", tableName, database);
  }
  
  return true;
}
#endif

bool
BackupRestore::table(const TableS & table){
  if (!m_restore_meta) 
  {
    return true;
  }
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if (dict->createTable(*table.m_dictTable) == -1) 
  {
    err << "Create table " << table.getTableName() << " failed: "
	<< dict->getNdbError() << endl;
    return false;
  }
  info << "Successfully restored table " << table.getTableName()<< endl ;
  return true;
}

void BackupRestore::tuple(const TupleS & tup, Uint32 fragId)
{
  if (!m_restore) 
  {
    delete &tup;
    return;  
  }

  restore_callback_t * cb = m_free_callback;

  if (cb)
  {
    m_free_callback = cb->next;
    cb->retries = 0;
    cb->fragId = fragId;
    cb->tup = &tup;
    tuple_a(cb);
  }

  if (m_free_callback == 0)
  {
    // send-poll all transactions
    // close transaction is done in callback
    m_ndb->sendPollNdb(3000, 1);
  }
}

void BackupRestore::tuple_a(restore_callback_t *cb)
{
  while (cb->retries < 10) 
  {
    /**
     * start transactions
     */
    cb->connection = m_ndb->startTransaction();
    if (cb->connection == NULL) 
    {
      /*
	if (asynchErrorHandler(cb->connection, m_ndb)) 
	{
	cb->retries++;
	continue;
	}
      */
      asynchExitHandler();
    } // if
    
    const TupleS &tup = *(cb->tup);
    const TableS * table = tup.getTable();
    NdbOperation * op = cb->connection->getNdbOperation(table->getTableName());
    
    if (op == NULL) 
    {
      if (asynchErrorHandler(cb->connection, m_ndb)) 
      {
	cb->retries++;
	continue;
      }
      asynchExitHandler();
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (asynchErrorHandler(cb->connection, m_ndb))
      {
	cb->retries++;
	continue;
      }
      asynchExitHandler();
    } // if
    
    Uint32 ret = 0;
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      char * dataPtr = attr->Data.string_value;
      Uint32 length = (size * arraySize) / 8;
      if (attr->Desc->m_column->getPrimaryKey()) 
      {
	ret = op->equal(i, dataPtr, length);
      }
      else
      {
	if (attr->Data.null) 
	  ret = op->setValue(i, NULL, 0);
	else
	  ret = op->setValue(i, dataPtr, length);
      }

      if (ret<0) 
	{
	  ndbout_c("Column: %d type %d",i,
		   tup.getTable()->m_dictTable->getColumn(i)->getType());
	  if (asynchErrorHandler(cb->connection, m_ndb)) 
	    {
	      cb->retries++;
	      break;
	    }
	  asynchExitHandler();
	}
    }
    if (ret < 0)
      continue;

    // Prepare transaction (the transaction is NOT yet sent to NDB)
    cb->connection->executeAsynchPrepare(Commit, &callback, cb);
    m_transactions++;
  }
  ndbout_c("Unable to recover from errors. Exiting...");
  asynchExitHandler();
}

void BackupRestore::cback(int result, restore_callback_t *cb)
{
  if (result<0)
  {
    /**
       * Error. temporary or permanent?
       */
    if (asynchErrorHandler(cb->connection, m_ndb)) 
    {
      cb->retries++;
      tuple_a(cb);
    }
    else
    {
      ndbout_c("Restore: Failed to restore data "
	       "due to a unrecoverable error. Exiting...");
      delete m_ndb;
      delete cb->tup;
      exit(-1);
    }
  }
  else 
  {
    /**
     * OK! close transaction
     */
    m_ndb->closeTransaction(cb->connection);
    delete cb->tup;
    m_transactions--;
  }
}

void BackupRestore::asynchExitHandler() 
{
  if (m_ndb != NULL)
    delete m_ndb;
  exit(-1);
}

#if 0 // old tuple impl
void
BackupRestore::tuple(const TupleS & tup)
{
  if (!m_restore)
    return;
  while (1) 
  {
    NdbTransaction * trans = m_ndb->startTransaction();
    if (trans == NULL) 
    {
      // TODO: handle the error
      ndbout << "Cannot start transaction" << endl;
      exit(-1);
    } // if
    
    const TableS * table = tup.getTable();
    NdbOperation * op = trans->getNdbOperation(table->getTableName());
    if (op == NULL) 
    {
      ndbout << "Cannot get operation: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    } // if
    
    // TODO: check return value and handle error
    if (op->writeTuple() == -1) 
    {
      ndbout << "writeTuple call failed: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    } // if
    
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      const char * dataPtr = attr->Data.string_value;
      
      const Uint32 length = (size * arraySize) / 8;
      if (attr->Desc->m_column->getPrimaryKey()) 
	op->equal(i, dataPtr, length);
    }
    
    for (int i = 0; i < tup.getNoOfAttributes(); i++) 
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      const char * dataPtr = attr->Data.string_value;
      
      const Uint32 length = (size * arraySize) / 8;
      if (!attr->Desc->m_column->getPrimaryKey())
	if (attr->Data.null)
	  op->setValue(i, NULL, 0);
	else
	  op->setValue(i, dataPtr, length);
    }
    int ret = trans->execute(Commit);
    if (ret != 0)
    {
      ndbout << "execute failed: ";
      ndbout << trans->getNdbError() << endl;
      exit(-1);
    }
    m_ndb->closeTransaction(trans);
    if (ret == 0)
      break;
  }
  m_dataCount++;
}
#endif

void
BackupRestore::endOfTuples()
{
  if (!m_restore)
    return;

  // Send all transactions to NDB 
  m_ndb->sendPreparedTransactions(0);

  // Poll all transactions
  m_ndb->pollNdb(3000, m_transactions);

  // Close all transactions
  //  for (int i = 0; i < nPreparedTransactions; i++) 
  // m_ndb->closeTransaction(asynchTrans[i]);
}

void
BackupRestore::logEntry(const LogEntry & tup)
{
  if (!m_restore)
    return;

  NdbTransaction * trans = m_ndb->startTransaction();
  if (trans == NULL) 
  {
    // TODO: handle the error
    ndbout << "Cannot start transaction" << endl;
    exit(-1);
  } // if
  
  const TableS * table = tup.m_table;
  NdbOperation * op = trans->getNdbOperation(table->getTableName());
  if (op == NULL) 
  {
    ndbout << "Cannot get operation: ";
    ndbout << trans->getNdbError() << endl;
    exit(-1);
  } // if
  
  int check = 0;
  switch(tup.m_type)
  {
  case LogEntry::LE_INSERT:
    check = op->insertTuple();
    break;
  case LogEntry::LE_UPDATE:
    check = op->updateTuple();
    break;
  case LogEntry::LE_DELETE:
    check = op->deleteTuple();
    break;
  default:
    ndbout << "Log entry has wrong operation type."
	   << " Exiting...";
    exit(-1);
  }
  
  for (int i = 0; i < tup.m_values.size(); i++) 
  {
    const AttributeS * attr = tup.m_values[i];
    int size = attr->Desc->size;
    int arraySize = attr->Desc->arraySize;
    const char * dataPtr = attr->Data.string_value;
    
    const Uint32 length = (size / 8) * arraySize;
    if (attr->Desc->m_column->getPrimaryKey()) 
      op->equal(attr->Desc->attrId, dataPtr, length);
    else
      op->setValue(attr->Desc->attrId, dataPtr, length);
  }
  
#if 1
  trans->execute(Commit);
#else
  const int ret = trans->execute(Commit);
  // Both insert update and delete can fail during log running
  // and it's ok
  
  if (ret != 0)
  {
    ndbout << "execute failed: ";
    ndbout << trans->getNdbError() << endl;
    exit(-1);
  }
#endif
  
  m_ndb->closeTransaction(trans);
  m_logCount++;
}

void
BackupRestore::endOfLogEntrys()
{
  if (m_restore) 
  {
    ndbout << "Restored " << m_dataCount << " tuples and "
	     << m_logCount << " log entries" << endl;
  }
}
#if 0
/*****************************************
 *
 * Callback function for asynchronous transactions
 *
 * Idea for error handling: Transaction objects have to be stored globally when
 *                they are prepared.
 *        In the callback function if the transaction:
 *          succeeded: delete the object from global storage
 *          failed but can be retried: execute the object that is in global storage
 *          failed but fatal: delete the object from global storage
 *
 ******************************************/
static void restoreCallback(int result,            // Result for transaction
			    NdbTransaction *object, // Transaction object
			    void *anything)        // Not used
{
  static Uint32 counter = 0;
  

  debug << "restoreCallback function called " << counter << " time(s)" << endl;

  ++counter;

  if (result == -1) 
  {
      ndbout << " restoreCallback (" << counter;
      if ((counter % 10) == 1) 
      {
	  ndbout << "st";
      } // if
      else if ((counter % 10) == 2) 
      {
	ndbout << "nd";
      } // else if
      else if ((counter % 10 ) ==3) 
      {
	ndbout << "rd";
      } // else if
      else 
      {
	ndbout << "th";
      } // else
      err << " time: error detected " << object->getNdbError() << endl;
    } // if
  
} // restoreCallback
#endif



/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbTransaction object, and 
 *   - A pointer to an arbitrary object.)
 */

static void
callback(int result, NdbTransaction* trans, void* aObject)
{
  restore_callback_t *cb = (restore_callback_t *)aObject;
  (cb->restore)->cback(result, cb);
}

/**
 * returns true if is recoverable,
 * Error handling based on hugo
 *  false if it is an  error that generates an abort.
 */
static
bool asynchErrorHandler(NdbTransaction * trans, Ndb* ndb) 
{
  NdbError error = trans->getNdbError();
  ndb->closeTransaction(trans);
  switch(error.status)
  {
  case NdbError::Success:
      return false;
      // ERROR!
      break;
      
  case NdbError::TemporaryError:
    NdbSleep_MilliSleep(10);
    return true;
    // RETRY
    break;
    
  case NdbError::UnknownResult:
    ndbout << error << endl;
    return false;
    // ERROR!
    break;
    
  default:
  case NdbError::PermanentError:
    switch (error.code)
    {
    case 499:
    case 250:
      NdbSleep_MilliSleep(10);
      return true; //temp errors?
    default:
      break;
    }
    //ERROR
    ndbout << error << endl;
    return false;
    break;
  }
  return false;
}
