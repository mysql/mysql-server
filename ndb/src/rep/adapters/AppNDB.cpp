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

#include "AppNDB.hpp"
#include <ConfigRetriever.hpp>
#include <AttributeHeader.hpp>
#include <NdbOperation.hpp>
#include <NdbDictionaryImpl.hpp>

#include <signaldata/RepImpl.hpp>
#include <TransporterFacade.hpp>
#include <trigger_definitions.h>
#include <rep/storage/GCIPage.hpp>
#include <rep/storage/GCIBuffer.hpp>
#include <rep/rep_version.hpp>

/*****************************************************************************
 * Constructor / Destructor / Init
 *****************************************************************************/

AppNDB::~AppNDB() 
{
  delete m_tableInfoPs;
  delete m_ndb;
  m_tableInfoPs = 0;
}

AppNDB::AppNDB(GCIContainer * gciContainer, RepState * repState)
{
  m_gciContainer = gciContainer;
  m_repState = repState;
  m_cond = NdbCondition_Create();
  m_started = true;
}

void 
AppNDB::init(const char* connectString) {

  //  NdbThread_SetConcurrencyLevel(1+ 2);
  m_ndb = new Ndb("");

  m_ndb->useFullyQualifiedNames(false);

  m_ndb->setConnectString(connectString);
  /**
   * @todo  Set proper max no of transactions?? needed?? Default 12??
   */
  m_ndb->init(2048);
  m_dict = m_ndb->getDictionary();

  m_ownNodeId = m_ndb->getNodeId();

  ndbout << "-- NDB Cluster -- REP node " << m_ownNodeId << " -- Version " 
	 << REP_VERSION_ID << " --" << endl;
  ndbout_c("Connecting to NDB Cluster...");
  if (m_ndb->waitUntilReady() != 0){
    REPABORT("NDB Cluster not ready for connections");
  }
  ndbout_c("Phase 1 (AppNDB): Connection 1 to NDB Cluster opened (Applier)");
  
  m_tableInfoPs = new TableInfoPs();

  m_applierThread = NdbThread_Create(runAppNDB_C,
				     (void**)this,
				     32768,
				     "AppNDBThread",
				     NDB_THREAD_PRIO_LOW);  
}


/*****************************************************************************
 * Threads
 *****************************************************************************/

extern "C" 
void* 
runAppNDB_C(void * me)
{
  ((AppNDB *) me)->threadMainAppNDB();
  NdbThread_Exit(0);
  return me;
}

void 
AppNDB::threadMainAppNDB() {
  MetaRecord * mr;
  LogRecord * lr;
  GCIBuffer::iterator * itBuffer;
  GCIPage::iterator * itPage;
  GCIBuffer * buffer;
  GCIPage * page;
  Uint32 gci=0;

  bool force;
  while(true){

    m_gciBufferList.lock();
    if(m_gciBufferList.size()==0)
      NdbCondition_Wait(m_cond, m_gciBufferList.getMutex());
    m_gciBufferList.unlock();
    
    /**
     * Do nothing if we are not started!
     */
    if(!m_started)
      continue;
    
    if(m_gciBufferList.size()>0) {
      m_gciBufferList.lock();
      buffer = m_gciBufferList[0];
      assert(buffer!=0);
      if(buffer==0) {
	m_gciBufferList.unlock();      
//	stopApplier(GrepError::REP_APPLY_NULL_GCIBUFFER);
	return;
      }
      m_gciBufferList.unlock();      

      RLOG(("Applying %d:[%d]", buffer->getId(), buffer->getGCI()));
      gci = buffer->getGCI();
      /**
       * Do stuff with buffer
       */
     
      force = buffer->m_force;
      itBuffer = new GCIBuffer::iterator(buffer);
      page = itBuffer->first();
      
      Record * record;     
      while(page!=0 && m_started) {
  
	itPage = new GCIPage::iterator(page);
	record = itPage->first();
	
	while(record!=0 && m_started) {
	  switch(Record::RecordType(record->recordType)) {
	  case Record::META:
	    mr  = (MetaRecord*)record;
	    if(applyMetaRecord(mr, gci) < 0){
	      /**
	       * If we fail with a meta record then 
	       * we should fail the replication!
 	       */
	      //stopApplier(GrepError::REP_APPLY_METARECORD_FAILED);
	    }
	  break;
	  case Record::LOG:
	    lr  = (LogRecord*)record;
	    if(applyLogRecord(lr, force, gci) < 0) {
	      /**
	       * If we fail to apply a log record AND
	       * we have sent a ref to repstate event,
	       * then we should not try to apply another one!
	       */
//	      stopApplier(GrepError::REP_APPLY_LOGRECORD_FAILED);
	    }
	    break;
	  default:
	    REPABORT("Illegal record type");
	  };
	  record = itPage->next();
	}
	delete itPage;
	itPage = 0;
	page = itBuffer->next();
      }

      m_gciBufferList.erase(0, true);
      /**
       * "callback" to RepState to send REP_INSERT_GCIBUFFER_CONF
       */
      m_repState->eventInsertConf(buffer->getGCI(), buffer->getId());
      delete itBuffer;
      itBuffer = 0;
      mr = 0;
      lr = 0;
      page = 0;
      buffer = 0;
    }
  }

 
}

void AppNDB::startApplier(){
  m_started = true;
}


void AppNDB::stopApplier(GrepError::Code err){
  m_started = false;
  m_repState->eventInsertRef(0,0,0, err);
}


GrepError::Code
AppNDB::applyBuffer(Uint32 nodeGrp, Uint32 epoch, Uint32 force)
{
  m_gciBufferList.lock();

  GCIBuffer * buffer = m_gciContainer->getGCIBuffer(epoch, nodeGrp);
  if (buffer == NULL) {
    RLOG(("WARNING! Request to apply NULL buffer %d[%d]. Force %d", 
	  nodeGrp, epoch, force));
    return GrepError::NO_ERROR;
  }
  if (!buffer->isComplete()) {
    RLOG(("WARNING! Request to apply non-complete buffer %d[%d]. Force %d",
	  nodeGrp, epoch, force));
    return GrepError::REP_APPLY_NONCOMPLETE_GCIBUFFER;
  }
  buffer->m_force = force;

  assert(buffer!=0);
  m_gciBufferList.push_back(buffer, false);
  NdbCondition_Broadcast(m_cond);
  m_gciBufferList.unlock();
  return GrepError::NO_ERROR;
}

int
AppNDB::applyLogRecord(LogRecord*  lr, bool force, Uint32 gci) 
{
#if 0
  RLOG(("Applying log record (force %d, Op %d, GCI %d)", 
	force, lr->operation, gci));
#endif
  
  int  retries =0;
 retry:
  if(retries == 10) {
    m_repState->eventInsertRef(gci, 0, lr->tableId,     
			       GrepError::REP_APPLIER_EXECUTE_TRANSACTION);
    return -1;
  }
  NdbConnection * trans = m_ndb->startTransaction();
  if (trans == NULL) {
    /**
     * Transaction could not be started
     * @todo Handle the error by:
     *       1. Return error code
     *       2. Print log message
     *       3. On higher level indicate that DB has been tainted
     */
    ndbout_c("AppNDB: Send the following error msg to NDB Cluster support");
    reportNdbError("Cannot start transaction!", trans->getNdbError());
    m_repState->eventInsertRef(gci, 0, 0, 
			       GrepError::REP_APPLIER_START_TRANSACTION);
    REPABORT("Can not start transaction");
  }
  
  /**
   * Resolve table name based on table id
   */
  const Uint32 tableId = lr->tableId;
  const char * tableName = m_tableInfoPs->getTableName(tableId);
  
  /**
   * Close trans and return if it is systab_0.
   */
  if (tableId == 0) {
    RLOG(("WARNING! System table log record received"));
    m_ndb->closeTransaction(trans);    
    return -1;
  }
  
  if (tableName==0) {
    /**
     * Table probably does not exist
     * (Under normal operation this should not happen 
     * since log records should not appear unless the 
     * table has been created.)
     *
     * @todo Perhaps the table is not cached due to a restart,
     *       so let's check in the dictionary if it exists.
     */
    m_ndb->closeTransaction(trans);
    m_repState->eventInsertRef(gci, 0, tableId, 
			       GrepError::REP_APPLIER_NO_TABLE);
    return -1;
  }
  
  const NdbDictionary::Table * table  = m_dict->getTable(tableName);
  
  NdbOperation * op = trans->getNdbOperation(tableName);
  if (op == NULL) {
    ndbout_c("AppNDB: Send the following error msg to NDB Cluster support");
    reportNdbError("Cannot get NdbOperation record",
		   trans->getNdbError());
    m_repState->eventInsertRef(gci,0,tableId,
			       GrepError::REP_APPLIER_NO_OPERATION);
    REPABORT("Can not get NdbOperation record");
  }
  
  int check=0;
  switch(lr->operation) {
  case TriggerEvent::TE_INSERT: // INSERT
    check = op->insertTuple();
    break;
  case TriggerEvent::TE_DELETE: // DELETE
    check = op->deleteTuple();    
    break;
  case TriggerEvent::TE_UPDATE: // UPDATE
    if (force) {
      check = op->writeTuple();
    } else {
      check = op->updateTuple();
    }
    break;
  case TriggerEvent::TE_CUSTOM: //SCAN
    check = op->writeTuple();
    break;
  default:
    m_ndb->closeTransaction(trans);
    return -1;
  };

  if (check<0) {
    ndbout_c("AppNDB: Something is weird");
  }
  
  /**
   * @todo index inside LogRecord struct somewhat prettier
   * Now it 4 (sizeof(Uint32)), and 9 the position inside the struct 
   * where the data starts.
   */
  AttributeHeader * ah=(AttributeHeader *)((char *)lr + sizeof(Uint32) * 9);
  AttributeHeader *end = (AttributeHeader *)(ah + lr->attributeHeaderWSize); 
  Uint32 * dataPtr = (Uint32 *)(end);

  /**
   *  @note attributeheader for operaration insert includes a duplicate
   *  p.k.  The quick fix for this problem/bug is to skip the first set of 
   *  of p.k, and start from the other set of P.Ks. Data is duplicated for
   *  the p.k.
   */
  if (lr->operation == 0) {
    for(int i = 0; i< table->getNoOfPrimaryKeys(); i++) {
      ah+=ah->getHeaderSize();
      dataPtr = dataPtr + ah->getDataSize();
    }
  }

  while (ah < end) {
    const NdbDictionary::Column * column = 
      table->getColumn(ah->getAttributeId());
    /**
     * @todo: Here is a limitation. I don't care if it is a tuplekey 
     * that is autogenerated or an ordinary pk. I just whack it in.
     * However, this must be examined.
     */
    if(column->getPrimaryKey()) {
      if(op->equal(ah->getAttributeId(), (const char *)dataPtr) < 0) {
	ndbout_c("AppNDB: Equal failed id %d op %d name %s, gci %d force %d", 
		 ah->getAttributeId(),
		 lr->operation,
		 column->getName(), gci, force);
	reportNdbError("Equal!", trans->getNdbError());
	}
      
    } else {
      if(op->setValue(ah->getAttributeId(), (const char *)dataPtr) < 0)
       ndbout_c("AppNDB: setvalue failed id %d op %d name %s, gci %d force %d",
		ah->getAttributeId(),
		lr->operation,
		column->getName(), gci, force);
    }
    
    dataPtr = dataPtr + ah->getDataSize();
    ah = ah + ah->getHeaderSize() ;
  }
  
  if(trans->execute(Commit) != 0) {
    /**
     * Transaction commit failure
     */
      const NdbError err = trans->getNdbError();
      m_ndb->closeTransaction(trans);      
      switch(err.status){
      case NdbError::Success:
	{
	  m_repState->eventInsertRef(gci, 0, tableId,     
				     GrepError::REP_APPLIER_EXECUTE_TRANSACTION);
	  return -1;
	}
        break;
      case NdbError::TemporaryError:      
	{
	  NdbSleep_MilliSleep(50);
	  retries++;	
	  goto retry;
	}
	break;
      case NdbError::UnknownResult:
	{
	  ndbout_c("AppNDB: Send the following error msg to NDB Cluster support");
	  reportNdbError("Execute transaction failed!",
			 trans->getNdbError());
	  m_repState->eventInsertRef(gci, 0, tableId,     
				     GrepError::REP_APPLIER_EXECUTE_TRANSACTION);
	  return -1;
	}
	break;
      case NdbError::PermanentError: 
	{
	  if(err.code == 626) {
	    if(force && lr->operation == TriggerEvent::TE_DELETE) /**delete*/ {
	      /**tuple was not found. Ignore this, since 
	       * we are trying to apply a "delete a tuple"-log record before 
	       * having applied the scan data.
	       */
	      return -1;
	    }
	  }

	  ndbout_c("AppNDB: Send the following error msg to NDB Cluster support");	  	  reportNdbError("Execute transaction failed!",
			 trans->getNdbError());
	  ndbout_c("\n\nAppNDB: RepNode will now crash.");
	  m_ndb->closeTransaction(trans);
	  m_repState->eventInsertRef(gci, 0, tableId,     
			     GrepError::REP_APPLIER_EXECUTE_TRANSACTION);
	  return -1;
	}	
      break;
      }
  }

  /**
   * No errors. Close transaction and continue in applierThread.
   */
  m_ndb->closeTransaction(trans); 
  return 1;
}


int
AppNDB::applyMetaRecord(MetaRecord*  mr, Uint32 gci) 
{
  /**
   * Validate table id
   */
  Uint32 tableId = mr->tableId;
  if (tableId==0) {
    RLOG(("WARNING! Meta record contained record with tableId 0"));
    return 0;
  }
  
  /**
   * Prepare meta record 
   */
  NdbDictionary::Table * table = prepareMetaRecord(mr);
  if(table == 0) {
    RLOG(("WARNING! Prepare table meta record failed for table %d", tableId));
    m_dict->getNdbError();
    m_repState->eventInsertRef(gci,0,tableId, 
			       GrepError::REP_APPLIER_PREPARE_TABLE);
    return -1;
  }
  
  /**
   * Table does not exist in TableInfoPs -> add it
   */
  if(m_tableInfoPs->getTableName(tableId)==0) {
    RLOG(("Table %d:%s added to m_tableInfoPs", tableId, table->getName()));
    m_tableInfoPs->insert(tableId,table->getName());
  }
  
  /**
   * Validate that table does not exist in Dict
   */

  const NdbDictionary::Table * tmpTable = m_dict->getTable(table->getName());
  if(tmpTable !=0) {
    /**
     * Oops, a table with the same name exists
     */
    if(tmpTable->getObjectVersion()!=table->getObjectVersion()) {
      char buf[100];
      sprintf(buf,"WARNING! Another version of table %d:%s already exists."
	      "Currently, we dont support versions, so will abort now!",
	       tableId, table->getName());
    
      REPABORT(buf);

    }
    RLOG(("WARNING! An identical table %d:%s already exists.", 
	  tableId, table->getName()));
    return -1;
  }


  /**
   * @todo WARNING! Should scan table MR for columns that are not supported
   */
  /*
  NdbDictionary::Column * column;
  
  for(int i=0; i<table->getNoOfColumns(); i++) {
    column = table->getColumn(i);
    if(column->getAutoIncrement()) {
      reportWarning(table->getName(), column->getName(),
		    "Uses AUTOINCREMENT of PK");   
    }
  }
  */
    
  
  /**
   * Create table
   */
  if(m_dict->createTable(*table)<0) {
    ndbout_c("AppNDB: Send the following error msg to NDB Cluster support");
    reportNdbError("Create table failed!", m_dict->getNdbError());
    m_repState->eventCreateTableRef(gci, 
				    tableId,
				    table->getName(),			  
				    GrepError::REP_APPLIER_CREATE_TABLE);
    return -1;
  }
  
  RLOG(("Table %d:%s created", tableId, table->getName()));
  return 0;
}

NdbDictionary::Table*
AppNDB::prepareMetaRecord(MetaRecord* mr) {
  NdbTableImpl * tmp = 0;
  NdbDictionary::Table * table =0;
  Uint32 * data =(Uint32*)( ((char*)mr + sizeof(Uint32)*6));
  int res = NdbDictInterface::parseTableInfo(&tmp, data, mr->dataLen,
					     m_ndb->usingFullyQualifiedNames());
  if(res == 0) {
    table = tmp;
    return table;
  } else{
    return 0;
  }
}

void 
AppNDB::reportNdbError(const char * msg, const NdbError & err) {
  ndbout_c("%s : Error code %d , error message %s", 
	   msg, err.code,
	   (err.message ? err.message : ""));  
}

void 
AppNDB::reportWarning(const char * tableName, const char * message) {
  ndbout_c("WARNING: Table %s, %s", tableName, message); 
}

void 
AppNDB::reportWarning(const char * tableName, const char * columnName,
		       const char * message) {
  ndbout_c("WARNING: Table %s, column %s, %s", tableName, columnName,message);
}

int 
AppNDB::dropTable(Uint32 tableId) 
{
  char * tableName = m_tableInfoPs->getTableName(tableId);
  if(tableName == 0) return -1;
  ndbout_c("AppNDB: Dropping table ");
  if(m_dict->dropTable(tableName) != 0) {
    reportNdbError("Failed dropping table",m_dict->getNdbError());
    return -1;
  }
  m_tableInfoPs->del(tableId);
  return 1;
}
