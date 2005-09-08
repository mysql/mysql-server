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

#include <NDBT_ReturnCodes.h>
#include "consumer_restore.hpp"
#include <NdbSleep.h>

extern my_bool opt_core;

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static void callback(int, NdbTransaction*, void*);

extern const char * g_connect_string;
bool
BackupRestore::init()
{
  release();

  if (!m_restore && !m_restore_meta)
    return true;

  m_cluster_connection = new Ndb_cluster_connection(g_connect_string);
  if(m_cluster_connection->connect(12, 5, 1) != 0)
  {
    return -1;
  }

  m_ndb = new Ndb(m_cluster_connection);

  if (m_ndb == NULL)
    return false;
  
  m_ndb->init(1024);
  if (m_ndb->waitUntilReady(30) != 0)
  {
    err << "Failed to connect to ndb!!" << endl;
    return false;
  }
  info << "Connected to ndb!!" << endl;

  m_callback = new restore_callback_t[m_parallelism];

  if (m_callback == 0)
  {
    err << "Failed to allocate callback structs" << endl;
    return false;
  }

  m_free_callback= m_callback;
  for (Uint32 i= 0; i < m_parallelism; i++) {
    m_callback[i].restore= this;
    m_callback[i].connection= 0;
    if (i > 0)
      m_callback[i-1].next= &(m_callback[i]);
  }
  m_callback[m_parallelism-1].next = 0;

  return true;
}

void BackupRestore::release()
{
  if (m_ndb)
  {
    delete m_ndb;
    m_ndb= 0;
  }

  if (m_callback)
  {
    delete [] m_callback;
    m_callback= 0;
  }

  if (m_cluster_connection)
  {
    delete m_cluster_connection;
    m_cluster_connection= 0;
  }
}

BackupRestore::~BackupRestore()
{
  release();
}

static
int 
match_blob(const char * name){
  int cnt, id1, id2;
  char buf[256];
  if((cnt = sscanf(name, "%[^/]/%[^/]/NDB$BLOB_%d_%d", buf, buf, &id1, &id2)) == 4){
    return id1;
  }
  
  return -1;
}

const NdbDictionary::Table*
BackupRestore::get_table(const NdbDictionary::Table* tab){
  if(m_cache.m_old_table == tab)
    return m_cache.m_new_table;
  m_cache.m_old_table = tab;

  int cnt, id1, id2;
  char db[256], schema[256];
  if((cnt = sscanf(tab->getName(), "%[^/]/%[^/]/NDB$BLOB_%d_%d", 
		   db, schema, &id1, &id2)) == 4){
    m_ndb->setDatabaseName(db);
    m_ndb->setSchemaName(schema);
    
    BaseString::snprintf(db, sizeof(db), "NDB$BLOB_%d_%d", 
			 m_new_tables[id1]->getTableId(), id2);
    
    m_cache.m_new_table = m_ndb->getDictionary()->getTable(db);
    
  } else {
    m_cache.m_new_table = m_new_tables[tab->getTableId()];
  }
  assert(m_cache.m_new_table);
  return m_cache.m_new_table;
}

bool
BackupRestore::finalize_table(const TableS & table){
  bool ret= true;
  if (!m_restore && !m_restore_meta)
    return ret;
  if (table.have_auto_inc())
  {
    Uint64 max_val= table.get_max_auto_val();
    Uint64 auto_val= m_ndb->readAutoIncrementValue(get_table(table.m_dictTable));
    if (max_val+1 > auto_val || auto_val == ~(Uint64)0)
      ret= m_ndb->setAutoIncrementValue(get_table(table.m_dictTable), max_val+1, false);
  }
  return ret;
}

bool
BackupRestore::table(const TableS & table){
  if (!m_restore && !m_restore_meta)
    return true;

  const char * name = table.getTableName();
  
  /**
   * Ignore blob tables
   */
  if(match_blob(name) >= 0)
    return true;
  
  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* table.m_dictTable);
  if(tmptab.m_indexType != NdbDictionary::Index::Undefined){
    m_indexes.push_back(table.m_dictTable);
    return true;
  }
  
  BaseString tmp(name);
  Vector<BaseString> split;
  if(tmp.split(split, "/") != 3){
    err << "Invalid table name format " << name << endl;
    return false;
  }

  m_ndb->setDatabaseName(split[0].c_str());
  m_ndb->setSchemaName(split[1].c_str());
  
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if(m_restore_meta){
    NdbDictionary::Table copy(*table.m_dictTable);

    copy.setName(split[2].c_str());

    if (dict->createTable(copy) == -1) 
    {
      err << "Create table " << table.getTableName() << " failed: "
	  << dict->getNdbError() << endl;
      return false;
    }
    info << "Successfully restored table " << table.getTableName()<< endl ;
  }  
  
  const NdbDictionary::Table* tab = dict->getTable(split[2].c_str());
  if(tab == 0){
    err << "Unable to find table: " << split[2].c_str() << endl;
    return false;
  }
  if(m_restore_meta){
    m_ndb->setAutoIncrementValue(tab, ~(Uint64)0, false);
  }
  const NdbDictionary::Table* null = 0;
  m_new_tables.fill(table.m_dictTable->getTableId(), null);
  m_new_tables[table.m_dictTable->getTableId()] = tab;
  return true;
}

bool
BackupRestore::endOfTables(){
  if(!m_restore_meta)
    return true;

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  for(size_t i = 0; i<m_indexes.size(); i++){
    NdbTableImpl & indtab = NdbTableImpl::getImpl(* m_indexes[i]);

    BaseString tmp(indtab.m_primaryTable.c_str());
    Vector<BaseString> split;
    if(tmp.split(split, "/") != 3){
      err << "Invalid table name format " << indtab.m_primaryTable.c_str()
	  << endl;
      return false;
    }
    
    m_ndb->setDatabaseName(split[0].c_str());
    m_ndb->setSchemaName(split[1].c_str());
    
    const NdbDictionary::Table * prim = dict->getTable(split[2].c_str());
    if(prim == 0){
      err << "Unable to find base table \"" << split[2].c_str() 
	  << "\" for index "
	  << indtab.getName() << endl;
      return false;
    }
    NdbTableImpl& base = NdbTableImpl::getImpl(*prim);
    NdbIndexImpl* idx;
    int id;
    char idxName[255], buf[255];
    if(sscanf(indtab.getName(), "%[^/]/%[^/]/%d/%s",
	      buf, buf, &id, idxName) != 4){
      err << "Invalid index name format " << indtab.getName() << endl;
      return false;
    }
    if(NdbDictInterface::create_index_obj_from_table(&idx, &indtab, &base))
    {
      err << "Failed to create index " << idxName
	  << " on " << split[2].c_str() << endl;
	return false;
    }
    idx->setName(idxName);
    if(dict->createIndex(* idx) != 0)
    {
      delete idx;
      err << "Failed to create index " << idxName
	  << " on " << split[2].c_str() << endl
	  << dict->getNdbError() << endl;

      return false;
    }
    delete idx;
    info << "Successfully created index " << idxName
	 << " on " << split[2].c_str() << endl;
  }
  return true;
}

void BackupRestore::tuple(const TupleS & tup)
{
  if (!m_restore) 
    return;

  while (m_free_callback == 0)
  {
    assert(m_transactions == m_parallelism);
    // send-poll all transactions
    // close transaction is done in callback
    m_ndb->sendPollNdb(3000, 1);
  }
  
  restore_callback_t * cb = m_free_callback;
  
  if (cb == 0)
    assert(false);
  
  m_free_callback = cb->next;
  cb->retries = 0;
  cb->tup = tup; // must do copy!
  tuple_a(cb);

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
      if (errorHandler(cb)) 
      {
	m_ndb->sendPollNdb(3000, 1);
	continue;
      }
      exitHandler();
    } // if
    
    const TupleS &tup = cb->tup;
    const NdbDictionary::Table * table = get_table(tup.getTable()->m_dictTable);

    NdbOperation * op = cb->connection->getNdbOperation(table);
    
    if (op == NULL) 
    {
      if (errorHandler(cb)) 
	continue;
      exitHandler();
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (errorHandler(cb))
	continue;
      exitHandler();
    } // if
    
    int ret = 0;
    for (int j = 0; j < 2; j++)
    {
      for (int i = 0; i < tup.getNoOfAttributes(); i++) 
      {
	const AttributeDesc * attr_desc = tup.getDesc(i);
	const AttributeData * attr_data = tup.getData(i);
	int size = attr_desc->size;
	int arraySize = attr_desc->arraySize;
	char * dataPtr = attr_data->string_value;
	Uint32 length = (size * arraySize) / 8;

	if (j == 0 && tup.getTable()->have_auto_inc(i))
	  tup.getTable()->update_max_auto_val(dataPtr,size);

	if (attr_desc->m_column->getPrimaryKey())
	{
	  if (j == 1) continue;
	  ret = op->equal(i, dataPtr, length);
	}
	else
	{
	  if (j == 0) continue;
	  if (attr_data->null) 
	    ret = op->setValue(i, NULL, 0);
	  else
	    ret = op->setValue(i, dataPtr, length);
	}
	if (ret < 0) {
	  ndbout_c("Column: %d type %d %d %d %d",i,
		   attr_desc->m_column->getType(),
		   size, arraySize, attr_data->size);
	  break;
	}
      }
      if (ret < 0)
	break;
    }
    if (ret < 0)
    {
      if (errorHandler(cb)) 
	continue;
      exitHandler();
    }

    // Prepare transaction (the transaction is NOT yet sent to NDB)
    cb->connection->executeAsynchPrepare(NdbTransaction::Commit,
					 &callback, cb);
    m_transactions++;
    return;
  }
  err << "Retried transaction " << cb->retries << " times.\nLast error"
      << m_ndb->getNdbError(cb->error_code) << endl
      << "...Unable to recover from errors. Exiting..." << endl;
  exitHandler();
}

void BackupRestore::cback(int result, restore_callback_t *cb)
{
  m_transactions--;

  if (result < 0)
  {
    /**
     * Error. temporary or permanent?
     */
    if (errorHandler(cb))
      tuple_a(cb); // retry
    else
    {
      err << "Restore: Failed to restore data due to a unrecoverable error. Exiting..." << endl;
      exitHandler();
    }
  }
  else
  {
    /**
     * OK! close transaction
     */
    m_ndb->closeTransaction(cb->connection);
    cb->connection= 0;
    cb->next= m_free_callback;
    m_free_callback= cb;
    m_dataCount++;
  }
}

/**
 * returns true if is recoverable,
 * Error handling based on hugo
 *  false if it is an  error that generates an abort.
 */
bool BackupRestore::errorHandler(restore_callback_t *cb) 
{
  NdbError error;
  if(cb->connection)
  {
    error= cb->connection->getNdbError();
    m_ndb->closeTransaction(cb->connection);
    cb->connection= 0;
  }
  else
  {
    error= m_ndb->getNdbError();
  } 

  Uint32 sleepTime = 100 + cb->retries * 300;
  
  cb->retries++;
  cb->error_code = error.code;

  switch(error.status)
  {
  case NdbError::Success:
    return false;
    // ERROR!
    break;
    
  case NdbError::TemporaryError:
    err << "Temporary error: " << error << endl;
    NdbSleep_MilliSleep(sleepTime);
    return true;
    // RETRY
    break;
    
  case NdbError::UnknownResult:
    err << error << endl;
    return false;
    // ERROR!
    break;
    
  default:
  case NdbError::PermanentError:
    //ERROR
    err << error << endl;
    return false;
    break;
  }
  return false;
}

void BackupRestore::exitHandler() 
{
  release();
  NDBT_ProgramExit(NDBT_FAILED);
  if (opt_core)
    abort();
  else
    exit(NDBT_FAILED);
}


void
BackupRestore::tuple_free()
{
  if (!m_restore)
    return;

  // Poll all transactions
  while (m_transactions)
  {
    m_ndb->sendPollNdb(3000);
  }
}

void
BackupRestore::endOfTuples()
{
  tuple_free();
}

void
BackupRestore::logEntry(const LogEntry & tup)
{
  if (!m_restore)
    return;

  NdbTransaction * trans = m_ndb->startTransaction();
  if (trans == NULL) 
  {
    // Deep shit, TODO: handle the error
    err << "Cannot start transaction" << endl;
    exitHandler();
  } // if
  
  const NdbDictionary::Table * table = get_table(tup.m_table->m_dictTable);
  NdbOperation * op = trans->getNdbOperation(table);
  if (op == NULL) 
  {
    err << "Cannot get operation: " << trans->getNdbError() << endl;
    exitHandler();
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
    err << "Log entry has wrong operation type."
	   << " Exiting...";
    exitHandler();
  }

  if (check != 0) 
  {
    err << "Error defining op: " << trans->getNdbError() << endl;
    exitHandler();
  } // if
  
  Bitmask<4096> keys;
  for (Uint32 i= 0; i < tup.size(); i++) 
  {
    const AttributeS * attr = tup[i];
    int size = attr->Desc->size;
    int arraySize = attr->Desc->arraySize;
    const char * dataPtr = attr->Data.string_value;
    
    if (tup.m_table->have_auto_inc(attr->Desc->attrId))
      tup.m_table->update_max_auto_val(dataPtr,size);

    const Uint32 length = (size / 8) * arraySize;
    if (attr->Desc->m_column->getPrimaryKey())
    {
      if(!keys.get(attr->Desc->attrId))
      {
	keys.set(attr->Desc->attrId);
	check= op->equal(attr->Desc->attrId, dataPtr, length);
      }
    }
    else
      check= op->setValue(attr->Desc->attrId, dataPtr, length);
    
    if (check != 0) 
    {
      err << "Error defining op: " << trans->getNdbError() << endl;
      exitHandler();
    } // if
  }
  
  const int ret = trans->execute(NdbTransaction::Commit);
  if (ret != 0)
  {
    // Both insert update and delete can fail during log running
    // and it's ok
    // TODO: check that the error is either tuple exists or tuple does not exist?
    bool ok= false;
    NdbError errobj= trans->getNdbError();
    switch(tup.m_type)
    {
    case LogEntry::LE_INSERT:
      if(errobj.status == NdbError::PermanentError &&
	 errobj.classification == NdbError::ConstraintViolation)
	ok= true;
      break;
    case LogEntry::LE_UPDATE:
    case LogEntry::LE_DELETE:
      if(errobj.status == NdbError::PermanentError &&
	 errobj.classification == NdbError::NoDataFound)
	ok= true;
      break;
    }
    if (!ok)
    {
      err << "execute failed: " << errobj << endl;
      exitHandler();
    }
  }
  
  m_ndb->closeTransaction(trans);
  m_logCount++;
}

void
BackupRestore::endOfLogEntrys()
{
  if (!m_restore)
    return;

  info << "Restored " << m_dataCount << " tuples and "
       << m_logCount << " log entries" << endl;
}

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
      // Deep shit, TODO: handle the error
      ndbout << "Cannot start transaction" << endl;
      exitHandler();
    } // if
    
    const TableS * table = tup.getTable();
    NdbOperation * op = trans->getNdbOperation(table->getTableName());
    if (op == NULL) 
    {
      ndbout << "Cannot get operation: ";
      ndbout << trans->getNdbError() << endl;
      exitHandler();
    } // if
    
    // TODO: check return value and handle error
    if (op->writeTuple() == -1) 
    {
      ndbout << "writeTuple call failed: ";
      ndbout << trans->getNdbError() << endl;
      exitHandler();
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
    int ret = trans->execute(NdbTransaction::Commit);
    if (ret != 0)
    {
      ndbout << "execute failed: ";
      ndbout << trans->getNdbError() << endl;
      exitHandler();
    }
    m_ndb->closeTransaction(trans);
    if (ret == 0)
      break;
  }
  m_dataCount++;
}
#endif

template class Vector<NdbDictionary::Table*>;
template class Vector<const NdbDictionary::Table*>;
