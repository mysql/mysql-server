/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <my_sys.h>

#define DBDICT_C
#include "Dbdict.hpp"
#include "diskpage.hpp"

#include <ndb_limits.h>
#include <ndb_math.h>
#include <NdbOut.hpp>
#include <OutputStream.hpp>
#include <Properties.hpp>
#include <Configuration.hpp>
#include <SectionReader.hpp>
#include <SimpleProperties.hpp>
#include <AttributeHeader.hpp>
#include <KeyDescriptor.hpp>
#include <signaldata/DictSchemaInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/DropTabFile.hpp>

#include <signaldata/EventReport.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/GetTableId.hpp>
#include <signaldata/HotSpareRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/RelTabMem.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/ListTables.hpp>

#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/BuildIndx.hpp>

#include <signaldata/DropFilegroup.hpp>
#include <signaldata/CreateFilegroup.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>

#include <signaldata/CreateEvnt.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilRelease.hpp>
#include <signaldata/SumaImpl.hpp>

#include <signaldata/LqhFrag.hpp>
#include <signaldata/DictStart.hpp>
#include <signaldata/DiAddTab.hpp>
#include <signaldata/DihStartTab.hpp>

#include <signaldata/DropTable.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/PrepDropTab.hpp>

#include <signaldata/CreateTable.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/CreateFragmentation.hpp>
#include <signaldata/CreateTab.hpp>
#include <NdbSleep.h>
#include <signaldata/ApiBroadcast.hpp>
#include <signaldata/DictLock.hpp>
#include <signaldata/BackupLockTab.hpp>
#include <SLList.hpp>

#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/CheckNodeGroups.hpp>

#include <NdbEnv.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <signaldata/SchemaTrans.hpp>
#include <DebuggerNames.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>

#include <signaldata/IndexStatSignal.hpp>

#define ZNOT_FOUND 626
#define ZALREADYEXIST 630

#define ZRESTART_OPS_PER_TRANS 25
#define ZRESTART_NO_WRITE_AFTER_READ 1

//#define EVENT_PH2_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG

static const char EVENT_SYSTEM_TABLE_NAME[] = "sys/def/NDB$EVENTS_0";

#define EVENT_TRACE \
//  ndbout_c("Event debug trace: File: %s Line: %u", __FILE__, __LINE__)

#define DIV(x,y) (((x)+(y)-1)/(y))
#define WORDS2PAGES(x) DIV(x, (ZSIZE_OF_PAGES_IN_WORDS - ZPAGE_HEADER_SIZE))
#include <ndb_version.h>

static
Uint32
alter_obj_inc_schema_version(Uint32 old)
{
   return (old & 0x00FFFFFF) + ((old + 0x1000000) & 0xFF000000);
}

static
Uint32
alter_obj_dec_schema_version(Uint32 old)
{
  return (old & 0x00FFFFFF) + ((old - 0x1000000) & 0xFF000000);
}

static
Uint32
create_obj_inc_schema_version(Uint32 old)
{
  return (old + 0x00000001) & 0x00FFFFFF;
}

static
void
do_swap(Uint32 & v0, Uint32 & v1)
{
  Uint32 save = v0;
  v0 = v1;
  v1 = save;
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          GENERAL MODULE -------------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains general stuff. Mostly debug signals and     */
/* general signals that go into a specific module after checking a  */
/* state variable. Also general subroutines used by many.           */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

/* ---------------------------------------------------------------- */
// This signal is used to dump states of various variables in the
// block by command.
/* ---------------------------------------------------------------- */
void
Dbdict::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();

#ifdef VM_TRACE
  if(signal->theData[0] == 1222){
    const Uint32 tab = signal->theData[1];
    PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
    req->senderRef = reference();
    req->senderData = 1222;
    req->tableId = tab;
    sendSignal(DBLQH_REF, GSN_PREP_DROP_TAB_REQ, signal,
	       PrepDropTabReq::SignalLength, JBB);
  }

  if(signal->theData[0] == 1223){
    const Uint32 tab = signal->theData[1];
    PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
    req->senderRef = reference();
    req->senderData = 1222;
    req->tableId = tab;
    sendSignal(DBTC_REF, GSN_PREP_DROP_TAB_REQ, signal,
	       PrepDropTabReq::SignalLength, JBB);
  }

  if(signal->theData[0] == 1224){
    const Uint32 tab = signal->theData[1];
    PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
    req->senderRef = reference();
    req->senderData = 1222;
    req->tableId = tab;
    sendSignal(DBDIH_REF, GSN_PREP_DROP_TAB_REQ, signal,
	       PrepDropTabReq::SignalLength, JBB);
  }

  if(signal->theData[0] == 1225){
    const Uint32 tab = signal->theData[1];
    const Uint32 ver = signal->theData[2];
    TableRecordPtr tabRecPtr;
    bool ok = find_object(tabRecPtr, tab);
    ndbrequire(ok);
    DropTableReq * req = (DropTableReq*)signal->getDataPtr();
    req->senderData = 1225;
    req->senderRef = numberToRef(1,1);
    req->tableId = tab;
    req->tableVersion = tabRecPtr.p->tableVersion + ver;
    sendSignal(DBDICT_REF, GSN_DROP_TABLE_REQ, signal,
	       DropTableReq::SignalLength, JBB);
  }
#endif
#define MEMINFO(x, y) infoEvent(x ": %d %d", y.getSize(), y.getNoOfFree())
  if(signal->theData[0] == 1226){
    MEMINFO("c_obj_pool", c_obj_pool);
    MEMINFO("c_opRecordPool", c_opRecordPool);
    MEMINFO("c_rope_pool", c_rope_pool);
  }

  if (signal->theData[0] == 1227)
  {
    DictObjectName_hash::Iterator iter;
    bool ok = c_obj_name_hash.first(iter);
    for (; ok; ok = c_obj_name_hash.next(iter))
    {
      LocalRope name(c_rope_pool, iter.curr.p->m_name);
      char buf[1024];
      name.copy(buf);
      ndbout_c("%s m_ref_count: %d", buf, iter.curr.p->m_ref_count);
      if (iter.curr.p->m_trans_key != 0)
        ndbout_c("- m_trans_key: %u m_op_ref_count: %u",
                 iter.curr.p->m_trans_key, iter.curr.p->m_op_ref_count);
    }
  }
  if (signal->theData[0] == DumpStateOrd::DictDumpLockQueue)
  {
    jam();
    m_dict_lock.dump_queue(m_dict_lock_pool, this);
    
    /* Space for hex form of enough words for node bitmask + \0 */
    char buf[(((MAX_NDB_NODES + 31)/32) * 8) + 1 ];
    infoEvent("DICT : c_sub_startstop _outstanding %u _lock %s",
              c_outstanding_sub_startstop,
              c_sub_startstop_lock.getText(buf));
  }

  if (signal->theData[0] == 8004)
  {
    infoEvent("DICT: c_counterMgr size: %u free: %u",
              c_counterMgr.getSize(),
              c_counterMgr.getNoOfFree());
    c_counterMgr.printNODE_FAILREP();
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_AP_SNAPSHOT_SAVE(c_rope_pool);
    RSS_AP_SNAPSHOT_SAVE(c_attributeRecordPool);
    RSS_AP_SNAPSHOT_SAVE(c_tableRecordPool_);
    RSS_AP_SNAPSHOT_SAVE(c_triggerRecordPool_);
    RSS_AP_SNAPSHOT_SAVE(c_obj_pool);
    RSS_AP_SNAPSHOT_SAVE(c_hash_map_pool);
    RSS_AP_SNAPSHOT_SAVE(g_hash_map);
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_AP_SNAPSHOT_CHECK(c_rope_pool);
    RSS_AP_SNAPSHOT_CHECK(c_attributeRecordPool);
    RSS_AP_SNAPSHOT_CHECK(c_tableRecordPool_);
    RSS_AP_SNAPSHOT_CHECK(c_triggerRecordPool_);
    RSS_AP_SNAPSHOT_CHECK(c_obj_pool);
    RSS_AP_SNAPSHOT_CHECK(c_hash_map_pool);
    RSS_AP_SNAPSHOT_CHECK(g_hash_map);
  }

  return;


}//Dbdict::execDUMP_STATE_ORD()

void Dbdict::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Attribute Record",
        c_attributeRecordPool.getUsed(),
        c_attributeRecordPool.getSize(),
        c_attributeRecordPool.getEntrySize(),
        c_attributeRecordPool.getUsedHi(),
        { CFG_DB_NO_ATTRIBUTES,0,0,0 }},
      { "Table Record",
        c_tableRecordPool_.getUsed(),
        c_noOfMetaTables,
        c_tableRecordPool_.getEntrySize(),
        c_tableRecordPool_.getUsedHi(),
        { CFG_DB_NO_TABLES,0,0,0 }},
      { "Trigger Record",
        c_triggerRecordPool_.getUsed(),
        c_triggerRecordPool_.getSize(),
        c_triggerRecordPool_.getEntrySize(),
        c_triggerRecordPool_.getUsedHi(),
        { CFG_DB_NO_TRIGGERS,0,0,0 }},
      { "FS Connect Record",
        c_fsConnectRecordPool.getUsed(),
        c_fsConnectRecordPool.getSize(),
        c_fsConnectRecordPool.getEntrySize(),
        c_fsConnectRecordPool.getUsedHi(),
        { 0,0,0,0 }},
      { "DictObject",
        c_obj_pool.getUsed(),
        c_obj_pool.getSize(),
        c_obj_pool.getEntrySize(),
        c_obj_pool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,
          CFG_DB_NO_TRIGGERS }},
      { "Transaction Handle",
        c_txHandlePool.getUsed(),
        c_txHandlePool.getSize(),
        c_txHandlePool.getEntrySize(),
        c_txHandlePool.getUsedHi(),
        { 0,0,0,0 }},
      { "Operation Record",
        c_opRecordPool.getUsed(),
        c_opRecordPool.getSize(),
        c_opRecordPool.getEntrySize(),
        c_opRecordPool.getUsedHi(),
        { 0,0,0,0 }},
      { NULL, 0,0,0,0,{0,0,0,0}}
    };

    const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    Uint32 pool = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);
      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}


/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
// CONTINUEB is used when a real-time break is needed for long
// processes.
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbdict::execCONTINUEB(Signal* signal)
{
  jamEntry();
  switch (signal->theData[0]) {
  case ZPACK_TABLE_INTO_PAGES :
    jam();
    packTableIntoPages(signal);
    break;

  case ZSEND_GET_TAB_RESPONSE :
    jam();
    sendGetTabResponse(signal);
    break;
  case ZWAIT_SUBSTARTSTOP:
    jam();
    wait_substartstop(signal, signal->theData[1]);
    return;
  case ZDICT_TAKEOVER_REQ :
  {
    jam();
    Uint32* data = &signal->theData[0];
    memmove(&data[0], &data[1], DictTakeoverReq::SignalLength << 2);
    execDICT_TAKEOVER_REQ(signal);
  }
  break;
  case ZINDEX_STAT_BG_PROCESS:
    jam();
    indexStatBg_process(signal);
    break;
#ifdef ERROR_INSERT
  case 6103: // search for it
    jam();
    {
      Uint32* data = &signal->theData[0];
      Uint32 masterRef = data[1];
      memmove(&data[0], &data[2], SchemaTransImplConf::SignalLength << 2);
      sendSignal(masterRef, GSN_SCHEMA_TRANS_IMPL_CONF, signal,
                 SchemaTransImplConf::SignalLength, JBB);
    }
    break;
  case 9999:
    ERROR_INSERTED(signal->theData[1]);
    CRASH_INSERTION(ERROR_INSERT_VALUE);
    break;
#endif
  case ZCOMMIT_WAIT_GCI:
    jam();
    trans_commit_wait_gci(signal);
    break;
  case ZGET_TABINFO_RETRY:
    // We have waited a while. Now we send a new request to the master.
    memmove(signal->theData, signal->theData+1, 
            GetTabInfoReq::SignalLength * sizeof *signal->theData);
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_GET_TABINFOREQ, signal,
               GetTabInfoReq::SignalLength, JBB);
    break;
  default :
    ndbrequire(false);
    break;
  }//switch
  return;
}//execCONTINUEB()

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
// Routine to handle pack table into pages.
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void Dbdict::packTableIntoPages(Signal* signal)
{
  const Uint32 tableId= signal->theData[1];
  const Uint32 type= signal->theData[2];
  const Uint32 pageId= signal->theData[3];

  Uint32 transId = c_retrieveRecord.schemaTransId;
  GetTabInfoReq req_copy;
  req_copy.senderRef = c_retrieveRecord.blockRef;
  req_copy.senderData = c_retrieveRecord.m_senderData;
  req_copy.schemaTransId = c_retrieveRecord.schemaTransId;
  req_copy.requestType = c_retrieveRecord.requestType;
  req_copy.tableId = tableId;

  {
    SchemaFile::TableEntry *objEntry = 0;
    if(tableId != RNIL)
    {
      XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
      objEntry = getTableEntry(xsf, tableId);
    }

    // The table seached for was not found
    if(objEntry == 0)
    {
      jam();
      sendGET_TABINFOREF(signal, &req_copy,
                         GetTabInfoRef::TableNotDefined, __LINE__);
      initRetrieveRecord(0, 0, 0);
      return;
    }

    if (transId != 0 && transId == objEntry->m_transId)
    {
      jam();
      // see own trans always
    }
    else if (refToBlock(req_copy.senderRef) != DBUTIL && /** XXX cheat */
             refToBlock(req_copy.senderRef) != SUMA)
    {
      jam();
      Uint32 err;
      if ((err = check_read_obj(objEntry)))
      {
        jam();
        // cannot see another uncommitted trans
        sendGET_TABINFOREF(signal, &req_copy,
                           (GetTabInfoRef::ErrorCode)err, __LINE__);
        initRetrieveRecord(0, 0, 0);
        return;
      }
    }
  }

  PageRecordPtr pagePtr;
  c_pageRecordArray.getPtr(pagePtr, pageId);

  memset(&pagePtr.p->word[0], 0, 4 * ZPAGE_HEADER_SIZE);
  LinearWriter w(&pagePtr.p->word[ZPAGE_HEADER_SIZE],
		 ZMAX_PAGES_OF_TABLE_DEFINITION * ZSIZE_OF_PAGES_IN_WORDS);
  w.first();
  switch((DictTabInfo::TableType)type) {
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:{
    jam();
    TableRecordPtr tablePtr;
    bool ok = find_object(tablePtr, tableId);
    if (!ok)
    {
      jam();
      sendGET_TABINFOREF(signal, &req_copy,
                         GetTabInfoRef::TableNotDefined, __LINE__);
      initRetrieveRecord(0, 0, 0);
      return;
    }

    packTableIntoPages(w, tablePtr, signal);
    if (unlikely(signal->theData[0] != 0))
    {
      jam();
      Uint32 err = signal->theData[0];
      GetTabInfoRef * ref = CAST_PTR(GetTabInfoRef, signal->getDataPtrSend());
      ref->tableId = c_retrieveRecord.tableId;
      ref->senderRef = reference();
      ref->senderData = c_retrieveRecord.m_senderData;
      ref->errorCode = err;
      Uint32 dstRef = c_retrieveRecord.blockRef;
      sendSignal(dstRef, GSN_GET_TABINFOREF, signal,
                 GetTabInfoRef::SignalLength, JBB);
      initRetrieveRecord(0,0,0);
      return;
    }
    break;
  }
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:{
    FilegroupPtr fg_ptr;
    ndbrequire(find_object(fg_ptr, tableId));
    const Uint32 free_hi= signal->theData[4];
    const Uint32 free_lo= signal->theData[5];
    packFilegroupIntoPages(w, fg_ptr, free_hi, free_lo);
    break;
  }
  case DictTabInfo::Datafile:{
    FilePtr fg_ptr;
    ndbrequire(find_object(fg_ptr, tableId));
    const Uint32 free_extents= signal->theData[4];
    packFileIntoPages(w, fg_ptr, free_extents);
    break;
  }
  case DictTabInfo::Undofile:{
    FilePtr fg_ptr;
    ndbrequire(find_object(fg_ptr, tableId));
    packFileIntoPages(w, fg_ptr, 0);
    break;
  }
  case DictTabInfo::HashMap:{
    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, tableId));
    packHashMapIntoPages(w, hm_ptr);
    break;
  }
  case DictTabInfo::UndefTableType:
  case DictTabInfo::HashIndexTrigger:
  case DictTabInfo::SubscriptionTrigger:
  case DictTabInfo::ReadOnlyConstraint:
  case DictTabInfo::IndexTrigger:
  case DictTabInfo::SchemaTransaction:
  case DictTabInfo::ReorgTrigger:
    ndbrequire(false);
  }

  Uint32 wordsOfTable = w.getWordsUsed();
  Uint32 pagesUsed = WORDS2PAGES(wordsOfTable);
  pagePtr.p->word[ZPOS_CHECKSUM] =
    computeChecksum(&pagePtr.p->word[0], pagesUsed * ZSIZE_OF_PAGES_IN_WORDS);

  switch (c_packTable.m_state) {
  case PackTable::PTS_IDLE:
    ndbrequire(false);
    break;
  case PackTable::PTS_GET_TAB:
    jam();
    c_retrieveRecord.retrievedNoOfPages = pagesUsed;
    c_retrieveRecord.retrievedNoOfWords = wordsOfTable;
    sendGetTabResponse(signal);
    return;
    break;
  }//switch
  ndbrequire(false);
  return;
}//packTableIntoPages()

void
Dbdict::packTableIntoPages(SimpleProperties::Writer & w,
			       TableRecordPtr tablePtr,
			       Signal* signal){

  union {
    char tableName[MAX_TAB_NAME_SIZE];
    char frmData[MAX_FRM_DATA_SIZE];
    char rangeData[16*MAX_NDB_PARTITIONS];
    char ngData[2*MAX_NDB_PARTITIONS];
    char defaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
    char attributeName[MAX_ATTR_NAME_SIZE];
  };
  ConstRope r(c_rope_pool, tablePtr.p->tableName);
  r.copy(tableName);
  w.add(DictTabInfo::TableName, tableName);
  w.add(DictTabInfo::TableId, tablePtr.p->tableId);
  w.add(DictTabInfo::TableVersion, tablePtr.p->tableVersion);
  w.add(DictTabInfo::NoOfKeyAttr, tablePtr.p->noOfPrimkey);
  w.add(DictTabInfo::NoOfAttributes, tablePtr.p->noOfAttributes);
  w.add(DictTabInfo::NoOfNullable, tablePtr.p->noOfNullAttr);
  w.add(DictTabInfo::NoOfVariable, (Uint32)0);
  w.add(DictTabInfo::KeyLength, tablePtr.p->tupKeyLength);

  w.add(DictTabInfo::TableLoggedFlag,
	!!(tablePtr.p->m_bits & TableRecord::TR_Logged));
  w.add(DictTabInfo::RowGCIFlag,
	!!(tablePtr.p->m_bits & TableRecord::TR_RowGCI));
  w.add(DictTabInfo::RowChecksumFlag,
	!!(tablePtr.p->m_bits & TableRecord::TR_RowChecksum));
  w.add(DictTabInfo::TableTemporaryFlag,
	!!(tablePtr.p->m_bits & TableRecord::TR_Temporary));
  w.add(DictTabInfo::ForceVarPartFlag,
	!!(tablePtr.p->m_bits & TableRecord::TR_ForceVarPart));

  w.add(DictTabInfo::MinLoadFactor, tablePtr.p->minLoadFactor);
  w.add(DictTabInfo::MaxLoadFactor, tablePtr.p->maxLoadFactor);
  w.add(DictTabInfo::TableKValue, tablePtr.p->kValue);
  w.add(DictTabInfo::FragmentTypeVal, tablePtr.p->fragmentType);
  w.add(DictTabInfo::TableTypeVal, tablePtr.p->tableType);
  w.add(DictTabInfo::MaxRowsLow, tablePtr.p->maxRowsLow);
  w.add(DictTabInfo::MaxRowsHigh, tablePtr.p->maxRowsHigh);
  w.add(DictTabInfo::DefaultNoPartFlag, tablePtr.p->defaultNoPartFlag);
  w.add(DictTabInfo::LinearHashFlag, tablePtr.p->linearHashFlag);
  w.add(DictTabInfo::FragmentCount, tablePtr.p->fragmentCount);
  w.add(DictTabInfo::MinRowsLow, tablePtr.p->minRowsLow);
  w.add(DictTabInfo::MinRowsHigh, tablePtr.p->minRowsHigh);
  w.add(DictTabInfo::SingleUserMode, tablePtr.p->singleUserMode);
  w.add(DictTabInfo::HashMapObjectId, tablePtr.p->hashMapObjectId);
  w.add(DictTabInfo::TableStorageType, tablePtr.p->storageType);
  w.add(DictTabInfo::ExtraRowGCIBits, tablePtr.p->m_extra_row_gci_bits);
  w.add(DictTabInfo::ExtraRowAuthorBits, tablePtr.p->m_extra_row_author_bits);


  if (tablePtr.p->hashMapObjectId != RNIL)
  {
    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, tablePtr.p->hashMapObjectId));
    w.add(DictTabInfo::HashMapVersion, hm_ptr.p->m_object_version);
  }

  if(signal)
  {
    /* This branch is run at GET_TABINFOREQ */

    Uint32 err = 0;
    if (!ERROR_INSERTED(6025))
    {
      err = get_fragmentation(signal, tablePtr.p->tableId);
    }
    else
    {
      err = CreateFragmentationRef::InvalidPrimaryTable;
    }
    if (unlikely(err != 0))
    {
      jam();
      signal->theData[0] = err;
      return;
    }

    ndbrequire(err == 0);
    Uint16 *data = (Uint16*)&signal->theData[25];
    Uint32 count = 2 + (1 + data[0]) * data[1];
    w.add(DictTabInfo::ReplicaDataLen, 2*count);
    for (Uint32 i = 0; i < count; i++)
      data[i] = htons(data[i]);
    w.add(DictTabInfo::ReplicaData, data, 2*count);
  }
  else
  {
    /* This part is run at CREATE_TABLEREQ, ALTER_TABLEREQ */
    ;
  }

  if (tablePtr.p->primaryTableId != RNIL)
  {
    jam();
    TableRecordPtr primTab;
    bool ok = find_object(primTab, tablePtr.p->primaryTableId);
    if (!ok)
    {
      jam();
      ndbrequire(signal != NULL);
      Uint32 err = CreateFragmentationRef::InvalidPrimaryTable;
      signal->theData[0] = err;
      return;
    }
    ConstRope r2(c_rope_pool, primTab.p->tableName);
    r2.copy(tableName);
    w.add(DictTabInfo::PrimaryTable, tableName);
    w.add(DictTabInfo::PrimaryTableId, tablePtr.p->primaryTableId);
    w.add(DictTabInfo::IndexState, tablePtr.p->indexState);
    w.add(DictTabInfo::CustomTriggerId, tablePtr.p->triggerId);
  }

  ConstRope frm(c_rope_pool, tablePtr.p->frmData);
  frm.copy(frmData);
  w.add(DictTabInfo::FrmLen, frm.size());
  w.add(DictTabInfo::FrmData, frmData, frm.size());

  {
    jam();
    w.add(DictTabInfo::TablespaceDataLen, (Uint32)0);

    ConstRope ng(c_rope_pool, tablePtr.p->ngData);
    ng.copy(ngData);
    w.add(DictTabInfo::FragmentDataLen, ng.size());
    w.add(DictTabInfo::FragmentData, ngData, ng.size());

    ConstRope range(c_rope_pool, tablePtr.p->rangeData);
    range.copy(rangeData);
    w.add(DictTabInfo::RangeListDataLen, range.size());
    w.add(DictTabInfo::RangeListData, rangeData, range.size());
  }

  if(tablePtr.p->m_tablespace_id != RNIL)
  {
    w.add(DictTabInfo::TablespaceId, tablePtr.p->m_tablespace_id);
    FilegroupPtr tsPtr;
    ndbrequire(find_object(tsPtr, tablePtr.p->m_tablespace_id));
    w.add(DictTabInfo::TablespaceVersion, tsPtr.p->m_version);
  }

  AttributeRecordPtr attrPtr;
  LocalAttributeRecord_list list(c_attributeRecordPool,
				    tablePtr.p->m_attributes);
  for(list.first(attrPtr); !attrPtr.isNull(); list.next(attrPtr)){
    jam();

    ConstRope name(c_rope_pool, attrPtr.p->attributeName);
    name.copy(attributeName);

    w.add(DictTabInfo::AttributeName, attributeName);
    w.add(DictTabInfo::AttributeId, attrPtr.p->attributeId);
    w.add(DictTabInfo::AttributeKeyFlag, attrPtr.p->tupleKey > 0);

    const Uint32 desc = attrPtr.p->attributeDescriptor;
    const Uint32 attrType = AttributeDescriptor::getType(desc);
    const Uint32 attrSize = AttributeDescriptor::getSize(desc);
    const Uint32 arraySize = AttributeDescriptor::getArraySize(desc);
    const Uint32 arrayType = AttributeDescriptor::getArrayType(desc);
    const Uint32 nullable = AttributeDescriptor::getNullable(desc);
    const Uint32 DKey = AttributeDescriptor::getDKey(desc);
    const Uint32 disk= AttributeDescriptor::getDiskBased(desc);
    const Uint32 dynamic= AttributeDescriptor::getDynamic(desc);


    // AttributeType deprecated
    w.add(DictTabInfo::AttributeSize, attrSize);
    w.add(DictTabInfo::AttributeArraySize, arraySize);
    w.add(DictTabInfo::AttributeArrayType, arrayType);
    w.add(DictTabInfo::AttributeNullableFlag, nullable);
    w.add(DictTabInfo::AttributeDynamic, dynamic);
    w.add(DictTabInfo::AttributeDKey, DKey);
    w.add(DictTabInfo::AttributeExtType, attrType);
    w.add(DictTabInfo::AttributeExtPrecision, attrPtr.p->extPrecision);
    w.add(DictTabInfo::AttributeExtScale, attrPtr.p->extScale);
    w.add(DictTabInfo::AttributeExtLength, attrPtr.p->extLength);
    w.add(DictTabInfo::AttributeAutoIncrement,
	  (Uint32)attrPtr.p->autoIncrement);

    if(disk)
      w.add(DictTabInfo::AttributeStorageType, (Uint32)NDB_STORAGETYPE_DISK);
    else
      w.add(DictTabInfo::AttributeStorageType, (Uint32)NDB_STORAGETYPE_MEMORY);

    ConstRope def(c_rope_pool, attrPtr.p->defaultValue);
    def.copy(defaultValue);

    if (def.size())
    {
      /* Convert default value to network byte order for storage */
      /* Attribute header */
      ndbrequire(def.size() >= sizeof(Uint32));
      Uint32 a;
      memcpy(&a, defaultValue, sizeof(Uint32));
      a = htonl(a);
      memcpy(defaultValue, &a, sizeof(Uint32));

      Uint32 remainBytes = def.size() - sizeof(Uint32);

      if (remainBytes)
        NdbSqlUtil::convertByteOrder(attrType,
                                     attrSize,
                                     arrayType,
                                     arraySize,
                                     (uchar*) defaultValue + sizeof(Uint32),
                                     remainBytes);
    }

    w.add(DictTabInfo::AttributeDefaultValueLen, def.size());
    w.add(DictTabInfo::AttributeDefaultValue, defaultValue, def.size());
    w.add(DictTabInfo::AttributeEnd, 1);
  }

  w.add(DictTabInfo::TableEnd, 1);
}

void
Dbdict::packFilegroupIntoPages(SimpleProperties::Writer & w,
			       FilegroupPtr fg_ptr,
			       const Uint32 undo_free_hi,
			       const Uint32 undo_free_lo){

  DictFilegroupInfo::Filegroup fg; fg.init();
  ConstRope r(c_rope_pool, fg_ptr.p->m_name);
  r.copy(fg.FilegroupName);

  fg.FilegroupId = fg_ptr.p->key;
  fg.FilegroupType = fg_ptr.p->m_type;
  fg.FilegroupVersion = fg_ptr.p->m_version;

  switch(fg.FilegroupType){
  case DictTabInfo::Tablespace:
    //fg.TS_DataGrow = group.m_grow_spec;
    fg.TS_ExtentSize = fg_ptr.p->m_tablespace.m_extent_size;
    fg.TS_LogfileGroupId = fg_ptr.p->m_tablespace.m_default_logfile_group_id;
    FilegroupPtr lfg_ptr;
    ndbrequire(find_object(lfg_ptr, fg.TS_LogfileGroupId));
    fg.TS_LogfileGroupVersion = lfg_ptr.p->m_version;
    break;
  case DictTabInfo::LogfileGroup:
    fg.LF_UndoBufferSize = fg_ptr.p->m_logfilegroup.m_undo_buffer_size;
    fg.LF_UndoFreeWordsHi= undo_free_hi;
    fg.LF_UndoFreeWordsLo= undo_free_lo;
    //fg.LF_UndoGrow = ;
    break;
  default:
    ndbrequire(false);
  }

  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w,
			     &fg,
			     DictFilegroupInfo::Mapping,
			     DictFilegroupInfo::MappingSize, true);

  ndbrequire(s == SimpleProperties::Eof);
}

void
Dbdict::packFileIntoPages(SimpleProperties::Writer & w,
			  FilePtr f_ptr, const Uint32 free_extents){

  DictFilegroupInfo::File f; f.init();
  ConstRope r(c_rope_pool, f_ptr.p->m_path);
  r.copy(f.FileName);

  f.FileType = f_ptr.p->m_type;
  f.FilegroupId = f_ptr.p->m_filegroup_id;; //group.m_id;
  f.FileSizeHi = (Uint32)(f_ptr.p->m_file_size >> 32);
  f.FileSizeLo = (Uint32)(f_ptr.p->m_file_size & 0xFFFFFFFF);
  f.FileFreeExtents= free_extents;
  f.FileId =  f_ptr.p->key;
  f.FileVersion = f_ptr.p->m_version;

  FilegroupPtr lfg_ptr;
  ndbrequire(find_object(lfg_ptr, f.FilegroupId));
  f.FilegroupVersion = lfg_ptr.p->m_version;

  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w,
			     &f,
			     DictFilegroupInfo::FileMapping,
			     DictFilegroupInfo::FileMappingSize, true);

  ndbrequire(s == SimpleProperties::Eof);
}

void
Dbdict::execCREATE_FRAGMENTATION_REQ(Signal* signal)
{
  CreateFragmentationReq* req = (CreateFragmentationReq*)signal->getDataPtr();

  if (req->primaryTableId == RNIL) {
    jam();
    EXECUTE_DIRECT(DBDIH, GSN_CREATE_FRAGMENTATION_REQ, signal,
                   CreateFragmentationReq::SignalLength);
    return;
  }

  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  SchemaFile::TableEntry * te = getTableEntry(xsf, req->primaryTableId);
  if (te->m_tableState != SchemaFile::SF_CREATE)
  {
    jam();
    if (req->requestInfo == 0)
    {
      jam();
      req->requestInfo |= CreateFragmentationReq::RI_GET_FRAGMENTATION;
    }
    EXECUTE_DIRECT(DBDIH, GSN_CREATE_FRAGMENTATION_REQ, signal,
                   CreateFragmentationReq::SignalLength);
    return;
  }

  DictObjectPtr obj_ptr;
  TableRecordPtr tablePtr;
  bool ok = find_object(obj_ptr, tablePtr, req->primaryTableId);
  ndbrequire(ok);
  SchemaOpPtr op_ptr;
  findDictObjectOp(op_ptr, obj_ptr);
  ndbrequire(!op_ptr.isNull());
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(memcmp(oprec_ptr.p->m_opType, "CTa", 4) == 0);

  Uint32 *theData = &signal->theData[0];
  const OpSection& fragSection =
    getOpSection(op_ptr, CreateTabReq::FRAGMENTATION);
  LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena,c_opSectionBufferPool);
  copyOut(op_sec_pool, fragSection, &theData[25], ZNIL);
  theData[0] = 0;
}

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
// The routines to handle responses from file system.
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
// A file was successfully closed.
/* ---------------------------------------------------------------- */
void Dbdict::execFSCLOSECONF(Signal* signal)
{
  FsConnectRecordPtr fsPtr;
  FsConf * const fsConf = (FsConf *)&signal->theData[0];
  jamEntry();
  c_fsConnectRecordPool.getPtr(fsPtr, fsConf->userPointer);
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::CLOSE_WRITE_SCHEMA:
    jam();
    closeWriteSchemaConf(signal, fsPtr);
    break;
  case FsConnectRecord::CLOSE_READ_SCHEMA:
    jam();
    closeReadSchemaConf(signal, fsPtr);
    break;
  case FsConnectRecord::CLOSE_READ_TAB_FILE:
    jam();
    closeReadTableConf(signal, fsPtr);
    break;
  case FsConnectRecord::CLOSE_WRITE_TAB_FILE:
    jam();
    closeWriteTableConf(signal, fsPtr);
    break;
  case FsConnectRecord::OPEN_READ_SCHEMA2:
    openSchemaFile(signal, 1, fsPtr.i, false, false);
    break;
  case FsConnectRecord::OPEN_READ_TAB_FILE2:
    openTableFile(signal, 1, fsPtr.i, c_readTableRecord.tableId, false);
    break;
  default:
    jamLine((fsPtr.p->fsState & 0xFFF));
    ndbrequire(false);
    break;
  }//switch
}//execFSCLOSECONF()


/* ---------------------------------------------------------------- */
// A file was successfully opened.
/* ---------------------------------------------------------------- */
void Dbdict::execFSOPENCONF(Signal* signal)
{
  FsConnectRecordPtr fsPtr;
  jamEntry();
  FsConf * const fsConf = (FsConf *)&signal->theData[0];
  c_fsConnectRecordPool.getPtr(fsPtr, fsConf->userPointer);

  Uint32 filePointer = fsConf->filePointer;
  fsPtr.p->filePtr = filePointer;
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::OPEN_WRITE_SCHEMA:
    jam();
    fsPtr.p->fsState = FsConnectRecord::WRITE_SCHEMA;
    writeSchemaFile(signal, filePointer, fsPtr.i);
    break;
  case FsConnectRecord::OPEN_READ_SCHEMA1:
    jam();
    fsPtr.p->fsState = FsConnectRecord::READ_SCHEMA1;
    readSchemaFile(signal, filePointer, fsPtr.i);
    break;
  case FsConnectRecord::OPEN_READ_SCHEMA2:
    jam();
    fsPtr.p->fsState = FsConnectRecord::READ_SCHEMA2;
    readSchemaFile(signal, filePointer, fsPtr.i);
    break;
  case FsConnectRecord::OPEN_READ_TAB_FILE1:
    jam();
    fsPtr.p->fsState = FsConnectRecord::READ_TAB_FILE1;
    readTableFile(signal, filePointer, fsPtr.i);
    break;
  case FsConnectRecord::OPEN_READ_TAB_FILE2:
    jam();
    fsPtr.p->fsState = FsConnectRecord::READ_TAB_FILE2;
    readTableFile(signal, filePointer, fsPtr.i);
    break;
  case FsConnectRecord::OPEN_WRITE_TAB_FILE:
    jam();
    fsPtr.p->fsState = FsConnectRecord::WRITE_TAB_FILE;
    writeTableFile(signal, filePointer, fsPtr.i);
    break;
  default:
    jamLine((fsPtr.p->fsState & 0xFFF));
    ndbrequire(false);
    break;
  }//switch
}//execFSOPENCONF()

/* ---------------------------------------------------------------- */
// An open file was refused.
/* ---------------------------------------------------------------- */
void Dbdict::execFSOPENREF(Signal* signal)
{
  jamEntry();
  FsRef * const fsRef = (FsRef *)&signal->theData[0];
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, fsRef->userPointer);
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::OPEN_READ_SCHEMA1:
    jam();
    openReadSchemaRef(signal, fsPtr);
    return;
  case FsConnectRecord::OPEN_READ_TAB_FILE1:
    jam();
    openReadTableRef(signal, fsPtr);
    return;
  default:
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system open failed during FsConnectRecord state %d", (Uint32)fsPtr.p->fsState);
    fsRefError(signal,__LINE__,msg);
  }
}//execFSOPENREF()

/* ---------------------------------------------------------------- */
// A file was successfully read.
/* ---------------------------------------------------------------- */
void Dbdict::execFSREADCONF(Signal* signal)
{
  jamEntry();
  FsConf * const fsConf = (FsConf *)&signal->theData[0];
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, fsConf->userPointer);
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::READ_SCHEMA1:
  case FsConnectRecord::READ_SCHEMA2:
    readSchemaConf(signal ,fsPtr);
    break;
  case FsConnectRecord::READ_TAB_FILE1:
    if(ERROR_INSERTED(6024))
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      FsRef * const fsRef = (FsRef *)&signal->theData[0];
      fsRef->userPointer = fsConf->userPointer;
      fsRef->setErrorCode(fsRef->errorCode, NDBD_EXIT_AFS_UNKNOWN);
      fsRef->osErrorCode = ~0; // Indicate local error
      execFSREADREF(signal);
      return;
    }//Testing how DICT behave if read of file 1 fails (Bug#28770)
  case FsConnectRecord::READ_TAB_FILE2:
    jam();
    readTableConf(signal ,fsPtr);
    break;
  default:
    jamLine((fsPtr.p->fsState & 0xFFF));
    ndbrequire(false);
    break;
  }//switch
}//execFSREADCONF()

/* ---------------------------------------------------------------- */
// A read file was refused.
/* ---------------------------------------------------------------- */
void Dbdict::execFSREADREF(Signal* signal)
{
  jamEntry();
  FsRef * const fsRef = (FsRef *)&signal->theData[0];
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, fsRef->userPointer);
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::READ_SCHEMA1:
    jam();
    readSchemaRef(signal, fsPtr);
    return;
  case FsConnectRecord::READ_TAB_FILE1:
    jam();
    readTableRef(signal, fsPtr);
    return;
  default:
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system read failed during FsConnectRecord state %d", (Uint32)fsPtr.p->fsState);
    fsRefError(signal,__LINE__,msg);
  }
}//execFSREADREF()

/* ---------------------------------------------------------------- */
// A file was successfully written.
/* ---------------------------------------------------------------- */
void Dbdict::execFSWRITECONF(Signal* signal)
{
  FsConf * const fsConf = (FsConf *)&signal->theData[0];
  FsConnectRecordPtr fsPtr;
  jamEntry();
  c_fsConnectRecordPool.getPtr(fsPtr, fsConf->userPointer);
  switch (fsPtr.p->fsState) {
  case FsConnectRecord::WRITE_TAB_FILE:
    writeTableConf(signal, fsPtr);
    break;
  case FsConnectRecord::WRITE_SCHEMA:
    jam();
    writeSchemaConf(signal, fsPtr);
    break;
  default:
    jamLine((fsPtr.p->fsState & 0xFFF));
    ndbrequire(false);
    break;
  }//switch
}//execFSWRITECONF()


/* ---------------------------------------------------------------- */
// Routines to handle Read/Write of Table Files
/* ---------------------------------------------------------------- */
void
Dbdict::writeTableFile(Signal* signal, Uint32 tableId,
		       SegmentedSectionPtr tabInfoPtr, Callback* callback){

  ndbrequire(c_writeTableRecord.tableWriteState == WriteTableRecord::IDLE);

  Uint32 pages = WORDS2PAGES(tabInfoPtr.sz);
  c_writeTableRecord.no_of_words = tabInfoPtr.sz;
  c_writeTableRecord.tableWriteState = WriteTableRecord::TWR_CALLBACK;
  c_writeTableRecord.m_callback = * callback;

  c_writeTableRecord.pageId = 0;
  ndbrequire(pages == 1);

  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_writeTableRecord.pageId);
  copy(&pageRecPtr.p->word[ZPAGE_HEADER_SIZE], tabInfoPtr);

  memset(&pageRecPtr.p->word[0], 0, 4 * ZPAGE_HEADER_SIZE);
  pageRecPtr.p->word[ZPOS_CHECKSUM] =
    computeChecksum(&pageRecPtr.p->word[0],
		    pages * ZSIZE_OF_PAGES_IN_WORDS);

  startWriteTableFile(signal, tableId);
}

// SchemaTrans variant
void
Dbdict::writeTableFile(Signal* signal, SchemaOpPtr op_ptr, Uint32 tableId,
                       OpSection tabInfoSec, Callback* callback)
{
  ndbrequire(c_writeTableRecord.tableWriteState == WriteTableRecord::IDLE);

  {
    const Uint32 size = tabInfoSec.getSize();
    const Uint32 pages = WORDS2PAGES(size);

    c_writeTableRecord.no_of_words = size;
    c_writeTableRecord.tableWriteState = WriteTableRecord::TWR_CALLBACK;
    c_writeTableRecord.m_callback = * callback;

    c_writeTableRecord.pageId = 0;

    PageRecordPtr pageRecPtr;
    c_pageRecordArray.getPtr(pageRecPtr, c_writeTableRecord.pageId);

    Uint32* dst = &pageRecPtr.p->word[ZPAGE_HEADER_SIZE];
    Uint32 dstSize = (ZMAX_PAGES_OF_TABLE_DEFINITION * ZSIZE_OF_PAGES_IN_WORDS)
      - ZPAGE_HEADER_SIZE;
    LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
    bool ok = copyOut(op_sec_pool, tabInfoSec, dst, dstSize);
    ndbrequire(ok);

    memset(&pageRecPtr.p->word[0], 0, 4 * ZPAGE_HEADER_SIZE);
    pageRecPtr.p->word[ZPOS_CHECKSUM] =
      computeChecksum(&pageRecPtr.p->word[0],
                      pages * ZSIZE_OF_PAGES_IN_WORDS);
  }

  startWriteTableFile(signal, tableId);
}

void Dbdict::startWriteTableFile(Signal* signal, Uint32 tableId)
{
  FsConnectRecordPtr fsPtr;
  c_writeTableRecord.tableId = tableId;
  c_fsConnectRecordPool.getPtr(fsPtr, getFsConnRecord());
  fsPtr.p->fsState = FsConnectRecord::OPEN_WRITE_TAB_FILE;
  openTableFile(signal, 0, fsPtr.i, tableId, true);
  c_writeTableRecord.noOfTableFilesHandled = 0;
}//Dbdict::startWriteTableFile()

void Dbdict::openTableFile(Signal* signal,
                           Uint32 fileNo,
                           Uint32 fsConPtr,
                           Uint32 tableId,
                           bool   writeFlag)
{
  FsOpenReq * const fsOpenReq = (FsOpenReq *)&signal->theData[0];

  fsOpenReq->userReference = reference();
  fsOpenReq->userPointer = fsConPtr;
  if (writeFlag) {
    jam();
    fsOpenReq->fileFlags =
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_TRUNCATE |
      FsOpenReq::OM_CREATE |
      FsOpenReq::OM_SYNC;
  } else {
    jam();
    fsOpenReq->fileFlags = FsOpenReq::OM_READONLY;
  }//if
  fsOpenReq->fileNumber[3] = 0; // Initialise before byte changes
  FsOpenReq::setVersion(fsOpenReq->fileNumber, 1);
  FsOpenReq::setSuffix(fsOpenReq->fileNumber, FsOpenReq::S_TABLELIST);
  FsOpenReq::v1_setDisk(fsOpenReq->fileNumber, (fileNo + 1));
  FsOpenReq::v1_setTable(fsOpenReq->fileNumber, tableId);
  FsOpenReq::v1_setFragment(fsOpenReq->fileNumber, (Uint32)-1);
  FsOpenReq::v1_setS(fsOpenReq->fileNumber, 0);
  FsOpenReq::v1_setP(fsOpenReq->fileNumber, 255);
/* ---------------------------------------------------------------- */
// File name : D1/DBDICT/T0/S1.TableList
// D1 means Disk 1 (set by fileNo + 1)
// T0 means table id = 0
// S1 means tableVersion 1
// TableList indicates that this is a file for a table description.
/* ---------------------------------------------------------------- */
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}//openTableFile()

void Dbdict::writeTableFile(Signal* signal, Uint32 filePtr, Uint32 fsConPtr)
{
  FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)&signal->theData[0];

  fsRWReq->filePointer = filePtr;
  fsRWReq->userReference = reference();
  fsRWReq->userPointer = fsConPtr;
  fsRWReq->operationFlag = 0; // Initialise before bit changes
  FsReadWriteReq::setSyncFlag(fsRWReq->operationFlag, 1);
  FsReadWriteReq::setFormatFlag(fsRWReq->operationFlag,
                                FsReadWriteReq::fsFormatArrayOfPages);
  fsRWReq->varIndex = ZBAT_TABLE_FILE;
  fsRWReq->numberOfPages = WORDS2PAGES(c_writeTableRecord.no_of_words);
  fsRWReq->data.arrayOfPages.varIndex = c_writeTableRecord.pageId;
  fsRWReq->data.arrayOfPages.fileOffset = 0; // Write to file page 0
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//writeTableFile()

void Dbdict::writeTableConf(Signal* signal,
                                FsConnectRecordPtr fsPtr)
{
  fsPtr.p->fsState = FsConnectRecord::CLOSE_WRITE_TAB_FILE;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}//Dbdict::writeTableConf()

void Dbdict::closeWriteTableConf(Signal* signal,
                                 FsConnectRecordPtr fsPtr)
{
  c_writeTableRecord.noOfTableFilesHandled++;
  if (c_writeTableRecord.noOfTableFilesHandled < 2) {
    jam();
    fsPtr.p->fsState = FsConnectRecord::OPEN_WRITE_TAB_FILE;
    openTableFile(signal, 1, fsPtr.i, c_writeTableRecord.tableId, true);
    return;
  }
  ndbrequire(c_writeTableRecord.noOfTableFilesHandled == 2);
  c_fsConnectRecordPool.release(fsPtr);
  WriteTableRecord::TableWriteState state = c_writeTableRecord.tableWriteState;
  c_writeTableRecord.tableWriteState = WriteTableRecord::IDLE;
  switch (state) {
  case WriteTableRecord::IDLE:
  case WriteTableRecord::WRITE_ADD_TABLE_MASTER :
  case WriteTableRecord::WRITE_ADD_TABLE_SLAVE :
  case WriteTableRecord::WRITE_RESTART_FROM_MASTER :
  case WriteTableRecord::WRITE_RESTART_FROM_OWN :
    ndbrequire(false);
    break;
  case WriteTableRecord::TWR_CALLBACK:
    jam();
    execute(signal, c_writeTableRecord.m_callback, 0);
    return;
  }
  ndbrequire(false);
}//Dbdict::closeWriteTableConf()

void Dbdict::startReadTableFile(Signal* signal, Uint32 tableId)
{
  //globalSignalLoggers.log(number(), "startReadTableFile");
  ndbrequire(!c_readTableRecord.inUse);

  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, getFsConnRecord());
  c_readTableRecord.inUse = true;
  c_readTableRecord.tableId = tableId;
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_TAB_FILE1;
  openTableFile(signal, 0, fsPtr.i, tableId, false);
}//Dbdict::startReadTableFile()

void Dbdict::openReadTableRef(Signal* signal,
                              FsConnectRecordPtr fsPtr)
{
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_TAB_FILE2;
  openTableFile(signal, 1, fsPtr.i, c_readTableRecord.tableId, false);
  return;
}//Dbdict::openReadTableConf()

void Dbdict::readTableFile(Signal* signal, Uint32 filePtr, Uint32 fsConPtr)
{
  FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)&signal->theData[0];

  fsRWReq->filePointer = filePtr;
  fsRWReq->userReference = reference();
  fsRWReq->userPointer = fsConPtr;
  fsRWReq->operationFlag = 0; // Initialise before bit changes
  FsReadWriteReq::setSyncFlag(fsRWReq->operationFlag, 0);
  FsReadWriteReq::setFormatFlag(fsRWReq->operationFlag,
                                FsReadWriteReq::fsFormatArrayOfPages);
  fsRWReq->varIndex = ZBAT_TABLE_FILE;
  fsRWReq->numberOfPages = WORDS2PAGES(c_readTableRecord.no_of_words);
  fsRWReq->data.arrayOfPages.varIndex = c_readTableRecord.pageId;
  fsRWReq->data.arrayOfPages.fileOffset = 0; // Write to file page 0
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//readTableFile()

void Dbdict::readTableConf(Signal* signal,
			   FsConnectRecordPtr fsPtr)
{
  /* ---------------------------------------------------------------- */
  // Verify the data read from disk
  /* ---------------------------------------------------------------- */
  bool crashInd;
  if (fsPtr.p->fsState == FsConnectRecord::READ_TAB_FILE1) {
    jam();
    crashInd = false;
  } else {
    jam();
    crashInd = true;
  }//if

  PageRecordPtr tmpPagePtr;
  c_pageRecordArray.getPtr(tmpPagePtr, c_readTableRecord.pageId);
  Uint32 sz =
    WORDS2PAGES(c_readTableRecord.no_of_words)*ZSIZE_OF_PAGES_IN_WORDS;
  Uint32 chk = computeChecksum((const Uint32*)tmpPagePtr.p, sz);

  ndbrequire((chk == 0) || !crashInd);
  if(chk != 0){
    jam();
    ndbrequire(fsPtr.p->fsState == FsConnectRecord::READ_TAB_FILE1);
    readTableRef(signal, fsPtr);
    return;
  }//if

  fsPtr.p->fsState = FsConnectRecord::CLOSE_READ_TAB_FILE;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}//Dbdict::readTableConf()

void Dbdict::readTableRef(Signal* signal,
                          FsConnectRecordPtr fsPtr)
{
  /**
   * First close corrupt file
   */
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_TAB_FILE2;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}//Dbdict::readTableRef()

void Dbdict::closeReadTableConf(Signal* signal,
                                FsConnectRecordPtr fsPtr)
{
  c_fsConnectRecordPool.release(fsPtr);
  c_readTableRecord.inUse = false;

  execute(signal, c_readTableRecord.m_callback, 0);
  return;
}//Dbdict::closeReadTableConf()

/* ---------------------------------------------------------------- */
// Routines to handle Read/Write of Schema Files
/* ---------------------------------------------------------------- */
NdbOut& operator<<(NdbOut& out, const SchemaFile::TableEntry entry);

void
Dbdict::updateSchemaState(Signal* signal, Uint32 tableId,
			  SchemaFile::TableEntry* te, Callback* callback,
                          bool savetodisk, bool dicttrans)
{
  jam();
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);

  if (!dicttrans) {
    /*
     * Old code may not zero transId, leaving the entry not visible
     * to tabinfo request (old code may also skip updateSchemaState).
   */
    te->m_transId = 0;
  }

  D("updateSchemaState" << V(tableId));
  D("old:" << *tableEntry);
  D("new:" << *te);

#ifndef TODO
  * tableEntry = * te;
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);
#else
  SchemaFile::TableState newState =
    (SchemaFile::TableState)te->m_tableState;
  SchemaFile::TableState oldState =
    (SchemaFile::TableState)tableEntry->m_tableState;

  Uint32 newVersion = te->m_tableVersion;
  Uint32 oldVersion = tableEntry->m_tableVersion;

  bool ok = false;
  switch (newState){
  case SchemaFile::CREATE_PARSED:
    jam();
    ok = true;
    ndbrequire(create_obj_inc_schema_version(oldVersion) == newVersion);
    ndbrequire(oldState == SchemaFile::INIT ||
               oldState == SchemaFile::DROP_TABLE_COMMITTED);
    break;
  case SchemaFile::DROP_PARSED:
    ok = true;
    break;
  case SchemaFile::ALTER_PARSED:
    // wl3600_todo
    ok = true;
    break;
  case SchemaFile::ADD_STARTED:
    jam();
    ok = true;
    if (dicttrans) {
      ndbrequire(oldVersion == newVersion);
      ndbrequire(oldState == SchemaFile::CREATE_PARSED);
      break;
    }
    ndbrequire(create_obj_inc_schema_version(oldVersion) == newVersion);
    ndbrequire(oldState == SchemaFile::INIT ||
	       oldState == SchemaFile::DROP_TABLE_COMMITTED);
    break;
  case SchemaFile::TABLE_ADD_COMMITTED:
    jam();
    ok = true;
    ndbrequire(newVersion == oldVersion);
    ndbrequire(oldState == SchemaFile::ADD_STARTED ||
	       oldState == SchemaFile::DROP_TABLE_STARTED);
    break;
  case SchemaFile::ALTER_TABLE_COMMITTED:
    jam();
    ok = true;
    ndbrequire(alter_obj_inc_schema_version(oldVersion) == newVersion);
    ndbrequire(oldState == SchemaFile::TABLE_ADD_COMMITTED ||
	       oldState == SchemaFile::ALTER_TABLE_COMMITTED);
    break;
  case SchemaFile::DROP_TABLE_STARTED:
    jam();
  case SchemaFile::DROP_TABLE_COMMITTED:
    jam();
    ok = true;
    break;
  case SchemaFile::TEMPORARY_TABLE_COMMITTED:
    jam();
    ndbrequire(oldState == SchemaFile::ADD_STARTED ||
               oldState == SchemaFile::TEMPORARY_TABLE_COMMITTED);
    ok = true;
    break;
  case SchemaFile::INIT:
    jam();
    ok = true;
    if (dicttrans) {
      ndbrequire((oldState == SchemaFile::CREATE_PARSED));
      break;
    }
    ndbrequire((oldState == SchemaFile::ADD_STARTED));
  }//if
  ndbrequire(ok);

  * tableEntry = * te;
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);

  if (savetodisk)
  {
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;

    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.newFile = false;
    c_writeSchemaRecord.firstPage = tableId / NDB_SF_PAGE_ENTRIES;
    c_writeSchemaRecord.noOfPages = 1;
    c_writeSchemaRecord.m_callback = * callback;

    startWriteSchemaFile(signal);
  }
  else
  {
    jam();
    if (callback != 0) {
      jam();
      execute(signal, *callback, 0);
    }
  }
#endif
}

void Dbdict::startWriteSchemaFile(Signal* signal)
{
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, getFsConnRecord());
  fsPtr.p->fsState = FsConnectRecord::OPEN_WRITE_SCHEMA;
  openSchemaFile(signal, 0, fsPtr.i, true, c_writeSchemaRecord.newFile);
  c_writeSchemaRecord.noOfSchemaFilesHandled = 0;
}//Dbdict::startWriteSchemaFile()

void Dbdict::openSchemaFile(Signal* signal,
                            Uint32 fileNo,
                            Uint32 fsConPtr,
                            bool writeFlag,
                            bool newFile)
{
  FsOpenReq * const fsOpenReq = (FsOpenReq *)&signal->theData[0];
  fsOpenReq->userReference = reference();
  fsOpenReq->userPointer = fsConPtr;
  if (writeFlag) {
    jam();
    fsOpenReq->fileFlags =
      FsOpenReq::OM_WRITEONLY |
      FsOpenReq::OM_SYNC;
    if (newFile)
      fsOpenReq->fileFlags |=
        FsOpenReq::OM_TRUNCATE |
        FsOpenReq::OM_CREATE;
  } else {
    jam();
    fsOpenReq->fileFlags = FsOpenReq::OM_READONLY;
  }//if
  fsOpenReq->fileNumber[3] = 0; // Initialise before byte changes
  FsOpenReq::setVersion(fsOpenReq->fileNumber, 1);
  FsOpenReq::setSuffix(fsOpenReq->fileNumber, FsOpenReq::S_SCHEMALOG);
  FsOpenReq::v1_setDisk(fsOpenReq->fileNumber, (fileNo + 1));
  FsOpenReq::v1_setTable(fsOpenReq->fileNumber, (Uint32)-1);
  FsOpenReq::v1_setFragment(fsOpenReq->fileNumber, (Uint32)-1);
  FsOpenReq::v1_setS(fsOpenReq->fileNumber, (Uint32)-1);
  FsOpenReq::v1_setP(fsOpenReq->fileNumber, 0);
/* ---------------------------------------------------------------- */
// File name : D1/DBDICT/P0.SchemaLog
// D1 means Disk 1 (set by fileNo + 1). Writes to both D1 and D2
// SchemaLog indicates that this is a file giving a list of current tables.
/* ---------------------------------------------------------------- */
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}//openSchemaFile()

void Dbdict::writeSchemaFile(Signal* signal, Uint32 filePtr, Uint32 fsConPtr)
{
  FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)&signal->theData[0];

  // check write record
  WriteSchemaRecord & wr = c_writeSchemaRecord;
  ndbrequire(wr.pageId == (wr.pageId != 0) * NDB_SF_MAX_PAGES);
  ndbrequire(wr.noOfPages != 0);
  ndbrequire(wr.firstPage + wr.noOfPages <= NDB_SF_MAX_PAGES);

  fsRWReq->filePointer = filePtr;
  fsRWReq->userReference = reference();
  fsRWReq->userPointer = fsConPtr;
  fsRWReq->operationFlag = 0; // Initialise before bit changes
  FsReadWriteReq::setSyncFlag(fsRWReq->operationFlag, 1);
  FsReadWriteReq::setFormatFlag(fsRWReq->operationFlag,
                                FsReadWriteReq::fsFormatArrayOfPages);
  fsRWReq->varIndex = ZBAT_SCHEMA_FILE;
  fsRWReq->numberOfPages = wr.noOfPages;
  // Write from memory page
  fsRWReq->data.arrayOfPages.varIndex = wr.pageId + wr.firstPage;
  fsRWReq->data.arrayOfPages.fileOffset = wr.firstPage;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//writeSchemaFile()

void Dbdict::writeSchemaConf(Signal* signal,
                                FsConnectRecordPtr fsPtr)
{
  fsPtr.p->fsState = FsConnectRecord::CLOSE_WRITE_SCHEMA;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}//Dbdict::writeSchemaConf()

void Dbdict::closeFile(Signal* signal, Uint32 filePtr, Uint32 fsConPtr)
{
  FsCloseReq * const fsCloseReq = (FsCloseReq *)&signal->theData[0];
  fsCloseReq->filePointer = filePtr;
  fsCloseReq->userReference = reference();
  fsCloseReq->userPointer = fsConPtr;
  FsCloseReq::setRemoveFileFlag(fsCloseReq->fileFlag, false);
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
  return;
}//closeFile()

void Dbdict::closeWriteSchemaConf(Signal* signal,
                                     FsConnectRecordPtr fsPtr)
{
  c_writeSchemaRecord.noOfSchemaFilesHandled++;
  if (c_writeSchemaRecord.noOfSchemaFilesHandled < 2) {
    jam();
    fsPtr.p->fsState = FsConnectRecord::OPEN_WRITE_SCHEMA;
    openSchemaFile(signal, 1, fsPtr.i, true, c_writeSchemaRecord.newFile);
    return;
  }
  ndbrequire(c_writeSchemaRecord.noOfSchemaFilesHandled == 2);

  c_fsConnectRecordPool.release(fsPtr);

  c_writeSchemaRecord.inUse = false;
  execute(signal, c_writeSchemaRecord.m_callback, 0);
  return;
}//Dbdict::closeWriteSchemaConf()

void Dbdict::startReadSchemaFile(Signal* signal)
{
  //globalSignalLoggers.log(number(), "startReadSchemaFile");
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.getPtr(fsPtr, getFsConnRecord());
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_SCHEMA1;
  openSchemaFile(signal, 0, fsPtr.i, false, false);
}//Dbdict::startReadSchemaFile()

void Dbdict::openReadSchemaRef(Signal* signal,
                               FsConnectRecordPtr fsPtr)
{
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_SCHEMA2;
  openSchemaFile(signal, 1, fsPtr.i, false, false);
}//Dbdict::openReadSchemaRef()

void Dbdict::readSchemaFile(Signal* signal, Uint32 filePtr, Uint32 fsConPtr)
{
  FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)&signal->theData[0];

  // check read record
  ReadSchemaRecord & rr = c_readSchemaRecord;
  ndbrequire(rr.pageId == (rr.pageId != 0) * NDB_SF_MAX_PAGES);
  ndbrequire(rr.noOfPages != 0);
  ndbrequire(rr.firstPage + rr.noOfPages <= NDB_SF_MAX_PAGES);

  fsRWReq->filePointer = filePtr;
  fsRWReq->userReference = reference();
  fsRWReq->userPointer = fsConPtr;
  fsRWReq->operationFlag = 0; // Initialise before bit changes
  FsReadWriteReq::setSyncFlag(fsRWReq->operationFlag, 0);
  FsReadWriteReq::setFormatFlag(fsRWReq->operationFlag,
                                FsReadWriteReq::fsFormatArrayOfPages);
  fsRWReq->varIndex = ZBAT_SCHEMA_FILE;
  fsRWReq->numberOfPages = rr.noOfPages;
  fsRWReq->data.arrayOfPages.varIndex = rr.pageId + rr.firstPage;
  fsRWReq->data.arrayOfPages.fileOffset = rr.firstPage;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//readSchemaFile()

void Dbdict::readSchemaConf(Signal* signal,
                            FsConnectRecordPtr fsPtr)
{
/* ---------------------------------------------------------------- */
// Verify the data read from disk
/* ---------------------------------------------------------------- */
  bool crashInd;
  if (fsPtr.p->fsState == FsConnectRecord::READ_SCHEMA1) {
    jam();
    crashInd = false;
  } else {
    jam();
    crashInd = true;
  }//if

  ReadSchemaRecord & rr = c_readSchemaRecord;
  XSchemaFile * xsf = &c_schemaFile[rr.pageId != 0];

  if (rr.schemaReadState == ReadSchemaRecord::INITIAL_READ_HEAD) {
    jam();
    ndbrequire(rr.firstPage == 0);
    SchemaFile * sf = &xsf->schemaPage[0];
    Uint32 noOfPages;
    if (sf->NdbVersion < NDB_SF_VERSION_5_0_6) {
      jam();
      const Uint32 pageSize_old = 32 * 1024;
      noOfPages = pageSize_old / NDB_SF_PAGE_SIZE - 1;
    } else {
      noOfPages = sf->FileSize / NDB_SF_PAGE_SIZE - 1;
    }
    rr.schemaReadState = ReadSchemaRecord::INITIAL_READ;
    if (noOfPages != 0) {
      rr.firstPage = 1;
      rr.noOfPages = noOfPages;
      readSchemaFile(signal, fsPtr.p->filePtr, fsPtr.i);
      return;
    }
  }

  SchemaFile * sf0 = &xsf->schemaPage[0];
  xsf->noOfPages = sf0->FileSize / NDB_SF_PAGE_SIZE;

  if (sf0->NdbVersion < NDB_SF_VERSION_5_0_6 &&
      ! convertSchemaFileTo_5_0_6(xsf)) {
    jam();
    ndbrequire(! crashInd);
    ndbrequire(fsPtr.p->fsState == FsConnectRecord::READ_SCHEMA1);
    readSchemaRef(signal, fsPtr);
    return;
  }

  if (sf0->NdbVersion < NDB_MAKE_VERSION(6,4,0) &&
      ! convertSchemaFileTo_6_4(xsf))
  {
    jam();
    ndbrequire(! crashInd);
    ndbrequire(fsPtr.p->fsState == FsConnectRecord::READ_SCHEMA1);
    readSchemaRef(signal, fsPtr);
    return;
  }


  for (Uint32 n = 0; n < xsf->noOfPages; n++) {
    SchemaFile * sf = &xsf->schemaPage[n];
    bool ok = false;
    const char *reason;
    if (memcmp(sf->Magic, NDB_SF_MAGIC, sizeof(sf->Magic)) != 0)
    { jam(); reason = "magic code"; }
    else if (sf->FileSize == 0)
    { jam(); reason = "file size == 0"; }
    else if (sf->FileSize % NDB_SF_PAGE_SIZE != 0)
    { jam(); reason = "invalid size multiple"; }
    else if (sf->FileSize != sf0->FileSize)
    { jam(); reason = "invalid size"; }
    else if (sf->PageNumber != n)
    { jam(); reason = "invalid page number"; }
    else if (computeChecksum((Uint32*)sf, NDB_SF_PAGE_SIZE_IN_WORDS) != 0)
    { jam(); reason = "invalid checksum"; }
    else
      ok = true;

    if (!ok)
    {
      char reason_msg[128];
      BaseString::snprintf(reason_msg, sizeof(reason_msg),
               "schema file corrupt, page %u (%s, "
               "sz=%u sz0=%u pn=%u)",
               n, reason, sf->FileSize, sf0->FileSize, sf->PageNumber);
      if (crashInd)
        progError(__LINE__, NDBD_EXIT_SR_SCHEMAFILE, reason_msg);
      ndbrequireErr(fsPtr.p->fsState == FsConnectRecord::READ_SCHEMA1,
                    NDBD_EXIT_SR_SCHEMAFILE);
      jam();
      infoEvent("primary %s, trying backup", reason_msg);
      readSchemaRef(signal, fsPtr);
      return;
    }
  }

  fsPtr.p->fsState = FsConnectRecord::CLOSE_READ_SCHEMA;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}//Dbdict::readSchemaConf()

void Dbdict::readSchemaRef(Signal* signal,
                           FsConnectRecordPtr fsPtr)
{
  /**
   * First close corrupt file
   */
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_SCHEMA2;
  closeFile(signal, fsPtr.p->filePtr, fsPtr.i);
  return;
}

void Dbdict::closeReadSchemaConf(Signal* signal,
                                 FsConnectRecordPtr fsPtr)
{
  c_fsConnectRecordPool.release(fsPtr);
  ReadSchemaRecord::SchemaReadState state = c_readSchemaRecord.schemaReadState;
  c_readSchemaRecord.schemaReadState = ReadSchemaRecord::IDLE;

  switch(state) {
  case ReadSchemaRecord::INITIAL_READ :
    jam();
    {
      // write back both copies

      ndbrequire(c_writeSchemaRecord.inUse == false);
      XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.oldSchemaPage != 0 ];
      Uint32 noOfPages =
        (c_noOfMetaTables + NDB_SF_PAGE_ENTRIES - 1) /
        NDB_SF_PAGE_ENTRIES;
      resizeSchemaFile(xsf, noOfPages);

      c_writeSchemaRecord.inUse = true;
      c_writeSchemaRecord.pageId = c_schemaRecord.oldSchemaPage;
      c_writeSchemaRecord.newFile = true;
      c_writeSchemaRecord.firstPage = 0;
      c_writeSchemaRecord.noOfPages = xsf->noOfPages;

      c_writeSchemaRecord.m_callback.m_callbackFunction =
        safe_cast(&Dbdict::initSchemaFile_conf);

      startWriteSchemaFile(signal);
    }
    break;

  default :
    ndbrequire(false);
    break;

  }//switch
}//Dbdict::closeReadSchemaConf()

bool
Dbdict::convertSchemaFileTo_5_0_6(XSchemaFile * xsf)
{
  const Uint32 pageSize_old = 32 * 1024;
  union {
    Uint32 page_old[pageSize_old >> 2];
    SchemaFile _SchemaFile;
  };
  (void)_SchemaFile;
  SchemaFile * sf_old = (SchemaFile *)page_old;

  if (xsf->noOfPages * NDB_SF_PAGE_SIZE != pageSize_old)
    return false;
  SchemaFile * sf0 = &xsf->schemaPage[0];
  memcpy(sf_old, sf0, pageSize_old);

  // init max number new pages needed
  xsf->noOfPages = (sf_old->NoOfTableEntries + NDB_SF_PAGE_ENTRIES - 1) /
                   NDB_SF_PAGE_ENTRIES;
  initSchemaFile(xsf, 0, xsf->noOfPages, true);

  Uint32 noOfPages = 1;
  Uint32 n, i, j;
  for (n = 0; n < xsf->noOfPages; n++) {
    jam();
    for (i = 0; i < NDB_SF_PAGE_ENTRIES; i++) {
      j = n * NDB_SF_PAGE_ENTRIES + i;
      if (j >= sf_old->NoOfTableEntries)
        continue;
      const SchemaFile::TableEntry_old & te_old = sf_old->TableEntries_old[j];
      if (te_old.m_tableState == SchemaFile::SF_UNUSED ||
          te_old.m_noOfPages == 0)
        continue;
      SchemaFile * sf = &xsf->schemaPage[n];
      SchemaFile::TableEntry & te = sf->TableEntries[i];
      te.m_tableState = te_old.m_tableState;
      te.m_tableVersion = te_old.m_tableVersion;
      te.m_tableType = te_old.m_tableType;
      te.m_info_words = te_old.m_noOfPages * ZSIZE_OF_PAGES_IN_WORDS -
                        ZPAGE_HEADER_SIZE;
      te.m_gcp = te_old.m_gcp;
      if (noOfPages < n)
        noOfPages = n;
    }
  }
  xsf->noOfPages = noOfPages;
  initSchemaFile(xsf, 0, xsf->noOfPages, false);

  return true;
}

bool
Dbdict::convertSchemaFileTo_6_4(XSchemaFile * xsf)
{
  for (Uint32 i = 0; i < xsf->noOfPages; i++)
  {
    xsf->schemaPage[i].NdbVersion = NDB_VERSION_D;
    for (Uint32 j = 0; j < NDB_SF_PAGE_ENTRIES; j++)
    {
      Uint32 n = i * NDB_SF_PAGE_ENTRIES + j;
      SchemaFile::TableEntry * transEntry = getTableEntry(xsf, n);

      switch(SchemaFile::Old::TableState(transEntry->m_tableState)) {
      case SchemaFile::Old::INIT:
        transEntry->m_tableState = SchemaFile::SF_UNUSED;
        break;
      case SchemaFile::Old::ADD_STARTED:
        transEntry->m_tableState = SchemaFile::SF_UNUSED;
        break;
      case SchemaFile::Old::TABLE_ADD_COMMITTED:
        transEntry->m_tableState = SchemaFile::SF_IN_USE;
        break;
      case SchemaFile::Old::DROP_TABLE_STARTED:
        transEntry->m_tableState = SchemaFile::SF_UNUSED;
        break;
      case SchemaFile::Old::DROP_TABLE_COMMITTED:
        transEntry->m_tableState = SchemaFile::SF_UNUSED;
        break;
      case SchemaFile::Old::ALTER_TABLE_COMMITTED:
        transEntry->m_tableState = SchemaFile::SF_IN_USE;
        break;
      case SchemaFile::Old::TEMPORARY_TABLE_COMMITTED:
        transEntry->m_tableState = SchemaFile::SF_IN_USE;
        break;
      default:
        transEntry->m_tableState = SchemaFile::SF_UNUSED;
        break;
      }
    }
    computeChecksum(xsf, i);
  }
  return true;
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          INITIALISATION MODULE ------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains initialisation of data at start/restart.    */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

Dbdict::Dbdict(Block_context& ctx):
  SimulatedBlock(DBDICT, ctx),
  c_attributeRecordHash(c_attributeRecordPool),
  c_obj_name_hash(c_obj_pool),
  c_obj_id_hash(c_obj_pool),
  c_schemaOpHash(c_schemaOpPool),
  c_schemaTransHash(c_schemaTransPool),
  c_schemaTransList(c_schemaTransPool),
  c_schemaTransCount(0),
  c_txHandleHash(c_txHandlePool),
  c_opCreateEvent(c_opRecordPool),
  c_opSubEvent(c_opRecordPool),
  c_opDropEvent(c_opRecordPool),
  c_opSignalUtil(c_opRecordPool),
  c_opRecordSequence(0)
{
  BLOCK_CONSTRUCTOR(Dbdict);

  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbdict::execDUMP_STATE_ORD);
  addRecSignal(GSN_GET_TABINFOREQ, &Dbdict::execGET_TABINFOREQ);
  addRecSignal(GSN_GET_TABLEID_REQ, &Dbdict::execGET_TABLEDID_REQ);
  addRecSignal(GSN_GET_TABINFOREF, &Dbdict::execGET_TABINFOREF);
  addRecSignal(GSN_GET_TABINFO_CONF, &Dbdict::execGET_TABINFO_CONF);
  addRecSignal(GSN_CONTINUEB, &Dbdict::execCONTINUEB);

  addRecSignal(GSN_DBINFO_SCANREQ, &Dbdict::execDBINFO_SCANREQ);

  addRecSignal(GSN_CREATE_TABLE_REQ, &Dbdict::execCREATE_TABLE_REQ);
  addRecSignal(GSN_CREATE_TAB_REQ, &Dbdict::execCREATE_TAB_REQ);
  addRecSignal(GSN_CREATE_TAB_REF, &Dbdict::execCREATE_TAB_REF);
  addRecSignal(GSN_CREATE_TAB_CONF, &Dbdict::execCREATE_TAB_CONF);
  addRecSignal(GSN_CREATE_FRAGMENTATION_REQ, &Dbdict::execCREATE_FRAGMENTATION_REQ);
  addRecSignal(GSN_CREATE_FRAGMENTATION_REF, &Dbdict::execCREATE_FRAGMENTATION_REF);
  addRecSignal(GSN_CREATE_FRAGMENTATION_CONF, &Dbdict::execCREATE_FRAGMENTATION_CONF);
  addRecSignal(GSN_DIADDTABCONF, &Dbdict::execDIADDTABCONF);
  addRecSignal(GSN_DIADDTABREF, &Dbdict::execDIADDTABREF);
  addRecSignal(GSN_ADD_FRAGREQ, &Dbdict::execADD_FRAGREQ);
  addRecSignal(GSN_TAB_COMMITCONF, &Dbdict::execTAB_COMMITCONF);
  addRecSignal(GSN_TAB_COMMITREF, &Dbdict::execTAB_COMMITREF);
  addRecSignal(GSN_ALTER_TABLE_REQ, &Dbdict::execALTER_TABLE_REQ);
  addRecSignal(GSN_ALTER_TAB_REF, &Dbdict::execALTER_TAB_REF);
  addRecSignal(GSN_ALTER_TAB_CONF, &Dbdict::execALTER_TAB_CONF);
  addRecSignal(GSN_ALTER_TABLE_REF, &Dbdict::execALTER_TABLE_REF);
  addRecSignal(GSN_ALTER_TABLE_CONF, &Dbdict::execALTER_TABLE_CONF);

  // Index signals
  addRecSignal(GSN_CREATE_INDX_REQ, &Dbdict::execCREATE_INDX_REQ);
  addRecSignal(GSN_CREATE_INDX_IMPL_CONF, &Dbdict::execCREATE_INDX_IMPL_CONF);
  addRecSignal(GSN_CREATE_INDX_IMPL_REF, &Dbdict::execCREATE_INDX_IMPL_REF);

  addRecSignal(GSN_ALTER_INDX_REQ, &Dbdict::execALTER_INDX_REQ);
  addRecSignal(GSN_ALTER_INDX_CONF, &Dbdict::execALTER_INDX_CONF);
  addRecSignal(GSN_ALTER_INDX_REF, &Dbdict::execALTER_INDX_REF);
  addRecSignal(GSN_ALTER_INDX_IMPL_CONF, &Dbdict::execALTER_INDX_IMPL_CONF);
  addRecSignal(GSN_ALTER_INDX_IMPL_REF, &Dbdict::execALTER_INDX_IMPL_REF);

  addRecSignal(GSN_CREATE_TABLE_CONF, &Dbdict::execCREATE_TABLE_CONF);
  addRecSignal(GSN_CREATE_TABLE_REF, &Dbdict::execCREATE_TABLE_REF);

  addRecSignal(GSN_DROP_INDX_REQ, &Dbdict::execDROP_INDX_REQ);
  addRecSignal(GSN_DROP_INDX_IMPL_CONF, &Dbdict::execDROP_INDX_IMPL_CONF);
  addRecSignal(GSN_DROP_INDX_IMPL_REF, &Dbdict::execDROP_INDX_IMPL_REF);

  addRecSignal(GSN_DROP_TABLE_CONF, &Dbdict::execDROP_TABLE_CONF);
  addRecSignal(GSN_DROP_TABLE_REF, &Dbdict::execDROP_TABLE_REF);

  addRecSignal(GSN_BUILDINDXREQ, &Dbdict::execBUILDINDXREQ);
  addRecSignal(GSN_BUILDINDXCONF, &Dbdict::execBUILDINDXCONF);
  addRecSignal(GSN_BUILDINDXREF, &Dbdict::execBUILDINDXREF);
  addRecSignal(GSN_BUILD_INDX_IMPL_CONF, &Dbdict::execBUILD_INDX_IMPL_CONF);
  addRecSignal(GSN_BUILD_INDX_IMPL_REF, &Dbdict::execBUILD_INDX_IMPL_REF);

  // Util signals
  addRecSignal(GSN_UTIL_PREPARE_CONF, &Dbdict::execUTIL_PREPARE_CONF);
  addRecSignal(GSN_UTIL_PREPARE_REF,  &Dbdict::execUTIL_PREPARE_REF);

  addRecSignal(GSN_UTIL_EXECUTE_CONF, &Dbdict::execUTIL_EXECUTE_CONF);
  addRecSignal(GSN_UTIL_EXECUTE_REF,  &Dbdict::execUTIL_EXECUTE_REF);

  addRecSignal(GSN_UTIL_RELEASE_CONF, &Dbdict::execUTIL_RELEASE_CONF);
  addRecSignal(GSN_UTIL_RELEASE_REF,  &Dbdict::execUTIL_RELEASE_REF);

  // Event signals
  addRecSignal(GSN_CREATE_EVNT_REQ,  &Dbdict::execCREATE_EVNT_REQ);
  addRecSignal(GSN_CREATE_EVNT_CONF, &Dbdict::execCREATE_EVNT_CONF);
  addRecSignal(GSN_CREATE_EVNT_REF,  &Dbdict::execCREATE_EVNT_REF);

  addRecSignal(GSN_CREATE_SUBID_CONF, &Dbdict::execCREATE_SUBID_CONF);
  addRecSignal(GSN_CREATE_SUBID_REF,  &Dbdict::execCREATE_SUBID_REF);

  addRecSignal(GSN_SUB_CREATE_CONF, &Dbdict::execSUB_CREATE_CONF);
  addRecSignal(GSN_SUB_CREATE_REF,  &Dbdict::execSUB_CREATE_REF);

  addRecSignal(GSN_SUB_START_REQ,  &Dbdict::execSUB_START_REQ);
  addRecSignal(GSN_SUB_START_CONF,  &Dbdict::execSUB_START_CONF);
  addRecSignal(GSN_SUB_START_REF,  &Dbdict::execSUB_START_REF);

  addRecSignal(GSN_SUB_STOP_REQ,  &Dbdict::execSUB_STOP_REQ);
  addRecSignal(GSN_SUB_STOP_CONF,  &Dbdict::execSUB_STOP_CONF);
  addRecSignal(GSN_SUB_STOP_REF,  &Dbdict::execSUB_STOP_REF);

  addRecSignal(GSN_DROP_EVNT_REQ,  &Dbdict::execDROP_EVNT_REQ);

  addRecSignal(GSN_SUB_REMOVE_REQ, &Dbdict::execSUB_REMOVE_REQ);
  addRecSignal(GSN_SUB_REMOVE_CONF, &Dbdict::execSUB_REMOVE_CONF);
  addRecSignal(GSN_SUB_REMOVE_REF,  &Dbdict::execSUB_REMOVE_REF);

  // Trigger signals
  addRecSignal(GSN_CREATE_TRIG_REQ, &Dbdict::execCREATE_TRIG_REQ);
  addRecSignal(GSN_CREATE_TRIG_CONF, &Dbdict::execCREATE_TRIG_CONF);
  addRecSignal(GSN_CREATE_TRIG_REF, &Dbdict::execCREATE_TRIG_REF);
  addRecSignal(GSN_DROP_TRIG_REQ, &Dbdict::execDROP_TRIG_REQ);
  addRecSignal(GSN_DROP_TRIG_CONF, &Dbdict::execDROP_TRIG_CONF);
  addRecSignal(GSN_DROP_TRIG_REF, &Dbdict::execDROP_TRIG_REF);
  // Impl
  addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &Dbdict::execCREATE_TRIG_IMPL_CONF);
  addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &Dbdict::execCREATE_TRIG_IMPL_REF);
  addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &Dbdict::execDROP_TRIG_IMPL_CONF);
  addRecSignal(GSN_DROP_TRIG_IMPL_REF, &Dbdict::execDROP_TRIG_IMPL_REF);

  // Received signals
  addRecSignal(GSN_GET_SCHEMA_INFOREQ, &Dbdict::execGET_SCHEMA_INFOREQ);
  addRecSignal(GSN_SCHEMA_INFO, &Dbdict::execSCHEMA_INFO);
  addRecSignal(GSN_SCHEMA_INFOCONF, &Dbdict::execSCHEMA_INFOCONF);
  addRecSignal(GSN_DICTSTARTREQ, &Dbdict::execDICTSTARTREQ);
  addRecSignal(GSN_READ_NODESCONF, &Dbdict::execREAD_NODESCONF);
  addRecSignal(GSN_FSOPENCONF, &Dbdict::execFSOPENCONF);
  addRecSignal(GSN_FSOPENREF, &Dbdict::execFSOPENREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Dbdict::execFSCLOSECONF);
  addRecSignal(GSN_FSWRITECONF, &Dbdict::execFSWRITECONF);
  addRecSignal(GSN_FSREADCONF, &Dbdict::execFSREADCONF);
  addRecSignal(GSN_FSREADREF, &Dbdict::execFSREADREF, true);
  addRecSignal(GSN_LQHFRAGCONF, &Dbdict::execLQHFRAGCONF);
  addRecSignal(GSN_LQHADDATTCONF, &Dbdict::execLQHADDATTCONF);
  addRecSignal(GSN_LQHADDATTREF, &Dbdict::execLQHADDATTREF);
  addRecSignal(GSN_LQHFRAGREF, &Dbdict::execLQHFRAGREF);
  addRecSignal(GSN_NDB_STTOR, &Dbdict::execNDB_STTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbdict::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_STTOR, &Dbdict::execSTTOR);
  addRecSignal(GSN_TC_SCHVERCONF, &Dbdict::execTC_SCHVERCONF);
  addRecSignal(GSN_NODE_FAILREP, &Dbdict::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Dbdict::execINCL_NODEREQ);
  addRecSignal(GSN_API_FAILREQ, &Dbdict::execAPI_FAILREQ);

  addRecSignal(GSN_WAIT_GCP_REF, &Dbdict::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Dbdict::execWAIT_GCP_CONF);

  addRecSignal(GSN_LIST_TABLES_REQ, &Dbdict::execLIST_TABLES_REQ);

  addRecSignal(GSN_DROP_TABLE_REQ, &Dbdict::execDROP_TABLE_REQ);

  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dbdict::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_PREP_DROP_TAB_REF, &Dbdict::execPREP_DROP_TAB_REF);
  addRecSignal(GSN_PREP_DROP_TAB_CONF, &Dbdict::execPREP_DROP_TAB_CONF);

  addRecSignal(GSN_DROP_TAB_REF, &Dbdict::execDROP_TAB_REF);
  addRecSignal(GSN_DROP_TAB_CONF, &Dbdict::execDROP_TAB_CONF);

  addRecSignal(GSN_CREATE_FILE_REQ, &Dbdict::execCREATE_FILE_REQ);
  addRecSignal(GSN_CREATE_FILEGROUP_REQ, &Dbdict::execCREATE_FILEGROUP_REQ);

  addRecSignal(GSN_DROP_FILE_REQ, &Dbdict::execDROP_FILE_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_REQ, &Dbdict::execDROP_FILEGROUP_REQ);

  addRecSignal(GSN_DROP_FILE_IMPL_REF, &Dbdict::execDROP_FILE_IMPL_REF);
  addRecSignal(GSN_DROP_FILE_IMPL_CONF, &Dbdict::execDROP_FILE_IMPL_CONF);

  addRecSignal(GSN_DROP_FILEGROUP_IMPL_REF,
               &Dbdict::execDROP_FILEGROUP_IMPL_REF);
  addRecSignal(GSN_DROP_FILEGROUP_IMPL_CONF,
               &Dbdict::execDROP_FILEGROUP_IMPL_CONF);

  addRecSignal(GSN_CREATE_FILE_IMPL_REF, &Dbdict::execCREATE_FILE_IMPL_REF);
  addRecSignal(GSN_CREATE_FILE_IMPL_CONF, &Dbdict::execCREATE_FILE_IMPL_CONF);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_REF,
               &Dbdict::execCREATE_FILEGROUP_IMPL_REF);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_CONF,
               &Dbdict::execCREATE_FILEGROUP_IMPL_CONF);

  addRecSignal(GSN_BACKUP_LOCK_TAB_REQ, &Dbdict::execBACKUP_LOCK_TAB_REQ);

  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REQ, &Dbdict::execSCHEMA_TRANS_BEGIN_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_CONF, &Dbdict::execSCHEMA_TRANS_BEGIN_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REF, &Dbdict::execSCHEMA_TRANS_BEGIN_REF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REQ, &Dbdict::execSCHEMA_TRANS_END_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_END_CONF, &Dbdict::execSCHEMA_TRANS_END_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REF, &Dbdict::execSCHEMA_TRANS_END_REF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REP, &Dbdict::execSCHEMA_TRANS_END_REP);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_REQ, &Dbdict::execSCHEMA_TRANS_IMPL_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_CONF, &Dbdict::execSCHEMA_TRANS_IMPL_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_REF, &Dbdict::execSCHEMA_TRANS_IMPL_REF);

  addRecSignal(GSN_DICT_LOCK_REQ, &Dbdict::execDICT_LOCK_REQ);
  addRecSignal(GSN_DICT_UNLOCK_ORD, &Dbdict::execDICT_UNLOCK_ORD);

  addRecSignal(GSN_DICT_TAKEOVER_REQ, &Dbdict::execDICT_TAKEOVER_REQ);
  addRecSignal(GSN_DICT_TAKEOVER_REF, &Dbdict::execDICT_TAKEOVER_REF);
  addRecSignal(GSN_DICT_TAKEOVER_CONF, &Dbdict::execDICT_TAKEOVER_CONF);

  addRecSignal(GSN_CREATE_HASH_MAP_REQ, &Dbdict::execCREATE_HASH_MAP_REQ);

  addRecSignal(GSN_COPY_DATA_REQ, &Dbdict::execCOPY_DATA_REQ);
  addRecSignal(GSN_COPY_DATA_REF, &Dbdict::execCOPY_DATA_REF);
  addRecSignal(GSN_COPY_DATA_CONF, &Dbdict::execCOPY_DATA_CONF);

  addRecSignal(GSN_COPY_DATA_IMPL_REF, &Dbdict::execCOPY_DATA_IMPL_REF);
  addRecSignal(GSN_COPY_DATA_IMPL_CONF, &Dbdict::execCOPY_DATA_IMPL_CONF);

  addRecSignal(GSN_CREATE_NODEGROUP_REQ, &Dbdict::execCREATE_NODEGROUP_REQ);
  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_REF, &Dbdict::execCREATE_NODEGROUP_IMPL_REF);
  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_CONF, &Dbdict::execCREATE_NODEGROUP_IMPL_CONF);

  addRecSignal(GSN_CREATE_HASH_MAP_REF, &Dbdict::execCREATE_HASH_MAP_REF);
  addRecSignal(GSN_CREATE_HASH_MAP_CONF, &Dbdict::execCREATE_HASH_MAP_CONF);

  addRecSignal(GSN_DROP_NODEGROUP_REQ, &Dbdict::execDROP_NODEGROUP_REQ);
  addRecSignal(GSN_DROP_NODEGROUP_IMPL_REF, &Dbdict::execDROP_NODEGROUP_IMPL_REF);
  addRecSignal(GSN_DROP_NODEGROUP_IMPL_CONF, &Dbdict::execDROP_NODEGROUP_IMPL_CONF);

  // ordered index statistics
  addRecSignal(GSN_INDEX_STAT_REQ, &Dbdict::execINDEX_STAT_REQ);
  addRecSignal(GSN_INDEX_STAT_CONF, &Dbdict::execINDEX_STAT_CONF);
  addRecSignal(GSN_INDEX_STAT_REF, &Dbdict::execINDEX_STAT_REF);
  addRecSignal(GSN_INDEX_STAT_IMPL_CONF, &Dbdict::execINDEX_STAT_IMPL_CONF);
  addRecSignal(GSN_INDEX_STAT_IMPL_REF, &Dbdict::execINDEX_STAT_IMPL_REF);
  addRecSignal(GSN_INDEX_STAT_REP, &Dbdict::execINDEX_STAT_REP);
}//Dbdict::Dbdict()

Dbdict::~Dbdict()
{
}//Dbdict::~Dbdict()

BLOCK_FUNCTIONS(Dbdict)

bool
Dbdict::getParam(const char * name, Uint32 * count)
{
  if (name != 0 && count != 0)
  {
    if (strcmp(name, "ActiveCounters") == 0)
    {
      * count = 25;
      return true;
    }
  }
  return false;
}

void Dbdict::initCommonData()
{
/* ---------------------------------------------------------------- */
// Initialise all common variables.
/* ---------------------------------------------------------------- */
  initRetrieveRecord(0, 0, 0);
  initSchemaRecord();
  initRestartRecord();
  initSendSchemaRecord();
  initReadTableRecord();
  initWriteTableRecord();
  initReadSchemaRecord();
  initWriteSchemaRecord();

  c_masterNodeId = ZNIL;
  c_numberNode = 0;
  c_noNodesFailed = 0;
  c_failureNr = 0;
  c_packTable.m_state = PackTable::PTS_IDLE;
  c_startPhase = 0;
  c_restartType = 255; //Ensure not used restartType
  c_tabinfoReceived = 0;
  c_initialStart = false;
  c_systemRestart = false;
  c_initialNodeRestart = false;
  c_nodeRestart = false;
  c_takeOverInProgress = false;

  c_outstanding_sub_startstop = 0;
  c_sub_startstop_lock.clear();

#if defined VM_TRACE || defined ERROR_INSERT
  g_trace = 99;
#else
  g_trace = 0;
#endif

}//Dbdict::initCommonData()

void Dbdict::initRecords()
{
  initNodeRecords();
  initPageRecords();
}//Dbdict::initRecords()

void Dbdict::initSendSchemaRecord()
{
  c_sendSchemaRecord.noOfWords = (Uint32)-1;
  c_sendSchemaRecord.pageId = RNIL;
  c_sendSchemaRecord.noOfWordsCurrentlySent = 0;
  c_sendSchemaRecord.noOfSignalsSentSinceDelay = 0;
  c_sendSchemaRecord.inUse = false;
  //c_sendSchemaRecord.sendSchemaState = SendSchemaRecord::IDLE;
}//initSendSchemaRecord()

void Dbdict::initReadTableRecord()
{
  c_readTableRecord.no_of_words= 0;
  c_readTableRecord.pageId = RNIL;
  c_readTableRecord.tableId = ZNIL;
  c_readTableRecord.inUse = false;
}//initReadTableRecord()

void Dbdict::initWriteTableRecord()
{
  c_writeTableRecord.no_of_words= 0;
  c_writeTableRecord.pageId = RNIL;
  c_writeTableRecord.noOfTableFilesHandled = 3;
  c_writeTableRecord.tableId = ZNIL;
  c_writeTableRecord.tableWriteState = WriteTableRecord::IDLE;
}//initWriteTableRecord()

void Dbdict::initReadSchemaRecord()
{
  c_readSchemaRecord.pageId = RNIL;
  c_readSchemaRecord.schemaReadState = ReadSchemaRecord::IDLE;
}//initReadSchemaRecord()

void Dbdict::initWriteSchemaRecord()
{
  c_writeSchemaRecord.inUse = false;
  c_writeSchemaRecord.pageId = RNIL;
  c_writeSchemaRecord.noOfSchemaFilesHandled = 3;
}//initWriteSchemaRecord()

void Dbdict::initRetrieveRecord(Signal* signal, Uint32 i, Uint32 returnCode)
{
  c_retrieveRecord.busyState = false;
  c_retrieveRecord.blockRef = 0;
  c_retrieveRecord.m_senderData = RNIL;
  c_retrieveRecord.tableId = RNIL;
  c_retrieveRecord.currentSent = 0;
  c_retrieveRecord.retrievedNoOfPages = 0;
  c_retrieveRecord.retrievedNoOfWords = 0;
  c_retrieveRecord.m_useLongSig = false;
}//initRetrieveRecord()

void Dbdict::initSchemaRecord()
{
  c_schemaRecord.schemaPage = RNIL;
  c_schemaRecord.oldSchemaPage = RNIL;
}//Dbdict::initSchemaRecord()

void Dbdict::initNodeRecords()
{
  jam();
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    NodeRecordPtr nodePtr;
    c_nodes.getPtr(nodePtr, i);
    new (nodePtr.p) NodeRecord();
    nodePtr.p->hotSpare = false;
    nodePtr.p->nodeState = NodeRecord::API_NODE;
  }//for
}//Dbdict::initNodeRecords()

void Dbdict::initPageRecords()
{
  c_retrieveRecord.retrievePage =  ZMAX_PAGES_OF_TABLE_DEFINITION;
  ndbrequire(ZNUMBER_OF_PAGES >= (ZMAX_PAGES_OF_TABLE_DEFINITION + 1));
  c_schemaRecord.schemaPage = 0;
  c_schemaRecord.oldSchemaPage = NDB_SF_MAX_PAGES;
}//Dbdict::initPageRecords()

void Dbdict::initialiseTableRecord(TableRecordPtr tablePtr, Uint32 tableId)
{
  new (tablePtr.p) TableRecord();
  tablePtr.p->filePtr[0] = RNIL;
  tablePtr.p->filePtr[1] = RNIL;
  tablePtr.p->tableId = tableId;
  tablePtr.p->tableVersion = (Uint32)-1;
  tablePtr.p->fragmentType = DictTabInfo::AllNodesSmallTable;
  tablePtr.p->gciTableCreated = 0;
  tablePtr.p->noOfAttributes = ZNIL;
  tablePtr.p->noOfNullAttr = 0;
  tablePtr.p->fragmentCount = 0;
  /*
    tablePtr.p->lh3PageIndexBits = 0;
    tablePtr.p->lh3DistrBits = 0;
    tablePtr.p->lh3PageBits = 6;
  */
  tablePtr.p->kValue = 6;
  tablePtr.p->localKeyLen = 1;
  tablePtr.p->maxLoadFactor = 80;
  tablePtr.p->minLoadFactor = 70;
  tablePtr.p->noOfPrimkey = 1;
  tablePtr.p->tupKeyLength = 1;
  tablePtr.p->maxRowsLow = 0;
  tablePtr.p->maxRowsHigh = 0;
  tablePtr.p->defaultNoPartFlag = true;
  tablePtr.p->linearHashFlag = true;
  tablePtr.p->m_bits = 0;
  tablePtr.p->minRowsLow = 0;
  tablePtr.p->minRowsHigh = 0;
  tablePtr.p->singleUserMode = 0;
  tablePtr.p->tableType = DictTabInfo::UserTable;
  tablePtr.p->primaryTableId = RNIL;
  // volatile elements
  tablePtr.p->indexState = TableRecord::IS_UNDEFINED;
  tablePtr.p->triggerId = RNIL;
  tablePtr.p->buildTriggerId = RNIL;
  tablePtr.p->m_read_locked= 0;
  tablePtr.p->storageType = NDB_STORAGETYPE_DEFAULT;
  tablePtr.p->indexStatFragId = ZNIL;
  bzero(tablePtr.p->indexStatNodes, sizeof(tablePtr.p->indexStatNodes));
  tablePtr.p->indexStatBgRequest = 0;
  tablePtr.p->m_obj_ptr_i = RNIL;
}//Dbdict::initialiseTableRecord()

void Dbdict::initialiseTriggerRecord(TriggerRecordPtr triggerPtr, Uint32 triggerId)
{
  new (triggerPtr.p) TriggerRecord();
  triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;
  triggerPtr.p->triggerId = triggerId;
  triggerPtr.p->tableId = RNIL;
  triggerPtr.p->attributeMask.clear();
  triggerPtr.p->indexId = RNIL;
  triggerPtr.p->m_obj_ptr_i = RNIL;
}

Uint32 Dbdict::getFsConnRecord()
{
  FsConnectRecordPtr fsPtr;
  c_fsConnectRecordPool.seize(fsPtr);
  ndbrequire(fsPtr.i != RNIL);
  fsPtr.p->filePtr = (Uint32)-1;
  fsPtr.p->ownerPtr = RNIL;
  fsPtr.p->fsState = FsConnectRecord::IDLE;
  return fsPtr.i;
}//Dbdict::getFsConnRecord()

/*
 * Search schemafile for free entry.  Its index is used as 'logical id'
 * of new disk-stored object.
 */
Uint32 Dbdict::getFreeObjId(bool both)
{
  const XSchemaFile * newxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  const XSchemaFile * oldxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
  const Uint32 noOfEntries = newxsf->noOfPages * NDB_SF_PAGE_ENTRIES;
  for (Uint32 i = 0; i<noOfEntries; i++)
  {
    const SchemaFile::TableEntry * oldentry = getTableEntry(oldxsf, i);
    const SchemaFile::TableEntry * newentry = getTableEntry(newxsf, i);
    if (newentry->m_tableState == (Uint32)SchemaFile::SF_UNUSED)
    {
      jam();
      if (both == false ||
          oldentry->m_tableState == (Uint32)SchemaFile::SF_UNUSED)
      {
        jam();
        return i;
      }
    }
  }
  return RNIL;
}

bool Dbdict::seizeTableRecord(TableRecordPtr& tablePtr, Uint32& schemaFileId)
{
  if (schemaFileId == RNIL)
  {
    jam();
    schemaFileId = getFreeObjId();
  }
  if (schemaFileId == RNIL)
  {
    jam();
    return false;
  }
  if (schemaFileId >= c_noOfMetaTables)
  {
    jam();
    return false;
  }

  c_tableRecordPool_.seize(tablePtr);
  if (tablePtr.isNull())
  {
    jam();
    return false;
  }
  initialiseTableRecord(tablePtr, schemaFileId);
  return true;
}

Uint32 Dbdict::getFreeTriggerRecord()
{
  const Uint32 size = c_triggerRecordPool_.getSize();
  TriggerRecordPtr triggerPtr;
  for (Uint32 id = 0; id < size; id++) {
    jam();
    bool ok = find_object(triggerPtr, id);
    if (!ok)
    {
      jam();
      return id;
    }
  }
  return RNIL;
}

bool Dbdict::seizeTriggerRecord(TriggerRecordPtr& triggerPtr, Uint32 triggerId)
{
  if (triggerId == RNIL)
  {
    triggerId = getFreeTriggerRecord();
  }
  else
  {
    TriggerRecordPtr ptr;
    bool ok =  find_object(ptr, triggerId);
    if (ok)
    { // triggerId already in use
      jam();
      return false;
    }
  }
  if (triggerId == RNIL)
  {
    jam();
    return false;
  }
  c_triggerRecordPool_.seize(triggerPtr);
  if (triggerPtr.isNull())
  {
    jam();
    return false;
  }
  initialiseTriggerRecord(triggerPtr, triggerId);
  return true;
}

Uint32
Dbdict::check_read_obj(Uint32 objId, Uint32 transId)
{
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  if (objId < (NDB_SF_PAGE_ENTRIES * xsf->noOfPages))
  {
    jam();
    return check_read_obj(getTableEntry(xsf, objId), transId);
  }
  return GetTabInfoRef::InvalidTableId;
}

Uint32
Dbdict::check_read_obj(SchemaFile::TableEntry* te, Uint32 transId)
{
  if (te->m_tableState == SchemaFile::SF_UNUSED)
  {
    jam();
    return GetTabInfoRef::TableNotDefined;
  }

  if (te->m_transId == 0 || te->m_transId == transId)
  {
    jam();
    return 0;
  }

  switch(te->m_tableState){
  case SchemaFile::SF_CREATE:
    jam();
    return GetTabInfoRef::TableNotDefined;
    break;
  case SchemaFile::SF_ALTER:
    jam();
    return 0;
  case SchemaFile::SF_DROP:
    jam();
    /** uncommitted drop */
    return DropTableRef::ActiveSchemaTrans;
  case SchemaFile::SF_IN_USE:
    jam();
    return 0;
  default:
    /** weird... */
    return GetTabInfoRef::TableNotDefined;
  }
}


Uint32
Dbdict::check_write_obj(Uint32 objId, Uint32 transId)
{
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  if (objId < (NDB_SF_PAGE_ENTRIES * xsf->noOfPages))
  {
    jam();
    SchemaFile::TableEntry* te = getTableEntry(xsf, objId);

    if (te->m_tableState == SchemaFile::SF_UNUSED)
    {
      jam();
      return GetTabInfoRef::TableNotDefined;
    }

    if (te->m_transId == 0 || te->m_transId == transId)
    {
      jam();
      return 0;
    }

    return DropTableRef::ActiveSchemaTrans;
  }
  return GetTabInfoRef::InvalidTableId;
}

Uint32
Dbdict::check_write_obj(Uint32 objId, Uint32 transId,
                        SchemaFile::EntryState op,
                        ErrorInfo& error)
{
  Uint32 err = check_write_obj(objId, transId);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
  }

  return err;
}



/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          START/RESTART HANDLING ------------------------ */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code that is common for all             */
/* start/restart types.                                             */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

/* ---------------------------------------------------------------- */
// This is sent as the first signal during start/restart.
/* ---------------------------------------------------------------- */
void Dbdict::execSTTOR(Signal* signal)
{
  jamEntry();
  c_startPhase = signal->theData[1];
  switch (c_startPhase) {
  case 1:
    break;
  case 3:
    c_restartType = signal->theData[7];         /* valid if 3 */
    ndbrequire(c_restartType == NodeState::ST_INITIAL_START ||
               c_restartType == NodeState::ST_SYSTEM_RESTART ||
               c_restartType == NodeState::ST_INITIAL_NODE_RESTART ||
               c_restartType == NodeState::ST_NODE_RESTART);
    break;
  case 7:
    /*
     * config cannot yet be changed dynamically but we start the
     * loop always anyway because the cost is minimal
     */
    c_indexStatBgId = 0;
    indexStatBg_sendContinueB(signal);
    break;
  }
  sendSTTORRY(signal);
}//execSTTOR()

void Dbdict::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;       /* garbage SIGNAL KEY */
  signal->theData[1] = 0;       /* garbage SIGNAL VERSION NUMBER  */
  signal->theData[2] = 0;       /* garbage */
  signal->theData[3] = 1;       /* first wanted start phase */
  signal->theData[4] = 3;       /* get type of start */
  signal->theData[5] = 7;       /* start index stat bg loop */
  signal->theData[6] = ZNOMOREPHASES;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}

/* ---------------------------------------------------------------- */
// We receive information about sizes of records.
/* ---------------------------------------------------------------- */
void Dbdict::execREAD_CONFIG_REQ(Signal* signal)
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  jamEntry();

  const ndb_mgm_configuration_iterator * p =
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 attributesize;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS,
					&c_maxNoOfTriggers));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_ATTRIBUTE,&attributesize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &c_noOfMetaTables));
  c_indexStatAutoCreate = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_AUTO_CREATE,
                            &c_indexStatAutoCreate);
  c_indexStatAutoUpdate = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_INDEX_STAT_AUTO_UPDATE,
                            &c_indexStatAutoUpdate);

  Pool_context pc;
  pc.m_block = this;

  c_arenaAllocator.init(796, RT_DBDICT_SCHEMA_TRANS_ARENA, pc); // TODO: set size automagical? INFO: 796 is about 1/41 of a page, and bigger than CreateIndexRec (784 bytes)

  c_attributeRecordPool.setSize(attributesize);
  c_attributeRecordHash.setSize(64);
  c_fsConnectRecordPool.setSize(ZFS_CONNECT_SIZE);
  c_nodes.setSize(MAX_NDB_NODES);
  c_pageRecordArray.setSize(ZNUMBER_OF_PAGES);
  c_schemaPageRecordArray.setSize(2 * NDB_SF_MAX_PAGES);
  c_tableRecordPool_.setSize(c_noOfMetaTables);
  g_key_descriptor_pool.setSize(c_noOfMetaTables);
  c_triggerRecordPool_.setSize(c_maxNoOfTriggers);

  Record_info ri;
  OpSectionBuffer::createRecordInfo(ri, RT_DBDICT_OP_SECTION_BUFFER);
  c_opSectionBufferPool.init(&c_arenaAllocator, ri, pc);

  c_schemaOpHash.setSize(MAX_SCHEMA_OPERATIONS);
  c_schemaTransPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_SCHEMA_TRANSACTION, pc);
  c_schemaTransHash.setSize(2);
  c_txHandlePool.setSize(2);
  c_txHandleHash.setSize(2);

  c_obj_pool.setSize(c_noOfMetaTables+c_maxNoOfTriggers);
  c_obj_name_hash.setSize((c_noOfMetaTables+c_maxNoOfTriggers+1)/2);
  c_obj_id_hash.setSize((c_noOfMetaTables+c_maxNoOfTriggers+1)/2);
  m_dict_lock_pool.setSize(MAX_NDB_NODES);

  c_file_pool.init(RT_DBDICT_FILE, pc);
  c_filegroup_pool.init(RT_DBDICT_FILEGROUP, pc);

  // new OpRec pools
  /**
   * one mysql index can be 2 ndb indexes
   */
  /**
   * TODO: Use arena-allocator for schema-transactions
   */
  c_createTableRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_TABLE, pc);
  c_dropTableRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_TABLE, pc);
  c_alterTableRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_ALTER_TABLE, pc);
  c_createTriggerRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_TRIGGER, pc);
  c_dropTriggerRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_TRIGGER, pc);
  c_createIndexRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_INDEX, pc);
  c_dropIndexRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_INDEX, pc);
  c_alterIndexRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_ALTER_INDEX, pc);
  c_buildIndexRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_BUILD_INDEX, pc);
  c_indexStatRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_INDEX_STAT, pc);
  c_createFilegroupRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_FILEGROUP, pc);
  c_createFileRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_FILE, pc);
  c_dropFilegroupRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_FILEGROUP, pc);
  c_dropFileRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_FILE, pc);
  c_createHashMapRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_HASH_MAP, pc);
  c_copyDataRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_COPY_DATA, pc);
  c_schemaOpPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_SCHEMA_OPERATION, pc);

  c_hash_map_pool.setSize(32);
  g_hash_map.setSize(32);

  c_createNodegroupRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_CREATE_NODEGROUP, pc);
  c_dropNodegroupRecPool.arena_pool_init(&c_arenaAllocator, RT_DBDICT_DROP_NODEGROUP, pc);

  c_opRecordPool.setSize(256);   // XXX need config params
  c_opCreateEvent.setSize(2);
  c_opSubEvent.setSize(2);
  c_opDropEvent.setSize(2);
  c_opSignalUtil.setSize(8);

  // Initialize schema file copies
  c_schemaFile[0].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(0 * NDB_SF_MAX_PAGES);
  c_schemaFile[0].noOfPages = 0;
  c_schemaFile[1].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(1 * NDB_SF_MAX_PAGES);
  c_schemaFile[1].noOfPages = 0;

  Uint32 rps = 0;
  rps += c_noOfMetaTables * (MAX_TAB_NAME_SIZE + MAX_FRM_DATA_SIZE);
  rps += attributesize * (MAX_ATTR_NAME_SIZE + MAX_ATTR_DEFAULT_VALUE_SIZE);
  rps += c_maxNoOfTriggers * MAX_TAB_NAME_SIZE;
  rps += (10 + 10) * MAX_TAB_NAME_SIZE;

  Uint32 sm = 5;
  ndb_mgm_get_int_parameter(p, CFG_DB_STRING_MEMORY, &sm);
  if (sm == 0)
    sm = 25;

  Uint64 sb = 0;
  if (sm <= 100)
  {
    sb = (Uint64(rps) * Uint64(sm)) / 100;
  }
  else
  {
    sb = sm;
  }

  sb /= (LocalRope::getSegmentSize() * sizeof(Uint32));
  sb += 100; // more safty
  ndbrequire(sb < (Uint64(1) << 32));
  c_rope_pool.setSize(Uint32(sb));

  // Initialize BAT for interface to file system
  NewVARIABLE* bat = allocateBat(2);
  bat[0].WA = &c_schemaPageRecordArray.getPtr(0)->word[0];
  bat[0].nrr = 2 * NDB_SF_MAX_PAGES;
  bat[0].ClusterSize = NDB_SF_PAGE_SIZE;
  bat[0].bits.q = NDB_SF_PAGE_SIZE_IN_WORDS_LOG2;
  bat[0].bits.v = 5;  // 32 bits per element
  bat[1].WA = &c_pageRecordArray.getPtr(0)->word[0];
  bat[1].nrr = ZNUMBER_OF_PAGES;
  bat[1].ClusterSize = ZSIZE_OF_PAGES_IN_WORDS * 4;
  bat[1].bits.q = ZLOG_SIZE_OF_PAGES_IN_WORDS; // 2**13 = 8192 elements
  bat[1].bits.v = 5;  // 32 bits per element

  initCommonData();
  initRecords();

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal,
	     ReadConfigConf::SignalLength, JBB);

  {
    DictObjectPtr ptr;
    DictObject_list objs(c_obj_pool);
    while(objs.seize(ptr))
      new (ptr.p) DictObject();
    objs.release();
  }

  unsigned trace = 0;
  char buf[100];
  if (NdbEnv_GetEnv("DICT_TRACE", buf, sizeof(buf)))
  {
    jam();
    g_trace = (unsigned)atoi(buf);
  }
  else if (ndb_mgm_get_int_parameter(p, CFG_DB_DICT_TRACE, &trace) == 0)
  {
    g_trace = trace;
  }
}//execSIZEALT_REP()

/* ---------------------------------------------------------------- */
// Start phase signals sent by CNTR. We reply with NDB_STTORRY when
// we completed this phase.
/* ---------------------------------------------------------------- */
void Dbdict::execNDB_STTOR(Signal* signal)
{
  jamEntry();
  c_startPhase = signal->theData[2];
  const Uint32 restartType = signal->theData[3];
  if (restartType == NodeState::ST_INITIAL_START) {
    jam();
    c_initialStart = true;
  } else if (restartType == NodeState::ST_SYSTEM_RESTART) {
    jam();
    c_systemRestart = true;
  } else if (restartType == NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
    c_initialNodeRestart = true;
  } else if (restartType == NodeState::ST_NODE_RESTART) {
    jam();
    c_nodeRestart = true;
  } else {
    ndbrequire(false);
  }//if
  switch (c_startPhase) {
  case 1:
    jam();
    initSchemaFile(signal);
    break;
  case 3:
    jam();
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    break;
  case 6:
    jam();
    c_initialStart = false;
    c_systemRestart = false;
    c_initialNodeRestart = false;
    c_nodeRestart = false;
    sendNDB_STTORRY(signal);
    break;
  case 7:
    sendNDB_STTORRY(signal);
    break;
  default:
    jam();
    sendNDB_STTORRY(signal);
    break;
  }//switch
}//execNDB_STTOR()

void Dbdict::sendNDB_STTORRY(Signal* signal)
{
  signal->theData[0] = reference();
  sendSignal(NDBCNTR_REF, GSN_NDB_STTORRY, signal, 1, JBB);
  return;
}//sendNDB_STTORRY()

/* ---------------------------------------------------------------- */
// We receive the information about which nodes that are up and down.
/* ---------------------------------------------------------------- */
void Dbdict::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();

  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  c_numberNode   = readNodes->noOfNodes;
  c_masterNodeId = readNodes->masterNodeId;

  c_noNodesFailed = 0;
  c_aliveNodes.clear();
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    NodeRecordPtr nodePtr;
    c_nodes.getPtr(nodePtr, i);

    if (NdbNodeBitmask::get(readNodes->allNodes, i)) {
      jam();
      nodePtr.p->nodeState = NodeRecord::NDB_NODE_ALIVE;
      if (NdbNodeBitmask::get(readNodes->inactiveNodes, i)) {
	jam();
	/**-------------------------------------------------------------------
	 *
	 * THIS NODE IS DEFINED IN THE CLUSTER BUT IS NOT ALIVE CURRENTLY.
	 * WE ADD THE NODE TO THE SET OF FAILED NODES AND ALSO SET THE
	 * BLOCKSTATE TO BUSY TO AVOID ADDING TABLES WHILE NOT ALL NODES ARE
	 * ALIVE.
	 *------------------------------------------------------------------*/
        nodePtr.p->nodeState = NodeRecord::NDB_NODE_DEAD;
	c_noNodesFailed++;
      } else {
	c_aliveNodes.set(i);
      }
    }//if
  }//for
  sendNDB_STTORRY(signal);
}//execREAD_NODESCONF()

void Dbdict::initSchemaFile(Signal* signal)
{
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  xsf->noOfPages = (c_noOfMetaTables + NDB_SF_PAGE_ENTRIES - 1)
                   / NDB_SF_PAGE_ENTRIES;
  initSchemaFile(xsf, 0, xsf->noOfPages, true);
  // init alt copy too for INR
  XSchemaFile * oldxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
  oldxsf->noOfPages = xsf->noOfPages;
  memcpy(&oldxsf->schemaPage[0], &xsf->schemaPage[0], xsf->schemaPage[0].FileSize);

  if (c_initialStart || c_initialNodeRestart) {
    jam();
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;
    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.newFile = true;
    c_writeSchemaRecord.firstPage = 0;
    c_writeSchemaRecord.noOfPages = xsf->noOfPages;

    c_writeSchemaRecord.m_callback.m_callbackFunction =
      safe_cast(&Dbdict::initSchemaFile_conf);

    startWriteSchemaFile(signal);
  } else if (c_systemRestart || c_nodeRestart) {
    jam();
    ndbrequire(c_readSchemaRecord.schemaReadState == ReadSchemaRecord::IDLE);
    c_readSchemaRecord.pageId = c_schemaRecord.oldSchemaPage;
    c_readSchemaRecord.firstPage = 0;
    c_readSchemaRecord.noOfPages = 1;
    c_readSchemaRecord.schemaReadState = ReadSchemaRecord::INITIAL_READ_HEAD;
    startReadSchemaFile(signal);
  } else {
    ndbrequire(false);
  }//if
}//Dbdict::initSchemaFile()

void
Dbdict::initSchemaFile_conf(Signal* signal, Uint32 callbackData, Uint32 rv){
  jam();
  sendNDB_STTORRY(signal);
}

void
Dbdict::activateIndexes(Signal* signal, Uint32 id)
{
  if (id == 0)
    D("activateIndexes start");

  Uint32 requestFlags = 0;

  switch (c_restartType) {
  case NodeState::ST_SYSTEM_RESTART:
    // activate in a distributed trans but do not build yet
    if (c_masterNodeId != getOwnNodeId()) {
      D("activateIndexes not master");
      goto out;
    }
    requestFlags |= DictSignal::RF_NO_BUILD;
    break;
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
    // activate on this node in a local trans
    requestFlags |= DictSignal::RF_LOCAL_TRANS;
    requestFlags |= DictSignal::RF_NO_BUILD;
    break;
  default:
    ndbrequire(false);
    break;
  }

  TableRecordPtr indexPtr;
  for (; id < c_noOfMetaTables; id++)
  {
    bool ok = find_object(indexPtr, id);
    if (!ok)
    {
      jam();
      continue;
    }

    if (check_read_obj(id))
    {
      continue;
    }

    if (!indexPtr.p->isIndex())
    {
      continue;
    }

    if ((requestFlags & DictSignal::RF_LOCAL_TRANS) &&
        indexPtr.p->indexState != TableRecord::IS_ONLINE)
    {
      jam();
      continue;
    }

    // wl3600_todo use simple schema trans when implemented
    D("activateIndexes id=" << id);

    TxHandlePtr tx_ptr;
    seizeTxHandle(tx_ptr);
    ndbrequire(!tx_ptr.isNull());

    tx_ptr.p->m_requestInfo = 0;
    tx_ptr.p->m_requestInfo |= requestFlags;
    tx_ptr.p->m_userData = indexPtr.i;

    Callback c = {
      safe_cast(&Dbdict::activateIndex_fromBeginTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;
    beginSchemaTrans(signal, tx_ptr);
    return;
  }

  D("activateIndexes done");
#ifdef VM_TRACE
  check_consistency();
#endif
out:
  signal->theData[0] = reference();
  signal->theData[1] = c_restartRecord.m_senderData;
  sendSignal(c_restartRecord.returnBlockRef, GSN_DICTSTARTCONF,
	     signal, 2, JBB);
}

void
Dbdict::activateIndex_fromBeginTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("activateIndex_fromBeginTrans" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); //wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  TableRecordPtr indexPtr;
  c_tableRecordPool_.getPtr(indexPtr, tx_ptr.p->m_userData);
  ndbrequire(!indexPtr.isNull());
  DictObjectPtr index_obj_ptr;
  c_obj_pool.getPtr(index_obj_ptr, indexPtr.p->m_obj_ptr_i);

  AlterIndxReq* req = (AlterIndxReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, AlterIndxImplReq::AlterIndexOnline);
  DictSignal::addRequestFlagsGlobal(requestInfo, tx_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = tx_ptr.p->tx_key;
  req->transId = tx_ptr.p->m_transId;
  req->transKey = tx_ptr.p->m_transKey;
  req->requestInfo = requestInfo;
  req->indexId = index_obj_ptr.p->m_id;
  req->indexVersion = indexPtr.p->tableVersion;

  Callback c = {
    safe_cast(&Dbdict::activateIndex_fromAlterIndex),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  sendSignal(reference(), GSN_ALTER_INDX_REQ, signal,
             AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::activateIndex_fromAlterIndex(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("activateIndex_fromAlterIndex" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); // wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (ret != 0)
    setError(tx_ptr.p->m_error, ret, __LINE__);

  Callback c = {
    safe_cast(&Dbdict::activateIndex_fromEndTrans),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  Uint32 flags = 0;
  if (hasError(tx_ptr.p->m_error))
    flags |= SchemaTransEndReq::SchemaTransAbort;
  endSchemaTrans(signal, tx_ptr, flags);
}

void
Dbdict::activateIndex_fromEndTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("activateIndex_fromEndTrans" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); //wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  TableRecordPtr indexPtr;
  c_tableRecordPool_.getPtr(indexPtr, tx_ptr.p->m_userData);
  DictObjectPtr index_obj_ptr;
  c_obj_pool.getPtr(index_obj_ptr, indexPtr.p->m_obj_ptr_i);

  char indexName[MAX_TAB_NAME_SIZE];
  {
    LocalRope name(c_rope_pool, index_obj_ptr.p->m_name);
    name.copy(indexName);
  }

  ErrorInfo error = tx_ptr.p->m_error;
  if (!hasError(error))
  {
    jam();
    infoEvent("DICT: activate index %u done (%s)",
             index_obj_ptr.p->m_id, indexName);
  }
  else
  {
    jam();
    warningEvent("DICT: activate index %u error: code=%u line=%u node=%u (%s)",
                index_obj_ptr.p->m_id,
		 error.errorCode, error.errorLine, error.errorNodeId,
		 indexName);
  }

  Uint32 id = index_obj_ptr.p->m_id;
  releaseTxHandle(tx_ptr);
  activateIndexes(signal, id + 1);
}

void
Dbdict::rebuildIndexes(Signal* signal, Uint32 id)
{
  if (id == 0)
    D("rebuildIndexes start");

  TableRecordPtr indexPtr;
  for (; id < c_noOfMetaTables; id++) {
    bool ok = find_object(indexPtr, id);
    if (!ok)
    {
      jam();
      continue;
    }
    if (check_read_obj(id))
      continue;
    if (!indexPtr.p->isIndex())
      continue;

    // wl3600_todo use simple schema trans when implemented
    D("rebuildIndexes id=" << id);

    TxHandlePtr tx_ptr;
    seizeTxHandle(tx_ptr);
    ndbrequire(!tx_ptr.isNull());

    Uint32 requestInfo = 0;
    if (indexPtr.p->m_bits & TableRecord::TR_Logged) {
      // only sets index online - the flag propagates to trans and ops
      requestInfo |= DictSignal::RF_NO_BUILD;
    }
    tx_ptr.p->m_requestInfo = requestInfo;
    tx_ptr.p->m_userData = indexPtr.i;

    Callback c = {
      safe_cast(&Dbdict::rebuildIndex_fromBeginTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;
    beginSchemaTrans(signal, tx_ptr);
    return;
  }

  D("rebuildIndexes done");
  sendNDB_STTORRY(signal);
}

void
Dbdict::rebuildIndex_fromBeginTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("rebuildIndex_fromBeginTrans" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); //wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  TableRecordPtr indexPtr;
  c_tableRecordPool_.getPtr(indexPtr, tx_ptr.p->m_userData);
  DictObjectPtr index_obj_ptr;
  c_obj_pool.getPtr(index_obj_ptr,indexPtr.p->m_obj_ptr_i);

  BuildIndxReq* req = (BuildIndxReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, BuildIndxReq::MainOp);
  DictSignal::addRequestFlagsGlobal(requestInfo, tx_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = tx_ptr.p->tx_key;
  req->transId = tx_ptr.p->m_transId;
  req->transKey = tx_ptr.p->m_transKey;
  req->requestInfo = requestInfo;
  req->buildId = 0;
  req->buildKey = 0;
  req->tableId = indexPtr.p->primaryTableId;
  req->indexId = index_obj_ptr.p->m_id;
  req->indexType = indexPtr.p->tableType;
  req->parallelism = 16;

  Callback c = {
    safe_cast(&Dbdict::rebuildIndex_fromBuildIndex),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  sendSignal(reference(), GSN_BUILDINDXREQ, signal,
             BuildIndxReq::SignalLength, JBB);
}

void
Dbdict::rebuildIndex_fromBuildIndex(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("rebuildIndex_fromBuildIndex" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); //wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (ret != 0)
    setError(tx_ptr.p->m_error, ret, __LINE__);

  Callback c = {
    safe_cast(&Dbdict::rebuildIndex_fromEndTrans),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  Uint32 flags = 0;
  if (hasError(tx_ptr.p->m_error))
    flags |= SchemaTransEndReq::SchemaTransAbort;
  endSchemaTrans(signal, tx_ptr, flags);
}

void
Dbdict::rebuildIndex_fromEndTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("rebuildIndex_fromEndTrans" << V(tx_key) << V(ret));

  ndbrequire(ret == 0); //wl3600_todo

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  TableRecordPtr indexPtr;
  c_tableRecordPool_.getPtr(indexPtr, tx_ptr.p->m_userData);

  const char* actionName;
  {
    Uint32 requestInfo = tx_ptr.p->m_requestInfo;
    bool noBuild = (requestInfo & DictSignal::RF_NO_BUILD);
    actionName = !noBuild ? "rebuild" : "online";
  }

  DictObjectPtr obj_ptr;
  c_obj_pool.getPtr(obj_ptr, indexPtr.p->m_obj_ptr_i);

  char indexName[MAX_TAB_NAME_SIZE];
  {
    LocalRope name(c_rope_pool, obj_ptr.p->m_name);
    name.copy(indexName);
  }

  ErrorInfo error = tx_ptr.p->m_error;
  if (!hasError(error)) {
    jam();
    infoEvent(
        "DICT: %s index %u done (%s)",
        actionName, obj_ptr.p->m_id, indexName);
  } else {
    jam();
    warningEvent(
        "DICT: %s index %u error: code=%u line=%u node=%u (%s)",
        actionName,
        obj_ptr.p->m_id, error.errorCode, error.errorLine, error.errorNodeId,
        indexName);
  }

  Uint32 id = obj_ptr.p->m_id;
  releaseTxHandle(tx_ptr);

  rebuildIndexes(signal, id + 1);
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          SYSTEM RESTART MODULE ------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains code specific for system restart            */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

/* ---------------------------------------------------------------- */
// DIH asks DICT to read in table data from disk during system
// restart. DIH also asks DICT to send information about which
// tables that should be started as part of this system restart.
// DICT will also activate the tables in TC as part of this process.
/* ---------------------------------------------------------------- */
void Dbdict::execDICTSTARTREQ(Signal* signal)
{
  jamEntry();
  c_restartRecord.gciToRestart = signal->theData[0];
  c_restartRecord.returnBlockRef = signal->theData[1];
  c_restartRecord.m_senderData = signal->theData[2];
  if (signal->getLength() < DictStartReq::SignalLength)
  {
    jam();
    c_restartRecord.m_senderData = 0;
  }
  if (c_nodeRestart || c_initialNodeRestart) {
    jam();

    CRASH_INSERTION(6000);

    BlockReference dictRef = calcDictBlockRef(c_masterNodeId);
    signal->theData[0] = getOwnNodeId();
    sendSignal(dictRef, GSN_GET_SCHEMA_INFOREQ, signal, 1, JBB);
    return;
  }
  ndbrequire(c_systemRestart);
  ndbrequire(c_masterNodeId == getOwnNodeId());

  c_schemaRecord.m_callback.m_callbackData = 0;
  c_schemaRecord.m_callback.m_callbackFunction =
    safe_cast(&Dbdict::masterRestart_checkSchemaStatusComplete);

  /**
   * master has same new/old schema file...
   *   copy old(read from disk) to new
   */
  {
    XSchemaFile * oldxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
    checkPendingSchemaTrans(oldxsf);
    XSchemaFile * newxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    newxsf->noOfPages = oldxsf->noOfPages;
    memcpy(&newxsf->schemaPage[0],
           &oldxsf->schemaPage[0],
           oldxsf->schemaPage[0].FileSize);
  }

  Callback cb =
    { safe_cast(&Dbdict::masterRestart_checkSchemaStatusComplete), 0 };
  startRestoreSchema(signal, cb);
}//execDICTSTARTREQ()

void
Dbdict::masterRestart_checkSchemaStatusComplete(Signal* signal,
						Uint32 callbackData,
						Uint32 returnCode)
{
  XSchemaFile * oldxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
  ndbrequire(oldxsf->noOfPages != 0);

  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)&oldxsf->schemaPage[0];
  ptr[0].sz = oldxsf->noOfPages * NDB_SF_PAGE_SIZE_IN_WORDS;

  c_sendSchemaRecord.m_SCHEMAINFO_Counter = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);

  rg.m_nodes.clear(getOwnNodeId());
  Callback c = { 0, 0 };
  sendFragmentedSignal(rg,
		       GSN_SCHEMA_INFO,
		       signal,
		       1, //SchemaInfo::SignalLength,
		       JBB,
		       ptr,
		       1,
		       c);

  XSchemaFile * newxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  newxsf->noOfPages = oldxsf->noOfPages;
  memcpy(&newxsf->schemaPage[0], &oldxsf->schemaPage[0],
         oldxsf->noOfPages * NDB_SF_PAGE_SIZE);

  signal->theData[0] = getOwnNodeId();
  sendSignal(reference(), GSN_SCHEMA_INFOCONF, signal, 1, JBB);
}

void
Dbdict::execGET_SCHEMA_INFOREQ(Signal* signal){

  const Uint32 ref = signal->getSendersBlockRef();
  //const Uint32 senderData = signal->theData[0];

  ndbrequire(c_sendSchemaRecord.inUse == false);
  c_sendSchemaRecord.inUse = true;

  LinearSectionPtr ptr[3];

  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  ndbrequire(xsf->noOfPages != 0);

  ptr[0].p = (Uint32*)&xsf->schemaPage[0];
  ptr[0].sz = xsf->noOfPages * NDB_SF_PAGE_SIZE_IN_WORDS;

  Callback c = { safe_cast(&Dbdict::sendSchemaComplete), 0 };
  sendFragmentedSignal(ref,
		       GSN_SCHEMA_INFO,
		       signal,
		       1, //GetSchemaInfoConf::SignalLength,
		       JBB,
		       ptr,
		       1,
		       c);
}//Dbdict::execGET_SCHEMA_INFOREQ()

void
Dbdict::sendSchemaComplete(Signal * signal,
			   Uint32 callbackData,
			   Uint32 returnCode){
  ndbrequire(c_sendSchemaRecord.inUse == true);
  c_sendSchemaRecord.inUse = false;

}


/* ---------------------------------------------------------------- */
// We receive the schema info from master as part of all restarts
// except the initial start where no tables exists.
/* ---------------------------------------------------------------- */
void Dbdict::execSCHEMA_INFO(Signal* signal)
{
  jamEntry();
  if(!assembleFragments(signal)){
    jam();
    return;
  }

  if(getNodeState().getNodeRestartInProgress()){
    CRASH_INSERTION(6001);
  }

  {
    /**
     * Copy "own" into new
     */
    XSchemaFile * oldxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
    XSchemaFile * newxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    memcpy(&newxsf->schemaPage[0],
           &oldxsf->schemaPage[0],
           oldxsf->schemaPage[0].FileSize);
  }

  SectionHandle handle(this, signal);
  SegmentedSectionPtr schemaDataPtr;
  handle.getSection(schemaDataPtr, 0);

  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
  ndbrequire(schemaDataPtr.sz % NDB_SF_PAGE_SIZE_IN_WORDS == 0);
  xsf->noOfPages = schemaDataPtr.sz / NDB_SF_PAGE_SIZE_IN_WORDS;
  copy((Uint32*)&xsf->schemaPage[0], schemaDataPtr);
  releaseSections(handle);

  SchemaFile * sf0 = &xsf->schemaPage[0];
  if (sf0->NdbVersion < NDB_SF_VERSION_5_0_6) {
    bool ok = convertSchemaFileTo_5_0_6(xsf);
    ndbrequire(ok);
  }

  if (sf0->NdbVersion < NDB_MAKE_VERSION(6,4,0))
  {
    jam();
    bool ok = convertSchemaFileTo_6_4(xsf);
    ndbrequire(ok);
  }

  validateChecksum(xsf);

  XSchemaFile * ownxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  checkPendingSchemaTrans(ownxsf);
  resizeSchemaFile(xsf, ownxsf->noOfPages);

  ndbrequire(signal->getSendersBlockRef() != reference());

  /* ---------------------------------------------------------------- */
  // Synchronise our view on data with other nodes in the cluster.
  // This is an important part of restart handling where we will handle
  // cases where the table have been added but only partially, where
  // tables have been deleted but not completed the deletion yet and
  // other scenarios needing synchronisation.
  /* ---------------------------------------------------------------- */
  Callback cb =
    { safe_cast(&Dbdict::restart_checkSchemaStatusComplete), 0 };
  startRestoreSchema(signal, cb);
}//execSCHEMA_INFO()

void
Dbdict::restart_checkSchemaStatusComplete(Signal * signal,
					  Uint32 callbackData,
					  Uint32 returnCode)
{
  jam();

  if(c_systemRestart)
  {
    jam();
    signal->theData[0] = getOwnNodeId();
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_SCHEMA_INFOCONF,
	       signal, 1, JBB);
    return;
  }

  ndbrequire(c_restartRecord.m_op_cnt == 0);
  ndbrequire(c_nodeRestart || c_initialNodeRestart);
  activateIndexes(signal, 0);
  return;
}

void Dbdict::execSCHEMA_INFOCONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);

/* ---------------------------------------------------------------- */
// This signal is received in the master as part of system restart
// from all nodes (including the master) after they have synchronised
// their data with the master node's schema information.
/* ---------------------------------------------------------------- */
  const Uint32 nodeId = signal->theData[0];
  c_sendSchemaRecord.m_SCHEMAINFO_Counter.clearWaitingFor(nodeId);

  if (!c_sendSchemaRecord.m_SCHEMAINFO_Counter.done()){
    jam();
    return;
  }//if
  activateIndexes(signal, 0);
}//execSCHEMA_INFOCONF()

static bool
checkSchemaStatus(Uint32 tableType, Uint32 pass)
{
  switch(tableType){
  case DictTabInfo::UndefTableType:
    return true;
  case DictTabInfo::HashIndexTrigger:
  case DictTabInfo::SubscriptionTrigger:
  case DictTabInfo::ReadOnlyConstraint:
  case DictTabInfo::IndexTrigger:
    return false;
  case DictTabInfo::LogfileGroup:
    return pass == 0 || pass == 11 || pass == 12;
  case DictTabInfo::Tablespace:
    return pass == 1 || pass == 10 || pass == 13;
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    return pass == 2 || pass == 9 || pass == 14;
  case DictTabInfo::HashMap:
    return pass == 3 || pass == 8 || pass == 15;
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
    return /* pass == 3 || pass == 7 || */ pass == 16;
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:
    return /* pass == 4 || pass == 6 || */ pass == 17;
  }

  return false;
}

static const Uint32 CREATE_OLD_PASS = 5;
static const Uint32 DROP_OLD_PASS = 11;
static const Uint32 CREATE_NEW_PASS = 17;
static const Uint32 LAST_PASS = 17;

NdbOut&
operator<<(NdbOut& out, const SchemaFile::TableEntry entry)
{
  out << "[";
  out << " state: " << entry.m_tableState;
  out << " version: " << hex << entry.m_tableVersion << dec;
  out << " type: " << entry.m_tableType;
  out << " words: " << entry.m_info_words;
  out << " gcp: " << entry.m_gcp;
  out << " trans: " << entry.m_transId;
  out << " ]";
  return out;
}

void Dbdict::initRestartRecord(Uint32 startpass, Uint32 lastpass,
                               const char * sb, const char * eb)
{
  c_restartRecord.gciToRestart = 0;
  c_restartRecord.activeTable = 0;
  c_restartRecord.m_op_cnt = 0;
  if (startpass == 0 && lastpass == 0)
  {
    jam();
    c_restartRecord.m_pass = 0;
    c_restartRecord.m_end_pass = LAST_PASS;
    c_restartRecord.m_start_banner = "Starting to restore schema";
    c_restartRecord.m_end_banner = "Restore of schema complete";
  }
  else
  {
    jam();
    c_restartRecord.m_pass = startpass;
    c_restartRecord.m_end_pass = lastpass;
    c_restartRecord.m_start_banner = sb;
    c_restartRecord.m_end_banner = eb;
  }
}//Dbdict::initRestartRecord()

/**
 * Pass 0 Create old LogfileGroup
 * Pass 1 Create old Tablespace
 * Pass 2 Create old Datafile/Undofile
 * Pass 3 Create old HashMap
 * Pass 4 Create old Table           // NOT DONE DUE TO DIH
 * Pass 5 Create old Index           // NOT DONE DUE TO DIH

 * Pass 6 Drop old Index             // NOT DONE DUE TO DIH
 * Pass 7 Drop old Table             // NOT DONE DUE TO DIH
 * Pass 8 Drop old HashMap
 * Pass 9 Drop old Datafile/Undofile
 * Pass 10 Drop old Tablespace
 * Pass 11 Drop old Logfilegroup

 * Pass 12 Create new LogfileGroup
 * Pass 13 Create new Tablespace
 * Pass 14 Create new Datafile/Undofile
 * Pass 15 Create new HashMap
 * Pass 16 Create new Table
 * Pass 17 Create new Index
 */

void Dbdict::checkSchemaStatus(Signal* signal)
{
  // masterxsf == schema file of master (i.e what's currently in cluster)
  // ownxsf = schema file read from disk
  XSchemaFile * masterxsf = &c_schemaFile[SchemaRecord::OLD_SCHEMA_FILE];
  XSchemaFile * ownxsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];

  ndbrequire(masterxsf->noOfPages == ownxsf->noOfPages);
  const Uint32 noOfEntries = masterxsf->noOfPages * NDB_SF_PAGE_ENTRIES;

  for (; c_restartRecord.activeTable < noOfEntries;
       c_restartRecord.activeTable++)
  {
    jam();

    Uint32 tableId = c_restartRecord.activeTable;
    SchemaFile::TableEntry *masterEntry = getTableEntry(masterxsf, tableId);
    SchemaFile::TableEntry *ownEntry = getTableEntry(ownxsf, tableId);
    SchemaFile::EntryState masterState =
      (SchemaFile::EntryState)masterEntry->m_tableState;
    SchemaFile::EntryState ownState =
      (SchemaFile::EntryState)ownEntry->m_tableState;

    if (c_restartRecord.activeTable >= c_noOfMetaTables)
    {
      jam();
      ndbrequire(masterState == SchemaFile::SF_UNUSED);
      ndbrequire(ownState == SchemaFile::SF_UNUSED);
      continue;
    }//if

    D("checkSchemaStatus" << V(*ownEntry) << V(*masterEntry));

//#define PRINT_SCHEMA_RESTART
#ifdef PRINT_SCHEMA_RESTART
    printf("checkSchemaStatus: pass: %d table: %d",
           c_restartRecord.m_pass, tableId);
    ndbout << "old: " << *ownEntry << " new: " << *masterEntry << endl;
#endif

    if (c_restartRecord.m_pass <= CREATE_OLD_PASS)
    {
      if (!::checkSchemaStatus(ownEntry->m_tableType, c_restartRecord.m_pass))
        continue;


      if (ownState == SchemaFile::SF_UNUSED)
        continue;

      restartCreateObj(signal, tableId, ownEntry, true);
      return;
    }

    if (c_restartRecord.m_pass <= DROP_OLD_PASS)
    {
      if (!::checkSchemaStatus(ownEntry->m_tableType, c_restartRecord.m_pass))
        continue;

      if (ownState != SchemaFile::SF_IN_USE)
        continue;

      if (* ownEntry == * masterEntry)
        continue;

      restartDropObj(signal, tableId, ownEntry);
      return;
    }

    if (c_restartRecord.m_pass <= CREATE_NEW_PASS)
    {
      if (!::checkSchemaStatus(masterEntry->m_tableType, c_restartRecord.m_pass))
        continue;

      if (masterState != SchemaFile::SF_IN_USE)
      {
        if (ownEntry->m_tableType != DictTabInfo::SchemaTransaction)
        {
          jam();
          ownEntry->init();
        }
        continue;
      }

      /**
       * handle table(index) special as DIH has already copied
       *   table (using COPY_TABREQ)
       */
      if (DictTabInfo::isIndex(masterEntry->m_tableType) ||
          DictTabInfo::isTable(masterEntry->m_tableType))
      {
        bool file = * ownEntry == *masterEntry &&
          (!DictTabInfo::isIndex(masterEntry->m_tableType) || c_systemRestart);

        restartCreateObj(signal, tableId, masterEntry, file);
        return;
      }

      if (* ownEntry == *masterEntry)
        continue;

      restartCreateObj(signal, tableId, masterEntry, false);
      return;
    }
  }

  if (c_restartRecord.m_op_cnt == 0)
  {
    jam();
    restartNextPass(signal);
    return;
  }
  else
  {
    jam();

    c_restartRecord.m_op_cnt = 0;

    TxHandlePtr tx_ptr;
    c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);

    Callback c = {
      safe_cast(&Dbdict::restartEndPass_fromEndTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;

    Uint32 flags = 0;
    endSchemaTrans(signal, tx_ptr, flags);
    return;
  }
}//checkSchemaStatus()

void
Dbdict::checkPendingSchemaTrans(XSchemaFile* xsf)
{
  for (Uint32 i = 0; i < xsf->noOfPages * NDB_SF_PAGE_ENTRIES; i++)
  {
    SchemaFile::TableEntry * transEntry = getTableEntry(xsf, i);

    if (transEntry->m_tableType == DictTabInfo::SchemaTransaction &&
        transEntry->m_transId != 0)
    {
      jam();

      bool commit = false;
      switch(transEntry->m_tableState){
      case SchemaFile::SF_STARTED:
      case SchemaFile::SF_PREPARE:
      case SchemaFile::SF_ABORT:
        jam();
        ndbout_c("Found pending trans (%u) - aborting", i);
        break;
      case SchemaFile::SF_COMMIT:
      case SchemaFile::SF_COMPLETE:
        jam();
        commit = true;
        ndbout_c("Found pending trans (%u) - committing", i);
        break;
      }

      const Uint32 transId = transEntry->m_transId;
      for (Uint32 j = 0; j<xsf->noOfPages * NDB_SF_PAGE_ENTRIES; j++)
      {
        SchemaFile::TableEntry * tmp = getTableEntry(xsf, j);
        if (tmp->m_transId == transId &&
            tmp->m_tableType != DictTabInfo::SchemaTransaction)
        {
          jam();
          tmp->m_transId = 0;
          switch(tmp->m_tableState){
          case SchemaFile::SF_CREATE:
            if (commit)
            {
              jam();
              tmp->m_tableState = SchemaFile::SF_IN_USE;
              ndbout_c("commit create %u", j);
            }
            else
            {
              jam();
              tmp->m_tableState = SchemaFile::SF_UNUSED;
              ndbout_c("abort create %u", j);
            }
            break;
          case SchemaFile::SF_ALTER:
            tmp->m_tableState = SchemaFile::SF_IN_USE;
            if (commit)
            {
              jam();
              ndbout_c("commit alter %u", j);
            }
            else
            {
              jam();
              ndbout_c("abort alter %u", j);
              tmp->m_tableVersion =
                alter_obj_dec_schema_version(tmp->m_tableVersion);
            }
            break;
          case SchemaFile::SF_DROP:
            if (commit)
            {
              jam();
              tmp->m_tableState = SchemaFile::SF_UNUSED;
              ndbout_c("commit drop %u", j);
            }
            else
            {
              jam();
              tmp->m_tableState = SchemaFile::SF_IN_USE;
              ndbout_c("abort drop %u", j);
            }
            break;
          }
        }
      }

      transEntry->m_tableType = DictTabInfo::UndefTableType;
      transEntry->m_tableState = SchemaFile::SF_UNUSED;
      transEntry->m_transId = 0;
    }
  }
}

void
Dbdict::startRestoreSchema(Signal* signal, Callback cb)
{
  jam();

  initRestartRecord();
  c_schemaRecord.m_callback = cb;

  TxHandlePtr tx_ptr;
  seizeTxHandle(tx_ptr);
  ndbrequire(!tx_ptr.isNull());

  c_restartRecord.m_tx_ptr_i = tx_ptr.i;
  tx_ptr.p->m_requestInfo = DictSignal::RF_LOCAL_TRANS;
  tx_ptr.p->m_userData = 0;

  Callback c = {
    safe_cast(&Dbdict::restart_fromBeginTrans),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;
  beginSchemaTrans(signal, tx_ptr);

  if (c_restartRecord.m_start_banner)
  {
    jam();
    infoEvent("%s", c_restartRecord.m_start_banner);
  }
}

void
Dbdict::restart_fromBeginTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  ndbrequire(ret == 0);

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  checkSchemaStatus(signal);
}

void
Dbdict::restart_nextOp(Signal* signal, bool commit)
{
  c_restartRecord.m_op_cnt++;

  Resource_limit rl;
  Uint32 free_words;
  m_ctx.m_mm.get_resource_limit(RG_SCHEMA_TRANS_MEMORY, rl);
  free_words = (rl.m_min - rl.m_curr) * GLOBAL_PAGE_SIZE_WORDS; // underestimate
  if (free_words < 2*MAX_WORDS_META_FILE)
  {
    jam();
    /**
     * Commit transaction now...so we don't risk overflowing
     *   c_opSectionBufferPool
     */
    c_restartRecord.m_op_cnt = ZRESTART_OPS_PER_TRANS;
  }

  if (commit || c_restartRecord.m_op_cnt >= ZRESTART_OPS_PER_TRANS)
  {
    jam();
    c_restartRecord.m_op_cnt = 0;

    TxHandlePtr tx_ptr;
    c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);

    Callback c = {
      safe_cast(&Dbdict::restart_fromEndTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;

    Uint32 flags = 0;
    endSchemaTrans(signal, tx_ptr, flags);
  }
  else
  {
    jam();
    c_restartRecord.activeTable++;
    checkSchemaStatus(signal);
  }
}

void
Dbdict::restart_fromEndTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (unlikely(hasError(tx_ptr.p->m_error)))
  {
    jam();
    /*
      Fatal error while restoring shchema during restart,
      dump debug info and crash
    */
    ndbout << "error: " << tx_ptr.p->m_error << endl;

    char msg[128];
    BaseString::snprintf(msg, sizeof(msg),
                         "Failed to restore schema during restart, error %u."
                         ,tx_ptr.p->m_error.errorCode);
    progError(__LINE__, NDBD_EXIT_RESTORE_SCHEMA, msg);
  }
  ndbrequire(ret == 0); //wl3600_todo

  releaseTxHandle(tx_ptr);

  ndbrequire(c_restartRecord.m_op_cnt == 0);
  c_restartRecord.activeTable++;

  seizeTxHandle(tx_ptr);
  ndbrequire(!tx_ptr.isNull());
  c_restartRecord.m_tx_ptr_i = tx_ptr.i;
  tx_ptr.p->m_requestInfo = DictSignal::RF_LOCAL_TRANS;
  tx_ptr.p->m_userData = 0;

  Callback c = {
    safe_cast(&Dbdict::restart_fromBeginTrans),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;
  beginSchemaTrans(signal, tx_ptr);
}

void
Dbdict::restartEndPass_fromEndTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (unlikely(hasError(tx_ptr.p->m_error)))
  {
    jam();
    /*
      Fatal error while restoring shchema during restart,
      dump debug info and crash
    */
    ndbout << "error: " << tx_ptr.p->m_error << endl;

    char msg[128];
    BaseString::snprintf(msg, sizeof(msg),
                         "Failed to restore schema during restart, error %u."
                         ,tx_ptr.p->m_error.errorCode);
    progError(__LINE__, NDBD_EXIT_RESTORE_SCHEMA, msg);
  }
  ndbrequire(ret == 0); //wl3600_todo

  releaseTxHandle(tx_ptr);
  c_restartRecord.m_tx_ptr_i = RNIL;

  restartNextPass(signal);
}

void
Dbdict::restartNextPass(Signal* signal)
{
  c_restartRecord.m_pass++;
  c_restartRecord.activeTable= 0;

  if (c_restartRecord.m_pass <= c_restartRecord.m_end_pass)
  {
    TxHandlePtr tx_ptr;
    if (c_restartRecord.m_tx_ptr_i == RNIL)
    {
      jam();
      seizeTxHandle(tx_ptr);
      ndbrequire(!tx_ptr.isNull());
      c_restartRecord.m_tx_ptr_i  = tx_ptr.i;
      tx_ptr.p->m_requestInfo = DictSignal::RF_LOCAL_TRANS;
      tx_ptr.p->m_userData = 0;

      Callback c = {
        safe_cast(&Dbdict::restart_fromBeginTrans),
        tx_ptr.p->tx_key
      };
      tx_ptr.p->m_callback = c;
      beginSchemaTrans(signal, tx_ptr);
      return;
    }
    else
    {
      jam();
      c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);
      restart_fromBeginTrans(signal, tx_ptr.p->tx_key, 0);
      return;
    }
  }
  else if (c_restartRecord.m_tx_ptr_i != RNIL)
  {
    /**
     * Complete last trans
     */
    jam();

    c_restartRecord.m_pass--;
    c_restartRecord.m_op_cnt = 0;

    TxHandlePtr tx_ptr;
    c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);

    Callback c = {
      safe_cast(&Dbdict::restartEndPass_fromEndTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;

    Uint32 flags = 0;
    endSchemaTrans(signal, tx_ptr, flags);
    return;
  }
  else
  {
    jam();

    ndbrequire(c_restartRecord.m_op_cnt == 0);

    /**
     * Write schema file at-end of checkSchemaStatus
     */
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;
    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.newFile = false;
    c_writeSchemaRecord.firstPage = 0;
    c_writeSchemaRecord.noOfPages = xsf->noOfPages;
    c_writeSchemaRecord.m_callback.m_callbackData = 0;
    c_writeSchemaRecord.m_callback.m_callbackFunction =
      safe_cast(&Dbdict::restart_fromWriteSchemaFile);

    for(Uint32 i = 0; i<xsf->noOfPages; i++)
      computeChecksum(xsf, i);

    startWriteSchemaFile(signal);
  }
}

void
Dbdict::restart_fromWriteSchemaFile(Signal* signal,
                                    Uint32 senderData,
                                    Uint32 retCode)
{
  if (c_restartRecord.m_end_banner)
  {
    jam();
    infoEvent("%s", c_restartRecord.m_end_banner);
  }

  execute(signal, c_schemaRecord.m_callback, retCode);
}

void
Dbdict::execGET_TABINFOREF(Signal* signal){
  jamEntry();
  /** 
   * Make copy of 'ref' such that we can build 'req' without overwriting 
   * source.
   */
  const GetTabInfoRef ref_copy =
    *reinterpret_cast<const GetTabInfoRef*>(signal->getDataPtr());

  if (ref_copy.errorCode == GetTabInfoRef::Busy)
  {
    jam();

    /**
     * Master is busy. Send delayed CONTINUEB to self to add some delay, then
     * send new GET_TABINFOREQ to master.
     */
    signal->getDataPtrSend()[0] = ZGET_TABINFO_RETRY;

    GetTabInfoReq* const req =
      reinterpret_cast<GetTabInfoReq*>(signal->getDataPtrSend()+1);
    memset(req, 0, sizeof *req);
    req->senderRef = reference();
    req->senderData = ref_copy.senderData;
    req->requestType =
      GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
    req->tableId = ref_copy.tableId;
    req->schemaTransId = ref_copy.schemaTransId;
    // Add a random 5-10ms delay.
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5 + rand()%6, 
                        GetTabInfoReq::SignalLength+1);
  }
  else
  {
    // Other error. Restart node.
    char msg[250];
    BaseString::snprintf(msg, sizeof(msg),
                         "Got GET_TABINFOREF from node %u with unexpected "
                         " error code %u", 
                         refToNode(signal->getSendersBlockRef()),
                         ref_copy.errorCode);
    progError(__LINE__, NDBD_EXIT_RESTORE_SCHEMA, msg);
  }
} // Dbdict::execGET_TABINFOREF()

void
Dbdict::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();

  switch(conf->tableType){
  case DictTabInfo::UndefTableType:
  case DictTabInfo::HashIndexTrigger:
  case DictTabInfo::SubscriptionTrigger:
  case DictTabInfo::ReadOnlyConstraint:
  case DictTabInfo::IndexTrigger:
    ndbrequire(false);
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:
    break;
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    if(refToBlock(conf->senderRef) == TSMAN
       && (refToNode(conf->senderRef) == 0
	   || refToNode(conf->senderRef) == getOwnNodeId()))
    {
      jam();
      FilePtr fg_ptr;
      ndbrequire(find_object(fg_ptr, conf->tableId));
      const Uint32 free_extents= conf->freeExtents;
      const Uint32 id= conf->tableId;
      const Uint32 type= conf->tableType;
      const Uint32 data= conf->senderData;
      signal->theData[0]= ZPACK_TABLE_INTO_PAGES;
      signal->theData[1]= id;
      signal->theData[2]= type;
      signal->theData[3]= data;
      signal->theData[4]= free_extents;
      sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
    }
    else if(refToBlock(conf->senderRef) == LGMAN
	    && (refToNode(conf->senderRef) == 0
		|| refToNode(conf->senderRef) == getOwnNodeId()))
    {
      jam();
      FilegroupPtr fg_ptr;
      ndbrequire(find_object(fg_ptr, conf->tableId));
      const Uint32 free_hi= conf->freeWordsHi;
      const Uint32 free_lo= conf->freeWordsLo;
      const Uint32 id= conf->tableId;
      const Uint32 type= conf->tableType;
      const Uint32 data= conf->senderData;
      signal->theData[0]= ZPACK_TABLE_INTO_PAGES;
      signal->theData[1]= id;
      signal->theData[2]= type;
      signal->theData[3]= data;
      signal->theData[4]= free_hi;
      signal->theData[5]= free_lo;
      sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
    }
    else
    {
      jam();
      break;
    }
    return;
  }

  restartCreateObj_getTabInfoConf(signal);
}

/**
 * Create Obj during NR/SR
 */
void
Dbdict::restartCreateObj(Signal* signal,
			 Uint32 tableId,
			 const SchemaFile::TableEntry * new_entry,
			 bool file){
  jam();


#ifdef PRINT_SCHEMA_RESTART
  ndbout_c("restartCreateObj table: %u file: %u", tableId, Uint32(file));
#endif

  c_restartRecord.m_entry = *new_entry;
  if(file)
  {
    c_readTableRecord.no_of_words = new_entry->m_info_words;
    c_readTableRecord.pageId = 0;
    c_readTableRecord.m_callback.m_callbackData = tableId;
    c_readTableRecord.m_callback.m_callbackFunction =
      safe_cast(&Dbdict::restartCreateObj_readConf);

    ndbout_c("restartCreateObj(%u) file: %u", tableId, file);
    startReadTableFile(signal, tableId);
  }
  else
  {
    /**
     * Get from master
     */
    GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = tableId;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_GET_TABINFOREQ, signal,
	       GetTabInfoReq::SignalLength, JBB);
  }
}

void
Dbdict::restartCreateObj_getTabInfoConf(Signal* signal)
{
  jam();

  SectionHandle handle(this, signal);
  SegmentedSectionPtr objInfoPtr;
  handle.getSection(objInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  handle.clear();

  restartCreateObj_parse(signal, objInfoPtr, false);
}

void
Dbdict::restartCreateObj_readConf(Signal* signal,
				  Uint32 callbackData,
				  Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);

  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_readTableRecord.pageId);

  Uint32 sz = c_readTableRecord.no_of_words;

  Ptr<SectionSegment> ptr;
  ndbrequire(import(ptr, pageRecPtr.p->word+ZPAGE_HEADER_SIZE, sz));
  SegmentedSectionPtr tmp(sz, ptr.i, ptr.p);
  restartCreateObj_parse(signal, tmp, true);
}

void
Dbdict::restartCreateObj_parse(Signal* signal,
                               SegmentedSectionPtr ptr,
                               bool file)
{
  jam();
  SchemaOpPtr op_ptr;

  TxHandlePtr tx_ptr;
  c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);

  SchemaTransPtr trans_ptr;
  findSchemaTrans(trans_ptr, tx_ptr.p->m_transKey);

  switch(c_restartRecord.m_entry.m_tableType){
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:
  {
    CreateTableRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    break;
  }
  case DictTabInfo::Undofile:
  case DictTabInfo::Datafile:
  {
    CreateFileRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    break;
  }
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
  {
    CreateFilegroupRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    break;
  }
  case DictTabInfo::HashMap:
  {
    CreateHashMapRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    break;
  }
  }

  op_ptr.p->m_restart = file ? 1 : 2;
  op_ptr.p->m_state = SchemaOp::OS_PARSE_MASTER;

  SectionHandle handle(this, ptr.i);
  ErrorInfo error;
  const OpInfo& info = getOpInfo(op_ptr);
  (this->*(info.m_parse))(signal, false, op_ptr, handle, error);
  releaseSections(handle);
  if (unlikely(hasError(error)))
  {
    jam();
    /*
      Fatal error while restoring shchema during restart,
      dump debug info and crash
    */
    ndbout << "error: " << error << endl;

    char msg[128];
    BaseString::snprintf(msg, sizeof(msg),
                         "Failed to recreate object %u during restart,"
                         " error %u."
                         ,c_restartRecord.activeTable
                         ,error.errorCode);
    progError(__LINE__, NDBD_EXIT_RESTORE_SCHEMA, msg);
  }
  ndbrequire(!hasError(error));

  restart_nextOp(signal);
}

/**
 * Drop object during NR/SR
 */
void
Dbdict::restartDropObj(Signal* signal,
                       Uint32 tableId,
                       const SchemaFile::TableEntry * entry)
{
  jam();
  c_restartRecord.m_entry = *entry;

  jam();
  SchemaOpPtr op_ptr;

  TxHandlePtr tx_ptr;
  c_txHandleHash.getPtr(tx_ptr, c_restartRecord.m_tx_ptr_i);

  SchemaTransPtr trans_ptr;
  findSchemaTrans(trans_ptr, tx_ptr.p->m_transKey);

  switch(c_restartRecord.m_entry.m_tableType){
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:
    DropTableRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    ndbrequire(false);
    break;
  case DictTabInfo::Undofile:
  case DictTabInfo::Datafile:
  {
    DropFileRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    opRecPtr.p->m_request.file_id = tableId;
    opRecPtr.p->m_request.file_version = entry->m_tableVersion;
    break;
  }
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
  {
    DropFilegroupRecPtr opRecPtr;
    seizeSchemaOp(trans_ptr, op_ptr, opRecPtr);
    opRecPtr.p->m_request.filegroup_id = tableId;
    opRecPtr.p->m_request.filegroup_version = entry->m_tableVersion;
    break;
  }
  }

  ndbout_c("restartDropObj(%u)", tableId);

  op_ptr.p->m_restart = 1; //
  op_ptr.p->m_state = SchemaOp::OS_PARSE_MASTER;

  SectionHandle handle(this);
  ErrorInfo error;
  const OpInfo& info = getOpInfo(op_ptr);
  (this->*(info.m_parse))(signal, false, op_ptr, handle, error);
  releaseSections(handle);
  if (unlikely(hasError(error)))
  {
    jam();
    /*
      Fatal error while restoring shchema during restart,
      dump debug info and crash
    */
    ndbout << "error: " << error << endl;

    char msg[128];
    BaseString::snprintf(msg, sizeof(msg),
                         "Failed to drop object %u during restart, error %u"
                         ,c_restartRecord.activeTable
                         ,error.errorCode);
    progError(__LINE__, NDBD_EXIT_RESTORE_SCHEMA, msg);
  }
  ndbrequire(!hasError(error));

  restart_nextOp(signal);
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          NODE FAILURE HANDLING ------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code that is used when nodes            */
/* (kernel/api) fails.                                              */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

void Dbdict::handleApiFailureCallback(Signal* signal,
                                      Uint32 failedNodeId,
                                      Uint32 ignoredRc)
{
  jamEntry();

  signal->theData[0] = failedNodeId;
  signal->theData[1] = reference();
  sendSignal(QMGR_REF, GSN_API_FAILCONF, signal, 2, JBB);
}

/* ---------------------------------------------------------------- */
// We receive a report of an API that failed.
/* ---------------------------------------------------------------- */
void Dbdict::execAPI_FAILREQ(Signal* signal)
{
  jamEntry();
  Uint32 failedApiNode = signal->theData[0];
  BlockReference retRef = signal->theData[1];

  ndbrequire(retRef == QMGR_REF); // As callback hard-codes QMGR_REF
#if 0
  Uint32 userNode = refToNode(c_connRecord.userBlockRef);
  if (userNode == failedApiNode) {
    jam();
    c_connRecord.userBlockRef = (Uint32)-1;
  }//if
#endif

  // sends API_FAILCONF when done
  handleApiFail(signal, failedApiNode);
}//execAPI_FAILREQ()

void Dbdict::handleNdbdFailureCallback(Signal* signal,
                                       Uint32 failedNodeId,
                                       Uint32 ignoredRc)
{
  jamEntry();

  /* Node failure handling is complete */
  NFCompleteRep * const nfCompRep = (NFCompleteRep *)&signal->theData[0];
  nfCompRep->blockNo      = DBDICT;
  nfCompRep->nodeId       = getOwnNodeId();
  nfCompRep->failedNodeId = failedNodeId;
  sendSignal(DBDIH_REF, GSN_NF_COMPLETEREP, signal,
             NFCompleteRep::SignalLength, JBB);
}

/* ---------------------------------------------------------------- */
// We receive a report of one or more node failures of kernel nodes.
/* ---------------------------------------------------------------- */
void Dbdict::execNODE_FAILREP(Signal* signal)
{
  jamEntry();
  NodeFailRep nodeFailRep = *(NodeFailRep *)&signal->theData[0];
  NodeFailRep * nodeFail = &nodeFailRep;
  NodeRecordPtr ownNodePtr;

  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  c_failureNr  = nodeFail->failNo;
  const Uint32 numberOfFailedNodes  = nodeFail->noOfNodes;
  const bool masterFailed = (c_masterNodeId != nodeFail->masterNodeId);
  c_masterNodeId = nodeFail->masterNodeId;

  c_noNodesFailed += numberOfFailedNodes;
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  memcpy(theFailedNodes, nodeFail->theNodes, sizeof(theFailedNodes));
  c_counterMgr.execNODE_FAILREP(signal);

  NdbNodeBitmask failedNodes;
  failedNodes.assign(NdbNodeBitmask::Size, theFailedNodes);
  c_aliveNodes.bitANDC(failedNodes);
  if (masterFailed && c_masterNodeId == getOwnNodeId())
  {
    /*
      Master node has failed, we need to take over
      any pending transaction(s) and decide if to
      rollforward or rollback.
     */
    jam();
    ownNodePtr.p->nodeState = NodeRecord::NDB_MASTER_TAKEOVER;
    ownNodePtr.p->nodeFailRep = nodeFailRep;
    infoEvent("Node %u taking over as DICT master", c_masterNodeId);
    handle_master_takeover(signal);
    return;
  }

  send_nf_complete_rep(signal, &nodeFailRep);
  return;
}//execNODE_FAILREP()

void Dbdict::send_nf_complete_rep(Signal* signal, const NodeFailRep* nodeFail)
{
  jam();
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  memcpy(theFailedNodes, nodeFail->theNodes, sizeof(theFailedNodes));
  NdbNodeBitmask tmp;
  tmp.assign(NdbNodeBitmask::Size, theFailedNodes);

  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  ownNodePtr.p->nodeState = NodeRecord::NDB_NODE_ALIVE; // reset take-over

  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(tmp.get(i)) {
      jam();
      NodeRecordPtr nodePtr;
      c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
      ndbout_c("Sending NF_COMPLETEREP for node %u", i);
#endif
      nodePtr.p->nodeState = NodeRecord::NDB_NODE_DEAD;

      Callback cb = {safe_cast(&Dbdict::handleNdbdFailureCallback),
                     i};

      simBlockNodeFailure(signal, nodePtr.i, cb);
    }//if
  }//for

  /*
   * NODE_FAILREP guarantees that no "in flight" signal from
   * a dead node is accepted, and also that the job buffer contains
   * no such (un-executed) signals.  Therefore no DICT_UNLOCK_ORD
   * from a dead node (leading to master crash) is possible after
   * this clean-up removes the lock record.
   */
  removeStaleDictLocks(signal, theFailedNodes);

  c_sub_startstop_lock.bitANDC(tmp);
}//send_nf_complete_rep

void Dbdict::handle_master_takeover(Signal* signal)
{
  /*
    This is the new master, handle take-over of
    pending schema transactions.
    Ask all slave nodes about state of any pending
    transactions
  */
  jam();
  NodeRecordPtr masterNodePtr;
  c_nodes.getPtr(masterNodePtr, c_masterNodeId);

  masterNodePtr.p->m_nodes = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, masterNodePtr.p->m_nodes);
  {
    SafeCounter sc(c_counterMgr, masterNodePtr.p->m_counter);
    bool ok = sc.init<DictTakeoverRef>(rg, c_masterNodeId);
    ndbrequire(ok);
  }
  DictTakeoverReq* req = (DictTakeoverReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  c_takeOverInProgress = true;
  sendSignal(rg, GSN_DICT_TAKEOVER_REQ, signal,
               DictTakeoverReq::SignalLength, JBB);
}


/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          NODE START HANDLING --------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code that is used when kernel nodes     */
/* starts.                                                          */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

/* ---------------------------------------------------------------- */
// Include a starting node in list of nodes to be part of adding
// and dropping tables.
/* ---------------------------------------------------------------- */
void Dbdict::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  NodeRecordPtr nodePtr;
  BlockReference retRef = signal->theData[0];
  nodePtr.i = signal->theData[1];

  ndbrequire(c_noNodesFailed > 0);
  c_noNodesFailed--;

  c_nodes.getPtr(nodePtr);
  ndbrequire(nodePtr.p->nodeState == NodeRecord::NDB_NODE_DEAD);
  nodePtr.p->nodeState = NodeRecord::NDB_NODE_ALIVE;
  signal->theData[0] = nodePtr.i;
  signal->theData[1] = reference();
  sendSignal(retRef, GSN_INCL_NODECONF, signal, 2, JBB);

  c_aliveNodes.set(nodePtr.i);
}//execINCL_NODEREQ()

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          ADD TABLE HANDLING ---------------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code that is used when adding a table.  */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

inline
void Dbdict::printTables()
{
  DictObjectName_hash::Iterator iter;
  bool moreTables = c_obj_name_hash.first(iter);
  printf("OBJECTS IN DICT:\n");
  char name[PATH_MAX];
  while (moreTables) {
    DictObjectPtr tablePtr = iter.curr;
    ConstRope r(c_rope_pool, tablePtr.p->m_name);
    r.copy(name);
    printf("%s ", name);
    moreTables = c_obj_name_hash.next(iter);
  }
  printf("\n");
}

#define tabRequire(cond, error) \
  if (!(cond)) { \
    jam();    \
    parseP->errorCode = error; parseP->errorLine = __LINE__; \
    parseP->errorKey = it.getKey(); \
    return;   \
  }//if

// handleAddTableFailure(signal, __LINE__, allocatedTable);

Dbdict::DictObject *
Dbdict::get_object(const char * name, Uint32 len, Uint32 hash){
  DictObjectPtr old_ptr;
  if (get_object(old_ptr, name, len, hash))
  {
    return old_ptr.p;
  }
  return 0;
}

bool
Dbdict::get_object(DictObjectPtr& obj_ptr, const char* name, Uint32 len, Uint32 hash)
{
  DictObject key;
  key.m_key.m_name_ptr = name;
  key.m_key.m_name_len = len;
  key.m_key.m_pool = &c_rope_pool;
  key.m_name.m_hash = hash;
  return c_obj_name_hash.find(obj_ptr, key);
}

void
Dbdict::release_object(Uint32 obj_ptr_i, DictObject* obj_ptr_p){
  jam();
  RopeHandle obj_name = obj_ptr_p->m_name;
  DictObjectPtr ptr = { obj_ptr_p, obj_ptr_i };

  LocalRope name(c_rope_pool, obj_name);
  name.erase();

jam();
  c_obj_name_hash.remove(ptr);
jam();
  c_obj_id_hash.remove(ptr);
jam();
  c_obj_pool.release(ptr);
jam();
}

void
Dbdict::increase_ref_count(Uint32 obj_ptr_i)
{
  DictObject* ptr = c_obj_pool.getPtr(obj_ptr_i);
  ptr->m_ref_count++;
}

void
Dbdict::decrease_ref_count(Uint32 obj_ptr_i)
{
  DictObject* ptr = c_obj_pool.getPtr(obj_ptr_i);
  ndbrequire(ptr->m_ref_count);
  ptr->m_ref_count--;
}

void Dbdict::handleTabInfoInit(Signal * signal, SchemaTransPtr & trans_ptr,
                               SimpleProperties::Reader & it,
			       ParseDictTabInfoRecord * parseP,
			       bool checkExist)
{
/* ---------------------------------------------------------------- */
// We always start by handling table name since this must be the first
// item in the list. Through the table name we can derive if it is a
// correct name, a new name or an already existing table.
/* ---------------------------------------------------------------- */

  it.first();

  SimpleProperties::UnpackStatus status;
  c_tableDesc.init();
  status = SimpleProperties::unpack(it, &c_tableDesc,
				    DictTabInfo::TableMapping,
				    DictTabInfo::TableMappingSize,
				    true, true);

  if(status != SimpleProperties::Break){
    parseP->errorCode = CreateTableRef::InvalidFormat;
    parseP->status    = status;
    parseP->errorKey  = it.getKey();
    parseP->errorLine = __LINE__;
    return;
  }

  if(parseP->requestType == DictTabInfo::AlterTableFromAPI)
  {
    ndbrequire(!checkExist);
  }
  if(!checkExist)
  {
    ndbrequire(parseP->requestType == DictTabInfo::AlterTableFromAPI);
  }

  /* ---------------------------------------------------------------- */
  // Verify that table name is an allowed table name.
  // TODO
  /* ---------------------------------------------------------------- */
  const Uint32 tableNameLength = Uint32(strlen(c_tableDesc.TableName) + 1);
  const Uint32 name_hash = LocalRope::hash(c_tableDesc.TableName, tableNameLength);

  if(checkExist){
    jam();
    tabRequire(get_object(c_tableDesc.TableName, tableNameLength) == 0,
	       CreateTableRef::TableAlreadyExist);
  }

  if (DictTabInfo::isIndex(c_tableDesc.TableType))
  {
    jam();
    parseP->requestType = DictTabInfo::AddTableFromDict;
  }

  TableRecordPtr tablePtr;
  Uint32 schemaFileId;
  switch (parseP->requestType) {
  case DictTabInfo::CreateTableFromAPI: {
    jam();
  }
  case DictTabInfo::AlterTableFromAPI:{
    jam();
    schemaFileId = RNIL;
    bool ok = seizeTableRecord(tablePtr,schemaFileId);
    tabRequire(ok, CreateTableRef::NoMoreTableRecords);
    break;
  }
  case DictTabInfo::AddTableFromDict:
  case DictTabInfo::ReadTableFromDiskSR:
  case DictTabInfo::GetTabInfoConf:
  {
/* ---------------------------------------------------------------- */
// Get table id and check that table doesn't already exist
/* ---------------------------------------------------------------- */
    if (parseP->requestType == DictTabInfo::ReadTableFromDiskSR) {
      ndbrequire(c_tableDesc.TableId == c_restartRecord.activeTable);
    }//if
    if (parseP->requestType == DictTabInfo::GetTabInfoConf) {
      ndbrequire(c_tableDesc.TableId == c_restartRecord.activeTable);
    }//if

    schemaFileId = c_tableDesc.TableId;
    bool ok = seizeTableRecord(tablePtr,schemaFileId);
    ndbrequire(ok); // Already exists or out of memory
/* ---------------------------------------------------------------- */
// Set table version
/* ---------------------------------------------------------------- */
    Uint32 tableVersion = c_tableDesc.TableVersion;
    tablePtr.p->tableVersion = tableVersion;

    break;
  }
  default:
    ndbrequire(false);
    break;
  }//switch

  {
    LocalRope name(c_rope_pool, tablePtr.p->tableName);
    tabRequire(name.assign(c_tableDesc.TableName, tableNameLength, name_hash),
	       CreateTableRef::OutOfStringBuffer);
  }

  if (parseP->requestType != DictTabInfo::AlterTableFromAPI) {
    jam();

    DictObjectPtr obj_ptr;
    ndbrequire(c_obj_pool.seize(obj_ptr));
    new (obj_ptr.p) DictObject;
    obj_ptr.p->m_id = schemaFileId;
    obj_ptr.p->m_type = c_tableDesc.TableType;
    obj_ptr.p->m_name = tablePtr.p->tableName;
    obj_ptr.p->m_ref_count = 0;
    ndbrequire(link_object(obj_ptr, tablePtr));
    c_obj_id_hash.add(obj_ptr);
    c_obj_name_hash.add(obj_ptr);

    if (g_trace)
    {
      g_eventLogger->info("Dbdict: %u: create name=%s,id=%u,obj_ptr_i=%d",__LINE__,
                          c_tableDesc.TableName,
                          schemaFileId, tablePtr.p->m_obj_ptr_i);
    }
    send_event(signal, trans_ptr,
               NDB_LE_CreateSchemaObject,
               schemaFileId,
               tablePtr.p->tableVersion,
               c_tableDesc.TableType);
  }
  parseP->tablePtr = tablePtr;

  // Disallow logging of a temporary table.
  tabRequire(!(c_tableDesc.TableTemporaryFlag && c_tableDesc.TableLoggedFlag),
             CreateTableRef::NoLoggingTemporaryTable);

  tablePtr.p->noOfAttributes = c_tableDesc.NoOfAttributes;
  tablePtr.p->m_bits |=
    (c_tableDesc.TableLoggedFlag ? TableRecord::TR_Logged : 0);
  tablePtr.p->m_bits |=
    (c_tableDesc.RowChecksumFlag ? TableRecord::TR_RowChecksum : 0);
  tablePtr.p->m_bits |=
    (c_tableDesc.RowGCIFlag ? TableRecord::TR_RowGCI : 0);
#if DOES_NOT_WORK_CURRENTLY
  tablePtr.p->m_bits |=
    (c_tableDesc.TableTemporaryFlag ? TableRecord::TR_Temporary : 0);
#endif
  tablePtr.p->m_bits |=
    (c_tableDesc.ForceVarPartFlag ? TableRecord::TR_ForceVarPart : 0);
  tablePtr.p->minLoadFactor = c_tableDesc.MinLoadFactor;
  tablePtr.p->maxLoadFactor = c_tableDesc.MaxLoadFactor;
  tablePtr.p->fragmentType = (DictTabInfo::FragmentType)c_tableDesc.FragmentType;
  tablePtr.p->tableType = (DictTabInfo::TableType)c_tableDesc.TableType;
  tablePtr.p->kValue = c_tableDesc.TableKValue;
  tablePtr.p->fragmentCount = c_tableDesc.FragmentCount;
  tablePtr.p->m_tablespace_id = c_tableDesc.TablespaceId;
  tablePtr.p->maxRowsLow = c_tableDesc.MaxRowsLow;
  tablePtr.p->maxRowsHigh = c_tableDesc.MaxRowsHigh;
  tablePtr.p->minRowsLow = c_tableDesc.MinRowsLow;
  tablePtr.p->minRowsHigh = c_tableDesc.MinRowsHigh;
  tablePtr.p->defaultNoPartFlag = c_tableDesc.DefaultNoPartFlag;
  tablePtr.p->linearHashFlag = c_tableDesc.LinearHashFlag;
  tablePtr.p->singleUserMode = c_tableDesc.SingleUserMode;
  tablePtr.p->hashMapObjectId = c_tableDesc.HashMapObjectId;
  tablePtr.p->hashMapVersion = c_tableDesc.HashMapVersion;
  tablePtr.p->storageType = c_tableDesc.TableStorageType;
  tablePtr.p->m_extra_row_gci_bits = c_tableDesc.ExtraRowGCIBits;
  tablePtr.p->m_extra_row_author_bits = c_tableDesc.ExtraRowAuthorBits;

  tabRequire(tablePtr.p->noOfAttributes <= MAX_ATTRIBUTES_IN_TABLE,
             CreateTableRef::NoMoreAttributeRecords); // bad error code!

  if (tablePtr.p->fragmentType == DictTabInfo::HashMapPartition &&
      tablePtr.p->hashMapObjectId == RNIL)
  {
    Uint32 fragments = tablePtr.p->fragmentCount;
    if (fragments == 0)
    {
      jam();
      tablePtr.p->fragmentCount = fragments = get_default_fragments(signal);
    }

    tabRequire(fragments <= MAX_NDB_PARTITIONS,
               CreateTableRef::TooManyFragments);

    char buf[MAX_TAB_NAME_SIZE+1];
    BaseString::snprintf(buf, sizeof(buf), "DEFAULT-HASHMAP-%u-%u",
                         NDB_DEFAULT_HASHMAP_BUCKETS,
                         fragments);
    DictObject* dictObj = get_object(buf);
    if (dictObj && dictObj->m_type == DictTabInfo::HashMap)
    {
      jam();
      HashMapRecordPtr hm_ptr;
      ndbrequire(find_object(hm_ptr, dictObj->m_id));
      tablePtr.p->hashMapObjectId = hm_ptr.p->m_object_id;
      tablePtr.p->hashMapVersion = hm_ptr.p->m_object_version;
    }
  }

  if (tablePtr.p->fragmentType == DictTabInfo::HashMapPartition)
  {
    jam();
    HashMapRecordPtr hm_ptr;
    tabRequire(find_object(hm_ptr, tablePtr.p->hashMapObjectId),
               CreateTableRef::InvalidHashMap);

    tabRequire(hm_ptr.p->m_object_version ==  tablePtr.p->hashMapVersion,
               CreateTableRef::InvalidHashMap);

    Ptr<Hash2FragmentMap> mapptr;
    g_hash_map.getPtr(mapptr, hm_ptr.p->m_map_ptr_i);

    if (tablePtr.p->fragmentCount == 0)
    {
      jam();
      tablePtr.p->fragmentCount = mapptr.p->m_fragments;
    }
    else
    {
      tabRequire(mapptr.p->m_fragments == tablePtr.p->fragmentCount,
                 CreateTableRef::InvalidHashMap);
    }
  }

  {
    LocalRope frm(c_rope_pool, tablePtr.p->frmData);
    tabRequire(frm.assign(c_tableDesc.FrmData, c_tableDesc.FrmLen),
	       CreateTableRef::OutOfStringBuffer);
    LocalRope range(c_rope_pool, tablePtr.p->rangeData);
    tabRequire(range.assign((const char*)c_tableDesc.RangeListData,
               c_tableDesc.RangeListDataLen),
	      CreateTableRef::OutOfStringBuffer);
    LocalRope fd(c_rope_pool, tablePtr.p->ngData);
    tabRequire(fd.assign((const char*)c_tableDesc.FragmentData,
                         c_tableDesc.FragmentDataLen),
	       CreateTableRef::OutOfStringBuffer);
  }

  c_fragDataLen = c_tableDesc.FragmentDataLen;
  memcpy(c_fragData, c_tableDesc.FragmentData,
         c_tableDesc.FragmentDataLen);

  if(c_tableDesc.PrimaryTableId != RNIL)
  {
    jam();
    tablePtr.p->primaryTableId = c_tableDesc.PrimaryTableId;
    tablePtr.p->indexState = (TableRecord::IndexState)c_tableDesc.IndexState;

    if (getNodeState().getSystemRestartInProgress())
    {
      jam();
      tablePtr.p->triggerId = RNIL;
    }
    else
    {
      jam();
      tablePtr.p->triggerId = c_tableDesc.CustomTriggerId;

      if (c_tableDesc.InsertTriggerId != RNIL ||
          c_tableDesc.UpdateTriggerId != RNIL ||
          c_tableDesc.DeleteTriggerId != RNIL)
      {
        jam();
        /**
         * Upgrade...unique index
         */
        ndbrequire(tablePtr.p->isUniqueIndex());
        ndbrequire(c_tableDesc.CustomTriggerId == RNIL);
        ndbrequire(c_tableDesc.InsertTriggerId != RNIL);
        ndbrequire(c_tableDesc.UpdateTriggerId != RNIL);
        ndbrequire(c_tableDesc.DeleteTriggerId != RNIL);
        ndbout_c("table: %u UPGRADE saving (%u/%u/%u)",
                 schemaFileId,
                 c_tableDesc.InsertTriggerId,
                 c_tableDesc.UpdateTriggerId,
                 c_tableDesc.DeleteTriggerId);
        infoEvent("table: %u UPGRADE saving (%u/%u/%u)",
                  schemaFileId,
                  c_tableDesc.InsertTriggerId,
                  c_tableDesc.UpdateTriggerId,
                  c_tableDesc.DeleteTriggerId);
        tablePtr.p->triggerId = c_tableDesc.InsertTriggerId;
        tablePtr.p->m_upgrade_trigger_handling.m_upgrade = true;
        tablePtr.p->m_upgrade_trigger_handling.insertTriggerId = c_tableDesc.InsertTriggerId;
        tablePtr.p->m_upgrade_trigger_handling.updateTriggerId = c_tableDesc.UpdateTriggerId;
        tablePtr.p->m_upgrade_trigger_handling.deleteTriggerId = c_tableDesc.DeleteTriggerId;

        upgrade_seizeTrigger(tablePtr,
                             c_tableDesc.InsertTriggerId,
                             c_tableDesc.UpdateTriggerId,
                             c_tableDesc.DeleteTriggerId);
      }
    }
  }
  else
  {
    jam();
    tablePtr.p->primaryTableId = RNIL;
    tablePtr.p->indexState = TableRecord::IS_UNDEFINED;
    tablePtr.p->triggerId = RNIL;
  }
  tablePtr.p->buildTriggerId = RNIL;

  handleTabInfo(it, parseP, c_tableDesc);

  if(parseP->errorCode != 0)
  {
    /**
     * Release table
     */
    releaseTableObject(tablePtr.i, checkExist);
    parseP->tablePtr.setNull();
    return;
  }

  if (checkExist && tablePtr.p->m_tablespace_id != RNIL)
  {
    /**
     * Increase ref count
     */
    FilegroupPtr ptr;
    ndbrequire(find_object(ptr, tablePtr.p->m_tablespace_id));
    increase_ref_count(ptr.p->m_obj_ptr_i);
  }
}//handleTabInfoInit()

void
Dbdict::upgrade_seizeTrigger(TableRecordPtr tabPtr,
                             Uint32 insertTriggerId,
                             Uint32 updateTriggerId,
                             Uint32 deleteTriggerId)
{
  /**
   * The insert trigger will be "main" trigger so
   *   it does not need any special treatment
   */
  const Uint32 size = c_triggerRecordPool_.getSize();
  ndbrequire(updateTriggerId == RNIL || updateTriggerId < size);
  ndbrequire(deleteTriggerId == RNIL || deleteTriggerId < size);

  DictObjectPtr tab_obj_ptr;
  c_obj_pool.getPtr(tab_obj_ptr, tabPtr.p->m_obj_ptr_i);

  TriggerRecordPtr triggerPtr;
  if (updateTriggerId != RNIL)
  {
    jam();
    bool ok = find_object(triggerPtr, updateTriggerId);
    if (!ok)
    {
      jam();
      bool ok = seizeTriggerRecord(triggerPtr, updateTriggerId);
      if (!ok)
      {
        jam();
        ndbrequire(ok);
      }
      triggerPtr.p->triggerState = TriggerRecord::TS_FAKE_UPGRADE;
      triggerPtr.p->tableId = tabPtr.p->primaryTableId;
      triggerPtr.p->indexId = tab_obj_ptr.p->m_id;
      TriggerInfo::packTriggerInfo(triggerPtr.p->triggerInfo,
                                   g_hashIndexTriggerTmpl[0].triggerInfo);

      char buf[256];
      BaseString::snprintf(buf, sizeof(buf),
                           "UPG_UPD_NDB$INDEX_%u_UI", tab_obj_ptr.p->m_id);
      {
        LocalRope name(c_rope_pool, triggerPtr.p->triggerName);
        name.assign(buf);
      }

      DictObjectPtr obj_ptr;
      ok = c_obj_pool.seize(obj_ptr);
      ndbrequire(ok);
      new (obj_ptr.p) DictObject();

      obj_ptr.p->m_name = triggerPtr.p->triggerName;
      obj_ptr.p->m_ref_count = 0;

      obj_ptr.p->m_id = triggerPtr.p->triggerId;
      obj_ptr.p->m_type =TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
      link_object(obj_ptr, triggerPtr);
      c_obj_name_hash.add(obj_ptr);
      c_obj_id_hash.add(obj_ptr);
    }
  }

  if (deleteTriggerId != RNIL)
  {
    jam();
    bool ok = find_object(triggerPtr, deleteTriggerId); // TODO: msundell seizeTriggerRecord
    if (!ok)
    {
      jam();
      bool ok = seizeTriggerRecord(triggerPtr, deleteTriggerId);
      if (!ok)
      {
        jam();
        ndbrequire(ok);
      }
      triggerPtr.p->triggerState = TriggerRecord::TS_FAKE_UPGRADE;
      triggerPtr.p->tableId = tabPtr.p->primaryTableId;
      triggerPtr.p->indexId = tab_obj_ptr.p->m_id;
      TriggerInfo::packTriggerInfo(triggerPtr.p->triggerInfo,
                                   g_hashIndexTriggerTmpl[0].triggerInfo);
      char buf[256];
      BaseString::snprintf(buf, sizeof(buf),
                           "UPG_DEL_NDB$INDEX_%u_UI", tab_obj_ptr.p->m_id);

      {
        LocalRope name(c_rope_pool, triggerPtr.p->triggerName);
        name.assign(buf);
      }

      DictObjectPtr obj_ptr;
      ok = c_obj_pool.seize(obj_ptr);
      ndbrequire(ok);
      new (obj_ptr.p) DictObject();

      obj_ptr.p->m_name = triggerPtr.p->triggerName;
      obj_ptr.p->m_ref_count = 0;

      obj_ptr.p->m_id = triggerPtr.p->triggerId;
      obj_ptr.p->m_type =TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
      link_object(obj_ptr, triggerPtr);
      c_obj_name_hash.add(obj_ptr);
      c_obj_id_hash.add(obj_ptr);
    }
  }
}

void Dbdict::handleTabInfo(SimpleProperties::Reader & it,
			   ParseDictTabInfoRecord * parseP,
			   DictTabInfo::Table &tableDesc)
{
  TableRecordPtr tablePtr = parseP->tablePtr;

  SimpleProperties::UnpackStatus status;

  Uint32 keyCount = 0;
  Uint32 keyLength = 0;
  Uint32 attrCount = tablePtr.p->noOfAttributes;
  Uint32 nullCount = 0;
  Uint32 nullBits = 0;
  Uint32 noOfCharsets = 0;
  Uint16 charsets[128];
  Uint32 recordLength = 0;
  AttributeRecordPtr attrPtr;
  c_attributeRecordHash.removeAll();

  LocalAttributeRecord_list list(c_attributeRecordPool,
					tablePtr.p->m_attributes);

  Uint32 counts[] = {0,0,0,0,0};

  for(Uint32 i = 0; i<attrCount; i++){
    /**
     * Attribute Name
     */
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    status = SimpleProperties::unpack(it, &attrDesc,
				      DictTabInfo::AttributeMapping,
				      DictTabInfo::AttributeMappingSize,
				      true, true);

    if(status != SimpleProperties::Break){
      parseP->errorCode = CreateTableRef::InvalidFormat;
      parseP->status    = status;
      parseP->errorKey  = it.getKey();
      parseP->errorLine = __LINE__;
      return;
    }

    /**
     * Check that attribute is not defined twice
     */
    const Uint32 len = Uint32(strlen(attrDesc.AttributeName)+1);
    const Uint32 name_hash = LocalRope::hash(attrDesc.AttributeName, len);
    {
      AttributeRecord key;
      key.m_key.m_name_ptr = attrDesc.AttributeName;
      key.m_key.m_name_len = len;
      key.attributeName.m_hash = name_hash;
      key.m_key.m_pool = &c_rope_pool;
      AttributeRecordPtr old_ptr;
      c_attributeRecordHash.find(old_ptr, key);

      if(old_ptr.i != RNIL){
	parseP->errorCode = CreateTableRef::AttributeNameTwice;
	return;
      }
    }

    list.seize(attrPtr);
    if(attrPtr.i == RNIL){
      jam();
      parseP->errorCode = CreateTableRef::NoMoreAttributeRecords;
      return;
    }

    new (attrPtr.p) AttributeRecord();
    attrPtr.p->attributeDescriptor = 0x00012255; //Default value
    attrPtr.p->tupleKey = 0;

    /**
     * TmpAttrib to Attribute mapping
     */
    {
      LocalRope name(c_rope_pool, attrPtr.p->attributeName);
      if (!name.assign(attrDesc.AttributeName, len, name_hash))
      {
	jam();
	parseP->errorCode = CreateTableRef::OutOfStringBuffer;
	parseP->errorLine = __LINE__;
	return;
      }
    }
    attrPtr.p->attributeId = i;
    //attrPtr.p->attributeId = attrDesc.AttributeId;
    attrPtr.p->tupleKey = (keyCount + 1) * attrDesc.AttributeKeyFlag;

    attrPtr.p->extPrecision = attrDesc.AttributeExtPrecision;
    attrPtr.p->extScale = attrDesc.AttributeExtScale;
    attrPtr.p->extLength = attrDesc.AttributeExtLength;
    // charset in upper half of precision
    unsigned csNumber = (attrPtr.p->extPrecision >> 16);
    if (csNumber != 0) {
      /*
       * A new charset is first accessed here on this node.
       * TODO use separate thread (e.g. via NDBFS) if need to load from file
       */
      CHARSET_INFO* cs = get_charset(csNumber, MYF(0));
      if (cs == NULL) {
        parseP->errorCode = CreateTableRef::InvalidCharset;
        parseP->errorLine = __LINE__;
        return;
      }
      // XXX should be done somewhere in mysql
      all_charsets[cs->number] = cs;
      unsigned i = 0;
      while (i < noOfCharsets) {
        if (charsets[i] == csNumber)
          break;
        i++;
      }
      if (i == noOfCharsets) {
        noOfCharsets++;
        if (noOfCharsets > sizeof(charsets)/sizeof(charsets[0])) {
          parseP->errorCode = CreateTableRef::InvalidFormat;
          parseP->errorLine = __LINE__;
          return;
        }
        charsets[i] = csNumber;
      }
    }

    // compute attribute size and array size
    bool translateOk = attrDesc.translateExtType();
    tabRequire(translateOk, CreateTableRef::Inconsistency);

    if(attrDesc.AttributeArraySize > 65535){
      parseP->errorCode = CreateTableRef::ArraySizeTooBig;
      parseP->status    = status;
      parseP->errorKey  = it.getKey();
      parseP->errorLine = __LINE__;
      return;
    }

    Uint32 desc = 0;
    AttributeDescriptor::setType(desc, attrDesc.AttributeExtType);
    AttributeDescriptor::setSize(desc, attrDesc.AttributeSize);
    AttributeDescriptor::setArraySize(desc, attrDesc.AttributeArraySize);
    AttributeDescriptor::setArrayType(desc, attrDesc.AttributeArrayType);
    AttributeDescriptor::setNullable(desc, attrDesc.AttributeNullableFlag);
    AttributeDescriptor::setDKey(desc, attrDesc.AttributeDKey);
    AttributeDescriptor::setPrimaryKey(desc, attrDesc.AttributeKeyFlag);
    AttributeDescriptor::setDiskBased(desc, attrDesc.AttributeStorageType == NDB_STORAGETYPE_DISK);
    AttributeDescriptor::setDynamic(desc, attrDesc.AttributeDynamic);
    attrPtr.p->attributeDescriptor = desc;
    attrPtr.p->autoIncrement = attrDesc.AttributeAutoIncrement;
    {
      char defaultValueBuf [MAX_ATTR_DEFAULT_VALUE_SIZE];

      if (attrDesc.AttributeDefaultValueLen)
      {
        ndbrequire(attrDesc.AttributeDefaultValueLen >= sizeof(Uint32));

        memcpy(defaultValueBuf, attrDesc.AttributeDefaultValue,
               attrDesc.AttributeDefaultValueLen);

        /* Table meta-info is normally stored in network byte order by
         * SimpleProperties.
         * For the default value, we convert as necessary here
         */
        /* Convert AttrInfoHeader */
        Uint32 a;
        memcpy(&a, defaultValueBuf, sizeof(Uint32));
        a = ntohl(a);
        memcpy(defaultValueBuf, &a, sizeof(Uint32));

        Uint32 remainBytes = attrDesc.AttributeDefaultValueLen - sizeof(Uint32);

        if (remainBytes)
        {
          /* Convert attribute */
          NdbSqlUtil::convertByteOrder(attrDesc.AttributeExtType,
                                       attrDesc.AttributeSize,
                                       attrDesc.AttributeArrayType,
                                       attrDesc.AttributeArraySize,
                                       (uchar*) defaultValueBuf + sizeof(Uint32),
                                       remainBytes);
        }
      }

      LocalRope defaultValue(c_rope_pool, attrPtr.p->defaultValue);
      defaultValue.assign(defaultValueBuf,
                          attrDesc.AttributeDefaultValueLen);
    }

    keyCount += attrDesc.AttributeKeyFlag;
    nullCount += attrDesc.AttributeNullableFlag;

    const Uint32 aSz = (1 << attrDesc.AttributeSize);
    Uint32 sz;
    if(aSz != 1)
    {
      sz = ((aSz * attrDesc.AttributeArraySize) + 31) >> 5;
    }
    else
    {
      sz = 0;
      nullBits += attrDesc.AttributeArraySize;
    }

    if(attrDesc.AttributeArraySize == 0)
    {
      parseP->errorCode = CreateTableRef::InvalidArraySize;
      parseP->status    = status;
      parseP->errorKey  = it.getKey();
      parseP->errorLine = __LINE__;
      return;
    }

    recordLength += sz;
    if(attrDesc.AttributeKeyFlag){
      keyLength += sz;

      if(attrDesc.AttributeNullableFlag){
	parseP->errorCode = CreateTableRef::NullablePrimaryKey;
	parseP->status    = status;
	parseP->errorKey  = it.getKey();
	parseP->errorLine = __LINE__;
	return;
      }
    }

    c_attributeRecordHash.add(attrPtr);

    int a= AttributeDescriptor::getDiskBased(desc);
    int b= AttributeDescriptor::getArrayType(desc);
    Uint32 pos= 2*(a ? 1 : 0) + (b == NDB_ARRAYTYPE_FIXED ? 0 : 1);
    counts[pos+1]++;

    if(b != NDB_ARRAYTYPE_FIXED && sz == 0)
    {
      parseP->errorCode = CreateTableRef::VarsizeBitfieldNotSupported;
      parseP->status    = status;
      parseP->errorKey  = it.getKey();
      parseP->errorLine = __LINE__;
      return;
    }

    if(!it.next())
      break;

    if(it.getKey() != DictTabInfo::AttributeName)
      break;
  }//while

  tablePtr.p->noOfPrimkey = keyCount;
  tablePtr.p->noOfNullAttr = nullCount;
  tablePtr.p->noOfCharsets = noOfCharsets;
  tablePtr.p->tupKeyLength = keyLength;
  tablePtr.p->noOfNullBits = nullCount + nullBits;

  tabRequire(recordLength<= MAX_TUPLE_SIZE_IN_WORDS,
	     CreateTableRef::RecordTooBig);
  tabRequire(keyLength <= MAX_KEY_SIZE_IN_WORDS,
	     CreateTableRef::InvalidPrimaryKeySize);
  tabRequire(keyLength > 0,
	     CreateTableRef::InvalidPrimaryKeySize);
  tabRequire(CHECK_SUMA_MESSAGE_SIZE(keyCount, keyLength, attrCount, recordLength),
             CreateTableRef::RecordTooBig);

  /* Check that all currently running nodes data support
   * table features
   */
  for (Uint32 nodeId=1; nodeId < MAX_NODES; nodeId++)
  {
    const NodeInfo& ni = getNodeInfo(nodeId);

    if (ni.m_connected &&
        (ni.m_type == NODE_TYPE_DB))
    {
      /* Check that all nodes support extra bits */
      if (tablePtr.p->m_extra_row_gci_bits ||
          tablePtr.p->m_extra_row_author_bits)
      {
        tabRequire(ndb_tup_extrabits(ni.m_version),
                   CreateTableRef::FeatureRequiresUpgrade);
      }
    }
  }


  if(tablePtr.p->m_tablespace_id != RNIL || counts[3] || counts[4])
  {
    FilegroupPtr tablespacePtr;
    if (!find_object(tablespacePtr, tablePtr.p->m_tablespace_id))
    {
      tabRequire(false, CreateTableRef::InvalidTablespace);
    }

    if(tablespacePtr.p->m_type != DictTabInfo::Tablespace)
    {
      tabRequire(false, CreateTableRef::NotATablespace);
    }

    if(tablespacePtr.p->m_version != tableDesc.TablespaceVersion)
    {
      tabRequire(false, CreateTableRef::InvalidTablespaceVersion);
    }
  }
}//handleTabInfo()

// MODULE: block/unblock substartstop
void
Dbdict::block_substartstop(Signal* signal, SchemaOpPtr op_ptr)
{
  ndbrequire(c_sub_startstop_lock.get(getOwnNodeId()) == false);
  c_sub_startstop_lock.set(getOwnNodeId());
  signal->theData[0] = ZWAIT_SUBSTARTSTOP;
  signal->theData[1] = op_ptr.p->op_key;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdict::unblock_substartstop()
{
  ndbrequire(c_sub_startstop_lock.get(getOwnNodeId()));
  c_sub_startstop_lock.clear(getOwnNodeId());
}

void
Dbdict::wait_substartstop(Signal* signal, Uint32 opkey)
{
  if (c_outstanding_sub_startstop == 0)
  {
    jam();
    Callback callback;
    bool ok = findCallback(callback, opkey);
    ndbrequire(ok);
    execute(signal, callback, 0);
    return;
  }

  signal->theData[0] = ZWAIT_SUBSTARTSTOP;
  signal->theData[1] = opkey;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100,
                      signal->length());
}

/* ---------------------------------------------------------------- */
// New global checkpoint created.
/* ---------------------------------------------------------------- */
void
Dbdict::wait_gcp(Signal* signal, SchemaOpPtr op_ptr, Uint32 flags)
{
  WaitGCPReq* req = (WaitGCPReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = flags;

  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
             WaitGCPReq::SignalLength, JBB);
}

void Dbdict::execWAIT_GCP_CONF(Signal* signal)
{
  WaitGCPConf* conf = (WaitGCPConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}//execWAIT_GCP_CONF()

/* ---------------------------------------------------------------- */
// Refused new global checkpoint.
/* ---------------------------------------------------------------- */
void Dbdict::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  WaitGCPRef* ref = (WaitGCPRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}//execWAIT_GCP_REF()

// MODULE: CreateTable

const Dbdict::OpInfo
Dbdict::CreateTableRec::g_opInfo = {
  { 'C', 'T', 'a', 0 },
  ~RT_DBDICT_CREATE_TABLE,
  GSN_CREATE_TAB_REQ,
  CreateTabReq::SignalLength,
  //
  &Dbdict::createTable_seize,
  &Dbdict::createTable_release,
  //
  &Dbdict::createTable_parse,
  &Dbdict::createTable_subOps,
  &Dbdict::createTable_reply,
  //
  &Dbdict::createTable_prepare,
  &Dbdict::createTable_commit,
  &Dbdict::createTable_complete,
  //
  &Dbdict::createTable_abortParse,
  &Dbdict::createTable_abortPrepare
};

bool
Dbdict::createTable_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateTableRec>(op_ptr);
}

void
Dbdict::createTable_release(SchemaOpPtr op_ptr)
{
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  if (createTabPtr.p->m_tabInfoPtrI != RNIL) {
    jam();
    SegmentedSectionPtr ss_ptr;
    getSection(ss_ptr, createTabPtr.p->m_tabInfoPtrI);
    SimulatedBlock::release(ss_ptr);
    createTabPtr.p->m_tabInfoPtrI = RNIL;
  }
  if (createTabPtr.p->m_fragmentsPtrI != RNIL) {
    jam();
    SegmentedSectionPtr ss_ptr;
    getSection(ss_ptr, createTabPtr.p->m_fragmentsPtrI);
    SimulatedBlock::release(ss_ptr);
    createTabPtr.p->m_fragmentsPtrI = RNIL;
  }
  releaseOpRec<CreateTableRec>(op_ptr);
}

void
Dbdict::execCREATE_TABLE_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  if (check_sender_version(signal, MAKE_VERSION(6,4,0)) < 0)
  {
    jam();
    /**
     * Pekka has for some obscure reason switched places of
     *   senderRef/senderData
     */
    CreateTableReq* tmp = (CreateTableReq*)signal->getDataPtr();
    do_swap(tmp->senderRef, tmp->senderData);
  }

  const CreateTableReq req_copy =
    *(const CreateTableReq*)signal->getDataPtr();
  const CreateTableReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateTableRecPtr createTabPtr;
    CreateTabReq* impl_req;

    startClientReq(op_ptr, createTabPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = RNIL;
    impl_req->tableVersion = 0;
    impl_req->gci = 0;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateTableRef* ref = (CreateTableRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->clientData = req->clientData;
  getError(error, ref);
  ref->errorStatus = error.errorStatus;
  ref->errorKey = error.errorKey;

  sendSignal(req->clientRef, GSN_CREATE_TABLE_REF, signal,
	     CreateTableRef::SignalLength, JBB);
}

// CreateTable: PARSE

Uint32
Dbdict::get_fragmentation(Signal* signal, Uint32 tableId)
{
  CreateFragmentationReq * req =
    (CreateFragmentationReq*)signal->getDataPtrSend();
  req->senderRef = 0;
  req->senderData = RNIL;
  req->fragmentationType = 0;
  req->noOfFragments = 0;
  req->primaryTableId = tableId;
  req->requestInfo = CreateFragmentationReq::RI_GET_FRAGMENTATION;
  EXECUTE_DIRECT(DBDICT, GSN_CREATE_FRAGMENTATION_REQ, signal,
                 CreateFragmentationReq::SignalLength);

  return signal->theData[0];
}

Uint32
Dbdict::create_fragmentation(Signal* signal,
                             TableRecordPtr tabPtr,
                             const Uint16 *src,
                             Uint32 cnt,
                             Uint32 flags)
{
  CreateFragmentationReq* frag_req =
    (CreateFragmentationReq*)signal->getDataPtrSend();
  frag_req->senderRef = 0; // direct conf
  frag_req->senderData = RNIL;
  frag_req->primaryTableId = tabPtr.p->primaryTableId;
  frag_req->noOfFragments = tabPtr.p->fragmentCount;
  frag_req->fragmentationType = tabPtr.p->fragmentType;
  frag_req->requestInfo = flags;

  if (tabPtr.p->hashMapObjectId != RNIL)
  {
    jam();
    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, tabPtr.p->hashMapObjectId));
    frag_req->map_ptr_i = hm_ptr.p->m_map_ptr_i;
  }
  else
  {
    jam();
    frag_req->map_ptr_i = RNIL;
  }

  memcpy(signal->theData+25, src, 2*cnt);

  if (cnt != 0 && cnt != tabPtr.p->fragmentCount)
  {
    /**
     * Either you dont specify fragmentation, or
     *   you specify it for each fragment
     */
    return CreateTableRef::InvalidFormat;
  }

  if (tabPtr.p->isOrderedIndex()) {
    jam();
    // ordered index has same fragmentation as the table
    frag_req->primaryTableId = tabPtr.p->primaryTableId;
    frag_req->fragmentationType = DictTabInfo::DistrKeyOrderedIndex;
  }
  else if (tabPtr.p->isHashIndex())
  {
    jam();
    /*
     * Unique hash indexes has same amount of fragments as primary table
     * and distributed in the same manner but has always a normal hash
     * fragmentation.
     */
    frag_req->primaryTableId = RNIL;
  }
  else
  {
    jam();
    /*
     * Blob tables come here with primaryTableId != RNIL but we only need
     * it for creating the fragments so we set it to RNIL now that we got
     * what we wanted from it to avoid other side effects.
     */
    tabPtr.p->primaryTableId = RNIL;
  }

  EXECUTE_DIRECT(DBDICT, GSN_CREATE_FRAGMENTATION_REQ, signal,
                 CreateFragmentationReq::SignalLength);
  jamEntry();

  return signal->theData[0];
}

void
Dbdict::createTable_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  D("createTable_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  CreateTabReq* impl_req = &createTabPtr.p->m_request;

  /*
   * Master parses client DictTabInfo (sec 0) into new table record.
   * DIH is called to create fragmentation.  The table record is
   * then dumped back into DictTabInfo to produce a canonical format.
   * The new DictTabInfo (sec 0) and the fragmentation (sec 1) are
   * sent to all participants when master parse returns.
   */
  if (master)
  {
    jam();

    // parse client DictTabInfo into new TableRecord

    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::CreateTableFromAPI;
    parseRecord.errorCode = 0;

    SegmentedSectionPtr ss_ptr;
    if (!handle.getSection(ss_ptr, CreateTableReq::DICT_TAB_INFO)) {
      jam();
      setError(error, CreateTableRef::InvalidFormat, __LINE__);
      return;
    }
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());

    handleTabInfoInit(signal, trans_ptr, r, &parseRecord);
    releaseSections(handle);

    if (parseRecord.errorCode == 0)
    {
      if (ERROR_INSERTED(6200) ||
          (ERROR_INSERTED(6201) &&
           DictTabInfo::isIndex(parseRecord.tablePtr.p->tableType)))
      {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        parseRecord.errorCode = 1;
      }
    }

    if (parseRecord.errorCode != 0)
    {
      jam();
      if (!parseRecord.tablePtr.isNull())
      {
        jam();
        releaseTableObject(parseRecord.tablePtr.i, true);
      }
      setError(error, parseRecord);
      BaseString::snprintf(error.errorObjectName, sizeof(error.errorObjectName),
                           "%s", c_tableDesc.TableName);
      return;
    }

    TableRecordPtr tabPtr = parseRecord.tablePtr;

    // link operation to object seized in handleTabInfoInit
    DictObjectPtr obj_ptr;
    {
      Uint32 obj_ptr_i = tabPtr.p->m_obj_ptr_i;
      bool ok = findDictObject(op_ptr, obj_ptr, obj_ptr_i);
      ndbrequire(ok);
    }

    {
      Uint32 version = getTableEntry(obj_ptr.p->m_id)->m_tableVersion;
      tabPtr.p->tableVersion = create_obj_inc_schema_version(version);
    }

    // fill in table id and version
    impl_req->tableId = obj_ptr.p->m_id;
    impl_req->tableVersion = tabPtr.p->tableVersion;

    if (ERROR_INSERTED(6202) ||
        (ERROR_INSERTED(6203) &&
         DictTabInfo::isIndex(parseRecord.tablePtr.p->tableType)))
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      setError(error, 1, __LINE__);
      return;
    }

    // create fragmentation via DIH (no changes in DIH)
    Uint16* frag_data = (Uint16*)(signal->getDataPtr()+25);
    Uint32 err = create_fragmentation(signal, tabPtr,
                                      c_fragData, c_fragDataLen / 2);
    if (err)
    {
      jam();
      setError(error, err, __LINE__);
      return;
    }

    // save fragmentation in long signal memory
    {
      // wl3600_todo make a method for this magic stuff
      Uint32 count = 2 + (1 + frag_data[0]) * frag_data[1];

      Ptr<SectionSegment> frag_ptr;
      bool ok = import(frag_ptr, (Uint32*)frag_data, (count + 1) / 2);
      ndbrequire(ok);
      createTabPtr.p->m_fragmentsPtrI = frag_ptr.i;

      // save fragment count
      tabPtr.p->fragmentCount = frag_data[1];
    }

    // dump table record back into DictTabInfo
    {
      SimplePropertiesSectionWriter w(* this);
      packTableIntoPages(w, tabPtr);

      SegmentedSectionPtr ss_ptr;
      w.getPtr(ss_ptr);
      createTabPtr.p->m_tabInfoPtrI = ss_ptr.i;
    }

    // assign signal sections to send to participants
    {
      SegmentedSectionPtr ss0_ptr;
      SegmentedSectionPtr ss1_ptr;

      getSection(ss0_ptr, createTabPtr.p->m_tabInfoPtrI);
      getSection(ss1_ptr, createTabPtr.p->m_fragmentsPtrI);

      ndbrequire(handle.m_cnt == 0);
      handle.m_ptr[CreateTabReq::DICT_TAB_INFO] = ss0_ptr;
      handle.m_ptr[CreateTabReq::FRAGMENTATION] = ss1_ptr;
      handle.m_cnt = 2;

      // handle owns the memory now
      createTabPtr.p->m_tabInfoPtrI = RNIL;
      createTabPtr.p->m_fragmentsPtrI = RNIL;
    }
  }
  else if (op_ptr.p->m_restart)
  {
    jam();
    impl_req->tableId = c_restartRecord.activeTable;
    impl_req->tableVersion = c_restartRecord.m_entry.m_tableVersion;
    impl_req->gci = c_restartRecord.m_entry.m_gcp;
  }

  const Uint32 gci = impl_req->gci;
  const Uint32 tableId = impl_req->tableId;
  const Uint32 tableVersion = impl_req->tableVersion;

  SegmentedSectionPtr tabInfoPtr;
  {
    bool ok = handle.getSection(tabInfoPtr, CreateTabReq::DICT_TAB_INFO);
    ndbrequire(ok);
  }

  // wl3600_todo parse the rewritten DictTabInfo in master too
  if (!master)
  {
    jam();
    createTabPtr.p->m_request.tableId = tableId;

    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::AddTableFromDict;
    parseRecord.errorCode = 0;

    SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());

    bool checkExist = true;
    handleTabInfoInit(signal, trans_ptr, r, &parseRecord, checkExist);

    if (parseRecord.errorCode != 0)
    {
      jam();
      setError(error, parseRecord);
      BaseString::snprintf(error.errorObjectName, sizeof(error.errorObjectName),
                           "%s", c_tableDesc.TableName);
      return;
    }

    TableRecordPtr tabPtr = parseRecord.tablePtr;

    // link operation to object seized in handleTabInfoInit
    {
      DictObjectPtr obj_ptr;
      Uint32 obj_ptr_i = tabPtr.p->m_obj_ptr_i;
      bool ok = findDictObject(op_ptr, obj_ptr, obj_ptr_i);
      ndbrequire(ok);
    }
  }

  {
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, CreateTabReq::DICT_TAB_INFO);
    if (ptr.sz > MAX_WORDS_META_FILE)
    {
      jam();
      setError(error, CreateTableRef::TableDefinitionTooBig, __LINE__);
      return;
    }
  }

  // save sections to DICT memory
  saveOpSection(op_ptr, handle, CreateTabReq::DICT_TAB_INFO);
  if (op_ptr.p->m_restart == 0)
  {
    jam();
    saveOpSection(op_ptr, handle, CreateTabReq::FRAGMENTATION);
  }

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, tableId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }
  tabPtr.p->packedSize = tabInfoPtr.sz;
  // wl3600_todo verify version on slave
  tabPtr.p->tableVersion = tableVersion;
  tabPtr.p->gciTableCreated = gci;

  if (ERROR_INSERTED(6121)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9121, __LINE__);
    return;
  }

  SchemaFile::TableEntry te; te.init();
  te.m_tableState = SchemaFile::SF_CREATE;
  te.m_tableVersion = tableVersion;
  te.m_tableType = tabPtr.p->tableType;
  te.m_info_words = tabInfoPtr.sz;
  te.m_gcp = gci;
  te.m_transId = trans_ptr.p->m_transId;

  Uint32 err = trans_log_schema_op(op_ptr, tableId, &te);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }
  D("createTable_parse: "
    << copyRope<MAX_TAB_NAME_SIZE>(tabPtr.p->tableName)
    << V(tabPtr.p->tableVersion));
}

bool
Dbdict::createTable_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTable_subOps" << V(op_ptr.i) << *op_ptr.p);
  // wl3600_todo blobs
  return false;
}

void
Dbdict::createTable_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("createTable_reply" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);

  if (!hasError(error)) {
    CreateTableConf* conf = (CreateTableConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = createTabPtr.p->m_request.tableId;
    conf->tableVersion = createTabPtr.p->m_request.tableVersion;

    D(V(conf->tableId) << V(conf->tableVersion));

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_TABLE_CONF, signal,
               CreateTableConf::SignalLength, JBB);
  } else {
    jam();
    CreateTableRef* ref = (CreateTableRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);
    ref->errorStatus = error.errorStatus;
    ref->errorKey = error.errorKey;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_TABLE_REF, signal,
               CreateTableRef::SignalLength, JBB);
  }
}

// CreateTable: PREPARE

void
Dbdict::createTable_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);

  Uint32 tableId = createTabPtr.p->m_request.tableId;
  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, tableId);

  Callback cb;
  cb.m_callbackData = op_ptr.p->op_key;
  cb.m_callbackFunction = safe_cast(&Dbdict::createTab_writeTableConf);

  if (ZRESTART_NO_WRITE_AFTER_READ && op_ptr.p->m_restart == 1)
  {
    jam();
    /**
     * We read obj from disk, no need to rewrite it
     */
    execute(signal, cb, 0);
    return;
  }

  ndbrequire(ok);
  bool savetodisk = !(tabPtr.p->m_bits & TableRecord::TR_Temporary);
  if (savetodisk)
  {
    jam();
    const OpSection& tabInfoSec =
      getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);
    writeTableFile(signal, op_ptr, createTabPtr.p->m_request.tableId,
                   tabInfoSec, &cb);
  }
  else
  {
    jam();
    execute(signal, cb, 0);
  }
}

void
Dbdict::createTab_writeTableConf(Signal* signal,
				 Uint32 op_key,
				 Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  OpSection& fragSec = op_ptr.p->m_section[CreateTabReq::FRAGMENTATION];
  if (op_ptr.p->m_restart)
  {
    jam();
    new (&fragSec) OpSection();
  }
  else
  {
    const OpSection& fragSec =
      getOpSection(op_ptr, CreateTabReq::FRAGMENTATION);
    ndbrequire(fragSec.getSize() > 0);
  }

  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction =
    safe_cast(&Dbdict::createTab_localComplete);

  createTab_local(signal, op_ptr, fragSec, &callback);
}

void
Dbdict::createTab_local(Signal* signal,
                        SchemaOpPtr op_ptr,
                        OpSection afragSec,
                        Callback * c)
{
  jam();
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  createTabPtr.p->m_callback = * c;

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
  ndbrequire(ok);

  /**
   * Start by createing table in LQH
   */
  CreateTabReq* req = (CreateTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->tableId = createTabPtr.p->m_request.tableId;
  req->tableVersion = tabPtr.p->tableVersion;
  req->requestType = 0;
  req->gci = 0;
  req->noOfCharsets = tabPtr.p->noOfCharsets;
  req->tableType = tabPtr.p->tableType;
  req->primaryTableId = tabPtr.p->primaryTableId;
  req->forceVarPartFlag = !!(tabPtr.p->m_bits& TableRecord::TR_ForceVarPart);
  req->noOfNullAttributes = tabPtr.p->noOfNullBits;
  req->noOfKeyAttr = tabPtr.p->noOfPrimkey;
  req->checksumIndicator = 1;
  req->GCPIndicator = 1 + tabPtr.p->m_extra_row_gci_bits;
  req->noOfAttributes = tabPtr.p->noOfAttributes;
  req->extraRowAuthorBits = tabPtr.p->m_extra_row_author_bits;
  sendSignal(DBLQH_REF, GSN_CREATE_TAB_REQ, signal,
             CreateTabReq::SignalLengthLDM, JBB);


  /**
   * Create KeyDescriptor
   */
  {
    KeyDescriptor* desc= g_key_descriptor_pool.getPtr(createTabPtr.p->m_request.tableId);
    new (desc) KeyDescriptor();

    if (tabPtr.p->primaryTableId == RNIL)
    {
      jam();
      desc->primaryTableId = createTabPtr.p->m_request.tableId;
    }
    else
    {
      jam();
      desc->primaryTableId = tabPtr.p->primaryTableId;
    }

    Uint32 key = 0;
    AttributeRecordPtr attrPtr;
    LocalAttributeRecord_list list(c_attributeRecordPool,
                                          tabPtr.p->m_attributes);
    for(list.first(attrPtr); !attrPtr.isNull(); list.next(attrPtr))
    {
      AttributeRecord* aRec = attrPtr.p;
      if (aRec->tupleKey)
      {
        Uint32 attr = aRec->attributeDescriptor;

        desc->noOfKeyAttr ++;
        desc->keyAttr[key].attributeDescriptor = attr;
        Uint32 csNumber = (aRec->extPrecision >> 16);
        if (csNumber)
        {
          desc->keyAttr[key].charsetInfo = all_charsets[csNumber];
          ndbrequire(all_charsets[csNumber] != 0);
          desc->hasCharAttr = 1;
        }
        else
        {
          desc->keyAttr[key].charsetInfo = 0;
        }
        if (AttributeDescriptor::getDKey(attr))
        {
          desc->noOfDistrKeys ++;
        }
        if (AttributeDescriptor::getArrayType(attr) != NDB_ARRAYTYPE_FIXED)
        {
          desc->noOfVarKeys ++;
        }
        key++;
      }
    }
    ndbrequire(key == tabPtr.p->noOfPrimkey);
  }
}

void
Dbdict::execCREATE_TAB_REF(Signal* signal)
{
  jamEntry();

  CreateTabRef * const ref = (CreateTabRef*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, ref->senderData);
  ndbrequire(!op_ptr.isNull());

  setError(op_ptr, ref->errorCode, __LINE__);
  execute(signal, createTabPtr.p->m_callback, 0);
}

void
Dbdict::execCREATE_TAB_CONF(Signal* signal)
{
  jamEntry();

  CreateTabConf* conf = (CreateTabConf*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, conf->senderData);
  ndbrequire(!op_ptr.isNull());

  createTabPtr.p->m_lqhFragPtr = conf->lqhConnectPtr;

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
  ndbrequire(ok);
  sendLQHADDATTRREQ(signal, op_ptr, tabPtr.p->m_attributes.firstItem);
}


void
Dbdict::sendLQHADDATTRREQ(Signal* signal,
			  SchemaOpPtr op_ptr,
			  Uint32 attributePtrI)
{
  jam();
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
  ndbrequire(ok);

  const bool isHashIndex = tabPtr.p->isHashIndex();

  LqhAddAttrReq * const req = (LqhAddAttrReq*)signal->getDataPtrSend();
  Uint32 i = 0;
  Uint32 startIndex = 25;
  Uint32 *defVal_dst = &signal->theData[startIndex];
  Uint32 defVal_length = 0;
  for(i = 0; i<LqhAddAttrReq::MAX_ATTRIBUTES && attributePtrI != RNIL; i++){
    jam();
    AttributeRecordPtr attrPtr;
    c_attributeRecordPool.getPtr(attrPtr, attributePtrI);
    LqhAddAttrReq::Entry& entry = req->attributes[i];
    entry.attrId = attrPtr.p->attributeId;
    entry.attrDescriptor = attrPtr.p->attributeDescriptor;
    entry.extTypeInfo = 0;
    // charset number passed to TUP, TUX in upper half
    entry.extTypeInfo |= (attrPtr.p->extPrecision & ~0xFFFF);

    ConstRope def(c_rope_pool, attrPtr.p->defaultValue);
    def.copy((char*)defVal_dst);

    Uint32 defValueLen = def.size();
    defVal_length += (defValueLen + 3)/4;
    defVal_dst += (defValueLen + 3)/4;

    if (tabPtr.p->isIndex()) {
      Uint32 primaryAttrId;
      if (attrPtr.p->nextList != RNIL) {
        getIndexAttr(tabPtr, attributePtrI, &primaryAttrId);
      } else {
        primaryAttrId = ZNIL;
        if (tabPtr.p->isOrderedIndex())
          entry.attrId = 0;     // attribute goes to TUP
      }
      entry.attrId |= (primaryAttrId << 16);
    }

    if (attrPtr.p->nextList == RNIL && isHashIndex)
    {
      jam();
      /**
       * Nasty code to handle upgrade of unique index(es)
       *   If unique index is "old" rewrite it to new format
       */
      char tmp[MAX_TAB_NAME_SIZE];
      ConstRope name(c_rope_pool, attrPtr.p->attributeName);
      name.copy(tmp);
      ndbrequire(memcmp(tmp, "NDB$PK", sizeof("NDB$PK")) == 0);
      Uint32 at = AttributeDescriptor::getArrayType(entry.attrDescriptor);
      if (at == NDB_ARRAYTYPE_MEDIUM_VAR)
      {
        jam();
        AttributeDescriptor::clearArrayType(entry.attrDescriptor);
        AttributeDescriptor::setArrayType(entry.attrDescriptor,
                                          NDB_ARRAYTYPE_NONE_VAR);
      }
    }

    attributePtrI = attrPtr.p->nextList;
  }
  req->lqhFragPtr = createTabPtr.p->m_lqhFragPtr;
  req->senderData = op_ptr.p->op_key;
  req->senderAttrPtr = attributePtrI;
  req->noOfAttributes = i;

  if (defVal_length != 0)
  {
    LinearSectionPtr ptr[3];
    ptr[0].p= &signal->theData[startIndex];
    ptr[0].sz= defVal_length;

    sendSignal(DBLQH_REF, GSN_LQHADDATTREQ, signal,
       	       LqhAddAttrReq::HeaderLength + LqhAddAttrReq::EntryLength * i, JBB, ptr, 1);
  }
  else
    sendSignal(DBLQH_REF, GSN_LQHADDATTREQ, signal,
               LqhAddAttrReq::HeaderLength + LqhAddAttrReq::EntryLength * i, JBB);
}

void
Dbdict::execLQHADDATTCONF(Signal * signal)
{
  jamEntry();
  LqhAddAttrConf * const conf = (LqhAddAttrConf*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, conf->senderData);
  ndbrequire(!op_ptr.isNull());

  const Uint32 nextAttrPtr = conf->senderAttrPtr;
  if(nextAttrPtr != RNIL){
    jam();
    sendLQHADDATTRREQ(signal, op_ptr, nextAttrPtr);
    return;
  }

  createTab_dih(signal, op_ptr);
}

void
Dbdict::execLQHADDATTREF(Signal * signal)
{
  jamEntry();
  LqhAddAttrRef * const ref = (LqhAddAttrRef*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, ref->senderData);
  ndbrequire(!op_ptr.isNull());

  setError(op_ptr, ref->errorCode, __LINE__);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createTab_dih(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  D("createTab_dih" << *op_ptr.p);

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
  ndbrequire(ok);

  /**
   * NOTE: use array access here...
   *   as during SR m_noOfSections == 0
   *   i.e getOpSection will crash
   */
  const OpSection& fragSec = op_ptr.p->m_section[CreateTabReq::FRAGMENTATION];

  DiAddTabReq * req = (DiAddTabReq*)signal->getDataPtrSend();
  req->connectPtr = op_ptr.p->op_key;
  req->tableId = createTabPtr.p->m_request.tableId;
  req->fragType = tabPtr.p->fragmentType;
  req->kValue = tabPtr.p->kValue;
  req->noOfReplicas = 0;
  req->loggedTable = !!(tabPtr.p->m_bits & TableRecord::TR_Logged);
  req->tableType = tabPtr.p->tableType;
  req->schemaVersion = tabPtr.p->tableVersion;
  req->primaryTableId = tabPtr.p->primaryTableId;
  req->temporaryTable = !!(tabPtr.p->m_bits & TableRecord::TR_Temporary);
  // no transaction for restart tab (should add one)
  req->schemaTransId = !trans_ptr.isNull() ? trans_ptr.p->m_transId : 0;

  if (tabPtr.p->hashMapObjectId != RNIL)
  {
    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, tabPtr.p->hashMapObjectId));
    req->hashMapPtrI = hm_ptr.p->m_map_ptr_i;
  }
  else
  {
    req->hashMapPtrI = RNIL;
  }

  // fragmentation in long signal section
  {
    Uint32 page[MAX_FRAGMENT_DATA_WORDS];
    LinearSectionPtr ptr[3];
    Uint32 noOfSections = 0;

    const Uint32 size = fragSec.getSize();
    ndbrequire(size <= NDB_ARRAY_SIZE(page));

    // wl3600_todo add ndbrequire on SR, NR
    if (size != 0) {
      jam();
      LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena,c_opSectionBufferPool);
      bool ok = copyOut(op_sec_pool, fragSec, page, size);
      ndbrequire(ok);
      ptr[noOfSections].sz = size;
      ptr[noOfSections].p = page;
      noOfSections++;
    }

    sendSignal(DBDIH_REF, GSN_DIADDTABREQ, signal,
               DiAddTabReq::SignalLength, JBB,
               ptr, noOfSections);
  }
}

static
void
calcLHbits(Uint32 * lhPageBits, Uint32 * lhDistrBits,
	   Uint32 fid, Uint32 totalFragments, Dbdict * dict)
{
  Uint32 distrBits = 0;
  Uint32 pageBits = 0;

  Uint32 tmp = 1;
  while (tmp < totalFragments) {
    jamBlock(dict);
    tmp <<= 1;
    distrBits++;
  }//while
#ifdef ndb_classical_lhdistrbits
  if (tmp != totalFragments) {
    tmp >>= 1;
    if ((fid >= (totalFragments - tmp)) && (fid < (tmp - 1))) {
      distrBits--;
    }//if
  }//if
#endif
  * lhPageBits = pageBits;
  * lhDistrBits = distrBits;

}//calcLHbits()

void
Dbdict::execADD_FRAGREQ(Signal* signal)
{
  jamEntry();

  AddFragReq * const req = (AddFragReq*)signal->getDataPtr();

  Uint32 dihPtr = req->dihPtr;
  Uint32 senderData = req->senderData;
  Uint32 tableId = req->tableId;
  Uint32 fragId = req->fragmentId;
  Uint32 node = req->nodeId;
  Uint32 lcpNo = req->nextLCP;
  Uint32 fragCount = req->totalFragments;
  Uint32 requestInfo = req->requestInfo;
  Uint32 startGci = req->startGci;
  Uint32 logPart = req->logPartId;
  Uint32 changeMask = req->changeMask;

  ndbrequire(node == getOwnNodeId());

  SchemaOpPtr op_ptr;
  TableRecordPtr tabPtr;
  if (AlterTableReq::getAddFragFlag(changeMask))
  {
    bool ok = find_object(tabPtr, tableId);
    ndbrequire(ok);
    if (DictTabInfo::isTable(tabPtr.p->tableType))
    {
      jam();
      AlterTableRecPtr alterTabPtr;
      findSchemaOp(op_ptr, alterTabPtr, senderData);
      ndbrequire(!op_ptr.isNull());
      alterTabPtr.p->m_dihAddFragPtr = dihPtr;
      tabPtr.i = alterTabPtr.p->m_newTablePtrI;
      c_tableRecordPool_.getPtr(tabPtr);
    }
    else
    {
      jam();
      ndbrequire(DictTabInfo::isOrderedIndex(tabPtr.p->tableType));
      AlterIndexRecPtr alterIndexPtr;
      findSchemaOp(op_ptr, alterIndexPtr, senderData);
      ndbrequire(!op_ptr.isNull());
      alterIndexPtr.p->m_dihAddFragPtr = dihPtr;
    }
  }
  else
  {
    jam();
    CreateTableRecPtr createTabPtr;
    findSchemaOp(op_ptr, createTabPtr, senderData);
    ndbrequire(!op_ptr.isNull());
    createTabPtr.p->m_dihAddFragPtr = dihPtr;
    bool ok = find_object(tabPtr, tableId);
    ndbrequire(ok);
  }

#if 0
  tabPtr.p->gciTableCreated = (startGci > tabPtr.p->gciTableCreated ? startGci:
			       startGci > tabPtr.p->gciTableCreated);
#endif

  /**
   * Calc lh3PageBits
   */
  Uint32 lhDistrBits = 0;
  Uint32 lhPageBits = 0;
  ::calcLHbits(&lhPageBits, &lhDistrBits, fragId, fragCount, this);

  Uint64 maxRows = tabPtr.p->maxRowsLow +
    (((Uint64)tabPtr.p->maxRowsHigh) << 32);
  Uint64 minRows = tabPtr.p->minRowsLow +
    (((Uint64)tabPtr.p->minRowsHigh) << 32);
  maxRows = (maxRows + fragCount - 1) / fragCount;
  minRows = (minRows + fragCount - 1) / fragCount;

  {
    LqhFragReq* req = (LqhFragReq*)signal->getDataPtrSend();
    req->senderData = senderData;
    req->senderRef = reference();
    req->fragmentId = fragId;
    req->requestInfo = requestInfo;
    req->tableId = tableId;
    req->localKeyLength = tabPtr.p->localKeyLen;
    req->maxLoadFactor = tabPtr.p->maxLoadFactor;
    req->minLoadFactor = tabPtr.p->minLoadFactor;
    req->kValue = tabPtr.p->kValue;
    req->lh3DistrBits = 0; //lhDistrBits;
    req->lh3PageBits = 0; //lhPageBits;
    req->maxRowsLow = (Uint32)(maxRows & 0xFFFFFFFF);
    req->maxRowsHigh = (Uint32)(maxRows >> 32);
    req->minRowsLow = (Uint32)(minRows & 0xFFFFFFFF);
    req->minRowsHigh = (Uint32)(minRows >> 32);
    Uint32 keyLen = tabPtr.p->tupKeyLength;
    req->keyLength = keyLen; // wl-2066 no more "long keys"
    req->nextLCP = lcpNo;

    req->startGci = startGci;
    req->tablespace_id= tabPtr.p->m_tablespace_id;
    req->tablespace_id = tabPtr.p->m_tablespace_id;
    req->logPartId = logPart;
    req->changeMask = changeMask;
    sendSignal(DBLQH_REF, GSN_LQHFRAGREQ, signal,
	       LqhFragReq::SignalLength, JBB);
  }
}

void
Dbdict::execLQHFRAGCONF(Signal * signal)
{
  jamEntry();
  LqhFragConf * const conf = (LqhFragConf*)signal->getDataPtr();

  Uint32 dihPtr;
  Uint32 fragId = conf->fragId;
  Uint32 tableId = conf->tableId;

  if (AlterTableReq::getAddFragFlag(conf->changeMask))
  {
    jam();
    SchemaOpPtr op_ptr;
    TableRecordPtr tabPtr;
    bool ok = find_object(tabPtr, tableId);
    ndbrequire(ok);
    if (DictTabInfo::isTable(tabPtr.p->tableType))
    {
      AlterTableRecPtr alterTabPtr;
      findSchemaOp(op_ptr, alterTabPtr, conf->senderData);
      ndbrequire(!op_ptr.isNull());
      dihPtr = alterTabPtr.p->m_dihAddFragPtr;
    }
    else
    {
      jam();
      ndbrequire(DictTabInfo::isOrderedIndex(tabPtr.p->tableType));
      AlterIndexRecPtr alterIndexPtr;
      findSchemaOp(op_ptr, alterIndexPtr, conf->senderData);
      ndbrequire(!op_ptr.isNull());
      dihPtr = alterIndexPtr.p->m_dihAddFragPtr;
    }
  }
  else
  {
    jam();
    SchemaOpPtr op_ptr;
    CreateTableRecPtr createTabPtr;
    findSchemaOp(op_ptr, createTabPtr, conf->senderData);
    ndbrequire(!op_ptr.isNull());

    createTabPtr.p->m_lqhFragPtr = conf->lqhFragPtr;
    dihPtr = createTabPtr.p->m_dihAddFragPtr;
  }

  {
    AddFragConf * const conf = (AddFragConf*)signal->getDataPtr();
    conf->dihPtr = dihPtr;
    conf->fragId = fragId;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGCONF, signal,
	       AddFragConf::SignalLength, JBB);
  }
}

void
Dbdict::execLQHFRAGREF(Signal * signal)
{
  jamEntry();
  LqhFragRef * const ref = (LqhFragRef*)signal->getDataPtr();

  Uint32 dihPtr;
  Uint32 tableId = ref->tableId;
  Uint32 fragId = ref->fragId;
  if (AlterTableReq::getAddFragFlag(ref->changeMask))
  {
    jam();
    SchemaOpPtr op_ptr;
    TableRecordPtr tabPtr;
    bool ok = find_object(tabPtr, tableId);
    ndbrequire(ok);
    if (DictTabInfo::isTable(tabPtr.p->tableType))
    {
      jam();
      AlterTableRecPtr alterTabPtr;
      findSchemaOp(op_ptr, alterTabPtr, ref->senderData);
      ndbrequire(!op_ptr.isNull());
      setError(op_ptr, ref->errorCode, __LINE__);
      dihPtr = alterTabPtr.p->m_dihAddFragPtr;
    }
    else
    {
      jam();
      AlterIndexRecPtr alterIndexPtr;
      findSchemaOp(op_ptr, alterIndexPtr, ref->senderData);
      ndbrequire(!op_ptr.isNull());
      setError(op_ptr, ref->errorCode, __LINE__);
      dihPtr = alterIndexPtr.p->m_dihAddFragPtr;
    }
  }
  else
  {
    jam();
    SchemaOpPtr op_ptr;
    CreateTableRecPtr createTabPtr;
    findSchemaOp(op_ptr, createTabPtr, ref->senderData);
    ndbrequire(!op_ptr.isNull());
    setError(op_ptr, ref->errorCode, __LINE__);
    dihPtr = createTabPtr.p->m_dihAddFragPtr;
  }

  {
    AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();
    ref->dihPtr = dihPtr;
    ref->fragId = fragId;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGREF, signal,
	       AddFragRef::SignalLength, JBB);
  }
}

void
Dbdict::execDIADDTABCONF(Signal* signal)
{
  jam();

  DiAddTabConf * const conf = (DiAddTabConf*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, conf->senderData);
  ndbrequire(!op_ptr.isNull());

  signal->theData[0] = op_ptr.p->op_key;
  signal->theData[1] = reference();
  signal->theData[2] = createTabPtr.p->m_request.tableId;

  sendSignal(DBLQH_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
}

void
Dbdict::execDIADDTABREF(Signal* signal)
{
  jam();

  DiAddTabRef * const ref = (DiAddTabRef*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, ref->senderData);
  ndbrequire(!op_ptr.isNull());

  setError(op_ptr, ref->errorCode, __LINE__);
  execute(signal, createTabPtr.p->m_callback, 0);
}

// wl3600_todo split into 2 methods for prepare/commit
void
Dbdict::execTAB_COMMITCONF(Signal* signal)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, signal->theData[0]);
  ndbrequire(!op_ptr.isNull());
  //const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
  ndbrequire(ok);

  if (refToBlock(signal->getSendersBlockRef()) == DBLQH)
  {
    jam();
    // prepare table in DBSPJ
    TcSchVerReq * req = (TcSchVerReq*)signal->getDataPtr();
    req->tableId = createTabPtr.p->m_request.tableId;
    req->tableVersion = tabPtr.p->tableVersion;
    req->tableLogged = (Uint32)!!(tabPtr.p->m_bits & TableRecord::TR_Logged);
    req->senderRef = reference();
    req->tableType = (Uint32)tabPtr.p->tableType;
    req->senderData = op_ptr.p->op_key;
    req->noOfPrimaryKeys = (Uint32)tabPtr.p->noOfPrimkey;
    req->singleUserMode = (Uint32)tabPtr.p->singleUserMode;
    req->userDefinedPartition = (tabPtr.p->fragmentType == DictTabInfo::UserDefined);

    if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
    {
      jam();
      TableRecordPtr basePtr;
      bool ok = find_object(basePtr, tabPtr.p->primaryTableId);
      ndbrequire(ok);
      req->userDefinedPartition = (basePtr.p->fragmentType == DictTabInfo::UserDefined);
    }

    sendSignal(DBSPJ_REF, GSN_TC_SCHVERREQ, signal,
               TcSchVerReq::SignalLength, JBB);
    return;
  }

  if (refToBlock(signal->getSendersBlockRef()) == DBDIH)
  {
    jam();
    // commit table in DBSPJ
    signal->theData[0] = op_ptr.p->op_key;
    signal->theData[1] = reference();
    signal->theData[2] = createTabPtr.p->m_request.tableId;
    sendSignal(DBSPJ_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
    return;
  }

  if (refToBlock(signal->getSendersBlockRef()) == DBSPJ)
  {
    jam();
    // commit table in DBTC
    signal->theData[0] = op_ptr.p->op_key;
    signal->theData[1] = reference();
    signal->theData[2] = createTabPtr.p->m_request.tableId;
    sendSignal(DBTC_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
    return;
  }

  if (refToBlock(signal->getSendersBlockRef()) == DBTC)
  {
    jam();
    execute(signal, createTabPtr.p->m_callback, 0);
    return;
  }

  ndbrequire(false);
}

void
Dbdict::execTAB_COMMITREF(Signal* signal) {
  jamEntry();
  ndbrequire(false);
}//execTAB_COMMITREF()

void
Dbdict::createTab_localComplete(Signal* signal,
                                Uint32 op_key,
                                Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  //const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  //@todo check for master failed

  if (ERROR_INSERTED(6131)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(op_ptr.p->m_error, 9131, __LINE__);
  }

  if (!hasError(op_ptr.p->m_error)) {
    jam();
    // prepare done
    sendTransConf(signal, op_ptr);
  } else {
    jam();
    sendTransRef(signal, op_ptr);
  }
}

// CreateTable: COMMIT

void
Dbdict::createTable_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);

  Uint32 tableId = createTabPtr.p->m_request.tableId;
  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, tableId);
  ndbrequire(ok);

  D("createTable_commit" << *op_ptr.p);

  Callback c;
  c.m_callbackData = op_ptr.p->op_key;
  c.m_callbackFunction = safe_cast(&Dbdict::createTab_alterComplete);
  createTab_activate(signal, op_ptr, &c);

  if (DictTabInfo::isIndex(tabPtr.p->tableType))
  {
    TableRecordPtr basePtr;
    bool ok = find_object(basePtr, tabPtr.p->primaryTableId);
    ndbrequire(ok);

    LocalTableRecord_list list(c_tableRecordPool_, basePtr.p->m_indexes);
    list.add(tabPtr);
  }
}

void
Dbdict::createTab_activate(Signal* signal, SchemaOpPtr op_ptr,
			  Callback * c)
{
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  createTabPtr.p->m_callback = * c;

  signal->theData[0] = op_ptr.p->op_key;
  signal->theData[1] = reference();
  signal->theData[2] = createTabPtr.p->m_request.tableId;
  sendSignal(DBDIH_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
}

void
Dbdict::execTC_SCHVERCONF(Signal* signal)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, signal->theData[1]);
  ndbrequire(!op_ptr.isNull());

  if (refToBlock(signal->getSendersBlockRef()) == DBSPJ)
  {
    jam();
    // prepare table in DBTC
    TableRecordPtr tabPtr;
    bool ok = find_object(tabPtr, createTabPtr.p->m_request.tableId);
    ndbrequire(ok);

    TcSchVerReq * req = (TcSchVerReq*)signal->getDataPtr();
    req->tableId = createTabPtr.p->m_request.tableId;
    req->tableVersion = tabPtr.p->tableVersion;
    req->tableLogged = (Uint32)!!(tabPtr.p->m_bits & TableRecord::TR_Logged);
    req->senderRef = reference();
    req->tableType = (Uint32)tabPtr.p->tableType;
    req->senderData = op_ptr.p->op_key;
    req->noOfPrimaryKeys = (Uint32)tabPtr.p->noOfPrimkey;
    req->singleUserMode = (Uint32)tabPtr.p->singleUserMode;
    req->userDefinedPartition = (tabPtr.p->fragmentType == DictTabInfo::UserDefined);

    if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
    {
      jam();
      TableRecordPtr basePtr;
      bool ok = find_object(basePtr, tabPtr.p->primaryTableId);
      ndbrequire(ok);
      req->userDefinedPartition = (basePtr.p->fragmentType == DictTabInfo::UserDefined);
    }

    sendSignal(DBTC_REF, GSN_TC_SCHVERREQ, signal,
               TcSchVerReq::SignalLength, JBB);
    return;
  }
  ndbrequire(refToBlock(signal->getSendersBlockRef()) == DBTC);
  execute(signal, createTabPtr.p->m_callback, 0);
}

void
Dbdict::createTab_alterComplete(Signal* signal,
				Uint32 op_key,
				Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, impl_req->tableId);
  ndbrequire(ok);

  D("createTab_alterComplete" << *op_ptr.p);

  //@todo check error
  //@todo check master failed

  // inform SUMA
  {
    CreateTabConf* conf = (CreateTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = impl_req->tableId; // special usage
#if 0
    signal->header.m_noOfSections = 1;
    SegmentedSectionPtr tabInfoPtr;
    getSection(tabInfoPtr, createTabPtr.p->m_tabInfoPtrI);
    signal->setSection(tabInfoPtr, 0);
#endif
    sendSignal(SUMA_REF, GSN_CREATE_TAB_CONF, signal,
               CreateTabConf::SignalLength, JBB);
#if 0
    signal->header.m_noOfSections = 0;
#endif
  }

  sendTransConf(signal, op_ptr);
}

// CreateTable: COMPLETE

void
Dbdict::createTable_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

void
Dbdict::createTable_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;
  const Uint32 tableId = impl_req->tableId;

  D("createTable_abortParse" << V(tableId) << *op_ptr.p);

  do {
    if (createTabPtr.p->m_abortPrepareDone)
    {
      jam();
      D("done by abort prepare");
      break;
    }

    if (tableId == RNIL) {
      jam();
      D("no table allocated");
      break;
    }

    TableRecordPtr tabPtr;
    bool ok = find_object(tabPtr, tableId);

    // any link was to a new object
    if (hasDictObject(op_ptr)) {
      jam();
      unlinkDictObject(op_ptr);
      if (ok)
      {
        jam();
        releaseTableObject(tabPtr.i, true);
      }
    }
  } while (0);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createTable_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;
  //const Uint32 tableId = impl_req->tableId;

  D("createTable_abortPrepare" << *op_ptr.p);

  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, impl_req->tableId);
  ndbrequire(ok);

  // create drop table operation  wl3600_todo must pre-allocate

  SchemaOpPtr oplnk_ptr;
  DropTableRecPtr dropTabPtr;
  ok = seizeLinkedSchemaOp(op_ptr, oplnk_ptr, dropTabPtr);
  ndbrequire(ok);

  DropTabReq* aux_impl_req = &dropTabPtr.p->m_request;

  aux_impl_req->senderRef = impl_req->senderRef;
  aux_impl_req->senderData = impl_req->senderData;
  aux_impl_req->requestType = DropTabReq::CreateTabDrop;
  aux_impl_req->tableId = impl_req->tableId;
  aux_impl_req->tableVersion = impl_req->tableVersion;

  // wl3600_todo use ref count
  unlinkDictObject(op_ptr);

  dropTabPtr.p->m_block = 0;
  dropTabPtr.p->m_blockNo[0] = DBTC;
  dropTabPtr.p->m_blockNo[1] = DBSPJ;
  dropTabPtr.p->m_blockNo[2] = DBLQH; // wait usage + LCP
  dropTabPtr.p->m_blockNo[3] = DBDIH; //
  dropTabPtr.p->m_blockNo[4] = DBLQH; // release
  dropTabPtr.p->m_blockNo[5] = 0;

  dropTabPtr.p->m_callback.m_callbackData =
    oplnk_ptr.p->op_key;
  dropTabPtr.p->m_callback.m_callbackFunction =
    safe_cast(&Dbdict::createTable_abortLocalConf);

  // invoke the "complete" phase of drop table
  dropTable_complete_nextStep(signal, oplnk_ptr);

  if (tabPtr.p->m_tablespace_id != RNIL) {
    FilegroupPtr ptr;
    ndbrequire(find_object(ptr, tabPtr.p->m_tablespace_id));
    decrease_ref_count(ptr.p->m_obj_ptr_i);
  }
}

void
Dbdict::createTable_abortLocalConf(Signal* signal,
                                   Uint32 oplnk_key,
                                   Uint32 ret)
{
  jam();
  D("createTable_abortLocalConf" << V(oplnk_key));

  SchemaOpPtr oplnk_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(oplnk_ptr, dropTabPtr, oplnk_key);
  ndbrequire(!oplnk_ptr.isNull());

  SchemaOpPtr op_ptr = oplnk_ptr.p->m_opbck_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;
  Uint32 tableId = impl_req->tableId;

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, tableId);
  if (ok)
  {
    jam();
    releaseTableObject(tablePtr.i);
  }
  createTabPtr.p->m_abortPrepareDone = true;
  sendTransConf(signal, op_ptr);
}

// CreateTable: MISC

void
Dbdict::execCREATE_FRAGMENTATION_REF(Signal * signal)
{
  // currently not received
  ndbrequire(false);
}

void
Dbdict::execCREATE_FRAGMENTATION_CONF(Signal* signal)
{
  // currently not received
  ndbrequire(false);
}

void
Dbdict::execCREATE_TAB_REQ(Signal* signal)
{
  // no longer received
  ndbrequire(false);
}

void Dbdict::execCREATE_TABLE_CONF(Signal* signal)
{
  jamEntry();
  const CreateTableConf* conf = (const CreateTableConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void Dbdict::execCREATE_TABLE_REF(Signal* signal)
{
  jamEntry();
  const CreateTableRef* ref = (const CreateTableRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void Dbdict::releaseTableObject(Uint32 table_ptr_i, bool removeFromHash)
{
  TableRecordPtr tablePtr;
  c_tableRecordPool_.getPtr(tablePtr, table_ptr_i);
  if (removeFromHash)
  {
    jam();
    ndbrequire(tablePtr.p->m_obj_ptr_i != RNIL);
    release_object(tablePtr.p->m_obj_ptr_i);
    tablePtr.p->m_obj_ptr_i = RNIL;
  }
  else
  {
    ndbrequire(tablePtr.p->m_obj_ptr_i == RNIL);
    LocalRope tmp(c_rope_pool, tablePtr.p->tableName);
    tmp.erase();
  }

  {
    LocalRope tmp(c_rope_pool, tablePtr.p->frmData);
    tmp.erase();
  }

  {
    LocalRope tmp(c_rope_pool, tablePtr.p->ngData);
    tmp.erase();
  }

  {
    LocalRope tmp(c_rope_pool, tablePtr.p->rangeData);
    tmp.erase();
  }

  LocalAttributeRecord_list list(c_attributeRecordPool,
					tablePtr.p->m_attributes);
  AttributeRecordPtr attrPtr;
  for(list.first(attrPtr); !attrPtr.isNull(); list.next(attrPtr)){
    LocalRope name(c_rope_pool, attrPtr.p->attributeName);
    LocalRope def(c_rope_pool, attrPtr.p->defaultValue);
    name.erase();
    def.erase();
  }
  list.release();

  {
    if (tablePtr.p->m_upgrade_trigger_handling.m_upgrade)
    {
      jam();
      Uint32 triggerId;

      triggerId = tablePtr.p->m_upgrade_trigger_handling.updateTriggerId;
      if (triggerId != RNIL)
      {
        jam();
        TriggerRecordPtr triggerPtr;
        bool ok = find_object(triggerPtr, triggerId);
        if (ok)
        {
          release_object(triggerPtr.p->m_obj_ptr_i);
          c_triggerRecordPool_.release(triggerPtr);
        }
      }

      triggerId = tablePtr.p->m_upgrade_trigger_handling.deleteTriggerId;
      if (triggerId != RNIL)
      {
        jam();
        TriggerRecordPtr triggerPtr;
        bool ok = find_object(triggerPtr, triggerId);
        if (ok)
        {
          release_object(triggerPtr.p->m_obj_ptr_i);
          c_triggerRecordPool_.release(triggerPtr);
        }
      }
    }
  }
  c_tableRecordPool_.release(tablePtr);
}//releaseTableObject()

// CreateTable: END

// MODULE: DropTable

const Dbdict::OpInfo
Dbdict::DropTableRec::g_opInfo = {
  { 'D', 'T', 'a', 0 },
  ~RT_DBDICT_DROP_TABLE,
  GSN_DROP_TAB_REQ,
  DropTabReq::SignalLength,
  //
  &Dbdict::dropTable_seize,
  &Dbdict::dropTable_release,
  //
  &Dbdict::dropTable_parse,
  &Dbdict::dropTable_subOps,
  &Dbdict::dropTable_reply,
  //
  &Dbdict::dropTable_prepare,
  &Dbdict::dropTable_commit,
  &Dbdict::dropTable_complete,
  //
  &Dbdict::dropTable_abortParse,
  &Dbdict::dropTable_abortPrepare
};

bool
Dbdict::dropTable_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropTableRec>(op_ptr);
}

void
Dbdict::dropTable_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropTableRec>(op_ptr);
}

void
Dbdict::execDROP_TABLE_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  if (check_sender_version(signal, MAKE_VERSION(6,4,0)) < 0)
  {
    jam();
    /**
     * Pekka has for some obscure reason switched places of
     *   senderRef/senderData
     */
    DropTableReq * tmp = (DropTableReq*)signal->getDataPtr();
    do_swap(tmp->senderRef, tmp->senderData);
  }

  const DropTableReq req_copy =
    *(const DropTableReq*)signal->getDataPtr();
  const DropTableReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropTableRecPtr dropTabPtr;
    DropTabReq* impl_req;

    startClientReq(op_ptr, dropTabPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropTableRef* ref = (DropTableRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->tableId = req->tableId;
  ref->tableVersion = req->tableVersion;
  getError(error, ref);
  sendSignal(req->clientRef, GSN_DROP_TABLE_REF, signal,
             DropTableRef::SignalLength, JBB);
}

// DropTable: PARSE

void
Dbdict::dropTable_parse(Signal* signal, bool master,
                        SchemaOpPtr op_ptr,
                        SectionHandle& handle, ErrorInfo& error)
{
  D("dropTable_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  DropTabReq* impl_req = &dropTabPtr.p->m_request;
  Uint32 tableId = impl_req->tableId;
  Uint32 err;

  TableRecordPtr tablePtr;
  if (!(tableId < c_noOfMetaTables)) {
    jam();
    setError(error, DropTableRef::NoSuchTable, __LINE__);
    return;
  }

  err = check_read_obj(impl_req->tableId, trans_ptr.p->m_transId);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }

  bool ok = find_object(tablePtr, impl_req->tableId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }


  // check version first (api will retry)
  if (tablePtr.p->tableVersion != impl_req->tableVersion) {
    jam();
    setError(error, DropTableRef::InvalidTableVersion, __LINE__);
    return;
  }

  if (tablePtr.p->m_read_locked)
  {
    jam();
    setError(error, DropTableRef::BackupInProgress, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->tableId,
                      trans_ptr.p->m_transId,
                      SchemaFile::SF_DROP, error))
  {
    jam();
    return;
  }

  // link operation to object
  {
    DictObjectPtr obj_ptr;
    Uint32 obj_ptr_i = tablePtr.p->m_obj_ptr_i;
    bool ok = findDictObject(op_ptr, obj_ptr, obj_ptr_i);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6121)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9121, __LINE__);
    return;
  }

  SchemaFile::TableEntry te; te.init();
  te.m_tableState = SchemaFile::SF_DROP;
  te.m_transId = trans_ptr.p->m_transId;
  err = trans_log_schema_op(op_ptr, tableId, &te);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }
}

bool
Dbdict::dropTable_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTable_subOps" << V(op_ptr.i) << *op_ptr.p);
  // wl3600_todo blobs, indexes, events
  return false;
}

void
Dbdict::dropTable_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("dropTable_reply" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  if (!hasError(error)) {
    DropTableConf* conf = (DropTableConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = dropTabPtr.p->m_request.tableId;
    conf->tableVersion = dropTabPtr.p->m_request.tableVersion;

    D(V(conf->tableId) << V(conf->tableVersion));

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_TABLE_CONF, signal,
               DropTableConf::SignalLength, JBB);
  } else {
    jam();
    DropTableRef* ref = (DropTableRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    ref->tableId = dropTabPtr.p->m_request.tableId;
    ref->tableVersion = dropTabPtr.p->m_request.tableVersion;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_TABLE_REF, signal,
               DropTableRef::SignalLength, JBB);
  }
}

// DropTable: PREPARE

void
Dbdict::dropTable_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  D("dropTable_prepare" << *op_ptr.p);

  Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
  Callback c = {
    safe_cast(&Dbdict::dropTable_backup_mutex_locked),
    op_ptr.p->op_key
  };
  bool ok = mutex.lock(c);
  ndbrequire(ok);
  return;
}

void
Dbdict::dropTable_backup_mutex_locked(Signal* signal,
                                      Uint32 op_key,
                                      Uint32 ret)
{
  jamEntry();
  D("dropTable_backup_mutex_locked");

  ndbrequire(ret == 0);

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, impl_req->tableId);
  ndbrequire(ok);

  Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
  mutex.unlock(); // ignore response

  if (tablePtr.p->m_read_locked)
  {
    jam();
    setError(op_ptr, AlterTableRef::BackupInProgress, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  sendTransConf(signal, op_ptr);
}
// DropTable: COMMIT

void
Dbdict::dropTable_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  D("dropTable_commit" << *op_ptr.p);

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, dropTabPtr.p->m_request.tableId);
  ndbrequire(ok);

  if (tablePtr.p->m_tablespace_id != RNIL)
  {
    FilegroupPtr ptr;
    ndbrequire(find_object(ptr, tablePtr.p->m_tablespace_id));
    decrease_ref_count(ptr.p->m_obj_ptr_i);
  }

  if (g_trace)
  // from a newer execDROP_TAB_REQ version
  {
    char buf[1024];
    LocalRope name(c_rope_pool, tablePtr.p->tableName);
    name.copy(buf);
    g_eventLogger->info("Dbdict: drop name=%s,id=%u,obj_id=%u", buf, dropTabPtr.p->m_request.tableId,
                        tablePtr.p->m_obj_ptr_i);
  }

  send_event(signal, trans_ptr,
             NDB_LE_DropSchemaObject,
             dropTabPtr.p->m_request.tableId,
             tablePtr.p->tableVersion,
             tablePtr.p->tableType);

  if (DictTabInfo::isIndex(tablePtr.p->tableType))
  {
    TableRecordPtr basePtr;
    bool ok = find_object(basePtr, tablePtr.p->primaryTableId);
    ndbrequire(ok);
    LocalTableRecord_list list(c_tableRecordPool_, basePtr.p->m_indexes);
    list.remove(tablePtr);
  }
  dropTabPtr.p->m_block = 0;
  dropTabPtr.p->m_blockNo[0] = DBLQH;
  dropTabPtr.p->m_blockNo[1] = DBSPJ;
  dropTabPtr.p->m_blockNo[2] = DBTC;
  dropTabPtr.p->m_blockNo[3] = DBDIH;
  dropTabPtr.p->m_blockNo[4] = 0;
  dropTable_commit_nextStep(signal, op_ptr);
}


void
Dbdict::dropTable_commit_nextStep(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  /**
   * No errors currently allowed
   */
  ndbrequire(!hasError(op_ptr.p->m_error));

  Uint32 block = dropTabPtr.p->m_block;
  Uint32 blockNo = dropTabPtr.p->m_blockNo[block];
  D("dropTable_commit_nextStep" << hex << V(blockNo) << *op_ptr.p);

  if (blockNo == 0)
  {
    jam();
    dropTable_commit_done(signal, op_ptr);
    return;
  }

  if (ERROR_INSERTED(6131) &&
      blockNo == DBDIH) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;

    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = numberToRef(block, getOwnNodeId());
    ref->senderData = op_ptr.p->op_key;
    ref->tableId = impl_req->tableId;
    ref->errorCode = 9131;
    sendSignal(reference(), GSN_PREP_DROP_TAB_REF, signal,
               PrepDropTabRef::SignalLength, JBB);
    return;
  }


  PrepDropTabReq* prep = (PrepDropTabReq*)signal->getDataPtrSend();
  prep->senderRef = reference();
  prep->senderData = op_ptr.p->op_key;
  prep->tableId = impl_req->tableId;
  prep->requestType = impl_req->requestType;

  BlockReference ref = numberToRef(blockNo, getOwnNodeId());
  sendSignal(ref, GSN_PREP_DROP_TAB_REQ, signal,
             PrepDropTabReq::SignalLength, JBB);
}

void
Dbdict::execPREP_DROP_TAB_REQ(Signal* signal)
{
  // no longer received
  ndbrequire(false);
}

void
Dbdict::execPREP_DROP_TAB_CONF(Signal * signal)
{
  jamEntry();
  const PrepDropTabConf* conf = (const PrepDropTabConf*)signal->getDataPtr();

  Uint32 nodeId = refToNode(conf->senderRef);
  Uint32 block = refToBlock(conf->senderRef);
  ndbrequire(nodeId == getOwnNodeId() && block != DBDICT);

  dropTable_commit_fromLocal(signal, conf->senderData, 0);
}

void
Dbdict::execPREP_DROP_TAB_REF(Signal* signal)
{
  jamEntry();
  const PrepDropTabRef* ref = (const PrepDropTabRef*)signal->getDataPtr();

  Uint32 nodeId = refToNode(ref->senderRef);
  Uint32 block = refToBlock(ref->senderRef);
  ndbrequire(nodeId == getOwnNodeId() && block != DBDICT);

  Uint32 errorCode = ref->errorCode;
  ndbrequire(errorCode != 0);

  if (errorCode == PrepDropTabRef::NoSuchTable && block == DBLQH)
  {
    jam();
    /**
     * Ignore errors:
     * 1) no such table and LQH, it might not exists in different LQH's
     */
    errorCode = 0;
  }
  dropTable_commit_fromLocal(signal, ref->senderData, errorCode);
}

void
Dbdict::dropTable_commit_fromLocal(Signal* signal, Uint32 op_key, Uint32 errorCode)
{
  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (errorCode != 0)
  {
    jam();
    setError(op_ptr, errorCode, __LINE__);
  }

  dropTabPtr.p->m_block++;
  dropTable_commit_nextStep(signal, op_ptr);
}

void
Dbdict::dropTable_commit_done(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("dropTable_commit_done");

  sendTransConf(signal, op_ptr);
}

// DropTable: COMPLETE

void
Dbdict::dropTable_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  dropTabPtr.p->m_block = 0;
  dropTabPtr.p->m_blockNo[0] = DBTC;
  dropTabPtr.p->m_blockNo[1] = DBSPJ;
  dropTabPtr.p->m_blockNo[2] = DBLQH; // wait usage + LCP
  dropTabPtr.p->m_blockNo[3] = DBDIH; //
  dropTabPtr.p->m_blockNo[4] = DBLQH; // release
  dropTabPtr.p->m_blockNo[5] = 0;
  dropTabPtr.p->m_callback.m_callbackData =
    op_ptr.p->op_key;
  dropTabPtr.p->m_callback.m_callbackFunction =
    safe_cast(&Dbdict::dropTable_complete_done);

  dropTable_complete_nextStep(signal, op_ptr);
}

void
Dbdict::dropTable_complete_nextStep(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  /**
   * No errors currently allowed
   */
  ndbrequire(!hasError(op_ptr.p->m_error));

  Uint32 block = dropTabPtr.p->m_block;
  Uint32 blockNo = dropTabPtr.p->m_blockNo[block];
  D("dropTable_complete_nextStep" << hex << V(blockNo) << *op_ptr.p);

  if (blockNo == 0)
  {
    jam();
    execute(signal, dropTabPtr.p->m_callback, 0);
    return;
  }

  DropTabReq* req = (DropTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->tableId = impl_req->tableId;
  req->requestType = impl_req->requestType;

  BlockReference ref = numberToRef(blockNo, getOwnNodeId());
  sendSignal(ref, GSN_DROP_TAB_REQ, signal,
             DropTabReq::SignalLength, JBB);
}

void
Dbdict::execDROP_TAB_CONF(Signal* signal)
{
  jamEntry();
  const DropTabConf* conf = (const DropTabConf*)signal->getDataPtr();

  Uint32 nodeId = refToNode(conf->senderRef);
  Uint32 block = refToBlock(conf->senderRef);
  ndbrequire(nodeId == getOwnNodeId() && block != DBDICT);

  dropTable_complete_fromLocal(signal, conf->senderData);
}

void
Dbdict::execDROP_TAB_REF(Signal* signal)
{
  jamEntry();
  const DropTabRef* ref = (const DropTabRef*)signal->getDataPtr();

  Uint32 nodeId = refToNode(ref->senderRef);
  Uint32 block = refToBlock(ref->senderRef);
  ndbrequire(nodeId == getOwnNodeId() && block != DBDICT);
  ndbrequire(ref->errorCode == DropTabRef::NoSuchTable);

  dropTable_complete_fromLocal(signal, ref->senderData);
}

void
Dbdict::dropTable_complete_fromLocal(Signal* signal, Uint32 op_key)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  //const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  D("dropTable_complete_fromLocal" << *op_ptr.p);

  dropTabPtr.p->m_block++;
  dropTable_complete_nextStep(signal, op_ptr);
}

void
Dbdict::dropTable_complete_done(Signal* signal,
                                Uint32 op_key,
                                Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  Uint32 tableId = dropTabPtr.p->m_request.tableId;

  unlinkDictObject(op_ptr);
  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, tableId);
  if (ok)
  {
    releaseTableObject(tablePtr.i);
  }

  // inform SUMA
  {
    DropTabConf* conf = (DropTabConf*)signal->getDataPtrSend();

    // special use of senderRef
    if (trans_ptr.p->m_isMaster) {
      jam();
      conf->senderRef = trans_ptr.p->m_clientRef;
    } else {
      jam();
      conf->senderRef = 0;
    }
    conf->senderData = op_key;
    conf->tableId = tableId;
    sendSignal(SUMA_REF, GSN_DROP_TAB_CONF, signal,
               DropTabConf::SignalLength, JBB);
  }
  ndbassert(op_ptr.p->m_state == SchemaOp::OS_COMPLETING);
  sendTransConf(signal, op_ptr);
}

// DropTable: ABORT

void
Dbdict::dropTable_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTable_abortParse" << *op_ptr.p);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropTable_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTable_abortPrepare" << *op_ptr.p);

  sendTransConf(signal, op_ptr);
}

// DropTable: MISC

void Dbdict::execDROP_TABLE_CONF(Signal* signal)
{
  jamEntry();
  const DropTableConf* conf = (const DropTableConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void Dbdict::execDROP_TABLE_REF(Signal* signal)
{
  jamEntry();
  const DropTableRef* ref = (const DropTableRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

// DropTable: END

// MODULE: AlterTable

const Dbdict::OpInfo
Dbdict::AlterTableRec::g_opInfo = {
  { 'A', 'T', 'a', 0 },
  ~RT_DBDICT_ALTER_TABLE,
  GSN_ALTER_TAB_REQ,
  AlterTabReq::SignalLength,
  //
  &Dbdict::alterTable_seize,
  &Dbdict::alterTable_release,
  //
  &Dbdict::alterTable_parse,
  &Dbdict::alterTable_subOps,
  &Dbdict::alterTable_reply,
  //
  &Dbdict::alterTable_prepare,
  &Dbdict::alterTable_commit,
  &Dbdict::alterTable_complete,
  //
  &Dbdict::alterTable_abortParse,
  &Dbdict::alterTable_abortPrepare
};

bool
Dbdict::alterTable_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<AlterTableRec>(op_ptr);
}

void
Dbdict::alterTable_release(SchemaOpPtr op_ptr)
{
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  {
    LocalRope r(c_rope_pool, alterTabPtr.p->m_oldTableName);
    r.erase();
  }
  {
    LocalRope r(c_rope_pool, alterTabPtr.p->m_oldFrmData);
    r.erase();
  }
  LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
  release(op_sec_pool, alterTabPtr.p->m_newAttrData);
  releaseOpRec<AlterTableRec>(op_ptr);
}

bool
Dbdict::check_ndb_versions() const
{
  Uint32 node = 0;
  Uint32 version = getNodeInfo(getOwnNodeId()).m_version;
  while((node = c_aliveNodes.find(node + 1)) != BitmaskImpl::NotFound)
  {
    if(getNodeInfo(node).m_version != version)
    {
      return false;
    }
  }
  return true;
}

int
Dbdict::check_sender_version(const Signal* signal, Uint32 version) const
{
  Uint32 ver = getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version;
  if (ver < version)
    return -1;
  else if (ver > version)
    return 1;
  return 0;
}

void
Dbdict::execALTER_TABLE_REQ(Signal* signal)
{
  jamEntry();

  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  if (check_sender_version(signal, MAKE_VERSION(6,4,0)) < 0)
  {
    jam();
    /**
     * Pekka has for some obscure reason switched places of
     *   senderRef/senderData
     */
    AlterTableReq * tmp = (AlterTableReq*)signal->getDataPtr();
    do_swap(tmp->clientRef, tmp->clientData);
  }

  const AlterTableReq req_copy =
    *(const AlterTableReq*)signal->getDataPtr();
  const AlterTableReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    AlterTableRecPtr alterTabPtr;
    AlterTabReq* impl_req;

    startClientReq(op_ptr, alterTabPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;
    impl_req->newTableVersion = 0; // set in master parse
    impl_req->gci = 0;
    impl_req->changeMask = req->changeMask;
    impl_req->connectPtr = RNIL; // set from TUP
    impl_req->noOfNewAttr = 0; // set these in master parse
    impl_req->newNoOfCharsets = 0;
    impl_req->newNoOfKeyAttrs = 0;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  AlterTableRef* ref = (AlterTableRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  getError(error, ref);
  sendSignal(req->clientRef, GSN_ALTER_TABLE_REF, signal,
             AlterTableRef::SignalLength, JBB);
}

// AlterTable: PARSE

void
Dbdict::alterTable_parse(Signal* signal, bool master,
                         SchemaOpPtr op_ptr,
                         SectionHandle& handle, ErrorInfo& error)
{
  D("alterTable_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  AlterTabReq* impl_req = &alterTabPtr.p->m_request;
  Uint32 err;

  if (AlterTableReq::getReorgSubOp(impl_req->changeMask))
  {
    /**
     * This should only be a sub-op to AddFragFrag
     */
    if (master && op_ptr.p->m_base_op_ptr_i == RNIL)
    {
      jam();
      setError(error, AlterTableRef::Inconsistency, __LINE__);
      return;
    }

    return;
  }

  // get table definition
  TableRecordPtr tablePtr;
  if (!(impl_req->tableId < c_noOfMetaTables)) {
    jam();
    setError(error, AlterTableRef::NoSuchTable, __LINE__);
    return;
  }

  bool ok = find_object(tablePtr, impl_req->tableId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }

  if (tablePtr.p->m_read_locked)
  {
    jam();
    setError(error, tablePtr.p->m_read_locked, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->tableId, trans_ptr.p->m_transId,
                      SchemaFile::SF_ALTER, error))
  {
    jam();
    return;
  }

  // save it for abort code

  if (tablePtr.p->tableVersion != impl_req->tableVersion) {
    jam();
    setError(error, AlterTableRef::InvalidTableVersion, __LINE__);
    return;
  }

  // parse new table definition into new table record
  TableRecordPtr newTablePtr;
  {
    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::AlterTableFromAPI;
    parseRecord.errorCode = 0;

    SegmentedSectionPtr ptr;
    bool ok = handle.getSection(ptr, AlterTableReq::DICT_TAB_INFO);
    ndbrequire(ok);
    SimplePropertiesSectionReader r(ptr, getSectionSegmentPool());

    handleTabInfoInit(signal,
                      trans_ptr, r, &parseRecord, false); // Will not save info

    if (parseRecord.errorCode != 0) {
      jam();
      setError(error, parseRecord);
      return;
    }

    // the new temporary table record seized from pool
    newTablePtr = parseRecord.tablePtr;
    alterTabPtr.p->m_newTablePtrI = newTablePtr.i;
    alterTabPtr.p->m_newTable_realObjectId = newTablePtr.p->tableId;
    newTablePtr.p->tableId = impl_req->tableId; // set correct table id...(not the temporary)
  }


  {
    /**
     * Mark SchemaObject as in-use so that it's won't be found by other op
     *   choose a state that will be automatically cleaned incase we crash
     */
    SchemaFile::TableEntry *
      objEntry = getTableEntry(alterTabPtr.p->m_newTable_realObjectId);
    objEntry->m_tableType = DictTabInfo::SchemaTransaction;
    objEntry->m_tableState = SchemaFile::SF_STARTED;
    objEntry->m_transId = trans_ptr.p->m_transId + 1;
  }

  // set the new version now
  impl_req->newTableVersion =
    newTablePtr.p->tableVersion =
    alter_obj_inc_schema_version(tablePtr.p->tableVersion);

  // rename stuff
  {
    ConstRope r1(c_rope_pool, tablePtr.p->tableName);
    ConstRope r2(c_rope_pool, newTablePtr.p->tableName);

    char name[MAX_TAB_NAME_SIZE];
    r2.copy(name);

    if (r1.compare(name) != 0)
    {
      jam();
      if (get_object(name) != 0)
      {
        jam();
        setError(error, CreateTableRef::TableAlreadyExist, __LINE__);
        return;
      }

      if (master)
      {
        jam();
        AlterTableReq::setNameFlag(impl_req->changeMask, 1);
      }
      else if (!AlterTableReq::getNameFlag(impl_req->changeMask))
      {
        jam();
        setError(error, AlterTableRef::Inconsistency, __LINE__);
        return;
      }
    }
    else if (AlterTableReq::getNameFlag(impl_req->changeMask))
    {
      jam();
      setError(error, AlterTableRef::Inconsistency, __LINE__);
      return;
    }
  }

  // frm stuff
  {
    ConstRope r1(c_rope_pool, tablePtr.p->frmData);
    ConstRope r2(c_rope_pool, newTablePtr.p->frmData);
    if (!r1.equal(r2))
    {
      if (master)
      {
        jam();
        AlterTableReq::setFrmFlag(impl_req->changeMask, 1);
      }
      else if (!AlterTableReq::getFrmFlag(impl_req->changeMask))
      {
        jam();
        setError(error, AlterTableRef::Inconsistency, __LINE__);
        return;
      }
    }
    else if (AlterTableReq::getFrmFlag(impl_req->changeMask))
    {
      jam();
      setError(error, AlterTableRef::Inconsistency, __LINE__);
      return;
    }
  }

  // add attribute stuff
  {
    const Uint32 noOfNewAttr =
      newTablePtr.p->noOfAttributes - tablePtr.p->noOfAttributes;

    if (newTablePtr.p->noOfAttributes > tablePtr.p->noOfAttributes)
    {
      if (master)
      {
        jam();
        AlterTableReq::setAddAttrFlag(impl_req->changeMask, 1);
      }
      else if (!AlterTableReq::getAddAttrFlag(impl_req->changeMask))
      {
        jam();
        setError(error, AlterTableRef::Inconsistency, __LINE__);
        return;
      }
    }
    else if (AlterTableReq::getAddAttrFlag(impl_req->changeMask))
    {
      jam();
      setError(error, AlterTableRef::Inconsistency, __LINE__);
      return;
    }
    else if (newTablePtr.p->noOfAttributes < tablePtr.p->noOfAttributes)
    {
      jam();
      setError(error, AlterTableRef::UnsupportedChange, __LINE__);
      return;
    }

    if (master)
    {
      jam();
      impl_req->noOfNewAttr = noOfNewAttr;
      impl_req->newNoOfCharsets = newTablePtr.p->noOfCharsets;
      impl_req->newNoOfKeyAttrs = newTablePtr.p->noOfPrimkey;
    }
    else if (impl_req->noOfNewAttr != noOfNewAttr)
    {
      jam();
      setError(error, AlterTableRef::Inconsistency, __LINE__);
      return;
    }

    LocalAttributeRecord_list
      list(c_attributeRecordPool, newTablePtr.p->m_attributes);
    AttributeRecordPtr attrPtr;
    list.first(attrPtr);
    Uint32 i = 0;
    LocalArenaPoolImpl op_sec_pool(trans_ptr.p->m_arena, c_opSectionBufferPool);
    for (i = 0; i < newTablePtr.p->noOfAttributes; i++) {
      if (i >= tablePtr.p->noOfAttributes) {
        jam();
        Uint32 attrData[2];
        attrData[0] = attrPtr.p->attributeDescriptor;
        attrData[1] = attrPtr.p->extPrecision & ~0xFFFF;
        if(!copyIn(op_sec_pool, alterTabPtr.p->m_newAttrData, attrData, 2))
        {
          jam();
          setError(error, SchemaTransBeginRef::OutOfSchemaTransMemory, __LINE__);
          return;
        }
      }
      list.next(attrPtr);
    }
  }

  if (AlterTableReq::getAddFragFlag(impl_req->changeMask))
  {
    if (newTablePtr.p->fragmentType != DictTabInfo::HashMapPartition)
    {
      jam();
      setError(error, AlterTableRef::UnsupportedChange, __LINE__);
      return;
    }

    /**
     * Verify that reorg is possible with the hash map(s)
     */
    if (ERROR_INSERTED(6212))
    {
      CLEAR_ERROR_INSERT_VALUE;
      setError(error, 1, __LINE__);
      return;
    }

    Uint32 err;
    if ((err = check_supported_reorg(tablePtr.p->hashMapObjectId,
                                     newTablePtr.p->hashMapObjectId)))
    {
      jam();
      setError(error, AlterTableRef::UnsupportedChange, __LINE__);
      return;
    }

    if (tablePtr.p->hashMapObjectId != newTablePtr.p->hashMapObjectId)
    {
      jam();
      AlterTableReq::setReorgFragFlag(impl_req->changeMask, 1);
    }

    if (master)
    {
      /**
       * 1) Create fragmentation for new table
       * 2) Get fragmentation for old table
       * 3) Check if supported alter table and
       *    save fragmentation for new fragment op operation record
       */
      jam();
      Uint32 save0 = newTablePtr.p->fragmentType;
      newTablePtr.p->fragmentType = DictTabInfo::DistrKeyHash;

      /**
       * Here we reset all the NODEGROUPS for the new partitions
       *   i.e they can't be specified...this should change later
       *   once we got our act together
       */
      Uint32 cnt = c_fragDataLen / 2;
      for (Uint32 i = cnt; i<newTablePtr.p->fragmentCount; i++)
      {
        jam();
        c_fragData[i] = NDB_UNDEF_NODEGROUP;
      }
      c_fragDataLen = 2 * newTablePtr.p->fragmentCount;
      Uint32 save1 = newTablePtr.p->primaryTableId;
      Uint32 flags = 0;
      if (save1 == RNIL)
      {
        /**
         * This is a "base" table
         *   signal that this is a add-partitions
         *   by setting primaryTableId to "original" table and setting flag
         */
        flags = CreateFragmentationReq::RI_ADD_PARTITION;
        newTablePtr.p->primaryTableId = tablePtr.p->tableId;
      }
      err = create_fragmentation(signal, newTablePtr,
                                 c_fragData, c_fragDataLen / 2,
                                 flags);
      newTablePtr.p->fragmentType = (DictTabInfo::FragmentType)save0;
      newTablePtr.p->primaryTableId = save1;

      if (err)
      {
        jam();
        setError(error, err, __LINE__);
        return;
      }

      Uint16* data = (Uint16*)(signal->theData+25);
      Uint32 count = 2 + (1 + data[0]) * data[1];
      memcpy(c_fragData, data, 2*count);

      err = get_fragmentation(signal, tablePtr.p->tableId);
      if (err)
      {
        jam();
        setError(error, err, __LINE__);
        return;
      }

      err = check_supported_add_fragment(c_fragData,
                                         (Uint16*)(signal->theData+25));
      if (err)
      {
        jam();
        setError(error, err, __LINE__);
        return;
      }

      count = 2 + (1 + c_fragData[0]) * c_fragData[1];
      c_fragDataLen = sizeof(Uint16)*count;
    }
  }

  D("alterTable_parse " << V(newTablePtr.i) << hex << V(newTablePtr.p->tableVersion));

  if (ERROR_INSERTED(6121)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9121, __LINE__);
    return;
  }

  // master rewrites DictTabInfo (it is re-parsed only on slaves)
  if (master)
  {
    jam();
    releaseSections(handle);
    SimplePropertiesSectionWriter w(* this);
    packTableIntoPages(w, newTablePtr);

    SegmentedSectionPtr tabInfoPtr;
    w.getPtr(tabInfoPtr);
    handle.m_ptr[AlterTabReq::DICT_TAB_INFO] = tabInfoPtr;
    handle.m_cnt = 1;

    if (AlterTableReq::getAddFragFlag(impl_req->changeMask))
    {
      jam();
      SegmentedSectionPtr ss_ptr;
      ndbrequire(import(ss_ptr, c_fragData_align32, (c_fragDataLen+1)/2));
      handle.m_ptr[AlterTabReq::FRAGMENTATION] = ss_ptr;
      handle.m_cnt = 2;
    }
   }


  {
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, AlterTabReq::DICT_TAB_INFO);
    if (ptr.sz > MAX_WORDS_META_FILE)
    {
      jam();
      setError(error, AlterTableRef::TableDefinitionTooBig, __LINE__);
      return;
    }
  }

  // save sections
  saveOpSection(op_ptr, handle, AlterTabReq::DICT_TAB_INFO);
  if (AlterTableReq::getAddFragFlag(impl_req->changeMask))
  {
    jam();
    saveOpSection(op_ptr, handle, AlterTabReq::FRAGMENTATION);
  }

  SchemaFile::TableEntry te; te.init();
  te.m_tableState = SchemaFile::SF_ALTER;
  te.m_tableVersion = newTablePtr.p->tableVersion;
  te.m_info_words = getOpSection(op_ptr, AlterTabReq::DICT_TAB_INFO).getSize();
  te.m_gcp = 0;
  te.m_transId = trans_ptr.p->m_transId;

  err = trans_log_schema_op(op_ptr, impl_req->tableId, &te);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }
}

Uint32
Dbdict::check_supported_reorg(Uint32 org_map_id, Uint32 new_map_id)
{
  if (org_map_id == new_map_id)
  {
    jam();
    return 0;
  }

  HashMapRecordPtr orgmap_ptr;
  ndbrequire(find_object(orgmap_ptr, org_map_id));

  HashMapRecordPtr newmap_ptr;
  ndbrequire(find_object(newmap_ptr, new_map_id));

  Ptr<Hash2FragmentMap> orgptr;
  g_hash_map.getPtr(orgptr, orgmap_ptr.p->m_map_ptr_i);

  Ptr<Hash2FragmentMap> newptr;
  g_hash_map.getPtr(newptr, newmap_ptr.p->m_map_ptr_i);

  /*
   * check that old fragments maps to same old fragment
   * or to a new fragment.
   * allow both extending and shrinking hashmap.
   */
  Uint32 period = lcm(orgptr.p->m_cnt, newptr.p->m_cnt);
  for (Uint32 i = 0; i < period; i++)
  {
    if (orgptr.p->m_map[i % orgptr.p->m_cnt] != newptr.p->m_map[i % newptr.p->m_cnt] &&
        newptr.p->m_map[i % newptr.p->m_cnt] < orgptr.p->m_fragments)
    {
      /**
       * Moving data from "old" fragment into "old" fragment
       *   is not supported...
       */
      jam();
      return AlterTableRef::UnsupportedChange;
    }
  }
  return 0;
}

Uint32
Dbdict::check_supported_add_fragment(Uint16* newdata, const Uint16* olddata)
{
  Uint32 replicas = newdata[0];
  if (replicas != olddata[0])
  {
    jam();
    return AlterTableRef::UnsupportedChange;
  }

  Uint32 fragments = newdata[1];
  if (fragments < olddata[1])
  {
    jam();
    return AlterTableRef::UnsupportedChange;
  }

  Uint32 oldFragments = olddata[1];
#ifdef TODO_XXX
  /**
   * This doesnt work after a add-nodegroup
   *   dont't know why, so we instead just ignore what the API
   *   for the already existing partitions
   */

  // Check that all the old has the same properties...
  // Only compare prefered primary, as replicas come in any order
  for (Uint32 i = 0; i<oldFragments; i++)
  {
    Uint32 idx = 2 + (1 + replicas) * i + 1;
    if (newdata[idx] != olddata[idx])
    {
      jam();
      return AlterTableRef::UnsupportedChange;
    }
  }
#endif

  memmove(newdata + 2,
          newdata + 2 + (1 + replicas) * oldFragments,
          sizeof(short) * (1 + replicas) * (fragments - oldFragments));

  newdata[1] = (fragments - oldFragments);

  return 0;
}

bool
Dbdict::alterTable_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTable_subOps" << V(op_ptr.i) << *op_ptr.p);

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  if (AlterTableReq::getAddFragFlag(impl_req->changeMask))
  {
    jam();
    if (alterTabPtr.p->m_sub_add_frag == false)
    {
      jam();
      TableRecordPtr tabPtr;
      TableRecordPtr indexPtr;
      bool ok = find_object(tabPtr, impl_req->tableId);
      ndbrequire(ok);
      LocalTableRecord_list list(c_tableRecordPool_, tabPtr.p->m_indexes);
      Uint32 ptrI = alterTabPtr.p->m_sub_add_frag_index_ptr;

      if (ptrI == RNIL)
      {
        jam();
        list.first(indexPtr);
      }
      else
      {
        jam();
        list.getPtr(indexPtr, ptrI);
        list.next(indexPtr);
      }

      for (; !indexPtr.isNull(); list.next(indexPtr))
      {
        if (DictTabInfo::isOrderedIndex(indexPtr.p->tableType))
        {
          jam();
          break;
        }
      }
      if (indexPtr.isNull())
      {
        jam();
        alterTabPtr.p->m_sub_add_frag = true;
      }
      else
      {
        jam();
        Callback c = {
          safe_cast(&Dbdict::alterTable_fromAlterIndex),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;

        alterTabPtr.p->m_sub_add_frag_index_ptr = indexPtr.i;
        alterTable_toAlterIndex(signal, op_ptr);
        return true;
      }
    }
  }

  if (AlterTableReq::getReorgFragFlag(impl_req->changeMask))
  {
    if (alterTabPtr.p->m_sub_reorg_commit == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromReorgTable),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTabPtr.p->m_sub_reorg_commit = true;
      alterTable_toReorgTable(signal, op_ptr, 0);
      return true;
    }

    if (alterTabPtr.p->m_sub_suma_enable == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromReorgTable),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTabPtr.p->m_sub_suma_enable = true;
      alterTable_toSumaSync(signal, op_ptr, 0);
      return true;
    }


    if (alterTabPtr.p->m_sub_suma_filter == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromReorgTable),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTabPtr.p->m_sub_suma_filter = true;
      alterTable_toSumaSync(signal, op_ptr, 1);
      return true;
    }

    if (alterTabPtr.p->m_sub_trigger == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromCreateTrigger),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTable_toCreateTrigger(signal, op_ptr);

      alterTabPtr.p->m_sub_trigger = true;
      return true;
    }

    if (alterTabPtr.p->m_sub_copy_data == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromCopyData),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTable_toCopyData(signal, op_ptr);

      alterTabPtr.p->m_sub_copy_data = true;
      return true;
    }

    if (alterTabPtr.p->m_sub_reorg_complete == false)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterTable_fromReorgTable),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      alterTabPtr.p->m_sub_reorg_complete = true;
      alterTable_toReorgTable(signal, op_ptr, 1);
      return true;
    }
  }

  return false;
}

void
Dbdict::alterTable_toAlterIndex(Signal* signal,
                                SchemaOpPtr op_ptr)
{
  jam();
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  TableRecordPtr indexPtr;
  c_tableRecordPool_.getPtr(indexPtr, alterTabPtr.p->m_sub_add_frag_index_ptr);
  ndbrequire(!indexPtr.isNull());

  AlterIndxReq* req = (AlterIndxReq*)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = 0;
  req->indexId = indexPtr.p->tableId;
  req->indexVersion = indexPtr.p->tableVersion;
  DictSignal::setRequestType(req->requestInfo,
                             AlterIndxImplReq::AlterIndexAddPartition);
  sendSignal(reference(), GSN_ALTER_INDX_REQ, signal,
             AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::alterTable_fromAlterIndex(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTablePtr;

  findSchemaOp(op_ptr, alterTablePtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    const AlterIndxConf* conf =
      (const AlterIndxConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterIndxRef* ref =
      (const AlterIndxRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterTable_toReorgTable(Signal* signal,
                                SchemaOpPtr op_ptr,
                                Uint32 step)
{
  jam();
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  AlterTableReq* req = (AlterTableReq*)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = 0;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->changeMask = 0;
  if (step == 0)
  {
    jam();
    AlterTableReq::setReorgCommitFlag(req->changeMask, 1);
  }
  else if (step == 1)
  {
    jam();
    AlterTableReq::setReorgCompleteFlag(req->changeMask, 1);
  }
  else
  {
    jamLine(step);
    ndbrequire(false);
  }
  sendSignal(reference(), GSN_ALTER_TABLE_REQ, signal,
             AlterTableReq::SignalLength, JBB);
}

void
Dbdict::alterTable_fromReorgTable(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTablePtr;

  findSchemaOp(op_ptr, alterTablePtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0)
  {
    jam();
    const AlterTableConf* conf =
      (const AlterTableConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterTableRef* ref =
      (const AlterTableRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterTable_toCreateTrigger(Signal* signal,
                                   SchemaOpPtr op_ptr)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTableRecPtr alterTablePtr;
  getOpRec(op_ptr, alterTablePtr);
  const AlterTabReq* impl_req = &alterTablePtr.p->m_request;

  const TriggerTmpl& triggerTmpl = g_reorgTriggerTmpl[0];

  CreateTrigReq* req = (CreateTrigReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, CreateTrigReq::CreateTriggerOnline);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->indexId = RNIL;
  req->indexVersion = RNIL;
  req->triggerNo = 0;
  req->forceTriggerId = RNIL;

  TriggerInfo::packTriggerInfo(req->triggerInfo, triggerTmpl.triggerInfo);

  req->receiverRef = 0;

  char triggerName[MAX_TAB_NAME_SIZE];
  sprintf(triggerName, triggerTmpl.nameFormat, impl_req->tableId);

  // name section
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  w.reset();
  w.add(DictTabInfo::TableName, triggerName);
  LinearSectionPtr lsPtr[3];
  lsPtr[0].p = buffer;
  lsPtr[0].sz = w.getWordsUsed();

  AttributeMask mask;
  mask.clear();
  lsPtr[1].p = mask.rep.data;
  lsPtr[1].sz = mask.getSizeInWords();

  sendSignal(reference(), GSN_CREATE_TRIG_REQ, signal,
             CreateTrigReq::SignalLength, JBB, lsPtr, 2);
}

void
Dbdict::alterTable_fromCreateTrigger(Signal* signal, Uint32 op_key, Uint32 ret)
{
  alterTable_fromAlterIndex(signal, op_key, ret);
}

void
Dbdict::alterTable_toCopyData(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTableRecPtr alterTablePtr;
  getOpRec(op_ptr, alterTablePtr);
  const AlterTabReq* impl_req = &alterTablePtr.p->m_request;
  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, impl_req->tableId);
  ndbrequire(ok);

  CopyDataReq* req = (CopyDataReq*)signal->getDataPtrSend();

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = 0;
  req->requestType = 0;
  req->srcTableId = impl_req->tableId;
  req->dstTableId = impl_req->tableId;
  req->srcFragments = tablePtr.p->fragmentCount;

  sendSignal(reference(), GSN_COPY_DATA_REQ, signal,
             CopyDataReq::SignalLength, JBB);
}

void
Dbdict::alterTable_fromCopyData(Signal* signal, Uint32 op_key, Uint32 ret)
{
  alterTable_fromAlterIndex(signal, op_key, ret);
}

void
Dbdict::alterTable_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("alterTable_reply" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  if (!hasError(error)) {
    AlterTableConf* conf = (AlterTableConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = impl_req->tableId;
    conf->tableVersion = impl_req->tableVersion;
    conf->newTableVersion = impl_req->newTableVersion;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_TABLE_CONF, signal,
               AlterTableConf::SignalLength, JBB);
  } else {
    jam();
    AlterTableRef* ref = (AlterTableRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);
    ref->errorStatus = error.errorStatus;
    ref->errorKey = error.errorKey;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_TABLE_REF, signal,
               AlterTableRef::SignalLength, JBB);
  }
}

void
Dbdict::alterTable_toSumaSync(Signal* signal,
                              SchemaOpPtr op_ptr,
                              Uint32 step)
{
  jam();
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  AlterTableReq* req = (AlterTableReq*)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = 0;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->changeMask = 0;
  if (step == 0)
  {
    jam();
    AlterTableReq::setReorgSumaEnableFlag(req->changeMask, 1);
  }
  else if (step == 1)
  {
    AlterTableReq::setReorgSumaFilterFlag(req->changeMask, 1);
  }
  else
  {
    jamLine(step);
    ndbrequire(false);
  }
  sendSignal(reference(), GSN_ALTER_TABLE_REQ, signal,
             AlterTableReq::SignalLength, JBB);
}


// AlterTable: PREPARE

void
Dbdict::alterTable_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  D("alterTable_prepare" << *op_ptr.p);

  if (AlterTableReq::getReorgSubOp(impl_req->changeMask))
  {
    jam();

    /**
     * Get DIH connectPtr for future commit
     */
    {
      SchemaOpPtr tmp = op_ptr;
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      for (list.prev(tmp); !tmp.isNull(); list.prev(tmp))
      {
        jam();
        if (&tmp.p->m_oprec_ptr.p->m_opInfo== &Dbdict::AlterTableRec::g_opInfo)
        {
          jam();
          break;
        }
      }
      ndbrequire(!tmp.isNull());
      alterTabPtr.p->m_dihAddFragPtr =
        ((AlterTableRec*)tmp.p->m_oprec_ptr.p)->m_dihAddFragPtr;
      alterTabPtr.p->m_lqhFragPtr =
        ((AlterTableRec*)tmp.p->m_oprec_ptr.p)->m_lqhFragPtr;
    }

    sendTransConf(signal, op_ptr);
    return;
  }

  Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_define_backup_mutex);
  Callback c = {
    safe_cast(&Dbdict::alterTable_backup_mutex_locked),
    op_ptr.p->op_key
  };
  bool ok = mutex.lock(c);
  ndbrequire(ok);
  return;
}

void
Dbdict::alterTable_backup_mutex_locked(Signal* signal,
                                       Uint32 op_key,
                                       Uint32 ret)
{
  jamEntry();
  D("alterTable_backup_mutex_locked");

  ndbrequire(ret == 0);

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, impl_req->tableId);
  ndbrequire(ok);

  Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_define_backup_mutex);
  mutex.unlock(); // ignore response

  if (tablePtr.p->m_read_locked)
  {
    jam();
    setError(op_ptr, tablePtr.p->m_read_locked, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  /**
   * Write new table definition on prepare
   */
  Callback callback = {
    safe_cast(&Dbdict::alterTab_writeTableConf),
    callback.m_callbackData = op_ptr.p->op_key
  };

  const OpSection& tabInfoSec =
    getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);

  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);
  if (savetodisk) {
    writeTableFile(signal, op_ptr, impl_req->tableId, tabInfoSec, &callback);
  } else {
    execute(signal, callback, 0);
  }
}

void
Dbdict::alterTab_writeTableConf(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  ndbrequire(ret == 0);

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);

  alterTable_toLocal(signal, op_ptr);
}

void
Dbdict::alterTable_toLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  const Uint32 blockIndex = alterTabPtr.p->m_blockIndex;
  if (blockIndex == AlterTableRec::BlockCount)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  ndbrequire(blockIndex < AlterTableRec::BlockCount);
  const Uint32 blockNo = alterTabPtr.p->m_blockNo[blockIndex];

  D("alterTable_toLocal" << V(blockIndex) << V(getBlockName(blockNo)));

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTablePrepare;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->connectPtr = RNIL;
  req->noOfNewAttr = impl_req->noOfNewAttr;
  req->newNoOfCharsets = impl_req->newNoOfCharsets;
  req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

  Callback c = {
    safe_cast(&Dbdict::alterTable_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  if (ERROR_INSERTED(6131) &&
      blockIndex + 1 == AlterTableRec::BlockCount) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    AlterTabRef* ref = (AlterTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->op_key;
    ref->errorCode = 9131;
    sendSignal(reference(), GSN_ALTER_TAB_REF, signal,
               AlterTabRef::SignalLength, JBB);
    return;
  }

  BlockReference blockRef = numberToRef(blockNo, getOwnNodeId());

  if (blockNo == DBLQH && req->noOfNewAttr > 0)
  {
    jam();
    LinearSectionPtr ptr[3];
    Uint32 newAttrData[2 * MAX_ATTRIBUTES_IN_TABLE];
    ndbrequire(impl_req->noOfNewAttr <= MAX_ATTRIBUTES_IN_TABLE);
    ndbrequire(2 * impl_req->noOfNewAttr == alterTabPtr.p->m_newAttrData.getSize());
    LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
    bool ok = copyOut(op_sec_pool, alterTabPtr.p->m_newAttrData, newAttrData, 2 * impl_req->noOfNewAttr);
    ndbrequire(ok);

    ptr[0].p = newAttrData;
    ptr[0].sz = 2 * impl_req->noOfNewAttr;
    sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB, ptr, 1);
  }
  else if (blockNo == DBDIH && AlterTableReq::getAddFragFlag(req->changeMask))
  {
    jam();
    const OpSection& fragInfoSec =
      getOpSection(op_ptr, AlterTabReq::FRAGMENTATION);
    SegmentedSectionPtr fragInfoPtr;
    LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena,c_opSectionBufferPool);
    bool ok = copyOut(op_sec_pool, fragInfoSec, fragInfoPtr);
    ndbrequire(ok);

    if (AlterTableReq::getReorgFragFlag(req->changeMask))
    {
      jam();
      HashMapRecordPtr hm_ptr;
      TableRecordPtr newTablePtr;
      newTablePtr.i = alterTabPtr.p->m_newTablePtrI;
      c_tableRecordPool_.getPtr(newTablePtr);
      ndbrequire(find_object(hm_ptr,
                             newTablePtr.p->hashMapObjectId));
      req->new_map_ptr_i = hm_ptr.p->m_map_ptr_i;
    }

    SectionHandle handle(this, fragInfoPtr.i);
    sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB, &handle);
  }
  else
  {
    jam();
    sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB);
  }
}

void
Dbdict::alterTable_fromLocal(Signal* signal,
                             Uint32 op_key,
                             Uint32 ret)
{
  D("alterTable_fromLocal");

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Uint32& blockIndex = alterTabPtr.p->m_blockIndex; //ref
  ndbrequire(blockIndex < AlterTableRec::BlockCount);
  const Uint32 blockNo = alterTabPtr.p->m_blockNo[blockIndex];

  if (ret)
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  const AlterTabConf* conf = (const AlterTabConf*)signal->getDataPtr();

  // save TUP operation record for commit/abort
  switch(blockNo){
  case DBLQH:
    jam();
    alterTabPtr.p->m_lqhFragPtr = conf->connectPtr;
    break;
  case DBDIH:
    jam();
    alterTabPtr.p->m_dihAddFragPtr = conf->connectPtr;
    break;
  }

  blockIndex += 1;
  alterTable_toLocal(signal, op_ptr);
}

// AlterTable: COMMIT

void
Dbdict::alterTable_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  D("alterTable_commit" << *op_ptr.p);

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, impl_req->tableId);
  ndbrequire(ok);

  if (op_ptr.p->m_sections)
  {
    jam();
    // main-op
    ndbrequire(AlterTableReq::getReorgSubOp(impl_req->changeMask) == false);

    const OpSection& tabInfoSec =
      getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);
    const Uint32 size = tabInfoSec.getSize();

    // update table record
    tablePtr.p->packedSize = size;
    tablePtr.p->tableVersion = impl_req->newTableVersion;
    tablePtr.p->gciTableCreated = impl_req->gci;

    TableRecordPtr newTablePtr;
    newTablePtr.i = alterTabPtr.p->m_newTablePtrI;
    c_tableRecordPool_.getPtr(newTablePtr);

    const Uint32 changeMask = impl_req->changeMask;

    // perform DICT memory changes
    if (AlterTableReq::getNameFlag(changeMask))
    {
      jam();
      const Uint32 sz = MAX_TAB_NAME_SIZE;
      D("alter name:"
        << " old=" << copyRope<sz>(tablePtr.p->tableName)
        << " new=" << copyRope<sz>(newTablePtr.p->tableName));

      DictObjectPtr obj_ptr;
      c_obj_pool.getPtr(obj_ptr, tablePtr.p->m_obj_ptr_i);

      // remove old name from hash
      c_obj_name_hash.remove(obj_ptr);

      // save old name and replace it by new
      bool ok =
        copyRope<sz>(alterTabPtr.p->m_oldTableName, tablePtr.p->tableName) &&
        copyRope<sz>(tablePtr.p->tableName, newTablePtr.p->tableName);
      ndbrequire(ok);

      // add new name to object hash
      obj_ptr.p->m_name = tablePtr.p->tableName;
      c_obj_name_hash.add(obj_ptr);
    }

    if (AlterTableReq::getFrmFlag(changeMask))
    {
      jam();
      // save old frm and replace it by new
      const Uint32 sz = MAX_FRM_DATA_SIZE;
      bool ok =
        copyRope<sz>(alterTabPtr.p->m_oldFrmData, tablePtr.p->frmData) &&
        copyRope<sz>(tablePtr.p->frmData, newTablePtr.p->frmData);
      ndbrequire(ok);
    }

    if (AlterTableReq::getAddAttrFlag(changeMask))
    {
      jam();

      /* Move the column definitions to the real table definitions. */
      LocalAttributeRecord_list
        list(c_attributeRecordPool, tablePtr.p->m_attributes);
      LocalAttributeRecord_list
        newlist(c_attributeRecordPool, newTablePtr.p->m_attributes);

      const Uint32 noOfNewAttr = impl_req->noOfNewAttr;
      ndbrequire(noOfNewAttr > 0);
      Uint32 i;

      /* Move back to find the first column to move. */
      AttributeRecordPtr pPtr;
      ndbrequire(newlist.last(pPtr));
      for (i = 1; i < noOfNewAttr; i++) {
        jam();
        ndbrequire(newlist.prev(pPtr));
      }

      /* Move columns. */
      for (i = 0; i < noOfNewAttr; i++) {
        AttributeRecordPtr qPtr = pPtr;
        newlist.next(pPtr);
        newlist.remove(qPtr);
        list.addLast(qPtr);
      }
      tablePtr.p->noOfAttributes += noOfNewAttr;
    }

    if (AlterTableReq::getAddFragFlag(changeMask))
    {
      jam();
      Uint32 save = tablePtr.p->fragmentCount;
      tablePtr.p->fragmentCount = newTablePtr.p->fragmentCount;
      newTablePtr.p->fragmentCount = save;
    }
  }

  alterTabPtr.p->m_blockIndex = 0;
  alterTabPtr.p->m_blockNo[0] = DBLQH;
  alterTabPtr.p->m_blockNo[1] = DBDIH;
  alterTabPtr.p->m_blockNo[2] = DBSPJ;
  alterTabPtr.p->m_blockNo[3] = DBTC;

  if (AlterTableReq::getReorgFragFlag(impl_req->changeMask))
  {
    /**
     * DIH is next op
     */
    TableRecordPtr newTablePtr;
    newTablePtr.i = alterTabPtr.p->m_newTablePtrI;
    c_tableRecordPool_.getPtr(newTablePtr);
    tablePtr.p->hashMapObjectId = newTablePtr.p->hashMapObjectId;
    tablePtr.p->hashMapVersion = newTablePtr.p->hashMapVersion;
    alterTabPtr.p->m_blockNo[1] = RNIL;
  }
  else if (AlterTableReq::getReorgCommitFlag(impl_req->changeMask))
  {
    jam();
    /**
     * Reorg commit, only commit at DIH
     */
    alterTabPtr.p->m_blockNo[0] = RNIL;
    alterTabPtr.p->m_blockNo[2] = RNIL;
    alterTabPtr.p->m_blockNo[3] = RNIL;
  }
  else if (AlterTableReq::getReorgCompleteFlag(impl_req->changeMask) ||
           AlterTableReq::getReorgSumaEnableFlag(impl_req->changeMask) ||
           AlterTableReq::getReorgSumaFilterFlag(impl_req->changeMask))
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  alterTable_toCommitComplete(signal, op_ptr);
}

void
Dbdict::alterTable_toCommitComplete(Signal* signal,
                                    SchemaOpPtr op_ptr,
                                    Uint32 type)
{
  D("alterTable_toTupCommit");

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  if (type == ~Uint32(0))
  {
    jam();
    switch(op_ptr.p->m_state){
    case SchemaOp::OS_COMMITTING:
      jam();
      req->requestType = AlterTabReq::AlterTableCommit;
      break;
    case SchemaOp::OS_COMPLETING:
      jam();
      req->requestType = AlterTabReq::AlterTableComplete;
      break;
    default:
      jamLine(op_ptr.p->m_state);
      ndbrequire(false);
    }
  }
  else
  {
    jam();
    jamLine(type);
    req->requestType = type;
  }

  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->noOfNewAttr = impl_req->noOfNewAttr;
  req->newNoOfCharsets = impl_req->newNoOfCharsets;
  req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;
  req->connectPtr = RNIL;

  Uint32 blockIndex = alterTabPtr.p->m_blockIndex; //ref
  const Uint32 blockNo = alterTabPtr.p->m_blockNo[blockIndex];
  switch(blockNo){
  case DBDIH:
    jam();
    req->connectPtr = alterTabPtr.p->m_dihAddFragPtr;
    break;
  case DBLQH:
    req->connectPtr = alterTabPtr.p->m_lqhFragPtr;
    break;
  case RNIL:
    alterTable_fromCommitComplete(signal, op_ptr.p->op_key, 0);
    return;
  }

  Callback c = {
    safe_cast(&Dbdict::alterTable_fromCommitComplete),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  sendSignal(numberToRef(blockNo, getOwnNodeId()),
             GSN_ALTER_TAB_REQ, signal,
             AlterTabReq::SignalLength, JBB);
}

void
Dbdict::alterTable_fromCommitComplete(Signal* signal,
                                      Uint32 op_key,
                                      Uint32 ret)
{
  D("alterTable_fromCommit");

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  ndbrequire(ret == 0); // Failure during commit is not allowed
  if (++ alterTabPtr.p->m_blockIndex < AlterTableRec::BlockCount)
  {
    jam();
    alterTable_toCommitComplete(signal, op_ptr);
    return;
  }

  if (AlterTableReq::getReorgSubOp(impl_req->changeMask))
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  if (op_ptr.p->m_state == SchemaOp::OS_COMPLETING)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  const Uint32 tableId = impl_req->tableId;
  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, tableId);
  ndbrequire(ok);

  // inform Suma so it can send events to any subscribers of the table
  {
    AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();

    SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

    // special use of senderRef
    if (trans_ptr.p->m_isMaster)
      req->senderRef = trans_ptr.p->m_clientRef;
    else
      req->senderRef = 0;
    req->senderData = op_key;
    req->tableId = impl_req->tableId;
    req->tableVersion = impl_req->tableVersion;
    req->newTableVersion = impl_req->newTableVersion;
    req->gci = tablePtr.p->gciTableCreated;
    req->requestType = 0;
    req->changeMask = impl_req->changeMask;
    req->connectPtr = RNIL;
    req->noOfNewAttr = impl_req->noOfNewAttr;
    req->newNoOfCharsets = impl_req->newNoOfCharsets;
    req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

    const OpSection& tabInfoSec =
      getOpSection(op_ptr, AlterTabReq::DICT_TAB_INFO);
    SegmentedSectionPtr tabInfoPtr;
    LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena,c_opSectionBufferPool);
    bool ok = copyOut(op_sec_pool, tabInfoSec, tabInfoPtr);
    ndbrequire(ok);

    SectionHandle handle(this, tabInfoPtr.i);
    sendSignal(SUMA_REF, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB, &handle);
  }

  // older way to notify  wl3600_todo disable to find SUMA problems
  {
    ApiBroadcastRep* api= (ApiBroadcastRep*)signal->getDataPtrSend();
    api->gsn = GSN_ALTER_TABLE_REP;
    api->minVersion = MAKE_VERSION(4,1,15);

    AlterTableRep* rep = (AlterTableRep*)api->theData;
    rep->tableId = tablePtr.p->tableId;
    // wl3600_todo wants old version?
    rep->tableVersion = impl_req->tableVersion;
    rep->changeType = AlterTableRep::CT_ALTERED;

    char oldTableName[MAX_TAB_NAME_SIZE];
    memset(oldTableName, 0, sizeof(oldTableName));
    {
      const RopeHandle& rh =
        AlterTableReq::getNameFlag(impl_req->changeMask)
          ? alterTabPtr.p->m_oldTableName
          : tablePtr.p->tableName;
      ConstRope r(c_rope_pool, rh);
      r.copy(oldTableName);
    }

    LinearSectionPtr ptr[3];
    ptr[0].p = (Uint32*)oldTableName;
    ptr[0].sz = (sizeof(oldTableName) + 3) >> 2;

    sendSignal(QMGR_REF, GSN_API_BROADCAST_REP, signal,
	       ApiBroadcastRep::SignalLength + AlterTableRep::SignalLength,
	       JBB, ptr, 1);
  }

  {
    // Remark object as free
    SchemaFile::TableEntry *
      objEntry = getTableEntry(alterTabPtr.p->m_newTable_realObjectId);
    objEntry->m_tableType = DictTabInfo::UndefTableType;
    objEntry->m_tableState = SchemaFile::SF_UNUSED;
    objEntry->m_transId = 0;
  }

  releaseTableObject(alterTabPtr.p->m_newTablePtrI, false);
  sendTransConf(signal, op_ptr);
}

// AlterTable: COMPLETE

void
Dbdict::alterTable_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  alterTabPtr.p->m_blockIndex = 0;
  alterTabPtr.p->m_blockNo[0] = RNIL;
  alterTabPtr.p->m_blockNo[1] = RNIL;
  alterTabPtr.p->m_blockNo[2] = RNIL;
  alterTabPtr.p->m_blockNo[3] = RNIL;

  if (AlterTableReq::getReorgCommitFlag(impl_req->changeMask))
  {
    jam();
    alterTabPtr.p->m_blockNo[0] = DBDIH;
    alterTable_toCommitComplete(signal, op_ptr,
                                AlterTabReq::AlterTableWaitScan);
    return;
  }
  else if (AlterTableReq::getReorgCompleteFlag(impl_req->changeMask))
  {
    jam();

    /**
     * Reorg complete, LQH/DIH
     */
    alterTabPtr.p->m_blockNo[0] = DBDIH;
    alterTabPtr.p->m_blockNo[1] = DBLQH;
    alterTable_toCommitComplete(signal, op_ptr);
    return;
  }
  else if (AlterTableReq::getReorgSumaEnableFlag(impl_req->changeMask))
  {
    jam();
    alterTabPtr.p->m_blockNo[0] = DBLQH;
    alterTable_toCommitComplete(signal, op_ptr,
                                AlterTabReq::AlterTableSumaEnable);
    return;
  }
  else if (AlterTableReq::getReorgSumaFilterFlag(impl_req->changeMask))
  {
    jam();
    alterTabPtr.p->m_blockNo[0] = DBLQH;
    alterTable_toCommitComplete(signal, op_ptr,
                                AlterTabReq::AlterTableSumaFilter);
    return;
  }
  sendTransConf(signal, op_ptr);
}

// AlterTable: ABORT

void
Dbdict::alterTable_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTable_abortParse" << *op_ptr.p);

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  if (AlterTableReq::getReorgSubOp(impl_req->changeMask))
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  if (alterTabPtr.p->m_newTablePtrI != RNIL) {
    jam();
    // release the temporary work table

    {
      // Remark object as free
      SchemaFile::TableEntry *
        objEntry = getTableEntry(alterTabPtr.p->m_newTable_realObjectId);
      objEntry->m_tableType = DictTabInfo::UndefTableType;
      objEntry->m_tableState = SchemaFile::SF_UNUSED;
      objEntry->m_transId = 0;
    }

    releaseTableObject(alterTabPtr.p->m_newTablePtrI, false);
    alterTabPtr.p->m_newTablePtrI = RNIL;
  }

  sendTransConf(signal, op_ptr);
}

void
Dbdict::alterTable_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTable_abortPrepare" << *op_ptr.p);

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  if (AlterTableReq::getReorgSubOp(impl_req->changeMask))
  {
    jam();

    /**
     * Does nothing...
     */
    sendTransConf(signal, op_ptr);
    return;
  }

  if (alterTabPtr.p->m_blockIndex > 0)
  {
    jam();
    /*
     * Local blocks have only updated table version.
     * Reset it to original in reverse block order.
     */
    alterTable_abortToLocal(signal, op_ptr);
    return;
  }
  else
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }
}

void
Dbdict::alterTable_abortToLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  const Uint32 blockCount = alterTabPtr.p->m_blockIndex;
  ndbrequire(blockCount != 0 && blockCount <= AlterTableRec::BlockCount);
  const Uint32 blockIndex = blockCount - 1;
  const Uint32 blockNo = alterTabPtr.p->m_blockNo[blockIndex];

  D("alterTable_abortToLocal" << V(blockIndex) << V(getBlockName(blockNo)));

  Uint32 connectPtr = RNIL;
  switch(blockNo){
  case DBLQH:
    jam();
    connectPtr = alterTabPtr.p->m_lqhFragPtr;
    break;
  case DBDIH:
    jam();
    connectPtr = alterTabPtr.p->m_dihAddFragPtr;
    break;
  }

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTableRevert;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->connectPtr = connectPtr;
  req->noOfNewAttr = impl_req->noOfNewAttr;
  req->newNoOfCharsets = impl_req->newNoOfCharsets;
  req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

  Callback c = {
    safe_cast(&Dbdict::alterTable_abortFromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  BlockReference blockRef = numberToRef(blockNo, getOwnNodeId());
  sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
             AlterTabReq::SignalLength, JBB);
}

void
Dbdict::alterTable_abortFromLocal(Signal*signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  const Uint32 blockCount = alterTabPtr.p->m_blockIndex;
  ndbrequire(blockCount != 0 && blockCount <= AlterTableRec::BlockCount);
  const Uint32 blockIndex = blockCount - 1;
  alterTabPtr.p->m_blockIndex = blockIndex;

  ndbrequire(ret == 0); // abort is not allowed to fail

  if (blockIndex > 0)
  {
    jam();
    alterTable_abortToLocal(signal, op_ptr);
    return;
  }
  else
  {
    jam();
    sendTransConf(signal, op_ptr);
  }
}

// AlterTable: MISC

void
Dbdict::execALTER_TAB_CONF(Signal* signal)
{
  jamEntry();
  const AlterTabConf* conf = (const AlterTabConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execALTER_TAB_REF(Signal* signal)
{
  jamEntry();
  const AlterTabRef* ref = (const AlterTabRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

void
Dbdict::execALTER_TABLE_CONF(Signal* signal)
{
  jamEntry();
  const AlterTableConf* conf = (const AlterTableConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execALTER_TABLE_REF(Signal* signal)
{
  jamEntry();
  const AlterTableRef* ref = (const AlterTableRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

// AlterTable: END

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          EXTERNAL INTERFACE TO DATA -------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code that is used by other modules to.  */
/* access the data within DBDICT.                                   */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

void Dbdict::execGET_TABLEDID_REQ(Signal * signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 1);
  GetTableIdReq const * req = (GetTableIdReq *)signal->getDataPtr();
  Uint32 senderData = req->senderData;
  Uint32 senderRef = req->senderRef;
  Uint32 len = req->len;

  if(len>PATH_MAX)
  {
    jam();
    sendGET_TABLEID_REF((Signal*)signal,
			(GetTableIdReq *)req,
			GetTableIdRef::TableNameTooLong);
    return;
  }

  char tableName[PATH_MAX];
  SectionHandle handle(this, signal);
  SegmentedSectionPtr ssPtr;
  handle.getSection(ssPtr,GetTableIdReq::TABLE_NAME);
  copy((Uint32*)tableName, ssPtr);
  releaseSections(handle);

  DictObject * obj_ptr_p = get_object(tableName, len);
  if(obj_ptr_p == 0 || !DictTabInfo::isTable(obj_ptr_p->m_type)){
    jam();
    sendGET_TABLEID_REF(signal,
			(GetTableIdReq *)req,
			GetTableIdRef::TableNotDefined);
    return;
  }

  TableRecordPtr tablePtr;
  c_tableRecordPool_.getPtr(tablePtr, obj_ptr_p->m_object_ptr_i);

  GetTableIdConf * conf = (GetTableIdConf *)req;
  conf->tableId = tablePtr.p->tableId;
  conf->schemaVersion = tablePtr.p->tableVersion;
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_GET_TABLEID_CONF, signal,
	     GetTableIdConf::SignalLength, JBB);
}


void Dbdict::sendGET_TABLEID_REF(Signal* signal,
				 GetTableIdReq * req,
				 GetTableIdRef::ErrorCode errorCode)
{
  GetTableIdRef * const ref = (GetTableIdRef *)req;
  /**
   * The format of GetTabInfo Req/Ref is the same
   */
  BlockReference retRef = req->senderRef;
  ref->err = errorCode;
  sendSignal(retRef, GSN_GET_TABLEID_REF, signal,
	     GetTableIdRef::SignalLength, JBB);
}

/* ---------------------------------------------------------------- */
// Get a full table description.
/* ---------------------------------------------------------------- */
void Dbdict::execGET_TABINFOREQ(Signal* signal)
{
  jamEntry();
  if(!assembleFragments(signal))
  {
    return;
  }

  GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];
  SectionHandle handle(this, signal);

  /**
   * If I get a GET_TABINFO_REQ from myself
   * it's is a one from the time queue
   */
  bool fromTimeQueue = (signal->senderBlockRef() == reference());

  if (c_retrieveRecord.busyState && fromTimeQueue == true) {
    jam();

    sendSignalWithDelay(reference(), GSN_GET_TABINFOREQ, signal, 30,
			signal->length(),
			&handle);
    return;
  }//if

  const Uint32 MAX_WAITERS = (MAX_NDB_NODES*3)/2;

  // Test sending GET_TABINFOREF to DICT (Bug#14647210).
  const bool testRef = refToMain(signal->senderBlockRef()) == DBDICT &&
    ERROR_INSERTED_CLEAR(6026);
  
  if ((c_retrieveRecord.busyState || testRef) && fromTimeQueue == false)
  {
    jam();

    const Uint32 senderVersion = 
      getNodeInfo(refToNode(signal->senderBlockRef())).m_version;

    /**
     * DBDICT may possibly generate large numbers of signals if many nodes
     * are started at the same time, so we do not want to queue those using
     * sendSignalWithDelay(). See also Bug#14647210. Signals from other 
     * blocks we do queue localy, since these blocks may not retry on
     * GET_TABINFOREF with error==busy, and since they also should not 
     * generate large bursts of GET_TABINFOREQ.
     */
    if (c_retrieveRecord.noOfWaiters < MAX_WAITERS &&
        (refToMain(signal->senderBlockRef()) != DBDICT ||
         !ndbd_dict_get_tabinforef_implemented(senderVersion)))
    {
      jam();
      c_retrieveRecord.noOfWaiters++;

      sendSignalWithDelay(reference(), GSN_GET_TABINFOREQ, signal, 30,
			  signal->length(),
			  &handle);
      return;
    }

    if (!c_retrieveRecord.busyState)
    {
      ndbout << "Sending extra TABINFOREF to node"
             << refToNode(signal->senderBlockRef()) << endl;
    }
    releaseSections(handle);
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::Busy, __LINE__);
    return;
  }

  if(fromTimeQueue){
    jam();
    c_retrieveRecord.noOfWaiters--;
  }

  const bool useLongSig = (req->requestType & GetTabInfoReq::LongSignalConf);
  const bool byName = (req->requestType & GetTabInfoReq::RequestByName);
  const Uint32 transId = req->schemaTransId;

  Uint32 obj_id = RNIL;
  if (byName) {
    jam();
    ndbrequire(handle.m_cnt == 1);
    const Uint32 len = req->tableNameLen;

    if(len > PATH_MAX){
      jam();
      releaseSections(handle);
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNameTooLong, __LINE__);
      return;
    }

    Uint32 tableName[(PATH_MAX + 3) / 4];
    SegmentedSectionPtr ssPtr;
    handle.getSection(ssPtr,GetTabInfoReq::TABLE_NAME);
    copy(tableName, ssPtr);

    DictObject * old_ptr_p = get_object((char*)tableName, len);
    if(old_ptr_p)
      obj_id = old_ptr_p->m_id;
  } else {
    jam();
    obj_id = req->tableId;
  }
  releaseSections(handle);

  SchemaFile::TableEntry *objEntry = 0;
  if(obj_id != RNIL)
  {
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    objEntry = getTableEntry(xsf, obj_id);
  }

  // The table seached for was not found
  if(objEntry == 0)
  {
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }//if

  // If istable/index, allow ADD_STARTED (not to ref)

  D("execGET_TABINFOREQ" << V(transId) << " " << *objEntry);

  if (transId != 0 && transId == objEntry->m_transId)
  {
    jam();
    // see own trans always
  }
  else if (refToBlock(req->senderRef) != DBUTIL && /** XXX cheat */
           refToBlock(req->senderRef) != SUMA)
  {
    jam();
    Uint32 err;
    if ((err = check_read_obj(objEntry)))
    {
      jam();
      // cannot see another uncommitted trans
      sendGET_TABINFOREF(signal, req, (GetTabInfoRef::ErrorCode)err, __LINE__);
      return;
    }
  }

  c_retrieveRecord.busyState = true;
  c_retrieveRecord.blockRef = req->senderRef;
  c_retrieveRecord.m_senderData = req->senderData;
  c_retrieveRecord.tableId = obj_id;
  c_retrieveRecord.currentSent = 0;
  c_retrieveRecord.m_useLongSig = useLongSig;
  c_retrieveRecord.m_table_type = objEntry->m_tableType;
  c_retrieveRecord.schemaTransId = transId;
  c_retrieveRecord.requestType = req->requestType;
  c_packTable.m_state = PackTable::PTS_GET_TAB;

  Uint32 len = 4;
  if(objEntry->m_tableType == DictTabInfo::Datafile)
  {
    jam();

    if (objEntry->m_tableState != SchemaFile::SF_CREATE)
    {
      jam();

      GetTabInfoReq *req= (GetTabInfoReq*)signal->getDataPtrSend();
      req->senderData= c_retrieveRecord.retrievePage;
      req->senderRef= reference();
      req->requestType= GetTabInfoReq::RequestById;
      req->tableId= obj_id;
      req->schemaTransId = 0;

      sendSignal(TSMAN_REF, GSN_GET_TABINFOREQ, signal,
                 GetTabInfoReq::SignalLength, JBB);
      return;
    }
    else
    {
      jam();
      /**
       * Obj is being created, return 0 free extents
       */
      len = 5;
      signal->theData[4] = 0;
    }
  }
  else if(objEntry->m_tableType == DictTabInfo::LogfileGroup)
  {
    jam();
    if (objEntry->m_tableState != SchemaFile::SF_CREATE)
    {
      jam();
      GetTabInfoReq *req= (GetTabInfoReq*)signal->getDataPtrSend();
      req->senderData= c_retrieveRecord.retrievePage;
      req->senderRef= reference();
      req->requestType= GetTabInfoReq::RequestById;
      req->tableId= obj_id;
      req->schemaTransId = 0;

      sendSignal(LGMAN_REF, GSN_GET_TABINFOREQ, signal,
                 GetTabInfoReq::SignalLength, JBB);
      return;
    }
    else
    {
      jam();
      /**
       * Obj is being created, return 0 free space
       */
      len = 6;
      signal->theData[4] = 0;
      signal->theData[5] = 0;
    }
  }

  jam();
  signal->theData[0] = ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = obj_id;
  signal->theData[2] = objEntry->m_tableType;
  signal->theData[3] = c_retrieveRecord.retrievePage;
  sendSignal(reference(), GSN_CONTINUEB, signal, len, JBB);
}//execGET_TABINFOREQ()

void Dbdict::sendGetTabResponse(Signal* signal)
{
  PageRecordPtr pagePtr;
  DictTabInfo * const conf = (DictTabInfo *)&signal->theData[0];
  conf->senderRef   = reference();
  conf->senderData  = c_retrieveRecord.m_senderData;
  conf->requestType = DictTabInfo::GetTabInfoConf;
  conf->totalLen    = c_retrieveRecord.retrievedNoOfWords;

  c_pageRecordArray.getPtr(pagePtr, c_retrieveRecord.retrievePage);
  Uint32* pagePointer = (Uint32*)&pagePtr.p->word[0] + ZPAGE_HEADER_SIZE;

  if(c_retrieveRecord.m_useLongSig){
    jam();
    GetTabInfoConf* conf = (GetTabInfoConf*)signal->getDataPtr();
    conf->gci = 0;
    conf->tableId = c_retrieveRecord.tableId;
    conf->senderData = c_retrieveRecord.m_senderData;
    conf->totalLen = c_retrieveRecord.retrievedNoOfWords;
    conf->tableType = c_retrieveRecord.m_table_type;

    Callback c = { safe_cast(&Dbdict::initRetrieveRecord), 0 };
    LinearSectionPtr ptr[3];
    ptr[0].p = pagePointer;
    ptr[0].sz = c_retrieveRecord.retrievedNoOfWords;
    sendFragmentedSignal(c_retrieveRecord.blockRef,
			 GSN_GET_TABINFO_CONF,
			 signal,
			 GetTabInfoConf::SignalLength,
			 JBB,
			 ptr,
			 1,
			 c);
    return;
  }

  ndbrequire(false);
}//sendGetTabResponse()

void Dbdict::sendGET_TABINFOREF(Signal* signal,
				GetTabInfoReq * req,
				GetTabInfoRef::ErrorCode errorCode,
                                Uint32 errorLine)
{
  jamEntry();
  const GetTabInfoReq req_copy = *req;
  GetTabInfoRef * const ref = (GetTabInfoRef *)&signal->theData[0];

  ref->senderData = req_copy.senderData;
  ref->senderRef = reference();
  ref->requestType = req_copy.requestType;
  ref->tableId = req_copy.tableId;
  ref->schemaTransId = req_copy.schemaTransId;
  ref->errorCode = (Uint32)errorCode;
  ref->errorLine = errorLine;

  BlockReference retRef = req_copy.senderRef;
  sendSignal(retRef, GSN_GET_TABINFOREF, signal,
             GetTabInfoRef::SignalLength, JBB);
}

void
Dbdict::execLIST_TABLES_REQ(Signal* signal)
{
  jamEntry();
  ListTablesReq * req = (ListTablesReq*)signal->getDataPtr();

  Uint32 senderRef  = req->senderRef;
  Uint32 receiverVersion = getNodeInfo(refToNode(senderRef)).m_version;

  if (ndbd_LIST_TABLES_CONF_long_signal(receiverVersion))
    sendLIST_TABLES_CONF(signal, req);
  else
    sendOLD_LIST_TABLES_CONF(signal, req);
}

void Dbdict::sendOLD_LIST_TABLES_CONF(Signal* signal, ListTablesReq* req)
{
  Uint32 senderRef  = req->senderRef;
  Uint32 senderData = req->senderData;
  // save req flags
  const Uint32 reqTableId = req->oldGetTableId();
  const Uint32 reqTableType = req->oldGetTableType();
  const bool reqListNames = req->getListNames();
  const bool reqListIndexes = req->getListIndexes();
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  // init the confs
  OldListTablesConf * conf = (OldListTablesConf *)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->counter = 0;
  Uint32 pos = 0;

  DictObjectName_hash::Iterator iter;
  bool ok = c_obj_name_hash.first(iter);
  for (; ok; ok = c_obj_name_hash.next(iter)){
    Uint32 type = iter.curr.p->m_type;
    if ((reqTableType != (Uint32)0) && (reqTableType != type))
      continue;

    if (reqListIndexes && !DictTabInfo::isIndex(type))
      continue;

    TableRecordPtr tablePtr;
    if (DictTabInfo::isTable(type) || DictTabInfo::isIndex(type)){
      c_tableRecordPool_.getPtr(tablePtr, iter.curr.p->m_object_ptr_i);

      if(reqListIndexes && (reqTableId != tablePtr.p->primaryTableId))
	continue;

      conf->tableData[pos] = 0;
      conf->setTableId(pos, iter.curr.p->m_id); // id
      conf->setTableType(pos, type); // type
      // state

      if(DictTabInfo::isTable(type))
      {
        SchemaFile::TableEntry * te = getTableEntry(xsf, iter.curr.p->m_id);
        switch(te->m_tableState){
        case SchemaFile::SF_CREATE:
          jam();
          conf->setTableState(pos, DictTabInfo::StateBuilding);
          break;
        case SchemaFile::SF_ALTER:
          jam();
          conf->setTableState(pos, DictTabInfo::StateOnline);
          break;
        case SchemaFile::SF_DROP:
          jam();
	  conf->setTableState(pos, DictTabInfo::StateDropping);
          break;
        case SchemaFile::SF_IN_USE:
        {
          if (tablePtr.p->m_read_locked)
          {
            jam();
            conf->setTableState(pos, DictTabInfo::StateBackup);
          }
          else
          {
            jam();
            conf->setTableState(pos, DictTabInfo::StateOnline);
          }
	  break;
        }
	default:
	  conf->setTableState(pos, DictTabInfo::StateBroken);
	  break;
	}
      }
      if (tablePtr.p->isIndex()) {
	switch (tablePtr.p->indexState) {
	case TableRecord::IS_OFFLINE:
	  conf->setTableState(pos, DictTabInfo::StateOffline);
	  break;
	case TableRecord::IS_BUILDING:
	  conf->setTableState(pos, DictTabInfo::StateBuilding);
	  break;
	case TableRecord::IS_DROPPING:
	  conf->setTableState(pos, DictTabInfo::StateDropping);
	  break;
	case TableRecord::IS_ONLINE:
	  conf->setTableState(pos, DictTabInfo::StateOnline);
	  break;
	default:
	  conf->setTableState(pos, DictTabInfo::StateBroken);
	  break;
	}
      }
      // Logging status
      if (! (tablePtr.p->m_bits & TableRecord::TR_Logged)) {
	conf->setTableStore(pos, DictTabInfo::StoreNotLogged);
      } else {
	conf->setTableStore(pos, DictTabInfo::StorePermanent);
      }
      // Temporary status
      if (tablePtr.p->m_bits & TableRecord::TR_Temporary) {
	conf->setTableTemp(pos, NDB_TEMP_TAB_TEMPORARY);
      } else {
	conf->setTableTemp(pos, NDB_TEMP_TAB_PERMANENT);
      }
      pos++;
    }
    if(DictTabInfo::isTrigger(type)){
      TriggerRecordPtr triggerPtr;
      bool ok = find_object(triggerPtr, iter.curr.p->m_id);
      conf->tableData[pos] = 0;
      conf->setTableId(pos, iter.curr.p->m_id);
      conf->setTableType(pos, type);
      if (!ok)
      {
        conf->setTableState(pos, DictTabInfo::StateBroken);
      }
      else
      {
        switch (triggerPtr.p->triggerState) {
        case TriggerRecord::TS_DEFINING:
          conf->setTableState(pos, DictTabInfo::StateBuilding);
          break;
        case TriggerRecord::TS_OFFLINE:
          conf->setTableState(pos, DictTabInfo::StateOffline);
          break;
        case TriggerRecord::TS_ONLINE:
          conf->setTableState(pos, DictTabInfo::StateOnline);
          break;
        default:
          conf->setTableState(pos, DictTabInfo::StateBroken);
          break;
        }
      }
      conf->setTableStore(pos, DictTabInfo::StoreNotLogged);
      pos++;
    }
    if (DictTabInfo::isFilegroup(type)){
      jam();
      conf->tableData[pos] = 0;
      conf->setTableId(pos, iter.curr.p->m_id);
      conf->setTableType(pos, type); // type
      conf->setTableState(pos, DictTabInfo::StateOnline);  // XXX todo
      pos++;
    }
    if (DictTabInfo::isFile(type)){
      jam();
      conf->tableData[pos] = 0;
      conf->setTableId(pos, iter.curr.p->m_id);
      conf->setTableType(pos, type); // type
      conf->setTableState(pos, DictTabInfo::StateOnline); // XXX todo
      pos++;
    }

    if (pos >= OldListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 OldListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }

    if (! reqListNames)
      continue;

    LocalRope name(c_rope_pool, iter.curr.p->m_name);
    const Uint32 size = name.size();
    conf->tableData[pos] = size;
    pos++;
    if (pos >= OldListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 OldListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    Uint32 i = 0;
    char tmp[PATH_MAX];
    name.copy(tmp);
    while (i < size) {
      char* p = (char*)&conf->tableData[pos];
      for (Uint32 j = 0; j < 4; j++) {
	if (i < size)
	  *p++ = tmp[i++];
	else
	  *p++ = 0;
      }
      pos++;
      if (pos >= OldListTablesConf::DataLength) {
	sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		   OldListTablesConf::SignalLength, JBB);
	conf->counter++;
	pos = 0;
      }
    }
  }
  // last signal must have less than max length
  sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
	     OldListTablesConf::HeaderLength + pos, JBB);
}

void Dbdict::sendLIST_TABLES_CONF(Signal* signal, ListTablesReq* req)
{
  Uint32 senderRef  = req->senderRef;
  Uint32 senderData = req->senderData;
  // save req flags
  const Uint32 reqTableId = req->getTableId();
  const Uint32 reqTableType = req->getTableType();
  const bool reqListNames = req->getListNames();
  const bool reqListIndexes = req->getListIndexes();
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  NodeReceiverGroup rg(senderRef);

  DictObjectName_hash::Iterator iter;
  bool done = !c_obj_name_hash.first(iter);

  if (done)
  {
    /*
     * Empty hashtable, send empty signal
     */
    jam();
    ListTablesConf * const conf = (ListTablesConf*)signal->getDataPtrSend();
    conf->senderData = senderData;
    conf->noOfTables = 0;
    sendSignal(rg, GSN_LIST_TABLES_CONF, signal,
               ListTablesConf::SignalLength, JBB);
    return;
  }

  /*
    Pack table data and table names (if requested) in
    two signal segments and send it in one long fragmented
    signal
   */
  ListTablesData ltd;
  const Uint32 listTablesDataSizeInWords = (sizeof(ListTablesData) + 3) / 4;
  char tname[PATH_MAX];
  SimplePropertiesSectionWriter tableDataWriter(* this);
  SimplePropertiesSectionWriter tableNamesWriter(* this);

  Uint32 count = 0;
  Uint32 fragId = rand();
  Uint32 fragInfo = 0;
  const Uint32 fragSize = 240;

  tableDataWriter.first();
  tableNamesWriter.first();
  while(true)
  {
    Uint32 type = iter.curr.p->m_type;

    if ((reqTableType != (Uint32)0) && (reqTableType != type))
      goto flush;

    if (reqListIndexes && !DictTabInfo::isIndex(type))
      goto flush;

    TableRecordPtr tablePtr;
    if (DictTabInfo::isTable(type) || DictTabInfo::isIndex(type)){
      c_tableRecordPool_.getPtr(tablePtr, iter.curr.p->m_object_ptr_i);

      if(reqListIndexes && (reqTableId != tablePtr.p->primaryTableId))
	goto flush;

      ltd.requestData = 0; // clear
      ltd.setTableId(iter.curr.p->m_id); // id
      ltd.setTableType(type); // type
      // state

      if(DictTabInfo::isTable(type)){
        SchemaFile::TableEntry * te = getTableEntry(xsf, iter.curr.p->m_id);
        switch(te->m_tableState){
        case SchemaFile::SF_CREATE:
          jam();
          ltd.setTableState(DictTabInfo::StateBuilding);
          break;
        case SchemaFile::SF_ALTER:
          jam();
          ltd.setTableState(DictTabInfo::StateOnline);
          break;
        case SchemaFile::SF_DROP:
          jam();
	  ltd.setTableState(DictTabInfo::StateDropping);
          break;
        case SchemaFile::SF_IN_USE:
        {
          if (tablePtr.p->m_read_locked)
          {
            jam();
            ltd.setTableState(DictTabInfo::StateBackup);
          }
          else
          {
            jam();
            ltd.setTableState(DictTabInfo::StateOnline);
          }
	  break;
        }
	default:
	  ltd.setTableState(DictTabInfo::StateBroken);
	  break;
	}
      }
      if (tablePtr.p->isIndex()) {
	switch (tablePtr.p->indexState) {
	case TableRecord::IS_OFFLINE:
	  ltd.setTableState(DictTabInfo::StateOffline);
	  break;
	case TableRecord::IS_BUILDING:
	  ltd.setTableState(DictTabInfo::StateBuilding);
	  break;
	case TableRecord::IS_DROPPING:
	  ltd.setTableState(DictTabInfo::StateDropping);
	  break;
	case TableRecord::IS_ONLINE:
	  ltd.setTableState(DictTabInfo::StateOnline);
	  break;
	default:
	  ltd.setTableState(DictTabInfo::StateBroken);
	  break;
	}
      }
      // Logging status
      if (! (tablePtr.p->m_bits & TableRecord::TR_Logged)) {
	ltd.setTableStore(DictTabInfo::StoreNotLogged);
      } else {
	ltd.setTableStore(DictTabInfo::StorePermanent);
      }
      // Temporary status
      if (tablePtr.p->m_bits & TableRecord::TR_Temporary) {
	ltd.setTableTemp(NDB_TEMP_TAB_TEMPORARY);
      } else {
	ltd.setTableTemp(NDB_TEMP_TAB_PERMANENT);
      }
    }
    if(DictTabInfo::isTrigger(type)){
      TriggerRecordPtr triggerPtr;
      bool ok = find_object(triggerPtr, iter.curr.p->m_id);

      ltd.requestData = 0;
      ltd.setTableId(iter.curr.p->m_id);
      ltd.setTableType(type);
      if (!ok)
      {
        ltd.setTableState(DictTabInfo::StateBroken);
      }
      else
      {
        switch (triggerPtr.p->triggerState) {
        case TriggerRecord::TS_DEFINING:
          ltd.setTableState(DictTabInfo::StateBuilding);
          break;
        case TriggerRecord::TS_OFFLINE:
          ltd.setTableState(DictTabInfo::StateOffline);
          break;
        case TriggerRecord::TS_ONLINE:
          ltd.setTableState(DictTabInfo::StateOnline);
          break;
        default:
          ltd.setTableState(DictTabInfo::StateBroken);
          break;
        }
      }
      ltd.setTableStore(DictTabInfo::StoreNotLogged);
    }
    if (DictTabInfo::isFilegroup(type)){
      jam();
      ltd.requestData = 0;
      ltd.setTableId(iter.curr.p->m_id);
      ltd.setTableType(type); // type
      ltd.setTableState(DictTabInfo::StateOnline);  // XXX todo
    }
    if (DictTabInfo::isFile(type)){
      jam();
      ltd.requestData = 0;
      ltd.setTableId(iter.curr.p->m_id);
      ltd.setTableType(type); // type
      ltd.setTableState(DictTabInfo::StateOnline); // XXX todo
    }
    if (DictTabInfo::isHashMap(type))
    {
      jam();
      ltd.setTableId(iter.curr.p->m_id);
      ltd.setTableType(type); // type
      ltd.setTableState(DictTabInfo::StateOnline); // XXX todo
    }
    tableDataWriter.putWords((Uint32 *) &ltd, listTablesDataSizeInWords);
    count++;

    if (reqListNames)
    {
      jam();
      LocalRope name(c_rope_pool, iter.curr.p->m_name);
      const Uint32 size = name.size(); // String length including \0
      const Uint32 wsize = (size + 3) / 4;
      tableNamesWriter.putWord(size);
      name.copy(tname);
      tableNamesWriter.putWords((Uint32 *) tname, wsize);
    }

flush:
    Uint32 tableDataWords = tableDataWriter.getWordsUsed();
    Uint32 tableNameWords = tableNamesWriter.getWordsUsed();

    done = !c_obj_name_hash.next(iter);
    if ((tableDataWords + tableNameWords) > fragSize || done)
    {
      jam();

      /*
       * Flush signal fragment to keep memory usage down
       */
      Uint32 sigLen = ListTablesConf::SignalLength;
      Uint32 secs = 0;
      if (tableDataWords != 0)
      {
        jam();
        secs++;
      }
      if (tableNameWords != 0)
      {
        jam();
        secs++;
      }
      Uint32 * secNos = &signal->theData[sigLen];
      signal->theData[sigLen + secs] = fragId;
      SectionHandle handle(this);
      switch (secs) {
      case(0):
        jam();
        sigLen++; // + fragId;
        handle.m_cnt = 0;
        break;
      case(1):
      {
        jam();
        SegmentedSectionPtr segSecPtr;
        sigLen += 2; // 1 sections + fragid
        if (tableNameWords == 0)
        {
          tableDataWriter.getPtr(segSecPtr);
          secNos[0] = ListTablesConf::TABLE_DATA;;
        }
        else
        {
          tableNamesWriter.getPtr(segSecPtr);
          secNos[0] = ListTablesConf::TABLE_NAMES;
        }
        handle.m_ptr[0] = segSecPtr;
        handle.m_cnt = 1;
        break;
      }
      case(2):
      {
        jam();
        sigLen += 3; // 2 sections + fragid
        SegmentedSectionPtr tableDataPtr;
        tableDataWriter.getPtr(tableDataPtr);
        SegmentedSectionPtr tableNamesPtr;
        tableNamesWriter.getPtr(tableNamesPtr);
        handle.m_ptr[0] = tableDataPtr;
        handle.m_ptr[1] = tableNamesPtr;
        handle.m_cnt = 2;
        secNos[0] = ListTablesConf::TABLE_DATA;
        secNos[1] = ListTablesConf::TABLE_NAMES;
        break;
      }
      }

      if (done)
      {
        jam();
        if (fragInfo)
        {
          jam();
          fragInfo = 3;
        }
      }
      else
      {
        jam();
        if (fragInfo == 0)
        {
          jam();
          fragInfo = 1;
        }
        else
        {
          jam();
          fragInfo = 2;
        }
      }
      signal->header.m_fragmentInfo = fragInfo;

      ListTablesConf * const conf = (ListTablesConf*)signal->getDataPtrSend();
      conf->senderData = senderData;
      conf->noOfTables = count;
      sendSignal(rg, GSN_LIST_TABLES_CONF, signal,
                 sigLen, JBB, &handle);

      signal->header.m_noOfSections = 0;
      signal->header.m_fragmentInfo = 0;

      if (done)
      {
        jam();
        return;
      }

      /**
       * Reset counter for next signal
       * Reset buffers
       */
      count = 0;
      tableDataWriter.first();
      tableNamesWriter.first();
    }
  }
}

// MODULE: CreateIndex

const Dbdict::OpInfo
Dbdict::CreateIndexRec::g_opInfo = {
  { 'C', 'I', 'n', 0 },
  ~RT_DBDICT_CREATE_INDEX,
  GSN_CREATE_INDX_IMPL_REQ,
  CreateIndxImplReq::SignalLength,
  //
  &Dbdict::createIndex_seize,
  &Dbdict::createIndex_release,
  //
  &Dbdict::createIndex_parse,
  &Dbdict::createIndex_subOps,
  &Dbdict::createIndex_reply,
  //
  &Dbdict::createIndex_prepare,
  &Dbdict::createIndex_commit,
  &Dbdict::createIndex_complete,
  //
  &Dbdict::createIndex_abortParse,
  &Dbdict::createIndex_abortPrepare
};

bool
Dbdict::createIndex_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateIndexRec>(op_ptr);
}

void
Dbdict::createIndex_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateIndexRec>(op_ptr);
}

void
Dbdict::execCREATE_INDX_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  if (check_sender_version(signal, MAKE_VERSION(6,4,0)) < 0)
  {
    jam();
    /**
     * Pekka has for some obscure reason switched places of
     *   senderRef/senderData
     */
    CreateIndxReq * tmp = (CreateIndxReq*)signal->getDataPtr();
    do_swap(tmp->clientRef, tmp->clientData);
  }

  const CreateIndxReq req_copy =
    *(const CreateIndxReq*)signal->getDataPtr();
  const CreateIndxReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateIndexRecPtr createIndexPtr;
    CreateIndxImplReq* impl_req;

    startClientReq(op_ptr, createIndexPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;
    impl_req->indexType = req->indexType;
    // these are set in sub-operation create table
    impl_req->indexId = RNIL;
    impl_req->indexVersion = 0;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateIndxRef* ref = (CreateIndxRef*)signal->getDataPtrSend();

  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_CREATE_INDX_REF, signal,
             CreateIndxRef::SignalLength, JBB);
}

// CreateIndex: PARSE

void
Dbdict::createIndex_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  D("createIndex_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);
  CreateIndxImplReq* impl_req = &createIndexPtr.p->m_request;

  // save attribute list
  IndexAttributeList& attrList = createIndexPtr.p->m_attrList;
  {
    SegmentedSectionPtr ss_ptr;
    handle.getSection(ss_ptr, CreateIndxReq::ATTRIBUTE_LIST_SECTION);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
    r.reset(); // undo implicit first()
    if (!r.getWord(&attrList.sz) ||
        r.getSize() != 1 + attrList.sz ||
        attrList.sz == 0 ||
        attrList.sz > MAX_ATTRIBUTES_IN_INDEX ||
        !r.getWords(attrList.id, attrList.sz)) {
      jam();
      setError(error, CreateIndxRef::InvalidIndexType, __LINE__);
      return;
    }
  }

  // save name and some index table properties
  Uint32& bits = createIndexPtr.p->m_bits;
  bits = TableRecord::TR_RowChecksum;
  {
    SegmentedSectionPtr ss_ptr;
    handle.getSection(ss_ptr, CreateIndxReq::INDEX_NAME_SECTION);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
    DictTabInfo::Table tableDesc;
    tableDesc.init();
    SimpleProperties::UnpackStatus status =
      SimpleProperties::unpack(
          r, &tableDesc,
          DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
          true, true);
    if (status != SimpleProperties::Eof ||
        tableDesc.TableName[0] == 0)
    {
      ndbassert(false);
      setError(error, CreateIndxRef::InvalidName, __LINE__);
      return;
    }
    const Uint32 bytesize = sizeof(createIndexPtr.p->m_indexName);
    memcpy(createIndexPtr.p->m_indexName, tableDesc.TableName, bytesize);
    if (tableDesc.TableLoggedFlag) {
      jam();
      bits |= TableRecord::TR_Logged;
    }
    if (tableDesc.TableTemporaryFlag) {
      jam();
      bits |= TableRecord::TR_Temporary;
    }
    D("index " << createIndexPtr.p->m_indexName);
  }

  // check primary table
  TableRecordPtr tablePtr;
  {
    if (!(impl_req->tableId < c_noOfMetaTables)) {
      jam();
      setError(error, CreateIndxRef::InvalidPrimaryTable, __LINE__);
      return;
    }
    bool ok = find_object(tablePtr, impl_req->tableId);
    if (!ok || !tablePtr.p->isTable()) {

      jam();
      setError(error, CreateIndxRef::InvalidPrimaryTable, __LINE__);
      return;
    }

    Uint32 err;
    if ((err = check_read_obj(impl_req->tableId, trans_ptr.p->m_transId)))
    {
      jam();
      setError(error, err, __LINE__);
      return;
    }
  }

  // check index type and set more properties
  {
    switch (impl_req->indexType) {
    case DictTabInfo::UniqueHashIndex:
      jam();
      createIndexPtr.p->m_fragmentType = DictTabInfo::HashMapPartition;
      break;
    case DictTabInfo::OrderedIndex:
      jam();
      if (bits & TableRecord::TR_Logged) {
        jam();
        setError(error, CreateIndxRef::InvalidIndexType, __LINE__);
        return;
      }
      createIndexPtr.p->m_fragmentType = DictTabInfo::DistrKeyOrderedIndex;
      // hash case is computed later from attribute list
      createIndexPtr.p->m_indexKeyLength = MAX_TTREE_NODE_SIZE;
      break;
    default:
      setError(error, CreateIndxRef::InvalidIndexType, __LINE__);
      return;
    }
  }

  // check that the temporary status of index is compatible with table
  {
    if (!(bits & TableRecord::TR_Temporary) &&
        (tablePtr.p->m_bits & TableRecord::TR_Temporary))
    {
      jam();
      setError(error, CreateIndxRef::TableIsTemporary, __LINE__);
      return;
    }
    if ((bits & TableRecord::TR_Temporary) &&
        !(tablePtr.p->m_bits & TableRecord::TR_Temporary))
    {
      // This could be implemented later, but mysqld does currently not detect
      // that the index disappears after SR, and it appears not too useful.
      jam();
      setError(error, CreateIndxRef::TableIsNotTemporary, __LINE__);
      return;
    }
    if ((bits & TableRecord::TR_Temporary) &&
        (bits & TableRecord::TR_Logged))
    {
      jam();
      setError(error, CreateIndxRef::NoLoggingTemporaryIndex, __LINE__);
      return;
    }
  }

  // check attribute list
  AttributeMask& attrMask = createIndexPtr.p->m_attrMask;
  AttributeMap& attrMap = createIndexPtr.p->m_attrMap;
  {
    Uint32 k;
    for (k = 0; k < attrList.sz; k++) {
      jam();
      Uint32 current_id = attrList.id[k];
      AttributeRecordPtr attrPtr;

      // find the attribute
      {
        LocalAttributeRecord_list
          list(c_attributeRecordPool, tablePtr.p->m_attributes);
        list.first(attrPtr);
        while (!attrPtr.isNull()) {
          jam();
          if (attrPtr.p->attributeId == current_id) {
            jam();
            break;
          }
          list.next(attrPtr);
        }
      }
      if (attrPtr.isNull()) {
        jam();
	ndbassert(false);
        setError(error, CreateIndxRef::BadRequestType, __LINE__);
        return;
      }

      // add attribute to mask
      if (attrMask.get(current_id)) {
        jam();
        setError(error, CreateIndxRef::DuplicateAttributes, __LINE__);
        return;
      }
      attrMask.set(current_id);

      // check disk based
      const Uint32 attrDesc = attrPtr.p->attributeDescriptor;
      if (AttributeDescriptor::getDiskBased(attrDesc)) {
        jam();
        setError(error, CreateIndxRef::IndexOnDiskAttributeError, __LINE__);
        return;
      }

      // re-order hash index by attribute id
      Uint32 new_index = k;
      if (impl_req->indexType == DictTabInfo::UniqueHashIndex) {
        jam();
        // map contains (old_index,attr_id) ordered by attr_id
        while (new_index > 0) {
          jam();
          if (current_id >= attrMap[new_index - 1].attr_id) {
            jam();
            break;
          }
          attrMap[new_index] = attrMap[new_index - 1];
          new_index--;
        }
      }
      attrMap[new_index].old_index = k;
      attrMap[new_index].attr_id = current_id;
      attrMap[new_index].attr_ptr_i = attrPtr.i;

      // add key size for hash index (ordered case was handled before)
      if (impl_req->indexType == DictTabInfo::UniqueHashIndex) {
        jam();
        const Uint32 s1 = AttributeDescriptor::getSize(attrDesc);
        const Uint32 s2 = AttributeDescriptor::getArraySize(attrDesc);
        createIndexPtr.p->m_indexKeyLength += ((1 << s1) * s2 + 31) >> 5;
      }
    }
  }

  if (master)
  {
    jam();
    impl_req->indexId = getFreeObjId();
  }

  if (impl_req->indexId == RNIL)
  {
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__);
    return;
  }

  if (impl_req->indexId >= c_noOfMetaTables)
  {
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__);
    return;
  }

  if (ERROR_INSERTED(6122)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9122, __LINE__);
    return;
  }
}

bool
Dbdict::createIndex_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createIndex_subOps" << V(op_ptr.i) << *op_ptr.p);

  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);

  // op to create index table
  if (!createIndexPtr.p->m_sub_create_table) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::createIndex_fromCreateTable),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    createIndex_toCreateTable(signal, op_ptr);
    return true;
  }
  // op to alter index online
  if (!createIndexPtr.p->m_sub_alter_index) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::createIndex_fromAlterIndex),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    createIndex_toAlterIndex(signal, op_ptr);
    return true;
  }
  return false;
}

void
Dbdict::createIndex_toCreateTable(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createIndex_toCreateTable");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, createIndexPtr.p->m_request.tableId);
  ndbrequire(ok);

  // signal data writer
  Uint32* wbuffer = &c_indexPage.word[0];
  LinearWriter w(wbuffer, sizeof(c_indexPage) >> 2);
  w.first();

  //indexPtr.p->noOfPrimkey = indexPtr.p->noOfAttributes;
  // plus concatenated primary table key attribute
  //indexPtr.p->noOfAttributes += 1;
  //indexPtr.p->noOfNullAttr = 0;

  w.add(DictTabInfo::TableId, createIndexPtr.p->m_request.indexId);
  w.add(DictTabInfo::TableName, createIndexPtr.p->m_indexName);
  { bool flag = createIndexPtr.p->m_bits & TableRecord::TR_Logged;
    w.add(DictTabInfo::TableLoggedFlag, (Uint32)flag);
  }
  { bool flag = createIndexPtr.p->m_bits & TableRecord::TR_Temporary;
    w.add(DictTabInfo::TableTemporaryFlag, (Uint32)flag);
  }
  w.add(DictTabInfo::FragmentTypeVal, createIndexPtr.p->m_fragmentType);
  w.add(DictTabInfo::TableTypeVal, createIndexPtr.p->m_request.indexType);
  { LocalRope name(c_rope_pool, tablePtr.p->tableName);
    char tableName[MAX_TAB_NAME_SIZE];
    name.copy(tableName);
    w.add(DictTabInfo::PrimaryTable, tableName);
  }
  w.add(DictTabInfo::PrimaryTableId, tablePtr.p->tableId);
  w.add(DictTabInfo::NoOfAttributes, createIndexPtr.p->m_attrList.sz + 1);
  w.add(DictTabInfo::NoOfKeyAttr, createIndexPtr.p->m_attrList.sz);//XXX ordered??
  w.add(DictTabInfo::NoOfNullable, (Uint32)0);
  w.add(DictTabInfo::KeyLength, createIndexPtr.p->m_indexKeyLength);
  w.add(DictTabInfo::SingleUserMode, (Uint32)NDB_SUM_READ_WRITE);

  // write index key attributes

  //const IndexAttributeList& attrList = createIndexPtr.p->m_attrList;
  const AttributeMap& attrMap = createIndexPtr.p->m_attrMap;
  Uint32 k;
  for (k = 0; k < createIndexPtr.p->m_attrList.sz; k++) {
    jam();
    // insert the attributes in the order decided before in attrMap
    // TODO: make sure "old_index" is stored with the table and
    // passed up to NdbDictionary
    AttributeRecordPtr attrPtr;
    c_attributeRecordPool.getPtr(attrPtr, attrMap[k].attr_ptr_i);

    { LocalRope attrName(c_rope_pool, attrPtr.p->attributeName);
      char attributeName[MAX_ATTR_NAME_SIZE];
      attrName.copy(attributeName);
      w.add(DictTabInfo::AttributeName, attributeName);
    }

    const Uint32 attrDesc = attrPtr.p->attributeDescriptor;
    const bool isNullable = AttributeDescriptor::getNullable(attrDesc);
    const Uint32 arrayType = AttributeDescriptor::getArrayType(attrDesc);
    const Uint32 attrType = AttributeDescriptor::getType(attrDesc);

    w.add(DictTabInfo::AttributeId, k);
    if (createIndexPtr.p->m_request.indexType == DictTabInfo::UniqueHashIndex) {
      jam();
      w.add(DictTabInfo::AttributeKeyFlag, (Uint32)true);
      w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    }
    if (createIndexPtr.p->m_request.indexType == DictTabInfo::OrderedIndex) {
      jam();
      w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
      w.add(DictTabInfo::AttributeNullableFlag, (Uint32)isNullable);
    }
    w.add(DictTabInfo::AttributeArrayType, arrayType);
    w.add(DictTabInfo::AttributeExtType, attrType);
    w.add(DictTabInfo::AttributeExtPrecision, attrPtr.p->extPrecision);
    w.add(DictTabInfo::AttributeExtScale, attrPtr.p->extScale);
    w.add(DictTabInfo::AttributeExtLength, attrPtr.p->extLength);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }

  if (createIndexPtr.p->m_request.indexType == DictTabInfo::UniqueHashIndex) {
    jam();
    Uint32 key_type = NDB_ARRAYTYPE_FIXED;
    AttributeRecordPtr attrPtr;
    LocalAttributeRecord_list list(c_attributeRecordPool,
                                          tablePtr.p->m_attributes);
    // XXX move to parse
    for (list.first(attrPtr); !attrPtr.isNull(); list.next(attrPtr))
    {
      const Uint32 desc = attrPtr.p->attributeDescriptor;
      if (AttributeDescriptor::getPrimaryKey(desc) &&
	  AttributeDescriptor::getArrayType(desc) != NDB_ARRAYTYPE_FIXED)
      {
	key_type = NDB_ARRAYTYPE_MEDIUM_VAR;
        if (NDB_VERSION_D >= MAKE_VERSION(7,1,0))
        {
          jam();
          /**
           * We can only set this new array type "globally" if
           *   version >= X, this to allow down-grade(s) within minor versions
           *   if unique index has been added in newer version
           *
           * There is anyway nasty code sendLQHADDATTRREQ to handle the
           *   "local" variant of this
           */
          key_type = NDB_ARRAYTYPE_NONE_VAR;
        }
	break;
      }
    }

    // write concatenated primary table key attribute i.e. keyinfo
    w.add(DictTabInfo::AttributeName, "NDB$PK");
    w.add(DictTabInfo::AttributeId, createIndexPtr.p->m_attrList.sz);
    w.add(DictTabInfo::AttributeArrayType, key_type);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeExtType, (Uint32)DictTabInfo::ExtUnsigned);
    w.add(DictTabInfo::AttributeExtLength, tablePtr.p->tupKeyLength+1);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  if (createIndexPtr.p->m_request.indexType == DictTabInfo::OrderedIndex) {
    jam();
    // write index tree node as Uint32 array attribute
    w.add(DictTabInfo::AttributeName, "NDB$TNODE");
    w.add(DictTabInfo::AttributeId, createIndexPtr.p->m_attrList.sz);
    // should not matter but VAR crashes in TUP
    w.add(DictTabInfo::AttributeArrayType, (Uint32)NDB_ARRAYTYPE_FIXED);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)true);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeExtType, (Uint32)DictTabInfo::ExtUnsigned);
    w.add(DictTabInfo::AttributeExtLength, createIndexPtr.p->m_indexKeyLength);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  // finish
  w.add(DictTabInfo::TableEnd, (Uint32)true);

  CreateTableReq* req = (CreateTableReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;

  LinearSectionPtr lsPtr[3];
  lsPtr[0].p = wbuffer;
  lsPtr[0].sz = w.getWordsUsed();
  sendSignal(reference(), GSN_CREATE_TABLE_REQ, signal,
             CreateTableReq::SignalLength, JBB, lsPtr, 1);
}

void
Dbdict::createIndex_fromCreateTable(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("createIndex_fromCreateTable" << dec << V(op_key) << V(ret));

  SchemaOpPtr op_ptr;
  CreateIndexRecPtr createIndexPtr;

  findSchemaOp(op_ptr, createIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateIndxImplReq* impl_req = &createIndexPtr.p->m_request;

  if (ret == 0) {
    const CreateTableConf* conf =
      (const CreateTableConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    impl_req->indexVersion = conf->tableVersion;
    createIndexPtr.p->m_sub_create_table = true;

    createSubOps(signal, op_ptr);
  } else {
    jam();
    const CreateTableRef* ref =
      (const CreateTableRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::createIndex_toAlterIndex(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createIndex_toAlterIndex");

  AlterIndxReq* req = (AlterIndxReq*)signal->getDataPtrSend();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, AlterIndxImplReq::AlterIndexOnline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  if (op_ptr.p->m_requestInfo & CreateIndxReq::RF_BUILD_OFFLINE)
  {
    jam();
    requestInfo |= AlterIndxReq::RF_BUILD_OFFLINE;
  }

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->indexId = createIndexPtr.p->m_request.indexId;
  req->indexVersion = createIndexPtr.p->m_request.indexVersion;

  sendSignal(reference(), GSN_ALTER_INDX_REQ, signal,
             AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::createIndex_fromAlterIndex(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("createIndex_fromAlterIndex");

  SchemaOpPtr op_ptr;
  CreateIndexRecPtr createIndexPtr;

  findSchemaOp(op_ptr, createIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  //Uint32 errorCode = 0;
  if (ret == 0) {
    const AlterIndxConf* conf =
      (const AlterIndxConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createIndexPtr.p->m_sub_alter_index = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterIndxRef* ref =
      (const AlterIndxRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::createIndex_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("createIndex_reply");

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);
  const CreateIndxImplReq* impl_req = &createIndexPtr.p->m_request;

  if (!hasError(error)) {
    CreateIndxConf* conf = (CreateIndxConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->indexId = impl_req->indexId;
    conf->indexVersion = impl_req->indexVersion;

    D(V(conf->indexId) << V(conf->indexVersion));

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_INDX_CONF, signal,
               CreateIndxConf::SignalLength, JBB);
  } else {
    jam();
    CreateIndxRef* ref = (CreateIndxRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_INDX_REF, signal,
               CreateIndxRef::SignalLength, JBB);
  }
}

// CreateIndex: PREPARE

void
Dbdict::createIndex_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("createIndex_prepare");

  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);

  sendTransConf(signal, op_ptr);
}

// CreateIndex: COMMIT

void
Dbdict::createIndex_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("createIndex_commit");

  sendTransConf(signal, op_ptr);
}

// CreateIndex: COMPLETE

void
Dbdict::createIndex_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// CreateIndex: ABORT

void
Dbdict::createIndex_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createIndex_abortParse" << *op_ptr.p);

  // wl3600_todo probably nothing..

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createIndex_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createIndex_abortPrepare" << *op_ptr.p);
  // wl3600_todo

  CreateIndexRecPtr createIndexPtr;
  getOpRec(op_ptr, createIndexPtr);

  sendTransConf(signal, op_ptr);
}

// CreateIndex: MISC

void
Dbdict::execCREATE_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const CreateIndxImplConf* conf = (const CreateIndxImplConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execCREATE_INDX_IMPL_REF(Signal* signal)
{
  jamEntry();
  const CreateIndxImplRef* ref = (const CreateIndxImplRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

// CreateIndex: END

// MODULE: DropIndex

const Dbdict::OpInfo
Dbdict::DropIndexRec::g_opInfo = {
  { 'D', 'I', 'n', 0 },
  ~RT_DBDICT_DROP_INDEX,
  GSN_DROP_INDX_IMPL_REQ,
  DropIndxImplReq::SignalLength,
  //
  &Dbdict::dropIndex_seize,
  &Dbdict::dropIndex_release,
  //
  &Dbdict::dropIndex_parse,
  &Dbdict::dropIndex_subOps,
  &Dbdict::dropIndex_reply,
  //
  &Dbdict::dropIndex_prepare,
  &Dbdict::dropIndex_commit,
  &Dbdict::dropIndex_complete,
  //
  &Dbdict::dropIndex_abortParse,
  &Dbdict::dropIndex_abortPrepare
};

bool
Dbdict::dropIndex_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropIndexRec>(op_ptr);
}

void
Dbdict::dropIndex_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropIndexRec>(op_ptr);
}

void
Dbdict::execDROP_INDX_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  if (check_sender_version(signal, MAKE_VERSION(6,4,0)) < 0)
  {
    jam();
    /**
     * Pekka has for some obscure reason switched places of
     *   senderRef/senderData
     */
    DropIndxReq * tmp = (DropIndxReq*)signal->getDataPtr();
    do_swap(tmp->clientRef, tmp->clientData);
  }

  const DropIndxReq req_copy =
    *(const DropIndxReq*)signal->getDataPtr();
  const DropIndxReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropIndexRecPtr dropIndexPtr;
    DropIndxImplReq* impl_req;

    startClientReq(op_ptr, dropIndexPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = RNIL;    // wl3600_todo fill in in parse
    impl_req->tableVersion = 0;
    impl_req->indexId = req->indexId;
    impl_req->indexVersion = req->indexVersion;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropIndxRef* ref = (DropIndxRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->indexId = req->indexId;
  ref->indexVersion = req->indexVersion;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_DROP_INDX_REF, signal,
             DropIndxRef::SignalLength, JBB);
}

// DropIndex: PARSE

void
Dbdict::dropIndex_parse(Signal* signal, bool master,
                        SchemaOpPtr op_ptr,
                        SectionHandle& handle, ErrorInfo& error)
{
  D("dropIndex_parse" << V(op_ptr.i) << *op_ptr.p);
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);
  DropIndxImplReq* impl_req = &dropIndexPtr.p->m_request;

  if (!(impl_req->indexId < c_noOfMetaTables)) {
    jam();
    setError(error, DropIndxRef::IndexNotFound, __LINE__);
    return;
  }

  Uint32 err = check_read_obj(impl_req->indexId, trans_ptr.p->m_transId);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  if (!ok || !indexPtr.p->isIndex())
  {
    jam();
    setError(error, DropIndxRef::NotAnIndex, __LINE__);
    return;
  }

  if (indexPtr.p->tableVersion != impl_req->indexVersion)
  {
    jam();
    setError(error, DropIndxRef::InvalidIndexVersion, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->indexId, trans_ptr.p->m_transId,
                      SchemaFile::SF_DROP, error))
  {
    jam();
    return;
  }

  TableRecordPtr tablePtr;
  ok = find_object(tablePtr, indexPtr.p->primaryTableId);
  if (!ok)
  {
    jam();
    setError(error, CreateIndxRef::InvalidPrimaryTable, __LINE__);
    return;
  }

  // master sets primary table, slave verifies it agrees
  if (master)
  {
    jam();
    impl_req->tableId = tablePtr.p->tableId;
    impl_req->tableVersion = tablePtr.p->tableVersion;
  }
  else
  {
    if (impl_req->tableId != tablePtr.p->tableId)
    {
      jam(); // wl3600_todo better error code
      setError(error, DropIndxRef::InvalidIndexVersion, __LINE__);
      return;
    }
    if (impl_req->tableVersion != tablePtr.p->tableVersion)
    {
      jam(); // wl3600_todo better error code
      setError(error, DropIndxRef::InvalidIndexVersion, __LINE__);
      return;
    }
  }

  if (ERROR_INSERTED(6122)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9122, __LINE__);
    return;
  }
}

bool
Dbdict::dropIndex_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropIndex_subOps" << V(op_ptr.i) << *op_ptr.p);

  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);

  // op to alter index offline
  if (!dropIndexPtr.p->m_sub_alter_index) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::dropIndex_fromAlterIndex),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    dropIndex_toAlterIndex(signal, op_ptr);
    return true;
  }

  // op to drop index table
  if (!dropIndexPtr.p->m_sub_drop_table) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::dropIndex_fromDropTable),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    dropIndex_toDropTable(signal, op_ptr);
    return true;
  }

  return false;
}

void
Dbdict::dropIndex_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("dropIndex_reply" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);
  const DropIndxImplReq* impl_req = &dropIndexPtr.p->m_request;

  if (!hasError(error)) {
    DropIndxConf* conf = (DropIndxConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    // wl3600_todo why send redundant fields?
    conf->indexId = impl_req->indexId;
    conf->indexVersion = impl_req->indexVersion;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_INDX_CONF, signal,
               DropIndxConf::SignalLength, JBB);
  } else {
    jam();
    DropIndxRef* ref = (DropIndxRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    // wl3600_todo why send redundant fields?
    ref->indexId = impl_req->indexId;
    ref->indexVersion = impl_req->indexVersion;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_INDX_REF, signal,
               DropIndxRef::SignalLength, JBB);
  }
}

// DropIndex: PREPARE

void
Dbdict::dropIndex_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);

  D("dropIndex_prepare" << *op_ptr.p);

  sendTransConf(signal, op_ptr);
}

// DropIndex: COMMIT

void
Dbdict::dropIndex_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);

  D("dropIndex_commit" << *op_ptr.p);

  sendTransConf(signal, op_ptr);
}

// DropIndex: COMPLETE

void
Dbdict::dropIndex_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// DropIndex: ABORT

void
Dbdict::dropIndex_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropIndex_abortParse" << *op_ptr.p);
  // wl3600_todo
  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropIndex_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropIndex_abortPrepare" << *op_ptr.p);
  ndbrequire(false);
  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropIndex_toAlterIndex(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropIndex_toAlterIndex");

  AlterIndxReq* req = (AlterIndxReq*)signal->getDataPtrSend();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, AlterIndxImplReq::AlterIndexOffline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->indexId = dropIndexPtr.p->m_request.indexId;
  req->indexVersion = dropIndexPtr.p->m_request.indexVersion;

  sendSignal(reference(), GSN_ALTER_INDX_REQ, signal,
             AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::dropIndex_fromAlterIndex(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("dropIndex_fromAlterIndex");

  SchemaOpPtr op_ptr;
  DropIndexRecPtr dropIndexPtr;

  findSchemaOp(op_ptr, dropIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    const AlterIndxConf* conf =
      (const AlterIndxConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    dropIndexPtr.p->m_sub_alter_index = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterIndxRef* ref =
      (const AlterIndxRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::dropIndex_toDropTable(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropIndex_toDropTable");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropIndexRecPtr dropIndexPtr;
  getOpRec(op_ptr, dropIndexPtr);
  const DropIndxImplReq* impl_req = &dropIndexPtr.p->m_request;

  DropTableReq* req = (DropTableReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->indexId;
  req->tableVersion = impl_req->indexVersion;
  sendSignal(reference(), GSN_DROP_TABLE_REQ, signal,
             DropTableReq::SignalLength, JBB);
}

void
Dbdict::dropIndex_fromDropTable(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("dropIndex_fromDropTable" << dec << V(op_key) << V(ret));

  SchemaOpPtr op_ptr;
  DropIndexRecPtr dropIndexPtr;

  findSchemaOp(op_ptr, dropIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const DropTableConf* conf =
      (const DropTableConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    dropIndexPtr.p->m_sub_drop_table = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const DropTableRef* ref =
      (const DropTableRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

// DropIndex: MISC

void
Dbdict::execDROP_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const DropIndxImplConf* conf = (const DropIndxImplConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execDROP_INDX_IMPL_REF(Signal* signal)
{
  jamEntry();
  const DropIndxImplRef* ref = (const DropIndxImplRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

// DropIndex: END

// MODULE: AlterIndex

const Dbdict::OpInfo
Dbdict::AlterIndexRec::g_opInfo = {
  { 'A', 'I', 'n', 0 },
  ~RT_DBDICT_ALTER_INDEX,
  GSN_ALTER_INDX_IMPL_REQ,
  AlterIndxImplReq::SignalLength,
  //
  &Dbdict::alterIndex_seize,
  &Dbdict::alterIndex_release,
  //
  &Dbdict::alterIndex_parse,
  &Dbdict::alterIndex_subOps,
  &Dbdict::alterIndex_reply,
  //
  &Dbdict::alterIndex_prepare,
  &Dbdict::alterIndex_commit,
  &Dbdict::alterIndex_complete,
  //
  &Dbdict::alterIndex_abortParse,
  &Dbdict::alterIndex_abortPrepare
};

bool
Dbdict::alterIndex_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<AlterIndexRec>(op_ptr);
}

void
Dbdict::alterIndex_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<AlterIndexRec>(op_ptr);
}

void
Dbdict::execALTER_INDX_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const AlterIndxReq req_copy =
    *(const AlterIndxReq*)signal->getDataPtr();
  const AlterIndxReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    AlterIndexRecPtr alterIndexPtr;
    AlterIndxImplReq* impl_req;

    startClientReq(op_ptr, alterIndexPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = RNIL;    // wl3600_todo fill in in parse
    impl_req->tableVersion = 0;
    impl_req->indexId = req->indexId;
    impl_req->indexVersion = req->indexVersion;
    impl_req->indexType = 0; // fill in parse

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  AlterIndxRef* ref = (AlterIndxRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->indexId = req->indexId;
  ref->indexVersion = req->indexVersion;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_ALTER_INDX_REF, signal,
             AlterIndxRef::SignalLength, JBB);
}

const Dbdict::TriggerTmpl
Dbdict::g_hashIndexTriggerTmpl[1] = {
  { "NDB$INDEX_%u_UI",
    {
      TriggerType::SECONDARY_INDEX,
      TriggerActionTime::TA_AFTER,
      TriggerEvent::TE_CUSTOM,
      false, false, true // monitor replicas, monitor all, report all
    }
  }
};

const Dbdict::TriggerTmpl
Dbdict::g_orderedIndexTriggerTmpl[1] = {
  { "NDB$INDEX_%u_CUSTOM",
    {
      TriggerType::ORDERED_INDEX,
      TriggerActionTime::TA_CUSTOM,
      TriggerEvent::TE_CUSTOM,
      true, false, true
    }
  }
};

const Dbdict::TriggerTmpl
Dbdict::g_buildIndexConstraintTmpl[1] = {
  { "NDB$INDEX_%u_BUILD",
    {
      TriggerType::READ_ONLY_CONSTRAINT,
      TriggerActionTime::TA_AFTER,
      TriggerEvent::TE_UPDATE,
      false, true, false
    }
  }
};

const Dbdict::TriggerTmpl
Dbdict::g_reorgTriggerTmpl[1] = {
  { "NDB$REORG_%u",
    {
      TriggerType::REORG_TRIGGER,
      TriggerActionTime::TA_AFTER,
      TriggerEvent::TE_CUSTOM,
      false, true, false
    }
  }
};

// AlterIndex: PARSE

void
Dbdict::alterIndex_parse(Signal* signal, bool master,
                         SchemaOpPtr op_ptr,
                         SectionHandle& handle, ErrorInfo& error)
{
  D("alterIndex_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  TableRecordPtr indexPtr;
  if (!(impl_req->indexId < c_noOfMetaTables)) {
    jam();
    setError(error, AlterIndxRef::IndexNotFound, __LINE__);
    return;
  }
  if (check_read_obj(impl_req->indexId, trans_ptr.p->m_transId) == GetTabInfoRef::TableNotDefined)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }
  jam();

  bool ok = find_object(indexPtr, impl_req->indexId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }

  // get name for system index check later
  char indexName[MAX_TAB_NAME_SIZE];
  memset(indexName, 0, sizeof(indexName));
  {
    ConstRope r(c_rope_pool, indexPtr.p->tableName);
    r.copy(indexName);
  }
  D("index " << indexName);

  if (indexPtr.p->tableVersion != impl_req->indexVersion) {
    jam();
    setError(error, AlterIndxRef::InvalidIndexVersion, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->indexId, trans_ptr.p->m_transId,
                      SchemaFile::SF_ALTER, error))
  {
    jam();
    return;
  }

  // check it is an index and set trigger info for sub-ops
  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    jam();
    alterIndexPtr.p->m_triggerTmpl = g_hashIndexTriggerTmpl;
    break;
  case DictTabInfo::OrderedIndex:
    jam();
    alterIndexPtr.p->m_triggerTmpl = g_orderedIndexTriggerTmpl;
    break;
  default:
    jam();
    setError(error, AlterIndxRef::NotAnIndex, __LINE__);
    return;
  }

  if (master)
  {
    jam();
    impl_req->indexType = indexPtr.p->tableType;
  }
  else
  {
    jam();
    ndbrequire(impl_req->indexType == (Uint32)indexPtr.p->tableType);
  }

  ndbrequire(indexPtr.p->primaryTableId != RNIL);
  TableRecordPtr tablePtr;
  ok = find_object(tablePtr, indexPtr.p->primaryTableId);
  ndbrequire(ok); // TODO:msundell set error

  // master sets primary table, participant verifies it agrees
  if (master)
  {
    jam();
    impl_req->tableId = tablePtr.p->tableId;
    impl_req->tableVersion = tablePtr.p->tableVersion;
  }
  else
  {
    if (impl_req->tableId != tablePtr.p->tableId)
    {
      jam(); // wl3600_todo better error code
      setError(error, AlterIndxRef::InvalidIndexVersion, __LINE__);
      return;
    }
    if (impl_req->tableVersion != tablePtr.p->tableVersion)
    {
      jam(); // wl3600_todo better error code
      setError(error, AlterIndxRef::InvalidIndexVersion, __LINE__);
      return;
    }
  }

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch (requestType) {
  case AlterIndxImplReq::AlterIndexOnline:
    jam();
    indexPtr.p->indexState = TableRecord::IS_BUILDING;
    break;
  case AlterIndxImplReq::AlterIndexOffline:
    jam();
    indexPtr.p->indexState = TableRecord::IS_DROPPING;
    break;
  case AlterIndxImplReq::AlterIndexAddPartition:
    jam();
    if (indexPtr.p->tableType == DictTabInfo::OrderedIndex)
    {
      jam();
      /**
       * Link operation to AlterTable
       *   either if prev op is AlterTable using baseop.i
       *       or if prev op is AlterIndex using baseop.p->m_base_op_ptr_i
       *   (i.e recursivly, assuming that no operation can come inbetween)
       */
      SchemaOpPtr baseop = op_ptr;
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      ndbrequire(list.prev(baseop));
      Uint32 sz = sizeof(baseop.p->m_oprec_ptr.p->m_opType);
      const char * opType = baseop.p->m_oprec_ptr.p->m_opType;
      if (memcmp(opType, AlterTableRec::g_opInfo.m_opType, sz) == 0)
      {
        jam();
        op_ptr.p->m_base_op_ptr_i = baseop.i;
      }
      else
      {
        jam();
        ndbrequire(memcmp(opType, AlterIndexRec::g_opInfo.m_opType, sz) == 0);
        op_ptr.p->m_base_op_ptr_i = baseop.p->m_base_op_ptr_i;
      }
      break;
    }
    // else invalid request type
  default:
    ndbassert(false);
    setError(error, AlterIndxRef::BadRequestType, __LINE__);
    return;
  }

  // set attribute mask (of primary table attribute ids)
  getIndexAttrMask(indexPtr, alterIndexPtr.p->m_attrMask);

  // ordered index stats
  if (indexPtr.p->tableType == DictTabInfo::OrderedIndex) {
    jam();

    // always compute monitored replica
    if (requestType == AlterIndxImplReq::AlterIndexOnline) {
      jam();
      set_index_stat_frag(signal, indexPtr);
    }

    // skip system indexes (at least index stats indexes)
    if (strstr(indexName, "/" NDB_INDEX_STAT_PREFIX) != 0) {
      jam();
      D("skip index stats operations for system index");
      alterIndexPtr.p->m_sub_index_stat_dml = true;
      alterIndexPtr.p->m_sub_index_stat_mon = true;
    }

    // disable update/delete if db not up
    if (getNodeState().startLevel != NodeState::SL_STARTED &&
        getNodeState().startLevel != NodeState::SL_SINGLEUSER) {
      jam();
      alterIndexPtr.p->m_sub_index_stat_dml = true;
    }

    // disable update on create if not auto
    if (requestType == AlterIndxImplReq::AlterIndexOnline &&
        !c_indexStatAutoCreate) {
      jam();
      alterIndexPtr.p->m_sub_index_stat_dml = true;
    }

    // always delete on drop because manual update may have been done
    if (requestType == AlterIndxImplReq::AlterIndexOffline) {
      jam();
    }

    // always assign/remove monitored replica in TUX instances
  }

  if (ERROR_INSERTED(6123)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9123, __LINE__);
    return;
  }
}

void
Dbdict::set_index_stat_frag(Signal* signal, TableRecordPtr indexPtr)
{
  jam();
  DictObjectPtr index_obj_ptr;
  c_obj_pool.getPtr(index_obj_ptr, indexPtr.p->m_obj_ptr_i);
  const Uint32 indexId = index_obj_ptr.p->m_id;
  Uint32 err = get_fragmentation(signal, indexId);
  ndbrequire(err == 0);
  // format: R F { fragId node1 .. nodeR } x { F }
  // fragId: 0 1 2 .. (or whatever)
  const Uint16* frag_data = (Uint16*)(signal->theData+25);
  const Uint32 noOfFragments = frag_data[1];
  const Uint32 noOfReplicas = frag_data[0];
  ndbrequire(noOfFragments != 0 && noOfReplicas != 0);

  // distribute by table and index id
  const Uint32 value = indexPtr.p->primaryTableId + indexId;
  const Uint32 fragId = value % noOfFragments;
  const Uint32 fragIndex = 2 + (1 + noOfReplicas) * fragId;
  const Uint32 nodeIndex = value % noOfReplicas;

  indexPtr.p->indexStatFragId = fragId;
  bzero(indexPtr.p->indexStatNodes, sizeof(indexPtr.p->indexStatNodes));
  for (Uint32 i = 0; i < noOfReplicas; i++)
  {
    Uint32 idx = fragIndex + 1 + (nodeIndex + i) % noOfReplicas;
    indexPtr.p->indexStatNodes[i] = frag_data[idx];
  }
  D("set_index_stat_frag" << V(indexId) << V(fragId)
    << V(indexPtr.p->indexStatNodes[0]));
}

bool
Dbdict::alterIndex_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterIndex_subOps" << V(op_ptr.i) << *op_ptr.p);

  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  // ops to create or drop triggers
  if (alterIndexPtr.p->m_sub_trigger == false)
  {
    jam();
    alterIndexPtr.p->m_sub_trigger = true;
    switch (requestType) {
    case AlterIndxImplReq::AlterIndexOnline:
      jam();
      {
        Callback c = {
          safe_cast(&Dbdict::alterIndex_fromCreateTrigger),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;
        alterIndex_toCreateTrigger(signal, op_ptr);
      }
      break;
    case AlterIndxImplReq::AlterIndexOffline:
      jam();
      {
        Callback c = {
          safe_cast(&Dbdict::alterIndex_fromDropTrigger),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;
        alterIndex_toDropTrigger(signal, op_ptr);
      }
      break;
    case AlterIndxImplReq::AlterIndexAddPartition:
      jam();
      return false;
    default:
      ndbrequire(false);
      break;
    }
    return true;
  }

  if (requestType == AlterIndxImplReq::AlterIndexOnline &&
      !alterIndexPtr.p->m_sub_build_index) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::alterIndex_fromBuildIndex),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    alterIndex_toBuildIndex(signal, op_ptr);
    return true;
  }

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);

  if (ok && indexPtr.p->isOrderedIndex() &&
      (!alterIndexPtr.p->m_sub_index_stat_dml ||
       !alterIndexPtr.p->m_sub_index_stat_mon)) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::alterIndex_fromIndexStat),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    alterIndex_toIndexStat(signal, op_ptr);
    return true;
  }

  return false;
}

void
Dbdict::alterIndex_toCreateTrigger(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterIndex_toCreateTrigger");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;
  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  const TriggerTmpl& triggerTmpl = alterIndexPtr.p->m_triggerTmpl[0];

  CreateTrigReq* req = (CreateTrigReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, CreateTrigReq::CreateTriggerOnline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->indexId = impl_req->indexId;
  req->indexVersion = impl_req->indexVersion;
  req->triggerNo = 0;

  Uint32 forceTriggerId = indexPtr.p->triggerId;
  D(V(getNodeState().startLevel) << V(NodeState::SL_STARTED));
  if (getNodeState().startLevel == NodeState::SL_STARTED)
  {
    ndbrequire(forceTriggerId == RNIL);
  }
  req->forceTriggerId = forceTriggerId;

  TriggerInfo::packTriggerInfo(req->triggerInfo, triggerTmpl.triggerInfo);

  req->receiverRef = 0;

  char triggerName[MAX_TAB_NAME_SIZE];
  sprintf(triggerName, triggerTmpl.nameFormat, impl_req->indexId);

  // name section
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  w.reset();
  w.add(DictTabInfo::TableName, triggerName);
  LinearSectionPtr lsPtr[3];
  lsPtr[0].p = buffer;
  lsPtr[0].sz = w.getWordsUsed();

  lsPtr[1].p = alterIndexPtr.p->m_attrMask.rep.data;
  lsPtr[1].sz = alterIndexPtr.p->m_attrMask.getSizeInWords();

  sendSignal(reference(), GSN_CREATE_TRIG_REQ, signal,
             CreateTrigReq::SignalLength, JBB, lsPtr, 2);
}

void
Dbdict::alterIndex_fromCreateTrigger(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;

  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  D("alterIndex_fromCreateTrigger" << dec << V(op_key) << V(ret));

  if (ret == 0) {
    jam();
    const CreateTrigConf* conf =
      (const CreateTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  } else {
    const CreateTrigRef* ref =
      (const CreateTrigRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterIndex_toDropTrigger(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterIndex_toDropTrigger");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  //const TriggerTmpl& triggerTmpl = alterIndexPtr.p->m_triggerTmpl[0];

  DropTrigReq* req = (DropTrigReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, 0);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->indexId = impl_req->indexId;
  req->indexVersion = impl_req->indexVersion;
  req->triggerNo = 0;
  req->triggerId = indexPtr.p->triggerId;

  sendSignal(reference(), GSN_DROP_TRIG_REQ, signal,
             DropTrigReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromDropTrigger(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;

  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  D("alterIndex_fromDropTrigger" << dec << V(op_key) << V(ret));

  if (ret == 0) {
    const DropTrigConf* conf =
      (const DropTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const DropTrigRef* ref =
      (const DropTrigRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterIndex_toBuildIndex(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterIndex_toBuildIndex");

  BuildIndxReq* req = (BuildIndxReq*)signal->getDataPtrSend();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, BuildIndxReq::MainOp);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  if (op_ptr.p->m_requestInfo & AlterIndxReq::RF_BUILD_OFFLINE)
  {
    jam();
    requestInfo |= BuildIndxReq::RF_BUILD_OFFLINE;
  }

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->buildId = 0; // not used
  req->buildKey = 0; // not used
  req->tableId = impl_req->tableId;
  req->indexId = impl_req->indexId;
  req->indexType = impl_req->indexType;
  req->parallelism = 16;

  sendSignal(reference(), GSN_BUILDINDXREQ, signal,
             BuildIndxReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromBuildIndex(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("alterIndex_fromBuildIndex");

  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;

  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const BuildIndxConf* conf =
      (const BuildIndxConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    alterIndexPtr.p->m_sub_build_index = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const BuildIndxRef* ref =
      (const BuildIndxRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterIndex_toIndexStat(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterIndex_toIndexStat");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  IndexStatReq* req = (IndexStatReq*)signal->getDataPtrSend();

  Uint32 requestType = 0;
  switch (impl_req->requestType) {
  case AlterIndxImplReq::AlterIndexOnline:
    if (!alterIndexPtr.p->m_sub_index_stat_dml)
      requestType = IndexStatReq::RT_UPDATE_STAT;
    else
      requestType = IndexStatReq::RT_START_MON;
    break;
  case AlterIndxImplReq::AlterIndexOffline:
    if (!alterIndexPtr.p->m_sub_index_stat_dml)
      requestType = IndexStatReq::RT_DELETE_STAT;
    else
      requestType = IndexStatReq::RT_STOP_MON;
    break;
  default:
    ndbrequire(false);
    break;
  }

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, requestType);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->requestFlag = 0;
  req->indexId = impl_req->indexId;
  req->indexVersion = indexPtr.p->tableVersion;
  req->tableId = impl_req->tableId;

  sendSignal(reference(), GSN_INDEX_STAT_REQ,
             signal, IndexStatReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromIndexStat(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("alterIndex_fromIndexStat");

  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;

  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const IndexStatConf* conf =
      (const IndexStatConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    ndbrequire(!alterIndexPtr.p->m_sub_index_stat_dml ||
               !alterIndexPtr.p->m_sub_index_stat_mon);
    alterIndexPtr.p->m_sub_index_stat_dml = true;
    alterIndexPtr.p->m_sub_index_stat_mon = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const IndexStatRef* ref =
      (const IndexStatRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::alterIndex_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  D("alterIndex_reply" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  if (!hasError(error)) {
    AlterIndxConf* conf = (AlterIndxConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->indexId = impl_req->indexId;
    conf->indexVersion = impl_req->indexVersion;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_INDX_CONF,
               signal, AlterIndxConf::SignalLength, JBB);
  } else {
    jam();
    AlterIndxRef* ref = (AlterIndxRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    // wl3600_todo requestType (fix all at once)
    // wl3600_todo next 2 redundant (fix all at once)
    ref->indexId = impl_req->indexId;
    ref->indexVersion = impl_req->indexVersion;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_INDX_REF,
               signal, AlterIndxRef::SignalLength, JBB);
  }
}

// AlterIndex: PREPARE

void
Dbdict::alterIndex_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  D("alterIndex_prepare" << *op_ptr.p);

  /*
   * Create table creates index table in all blocks.  DBTC has
   * an additional entry for hash index.  It is created (offline)
   * and dropped here.  wl3600_todo redesign
   */

  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    {
      Callback c = {
        safe_cast(&Dbdict::alterIndex_fromLocal),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      switch (requestType) {
      case AlterIndxImplReq::AlterIndexOnline:
        alterIndex_toCreateLocal(signal, op_ptr);
        break;
      case AlterIndxImplReq::AlterIndexOffline:
        alterIndex_toDropLocal(signal, op_ptr);
        break;
      default:
        ndbrequire(false);
        break;
      }
    }
    break;
  case DictTabInfo::OrderedIndex:
    if (requestType == AlterIndxImplReq::AlterIndexAddPartition)
    {
      jam();
      Callback c = {
        safe_cast(&Dbdict::alterIndex_fromAddPartitions),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;
      alterIndex_toAddPartitions(signal, op_ptr);
      return;
    }
    sendTransConf(signal, op_ptr);
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::alterIndex_toCreateLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  D("alterIndex_toCreateLocal" << *op_ptr.p);

  CreateIndxImplReq* req = (CreateIndxImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = 0;
  req->tableId = impl_req->tableId;
  req->tableVersion = 0; // not used
  req->indexType = indexPtr.p->tableType;
  req->indexId = impl_req->indexId;
  req->indexVersion = 0; // not used

  // attribute list
  getIndexAttrList(indexPtr, alterIndexPtr.p->m_attrList);
  LinearSectionPtr ls_ptr[3];
  ls_ptr[0].p = (Uint32*)&alterIndexPtr.p->m_attrList;
  ls_ptr[0].sz = 1 + alterIndexPtr.p->m_attrList.sz;

  BlockReference ref = DBTC_REF;
  sendSignal(ref, GSN_CREATE_INDX_IMPL_REQ, signal,
             CreateIndxImplReq::SignalLength, JBB, ls_ptr, 1);
}

void
Dbdict::alterIndex_toDropLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  D("alterIndex_toDropLocal" << *op_ptr.p);

  DropIndxImplReq* req = (DropIndxImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->tableId = impl_req->tableId;
  req->tableVersion = 0; // not used
  req->indexId = impl_req->indexId;
  req->indexVersion = 0; // not used

  BlockReference ref = DBTC_REF;
  sendSignal(ref, GSN_DROP_INDX_IMPL_REQ, signal,
             DropIndxImplReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;
  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  D("alterIndex_fromLocal" << *op_ptr.p << V(ret));

  if (ret == 0) {
    jam();
    alterIndexPtr.p->m_tc_index_done = true;
    sendTransConf(signal, op_ptr);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::alterIndex_toAddPartitions(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  /**
   * Get fragmentation for table table from alterTable operation
   */
  SchemaOpPtr base_op;
  c_schemaOpPool.getPtr(base_op, op_ptr.p->m_base_op_ptr_i);

  const OpSection& fragInfoSec =
    getOpSection(base_op, AlterTabReq::FRAGMENTATION);
  SegmentedSectionPtr fragInfoPtr;
  LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
  bool ok = copyOut(op_sec_pool, fragInfoSec, fragInfoPtr);
  ndbrequire(ok);
  SectionHandle handle(this, fragInfoPtr.i);

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTablePrepare;
  req->tableId = impl_req->indexId;
  req->tableVersion = impl_req->indexVersion;
  req->newTableVersion = impl_req->indexVersion;
  req->gci = 0;
  req->changeMask = 0;
  req->connectPtr = RNIL;
  req->noOfNewAttr = 0;
  req->newNoOfCharsets = 0;
  req->newNoOfKeyAttrs = 0;
  AlterTableReq::setAddFragFlag(req->changeMask, 1);

  sendSignal(DBDIH_REF, GSN_ALTER_TAB_REQ, signal,
             AlterTabReq::SignalLength, JBB, &handle);
}

void
Dbdict::alterIndex_fromAddPartitions(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;
  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (ret == 0) {
    jam();

    const AlterTabConf* conf =
      (const AlterTabConf*)signal->getDataPtr();

    alterIndexPtr.p->m_dihAddFragPtr = conf->connectPtr;

    sendTransConf(signal, op_ptr);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

// AlterIndex: COMMIT

void
Dbdict::alterIndex_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;

  if (impl_req->requestType == AlterIndxImplReq::AlterIndexAddPartition)
  {
    AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = op_ptr.p->op_key;
    req->requestType = AlterTabReq::AlterTableCommit;
    req->tableId = impl_req->indexId;
    req->tableVersion = impl_req->indexVersion;
    req->newTableVersion = impl_req->indexVersion;
    req->gci = 0;
    req->changeMask = 0;
    req->connectPtr = RNIL;
    req->noOfNewAttr = 0;
    req->newNoOfCharsets = 0;
    req->newNoOfKeyAttrs = 0;
    req->connectPtr = alterIndexPtr.p->m_dihAddFragPtr;

    Callback c = {
      safe_cast(&Dbdict::alterIndex_fromLocal),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    sendSignal(DBDIH_REF,
               GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB);
    return;
  }

  sendTransConf(signal, op_ptr);
}

// AlterIndex: COMPLETE

void
Dbdict::alterIndex_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// AlterIndex: ABORT

void
Dbdict::alterIndex_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;
  Uint32 indexId = impl_req->indexId;

  D("alterIndex_abortParse" << *op_ptr.p);

  do {
    if (!(impl_req->indexId < c_noOfMetaTables)) {
      jam();
      D("invalid index id" << V(indexId));
      break;
    }

    TableRecordPtr indexPtr;
    bool ok = find_object(indexPtr, indexId);
    if (!ok)
    {
      jam();
      break;
    }

    switch (requestType) {
    case AlterIndxImplReq::AlterIndexOnline:
      jam();
      indexPtr.p->indexState = TableRecord::IS_OFFLINE;
      break;
    case AlterIndxImplReq::AlterIndexOffline:
      jam();
      indexPtr.p->indexState = TableRecord::IS_ONLINE;
      break;
    default:
      break;
    }
  } while (0);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::alterIndex_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterIndexRecPtr alterIndexPtr;
  getOpRec(op_ptr, alterIndexPtr);
  const AlterIndxImplReq* impl_req = &alterIndexPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  D("alterIndex_abortPrepare" << *op_ptr.p);

  if (impl_req->requestType == AlterIndxImplReq::AlterIndexAddPartition)
  {
    AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = op_ptr.p->op_key;
    req->requestType = AlterTabReq::AlterTableRevert;
    req->tableId = impl_req->indexId;
    req->tableVersion = impl_req->indexVersion;
    req->newTableVersion = impl_req->indexVersion;
    req->gci = 0;
    req->changeMask = 0;
    req->connectPtr = RNIL;
    req->noOfNewAttr = 0;
    req->newNoOfCharsets = 0;
    req->newNoOfKeyAttrs = 0;
    req->connectPtr = alterIndexPtr.p->m_dihAddFragPtr;
    AlterTableReq::setAddFragFlag(req->changeMask, 1);

    Callback c = {
      safe_cast(&Dbdict::alterIndex_fromLocal),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    sendSignal(DBDIH_REF,
               GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB);
    return;
  }

  if (!alterIndexPtr.p->m_tc_index_done) {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  Callback c = {
    safe_cast(&Dbdict::alterIndex_abortFromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  switch (requestType) {
  case AlterIndxImplReq::AlterIndexOnline:
    jam();
    alterIndex_toDropLocal(signal, op_ptr);
    break;
  case AlterIndxImplReq::AlterIndexOffline:
    jam();
    alterIndex_toCreateLocal(signal, op_ptr);
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::alterIndex_abortFromLocal(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterIndexRecPtr alterIndexPtr;
  findSchemaOp(op_ptr, alterIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  D("alterIndex_abortFromLocal" << V(ret) << *op_ptr.p);

  if (ret == 0) {
    jam();
    alterIndexPtr.p->m_tc_index_done = false;
    sendTransConf(signal, op_ptr);
  } else {
    // abort is not allowed to fail
    ndbrequire(false);
  }
}

// AlterIndex: MISC

void
Dbdict::execALTER_INDX_CONF(Signal* signal)
{
  jamEntry();
  const AlterIndxConf* conf = (const AlterIndxConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execALTER_INDX_REF(Signal* signal)
{
  jamEntry();
  const AlterIndxRef* ref = (const AlterIndxRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execALTER_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const AlterIndxImplConf* conf = (const AlterIndxImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execALTER_INDX_IMPL_REF(Signal* signal)
{
  jamEntry();
  const AlterIndxImplRef* ref = (const AlterIndxImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

// AlterIndex: END

// MODULE: BuildIndex

const Dbdict::OpInfo
Dbdict::BuildIndexRec::g_opInfo = {
  { 'B', 'I', 'n', 0 },
  ~RT_DBDICT_BUILD_INDEX,
  GSN_BUILD_INDX_IMPL_REQ,
  BuildIndxImplReq::SignalLength,
  //
  &Dbdict::buildIndex_seize,
  &Dbdict::buildIndex_release,
  //
  &Dbdict::buildIndex_parse,
  &Dbdict::buildIndex_subOps,
  &Dbdict::buildIndex_reply,
  //
  &Dbdict::buildIndex_prepare,
  &Dbdict::buildIndex_commit,
  &Dbdict::buildIndex_complete,
  //
  &Dbdict::buildIndex_abortParse,
  &Dbdict::buildIndex_abortPrepare
};

bool
Dbdict::buildIndex_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<BuildIndexRec>(op_ptr);
}

void
Dbdict::buildIndex_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<BuildIndexRec>(op_ptr);
}

void
Dbdict::execBUILDINDXREQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const BuildIndxReq req_copy =
    *(const BuildIndxReq*)signal->getDataPtr();
  const BuildIndxReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    BuildIndexRecPtr buildIndexPtr;
    BuildIndxImplReq* impl_req;

    startClientReq(op_ptr, buildIndexPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->buildId = req->buildId; //wl3600_todo remove from client sig
    impl_req->buildKey = req->buildKey;
    impl_req->tableId = req->tableId;
    impl_req->indexId = req->indexId;
    impl_req->indexType = req->indexType;  //wl3600_todo remove from client sig
    impl_req->parallelism = req->parallelism;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  BuildIndxRef* ref = (BuildIndxRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->tableId = req->tableId;
  ref->indexId = req->indexId;
  ref->indexType = req->indexType;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_BUILDINDXREF, signal,
             BuildIndxRef::SignalLength, JBB);
}

// BuildIndex: PARSE

void
Dbdict::buildIndex_parse(Signal* signal, bool master,
                         SchemaOpPtr op_ptr,
                         SectionHandle& handle, ErrorInfo& error)
{
   D("buildIndex_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;
  Uint32 err;

  // get index
  TableRecordPtr indexPtr;
  err = check_read_obj(impl_req->indexId, trans_ptr.p->m_transId);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }
  bool ok = find_object(indexPtr, impl_req->indexId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }

  ndbrequire(indexPtr.p->primaryTableId == impl_req->tableId);

  // get primary table
  TableRecordPtr tablePtr;
  if (!(impl_req->tableId < c_noOfMetaTables)) {
    jam();
    setError(error, BuildIndxRef::IndexNotFound, __LINE__);
    return;
  }
  ok = find_object(tablePtr, impl_req->tableId);
  if (!ok)
  {
    jam();
    setError(error, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }

  // set attribute lists
  getIndexAttrList(indexPtr, buildIndexPtr.p->m_indexKeyList);
  getTableKeyList(tablePtr, buildIndexPtr.p->m_tableKeyList);

  Uint32 requestType = impl_req->requestType;
  switch (requestType) {
  case BuildIndxReq::MainOp:
  case BuildIndxReq::SubOp:
    break;
  default:
    jam();
    ndbassert(false);
    setError(error, BuildIndxRef::BadRequestType, __LINE__);
    return;
  }

  // set build constraint info and attribute mask
  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    jam();
    if (requestType == BuildIndxReq::MainOp) {
      buildIndexPtr.p->m_triggerTmpl = g_buildIndexConstraintTmpl;
      buildIndexPtr.p->m_subOpCount = 3;
      buildIndexPtr.p->m_subOpIndex = 0;

      // mask is NDB$PK (last attribute)
      Uint32 attrId = indexPtr.p->noOfAttributes - 1;
      buildIndexPtr.p->m_attrMask.clear();
      buildIndexPtr.p->m_attrMask.set(attrId);
      break;
    }
    /*FALLTHRU*/
  default:
    jam();
    {
      buildIndexPtr.p->m_triggerTmpl = 0;
      buildIndexPtr.p->m_subOpCount = 0;
      buildIndexPtr.p->m_doBuild = true;
    }
    break;
  }

  if (ERROR_INSERTED(6126)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9126, __LINE__);
    return;
  }
}

bool
Dbdict::buildIndex_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_subOps" << V(op_ptr.i) << *op_ptr.p);

  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  //const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  if (buildIndexPtr.p->m_subOpIndex < buildIndexPtr.p->m_subOpCount) {
    jam();
    switch (buildIndexPtr.p->m_subOpIndex) {
    case 0:
      jam();
      {
        Callback c = {
          safe_cast(&Dbdict::buildIndex_fromCreateConstraint),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;
        buildIndex_toCreateConstraint(signal, op_ptr);
        return true;
      }
      break;
    case 1:
      jam();
      // the sub-op that does the actual hash index build
      {
        Callback c = {
          safe_cast(&Dbdict::buildIndex_fromBuildIndex),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;
        buildIndex_toBuildIndex(signal, op_ptr);
        return true;
      }
      break;
    case 2:
      jam();
      {
        Callback c = {
          safe_cast(&Dbdict::buildIndex_fromDropConstraint),
          op_ptr.p->op_key
        };
        op_ptr.p->m_callback = c;
        buildIndex_toDropConstraint(signal, op_ptr);
        return true;
      }
      break;
    default:
      ndbrequire(false);
      break;
    }
  }

  return false;
}

void
Dbdict::buildIndex_toCreateConstraint(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_toCreateConstraint");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  const TriggerTmpl& triggerTmpl = buildIndexPtr.p->m_triggerTmpl[0];

  CreateTrigReq* req = (CreateTrigReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, CreateTrigReq::CreateTriggerOnline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->indexId; // constraint is on index table
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;
  req->triggerNo = 0;
  req->forceTriggerId = RNIL;

  TriggerInfo::packTriggerInfo(req->triggerInfo, triggerTmpl.triggerInfo);

  req->receiverRef = 0;

  char triggerName[MAX_TAB_NAME_SIZE];
  sprintf(triggerName, triggerTmpl.nameFormat, impl_req->indexId);

  // name section
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  w.reset();
  w.add(DictTabInfo::TableName, triggerName);
  LinearSectionPtr ls_ptr[3];
  ls_ptr[0].p = buffer;
  ls_ptr[0].sz = w.getWordsUsed();

  ls_ptr[1].p = buildIndexPtr.p->m_attrMask.rep.data;
  ls_ptr[1].sz = buildIndexPtr.p->m_attrMask.getSizeInWords();

  sendSignal(reference(), GSN_CREATE_TRIG_REQ, signal,
             CreateTrigReq::SignalLength, JBB, ls_ptr, 2);
}

void
Dbdict::buildIndex_fromCreateConstraint(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  BuildIndexRecPtr buildIndexPtr;

  findSchemaOp(op_ptr, buildIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  D("buildIndex_fromCreateConstraint" << dec << V(op_key) << V(ret));

  if (ret == 0) {
    jam();
    const CreateTrigConf* conf =
      (const CreateTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    ndbrequire(buildIndexPtr.p->m_subOpIndex < buildIndexPtr.p->m_subOpCount);
    buildIndexPtr.p->m_subOpIndex += 1;
    createSubOps(signal, op_ptr);
  } else {
    const CreateTrigRef* ref =
      (const CreateTrigRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::buildIndex_toBuildIndex(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_toBuildIndex");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  BuildIndxReq* req = (BuildIndxReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, BuildIndxReq::SubOp);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->buildId = 0;
  req->buildKey = 0;
  req->tableId = impl_req->tableId;
  req->indexId = impl_req->indexId;
  req->indexType = impl_req->indexType;
  req->parallelism = impl_req->parallelism;

  sendSignal(reference(), GSN_BUILDINDXREQ, signal,
            BuildIndxReq::SignalLength, JBB);
}

void
Dbdict::buildIndex_fromBuildIndex(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("buildIndex_fromBuildIndex");

  SchemaOpPtr op_ptr;
  BuildIndexRecPtr buildIndexPtr;

  findSchemaOp(op_ptr, buildIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const BuildIndxConf* conf =
      (const BuildIndxConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    ndbrequire(buildIndexPtr.p->m_subOpIndex < buildIndexPtr.p->m_subOpCount);
    buildIndexPtr.p->m_subOpIndex += 1;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const BuildIndxRef* ref =
      (const BuildIndxRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::buildIndex_toDropConstraint(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_toDropConstraint");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  const TriggerTmpl& triggerTmpl = buildIndexPtr.p->m_triggerTmpl[0];

  DropTrigReq* req = (DropTrigReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, 0);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->indexId; // constraint is on index table
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;
  req->triggerNo = 0;
  req->triggerId = RNIL;

  // wl3600_todo use name now, connect by tree walk later

  char triggerName[MAX_TAB_NAME_SIZE];
  sprintf(triggerName, triggerTmpl.nameFormat, impl_req->indexId);

  // name section
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  w.reset();
  w.add(DictTabInfo::TableName, triggerName);
  LinearSectionPtr ls_ptr[3];
  ls_ptr[0].p = buffer;
  ls_ptr[0].sz = w.getWordsUsed();

  sendSignal(reference(), GSN_DROP_TRIG_REQ, signal,
             DropTrigReq::SignalLength, JBB, ls_ptr, 1);
}

void
Dbdict::buildIndex_fromDropConstraint(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  BuildIndexRecPtr buildIndexPtr;

  findSchemaOp(op_ptr, buildIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  D("buildIndex_fromDropConstraint" << dec << V(op_key) << V(ret));

  if (ret == 0) {
    const DropTrigConf* conf =
      (const DropTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    ndbrequire(buildIndexPtr.p->m_subOpIndex < buildIndexPtr.p->m_subOpCount);
    buildIndexPtr.p->m_subOpIndex += 1;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const DropTrigRef* ref =
      (const DropTrigRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::buildIndex_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  D("buildIndex_reply" << V(impl_req->indexId));

  if (!hasError(error)) {
    BuildIndxConf* conf = (BuildIndxConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = impl_req->tableId;
    conf->indexId = impl_req->indexId;
    conf->indexType = impl_req->indexType;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_BUILDINDXCONF, signal,
               BuildIndxConf::SignalLength, JBB);
  } else {
    jam();
    BuildIndxRef* ref = (BuildIndxRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    ref->tableId = impl_req->tableId;
    ref->indexId = impl_req->indexId;
    ref->indexType = impl_req->indexType;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_BUILDINDXREF, signal,
               BuildIndxRef::SignalLength, JBB);
  }
}

// BuildIndex: PREPARE

void
Dbdict::buildIndex_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  //const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  Uint32 requestInfo = op_ptr.p->m_requestInfo;
  bool noBuild = (requestInfo & DictSignal::RF_NO_BUILD);

  D("buildIndex_prepare" << hex << V(requestInfo));

  if (noBuild || !buildIndexPtr.p->m_doBuild) {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  buildIndex_toLocalBuild(signal, op_ptr);
}

void
Dbdict:: buildIndex_toLocalBuild(Signal* signal, SchemaOpPtr op_ptr)
{
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  D("buildIndex_toLocalBuild");

  BuildIndxImplReq* req = (BuildIndxImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = impl_req->requestType;
  req->transId = trans_ptr.p->m_transId;
  req->buildId = impl_req->buildId;
  req->buildKey = impl_req->buildKey;
  req->tableId = impl_req->tableId;
  req->indexId = impl_req->indexId;
  req->indexType = indexPtr.p->tableType;
  req->parallelism = impl_req->parallelism;

  /* All indexed columns must be in memory currently */
  req->requestType |= BuildIndxImplReq::RF_NO_DISK;

  Callback c = {
    safe_cast(&Dbdict::buildIndex_fromLocalBuild),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    jam();
    {
      LinearSectionPtr ls_ptr[3];
      ls_ptr[0].sz = buildIndexPtr.p->m_indexKeyList.sz;
      ls_ptr[0].p = buildIndexPtr.p->m_indexKeyList.id;
      ls_ptr[1].sz = buildIndexPtr.p->m_tableKeyList.sz;
      ls_ptr[1].p = buildIndexPtr.p->m_tableKeyList.id;

      sendSignal(TRIX_REF, GSN_BUILD_INDX_IMPL_REQ, signal,
                 BuildIndxImplReq::SignalLength, JBB, ls_ptr, 2);
    }
    break;
  case DictTabInfo::OrderedIndex:
    jam();
    if (op_ptr.p->m_requestInfo & BuildIndxReq::RF_BUILD_OFFLINE)
    {
      jam();
      req->requestType |= BuildIndxImplReq::RF_BUILD_OFFLINE;
    }

    {
      sendSignal(DBTUP_REF, GSN_BUILD_INDX_IMPL_REQ, signal,
                 BuildIndxImplReq::SignalLength, JBB);
    }
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::buildIndex_fromLocalBuild(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  BuildIndexRecPtr buildIndexPtr;
  findSchemaOp(op_ptr, buildIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  D("buildIndex_fromLocalBuild");

  if (ret == 0) {
    jam();
    sendTransConf(signal, op_ptr);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

// BuildIndex: COMMIT

void
Dbdict::buildIndex_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);

  D("buildIndex_commit" << *op_ptr.p);

  buildIndex_toLocalOnline(signal, op_ptr);
}

void
Dbdict::buildIndex_toLocalOnline(Signal* signal, SchemaOpPtr op_ptr)
{
  BuildIndexRecPtr buildIndexPtr;
  getOpRec(op_ptr, buildIndexPtr);
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  D("buildIndex_toLocalOnline");

  AlterIndxImplReq* req = (AlterIndxImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = impl_req->requestType;
  req->tableId = impl_req->tableId;
  req->tableVersion = 0; // not used
  req->indexId = impl_req->indexId;
  req->indexVersion = 0; // not used

  Callback c = {
    safe_cast(&Dbdict::buildIndex_fromLocalOnline),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    jam();
    {
      sendSignal(DBTC_REF, GSN_ALTER_INDX_IMPL_REQ, signal,
                 AlterIndxImplReq::SignalLength, JBB);
    }
    break;
  case DictTabInfo::OrderedIndex:
    jam();
    {
      sendSignal(DBTUX_REF, GSN_ALTER_INDX_IMPL_REQ, signal,
                 AlterIndxImplReq::SignalLength, JBB);
    }
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::buildIndex_fromLocalOnline(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  BuildIndexRecPtr buildIndexPtr;
  findSchemaOp(op_ptr, buildIndexPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const BuildIndxImplReq* impl_req = &buildIndexPtr.p->m_request;

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  D("buildIndex_fromLocalOnline");

  if (ret == 0) {
    jam();
    // set index online
    indexPtr.p->indexState = TableRecord::IS_ONLINE;
    sendTransConf(signal, op_ptr);
  } else {
    //wl3600_todo
    ndbrequire(false);
  }
}

// BuildIndex: COMPLETE

void
Dbdict::buildIndex_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// BuildIndex: ABORT

void
Dbdict::buildIndex_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_abortParse" << *op_ptr.p);
  // wl3600_todo
  sendTransConf(signal, op_ptr);
}

void
Dbdict::buildIndex_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("buildIndex_abortPrepare" << *op_ptr.p);

  // nothing to do, entire index table will be dropped
  sendTransConf(signal, op_ptr);
}

// BuildIndex: MISC

void
Dbdict::execBUILDINDXCONF(Signal* signal)
{
  jamEntry();
  const BuildIndxConf* conf = (const BuildIndxConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execBUILDINDXREF(Signal* signal)
{
  jamEntry();
  const BuildIndxRef* ref = (const BuildIndxRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execBUILD_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const BuildIndxImplConf* conf = (const BuildIndxImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execBUILD_INDX_IMPL_REF(Signal* signal)
{
  jamEntry();
  const BuildIndxImplRef* ref = (const BuildIndxImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

// BuildIndex: END

// MODULE: IndexStat

const Dbdict::OpInfo
Dbdict::IndexStatRec::g_opInfo = {
  { 'S', 'I', 'n', 0 },
  ~RT_DBDICT_INDEX_STAT,
  GSN_INDEX_STAT_IMPL_REQ,
  IndexStatImplReq::SignalLength,
  //
  &Dbdict::indexStat_seize,
  &Dbdict::indexStat_release,
  //
  &Dbdict::indexStat_parse,
  &Dbdict::indexStat_subOps,
  &Dbdict::indexStat_reply,
  //
  &Dbdict::indexStat_prepare,
  &Dbdict::indexStat_commit,
  &Dbdict::indexStat_complete,
  //
  &Dbdict::indexStat_abortParse,
  &Dbdict::indexStat_abortPrepare
};

bool
Dbdict::indexStat_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<IndexStatRec>(op_ptr);
}

void
Dbdict::indexStat_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<IndexStatRec>(op_ptr);
}

void
Dbdict::execINDEX_STAT_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const IndexStatReq req_copy =
    *(const IndexStatReq*)signal->getDataPtr();
  const IndexStatReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    IndexStatRecPtr indexStatPtr;
    IndexStatImplReq* impl_req;

    startClientReq(op_ptr, indexStatPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    // senderRef, senderData, requestType have been set already
    impl_req->transId = req->transId;
    impl_req->requestFlag = req->requestFlag;
    impl_req->indexId = req->indexId;
    impl_req->indexVersion = req->indexVersion;
    impl_req->tableId = req->tableId;
    impl_req->fragId = ZNIL;
    impl_req->fragCount = ZNIL;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  IndexStatRef* ref = (IndexStatRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_INDEX_STAT_REF, signal,
             IndexStatRef::SignalLength, JBB);
}

// IndexStat: PARSE

void
Dbdict::indexStat_parse(Signal* signal, bool master,
                        SchemaOpPtr op_ptr,
                        SectionHandle& handle, ErrorInfo& error)
{
  D("indexStat_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;
  Uint32 err;

  // get index
  TableRecordPtr indexPtr;
  err = check_read_obj(impl_req->indexId, trans_ptr.p->m_transId);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }
  bool ok = find_object(indexPtr, impl_req->indexId);
  if (!ok || !indexPtr.p->isOrderedIndex()) {
    jam();
    setError(error, IndexStatRef::InvalidIndex, __LINE__);
    return;
  }

  XSchemaFile* xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  const SchemaFile::TableEntry* te = getTableEntry(xsf, impl_req->indexId);
  if (te->m_tableState != SchemaFile::SF_CREATE &&
      te->m_tableState != SchemaFile::SF_IN_USE) {
    jam();
    setError(error, IndexStatRef::InvalidIndex, __LINE__);
    return;
  }

  // fragmentId is defined only in signals from DICT to TRIX,TUX
  if (impl_req->fragId != ZNIL) {
    jam();
    setError(error, IndexStatRef::InvalidRequest, __LINE__);
    return;
  }
  impl_req->fragCount = indexPtr.p->fragmentCount;

  switch (impl_req->requestType) {
  case IndexStatReq::RT_UPDATE_STAT:
    jam();
    // clean new samples, scan, clean old samples, start frag monitor
    indexStatPtr.p->m_subOpCount = 4;
    indexStatPtr.p->m_subOpIndex = 0;
    break;
  case IndexStatReq::RT_DELETE_STAT:
    jam();
    // stop frag monitor, delete head, clean all samples
    indexStatPtr.p->m_subOpCount = 3;
    indexStatPtr.p->m_subOpIndex = 0;
    break;
  case IndexStatReq::RT_SCAN_FRAG:
  case IndexStatReq::RT_CLEAN_NEW:
  case IndexStatReq::RT_CLEAN_OLD:
  case IndexStatReq::RT_CLEAN_ALL:
  case IndexStatReq::RT_DROP_HEAD:
  case IndexStatReq::RT_START_MON:
  case IndexStatReq::RT_STOP_MON:
    jam();
    // sub-operations can be invoked only by DICT
    if (master && refToBlock(op_ptr.p->m_clientRef) != DBDICT) {
      jam();
      setError(error, IndexStatRef::InvalidRequest, __LINE__);
      return;
    }
    indexStatPtr.p->m_subOpCount = 0;
    indexStatPtr.p->m_subOpIndex = 0;
    break;
  default:
    jam();
    setError(error, IndexStatRef::InvalidRequest, __LINE__);
    return;
  }
}

bool
Dbdict::indexStat_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("indexStat_subOps" << V(op_ptr.i) << V(*op_ptr.p));

  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;

  const Uint32 subOpIndex = indexStatPtr.p->m_subOpIndex;
  const Uint32 subOpCount = indexStatPtr.p->m_subOpCount;
  if (subOpIndex >= subOpCount) {
    jam();
    ndbrequire(subOpIndex == subOpCount);
    return false;
  }

  Uint32 requestType = 0;

  switch (impl_req->requestType) {
  case IndexStatReq::RT_UPDATE_STAT:
    if (subOpIndex == 0)
      requestType = IndexStatReq::RT_CLEAN_NEW;
    else if (subOpIndex == 1)
      requestType = IndexStatReq::RT_SCAN_FRAG;
    else if (subOpIndex == 2)
      requestType = IndexStatReq::RT_CLEAN_OLD;
    else if (subOpIndex == 3)
      requestType = IndexStatReq::RT_START_MON;
    break;

  case IndexStatReq::RT_DELETE_STAT:
    jam();
    if (subOpIndex == 0)
      requestType = IndexStatReq::RT_STOP_MON;
    else if (subOpIndex == 1)
      requestType = IndexStatReq::RT_DROP_HEAD;
    else if (subOpIndex == 2)
      requestType = IndexStatReq::RT_CLEAN_ALL;
    break;
  };

  ndbrequire(requestType != 0);
  Callback c = {
    safe_cast(&Dbdict::indexStat_fromIndexStat),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;
  indexStat_toIndexStat(signal, op_ptr, requestType);
  return true;
}

void
Dbdict::indexStat_toIndexStat(Signal* signal, SchemaOpPtr op_ptr,
                              Uint32 requestType)
{
  D("indexStat_toIndexStat");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;

  IndexStatReq* req = (IndexStatReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, requestType);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->requestFlag = 0;
  req->indexId = impl_req->indexId;
  req->indexVersion = indexPtr.p->tableVersion;
  req->tableId = impl_req->tableId;

  sendSignal(reference(), GSN_INDEX_STAT_REQ,
             signal, IndexStatReq::SignalLength, JBB);
}

void
Dbdict::indexStat_fromIndexStat(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  D("indexStat_fromIndexStat");

  SchemaOpPtr op_ptr;
  IndexStatRecPtr indexStatPtr;

  findSchemaOp(op_ptr, indexStatPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const IndexStatConf* conf =
      (const IndexStatConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    ndbrequire(indexStatPtr.p->m_subOpIndex < indexStatPtr.p->m_subOpCount);
    indexStatPtr.p->m_subOpIndex += 1;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const IndexStatRef* ref =
      (const IndexStatRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::indexStat_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;

  D("indexStat_reply" << V(impl_req->indexId));

  if (!hasError(error)) {
    IndexStatConf* conf = (IndexStatConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_INDEX_STAT_CONF, signal,
               IndexStatConf::SignalLength, JBB);
  } else {
    jam();
    IndexStatRef* ref = (IndexStatRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_INDEX_STAT_REF, signal,
               IndexStatRef::SignalLength, JBB);
  }
}

// IndexStat: PREPARE

void
Dbdict::indexStat_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;

  D("indexStat_prepare" << V(*op_ptr.p));

  if (impl_req->requestType == IndexStatReq::RT_UPDATE_STAT ||
      impl_req->requestType == IndexStatReq::RT_DELETE_STAT) {
    // the main op of stat update or delete does nothing
    sendTransConf(signal, op_ptr);
    return;
  }

  indexStat_toLocalStat(signal, op_ptr);
}

static
bool
do_action(const NdbNodeBitmask & mask, const Uint16 list[], Uint16 ownId)
{
  for (Uint32 i = 0; i < MAX_REPLICAS; i++)
  {
    if (mask.get(list[i]))
    {
      if (list[i] == ownId)
        return true;
      return false;
    }
  }
  return false;
}

void
Dbdict::indexStat_toLocalStat(Signal* signal, SchemaOpPtr op_ptr)
{
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  D("indexStat_toLocalStat");

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, impl_req->indexId);
  ndbrequire(ok && indexPtr.p->isOrderedIndex());

  Callback c = {
    safe_cast(&Dbdict::indexStat_fromLocalStat),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  IndexStatImplReq* req = (IndexStatImplReq*)signal->getDataPtrSend();
  *req = *impl_req;
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  ndbrequire(req->fragId == ZNIL);
  ndbrequire(indexPtr.p->indexStatFragId != ZNIL);
  BlockReference ref = 0;

  switch (impl_req->requestType) {
  case IndexStatReq::RT_SCAN_FRAG:
    trans_ptr.p->m_abort_on_node_fail = true;
    req->fragId = indexPtr.p->indexStatFragId;
    if (!do_action(trans_ptr.p->m_nodes, indexPtr.p->indexStatNodes,
                   getOwnNodeId()))
    {
      jam();
      D("skip" << V(impl_req->requestType));
      execute(signal, c, 0);
      return;
    }
    ref = TRIX_REF;
    break;

  case IndexStatReq::RT_CLEAN_NEW:
  case IndexStatReq::RT_CLEAN_OLD:
  case IndexStatReq::RT_CLEAN_ALL:
    /*
     * Index stats "v4" does scan deletes via TRIX-SUMA.  But SUMA does
     * only local scans so do it on all nodes.
     */
    req->fragId = ZNIL;
    ref = TRIX_REF;
    break;

  case IndexStatReq::RT_DROP_HEAD:
    req->fragId = indexPtr.p->indexStatFragId;
    if (!do_action(trans_ptr.p->m_nodes, indexPtr.p->indexStatNodes,
                   getOwnNodeId()))
    {
      jam();
      D("skip" << V(impl_req->requestType));
      execute(signal, c, 0);
      return;
    }
    ref = TRIX_REF;
    break;

  case IndexStatReq::RT_START_MON:
    req->fragId = indexPtr.p->indexStatFragId;
    if (!do_action(trans_ptr.p->m_nodes, indexPtr.p->indexStatNodes,
                   getOwnNodeId()))
    {
      jam();
      req->fragId = ZNIL;
    }
    ref = DBTUX_REF;
    break;

  case IndexStatReq::RT_STOP_MON:
    req->fragId = ZNIL;
    ref = DBTUX_REF;
    break;

  default:
    ndbrequire(false); // only sub-ops seen here
    break;
  }

  sendSignal(ref, GSN_INDEX_STAT_IMPL_REQ,
             signal, IndexStatImplReq::SignalLength, JBB);
}

void
Dbdict::indexStat_fromLocalStat(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  IndexStatRecPtr indexStatPtr;
  findSchemaOp(op_ptr, indexStatPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const IndexStatImplReq* impl_req = &indexStatPtr.p->m_request;

  if (ret != 0) {
    jam();
    if (impl_req->requestType != IndexStatReq::RT_CLEAN_OLD &&
        impl_req->requestType != IndexStatReq::RT_CLEAN_ALL &&
        impl_req->requestType != IndexStatReq::RT_DROP_HEAD) {
      jam();
      setError(op_ptr, ret, __LINE__);
      sendTransRef(signal, op_ptr);
      return;
    }
    D("ignore failed index stat cleanup");
  }
  sendTransConf(signal, op_ptr);
}

// IndexStat: COMMIT

void
Dbdict::indexStat_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  D("indexStat_commit" << *op_ptr.p);
  sendTransConf(signal, op_ptr);
}

// IndexStat: COMPLETE

void
Dbdict::indexStat_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  IndexStatRecPtr indexStatPtr;
  getOpRec(op_ptr, indexStatPtr);
  D("indexStat_complete" << *op_ptr.p);
  sendTransConf(signal, op_ptr);
}

// IndexStat: ABORT

void
Dbdict::indexStat_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("indexStat_abortParse" << *op_ptr.p);
  // wl3600_todo
  sendTransConf(signal, op_ptr);
}

void
Dbdict::indexStat_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("indexStat_abortPrepare" << *op_ptr.p);

  // nothing to do, entire index table will be dropped
  sendTransConf(signal, op_ptr);
}

// IndexStat: MISC

void
Dbdict::execINDEX_STAT_CONF(Signal* signal)
{
  jamEntry();
  const IndexStatConf* conf = (const IndexStatConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execINDEX_STAT_REF(Signal* signal)
{
  jamEntry();
  const IndexStatRef* ref = (const IndexStatRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execINDEX_STAT_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const IndexStatImplConf* conf = (const IndexStatImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execINDEX_STAT_IMPL_REF(Signal* signal)
{
  jamEntry();
  const IndexStatImplRef* ref = (const IndexStatImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

// IndexStat: background processing

/*
 * Receive report that an index needs stats update.  Request to
 * non-master is sent to master.  Index is marked for stats update.
 * Invalid request is simply ignored.  Master-NF really need not be
 * handled but could be, by broadcasting all reports to all DICTs.
 */
void
Dbdict::execINDEX_STAT_REP(Signal* signal)
{
  const IndexStatRep* rep = (const IndexStatRep*)signal->getDataPtr();

  // non-master
  if (c_masterNodeId != getOwnNodeId()) {
    jam();
    BlockReference dictRef = calcDictBlockRef(c_masterNodeId);
    sendSignal(dictRef, GSN_INDEX_STAT_REP, signal,
               IndexStatRep::SignalLength, JBB);
    return;
  }

  // check
  TableRecordPtr indexPtr;
  if (rep->indexId >= c_noOfMetaTables) {
    jam();
    return;
  }
  XSchemaFile* xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  const SchemaFile::TableEntry* te = getTableEntry(xsf, rep->indexId);
  if (te->m_tableState != SchemaFile::SF_IN_USE) {
    jam();
    return;
  }
  bool ok = find_object(indexPtr, rep->indexId);
  if (!ok)
  {
    jam();
    return;
  }
  if (rep->indexVersion != 0 &&
      rep->indexVersion != indexPtr.p->tableVersion) {
    jam();
    return;
  }
  if (!indexPtr.p->isOrderedIndex()) {
    jam();
    return;
  }
  if (rep->requestType != IndexStatRep::RT_UPDATE_REQ) {
    jam();
    return;
  }

  D("index stat: " << copyRope<MAX_TAB_NAME_SIZE>(indexPtr.p->tableName)
    << " request type:" << rep->requestType);

  infoEvent("DICT: index %u stats auto-update requested", rep->indexId);
  indexPtr.p->indexStatBgRequest = rep->requestType;
}

void
Dbdict::indexStatBg_process(Signal* signal)
{
  if (!c_indexStatAutoUpdate ||
      c_masterNodeId != getOwnNodeId() ||
      getNodeState().startLevel != NodeState::SL_STARTED) {
    jam();
    indexStatBg_sendContinueB(signal);
    return;
  }

  D("indexStatBg_process" << V(c_indexStatBgId));
  const uint maxloop = 32;
  uint loop;
  for (loop = 0; loop < maxloop; loop++, c_indexStatBgId++) {
    jam();
    c_indexStatBgId %= c_noOfMetaTables;

    // check
    TableRecordPtr indexPtr;
    XSchemaFile* xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    const SchemaFile::TableEntry* te = getTableEntry(xsf, c_indexStatBgId);
    if (te->m_tableState != SchemaFile::SF_IN_USE) {
      jam();
      continue;
    }
    bool ok = find_object(indexPtr, c_indexStatBgId);
    if (!ok || !indexPtr.p->isOrderedIndex()) {
      jam();
      continue;
    }
    if (indexPtr.p->indexStatBgRequest == 0) {
      jam();
      continue;
    }
    ndbrequire(indexPtr.p->indexStatBgRequest == IndexStatRep::RT_UPDATE_REQ);

    TxHandlePtr tx_ptr;
    if (!seizeTxHandle(tx_ptr)) {
      jam();
      return; // wait for one
    }
    Callback c = {
      safe_cast(&Dbdict::indexStatBg_fromBeginTrans),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;
    beginSchemaTrans(signal, tx_ptr);
    return;
  }

  indexStatBg_sendContinueB(signal);
}

void
Dbdict::indexStatBg_fromBeginTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("indexStatBg_fromBeginTrans" << V(c_indexStatBgId) << V(tx_key) << V(ret));

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (ret != 0) {
    jam();
    indexStatBg_sendContinueB(signal);
    return;
  }

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, c_indexStatBgId);
  ndbrequire(ok);

  Callback c = {
    safe_cast(&Dbdict::indexStatBg_fromIndexStat),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  IndexStatReq* req = (IndexStatReq*)signal->getDataPtrSend();
  req->clientRef = reference();
  req->clientData = tx_ptr.p->tx_key;
  req->transId = tx_ptr.p->m_transId;
  req->transKey = tx_ptr.p->m_transKey;
  req->requestInfo = IndexStatReq::RT_UPDATE_STAT;
  req->requestFlag = 0;
  req->indexId = c_indexStatBgId;
  req->indexVersion = indexPtr.p->tableVersion;
  req->tableId = indexPtr.p->primaryTableId;
  sendSignal(reference(), GSN_INDEX_STAT_REQ,
             signal, IndexStatReq::SignalLength, JBB);
}

void
Dbdict::indexStatBg_fromIndexStat(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("indexStatBg_fromIndexStat" << V(c_indexStatBgId) << V(tx_key) << (ret));

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  if (ret != 0) {
    jam();
    setError(tx_ptr.p->m_error, ret, __LINE__);
    warningEvent("DICT: index %u stats auto-update error: %d", c_indexStatBgId, ret);
  }

  Callback c = {
    safe_cast(&Dbdict::indexStatBg_fromEndTrans),
    tx_ptr.p->tx_key
  };
  tx_ptr.p->m_callback = c;

  Uint32 flags = 0;
  if (hasError(tx_ptr.p->m_error))
    flags |= SchemaTransEndReq::SchemaTransAbort;
  endSchemaTrans(signal, tx_ptr, flags);
}

void
Dbdict::indexStatBg_fromEndTrans(Signal* signal, Uint32 tx_key, Uint32 ret)
{
  D("indexStatBg_fromEndTrans" << V(c_indexStatBgId) << V(tx_key) << (ret));

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, tx_key);
  ndbrequire(!tx_ptr.isNull());

  TableRecordPtr indexPtr;
  bool ok = find_object(indexPtr, c_indexStatBgId);

  if (ret != 0) {
    jam();
    // skip over but leave the request on
    warningEvent("DICT: index %u stats auto-update error: %d", c_indexStatBgId, ret);
  } else {
    jam();
    ndbrequire(ok);
    // mark request done
    indexPtr.p->indexStatBgRequest = 0;
    infoEvent("DICT: index %u stats auto-update done", c_indexStatBgId);
  }

  releaseTxHandle(tx_ptr);
  c_indexStatBgId++;
  indexStatBg_sendContinueB(signal);
}

void
Dbdict::indexStatBg_sendContinueB(Signal* signal)
{
  D("indexStatBg_sendContinueB" << V(c_indexStatBgId));
  signal->theData[0] = ZINDEX_STAT_BG_PROCESS;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
}

// IndexStat: END

// MODULE: CopyData

const Dbdict::OpInfo
Dbdict::CopyDataRec::g_opInfo = {
  { 'D', 'C', 'D', 0 },
  ~RT_DBDICT_COPY_DATA,
  GSN_COPY_DATA_IMPL_REQ,
  CopyDataImplReq::SignalLength,

  //
  &Dbdict::copyData_seize,
  &Dbdict::copyData_release,
  //
  &Dbdict::copyData_parse,
  &Dbdict::copyData_subOps,
  &Dbdict::copyData_reply,
  //
  &Dbdict::copyData_prepare,
  &Dbdict::copyData_commit,
  &Dbdict::copyData_complete,
  //
  &Dbdict::copyData_abortParse,
  &Dbdict::copyData_abortPrepare
};

bool
Dbdict::copyData_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CopyDataRec>(op_ptr);
}

void
Dbdict::copyData_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CopyDataRec>(op_ptr);
}

void
Dbdict::execCOPY_DATA_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CopyDataReq req_copy =
    *(const CopyDataReq*)signal->getDataPtr();
  const CopyDataReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CopyDataRecPtr copyDataPtr;
    CopyDataImplReq* impl_req;

    startClientReq(op_ptr, copyDataPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->srcTableId = req->srcTableId;
    impl_req->dstTableId = req->dstTableId;
    impl_req->srcFragments = req->srcFragments;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CopyDataRef* ref = (CopyDataRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = req->clientData;
  ref->transId = req->transId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_COPY_DATA_REF, signal,
             CopyDataRef::SignalLength, JBB);
}

// CopyData: PARSE

void
Dbdict::copyData_parse(Signal* signal, bool master,
                       SchemaOpPtr op_ptr,
                       SectionHandle& handle, ErrorInfo& error)
{
  D("copyData_parse");

  /**
   * Nothing here...
   */
}

bool
Dbdict::copyData_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("copyData_subOps" << V(op_ptr.i) << *op_ptr.p);

  return false;
}

void
Dbdict::copyData_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  CopyDataRecPtr copyDataPtr;
  getOpRec(op_ptr, copyDataPtr);
  //const CopyDataImplReq* impl_req = &copyDataPtr.p->m_request;

  if (!hasError(error)) {
    CopyDataConf* conf = (CopyDataConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_COPY_DATA_CONF, signal,
               CopyDataConf::SignalLength, JBB);
  } else {
    jam();
    CopyDataRef* ref = (CopyDataRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_COPY_DATA_REF, signal,
               CopyDataRef::SignalLength, JBB);
  }
}

// CopyData: PREPARE

void
Dbdict::copyData_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CopyDataRecPtr copyDataPtr;
  getOpRec(op_ptr, copyDataPtr);
  const CopyDataImplReq* impl_req = &copyDataPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  CopyDataImplReq* req = (CopyDataImplReq*)signal->getDataPtrSend();
  * req = * impl_req;
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->srcFragments = 0; // All

  Callback c = {
    safe_cast(&Dbdict::copyData_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  Uint32 cnt =0;
  Uint32 tmp[MAX_ATTRIBUTES_IN_TABLE];
  bool tabHasDiskCols = false;
  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, impl_req->srcTableId);
  ndbrequire(ok);
  {
    LocalAttributeRecord_list alist(c_attributeRecordPool,
                                           tabPtr.p->m_attributes);
    AttributeRecordPtr attrPtr;
    for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr))
    {
      if (AttributeDescriptor::getPrimaryKey(attrPtr.p->attributeDescriptor))
        tmp[cnt++] = attrPtr.p->attributeId;
    }
    for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr))
    {
      if (!AttributeDescriptor::getPrimaryKey(attrPtr.p->attributeDescriptor))
      {
        tmp[cnt++] = attrPtr.p->attributeId;

        if (AttributeDescriptor::getDiskBased(attrPtr.p->attributeDescriptor))
          tabHasDiskCols = true;
      }
    }
  }

  /* Request Tup-ordered copy when we have disk columns for efficiency */
  if (tabHasDiskCols)
  {
    jam();
    req->requestInfo |= CopyDataReq::TupOrder;
  }

  LinearSectionPtr ls_ptr[3];
  ls_ptr[0].sz = cnt;
  ls_ptr[0].p = tmp;

  sendSignal(TRIX_REF, GSN_COPY_DATA_IMPL_REQ, signal,
             CopyDataImplReq::SignalLength, JBB, ls_ptr, 1);
}

void
Dbdict::copyData_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CopyDataRecPtr copyDataPtr;
  findSchemaOp(op_ptr, copyDataPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (ERROR_INSERTED(6214))
  {
    CLEAR_ERROR_INSERT_VALUE;
    ret = 1;
  }

  if (ret == 0) {
    jam();
    sendTransConf(signal, op_ptr);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}


// CopyData: COMMIT

void
Dbdict::copyData_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  sendTransConf(signal, op_ptr);
}

// CopyData: COMPLETE

void
Dbdict::copyData_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CopyDataRecPtr copyDataPtr;
  getOpRec(op_ptr, copyDataPtr);
  const CopyDataImplReq* impl_req = &copyDataPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  CopyDataImplReq* req = (CopyDataImplReq*)signal->getDataPtrSend();
  * req = * impl_req;
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->requestType = CopyDataImplReq::ReorgDelete;

  Callback c = {
    safe_cast(&Dbdict::copyData_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  Uint32 cnt =0;
  Uint32 tmp[MAX_ATTRIBUTES_IN_TABLE];
  bool tabHasDiskCols = false;
  TableRecordPtr tabPtr;
  bool ok = find_object(tabPtr, impl_req->srcTableId);
  ndbrequire(ok);
  {
    LocalAttributeRecord_list alist(c_attributeRecordPool,
                                           tabPtr.p->m_attributes);
    AttributeRecordPtr attrPtr;
    for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr))
    {
      if (AttributeDescriptor::getPrimaryKey(attrPtr.p->attributeDescriptor))
        tmp[cnt++] = attrPtr.p->attributeId;
      else
      {
        if (AttributeDescriptor::getDiskBased(attrPtr.p->attributeDescriptor))
          tabHasDiskCols = true;
      }
    }
  }

  /* Request Tup-ordered delete when we have disk columns for efficiency */
  if (tabHasDiskCols)
  {
    jam();
    req->requestInfo |= CopyDataReq::TupOrder;
  }

  LinearSectionPtr ls_ptr[3];
  ls_ptr[0].sz = cnt;
  ls_ptr[0].p = tmp;

  sendSignal(TRIX_REF, GSN_COPY_DATA_IMPL_REQ, signal,
             CopyDataImplReq::SignalLength, JBB, ls_ptr, 1);
}

// CopyData: ABORT

void
Dbdict::copyData_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("copyData_abortParse" << *op_ptr.p);
  // wl3600_todo
  sendTransConf(signal, op_ptr);
}

void
Dbdict::copyData_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("copyData_abortPrepare" << *op_ptr.p);

  // nothing to do, entire index table will be dropped
  sendTransConf(signal, op_ptr);
}

void
Dbdict::execCOPY_DATA_CONF(Signal* signal)
{
  jamEntry();
  const CopyDataConf* conf = (const CopyDataConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execCOPY_DATA_REF(Signal* signal)
{
  jamEntry();
  const CopyDataRef* ref = (const CopyDataRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCOPY_DATA_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const CopyDataImplConf* conf = (const CopyDataImplConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execCOPY_DATA_IMPL_REF(Signal* signal)
{
  jamEntry();
  const CopyDataImplRef* ref = (const CopyDataImplRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}


// CopyData: END


/*****************************************************
 *
 * MODULE: Util signalling
 *
 *****************************************************/

int
Dbdict::sendSignalUtilReq(Callback *pcallback,
			  BlockReference ref,
			  GlobalSignalNumber gsn,
			  Signal* signal,
			  Uint32 length,
			  JobBufferLevel jbuf,
			  LinearSectionPtr ptr[3],
			  Uint32 noOfSections)
{
  jam();
  EVENT_TRACE;
  OpSignalUtilPtr utilRecPtr;

  // Seize a Util Send record
  if (!c_opSignalUtil.seize(utilRecPtr)) {
    // Failed to allocate util record
    return -1;
  }
  utilRecPtr.p->m_callback = *pcallback;

  // should work for all util signal classes
  UtilPrepareReq *req = (UtilPrepareReq*)signal->getDataPtrSend();
  utilRecPtr.p->m_userData = req->getSenderData();
  req->setSenderData(utilRecPtr.i);

  if (ptr) {
    jam();
    sendSignal(ref, gsn, signal, length, jbuf, ptr, noOfSections);
  } else {
    jam();
    sendSignal(ref, gsn, signal, length, jbuf);
  }

  return 0;
}

int
Dbdict::recvSignalUtilReq(Signal* signal, Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  UtilPrepareConf * const req = (UtilPrepareConf*)signal->getDataPtr();
  OpSignalUtilPtr utilRecPtr;
  utilRecPtr.i = req->getSenderData();
  if ((utilRecPtr.p = c_opSignalUtil.getPtr(utilRecPtr.i)) == NULL) {
    jam();
    return -1;
  }

  req->setSenderData(utilRecPtr.p->m_userData);
  Callback c = utilRecPtr.p->m_callback;
  c_opSignalUtil.release(utilRecPtr);

  execute(signal, c, returnCode);
  return 0;
}

void Dbdict::execUTIL_PREPARE_CONF(Signal *signal)
{
  jamEntry();
  EVENT_TRACE;
  ndbrequire(recvSignalUtilReq(signal, 0) == 0);
}

void
Dbdict::execUTIL_PREPARE_REF(Signal *signal)
{
  jamEntry();
  const UtilPrepareRef * ref = CAST_CONSTPTR(UtilPrepareRef,
                                             signal->getDataPtr());
  Uint32 code = ref->errorCode;
  if (code == UtilPrepareRef::DICT_TAB_INFO_ERROR)
    code = ref->dictErrCode;
  EVENT_TRACE;
  ndbrequire(recvSignalUtilReq(signal, code) == 0);
}

void Dbdict::execUTIL_EXECUTE_CONF(Signal *signal)
{
  jamEntry();
  EVENT_TRACE;
  ndbrequire(recvSignalUtilReq(signal, 0) == 0);
}

void Dbdict::execUTIL_EXECUTE_REF(Signal *signal)
{
  jamEntry();
  EVENT_TRACE;

#ifdef EVENT_DEBUG
  UtilExecuteRef * ref = (UtilExecuteRef *)signal->getDataPtrSend();

  ndbout_c("execUTIL_EXECUTE_REF");
  ndbout_c("senderData %u",ref->getSenderData());
  ndbout_c("errorCode %u",ref->getErrorCode());
  ndbout_c("TCErrorCode %u",ref->getTCErrorCode());
#endif

  ndbrequire(recvSignalUtilReq(signal, 1) == 0);
}
void Dbdict::execUTIL_RELEASE_CONF(Signal *signal)
{
  jamEntry();
  EVENT_TRACE;
  ndbrequire(false);
  ndbrequire(recvSignalUtilReq(signal, 0) == 0);
}
void Dbdict::execUTIL_RELEASE_REF(Signal *signal)
{
  jamEntry();
  EVENT_TRACE;
  ndbrequire(false);
  ndbrequire(recvSignalUtilReq(signal, 1) == 0);
}

/**
 * MODULE: Create event
 *
 * Create event in DICT.
 *
 *
 * Request type in CREATE_EVNT signals:
 *
 * Signalflow see Dbdict.txt
 *
 */

/*****************************************************************
 *
 * Systable stuff
 *
 */

static const
Uint32
sysTab_NDBEVENTS_0_szs[] =
{
  sizeof(((sysTab_NDBEVENTS_0*)0)->NAME),
  sizeof(((sysTab_NDBEVENTS_0*)0)->EVENT_TYPE),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLEID),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLEVERSION),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLE_NAME),
  sizeof(((sysTab_NDBEVENTS_0*)0)->ATTRIBUTE_MASK),
  sizeof(((sysTab_NDBEVENTS_0*)0)->SUBID),
  sizeof(((sysTab_NDBEVENTS_0*)0)->SUBKEY),
  sizeof(((sysTab_NDBEVENTS_0*)0)->ATTRIBUTE_MASK2)
};

static const
UintPtr
sysTab_NDBEVENTS_0_offsets[] =
{
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->NAME),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->EVENT_TYPE),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->TABLEID),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->TABLEVERSION),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->TABLE_NAME),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->ATTRIBUTE_MASK),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->SUBID),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->SUBKEY),
  (UintPtr)&(((sysTab_NDBEVENTS_0*)0)->ATTRIBUTE_MASK2)
};

void
Dbdict::prepareTransactionEventSysTable (Callback *pcallback,
					 Signal* signal,
					 Uint32 senderData,
					 UtilPrepareReq::OperationTypeValue prepReq)
{
  // find table id for event system table
  DictObject * opj_ptr_p = get_object(EVENT_SYSTEM_TABLE_NAME,
				      sizeof(EVENT_SYSTEM_TABLE_NAME));

  ndbrequire(opj_ptr_p != 0);
  TableRecordPtr tablePtr;
  c_tableRecordPool_.getPtr(tablePtr, opj_ptr_p->m_object_ptr_i);
  ndbrequire(tablePtr.i != RNIL); // system table must exist

  Uint32 tableId = tablePtr.p->tableId; /* System table */
  Uint32 noAttr = tablePtr.p->noOfAttributes;
  if (noAttr > EVENT_SYSTEM_TABLE_LENGTH)
  {
    jam();
    noAttr = EVENT_SYSTEM_TABLE_LENGTH;
  }

  switch (prepReq) {
  case UtilPrepareReq::Update:
  case UtilPrepareReq::Insert:
  case UtilPrepareReq::Write:
  case UtilPrepareReq::Read:
    jam();
    break;
  case UtilPrepareReq::Delete:
    jam();
    noAttr = 1; // only involves Primary key which should be the first
    break;
  }
  prepareUtilTransaction(pcallback, signal, senderData, tableId, NULL,
		      prepReq, noAttr, NULL, NULL);
}

void
Dbdict::prepareUtilTransaction(Callback *pcallback,
			       Signal* signal,
			       Uint32 senderData,
			       Uint32 tableId,
			       const char* tableName,
			       UtilPrepareReq::OperationTypeValue prepReq,
			       Uint32 noAttr,
			       Uint32 attrIds[],
			       const char *attrNames[])
{
  jam();
  EVENT_TRACE;

  UtilPrepareReq * utilPrepareReq =
    (UtilPrepareReq *)signal->getDataPtrSend();

  utilPrepareReq->setSenderRef(reference());
  utilPrepareReq->setSenderData(senderData);

  const Uint32 pageSizeInWords = 128;
  Uint32 propPage[pageSizeInWords];
  LinearWriter w(&propPage[0],128);
  w.first();
  w.add(UtilPrepareReq::NoOfOperations, 1);
  w.add(UtilPrepareReq::OperationType, prepReq);
  if (tableName) {
    jam();
    w.add(UtilPrepareReq::TableName, tableName);
  } else {
    jam();
    w.add(UtilPrepareReq::TableId, tableId);
  }
  for(Uint32 i = 0; i < noAttr; i++)
    if (tableName) {
      jam();
      w.add(UtilPrepareReq::AttributeName, attrNames[i]);
    } else {
      if (attrIds) {
	jam();
	w.add(UtilPrepareReq::AttributeId, attrIds[i]);
      } else {
	jam();
	w.add(UtilPrepareReq::AttributeId, i);
      }
    }
#ifdef EVENT_DEBUG
  // Debugging
  SimplePropertiesLinearReader reader(propPage, w.getWordsUsed());
  printf("Dict::prepareInsertTransactions: Sent SimpleProperties:\n");
  reader.printAll(ndbout);
#endif

  struct LinearSectionPtr sectionsPtr[UtilPrepareReq::NoOfSections];
  sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].p = propPage;
  sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].sz = w.getWordsUsed();

  sendSignalUtilReq(pcallback, DBUTIL_REF, GSN_UTIL_PREPARE_REQ, signal,
		    UtilPrepareReq::SignalLength, JBB,
		    sectionsPtr, UtilPrepareReq::NoOfSections);
}

/*****************************************************************
 *
 * CREATE_EVNT_REQ has three types RT_CREATE, RT_GET (from user)
 * and RT_DICT_AFTER_GET send from master DICT to slaves
 *
 * This function just dscpaches these to
 *
 * createEvent_RT_USER_CREATE
 * createEvent_RT_USER_GET
 * createEvent_RT_DICT_AFTER_GET
 *
 * repectively
 *
 */

void
Dbdict::execCREATE_EVNT_REQ(Signal* signal)
{
  jamEntry();

#if 0
  {
    SafeCounterHandle handle;
    {
      SafeCounter tmp(c_counterMgr, handle);
      tmp.init<CreateEvntRef>(CMVMI, GSN_DUMP_STATE_ORD, /* senderData */ 13);
      tmp.clearWaitingFor();
      tmp.setWaitingFor(3);
      ndbrequire(!tmp.done());
      ndbout_c("Allocted");
    }
    ndbrequire(!handle.done());
    {
      SafeCounter tmp(c_counterMgr, handle);
      tmp.clearWaitingFor(3);
      ndbrequire(tmp.done());
      ndbout_c("Deallocted");
    }
    ndbrequire(handle.done());
  }
  {
    NodeBitmask nodes;
    nodes.clear();

    nodes.set(2);
    nodes.set(3);
    nodes.set(4);
    nodes.set(5);

    {
      Uint32 i = 0;
      while((i = nodes.find(i)) != NodeBitmask::NotFound){
	ndbout_c("1 Node id = %u", i);
	i++;
      }
    }

    NodeReceiverGroup rg(DBDICT, nodes);
    RequestTracker rt2;
    ndbrequire(rt2.done());
    ndbrequire(!rt2.hasRef());
    ndbrequire(!rt2.hasConf());
    rt2.init<CreateEvntRef>(c_counterMgr, rg, GSN_CREATE_EVNT_REF, 13);

    RequestTracker rt3;
    rt3.init<CreateEvntRef>(c_counterMgr, rg, GSN_CREATE_EVNT_REF, 13);

    ndbrequire(!rt2.done());
    ndbrequire(!rt3.done());

    rt2.reportRef(c_counterMgr, 2);
    rt3.reportConf(c_counterMgr, 2);

    ndbrequire(!rt2.done());
    ndbrequire(!rt3.done());

    rt2.reportConf(c_counterMgr, 3);
    rt3.reportConf(c_counterMgr, 3);

    ndbrequire(!rt2.done());
    ndbrequire(!rt3.done());

    rt2.reportConf(c_counterMgr, 4);
    rt3.reportConf(c_counterMgr, 4);

    ndbrequire(!rt2.done());
    ndbrequire(!rt3.done());

    rt2.reportConf(c_counterMgr, 5);
    rt3.reportConf(c_counterMgr, 5);

    ndbrequire(rt2.done());
    ndbrequire(rt3.done());
  }
#endif

  CreateEvntReq *req = (CreateEvntReq*)signal->getDataPtr();

  if (! assembleFragments(signal)) {
    jam();
    return;
  }

  const CreateEvntReq::RequestType requestType = req->getRequestType();
  const Uint32                     requestFlag = req->getRequestFlag();
  SectionHandle handle(this, signal);

  if (refToBlock(signal->senderBlockRef()) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    jam();
    releaseSections(handle);

    CreateEvntRef * ref = (CreateEvntRef *)signal->getDataPtrSend();
    ref->setUserRef(reference());
    ref->setErrorCode(CreateEvntRef::NotMaster);
    ref->setErrorLine(__LINE__);
    ref->setErrorNode(reference());
    ref->setMasterNode(c_masterNodeId);
    sendSignal(signal->senderBlockRef(), GSN_CREATE_EVNT_REF, signal,
	       CreateEvntRef::SignalLength2, JBB);
    return;
  }

  OpCreateEventPtr evntRecPtr;
  // Seize a Create Event record
  if (!c_opCreateEvent.seize(evntRecPtr)) {
    // Failed to allocate event record
    jam();
    releaseSections(handle);

    CreateEvntRef * ret = (CreateEvntRef *)signal->getDataPtrSend();
    ret->senderRef = reference();
    ret->setErrorCode(747);
    ret->setErrorLine(__LINE__);
    ret->setErrorNode(reference());
    sendSignal(signal->senderBlockRef(), GSN_CREATE_EVNT_REF, signal,
	       CreateEvntRef::SignalLength, JBB);
    return;
  }

#ifdef EVENT_DEBUG
  ndbout_c("DBDICT::execCREATE_EVNT_REQ from %u evntRecId = (%d)", refToNode(signal->getSendersBlockRef()), evntRecPtr.i);
#endif

  ndbrequire(req->getUserRef() == signal->getSendersBlockRef());

  evntRecPtr.p->init(req,this);

  if (requestFlag & (Uint32)CreateEvntReq::RT_DICT_AFTER_GET) {
    jam();
    EVENT_TRACE;
    releaseSections(handle);

    if (ERROR_INSERTED(6023))
    {
      jam();
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
      return;
    }

    createEvent_RT_DICT_AFTER_GET(signal, evntRecPtr);
    return;
  }
  if (requestType == CreateEvntReq::RT_USER_GET) {
    jam();
    EVENT_TRACE;
    createEvent_RT_USER_GET(signal, evntRecPtr, handle);
    return;
  }
  if (requestType == CreateEvntReq::RT_USER_CREATE) {
    jam();
    EVENT_TRACE;
    createEvent_RT_USER_CREATE(signal, evntRecPtr, handle);
    return;
  }

#ifdef EVENT_DEBUG
  ndbout << "Dbdict.cpp: Dbdict::execCREATE_EVNT_REQ other" << endl;
#endif
  jam();
  releaseSections(handle);

  evntRecPtr.p->m_errorCode = 1;
  evntRecPtr.p->m_errorLine = __LINE__;
  evntRecPtr.p->m_errorNode = reference();

  createEvent_sendReply(signal, evntRecPtr);
}

/********************************************************************
 *
 * Event creation
 *
 *****************************************************************/

void
Dbdict::createEvent_RT_USER_CREATE(Signal* signal,
				   OpCreateEventPtr evntRecPtr,
				   SectionHandle& handle)
{
  jam();
  DBUG_ENTER("Dbdict::createEvent_RT_USER_CREATE");
  evntRecPtr.p->m_request.setUserRef(signal->senderBlockRef());

#ifdef EVENT_DEBUG
  ndbout << "Dbdict.cpp: Dbdict::execCREATE_EVNT_REQ RT_USER" << endl;
  char buf[128] = {0};
  AttributeMask mask = evntRecPtr.p->m_request.getAttrListBitmask();
  mask.getText(buf);
  ndbout_c("mask = %s", buf);
#endif

  // Interpret the long signal

  SegmentedSectionPtr ssPtr;
  // save name and event properties
  ndbrequire(handle.getSection(ssPtr, CreateEvntReq::EVENT_NAME_SECTION));

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
    // event name
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(handle);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  {
    int len = (int)strlen(evntRecPtr.p->m_eventRec.NAME);
    memset(evntRecPtr.p->m_eventRec.NAME+len, 0, MAX_TAB_NAME_SIZE-len);
#ifdef EVENT_DEBUG
    printf("CreateEvntReq::RT_USER_CREATE; EventName %s, len %u\n",
	   evntRecPtr.p->m_eventRec.NAME, len);
    for(int i = 0; i < MAX_TAB_NAME_SIZE/4; i++)
      printf("H'%.8x ", ((Uint32*)evntRecPtr.p->m_eventRec.NAME)[i]);
    printf("\n");
#endif
  }
  // table name
  if ((!r0.next()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();

    evntRecPtr.p->m_errorCode = 1;
sendref:
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    releaseSections(handle);

    createEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.TABLE_NAME);
  {
    int len = (int)strlen(evntRecPtr.p->m_eventRec.TABLE_NAME);
    memset(evntRecPtr.p->m_eventRec.TABLE_NAME+len, 0, MAX_TAB_NAME_SIZE-len);
  }

  if (handle.m_cnt >= CreateEvntReq::ATTRIBUTE_MASK)
  {
    jam();
    handle.getSection(ssPtr, CreateEvntReq::ATTRIBUTE_MASK);
    if (ssPtr.sz >= NDB_ARRAY_SIZE(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2))
    {
      jam();
      evntRecPtr.p->m_errorCode = 1;
      goto sendref;
    }
    bzero(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2,
          sizeof(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2));
    copy(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2, ssPtr);
    memcpy(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK,
           evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2,
           sizeof(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK));
  }
  else
  {
    jam();
    AttributeMask_OLD m = evntRecPtr.p->m_request.getAttrListBitmask();
    Uint32 sz0 = m.getSizeInWords();
    Uint32 sz1 = NDB_ARRAY_SIZE(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK);
    ndbrequire(sz1 == sz0);
    bzero(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK,
          sizeof(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK));
    bzero(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2,
          sizeof(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2));
    BitmaskImpl::assign(sz0, evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK,
                        m.rep.data);
    BitmaskImpl::assign(sz0, evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2,
                        m.rep.data);
  }

  releaseSections(handle);

  // Send request to SUMA

  CreateSubscriptionIdReq * sumaIdReq =
    (CreateSubscriptionIdReq *)signal->getDataPtrSend();

  // make sure we save the original sender for later
  sumaIdReq->senderRef  = reference();
  sumaIdReq->senderData = evntRecPtr.i;
#ifdef EVENT_DEBUG
  ndbout << "sumaIdReq->senderData = " << sumaIdReq->senderData << endl;
#endif
  sendSignal(SUMA_REF, GSN_CREATE_SUBID_REQ, signal,
	     CreateSubscriptionIdReq::SignalLength, JBB);
  // we should now return in either execCREATE_SUBID_CONF
  // or execCREATE_SUBID_REF
  DBUG_VOID_RETURN;
}

void Dbdict::execCREATE_SUBID_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execCREATE_SUBID_REF");
  CreateSubscriptionIdRef * const ref =
    (CreateSubscriptionIdRef *)signal->getDataPtr();
  OpCreateEventPtr evntRecPtr;

  evntRecPtr.i = ref->senderData;
  ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

  if (ref->errorCode)
  {
    evntRecPtr.p->m_errorCode = ref->errorCode;
    evntRecPtr.p->m_errorLine = __LINE__;
  }
  else
  {
    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
  }
  evntRecPtr.p->m_errorNode = reference();

  createEvent_sendReply(signal, evntRecPtr);
  DBUG_VOID_RETURN;
}

void Dbdict::execCREATE_SUBID_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execCREATE_SUBID_CONF");

  CreateSubscriptionIdConf const * sumaIdConf =
    (CreateSubscriptionIdConf *)signal->getDataPtr();

  Uint32 evntRecId = sumaIdConf->senderData;
  OpCreateEvent *evntRec;

  ndbrequire((evntRec = c_opCreateEvent.getPtr(evntRecId)) != NULL);

  evntRec->m_request.setEventId(sumaIdConf->subscriptionId);
  evntRec->m_request.setEventKey(sumaIdConf->subscriptionKey);

  Callback c = { safe_cast(&Dbdict::createEventUTIL_PREPARE), 0 };

  prepareTransactionEventSysTable(&c, signal, evntRecId,
				  UtilPrepareReq::Insert);
  DBUG_VOID_RETURN;
}

void
Dbdict::createEventComplete_RT_USER_CREATE(Signal* signal,
					   OpCreateEventPtr evntRecPtr){
  jam();
  createEvent_sendReply(signal, evntRecPtr);
}

/*********************************************************************
 *
 * UTIL_PREPARE, UTIL_EXECUTE
 *
 * insert or read systable NDB$EVENTS_0
 */

void interpretUtilPrepareErrorCode(UtilPrepareRef::ErrorCode errorCode,
				   Uint32& error, Uint32& line,
                                   const Dbdict *dict)
{
  DBUG_ENTER("interpretUtilPrepareErrorCode");
  switch (errorCode) {
  case UtilPrepareRef::NO_ERROR:
    jamBlock(dict);
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARE_SEIZE_ERROR:
    jamBlock(dict);
    error = 748;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARE_PAGES_SEIZE_ERROR:
    jamBlock(dict);
    error = 748;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR:
    jamBlock(dict);
    error = 748;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::DICT_TAB_INFO_ERROR:
    jamBlock(dict);
    error = 748;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::MISSING_PROPERTIES_SECTION:
    jamBlock(dict);
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  default:
    jamBlock(dict);
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  }
  DBUG_VOID_RETURN;
}

void
Dbdict::createEventUTIL_PREPARE(Signal* signal,
				Uint32 callbackData,
				Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode == 0) {
    UtilPrepareConf* const req = (UtilPrepareConf*)signal->getDataPtr();
    OpCreateEventPtr evntRecPtr;
    jam();
    evntRecPtr.i = req->getSenderData();
    const Uint32 prepareId = req->getPrepareId();

    ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

    Callback c = { safe_cast(&Dbdict::createEventUTIL_EXECUTE), 0 };

    switch (evntRecPtr.p->m_requestType) {
    case CreateEvntReq::RT_USER_GET:
      jam();
      executeTransEventSysTable(&c, signal,
				evntRecPtr.i, evntRecPtr.p->m_eventRec,
				prepareId, UtilPrepareReq::Read);
      break;
    case CreateEvntReq::RT_USER_CREATE:
      {
	evntRecPtr.p->m_eventRec.EVENT_TYPE =
          evntRecPtr.p->m_request.getEventType() | evntRecPtr.p->m_request.getReportFlags();
	evntRecPtr.p->m_eventRec.TABLEID  = evntRecPtr.p->m_request.getTableId();
	evntRecPtr.p->m_eventRec.TABLEVERSION=evntRecPtr.p->m_request.getTableVersion();
	evntRecPtr.p->m_eventRec.SUBID  = evntRecPtr.p->m_request.getEventId();
	evntRecPtr.p->m_eventRec.SUBKEY = evntRecPtr.p->m_request.getEventKey();
	DBUG_PRINT("info",
		   ("CREATE: event name: %s table name: %s table id: %u table version: %u",
		    evntRecPtr.p->m_eventRec.NAME,
		    evntRecPtr.p->m_eventRec.TABLE_NAME,
		    evntRecPtr.p->m_eventRec.TABLEID,
		    evntRecPtr.p->m_eventRec.TABLEVERSION));

      }
      jam();
      executeTransEventSysTable(&c, signal,
				evntRecPtr.i, evntRecPtr.p->m_eventRec,
				prepareId, UtilPrepareReq::Insert);
      break;
    default:
#ifdef EVENT_DEBUG
      printf("type = %d\n", evntRecPtr.p->m_requestType);
      printf("bet type = %d\n", CreateEvntReq::RT_USER_GET);
      printf("create type = %d\n", CreateEvntReq::RT_USER_CREATE);
#endif
      ndbrequire(false);
    }
  } else { // returnCode != 0
    UtilPrepareRef* const ref = (UtilPrepareRef*)signal->getDataPtr();

    const UtilPrepareRef::ErrorCode errorCode =
      (UtilPrepareRef::ErrorCode)ref->getErrorCode();

    OpCreateEventPtr evntRecPtr;
    evntRecPtr.i = ref->getSenderData();
    ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

    interpretUtilPrepareErrorCode(errorCode, evntRecPtr.p->m_errorCode,
				  evntRecPtr.p->m_errorLine, this);
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
  }
}

static
Uint32
countPrefixBytes(Uint32 len, const Uint8 * mask)
{
  while (len && mask[len - 1] == 0)
    len--;
  return len;
}

void Dbdict::executeTransEventSysTable(Callback *pcallback, Signal *signal,
				       const Uint32 ptrI,
				       sysTab_NDBEVENTS_0& m_eventRec,
				       const Uint32 prepareId,
				       UtilPrepareReq::OperationTypeValue prepReq)
{
  jam();

  DictObject * opj_ptr_p = get_object(EVENT_SYSTEM_TABLE_NAME,
				      sizeof(EVENT_SYSTEM_TABLE_NAME));

  ndbrequire(opj_ptr_p != 0);
  TableRecordPtr tablePtr;
  c_tableRecordPool_.getPtr(tablePtr, opj_ptr_p->m_object_ptr_i);
  ndbrequire(tablePtr.i != RNIL); // system table must exist

  Uint32 noAttr = tablePtr.p->noOfAttributes;
  if (noAttr > EVENT_SYSTEM_TABLE_LENGTH)
  {
    jam();
    noAttr = EVENT_SYSTEM_TABLE_LENGTH;
  }

  Uint32 total_len = 0;

  Uint32* attrHdr = signal->theData + 25;
  Uint32* attrPtr = attrHdr;
  Uint32* dataPtr = attrHdr + noAttr;

  Uint32 id=0;
  // attribute 0 event name: Primary Key
  {
    char *base = (char*)&m_eventRec;
    Uint32 sz = sysTab_NDBEVENTS_0_szs[id];
    const Uint32 *src = (const Uint32*)(base +sysTab_NDBEVENTS_0_offsets[id]);
    memcpy(dataPtr, src, sz);
    dataPtr += (sz / 4);

    AttributeHeader::init(attrPtr, id, sz);
    total_len += sz;
    attrPtr++; id++;
  }

  switch (prepReq) {
  case UtilPrepareReq::Read:
    jam();
    EVENT_TRACE;

    // clear it, since NDB$EVENTS_0.ATTRIBUTE_MASK2 might not be present
    bzero(m_eventRec.ATTRIBUTE_MASK2, sizeof(m_eventRec.ATTRIBUTE_MASK2));

    // no more
    while ( id < noAttr )
      AttributeHeader::init(attrPtr++, id++, 0);
    ndbrequire(id == noAttr);
    break;
  case UtilPrepareReq::Insert:
  {
    jam();
    EVENT_TRACE;
    char *base = (char*)&m_eventRec;
    while ( id < noAttr )
    {
      if (id != EVENT_SYSTEM_TABLE_ATTRIBUTE_MASK2_ID)
      {
        jam();
        Uint32 sz = sysTab_NDBEVENTS_0_szs[id];
        AttributeHeader::init(attrPtr, id, sz);
        const Uint32 *src = (const Uint32*)(base +sysTab_NDBEVENTS_0_offsets[id]);
        memcpy(dataPtr, src, sz);
        dataPtr += (sz / 4);
        total_len += sysTab_NDBEVENTS_0_szs[id];
      }
      else
      {
        jam();
        Uint32 szBytes = countPrefixBytes(sizeof(m_eventRec.ATTRIBUTE_MASK2) - 4,
                                          (Uint8*)m_eventRec.ATTRIBUTE_MASK2);
        AttributeHeader::init(attrPtr, id, 2 + szBytes);

        Uint8 * lenbytes = (Uint8*)dataPtr;
        lenbytes[0] = (szBytes & 0xFF);
        lenbytes[1] = (szBytes / 256);
        memcpy(lenbytes + 2, m_eventRec.ATTRIBUTE_MASK2, szBytes);
        if (szBytes & 3)
        {
          bzero(lenbytes + 2 + szBytes, 4 - (szBytes & 3));
        }
        szBytes += 2;
        dataPtr += (szBytes + 3) / 4;
        total_len += 4 * ((szBytes + 3) / 4);
      }
      attrPtr++; id++;
    }
    break;
  }
  case UtilPrepareReq::Delete:
    ndbrequire(id == 1);
    break;
  default:
    ndbrequire(false);
  }

  LinearSectionPtr headerPtr;
  LinearSectionPtr lsdataPtr;

  headerPtr.p = attrHdr;
  headerPtr.sz = id;

  lsdataPtr.p = attrHdr + noAttr;
  lsdataPtr.sz = total_len/4;

#if 0
    printf("Header size %u\n", headerPtr.sz);
    for(int i = 0; i < (int)headerPtr.sz; i++)
      printf("H'%.8x ", attrHdr[i]);
    printf("\n");

    printf("Data size %u\n", lsdataPtr.sz);
    for(int i = 0; i < (int)lsdataPtr.sz; i++)
      printf("H'%.8x ", dataPage[i]);
    printf("\n");
#endif

  executeTransaction(pcallback, signal,
		     ptrI,
		     prepareId,
		     id,
		     headerPtr,
		     lsdataPtr);
}

void Dbdict::executeTransaction(Callback *pcallback,
				Signal* signal,
				Uint32 senderData,
				Uint32 prepareId,
				Uint32 noAttr,
				LinearSectionPtr headerPtr,
				LinearSectionPtr dataPtr)
{
  jam();
  EVENT_TRACE;

  UtilExecuteReq * utilExecuteReq =
    (UtilExecuteReq *)signal->getDataPtrSend();

  utilExecuteReq->setSenderRef(reference());
  utilExecuteReq->setSenderData(senderData);
  utilExecuteReq->setPrepareId(prepareId);
  utilExecuteReq->setReleaseFlag(); // must be done after setting prepareId

#if 0
  printf("Header size %u\n", headerPtr.sz);
  for(int i = 0; i < (int)headerPtr.sz; i++)
    printf("H'%.8x ", headerBuffer[i]);
  printf("\n");

  printf("Data size %u\n", dataPtr.sz);
  for(int i = 0; i < (int)dataPtr.sz; i++)
    printf("H'%.8x ", dataBuffer[i]);
  printf("\n");
#endif

  struct LinearSectionPtr sectionsPtr[UtilExecuteReq::NoOfSections];
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].p = headerPtr.p;
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].sz = noAttr;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].p = dataPtr.p;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].sz = dataPtr.sz;

  sendSignalUtilReq(pcallback, DBUTIL_REF, GSN_UTIL_EXECUTE_REQ, signal,
		    UtilExecuteReq::SignalLength, JBB,
		    sectionsPtr, UtilExecuteReq::NoOfSections);
}

void Dbdict::parseReadEventSys(Signal* signal, sysTab_NDBEVENTS_0& m_eventRec)
{
  jam();
  SectionHandle handle(this, signal);
  SegmentedSectionPtr headerPtr, dataPtr;

  handle.getSection(headerPtr, UtilExecuteReq::HEADER_SECTION);
  SectionReader headerReader(headerPtr, getSectionSegmentPool());

  handle.getSection(dataPtr, UtilExecuteReq::DATA_SECTION);
  SectionReader dataReader(dataPtr, getSectionSegmentPool());

  char *base = (char*)&m_eventRec;

  DictObject * opj_ptr_p = get_object(EVENT_SYSTEM_TABLE_NAME,
				      sizeof(EVENT_SYSTEM_TABLE_NAME));

  ndbrequire(opj_ptr_p != 0);
  TableRecordPtr tablePtr;
  c_tableRecordPool_.getPtr(tablePtr, opj_ptr_p->m_object_ptr_i);
  ndbrequire(tablePtr.i != RNIL); // system table must exist

  Uint32 noAttr = tablePtr.p->noOfAttributes;
  if (noAttr > EVENT_SYSTEM_TABLE_LENGTH)
  {
    jam();
    noAttr = EVENT_SYSTEM_TABLE_LENGTH;
  }

  for (Uint32 i = 0; i < noAttr; i++)
  {
    jam();
    Uint32 headerWord;
    headerReader.getWord(&headerWord);
    Uint32 sz = AttributeHeader::getDataSize(headerWord);
    ndbrequire(4 * sz <= sysTab_NDBEVENTS_0_szs[i]);
    Uint32 * dst = (Uint32*)(base + sysTab_NDBEVENTS_0_offsets[i]);
    for (Uint32 j = 0; j < sz; j++)
      dataReader.getWord(dst++);
  }

  releaseSections(handle);

  if (noAttr < EVENT_SYSTEM_TABLE_LENGTH)
  {
    jam();
    bzero(m_eventRec.ATTRIBUTE_MASK2, sizeof(m_eventRec.ATTRIBUTE_MASK2));
    memcpy(m_eventRec.ATTRIBUTE_MASK2, m_eventRec.ATTRIBUTE_MASK,
           sizeof(m_eventRec.ATTRIBUTE_MASK));
  }
  else
  {
    jam();
    Uint8* lenbytes = (Uint8*)m_eventRec.ATTRIBUTE_MASK2;
    Uint32 szBytes  = lenbytes[0] + (lenbytes[1] * 256);
    memmove(lenbytes, lenbytes + 2, szBytes);
    bzero(lenbytes + szBytes, sizeof(m_eventRec.ATTRIBUTE_MASK2) - szBytes);
  }
}

void Dbdict::createEventUTIL_EXECUTE(Signal *signal,
				     Uint32 callbackData,
				     Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode == 0) {
    // Entry into system table all set
    UtilExecuteConf* const conf = (UtilExecuteConf*)signal->getDataPtr();
    jam();
    OpCreateEventPtr evntRecPtr;
    evntRecPtr.i = conf->getSenderData();

    ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);
    OpCreateEvent *evntRec = evntRecPtr.p;

    switch (evntRec->m_requestType) {
    case CreateEvntReq::RT_USER_GET: {
      parseReadEventSys(signal, evntRecPtr.p->m_eventRec);

      evntRec->m_request.setEventType(evntRecPtr.p->m_eventRec.EVENT_TYPE);
      evntRec->m_request.setReportFlags(evntRecPtr.p->m_eventRec.EVENT_TYPE);
      evntRec->m_request.setTableId(evntRecPtr.p->m_eventRec.TABLEID);
      evntRec->m_request.setTableVersion(evntRecPtr.p->m_eventRec.TABLEVERSION);
      Uint32 sz = NDB_ARRAY_SIZE(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK);
      evntRec->m_request.setAttrListBitmask(sz, evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK);
      evntRec->m_request.setEventId(evntRecPtr.p->m_eventRec.SUBID);
      evntRec->m_request.setEventKey(evntRecPtr.p->m_eventRec.SUBKEY);

      DBUG_PRINT("info",
		 ("GET: event name: %s table name: %s table id: %u table version: %u",
		  evntRecPtr.p->m_eventRec.NAME,
		  evntRecPtr.p->m_eventRec.TABLE_NAME,
		  evntRecPtr.p->m_eventRec.TABLEID,
		  evntRecPtr.p->m_eventRec.TABLEVERSION));

      // find table id for event table
      DictObject* obj_ptr_p = get_object(evntRecPtr.p->m_eventRec.TABLE_NAME);
      if(!obj_ptr_p){
	jam();
	evntRecPtr.p->m_errorCode = 723;
	evntRecPtr.p->m_errorLine = __LINE__;
	evntRecPtr.p->m_errorNode = reference();
	
	createEvent_sendReply(signal, evntRecPtr);
	return;
      }

      TableRecordPtr tablePtr;
      c_tableRecordPool_.getPtr(tablePtr, obj_ptr_p->m_object_ptr_i);
      evntRec->m_request.setTableId(tablePtr.p->tableId);
      evntRec->m_request.setTableVersion(tablePtr.p->tableVersion);

      createEventComplete_RT_USER_GET(signal, evntRecPtr);
      return;
    }
    case CreateEvntReq::RT_USER_CREATE: {
#ifdef EVENT_DEBUG
      printf("create type = %d\n", CreateEvntReq::RT_USER_CREATE);
#endif
      jam();
      createEventComplete_RT_USER_CREATE(signal, evntRecPtr);
      return;
    }
      break;
    default:
      ndbrequire(false);
    }
  } else { // returnCode != 0
    UtilExecuteRef * const ref = (UtilExecuteRef *)signal->getDataPtr();
    OpCreateEventPtr evntRecPtr;
    evntRecPtr.i = ref->getSenderData();
    ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);
    jam();
    evntRecPtr.p->m_errorNode = reference();
    evntRecPtr.p->m_errorLine = __LINE__;

    switch (ref->getErrorCode()) {
    case UtilExecuteRef::TCError:
      switch (ref->getTCErrorCode()) {
      case ZNOT_FOUND:
	jam();
	evntRecPtr.p->m_errorCode = 4710;
	break;
      case ZALREADYEXIST:
	jam();
	evntRecPtr.p->m_errorCode = 746;
	break;
      default:
	jam();
	evntRecPtr.p->m_errorCode = ref->getTCErrorCode();
	break;
      }
      break;
    default:
      jam();
      evntRecPtr.p->m_errorCode = ref->getErrorCode();
      break;
    }

    createEvent_sendReply(signal, evntRecPtr);
  }
}

/***********************************************************************
 *
 * NdbEventOperation, reading systable, creating event in suma
 *
 */

void
Dbdict::createEvent_RT_USER_GET(Signal* signal,
				OpCreateEventPtr evntRecPtr,
				SectionHandle& handle){
  jam();
  EVENT_TRACE;
#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_CREATE_EVNT_REQ::RT_USER_GET evntRecPtr.i = (%d), ref = %u", evntRecPtr.i, evntRecPtr.p->m_request.getUserRef());
#endif

  SegmentedSectionPtr ssPtr;

  handle.getSection(ssPtr, 0);

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(handle);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
    return;
  }

  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  int len = (int)strlen(evntRecPtr.p->m_eventRec.NAME);
  memset(evntRecPtr.p->m_eventRec.NAME+len, 0, MAX_TAB_NAME_SIZE-len);

  releaseSections(handle);

  Callback c = { safe_cast(&Dbdict::createEventUTIL_PREPARE), 0 };

  prepareTransactionEventSysTable(&c, signal, evntRecPtr.i,
				  UtilPrepareReq::Read);
  /*
   * Will read systable and fill an OpCreateEventPtr
   * and return below
   */
}

void
Dbdict::createEventComplete_RT_USER_GET(Signal* signal,
					OpCreateEventPtr evntRecPtr){
  jam();

  // Send to oneself and the other DICT's
  CreateEvntReq * req = (CreateEvntReq *)signal->getDataPtrSend();

  *req = evntRecPtr.p->m_request;
  req->senderRef = reference();
  req->senderData = evntRecPtr.i;

  req->addRequestFlag(CreateEvntReq::RT_DICT_AFTER_GET);

#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Coordinator) sending GSN_CREATE_EVNT_REQ::RT_DICT_AFTER_GET to DBDICT participants evntRecPtr.i = (%d)", evntRecPtr.i);
#endif

  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  RequestTracker & p = evntRecPtr.p->m_reqTracker;
  if (!p.init<CreateEvntRef>(c_counterMgr, rg, GSN_CREATE_EVNT_REF,
			     evntRecPtr.i))
  {
    jam();
    evntRecPtr.p->m_errorCode = 701;
    createEvent_sendReply(signal, evntRecPtr);
    return;
  }

  sendSignal(rg, GSN_CREATE_EVNT_REQ, signal, CreateEvntReq::SignalLength, JBB);
  return;
}

void
Dbdict::createEvent_nodeFailCallback(Signal* signal, Uint32 eventRecPtrI,
				     Uint32 returnCode){
  OpCreateEventPtr evntRecPtr;
  c_opCreateEvent.getPtr(evntRecPtr, eventRecPtrI);
  createEvent_sendReply(signal, evntRecPtr);
}

void Dbdict::execCREATE_EVNT_REF(Signal* signal)
{
  jamEntry();
  EVENT_TRACE;
  CreateEvntRef * const ref = (CreateEvntRef *)signal->getDataPtr();
  OpCreateEventPtr evntRecPtr;

  evntRecPtr.i = ref->getUserData();
  ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_CREATE_EVNT_REF evntRecPtr.i = (%d)", evntRecPtr.i);
#endif

  LinearSectionPtr ptr[2];

  int noLSP = 0;
  LinearSectionPtr *ptr0 = NULL;
  if (ref->errorCode == CreateEvntRef::NF_FakeErrorREF)
  {
    jam();
    evntRecPtr.p->m_reqTracker.ignoreRef(c_counterMgr,
                                         refToNode(ref->senderRef));

    /**
     * If a CREATE_EVNT_REF finishes request,
     *   we need to make sure that table name is sent
     *   same as it is if a CREATE_EVNT_CONF finishes request
     */
    if (evntRecPtr.p->m_reqTracker.done())
    {
      jam();
      /**
       * Add "extra" check if tracker is done...
       *   (strictly not necessary...)
       *   but makes case more explicit
       */
      ptr[0].p = (Uint32 *)evntRecPtr.p->m_eventRec.TABLE_NAME;
      ptr[0].sz = Uint32((strlen(evntRecPtr.p->m_eventRec.TABLE_NAME)+4)/4);
      ptr[1].p = evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2;
      ptr[1].sz = NDB_ARRAY_SIZE(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2) - 1;
      ptr0 = ptr;
      noLSP = 2;
    }
  }
  else
  {
    jam();
    evntRecPtr.p->m_errorCode = ref->errorCode;
    evntRecPtr.p->m_reqTracker.reportRef(c_counterMgr,
                                         refToNode(ref->senderRef));
  }

  createEvent_sendReply(signal, evntRecPtr, ptr0, noLSP);

  return;
}

void Dbdict::execCREATE_EVNT_CONF(Signal* signal)
{
  jamEntry();
  EVENT_TRACE;
  CreateEvntConf * const conf = (CreateEvntConf *)signal->getDataPtr();
  OpCreateEventPtr evntRecPtr;

  evntRecPtr.i = conf->getUserData();

  ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_CREATE_EVNT_CONF evntRecPtr.i = (%d)", evntRecPtr.i);
#endif

  evntRecPtr.p->m_reqTracker.reportConf(c_counterMgr, refToNode(conf->senderRef));

  // we will only have a valid tablename if it the master DICT sending this
  // but that's ok
  LinearSectionPtr ptr[2];
  ptr[0].p = (Uint32 *)evntRecPtr.p->m_eventRec.TABLE_NAME;
  ptr[0].sz =
    Uint32(strlen(evntRecPtr.p->m_eventRec.TABLE_NAME)+4)/4; // to make sure we have a null
  ptr[1].p = evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2;
  ptr[1].sz = NDB_ARRAY_SIZE(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK2) - 1;

  createEvent_sendReply(signal, evntRecPtr, ptr, 2);

  return;
}

/************************************************
 *
 * Participant stuff
 *
 */

void
Dbdict::createEvent_RT_DICT_AFTER_GET(Signal* signal, OpCreateEventPtr evntRecPtr){
  DBUG_ENTER("Dbdict::createEvent_RT_DICT_AFTER_GET");
  jam();
  evntRecPtr.p->m_request.setUserRef(signal->senderBlockRef());

#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Participant) got CREATE_EVNT_REQ::RT_DICT_AFTER_GET evntRecPtr.i = (%d)", evntRecPtr.i);
#endif

  // the signal comes from the DICT block that got the first user request!
  // This code runs on all DICT nodes, including oneself

  // Seize a Create Event record, the Coordinator will now have two seized
  // but that's ok, it's like a recursion

  CRASH_INSERTION2(6009, getOwnNodeId() != c_masterNodeId);

  SubCreateReq * sumaReq = (SubCreateReq *)signal->getDataPtrSend();

  sumaReq->senderRef        = reference(); // reference to DICT
  sumaReq->senderData       = evntRecPtr.i;
  sumaReq->subscriptionId   = evntRecPtr.p->m_request.getEventId();
  sumaReq->subscriptionKey  = evntRecPtr.p->m_request.getEventKey();
  sumaReq->subscriptionType = SubCreateReq::TableEvent;
  if (evntRecPtr.p->m_request.getReportAll())
    sumaReq->subscriptionType|= SubCreateReq::ReportAll;
  if (evntRecPtr.p->m_request.getReportSubscribe())
    sumaReq->subscriptionType|= SubCreateReq::ReportSubscribe;
  if (! evntRecPtr.p->m_request.getReportDDL())
  {
    sumaReq->subscriptionType |= SubCreateReq::NoReportDDL;
  }
  sumaReq->tableId          = evntRecPtr.p->m_request.getTableId();
  sumaReq->schemaTransId    = 0;

#ifdef EVENT_PH2_DEBUG
  ndbout_c("sending GSN_SUB_CREATE_REQ");
#endif

  sendSignal(SUMA_REF, GSN_SUB_CREATE_REQ, signal,
	     SubCreateReq::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

void Dbdict::execSUB_CREATE_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execSUB_CREATE_REF");

  SubCreateRef * const ref = (SubCreateRef *)signal->getDataPtr();
  OpCreateEventPtr evntRecPtr;

  evntRecPtr.i = ref->senderData;
  ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

  if (ref->errorCode == SubCreateRef::NotStarted)
  {
    jam();
    // ignore (was previously NF_FakeErrorREF)
    // NOTE: different handling then rest of execSUB_XXX_REF
    // note to mess with GSN_CREATE_EVNT
  }
  else if (ref->errorCode)
  {
    jam();
    evntRecPtr.p->m_errorCode = ref->errorCode;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();
  }
  else
  {
    jam();
    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();
  }

  createEvent_sendReply(signal, evntRecPtr);
  DBUG_VOID_RETURN;
}

void Dbdict::execSUB_CREATE_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execSUB_CREATE_CONF");
  EVENT_TRACE;

  SubCreateConf * const sumaConf = (SubCreateConf *)signal->getDataPtr();
  OpCreateEventPtr evntRecPtr;
  evntRecPtr.i = sumaConf->senderData;
  ndbrequire((evntRecPtr.p = c_opCreateEvent.getPtr(evntRecPtr.i)) != NULL);

  createEvent_sendReply(signal, evntRecPtr);

  DBUG_VOID_RETURN;
}

/****************************************************
 *
 * common create reply method
 *
 *******************************************************/

void Dbdict::createEvent_sendReply(Signal* signal,
				   OpCreateEventPtr evntRecPtr,
				   LinearSectionPtr *ptr, int noLSP)
{
  jam();
  EVENT_TRACE;

  // check if we're ready to sent reply
  // if we are the master dict we might be waiting for conf/ref

  if (!evntRecPtr.p->m_reqTracker.done()) {
    jam();
    return; // there's more to come
  }

  if (evntRecPtr.p->m_reqTracker.hasRef()) {
    ptr = NULL; // we don't want to return anything if there's an error
    if (!evntRecPtr.p->hasError()) {
      evntRecPtr.p->m_errorCode = 1;
      evntRecPtr.p->m_errorLine = __LINE__;
      evntRecPtr.p->m_errorNode = reference();
      jam();
    }
    else
    {
      jam();
    }
  }

  // reference to API if master DICT
  // else reference to master DICT
  Uint32 senderRef = evntRecPtr.p->m_request.getUserRef();
  Uint32 signalLength;
  Uint32 gsn;

  if (evntRecPtr.p->hasError()) {
    jam();
    EVENT_TRACE;
    CreateEvntRef * ret = (CreateEvntRef *)signal->getDataPtrSend();

    ret->setEventId(evntRecPtr.p->m_request.getEventId());
    ret->setEventKey(evntRecPtr.p->m_request.getEventKey());
    ret->setUserData(evntRecPtr.p->m_request.getUserData());
    ret->senderRef = reference();
    ret->setTableId(evntRecPtr.p->m_request.getTableId());
    ret->setTableVersion(evntRecPtr.p->m_request.getTableVersion());
    ret->setEventType(evntRecPtr.p->m_request.getEventType());
    ret->setRequestType(evntRecPtr.p->m_request.getRequestType());

    ret->setErrorCode(evntRecPtr.p->m_errorCode);
    ret->setErrorLine(evntRecPtr.p->m_errorLine);
    ret->setErrorNode(evntRecPtr.p->m_errorNode);

    signalLength = CreateEvntRef::SignalLength;
#ifdef EVENT_PH2_DEBUG
    ndbout_c("DBDICT sending GSN_CREATE_EVNT_REF to evntRecPtr.i = (%d) node = %u ref = %u", evntRecPtr.i, refToNode(senderRef), senderRef);
    ndbout_c("errorCode = %u", evntRecPtr.p->m_errorCode);
    ndbout_c("errorLine = %u", evntRecPtr.p->m_errorLine);
#endif
    gsn = GSN_CREATE_EVNT_REF;

  } else {
    jam();
    EVENT_TRACE;
    CreateEvntConf * evntConf = (CreateEvntConf *)signal->getDataPtrSend();

    evntConf->setEventId(evntRecPtr.p->m_request.getEventId());
    evntConf->setEventKey(evntRecPtr.p->m_request.getEventKey());
    evntConf->setUserData(evntRecPtr.p->m_request.getUserData());
    evntConf->senderRef = reference();
    evntConf->setTableId(evntRecPtr.p->m_request.getTableId());
    evntConf->setTableVersion(evntRecPtr.p->m_request.getTableVersion());
    evntConf->setAttrListBitmask(evntRecPtr.p->m_request.getAttrListBitmask());
    evntConf->setEventType(evntRecPtr.p->m_request.getEventType());
    evntConf->setRequestType(evntRecPtr.p->m_request.getRequestType());

    signalLength = CreateEvntConf::SignalLength;
#ifdef EVENT_PH2_DEBUG
    ndbout_c("DBDICT sending GSN_CREATE_EVNT_CONF to evntRecPtr.i = (%d) node = %u ref = %u", evntRecPtr.i, refToNode(senderRef), senderRef);
#endif
    gsn = GSN_CREATE_EVNT_CONF;
  }

  if (ptr) {
    jam();
    sendSignal(senderRef, gsn, signal, signalLength, JBB, ptr, noLSP);
  } else {
    jam();
    sendSignal(senderRef, gsn, signal, signalLength, JBB);
  }

  c_opCreateEvent.release(evntRecPtr);
}

/*************************************************************/

/********************************************************************
 *
 * Start event
 *
 *******************************************************************/

void Dbdict::execSUB_START_REQ(Signal* signal)
{
  jamEntry();

  Uint32 origSenderRef = signal->senderBlockRef();

  if (refToBlock(origSenderRef) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    /*
     * Coordinator but not master
     */
    SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = SubStartRef::NotMaster;
    ref->m_masterNodeId = c_masterNodeId;
    sendSignal(origSenderRef, GSN_SUB_START_REF, signal,
	       SubStartRef::SignalLength2, JBB);
    return;
  }
  OpSubEventPtr subbPtr;
  Uint32 errCode = 0;

  if (!c_opSubEvent.seize(subbPtr)) {
    errCode = SubStartRef::Busy;
busy:
    jam();
    SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();

    { // fix
      Uint32 subcriberRef = ((SubStartReq*)signal->getDataPtr())->subscriberRef;
      ref->subscriberRef = subcriberRef;
    }
    jam();
    //      ret->setErrorCode(SubStartRef::SeizeError);
    //      ret->setErrorLine(__LINE__);
    //      ret->setErrorNode(reference());
    ref->senderRef = reference();
    ref->errorCode = errCode;
    ref->m_masterNodeId = c_masterNodeId;

    sendSignal(origSenderRef, GSN_SUB_START_REF, signal,
	       SubStartRef::SL_MasterNode, JBB);
    return;
  }

  {
    const SubStartReq* req = (SubStartReq*) signal->getDataPtr();
    subbPtr.p->m_senderRef = req->senderRef;
    subbPtr.p->m_senderData = req->senderData;
    subbPtr.p->m_errorCode = 0;
    subbPtr.p->m_gsn = GSN_SUB_START_REQ;
    subbPtr.p->m_subscriptionId = req->subscriptionId;
    subbPtr.p->m_subscriptionKey = req->subscriptionKey;
    subbPtr.p->m_subscriberRef = req->subscriberRef;
    subbPtr.p->m_subscriberData = req->subscriberData;
    bzero(subbPtr.p->m_buckets_per_ng, sizeof(subbPtr.p->m_buckets_per_ng));
  }

  if (refToBlock(origSenderRef) != DBDICT) {
    /*
     * Coordinator
     */
    jam();

    if (c_masterNodeId != getOwnNodeId())
    {
      jam();
      c_opSubEvent.release(subbPtr);
      errCode = SubStartRef::NotMaster;
      goto busy;
    }

    if (!c_sub_startstop_lock.isclear())
    {
      jam();
      c_opSubEvent.release(subbPtr);
      errCode = SubStartRef::BusyWithNR;
      goto busy;
    }

    subbPtr.p->m_senderRef = origSenderRef; // not sure if API sets correctly
    NodeReceiverGroup rg(DBDICT, c_aliveNodes);

    RequestTracker & p = subbPtr.p->m_reqTracker;
    if (!p.init<SubStartRef>(c_counterMgr, rg, GSN_SUB_START_REF, subbPtr.i))
    {
      c_opSubEvent.release(subbPtr);
      errCode = SubStartRef::Busy;
      goto busy;
    }

    SubStartReq* req = (SubStartReq*) signal->getDataPtrSend();

    req->senderRef  = reference();
    req->senderData = subbPtr.i;

#ifdef EVENT_PH3_DEBUG
    ndbout_c("DBDICT(Coordinator) sending GSN_SUB_START_REQ to DBDICT participants subbPtr.i = (%d)", subbPtr.i);
#endif

    if (ERROR_INSERTED(6011))
    {
      ndbout_c("sending delayed to self...");
      if (ERROR_INSERTED(6011))
      {
        rg.m_nodes.clear(getOwnNodeId());
      }
      sendSignal(rg, GSN_SUB_START_REQ, signal,
                 SubStartReq::SignalLength, JBB);
      sendSignalWithDelay(reference(),
                          GSN_SUB_START_REQ,
                          signal, 5000, SubStartReq::SignalLength);
    }
    else
    {
      c_outstanding_sub_startstop++;
      sendSignal(rg, GSN_SUB_START_REQ, signal,
                 SubStartReq::SignalLength, JBB);
    }
    return;
  }
  /*
   * Participant
   */
  ndbrequire(refToBlock(origSenderRef) == DBDICT);

  CRASH_INSERTION(6007);

  {
    SubStartReq* req = (SubStartReq*) signal->getDataPtrSend();

    req->senderRef = reference();
    req->senderData = subbPtr.i;

#ifdef EVENT_PH3_DEBUG
    ndbout_c("DBDICT(Participant) sending GSN_SUB_START_REQ to SUMA subbPtr.i = (%d)", subbPtr.i);
#endif
    sendSignal(SUMA_REF, GSN_SUB_START_REQ, signal, SubStartReq::SignalLength, JBB);
  }
}

bool
Dbdict::upgrade_suma_NotStarted(Uint32 err, Uint32 ref) const
{
  /**
   * Check that receiver can handle 1428,
   *   else return true if error code should be replaced by NF_FakeErrorREF
   */
  if (err == 1428)
  {
    jam();
    if (!ndb_suma_not_started_ref(getNodeInfo(refToNode(ref)).m_version))
      return true;
  }
  return false;
}

void Dbdict::execSUB_START_REF(Signal* signal)
{
  jamEntry();

  const SubStartRef* ref = (SubStartRef*) signal->getDataPtr();
  Uint32 senderRef  = ref->senderRef;
  Uint32 err = ref->errorCode;

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, ref->senderData);

  if (refToBlock(senderRef) == SUMA)
  {
    /*
     * Participant
     */
    jam();

#ifdef EVENT_PH3_DEBUG
    ndbout_c("DBDICT(Participant) got GSN_SUB_START_REF = (%d)", subbPtr.i);
#endif

    if (upgrade_suma_NotStarted(err, subbPtr.p->m_senderRef))
    {
      jam();
      err = SubStartRef::NF_FakeErrorREF;
    }

    SubStartRef* ref = (SubStartRef*) signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = subbPtr.p->m_senderData;
    ref->errorCode = err;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_REF,
	       signal, SubStartRef::SignalLength2, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
#ifdef EVENT_PH3_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_SUB_START_REF = (%d)", subbPtr.i);
#endif
  if (err == SubStartRef::NotStarted)
  {
    jam();
    subbPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  }
  else
  {
    if (err == SubStartRef::NF_FakeErrorREF)
    {
      jam();
      err = SubStartRef::NodeDied;
    }

    if (subbPtr.p->m_errorCode == 0)
    {
      subbPtr.p->m_errorCode= err ? err : 1;
    }
    subbPtr.p->m_reqTracker.reportRef(c_counterMgr, refToNode(senderRef));
  }
  completeSubStartReq(signal,subbPtr.i,0);
}

void Dbdict::execSUB_START_CONF(Signal* signal)
{
  jamEntry();

  const SubStartConf* conf = (SubStartConf*) signal->getDataPtr();
  Uint32 senderRef  = conf->senderRef;
  Uint32 buckets = conf->bucketCount;
  Uint32 nodegroup = conf->nodegroup;

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, conf->senderData);

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();
    SubStartConf* conf = (SubStartConf*) signal->getDataPtrSend();

#ifdef EVENT_PH3_DEBUG
  ndbout_c("DBDICT(Participant) got GSN_SUB_START_CONF = (%d)", subbPtr.i);
#endif

    conf->senderRef = reference();
    conf->senderData = subbPtr.p->m_senderData;
    conf->bucketCount = buckets;
    conf->nodegroup = nodegroup;

    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_CONF,
	       signal, SubStartConf::SignalLength, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
#ifdef EVENT_PH3_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_SUB_START_CONF = (%d)", subbPtr.i);
#endif
#define ARRAY_SIZE(x) (sizeof(x)/(sizeof(x[0])))

  if (buckets)
  {
    jam();
    ndbrequire(nodegroup < ARRAY_SIZE(subbPtr.p->m_buckets_per_ng));
    ndbrequire(subbPtr.p->m_buckets_per_ng[nodegroup] == 0 ||
               subbPtr.p->m_buckets_per_ng[nodegroup] == buckets);
    subbPtr.p->m_buckets_per_ng[nodegroup] = buckets;
  }
  subbPtr.p->m_sub_start_conf = *conf;
  subbPtr.p->m_reqTracker.reportConf(c_counterMgr, refToNode(senderRef));
  completeSubStartReq(signal,subbPtr.i,0);
}

/*
 * Coordinator
 */
void Dbdict::completeSubStartReq(Signal* signal,
				 Uint32 ptrI,
				 Uint32 returnCode){
  jam();

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, ptrI);

  if (!subbPtr.p->m_reqTracker.done()){
    jam();
    return;
  }

  if (subbPtr.p->m_reqTracker.hasRef())
  {
    jam();
#ifdef EVENT_DEBUG
    ndbout_c("SUB_START_REF");
#endif

    if (subbPtr.p->m_reqTracker.hasConf())
    {
      jam();
      NodeReceiverGroup rg(DBDICT, subbPtr.p->m_reqTracker.m_confs);
      RequestTracker & p = subbPtr.p->m_reqTracker;
      ndbrequire(p.init<SubStopRef>(c_counterMgr, rg, GSN_SUB_STOP_REF,
                                    subbPtr.i));

      SubStopReq* req = (SubStopReq*) signal->getDataPtrSend();

      req->senderRef  = reference();
      req->senderData = subbPtr.i;
      req->subscriptionId = subbPtr.p->m_subscriptionId;
      req->subscriptionKey = subbPtr.p->m_subscriptionKey;
      req->subscriberRef = subbPtr.p->m_subscriberRef;
      req->subscriberData = subbPtr.p->m_subscriberData;
      req->requestInfo = SubStopReq::RI_ABORT_START;
      sendSignal(rg, GSN_SUB_STOP_REQ, signal, SubStopReq::SignalLength, JBB);
      return;
    }
    else
    {
      jam();
      completeSubStopReq(signal, subbPtr.i, 0);
      return;
    }
  }
#ifdef EVENT_DEBUG
  ndbout_c("SUB_START_CONF");
#endif

  ndbrequire(c_outstanding_sub_startstop);
  c_outstanding_sub_startstop--;
  SubStartConf* conf = (SubStartConf*)signal->getDataPtrSend();
  * conf = subbPtr.p->m_sub_start_conf;

  Uint32 cnt = 0;
  for (Uint32 i = 0; i<ARRAY_SIZE(subbPtr.p->m_buckets_per_ng); i++)
    cnt += subbPtr.p->m_buckets_per_ng[i];
  conf->bucketCount = cnt;

  sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_CONF,
	     signal, SubStartConf::SignalLength, JBB);
  c_opSubEvent.release(subbPtr);
}

/********************************************************************
 *
 * Stop event
 *
 *******************************************************************/

void Dbdict::execSUB_STOP_REQ(Signal* signal)
{
  jamEntry();

  Uint32 origSenderRef = signal->senderBlockRef();

  OpSubEventPtr subbPtr;
  Uint32 errCode = 0;
  if (!c_opSubEvent.seize(subbPtr)) {
    errCode = SubStopRef::Busy;
busy:
    SubStopRef * ref = (SubStopRef *)signal->getDataPtrSend();
    jam();
    //      ret->setErrorCode(SubStartRef::SeizeError);
    //      ret->setErrorLine(__LINE__);
    //      ret->setErrorNode(reference());
    ref->senderRef = reference();
    ref->errorCode = errCode;
    ref->m_masterNodeId = c_masterNodeId;

    sendSignal(origSenderRef, GSN_SUB_STOP_REF, signal,
	       SubStopRef::SL_MasterNode, JBB);
    return;
  }

  {
    SubStopReq* req = (SubStopReq*) signal->getDataPtr();
    subbPtr.p->m_senderRef = req->senderRef;
    subbPtr.p->m_senderData = req->senderData;
    subbPtr.p->m_errorCode = 0;
    subbPtr.p->m_gsn = GSN_SUB_STOP_REQ;
    subbPtr.p->m_subscriptionId = req->subscriptionId;
    subbPtr.p->m_subscriptionKey = req->subscriptionKey;
    subbPtr.p->m_subscriberRef = req->subscriberRef;
    subbPtr.p->m_subscriberData = req->subscriberData;
    bzero(&subbPtr.p->m_sub_stop_conf, sizeof(subbPtr.p->m_sub_stop_conf));

    if (signal->getLength() < SubStopReq::SignalLength)
    {
      jam();
      req->requestInfo = 0;
    }
  }

  if (refToBlock(origSenderRef) != DBDICT) {
    /*
     * Coordinator
     */
    jam();

    if (c_masterNodeId != getOwnNodeId())
    {
      jam();
      c_opSubEvent.release(subbPtr);
      errCode = SubStopRef::NotMaster;
      goto busy;
    }

    if (!c_sub_startstop_lock.isclear())
    {
      jam();
      c_opSubEvent.release(subbPtr);
      errCode = SubStopRef::BusyWithNR;
      goto busy;
    }

#ifdef EVENT_DEBUG
    ndbout_c("SUB_STOP_REQ 1");
#endif
    subbPtr.p->m_senderRef = origSenderRef; // not sure if API sets correctly
    NodeReceiverGroup rg(DBDICT, c_aliveNodes);
    RequestTracker & p = subbPtr.p->m_reqTracker;
    if (!p.init<SubStopRef>(c_counterMgr, rg, GSN_SUB_STOP_REF, subbPtr.i))
    {
      jam();
      c_opSubEvent.release(subbPtr);
      errCode = SubStopRef::Busy;
      goto busy;
    }

    SubStopReq* req = (SubStopReq*) signal->getDataPtrSend();

    req->senderRef  = reference();
    req->senderData = subbPtr.i;

    c_outstanding_sub_startstop++;
    sendSignal(rg, GSN_SUB_STOP_REQ, signal, SubStopReq::SignalLength, JBB);
    return;
  }
  /*
   * Participant
   */
#ifdef EVENT_DEBUG
  ndbout_c("SUB_STOP_REQ 2");
#endif
  ndbrequire(refToBlock(origSenderRef) == DBDICT);

  CRASH_INSERTION(6008);

  {
    SubStopReq* req = (SubStopReq*) signal->getDataPtrSend();

    req->senderRef = reference();
    req->senderData = subbPtr.i;

    sendSignal(SUMA_REF, GSN_SUB_STOP_REQ, signal, SubStopReq::SignalLength, JBB);
  }
}

void Dbdict::execSUB_STOP_REF(Signal* signal)
{
  jamEntry();
  const SubStopRef* ref = (SubStopRef*) signal->getDataPtr();
  Uint32 senderRef  = ref->senderRef;
  Uint32 err = ref->errorCode;

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, ref->senderData);

  if (refToBlock(senderRef) == SUMA)
  {
    /*
     * Participant
     */
    jam();

    if (upgrade_suma_NotStarted(err, subbPtr.p->m_senderRef))
    {
      jam();
      err = SubStopRef::NF_FakeErrorREF;
    }

    SubStopRef* ref = (SubStopRef*) signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = subbPtr.p->m_senderData;
    ref->errorCode = err;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_STOP_REF,
	       signal, SubStopRef::SignalLength, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
  if (err == SubStopRef::NF_FakeErrorREF || err == SubStopRef::NotStarted)
  {
    jam();
    subbPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  }
  else
  {
    jam();
    if (subbPtr.p->m_errorCode == 0)
    {
      subbPtr.p->m_errorCode= err ? err : 1;
    }
    subbPtr.p->m_reqTracker.reportRef(c_counterMgr, refToNode(senderRef));
  }
  completeSubStopReq(signal,subbPtr.i,0);
}

void Dbdict::execSUB_STOP_CONF(Signal* signal)
{
  jamEntry();

  const SubStopConf* conf = (SubStopConf*) signal->getDataPtr();
  Uint32 senderRef  = conf->senderRef;

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, conf->senderData);

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();
    SubStopConf* conf = (SubStopConf*) signal->getDataPtrSend();

    conf->senderRef = reference();
    conf->senderData = subbPtr.p->m_senderData;

    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_STOP_CONF,
	       signal, SubStopConf::SignalLength, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
  Uint64 old_gci, new_gci = 0;
  {
    Uint32 old_gci_hi = subbPtr.p->m_sub_stop_conf.gci_hi;
    Uint32 old_gci_lo = subbPtr.p->m_sub_stop_conf.gci_lo;
    old_gci = old_gci_lo | (Uint64(old_gci_hi) << 32);
    if (signal->getLength() >= SubStopConf::SignalLengthWithGci)
    {
      Uint32 new_gci_hi = conf->gci_hi;
      Uint32 new_gci_lo = conf->gci_lo;
      new_gci = new_gci_lo | (Uint64(new_gci_hi) << 32);
    }
  }
  subbPtr.p->m_sub_stop_conf = *conf;
  if (old_gci > new_gci)
  {
    subbPtr.p->m_sub_stop_conf.gci_hi= Uint32(old_gci>>32);
    subbPtr.p->m_sub_stop_conf.gci_lo= Uint32(old_gci);
  }
  subbPtr.p->m_reqTracker.reportConf(c_counterMgr, refToNode(senderRef));
  completeSubStopReq(signal,subbPtr.i,0);
}

/*
 * Coordinator
 */
void Dbdict::completeSubStopReq(Signal* signal,
				Uint32 ptrI,
				Uint32 returnCode){
  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, ptrI);

  if (!subbPtr.p->m_reqTracker.done()){
    jam();
    return;
  }

  ndbrequire(c_outstanding_sub_startstop);
  c_outstanding_sub_startstop--;

  if (subbPtr.p->m_gsn == GSN_SUB_START_REQ)
  {
    jam();
    SubStartRef* ref = (SubStartRef*)signal->getDataPtrSend();
    ref->senderRef  = reference();
    ref->senderData = subbPtr.p->m_senderData;
    ref->errorCode  = subbPtr.p->m_errorCode;

    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_REF,
	       signal, SubStartRef::SignalLength, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }

  if (subbPtr.p->m_reqTracker.hasRef()) {
    jam();
#ifdef EVENT_DEBUG
    ndbout_c("SUB_STOP_REF");
#endif
    SubStopRef* ref = (SubStopRef*)signal->getDataPtrSend();

    ref->senderRef  = reference();
    ref->senderData = subbPtr.p->m_senderData;
    ref->errorCode  = subbPtr.p->m_errorCode;

    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_STOP_REF,
	       signal, SubStopRef::SignalLength, JBB);
    if (subbPtr.p->m_reqTracker.hasConf()) {
      //  stopStartedNodes(signal);
    }
    c_opSubEvent.release(subbPtr);
    return;
  }
#ifdef EVENT_DEBUG
  ndbout_c("SUB_STOP_CONF");
#endif
  SubStopConf* conf = (SubStopConf*)signal->getDataPtrSend();
  * conf = subbPtr.p->m_sub_stop_conf;
  sendSignal(subbPtr.p->m_senderRef, GSN_SUB_STOP_CONF,
	     signal, SubStopConf::SignalLength, JBB);
  c_opSubEvent.release(subbPtr);
}

/***************************************************************
 * MODULE: Drop event.
 *
 * Drop event.
 *
 * TODO
 */

void
Dbdict::execDROP_EVNT_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execDROP_EVNT_REQ");

  DropEvntReq *req = (DropEvntReq*)signal->getDataPtr();
  const Uint32 senderRef = signal->senderBlockRef();
  OpDropEventPtr evntRecPtr;
  SectionHandle handle(this, signal);

  if (refToBlock(senderRef) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    jam();
    releaseSections(handle);

    DropEvntRef * ref = (DropEvntRef *)signal->getDataPtrSend();
    ref->setUserRef(reference());
    ref->setErrorCode(DropEvntRef::NotMaster);
    ref->setErrorLine(__LINE__);
    ref->setErrorNode(reference());
    ref->setMasterNode(c_masterNodeId);
    sendSignal(senderRef, GSN_DROP_EVNT_REF, signal,
	       DropEvntRef::SignalLength2, JBB);
    DBUG_VOID_RETURN;
  }

  // Seize a Create Event record
  if (!c_opDropEvent.seize(evntRecPtr)) {
    // Failed to allocate event record
    jam();
    releaseSections(handle);

    DropEvntRef * ret = (DropEvntRef *)signal->getDataPtrSend();
    ret->setErrorCode(747);
    ret->setErrorLine(__LINE__);
    ret->setErrorNode(reference());
    sendSignal(senderRef, GSN_DROP_EVNT_REF, signal,
	       DropEvntRef::SignalLength, JBB);
    DBUG_VOID_RETURN;
  }

#ifdef EVENT_DEBUG
  ndbout_c("DBDICT::execDROP_EVNT_REQ evntRecId = (%d)", evntRecPtr.i);
#endif

  OpDropEvent* evntRec = evntRecPtr.p;
  evntRec->init(req);

  SegmentedSectionPtr ssPtr;

  handle.getSection(ssPtr, 0);

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
  // event name
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(handle);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    dropEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  {
    int len = (int)strlen(evntRecPtr.p->m_eventRec.NAME);
    memset(evntRecPtr.p->m_eventRec.NAME+len, 0, MAX_TAB_NAME_SIZE-len);
#ifdef EVENT_DEBUG
    printf("DropEvntReq; EventName %s, len %u\n",
	   evntRecPtr.p->m_eventRec.NAME, len);
    for(int i = 0; i < MAX_TAB_NAME_SIZE/4; i++)
      printf("H'%.8x ", ((Uint32*)evntRecPtr.p->m_eventRec.NAME)[i]);
    printf("\n");
#endif
  }

  releaseSections(handle);

  Callback c = { safe_cast(&Dbdict::dropEventUTIL_PREPARE_READ), 0 };

  prepareTransactionEventSysTable(&c, signal, evntRecPtr.i,
				  UtilPrepareReq::Read);
  DBUG_VOID_RETURN;
}

void
Dbdict::dropEventUTIL_PREPARE_READ(Signal* signal,
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode != 0) {
    EVENT_TRACE;
    dropEventUtilPrepareRef(signal, callbackData, returnCode);
    return;
  }

  UtilPrepareConf* const req = (UtilPrepareConf*)signal->getDataPtr();
  OpDropEventPtr evntRecPtr;
  evntRecPtr.i = req->getSenderData();
  const Uint32 prepareId = req->getPrepareId();

  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);

  Callback c = { safe_cast(&Dbdict::dropEventUTIL_EXECUTE_READ), 0 };

  executeTransEventSysTable(&c, signal,
			    evntRecPtr.i, evntRecPtr.p->m_eventRec,
			    prepareId, UtilPrepareReq::Read);
}

void
Dbdict::dropEventUTIL_EXECUTE_READ(Signal* signal,
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode != 0) {
    EVENT_TRACE;
    dropEventUtilExecuteRef(signal, callbackData, returnCode);
    return;
  }

  OpDropEventPtr evntRecPtr;
  UtilExecuteConf * const ref = (UtilExecuteConf *)signal->getDataPtr();
  jam();
  evntRecPtr.i = ref->getSenderData();
  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);

  parseReadEventSys(signal, evntRecPtr.p->m_eventRec);

  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  RequestTracker & p = evntRecPtr.p->m_reqTracker;
  if (!p.init<SubRemoveRef>(c_counterMgr, rg, GSN_SUB_REMOVE_REF,
			    evntRecPtr.i))
  {
    evntRecPtr.p->m_errorCode = 701;
    dropEvent_sendReply(signal, evntRecPtr);
    return;
  }

  SubRemoveReq* req = (SubRemoveReq*) signal->getDataPtrSend();

  req->senderRef       = reference();
  req->senderData      = evntRecPtr.i;
  req->subscriptionId  = evntRecPtr.p->m_eventRec.SUBID;
  req->subscriptionKey = evntRecPtr.p->m_eventRec.SUBKEY;

  sendSignal(rg, GSN_SUB_REMOVE_REQ, signal, SubRemoveReq::SignalLength, JBB);
}

/*
 * Participant
 */

void
Dbdict::execSUB_REMOVE_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execSUB_REMOVE_REQ");

  Uint32 origSenderRef = signal->senderBlockRef();

  OpSubEventPtr subbPtr;
  if (!c_opSubEvent.seize(subbPtr)) {
    SubRemoveRef * ref = (SubRemoveRef *)signal->getDataPtrSend();
    jam();
    ref->senderRef = reference();
    ref->errorCode = SubRemoveRef::Busy;

    sendSignal(origSenderRef, GSN_SUB_REMOVE_REF, signal,
	       SubRemoveRef::SignalLength, JBB);
    DBUG_VOID_RETURN;
  }

  {
    const SubRemoveReq* req = (SubRemoveReq*) signal->getDataPtr();
    subbPtr.p->m_senderRef = req->senderRef;
    subbPtr.p->m_senderData = req->senderData;
    subbPtr.p->m_errorCode = 0;
    subbPtr.p->m_gsn = GSN_SUB_REMOVE_REQ;
    subbPtr.p->m_subscriptionId = req->subscriptionId;
    subbPtr.p->m_subscriptionKey = req->subscriptionKey;
    subbPtr.p->m_subscriberRef = RNIL;
    subbPtr.p->m_subscriberData = RNIL;
  }

  CRASH_INSERTION2(6010, getOwnNodeId() != c_masterNodeId);

  SubRemoveReq* req = (SubRemoveReq*) signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = subbPtr.i;

  sendSignal(SUMA_REF, GSN_SUB_REMOVE_REQ, signal, SubRemoveReq::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

/*
 * Coordintor/Participant
 */

void
Dbdict::execSUB_REMOVE_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Dbdict::execSUB_REMOVE_REF");

  const SubRemoveRef* ref = (SubRemoveRef*) signal->getDataPtr();
  Uint32 senderRef = ref->senderRef;
  Uint32 err= ref->errorCode;

  if (refToBlock(senderRef) == SUMA)
  {
    /*
     * Participant
     */
    jam();
    OpSubEventPtr subbPtr;
    c_opSubEvent.getPtr(subbPtr, ref->senderData);
    if (err == SubRemoveRef::NoSuchSubscription)
    {
      jam();
      // conf this since this may occur if a nodefailure has occured
      // earlier so that the systable was not cleared
      SubRemoveConf* conf = (SubRemoveConf*) signal->getDataPtrSend();
      conf->senderRef  = reference();
      conf->senderData = subbPtr.p->m_senderData;
      sendSignal(subbPtr.p->m_senderRef, GSN_SUB_REMOVE_CONF,
		 signal, SubRemoveConf::SignalLength, JBB);
    }
    else
    {
      jam();

      if (upgrade_suma_NotStarted(err, subbPtr.p->m_senderRef))
      {
        jam();
        err = SubRemoveRef::NF_FakeErrorREF;
      }

      SubRemoveRef* ref = (SubRemoveRef*) signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->senderData = subbPtr.p->m_senderData;
      ref->errorCode = err;
      sendSignal(subbPtr.p->m_senderRef, GSN_SUB_REMOVE_REF,
		 signal, SubRemoveRef::SignalLength, JBB);
    }
    c_opSubEvent.release(subbPtr);
    DBUG_VOID_RETURN;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
  OpDropEventPtr eventRecPtr;
  c_opDropEvent.getPtr(eventRecPtr, ref->senderData);
  if (err == SubRemoveRef::NF_FakeErrorREF || err == SubRemoveRef::NotStarted)
  {
    jam();
    eventRecPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  }
  else
  {
    jam();
    if (eventRecPtr.p->m_errorCode == 0)
    {
      eventRecPtr.p->m_errorCode= err ? err : 1;
      eventRecPtr.p->m_errorLine= __LINE__;
      eventRecPtr.p->m_errorNode= reference();
    }
    eventRecPtr.p->m_reqTracker.reportRef(c_counterMgr, refToNode(senderRef));
  }
  completeSubRemoveReq(signal,eventRecPtr.i,0);
  DBUG_VOID_RETURN;
}

void
Dbdict::execSUB_REMOVE_CONF(Signal* signal)
{
  jamEntry();
  const SubRemoveConf* conf = (SubRemoveConf*) signal->getDataPtr();
  Uint32 senderRef = conf->senderRef;

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();
    OpSubEventPtr subbPtr;
    c_opSubEvent.getPtr(subbPtr, conf->senderData);
    SubRemoveConf* conf = (SubRemoveConf*) signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = subbPtr.p->m_senderData;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_REMOVE_CONF,
	       signal, SubRemoveConf::SignalLength, JBB);
    c_opSubEvent.release(subbPtr);
    return;
  }
  /*
   * Coordinator
   */
  ndbrequire(refToBlock(senderRef) == DBDICT);
  OpDropEventPtr eventRecPtr;
  c_opDropEvent.getPtr(eventRecPtr, conf->senderData);
  eventRecPtr.p->m_reqTracker.reportConf(c_counterMgr, refToNode(senderRef));
  completeSubRemoveReq(signal,eventRecPtr.i,0);
}

void
Dbdict::completeSubRemoveReq(Signal* signal, Uint32 ptrI, Uint32 xxx)
{
  OpDropEventPtr evntRecPtr;
  c_opDropEvent.getPtr(evntRecPtr, ptrI);

  if (!evntRecPtr.p->m_reqTracker.done()){
    jam();
    return;
  }

  if (evntRecPtr.p->m_reqTracker.hasRef()) {
    jam();
    if ( evntRecPtr.p->m_errorCode == 0 )
    {
      evntRecPtr.p->m_errorNode = reference();
      evntRecPtr.p->m_errorLine = __LINE__;
      evntRecPtr.p->m_errorCode = 1;
    }
    dropEvent_sendReply(signal, evntRecPtr);
    return;
  }

  Callback c = { safe_cast(&Dbdict::dropEventUTIL_PREPARE_DELETE), 0 };

  prepareTransactionEventSysTable(&c, signal, evntRecPtr.i,
				  UtilPrepareReq::Delete);
}

void
Dbdict::dropEventUTIL_PREPARE_DELETE(Signal* signal,
				     Uint32 callbackData,
				     Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode != 0) {
    EVENT_TRACE;
    dropEventUtilPrepareRef(signal, callbackData, returnCode);
    return;
  }

  UtilPrepareConf* const req = (UtilPrepareConf*)signal->getDataPtr();
  OpDropEventPtr evntRecPtr;
  jam();
  evntRecPtr.i = req->getSenderData();
  const Uint32 prepareId = req->getPrepareId();

  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);
#ifdef EVENT_DEBUG
  printf("DropEvntUTIL_PREPARE; evntRecPtr.i len %u\n",evntRecPtr.i);
#endif

  Callback c = { safe_cast(&Dbdict::dropEventUTIL_EXECUTE_DELETE), 0 };

  executeTransEventSysTable(&c, signal,
			    evntRecPtr.i, evntRecPtr.p->m_eventRec,
			    prepareId, UtilPrepareReq::Delete);
}

void
Dbdict::dropEventUTIL_EXECUTE_DELETE(Signal* signal,
				     Uint32 callbackData,
				     Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  if (returnCode != 0) {
    EVENT_TRACE;
    dropEventUtilExecuteRef(signal, callbackData, returnCode);
    return;
  }

  OpDropEventPtr evntRecPtr;
  UtilExecuteConf * const ref = (UtilExecuteConf *)signal->getDataPtr();
  jam();
  evntRecPtr.i = ref->getSenderData();
  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);

  dropEvent_sendReply(signal, evntRecPtr);
}

void
Dbdict::dropEventUtilPrepareRef(Signal* signal,
				Uint32 callbackData,
				Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  UtilPrepareRef * const ref = (UtilPrepareRef *)signal->getDataPtr();
  OpDropEventPtr evntRecPtr;
  evntRecPtr.i = ref->getSenderData();
  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);

  interpretUtilPrepareErrorCode((UtilPrepareRef::ErrorCode)ref->getErrorCode(),
				evntRecPtr.p->m_errorCode,
                                evntRecPtr.p->m_errorLine, this);
  evntRecPtr.p->m_errorNode = reference();

  dropEvent_sendReply(signal, evntRecPtr);
}

void
Dbdict::dropEventUtilExecuteRef(Signal* signal,
				Uint32 callbackData,
				Uint32 returnCode)
{
  jam();
  EVENT_TRACE;
  OpDropEventPtr evntRecPtr;
  UtilExecuteRef * const ref = (UtilExecuteRef *)signal->getDataPtr();
  jam();
  evntRecPtr.i = ref->getSenderData();
  ndbrequire((evntRecPtr.p = c_opDropEvent.getPtr(evntRecPtr.i)) != NULL);

  evntRecPtr.p->m_errorNode = reference();
  evntRecPtr.p->m_errorLine = __LINE__;

  switch (ref->getErrorCode()) {
  case UtilExecuteRef::TCError:
    switch (ref->getTCErrorCode()) {
    case ZNOT_FOUND:
      jam();
      evntRecPtr.p->m_errorCode = 4710;
      break;
    default:
      jam();
      evntRecPtr.p->m_errorCode = ref->getTCErrorCode();
      break;
    }
    break;
  default:
    jam();
    evntRecPtr.p->m_errorCode = ref->getErrorCode();
    break;
  }
  dropEvent_sendReply(signal, evntRecPtr);
}

void Dbdict::dropEvent_sendReply(Signal* signal,
				 OpDropEventPtr evntRecPtr)
{
  jam();
  EVENT_TRACE;
  Uint32 senderRef = evntRecPtr.p->m_request.getUserRef();

  if (evntRecPtr.p->hasError()) {
    jam();
    DropEvntRef * ret = (DropEvntRef *)signal->getDataPtrSend();

    ret->setUserData(evntRecPtr.p->m_request.getUserData());
    ret->setUserRef(evntRecPtr.p->m_request.getUserRef());

    ret->setErrorCode(evntRecPtr.p->m_errorCode);
    ret->setErrorLine(evntRecPtr.p->m_errorLine);
    ret->setErrorNode(evntRecPtr.p->m_errorNode);

    sendSignal(senderRef, GSN_DROP_EVNT_REF, signal,
	       DropEvntRef::SignalLength, JBB);
  } else {
    jam();
    DropEvntConf * evntConf = (DropEvntConf *)signal->getDataPtrSend();

    evntConf->setUserData(evntRecPtr.p->m_request.getUserData());
    evntConf->setUserRef(evntRecPtr.p->m_request.getUserRef());

    sendSignal(senderRef, GSN_DROP_EVNT_CONF, signal,
	       DropEvntConf::SignalLength, JBB);
  }

  c_opDropEvent.release(evntRecPtr);
}

// MODULE: CreateTrigger

const Dbdict::OpInfo
Dbdict::CreateTriggerRec::g_opInfo = {
  { 'C', 'T', 'r', 0 },
  ~RT_DBDICT_CREATE_TRIGGER,
  GSN_CREATE_TRIG_IMPL_REQ,
  CreateTrigImplReq::SignalLength,
  //
  &Dbdict::createTrigger_seize,
  &Dbdict::createTrigger_release,
  //
  &Dbdict::createTrigger_parse,
  &Dbdict::createTrigger_subOps,
  &Dbdict::createTrigger_reply,
  //
  &Dbdict::createTrigger_prepare,
  &Dbdict::createTrigger_commit,
  &Dbdict::createTrigger_complete,
  //
  &Dbdict::createTrigger_abortParse,
  &Dbdict::createTrigger_abortPrepare
};

bool
Dbdict::createTrigger_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateTriggerRec>(op_ptr);
}

void
Dbdict::createTrigger_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateTriggerRec>(op_ptr);
}

void
Dbdict::execCREATE_TRIG_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CreateTrigReq req_copy =
    *(const CreateTrigReq*)signal->getDataPtr();
  const CreateTrigReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateTriggerRecPtr createTriggerPtr;
    CreateTrigImplReq* impl_req;

    startClientReq(op_ptr, createTriggerPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;
    impl_req->indexId = req->indexId;
    impl_req->indexVersion = req->indexVersion;
    impl_req->triggerNo = req->triggerNo;
    impl_req->triggerId = req->forceTriggerId;
    impl_req->triggerInfo = req->triggerInfo;
    impl_req->receiverRef = req->receiverRef;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateTrigRef* ref = (CreateTrigRef*)signal->getDataPtrSend();

  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->tableId = req->tableId;
  ref->indexId = req->indexId;
  ref->triggerInfo = req->triggerInfo;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_CREATE_TRIG_REF, signal,
             CreateTrigRef::SignalLength, JBB);
}

// CreateTrigger: PARSE

void
Dbdict::createTrigger_parse(Signal* signal, bool master,
                            SchemaOpPtr op_ptr,
                            SectionHandle& handle, ErrorInfo& error)
{
  D("createTrigger_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch (CreateTrigReq::getOnlineFlag(requestType)) {
  case CreateTrigImplReq::CreateTriggerOnline:
    break;
  case CreateTrigImplReq::CreateTriggerOffline:
    jam();
  default:
    ndbassert(false);
    setError(error, CreateTrigRef::BadRequestType, __LINE__);
    return;
  }

  // check the table
  {
    const Uint32 tableId = impl_req->tableId;
    if (! (tableId < c_noOfMetaTables))
    {
      jam();
      setError(error, CreateTrigRef::InvalidTable, __LINE__);
      return;
    }

    Uint32 err;
    if ((err = check_read_obj(tableId, trans_ptr.p->m_transId)))
    {
      jam();
      setError(error, err, __LINE__);
      return;
    }
  }

  switch(CreateTrigReq::getEndpointFlag(requestType)){
  case CreateTrigReq::MainTrigger:
    jam();
    createTriggerPtr.p->m_main_op = true;
    break;
  case CreateTrigReq::TriggerDst:
    jam();
  case CreateTrigReq::TriggerSrc:
    jam();
    if (handle.m_cnt)
    {
      jam();
      ndbassert(false);
      setError(error, CreateTrigRef::BadRequestType, __LINE__);
      return;
    }
    createTriggerPtr.p->m_main_op = false;
    createTrigger_parse_endpoint(signal, op_ptr, error);
    return;
  }

  SegmentedSectionPtr ss_ptr;
  handle.getSection(ss_ptr, CreateTrigReq::TRIGGER_NAME_SECTION);
  SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
  DictTabInfo::Table tableDesc;
  tableDesc.init();
  SimpleProperties::UnpackStatus status =
    SimpleProperties::unpack(r, &tableDesc,
			     DictTabInfo::TableMapping,
			     DictTabInfo::TableMappingSize,
			     true, true);

  if (status != SimpleProperties::Eof ||
      tableDesc.TableName[0] == 0)
  {
    jam();
    ndbassert(false);
    setError(error, CreateTrigRef::InvalidName, __LINE__);
    return;
  }
  const Uint32 bytesize = sizeof(createTriggerPtr.p->m_triggerName);
  memcpy(createTriggerPtr.p->m_triggerName, tableDesc.TableName, bytesize);
  D("trigger " << createTriggerPtr.p->m_triggerName);

  // check name is unique
  if (get_object(createTriggerPtr.p->m_triggerName) != 0) {
    jam();
    D("duplicate trigger name " << createTriggerPtr.p->m_triggerName);
    setError(error, CreateTrigRef::TriggerExists, __LINE__);
    return;
  }

  TriggerRecordPtr triggerPtr;
  if (master)
  {
    jam();
    if (impl_req->triggerId == RNIL)
    {
      jam();
      impl_req->triggerId = getFreeTriggerRecord();
      if (impl_req->triggerId == RNIL)
      {
        jam();
        setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
        return;
      }
      bool ok = find_object(triggerPtr, impl_req->triggerId);
      if (ok)
      {
        jam();
        setError(error, CreateTrigRef::TriggerExists, __LINE__);
        return;
      }
      D("master allocated triggerId " << impl_req->triggerId);
    }
    else
    {
      if (!(impl_req->triggerId < c_triggerRecordPool_.getSize()))
      {
	jam();
	setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
	return;
      }
      bool ok = find_object(triggerPtr, impl_req->triggerId);
      if (ok)
      {
        jam();
        setError(error, CreateTrigRef::TriggerExists, __LINE__);
        return;
      }
      D("master forced triggerId " << impl_req->triggerId);
    }
  }
  else
  {
    jam();
    // slave receives trigger id from master
    if (! (impl_req->triggerId < c_triggerRecordPool_.getSize()))
    {
      jam();
      setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
      return;
    }
    bool ok = find_object(triggerPtr, impl_req->triggerId);
    if (ok)
    {
      jam();
      setError(error, CreateTrigRef::TriggerExists, __LINE__);
      return;
    }
    D("slave allocated triggerId " << hex << impl_req->triggerId);
  }

  bool ok = seizeTriggerRecord(triggerPtr, impl_req->triggerId);
  if (!ok)
  {
    jam();
    setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
    return;
  }
  triggerPtr.p->tableId = impl_req->tableId;
  triggerPtr.p->indexId = RNIL; // feedback method connects to index
  triggerPtr.p->triggerInfo = impl_req->triggerInfo;
  triggerPtr.p->receiverRef = impl_req->receiverRef;
  triggerPtr.p->triggerState = TriggerRecord::TS_DEFINING;

  // TODO:msundell on failure below, leak of TriggerRecord
  if (handle.m_cnt >= 2)
  {
    jam();
    SegmentedSectionPtr mask_ptr;
    handle.getSection(mask_ptr, CreateTrigReq::ATTRIBUTE_MASK_SECTION);
    if (mask_ptr.sz > triggerPtr.p->attributeMask.getSizeInWords())
    {
      jam();
      setError(error, CreateTrigRef::BadRequestType, __LINE__);
      return;
    }
    ::copy(triggerPtr.p->attributeMask.rep.data, mask_ptr);
    if (mask_ptr.sz < triggerPtr.p->attributeMask.getSizeInWords())
    {
      jam();
      Uint32 len = triggerPtr.p->attributeMask.getSizeInWords() - mask_ptr.sz;
      bzero(triggerPtr.p->attributeMask.rep.data + mask_ptr.sz,
            4 * len);
    }
  }
  else
  {
    jam();
    setError(error, CreateTrigRef::BadRequestType, __LINE__);
    return;
  }

  {
    LocalRope name(c_rope_pool, triggerPtr.p->triggerName);
    if (!name.assign(createTriggerPtr.p->m_triggerName)) {
      jam();
      setError(error, CreateTrigRef::OutOfStringBuffer, __LINE__);
      return;
    }
  }

  // connect to new DictObject
  {
    DictObjectPtr obj_ptr;
    seizeDictObject(op_ptr, obj_ptr, triggerPtr.p->triggerName); // added to c_obj_name_hash

    obj_ptr.p->m_id = impl_req->triggerId; // wl3600_todo id
    obj_ptr.p->m_type =
      TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
    link_object(obj_ptr, triggerPtr);
    c_obj_id_hash.add(obj_ptr);
  }

  {
    TriggerType::Value type =
      TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
    switch(type){
    case TriggerType::REORG_TRIGGER:
      jam();
      createTrigger_create_drop_trigger_operation(signal, op_ptr, error);
    case TriggerType::SECONDARY_INDEX:
      jam();
      createTriggerPtr.p->m_sub_dst = false;
      createTriggerPtr.p->m_sub_src = false;
      break;
    case TriggerType::ORDERED_INDEX:
    case TriggerType::READ_ONLY_CONSTRAINT:
      jam();
      createTriggerPtr.p->m_sub_dst = true; // Only need LQH
      createTriggerPtr.p->m_sub_src = false;
      break;
    default:
    case TriggerType::SUBSCRIPTION:
    case TriggerType::SUBSCRIPTION_BEFORE:
      ndbassert(false);
      setError(error, CreateTrigRef::UnsupportedTriggerType, __LINE__);
      return;
    }
  }

  if (ERROR_INSERTED(6124))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9124, __LINE__);
    return;
  }

  if (impl_req->indexId != RNIL)
  {
    TableRecordPtr indexPtr;
    bool ok = find_object(indexPtr, impl_req->indexId);
    ndbrequire(ok);
    triggerPtr.p->indexId = impl_req->indexId;
    indexPtr.p->triggerId = impl_req->triggerId;
  }
}

void
Dbdict::createTrigger_parse_endpoint(Signal* signal,
				     SchemaOpPtr op_ptr,
				     ErrorInfo& error)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch(CreateTrigReq::getEndpointFlag(requestType)){
  case CreateTrigReq::TriggerDst:
    jam();
    createTriggerPtr.p->m_block_list[0] = DBTC_REF;
    break;
  case CreateTrigReq::TriggerSrc:
    jam();
    createTriggerPtr.p->m_block_list[0] = DBLQH_REF;
    break;
  default:
    ndbassert(false);
    setError(error, CreateTrigRef::BadRequestType, __LINE__);
    return;
  }

  TriggerRecordPtr triggerPtr;
  bool ok = find_object(triggerPtr, impl_req->triggerId);
  if (!ok)
  {
    jam();
    return;
  }
  switch(TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo)){
  case TriggerType::REORG_TRIGGER:
    jam();
    createTrigger_create_drop_trigger_operation(signal, op_ptr, error);
    return;
  default:
    return;
  }
}

void
Dbdict::createTrigger_create_drop_trigger_operation(Signal* signal,
                                                    SchemaOpPtr op_ptr,
                                                    ErrorInfo& error)
{
  jam();

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  /**
   * Construct a dropTrigger operation
   */
  SchemaOpPtr oplnk_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  if(!seizeLinkedSchemaOp(op_ptr, oplnk_ptr, dropTriggerPtr))
  {
    jam();
    setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
    return;
  }


  DropTrigImplReq* aux_impl_req = &dropTriggerPtr.p->m_request;
  aux_impl_req->senderRef = reference();
  aux_impl_req->senderData = op_ptr.p->op_key;
  aux_impl_req->requestType = 0;
  aux_impl_req->tableId = impl_req->tableId;
  aux_impl_req->tableVersion = 0; // not used
  aux_impl_req->indexId = 0;
  aux_impl_req->indexVersion = 0; // not used
  aux_impl_req->triggerNo = 0;
  aux_impl_req->triggerId = impl_req->triggerId;
  aux_impl_req->triggerInfo = impl_req->triggerInfo;

  dropTriggerPtr.p->m_main_op = createTriggerPtr.p->m_main_op;

  if (createTriggerPtr.p->m_main_op)
  {
    jam();
    return;
  }

  switch(refToBlock(createTriggerPtr.p->m_block_list[0])){
  case DBTC:
    jam();
    dropTriggerPtr.p->m_block_list[0] = DBLQH_REF;
    break;
  case DBLQH:
    jam();
    dropTriggerPtr.p->m_block_list[0] = DBTC_REF;
    break;
  default:
    ndbassert(false);
    setError(error, CreateTrigRef::BadRequestType, __LINE__);
    return;
  }
}

bool
Dbdict::createTrigger_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_subOps" << V(op_ptr.i) << *op_ptr.p);

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);

  Uint32 requestType =
    DictSignal::getRequestType(createTriggerPtr.p->m_request.requestType);
  switch(CreateTrigReq::getEndpointFlag(requestType)){
  case CreateTrigReq::MainTrigger:
    jam();
    break;
  case CreateTrigReq::TriggerDst:
  case CreateTrigReq::TriggerSrc:
    jam();
    return false;
  }

  /**
   * NOTE create dst before src
   */
  if (!createTriggerPtr.p->m_sub_dst)
  {
    jam();
    createTriggerPtr.p->m_sub_dst = true;
    Callback c = {
      safe_cast(&Dbdict::createTrigger_fromCreateEndpoint),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    createTrigger_toCreateEndpoint(signal, op_ptr,
				   CreateTrigReq::TriggerDst);
    return true;
  }

  if (!createTriggerPtr.p->m_sub_src)
  {
    jam();
    createTriggerPtr.p->m_sub_src = true;
    Callback c = {
      safe_cast(&Dbdict::createTrigger_fromCreateEndpoint),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    createTrigger_toCreateEndpoint(signal, op_ptr,
				   CreateTrigReq::TriggerSrc);
    return true;
  }

  return false;
}

void
Dbdict::createTrigger_toCreateEndpoint(Signal* signal,
				       SchemaOpPtr op_ptr,
				       CreateTrigReq::EndpointFlag endpoint)
{
  D("alterIndex_toCreateTrigger");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  CreateTrigReq* req = (CreateTrigReq*)signal->getDataPtrSend();

  Uint32 type = 0;
  CreateTrigReq::setOnlineFlag(type, CreateTrigReq::CreateTriggerOnline);
  CreateTrigReq::setEndpointFlag(type, endpoint);

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, type);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->indexId = impl_req->indexId;
  req->indexVersion = impl_req->indexVersion;
  req->forceTriggerId = impl_req->triggerId;
  req->triggerInfo = impl_req->triggerInfo;

  sendSignal(reference(), GSN_CREATE_TRIG_REQ, signal,
             CreateTrigReq::SignalLength, JBB);
}

void
Dbdict::createTrigger_fromCreateEndpoint(Signal* signal,
					 Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  findSchemaOp(op_ptr, createTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0)
  {
    jam();
    const CreateTrigConf* conf =
      (const CreateTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  }
  else
  {
    jam();
    const CreateTrigRef* ref =
      (const CreateTrigRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::createTrigger_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  D("createTrigger_reply" << V(impl_req->triggerId) << *op_ptr.p);

  if (!hasError(error)) {
    CreateTrigConf* conf = (CreateTrigConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = impl_req->tableId;
    conf->indexId = impl_req->indexId;
    conf->triggerId = impl_req->triggerId;
    conf->triggerInfo = impl_req->triggerInfo;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_TRIG_CONF, signal,
               CreateTrigConf::SignalLength, JBB);
  } else {
    jam();
    CreateTrigRef* ref = (CreateTrigRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    ref->tableId = impl_req->tableId;
    ref->indexId = impl_req->indexId;
    ref->triggerInfo =impl_req->triggerInfo;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_TRIG_REF, signal,
               CreateTrigRef::SignalLength, JBB);
  }
}

// CreateTrigger: PREPARE

void
Dbdict::createTrigger_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);

  if (createTriggerPtr.p->m_main_op)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  if (ERROR_INSERTED(6213))
  {
    CLEAR_ERROR_INSERT_VALUE;
    setError(op_ptr, 1, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  Callback c =  { safe_cast(&Dbdict::createTrigger_prepare_fromLocal),
                  op_ptr.p->op_key
  };

  op_ptr.p->m_callback = c;
  send_create_trig_req(signal, op_ptr);
}

void
Dbdict::createTrigger_prepare_fromLocal(Signal* signal,
                                        Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  findSchemaOp(op_ptr, createTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0)
  {
    jam();
    createTriggerPtr.p->m_created = true;
    sendTransConf(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

// CreateTrigger: COMMIT

void
Dbdict::createTrigger_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  if (!op_ptr.p->m_oplnk_ptr.isNull())
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  if (createTriggerPtr.p->m_main_op)
  {
    jam();

    Uint32 triggerId = impl_req->triggerId;
    TriggerRecordPtr triggerPtr;
    bool ok = find_object(triggerPtr, triggerId);
    ndbrequire(ok);
    triggerPtr.p->triggerState = TriggerRecord::TS_ONLINE;
    unlinkDictObject(op_ptr);
  }

  sendTransConf(signal, op_ptr);
}

// CreateTrigger: COMPLETE

void
Dbdict::createTrigger_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);

  if (!op_ptr.p->m_oplnk_ptr.isNull())
  {
    jam();

    /**
     * Create trigger commit...will drop trigger
     */
    if (hasDictObject(op_ptr))
    {
      jam();
      DictObjectPtr obj_ptr;
      getDictObject(op_ptr, obj_ptr);
      unlinkDictObject(op_ptr);
      linkDictObject(op_ptr.p->m_oplnk_ptr, obj_ptr);
    }
    op_ptr.p->m_oplnk_ptr.p->m_state = SchemaOp::OS_COMPLETING;
    dropTrigger_commit(signal, op_ptr.p->m_oplnk_ptr);
    return;
  }

  sendTransConf(signal, op_ptr);
}

// CreateTrigger: ABORT

void
Dbdict::createTrigger_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_abortParse" << *op_ptr.p);

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;
  Uint32 triggerId = impl_req->triggerId;

  if (createTriggerPtr.p->m_main_op)
  {
    jam();

    TriggerRecordPtr triggerPtr;
    if (! (triggerId < c_triggerRecordPool_.getSize()))
    {
      jam();
      goto done;
    }

    bool ok = find_object(triggerPtr, triggerId);
    if (ok)
    {
      jam();

      if (triggerPtr.p->indexId != RNIL)
      {
        TableRecordPtr indexPtr;
        bool ok = find_object(indexPtr, triggerPtr.p->indexId);
        if (ok)
        {
          jam();
          indexPtr.p->triggerId = RNIL;
        }
        triggerPtr.p->indexId = RNIL;
      }

      c_triggerRecordPool_.release(triggerPtr);
    }

    // ignore Feedback for now (referencing object will be dropped too)

    if (hasDictObject(op_ptr))
    {
      jam();
      releaseDictObject(op_ptr);
    }
  }

done:

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createTrigger_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_abortPrepare" << *op_ptr.p);

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  if (createTriggerPtr.p->m_main_op)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  if (createTriggerPtr.p->m_created == false)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  Callback c =  { safe_cast(&Dbdict::createTrigger_abortPrepare_fromLocal),
		  op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = 0;
  req->tableId = impl_req->tableId;
  req->tableVersion = 0; // not used
  req->indexId = impl_req->indexId;
  req->indexVersion = 0; // not used
  req->triggerNo = 0;
  req->triggerId = impl_req->triggerId;
  req->triggerInfo = impl_req->triggerInfo;
  req->receiverRef = impl_req->receiverRef;

  BlockReference ref = createTriggerPtr.p->m_block_list[0];
  sendSignal(ref, GSN_DROP_TRIG_IMPL_REQ, signal,
             DropTrigImplReq::SignalLength, JBB);
}

void
Dbdict::createTrigger_abortPrepare_fromLocal(Signal* signal,
                                             Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  findSchemaOp(op_ptr, createTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  ndbrequire(ret == 0); // abort can't fail

  sendTransConf(signal, op_ptr);
}

void
Dbdict::send_create_trig_req(Signal* signal,
                             SchemaOpPtr op_ptr)
{
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  TriggerRecordPtr triggerPtr;
  bool ok = find_object(triggerPtr, impl_req->triggerId);
  ndbrequire(ok);
  D("send_create_trig_req");

  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = 0;
  req->tableId = triggerPtr.p->tableId;
  req->tableVersion = 0; // not used
  req->indexId = triggerPtr.p->indexId;
  req->indexVersion = 0; // not used
  req->triggerNo = 0; // not used
  req->triggerId = triggerPtr.p->triggerId;
  req->triggerInfo = triggerPtr.p->triggerInfo;
  req->receiverRef = triggerPtr.p->receiverRef;

  {
    /**
     * Handle the upgrade...
     */
    Uint32 tmp[3];
    tmp[0] = triggerPtr.p->triggerId;
    tmp[1] = triggerPtr.p->triggerId;
    tmp[2] = triggerPtr.p->triggerId;

    TableRecordPtr indexPtr;
    if (triggerPtr.p->indexId != RNIL)
    {
      jam();
      bool ok = find_object(indexPtr, triggerPtr.p->indexId);
      ndbrequire(ok);
      if (indexPtr.p->m_upgrade_trigger_handling.m_upgrade)
      {
        jam();
        tmp[0] = indexPtr.p->m_upgrade_trigger_handling.insertTriggerId;
        tmp[1] = indexPtr.p->m_upgrade_trigger_handling.updateTriggerId;
        tmp[2] = indexPtr.p->m_upgrade_trigger_handling.deleteTriggerId;
      }
    }
    req->upgradeExtra[0] = tmp[0];
    req->upgradeExtra[1] = tmp[1];
    req->upgradeExtra[2] = tmp[2];
  }

  LinearSectionPtr ptr[3];
  ptr[0].p = triggerPtr.p->attributeMask.rep.data;
  ptr[0].sz = triggerPtr.p->attributeMask.getSizeInWords();

  BlockReference ref = createTriggerPtr.p->m_block_list[0];
  sendSignal(ref, GSN_CREATE_TRIG_IMPL_REQ, signal,
             CreateTrigImplReq::SignalLength, JBB, ptr, 1);
}

// CreateTrigger: MISC

void
Dbdict::execCREATE_TRIG_CONF(Signal* signal)
{
  jamEntry();
  const CreateTrigConf* conf = (const CreateTrigConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execCREATE_TRIG_REF(Signal* signal)
{
  jamEntry();
  const CreateTrigRef* ref = (const CreateTrigRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplConf* conf = (const CreateTrigImplConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplRef* ref = (const CreateTrigImplRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

// CreateTrigger: END

// MODULE: DropTrigger

const Dbdict::OpInfo
Dbdict::DropTriggerRec::g_opInfo = {
  { 'D', 'T', 'r', 0 },
  ~RT_DBDICT_DROP_TRIGGER,
  GSN_DROP_TRIG_IMPL_REQ,
  DropTrigImplReq::SignalLength,
  //
  &Dbdict::dropTrigger_seize,
  &Dbdict::dropTrigger_release,
  //
  &Dbdict::dropTrigger_parse,
  &Dbdict::dropTrigger_subOps,
  &Dbdict::dropTrigger_reply,
  //
  &Dbdict::dropTrigger_prepare,
  &Dbdict::dropTrigger_commit,
  &Dbdict::dropTrigger_complete,
  //
  &Dbdict::dropTrigger_abortParse,
  &Dbdict::dropTrigger_abortPrepare
};

bool
Dbdict::dropTrigger_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropTriggerRec>(op_ptr);
}

void
Dbdict::dropTrigger_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropTriggerRec>(op_ptr);
}

void
Dbdict::execDROP_TRIG_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const DropTrigReq req_copy =
    *(const DropTrigReq*)signal->getDataPtr();
  const DropTrigReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropTriggerRecPtr dropTriggerPtr;
    DropTrigImplReq* impl_req;

    startClientReq(op_ptr, dropTriggerPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;
    impl_req->indexId = req->indexId;
    impl_req->indexVersion = req->indexVersion;
    impl_req->triggerNo = req->triggerNo;
    impl_req->triggerId = req->triggerId;
    impl_req->triggerInfo = ~(Uint32)0;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropTrigRef* ref = (DropTrigRef*)signal->getDataPtrSend();

  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->tableId = req->tableId;
  ref->indexId = req->indexId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_DROP_TRIG_REF, signal,
             DropTrigRef::SignalLength, JBB);
}

// DropTrigger: PARSE

void
Dbdict::dropTrigger_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  D("dropTrigger_parse" << V(op_ptr.i) << *op_ptr.p);

  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  bool reqByName = false;
  if (handle.m_cnt > 0) { // wl3600_todo use requestType
    jam();
    ndbrequire(handle.m_cnt == 1);
    reqByName = true;
  }

  if (reqByName) {
    jam();
    SegmentedSectionPtr ss_ptr;
    handle.getSection(ss_ptr, DropTrigImplReq::TRIGGER_NAME_SECTION);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
    DictTabInfo::Table tableDesc;
    tableDesc.init();
    SimpleProperties::UnpackStatus status =
      SimpleProperties::unpack(
          r, &tableDesc,
          DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
          true, true);

    if (status != SimpleProperties::Eof ||
        tableDesc.TableName[0] == 0)
    {
      jam();
      ndbassert(false);
      setError(error, DropTrigRef::InvalidName, __LINE__);
      return;
    }
    const Uint32 bytesize = sizeof(dropTriggerPtr.p->m_triggerName);
    memcpy(dropTriggerPtr.p->m_triggerName, tableDesc.TableName, bytesize);
    D("parsed trigger name: " << dropTriggerPtr.p->m_triggerName);

    // find object by name and link it to operation
    DictObjectPtr obj_ptr;
    if (!findDictObject(op_ptr, obj_ptr, dropTriggerPtr.p->m_triggerName)) {
      jam();
      setError(error, DropTrigRef::TriggerNotFound, __LINE__);
      return;
    }
    if (impl_req->triggerId != RNIL &&
        impl_req->triggerId != obj_ptr.p->m_id) {
      jam();
      // inconsistency in request
      setError(error, DropTrigRef::TriggerNotFound, __LINE__);
      return;
    }
    impl_req->triggerId = obj_ptr.p->m_id;
  }

  // check trigger id from user or via name
  TriggerRecordPtr triggerPtr;
  {
    if (!(impl_req->triggerId < c_triggerRecordPool_.getSize())) {
      jam();
      setError(error, DropTrigImplRef::TriggerNotFound, __LINE__);
      return;
    }
    bool ok = find_object(triggerPtr, impl_req->triggerId);
    if (!ok)
    {
      jam();
      setError(error, DropTrigImplRef::TriggerNotFound, __LINE__);
      return;
    }
    // wl3600_todo state check
  }

  D("trigger " << copyRope<MAX_TAB_NAME_SIZE>(triggerPtr.p->triggerName));
  impl_req->triggerInfo = triggerPtr.p->triggerInfo;
  Uint32 requestType = impl_req->requestType;

  switch(DropTrigReq::getEndpointFlag(requestType)){
  case DropTrigReq::MainTrigger:
    jam();
    dropTriggerPtr.p->m_main_op = true;
    break;
  case DropTrigReq::TriggerDst:
  case DropTrigReq::TriggerSrc:
    jam();
    if (handle.m_cnt)
    {
      jam();
      ndbassert(false);
      setError(error, DropTrigRef::BadRequestType, __LINE__);
      return;
    }
    dropTriggerPtr.p->m_main_op = false;
    dropTrigger_parse_endpoint(signal, op_ptr, error);
    return;
  }

  // connect to object in request by id
  if (!reqByName) {
    jam();
    DictObjectPtr obj_ptr;
    if (!findDictObject(op_ptr, obj_ptr, triggerPtr.p->m_obj_ptr_i)) {
      jam();
      // broken trigger object wl3600_todo bad error code
      setError(error, DropTrigRef::TriggerNotFound, __LINE__);
      return;
    }
    ndbrequire(obj_ptr.p->m_id == triggerPtr.p->triggerId);

    // fill in name just to be complete
    ConstRope name(c_rope_pool, triggerPtr.p->triggerName);
    name.copy(dropTriggerPtr.p->m_triggerName); //wl3600_todo length check
  }

  // check the table (must match trigger record)
  {
    if (impl_req->tableId != triggerPtr.p->tableId) {
      jam();
      setError(error, DropTrigRef::InvalidTable, __LINE__);
      return;
    }
  }

  // check the index (must match trigger record, maybe both RNIL)
  {
    if (impl_req->indexId != triggerPtr.p->indexId) {
      jam();  // wl3600_todo wrong error code
      setError(error, DropTrigRef::InvalidTable, __LINE__);
      return;
    }
  }

  {
    TriggerType::Value type =
      TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
    switch(type){
    case TriggerType::SECONDARY_INDEX:
      jam();
      dropTriggerPtr.p->m_sub_dst = false;
      dropTriggerPtr.p->m_sub_src = false;
      break;
    case TriggerType::ORDERED_INDEX:
    case TriggerType::READ_ONLY_CONSTRAINT:
      jam();
      dropTriggerPtr.p->m_sub_dst = true; // Only need LQH
      dropTriggerPtr.p->m_sub_src = false;
      break;
    default:
    case TriggerType::SUBSCRIPTION:
    case TriggerType::SUBSCRIPTION_BEFORE:
      ndbassert(false);
      setError(error, DropTrigRef::UnsupportedTriggerType, __LINE__);
      return;
    }
  }

  if (ERROR_INSERTED(6124)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 9124, __LINE__);
    return;
  }
}

void
Dbdict::dropTrigger_parse_endpoint(Signal* signal,
				   SchemaOpPtr op_ptr,
				   ErrorInfo& error)
{
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch(DropTrigReq::getEndpointFlag(requestType)){
  case DropTrigReq::TriggerDst:
    jam();
    dropTriggerPtr.p->m_block_list[0] = DBTC_REF;
    break;
  case DropTrigReq::TriggerSrc:
    jam();
    dropTriggerPtr.p->m_block_list[0] = DBLQH_REF;
    break;
  default:
    ndbassert(false);
    setError(error, DropTrigRef::BadRequestType, __LINE__);
    return;
  }
}

bool
Dbdict::dropTrigger_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTrigger_subOps" << V(op_ptr.i) << *op_ptr.p);

  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);

  Uint32 requestType = dropTriggerPtr.p->m_request.requestType;
  switch(DropTrigReq::getEndpointFlag(requestType)){
  case DropTrigReq::MainTrigger:
    jam();
    break;
  case DropTrigReq::TriggerDst:
  case DropTrigReq::TriggerSrc:
    jam();
    return false;
  }

  /**
   * NOTE drop src before dst
   */
  if (!dropTriggerPtr.p->m_sub_src)
  {
    jam();
    dropTriggerPtr.p->m_sub_src = true;
    Callback c = {
      safe_cast(&Dbdict::dropTrigger_fromDropEndpoint),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    dropTrigger_toDropEndpoint(signal, op_ptr,
			       DropTrigReq::TriggerSrc);
    return true;
  }

  if (!dropTriggerPtr.p->m_sub_dst)
  {
    jam();
    dropTriggerPtr.p->m_sub_dst = true;
    Callback c = {
      safe_cast(&Dbdict::dropTrigger_fromDropEndpoint),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    dropTrigger_toDropEndpoint(signal, op_ptr,
			       DropTrigReq::TriggerDst);
    return true;
  }

  return false;
}

void
Dbdict::dropTrigger_toDropEndpoint(Signal* signal,
				   SchemaOpPtr op_ptr,
				   DropTrigReq::EndpointFlag endpoint)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  DropTrigReq* req = (DropTrigReq*)signal->getDataPtrSend();

  Uint32 requestType = 0;
  DropTrigReq::setEndpointFlag(requestType, endpoint);

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, requestType);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->indexId = impl_req->indexId;
  req->indexVersion = impl_req->indexVersion;
  req->triggerId = impl_req->triggerId;
  req->triggerNo = 0;

  sendSignal(reference(), GSN_DROP_TRIG_REQ, signal,
             DropTrigReq::SignalLength, JBB);
}

void
Dbdict::dropTrigger_fromDropEndpoint(Signal* signal,
					 Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  findSchemaOp(op_ptr, dropTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0)
  {
    jam();
    const DropTrigConf* conf =
      (const DropTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createSubOps(signal, op_ptr);
  }
  else
  {
    jam();
    const DropTrigRef* ref =
      (const DropTrigRef*)signal->getDataPtr();
    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::dropTrigger_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  D("dropTrigger_reply" << V(impl_req->triggerId));

  if (!hasError(error)) {
    DropTrigConf* conf = (DropTrigConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = impl_req->tableId;
    conf->indexId = impl_req->indexId;
    conf->triggerId = impl_req->triggerId;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_TRIG_CONF, signal,
               DropTrigConf::SignalLength, JBB);
  } else {
    jam();
    DropTrigRef* ref = (DropTrigRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    ref->tableId = impl_req->tableId;
    ref->indexId = impl_req->indexId;
    ref->triggerId = impl_req->triggerId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_TRIG_REF, signal,
               DropTrigRef::SignalLength, JBB);
  }
}

// DropTrigger: PREPARE

void
Dbdict::dropTrigger_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("dropTrigger_prepare");

  /**
   * This could check that triggers actually are present
   */

  sendTransConf(signal, op_ptr);
}

// DropTrigger: COMMIT

void
Dbdict::dropTrigger_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);

  if (dropTriggerPtr.p->m_main_op)
  {
    jam();

    Uint32 triggerId = dropTriggerPtr.p->m_request.triggerId;

    TriggerRecordPtr triggerPtr;
    bool ok = find_object(triggerPtr, triggerId);
    ndbrequire(ok);
    if (triggerPtr.p->indexId != RNIL)
    {
      jam();
      TableRecordPtr indexPtr;
      bool ok = find_object(indexPtr, triggerPtr.p->indexId);
      if (ok)
      {
        jam();
        indexPtr.p->triggerId = RNIL;
      }
      triggerPtr.p->indexId = RNIL;
    }

    // remove trigger
    c_triggerRecordPool_.release(triggerPtr);
    releaseDictObject(op_ptr);

    sendTransConf(signal, op_ptr);
    return;
  }

  Callback c =  { safe_cast(&Dbdict::dropTrigger_commit_fromLocal),
                  op_ptr.p->op_key
  };

  op_ptr.p->m_callback = c;
  send_drop_trig_req(signal, op_ptr);
}

void
Dbdict::dropTrigger_commit_fromLocal(Signal* signal,
                                       Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  findSchemaOp(op_ptr, dropTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret)
  {
    jam();
    warningEvent("Got error code %u from %x (DROP_TRIG_IMPL_REQ)",
                 ret,
                 dropTriggerPtr.p->m_block_list[0]);
  }

  sendTransConf(signal, op_ptr);
}

void
Dbdict::send_drop_trig_req(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = impl_req->requestType;
  req->tableId = impl_req->tableId;
  req->tableVersion = 0; // not used
  req->indexId = impl_req->indexId;
  req->indexVersion = 0; // not used
  req->triggerNo = 0;
  req->triggerId = impl_req->triggerId;
  req->triggerInfo = impl_req->triggerInfo;
  req->receiverRef = impl_req->receiverRef;

  BlockReference ref = dropTriggerPtr.p->m_block_list[0];
  sendSignal(ref, GSN_DROP_TRIG_IMPL_REQ, signal,
             DropTrigImplReq::SignalLength, JBB);
}

// DropTrigger: COMPLETE

void
Dbdict::dropTrigger_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// DropTrigger: ABORT

void
Dbdict::dropTrigger_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);

  D("dropTrigger_abortParse" << *op_ptr.p);

  if (hasDictObject(op_ptr)) {
    jam();
    unlinkDictObject(op_ptr);
  }
  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropTrigger_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTrigger_abortPrepare" << *op_ptr.p);

  sendTransConf(signal, op_ptr);
}

// DropTrigger: MISC

void
Dbdict::execDROP_TRIG_CONF(Signal* signal)
{
  jamEntry();
  const DropTrigConf* conf = (const DropTrigConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execDROP_TRIG_REF(Signal* signal)
{
  jamEntry();
  const DropTrigRef* ref = (const DropTrigRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const DropTrigImplConf* conf = (const DropTrigImplConf*)signal->getDataPtr();
  ndbrequire(refToNode(conf->senderRef) == getOwnNodeId());
  handleDictConf(signal, conf);
}

void
Dbdict::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();
  const DropTrigImplRef* ref = (const DropTrigImplRef*)signal->getDataPtr();
  ndbrequire(refToNode(ref->senderRef) == getOwnNodeId());
  handleDictRef(signal, ref);
}

// DropTrigger: END


/**
 * MODULE: Support routines for index and trigger.
 */

/*
  This routine is used to set-up the primary key attributes of the unique
  hash index. Since we store fragment id as part of the primary key here
  we insert the pseudo column for getting fragment id first in the array.
  This routine is used as part of the building of the index.
*/

void
Dbdict::getTableKeyList(TableRecordPtr tablePtr,
			Id_array<MAX_ATTRIBUTES_IN_INDEX+1>& list)
{
  jam();
  list.sz = 0;
  list.id[list.sz++] = AttributeHeader::FRAGMENT;
  LocalAttributeRecord_list alist(c_attributeRecordPool,
                                         tablePtr.p->m_attributes);
  AttributeRecordPtr attrPtr;
  for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr)) {
    if (attrPtr.p->tupleKey) {
      list.id[list.sz++] = attrPtr.p->attributeId;
    }
  }
  ndbrequire(list.sz == (uint)(tablePtr.p->noOfPrimkey + 1));
  ndbrequire(list.sz <= MAX_ATTRIBUTES_IN_INDEX + 1);
}

// XXX should store the primary attribute id
void
Dbdict::getIndexAttr(TableRecordPtr indexPtr, Uint32 itAttr, Uint32* id)
{
  jam();

  Uint32 len;
  char name[MAX_ATTR_NAME_SIZE];
  TableRecordPtr tablePtr;
  AttributeRecordPtr attrPtr;

  bool ok = find_object(tablePtr, indexPtr.p->primaryTableId);
  ndbrequire(ok);
  AttributeRecord* iaRec = c_attributeRecordPool.getPtr(itAttr);
  {
    ConstRope tmp(c_rope_pool, iaRec->attributeName);
    tmp.copy(name);
    len = tmp.size();
  }
  LocalAttributeRecord_list alist(c_attributeRecordPool,
					 tablePtr.p->m_attributes);
  for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr)){
    ConstRope tmp(c_rope_pool, attrPtr.p->attributeName);
    if(tmp.compare(name, len) == 0){
      id[0] = attrPtr.p->attributeId;
      return;
    }
  }
  ndbrequire(false);
}

void
Dbdict::getIndexAttrList(TableRecordPtr indexPtr, IndexAttributeList& list)
{
  jam();
  list.sz = 0;
  memset(list.id, 0, sizeof(list.id));
  ndbrequire(indexPtr.p->noOfAttributes >= 2);

  LocalAttributeRecord_list alist(c_attributeRecordPool,
                                         indexPtr.p->m_attributes);
  AttributeRecordPtr attrPtr;
  for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr)) {
    // skip last
    AttributeRecordPtr tempPtr = attrPtr;
    if (! alist.next(tempPtr))
      break;
    /**
     * Post-increment moved out of original expression &list.id[list.sz++]
     * due to Intel compiler bug on ia64 (BUG#34208).
     */
    getIndexAttr(indexPtr, attrPtr.i, &list.id[list.sz]);
    list.sz++;
  }
  ndbrequire(indexPtr.p->noOfAttributes == list.sz + 1);
}

void
Dbdict::getIndexAttrMask(TableRecordPtr indexPtr, AttributeMask& mask)
{
  jam();
  mask.clear();
  ndbrequire(indexPtr.p->noOfAttributes >= 2);

  AttributeRecordPtr attrPtr, currPtr;
  LocalAttributeRecord_list alist(c_attributeRecordPool,
					 indexPtr.p->m_attributes);


  for (alist.first(attrPtr); currPtr = attrPtr, alist.next(attrPtr); ){
    Uint32 id;
    getIndexAttr(indexPtr, currPtr.i, &id);
    mask.set(id);
  }
}

// DICT lock master

const Dbdict::DictLockType*
Dbdict::getDictLockType(Uint32 lockType)
{
  static const DictLockType lt[] = {
    { DictLockReq::NodeRestartLock, "NodeRestart" }
    ,{ DictLockReq::SchemaTransLock, "SchemaTransLock" }
    ,{ DictLockReq::CreateFileLock,  "CreateFile" }
    ,{ DictLockReq::CreateFilegroupLock, "CreateFilegroup" }
    ,{ DictLockReq::DropFileLock,    "DropFile" }
    ,{ DictLockReq::DropFilegroupLock, "DropFilegroup" }
  };
  for (unsigned int i = 0; i < sizeof(lt)/sizeof(lt[0]); i++) {
    if ((Uint32) lt[i].lockType == lockType)
      return &lt[i];
  }
  return NULL;
}

void
Dbdict::debugLockInfo(Signal* signal, 
                      const char* text,
                      Uint32 rc)
{
  if (!g_trace)
    return;
  
  static const char* rctext = "Unknown result";
  
  switch(rc)
  {
  case UtilLockRef::OK:
    rctext = "Success";
    break;
  case UtilLockRef::NoSuchLock:
    rctext = "No such lock";
    break;
  case UtilLockRef::OutOfLockRecords:
    rctext = "Out of records";
    break;
  case UtilLockRef::DistributedLockNotSupported:
    rctext = "Distributed lock not supported";
    break;
  case UtilLockRef::LockAlreadyHeld:
    rctext = "Already held";
    break;
  case UtilLockRef::InLockQueue:
    rctext = "Queued";
    break;
    /* try returns these... */
  case SchemaTransBeginRef::Busy:
    rctext = "SchemaTransBeginRef::Busy";
    break;
  case SchemaTransBeginRef::BusyWithNR:
    rctext = "SchemaTransBeginRef::BusyWithNR";
    break;
  default:
    break;
  }
  
  infoEvent("DICT : %s %u %s",
            text,
            rc,
            rctext);
}

void
Dbdict::sendDictLockInfoEvent(Signal*, const UtilLockReq* req, const char* text)
{
  const Dbdict::DictLockType* lt = getDictLockType(req->extra);

  infoEvent("DICT: %s %u for %s",
            text,
            (unsigned)refToNode(req->senderRef), lt->text);
}

void
Dbdict::execDICT_LOCK_REQ(Signal* signal)
{
  jamEntry();
  const DictLockReq req = *(DictLockReq*)&signal->theData[0];

  UtilLockReq lockReq;
  lockReq.senderRef = req.userRef;
  lockReq.senderData = req.userPtr;
  lockReq.lockId = 0;
  lockReq.requestInfo = 0;
  lockReq.extra = req.lockType;

  const DictLockType* lt = getDictLockType(req.lockType);

  Uint32 err;
  if (req.lockType == DictLockReq::SumaStartMe ||
      req.lockType == DictLockReq::SumaHandOver)
  {
    jam();

    if (c_outstanding_sub_startstop)
    {
      jam();
      g_eventLogger->info("refing dict lock to %u", refToNode(req.userRef));
      err = DictLockRef::TooManyRequests;
      goto ref;
    }

    if (req.lockType == DictLockReq::SumaHandOver &&
        !c_sub_startstop_lock.isclear())
    {
      g_eventLogger->info("refing dict lock to %u", refToNode(req.userRef));
      err = DictLockRef::TooManyRequests;
      goto ref;
    }

    c_sub_startstop_lock.set(refToNode(req.userRef));

    g_eventLogger->info("granting SumaStartMe dict lock to %u", refToNode(req.userRef));
    DictLockConf* conf = (DictLockConf*)signal->getDataPtrSend();
    conf->userPtr = req.userPtr;
    conf->lockType = req.lockType;
    conf->lockPtr = 0;
    sendSignal(req.userRef, GSN_DICT_LOCK_CONF, signal,
               DictLockConf::SignalLength, JBB);
    return;
  }

  if (req.lockType == DictLockReq::NodeRestartLock)
  {
    jam();
    lockReq.requestInfo |= UtilLockReq::SharedLock;
  }

  // make sure bad request crashes slave, not master (us)
  Uint32 res;
  if (getOwnNodeId() != c_masterNodeId)
  {
    jam();
    err = DictLockRef::NotMaster;
    goto ref;
  }

  if (lt == NULL)
  {
    jam();
    err = DictLockRef::InvalidLockType;
    goto ref;
  }

  if (req.userRef != signal->getSendersBlockRef() ||
      getNodeInfo(refToNode(req.userRef)).m_type != NodeInfo::DB)
  {
    jam();
    err = DictLockRef::BadUserRef;
    goto ref;
  }

  if (c_aliveNodes.get(refToNode(req.userRef)))
  {
    jam();
    err = DictLockRef::TooLate;
    goto ref;
  }

  res = m_dict_lock.lock(this, m_dict_lock_pool, &lockReq, 0);
  debugLockInfo(signal,
                "DICT_LOCK_REQ lock",
                res);
  switch(res){
  case 0:
    jam();
    sendDictLockInfoEvent(signal, &lockReq, "locked by node");
    goto conf;
    break;
  case UtilLockRef::OutOfLockRecords:
    jam();
    err = DictLockRef::TooManyRequests;
    goto ref;
    break;
  default:
    jam();
    sendDictLockInfoEvent(signal, &lockReq, "lock request by node queued");
    m_dict_lock.dump_queue(m_dict_lock_pool, this);
    break;
  }
  return;

ref:
  {
    DictLockRef* ref = (DictLockRef*)signal->getDataPtrSend();
    ref->userPtr = lockReq.senderData;
    ref->lockType = lockReq.extra;
    ref->errorCode = err;

    sendSignal(lockReq.senderRef, GSN_DICT_LOCK_REF, signal,
               DictLockRef::SignalLength, JBB);
  }
  return;

conf:
  {
    DictLockConf* conf = (DictLockConf*)signal->getDataPtrSend();

    conf->userPtr = lockReq.senderData;
    conf->lockType = lockReq.extra;
    conf->lockPtr = lockReq.senderData;

    sendSignal(lockReq.senderRef, GSN_DICT_LOCK_CONF, signal,
               DictLockConf::SignalLength, JBB);
  }
  return;
}

void
Dbdict::execDICT_UNLOCK_ORD(Signal* signal)
{
  jamEntry();
  const DictUnlockOrd* ord = (const DictUnlockOrd*)&signal->theData[0];

  DictLockReq req;
  req.userPtr = ord->senderData;
  req.userRef = ord->senderRef;

  if (signal->getLength() < DictUnlockOrd::SignalLength)
  {
    jam();
    req.userPtr = ord->lockPtr;
    req.userRef = signal->getSendersBlockRef();
  }

  if (ord->lockType == DictLockReq::SumaStartMe ||
      ord->lockType == DictLockReq::SumaHandOver)
  {
    jam();
    g_eventLogger->info("clearing SumaStartMe dict lock for %u", refToNode(ord->senderRef));
    c_sub_startstop_lock.clear(refToNode(ord->senderRef));
    return;
  }

  UtilLockReq lockReq;
  lockReq.senderData = req.userPtr;
  lockReq.senderRef = req.userRef;
  DictLockReq::LockType lockType = DictLockReq::NodeRestartLock;
  Uint32 res = dict_lock_unlock(signal, &req, &lockType);
  debugLockInfo(signal,
                "DICT_UNLOCK_ORD unlock",
                res);
  lockReq.extra = lockType;
  switch(res){
  case UtilUnlockRef::OK:
    jam();
    sendDictLockInfoEvent(signal, &lockReq, "unlocked by node");
    return;
  case UtilUnlockRef::NotLockOwner:
    jam();
    sendDictLockInfoEvent(signal, &lockReq, "lock request removed by node");
    return;
  default:
    ndbassert(false);
  }
}

// Master take-over

void
Dbdict::execDICT_TAKEOVER_REQ(Signal* signal)
 {
   jamEntry();

   if (!checkNodeFailSequence(signal))
   {
     jam();
     return;
   }

   DictTakeoverReq* req = (DictTakeoverReq*)signal->getDataPtr();
   Uint32 masterRef = req->senderRef;
   Uint32 op_count = 0;
   Uint32 rollforward_op = 0;
   Uint32 rollforward_op_state = SchemaOp::OS_COMPLETED;
   Uint32 rollback_op = 0;
   Uint32 rollback_op_state = SchemaOp::OS_INITIAL;
   Uint32 lowest_op = 0;
   Uint32 lowest_op_state = SchemaOp::OS_COMPLETED;
   Uint32 lowest_op_impl_req_gsn = 0;
   Uint32 highest_op = 0;
   Uint32 highest_op_state = SchemaOp::OS_INITIAL;
   Uint32 highest_op_impl_req_gsn = 0;
   SchemaTransPtr trans_ptr;
   bool ending = false;

   jam();
   bool pending_trans = c_schemaTransList.first(trans_ptr);
   if (!pending_trans)
   {
     /*
       Slave has no pending transaction
     */
     jam();
     DictTakeoverRef* ref = (DictTakeoverRef*)signal->getDataPtrSend();
     ref->senderRef = reference();
     ref->masterRef = masterRef;
     ref->errorCode = DictTakeoverRef::NoTransaction;
     sendSignal(masterRef, GSN_DICT_TAKEOVER_REF, signal,
                DictTakeoverRef::SignalLength, JBB);
     return;
   }
   while (pending_trans)
   {
     trans_ptr.p->m_masterRef = masterRef;
#ifdef VM_TRACE
      ndbout_c("Dbdict::execDICT_TAKEOVER_REQ: trans %u(0x%8x), state %u, op_list %s", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state, (trans_ptr.p->m_op_list.in_use)?"yes":"no");
#endif

     SchemaOpPtr op_ptr;
     LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
     bool pending_op = list.first(op_ptr);
     if (pending_op &&
         (trans_ptr.p->m_state == SchemaTrans::TS_COMPLETING ||
          trans_ptr.p->m_state == SchemaTrans::TS_ENDING))
     {
       jam();
       /*
         We were ending (releasing) operations, check how
         far slave got by finding lowest operation.
       */
       ending = true;
       lowest_op = op_ptr.p->op_key;
       lowest_op_state = op_ptr.p->m_state;
       /*
         Find the OpInfo gsn for the next operation to
         be removed,
         this might be needed by new master to create missing operation.
       */
       lowest_op_impl_req_gsn = getOpInfo(op_ptr).m_impl_req_gsn;
     }
     while (pending_op)
     {
       jam();
       op_count++;
       ndbrequire(!op_ptr.isNull());
       ndbrequire(trans_ptr.i == op_ptr.p->m_trans_ptr.i);
#ifdef VM_TRACE
       ndbout_c("Dbdict::execDICT_TAKEOVER_REQ: op %u state %u", op_ptr.p->op_key, op_ptr.p->m_state);
#endif

       /*
         Check if operation is busy
       */
       switch(op_ptr.p->m_state) {
       case SchemaOp::OS_PARSING:
       case SchemaOp::OS_PREPARING:
       case SchemaOp::OS_ABORTING_PREPARE:
       case SchemaOp::OS_ABORTING_PARSE:
       case SchemaOp::OS_COMMITTING:
       case SchemaOp::OS_COMPLETING:
       {
         /**
          * Wait 100ms and check again. This delay is there to save CPU cycles
          * and to avoid filling the jam trace buffer.
          */
         jam();
         Uint32* data = &signal->theData[0];
         memmove(&data[1], &data[0], DictTakeoverReq::SignalLength << 2);
         data[0] = ZDICT_TAKEOVER_REQ;
         sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                             100, 1 + DictTakeoverReq::SignalLength);
         return;
       }
       default:
         break;
       }
       if (ending)
       {
         pending_op = list.next(op_ptr);
         continue;
       }
       if (trans_ptr.p->m_state == SchemaTrans::TS_STARTED || // master
           trans_ptr.p->m_state == SchemaTrans::TS_PARSING)
       {
         jam();
         /*
           We were parsing operations from client, check how
           far slave got by finding highest operation.
         */
         highest_op = op_ptr.p->op_key;
         highest_op_state = op_ptr.p->m_state;
         /*
           Find the OpInfo gsn for the latest created operation,
           this might be needed by new master to create missing operation.
         */
         highest_op_impl_req_gsn = getOpInfo(op_ptr).m_impl_req_gsn;
       }
       else
       {
         jam();
#ifdef VM_TRACE
         ndbout_c("Op %u, state %u, rollforward %u/%u, rollback %u/%u",op_ptr.p->op_key,op_ptr.p->m_state, rollforward_op,  rollforward_op_state, rollback_op,  rollback_op_state);
#endif
         /*
           Find the starting point for a roll forward, the first
           operation found with a lower state than the previous.
         */
         if (SchemaOp::weight(op_ptr.p->m_state) <
             SchemaOp::weight(rollforward_op_state))
         {
           rollforward_op = op_ptr.p->op_key;
           rollforward_op_state = op_ptr.p->m_state;
         }
         /*
           Find the starting point for a rollback, the last
           operation found that changed state.
         */
         if (SchemaOp::weight(op_ptr.p->m_state) >=
             SchemaOp::weight(rollback_op_state))
         {
           rollback_op = op_ptr.p->op_key;
           rollback_op_state = op_ptr.p->m_state;
         }
       }
       pending_op = list.next(op_ptr);
     }
#ifdef VM_TRACE
     ndbout_c("Slave transaction %u has %u schema operations, rf %u/%u, rb %u/%u", trans_ptr.p->trans_key, op_count, rollforward_op, rollforward_op_state, rollback_op, rollback_op_state);
#endif
     DictTakeoverConf* conf = (DictTakeoverConf*)signal->getDataPtrSend();
     conf->senderRef = reference();
     conf->clientRef = trans_ptr.p->m_clientRef;
     conf->trans_key = trans_ptr.p->trans_key;
     conf->trans_state = trans_ptr.p->m_state;
     conf->op_count = op_count;
     conf->rollforward_op = rollforward_op;
     conf->rollforward_op_state = rollforward_op_state;
     conf->rollback_op = rollback_op;
     conf->rollback_op_state = rollback_op_state;
     if (trans_ptr.p->m_state == SchemaTrans::TS_STARTED ||
         trans_ptr.p->m_state == SchemaTrans::TS_PARSING)
     {
       /*
         New master might not have parsed highest found operation yet.
        */
       conf->highest_op = highest_op;
       conf->highest_op_state = highest_op_state;
       conf->highest_op_impl_req_gsn = highest_op_impl_req_gsn;
     }
     if (ending)
     {
       /*
         New master might already have released lowest operation found.
        */
       conf->lowest_op = lowest_op;
       conf->lowest_op_state = lowest_op_state;
       conf->lowest_op_impl_req_gsn = lowest_op_impl_req_gsn;
     }
     sendSignal(masterRef, GSN_DICT_TAKEOVER_CONF, signal,
                DictTakeoverConf::SignalLength, JBB);
     ndbrequire(!(pending_trans = c_schemaTransList.next(trans_ptr)));
   }
 }

void
Dbdict::execDICT_TAKEOVER_REF(Signal* signal)
{
  DictTakeoverRef* ref = (DictTakeoverRef*)signal->getDataPtr();
  Uint32 senderRef = ref->senderRef;
  Uint32 nodeId = refToNode(senderRef);
  Uint32 masterRef = ref->masterRef;
  NodeRecordPtr masterNodePtr;
  jamEntry();
#ifdef VM_TRACE
  ndbout_c("Dbdict::execDICT_TAKEOVER_REF: error %u, from %u", ref->errorCode, nodeId);
#endif
  /*
    Slave has died (didn't reply) or doesn't not have any transaction
    Ignore it during rest of master takeover.
  */
  ndbassert(refToNode(masterRef) == c_masterNodeId);
  c_nodes.getPtr(masterNodePtr, c_masterNodeId);
  masterNodePtr.p->m_nodes.clear(nodeId);
  /*
    Check if we got replies from all nodes
  */
  {
    SafeCounter sc(c_counterMgr, masterNodePtr.p->m_counter);
    if (!sc.clearWaitingFor(nodeId)) {
      jam();
      return;
    }
  }
  c_takeOverInProgress = false;
  check_takeover_replies(signal);
}

void
Dbdict::execDICT_TAKEOVER_CONF(Signal* signal)
{
  jamEntry();
  DictTakeoverConf* conf = (DictTakeoverConf*)signal->getDataPtr();
  jamEntry();
  Uint32 senderRef = conf->senderRef;
  Uint32 nodeId = refToNode(senderRef);
  //Uint32 clientRef = conf->clientRef;
  //Uint32 op_count = conf->op_count;
  //Uint32 trans_key = conf->trans_key;
  //Uint32 rollforward_op = conf->rollforward_op;
  //Uint32 rollforward_op_state = conf->rollforward_op_state;
  //Uint32 rollback_op = conf->rollback_op;
  //Uint32 rollback_op_state = conf->rollback_op_state;
  NodeRecordPtr masterNodePtr;

  /*
    Accumulate all responses
  */
  NodeRecordPtr nodePtr;
  c_nodes.getPtr(nodePtr, nodeId);
  nodePtr.p->takeOverConf = *conf;

  ndbassert(getOwnNodeId() == c_masterNodeId);
  c_nodes.getPtr(masterNodePtr, c_masterNodeId);
#ifdef VM_TRACE
  ndbout_c("execDICT_TAKEOVER_CONF: Node %u, trans %u(%u), count %u, rollf %u/%u, rb %u/%u",
           nodeId, conf->trans_key, conf->trans_state, conf->op_count, conf->rollforward_op,
           conf->rollforward_op_state, conf->rollback_op, conf->rollback_op_state);
#endif

  /*
    Check that we got reply from all nodes
  */
  {
    SafeCounter sc(c_counterMgr, masterNodePtr.p->m_counter);
    if (!sc.clearWaitingFor(nodeId)) {
      jam();
      return;
    }
  }
  c_takeOverInProgress = false;
  check_takeover_replies(signal);
}

void Dbdict::check_takeover_replies(Signal* signal)
{
  SchemaTransPtr trans_ptr;
  NodeRecordPtr masterNodePtr;
  ErrorInfo error;

  c_nodes.getPtr(masterNodePtr, c_masterNodeId); // this node
  if (masterNodePtr.p->m_nodes.isclear())
  {
    /*
      No slave found any pending transactions, we are done
     */
    jam();
    send_nf_complete_rep(signal, &masterNodePtr.p->nodeFailRep);
    return;
  }
  /*
    Take schema trans lock.
    Set initial values for rollforward/rollback points
    and highest/lowest transaction states
   */
  bool pending_trans = c_schemaTransList.first(trans_ptr);
  while (pending_trans) {
    jam();
    Uint32 trans_key = trans_ptr.p->trans_key;
    DictLockReq& lockReq = trans_ptr.p->m_lockReq;
    lockReq.userPtr = trans_key;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::SchemaTransLock;
    int lockError = dict_lock_trylock(&lockReq);
    debugLockInfo(signal,
                  "check_takeover_replies trylock 1",
                  lockError);
    if (lockError != 0)
    {
      jam();
#ifdef VM_TRACE
      ndbout_c("New master failed locking transaction %u, error %u", trans_key, lockError);
#endif
      ndbassert(false);
    }
    else
    {
      jam();
#ifdef VM_TRACE
      ndbout_c("New master locked transaction %u", trans_key);
#endif
    }
    trans_ptr.p->m_isMaster = true;
    trans_ptr.p->m_nodes.clear();
    trans_ptr.p->m_rollforward_op = -1;
    trans_ptr.p->m_rollforward_op_state = SchemaOp::OS_COMPLETED;
    trans_ptr.p->m_rollback_op = 0;
    trans_ptr.p->m_rollback_op_state = SchemaOp::OS_INITIAL;
    trans_ptr.p->m_lowest_trans_state = SchemaTrans::TS_ENDING;
    trans_ptr.p->m_highest_trans_state = SchemaTrans::TS_INITIAL;
    trans_ptr.p->check_partial_rollforward = false;
    trans_ptr.p->ressurected_op = false;
    pending_trans = c_schemaTransList.next(trans_ptr);
  }
  /*
    Find rollforward/rollback operations and highest/lowest transaction state
   */
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    NodeRecordPtr nodePtr;
    if (masterNodePtr.p->m_nodes.get(i))
    {
      jam();
      c_nodes.getPtr(nodePtr, i);
      {
	DictTakeoverConf* conf = &nodePtr.p->takeOverConf;
        Uint32 clientRef = conf->clientRef;
	Uint32 rollforward_op = conf->rollforward_op;
	Uint32 rollforward_op_state = conf->rollforward_op_state;
	Uint32 rollback_op = conf->highest_op;
	Uint32 rollback_op_state = conf->rollback_op_state;
        Uint32 trans_key = conf->trans_key;
        Uint32 trans_state = conf->trans_state;
        SchemaTransPtr trans_ptr;

        if (!findSchemaTrans(trans_ptr, trans_key))
        {
          jam();
          /*
            New master doesn't know about the transaction.
          */
          if (!seizeSchemaTrans(trans_ptr, trans_key))
          {
            jam();
            ndbassert(false);
          }
#ifdef VM_TRACE
          ndbout_c("New master seized transaction %u", trans_key);
#endif
          /*
            Take schema trans lock.
          */
          DictLockReq& lockReq = trans_ptr.p->m_lockReq;
          lockReq.userPtr = trans_ptr.p->trans_key;
          lockReq.userRef = reference();
          lockReq.lockType = DictLockReq::SchemaTransLock;
          int lockError = dict_lock_trylock(&lockReq);
          debugLockInfo(signal,
                        "check_takeover_replies trylock 2",
                        lockError);
          if (lockError != 0)
          {
            jam();
#ifdef VM_TRACE
            ndbout_c("New master failed locking transaction %u, error %u", trans_key, lockError);
#endif
            ndbassert(false);
          }
          else
          {
            jam();
#ifdef VM_TRACE
            ndbout_c("New master locked transaction %u", trans_key);
#endif
          }
          trans_ptr.p->m_rollforward_op = -1;
          trans_ptr.p->m_rollforward_op_state = SchemaOp::OS_COMPLETED;
          trans_ptr.p->m_rollback_op = 0;
          trans_ptr.p->m_rollback_op_state = SchemaOp::OS_INITIAL;
          trans_ptr.p->m_lowest_trans_state = SchemaTrans::TS_ENDING;
          trans_ptr.p->m_highest_trans_state = SchemaTrans::TS_INITIAL;
        }

        trans_ptr.p->m_isMaster = true;
        trans_ptr.p->m_masterRef = reference();
        trans_ptr.p->m_clientRef = clientRef;
        trans_ptr.p->m_nodes.set(i);
#ifdef VM_TRACE
        ndbout_c("Adding node %u to transaction %u", i, trans_ptr.p->trans_key);
#endif
        /*
          Save the operation with lowest state and lowest key
          for roll forward
         */
        if ((SchemaOp::weight(rollforward_op_state) <
             SchemaOp::weight(trans_ptr.p->m_rollforward_op_state)) ||
            ((rollforward_op_state ==
              trans_ptr.p->m_rollforward_op_state &&
              rollforward_op < trans_ptr.p->m_rollforward_op)))
	{
          jam();
	  trans_ptr.p->m_rollforward_op = rollforward_op;
	  trans_ptr.p->m_rollforward_op_state = rollforward_op_state;
	}

        /*
          Save operation with the highest state and the highest key
          for rollback
         */
        if ((SchemaOp::weight(rollback_op_state) >
             SchemaOp::weight(trans_ptr.p->m_rollback_op_state)) ||
            ((rollback_op_state ==
              trans_ptr.p->m_rollback_op_state &&
              rollback_op > trans_ptr.p->m_rollback_op)))
	{
          jam();
	  trans_ptr.p->m_rollback_op = rollback_op;
	  trans_ptr.p->m_rollback_op_state = rollback_op_state;
	}

        if (SchemaTrans::weight(trans_state) <
            SchemaTrans::weight(trans_ptr.p->m_lowest_trans_state))
        {
          jam();
          trans_ptr.p->m_lowest_trans_state = trans_state;
        }
        if (SchemaTrans::weight(trans_state) >
            SchemaTrans::weight(trans_ptr.p->m_highest_trans_state))
        {
          jam();
          trans_ptr.p->m_highest_trans_state = trans_state;
        }
      }
    }
  }

  /*
    Check the progress of transactions.
  */
  pending_trans = c_schemaTransList.first(trans_ptr);
  while (pending_trans)
  {
    jam();
#ifdef VM_TRACE
    ndbout_c("Analyzing transaction progress, trans %u/%u, lowest/highest %u/%u", trans_ptr.p->trans_key, trans_ptr.p->m_state, trans_ptr.p->m_lowest_trans_state, trans_ptr.p->m_highest_trans_state);
#endif
    switch(trans_ptr.p->m_highest_trans_state) {
    case SchemaTrans::TS_INITIAL:
    case SchemaTrans::TS_STARTING:
    case SchemaTrans::TS_STARTED:
    case SchemaTrans::TS_PARSING:
    case SchemaTrans::TS_SUBOP:
    case SchemaTrans::TS_ROLLBACK_SP:
    case SchemaTrans::TS_FLUSH_PREPARE:
    case SchemaTrans::TS_PREPARING:
    case SchemaTrans::TS_ABORTING_PREPARE:
    case SchemaTrans::TS_ABORTING_PARSE:
      jam();
      trans_ptr.p->m_master_recovery_state = SchemaTrans::TRS_ROLLBACK;
      break;
    case SchemaTrans::TS_FLUSH_COMMIT:
    case SchemaTrans::TS_COMMITTING:
    case SchemaTrans::TS_FLUSH_COMPLETE:
    case SchemaTrans::TS_COMPLETING:
    case SchemaTrans::TS_ENDING:
      jam();
      trans_ptr.p->m_master_recovery_state = SchemaTrans::TRS_ROLLFORWARD;
      break;
    }

    if (trans_ptr.p->m_master_recovery_state == SchemaTrans::TRS_ROLLFORWARD)
    {
      /*
        We must start rolling forward from lowest state of any slave
        and partially skip more progressed slaves.
       */
      jam();
      infoEvent("Pending schema transaction %u will be rolled forward", trans_ptr.p->trans_key);
      trans_ptr.p->check_partial_rollforward = true;
      trans_ptr.p->m_state = trans_ptr.p->m_lowest_trans_state;
#ifdef VM_TRACE
      ndbout_c("Setting transaction state to %u for rollforward", trans_ptr.p->m_state);
#endif
    }
    else
    {
      /*
        We must start rolling back from highest state of any slave
        and partially skip less progressed slaves.
       */
      jam();
      infoEvent("Pending schema transaction %u will be rolled back", trans_ptr.p->trans_key);
      trans_ptr.p->m_state = trans_ptr.p->m_highest_trans_state;
#ifdef VM_TRACE
      ndbout_c("Setting transaction state to %u for rollback", trans_ptr.p->m_state);
#endif
    }
#ifdef VM_TRACE
    ndbout_c("Setting start state for transaction %u to %u", trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    pending_trans = c_schemaTransList.next(trans_ptr);
  }

  /*
     Initialize all node recovery states
  */
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    NodeRecordPtr nodePtr;
    c_nodes.getPtr(nodePtr, i);
    nodePtr.p->recoveryState = NodeRecord::RS_NORMAL;
  }

  pending_trans = c_schemaTransList.first(trans_ptr);
  while (pending_trans)
  {
    /*
      Find nodes that need partial rollforward/rollback,
      create any missing operations on new master
    */

    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      jam();
      NodeRecordPtr nodePtr;
      if (trans_ptr.p->m_nodes.get(i))
      {
        jam();
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Node %u had %u operations, master has %u",i , nodePtr.p->takeOverConf.op_count, masterNodePtr.p->takeOverConf.op_count);
#endif
        if (nodePtr.p->takeOverConf.op_count == 0)
        {
          if (SchemaTrans::weight(trans_ptr.p->m_state)
              < SchemaTrans::weight(SchemaTrans::TS_PREPARING))
          {
            /*
              Node didn't parse any operations,
              remove, skip it when aborting parse.
            */
            jam();
#ifdef VM_TRACE
            ndbout_c("Node %u had no operations for  transaction %u, ignore it when aborting", i, trans_ptr.p->trans_key);
#endif
            nodePtr.p->start_op = 0;
            nodePtr.p->start_op_state = SchemaOp::OS_PARSED;
          }
          else
          {
            /*
              Node is ended
            */
            jam();
            // Is this possible??
          }
        }
        else if (nodePtr.p->takeOverConf.op_count <
                 masterNodePtr.p->takeOverConf.op_count)
        {
          jam();
          /*
              Operation is missing on slave
          */
          if (SchemaTrans::weight(trans_ptr.p->m_state) <
              SchemaTrans::weight(SchemaTrans::TS_PREPARING))
          {
            /*
              Last parsed operation is missing on slave, skip it
              when aborting parse.
            */
            jam();
#ifdef VM_TRACE
            ndbout_c("Node %u did not have all operations for transaction %u, skip > %u", i, trans_ptr.p->trans_key, nodePtr.p->takeOverConf.highest_op);
#endif
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLBACK;
            nodePtr.p->start_op = nodePtr.p->takeOverConf.highest_op;
            nodePtr.p->start_op_state = nodePtr.p->takeOverConf.highest_op_state;
          }
          else
          {
            /*
              Slave has already ended some operations
            */
            jam();
#ifdef VM_TRACE
            ndbout_c("Node %u did not have all operations for transaction %u, skip < %u", i, trans_ptr.p->trans_key, nodePtr.p->takeOverConf.lowest_op);
#endif
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLFORWARD;
            nodePtr.p->start_op = nodePtr.p->takeOverConf.lowest_op;
            nodePtr.p->start_op_state = nodePtr.p->takeOverConf.lowest_op_state;
          }
        }
        else if (nodePtr.p->takeOverConf.op_count >
                 masterNodePtr.p->takeOverConf.op_count)
        {
          /*
            Operation missing on new master
           */
          jam();
          if (SchemaTrans::weight(trans_ptr.p->m_state)
              < SchemaTrans::weight(SchemaTrans::TS_PREPARING))
          {
            /*
              Last parsed operation is missing on new master
             */
            jam();
            if (masterNodePtr.p->recoveryState !=
                NodeRecord::RS_PARTIAL_ROLLBACK)
            {
              /*
                We haven't decided to partially rollback master yet.
                Operation is missing on new master (not yet parsed).
                Create it so new master can tell slaves to abort it,
                but skip it on master.
              */
              jam();
              SchemaOpPtr missing_op_ptr;
              const OpInfo& info =
                *findOpInfo(nodePtr.p->takeOverConf.highest_op_impl_req_gsn);
              if (seizeSchemaOp(trans_ptr,
                                missing_op_ptr,
                                nodePtr.p->takeOverConf.highest_op,
                                info))
              {
                jam();
#ifdef VM_TRACE
                ndbout_c("Created missing operation %u, on new master", missing_op_ptr.p->op_key);
#endif
                missing_op_ptr.p->m_state = nodePtr.p->takeOverConf.highest_op_state;
                masterNodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLBACK;
                masterNodePtr.p->start_op = masterNodePtr.p->takeOverConf.highest_op;
                masterNodePtr.p->start_op_state = masterNodePtr.p->takeOverConf.highest_op_state;
              }
              else
              {
                jam();
                ndbassert(false);
              }
              trans_ptr.p->m_nodes.set(c_masterNodeId);
#ifdef VM_TRACE
              ndbout_c("Adding master node %u to transaction %u", c_masterNodeId, trans_ptr.p->trans_key);
#endif
            }
          }
          else if (SchemaTrans::weight(trans_ptr.p->m_state)
                   >= SchemaTrans::weight(SchemaTrans::TS_PREPARING) &&
                   (!trans_ptr.p->ressurected_op))
          {
            /*
              New master has already ended some operation,
              create it again so we can tell slaves to end it.
              Note: we don't add node to transaction since the
              ressurected operation cannot be completed. Instead
              we need to release it explicitly when transaction is
              ended.
            */
            jam();
            SchemaOpPtr missing_op_ptr;
            Uint32 op_key = nodePtr.p->takeOverConf.lowest_op;
            Uint32 op_state = nodePtr.p->takeOverConf.lowest_op_state;
            const OpInfo& info =
              *findOpInfo(nodePtr.p->takeOverConf.lowest_op_impl_req_gsn);
            if (seizeSchemaOp(trans_ptr,
                              missing_op_ptr,
                              op_key,
                              info))
            {
              jam();
#ifdef VM_TRACE
              ndbout_c("Created ressurected operation %u, on new master", op_key);
#endif
              trans_ptr.p->ressurected_op = true;
              missing_op_ptr.p->m_state = op_state;
              nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLFORWARD;
              nodePtr.p->start_op = op_key;
              nodePtr.p->start_op_state = op_state;

            }
            else
            {
              jam();
              assert(false);
            }
            continue;
          }
        }
      }
    }

    /*
      Compare node progress
    */
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      jam();
      NodeRecordPtr nodePtr;
      if (trans_ptr.p->m_nodes.get(i))
      {
        jam();
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Comparing node %u rollforward(%u(%u)<%u(%u))/rollback(%u(%u)<%u(%u))", i, nodePtr.p->takeOverConf.rollforward_op_state, nodePtr.p->takeOverConf.rollforward_op, trans_ptr.p->m_rollforward_op_state, trans_ptr.p->m_rollforward_op, nodePtr.p->takeOverConf.rollback_op_state, nodePtr.p->takeOverConf.rollback_op, trans_ptr.p->m_rollback_op_state, trans_ptr.p->m_rollback_op);
#endif
        if (trans_ptr.p->m_master_recovery_state == SchemaTrans::TRS_ROLLFORWARD)
        {
          jam();
          if (trans_ptr.p->m_lowest_trans_state == SchemaTrans::TS_PREPARING &&
              nodePtr.p->takeOverConf.trans_state == SchemaTrans::TS_COMMITTING)
          {
            /*
              Some slave have flushed the commit start, but not all.
              Flushed slaves need to be partially rolled forward.
             */
            jam();
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLFORWARD;
#ifdef VM_TRACE
            ndbout_c("Node %u will be partially rolled forward, skipping RT_FLUSH_COMMIT", nodePtr.i);
#endif
          }
          else if (SchemaOp::weight(nodePtr.p->takeOverConf.rollforward_op_state) >
                   SchemaOp::weight(trans_ptr.p->m_rollforward_op_state) ||
                   nodePtr.p->takeOverConf.rollforward_op >
                   trans_ptr.p->m_rollforward_op)
          {
            /*
              Slave has started committing, but other slaves have non-committed
              operations. Node needs to be partially rollforward.
            */
            jam();
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLFORWARD;
            nodePtr.p->start_op = nodePtr.p->takeOverConf.rollforward_op;
            nodePtr.p->start_op_state = nodePtr.p->takeOverConf.rollforward_op_state;
#ifdef VM_TRACE
            ndbout_c("Node %u will be partially rolled forward to operation %u, state %u", nodePtr.i, nodePtr.p->start_op, nodePtr.p->start_op_state);
#endif
            if (i == c_masterNodeId)
            {
              /*
                New master is ahead of other slaves
                Change operation state back to rollforward
                other slaves.
               */
              jam();
              SchemaOpPtr op_ptr;
              ndbrequire(findSchemaOp(op_ptr,
                                      trans_ptr.p->m_rollforward_op));
#ifdef VM_TRACE
              ndbout_c("Changed op %u from state %u to %u", trans_ptr.p->m_rollforward_op, op_ptr.p->m_state, trans_ptr.p->m_rollforward_op_state);
#endif
              op_ptr.p->m_state = trans_ptr.p->m_rollforward_op_state;
            }
          }
          else if (trans_ptr.p->m_lowest_trans_state == SchemaTrans::TS_COMMITTING &&
                   nodePtr.p->takeOverConf.trans_state >= SchemaTrans::TS_FLUSH_COMPLETE)
          {
            /*
              Some slave have flushed the commit complete, but not all.
              Flushed slaves need to be partially rolled forward.
            */
            jam();
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLFORWARD;
#ifdef VM_TRACE
            ndbout_c("Node %u will be partially rolled forward, skipping RT_FLUSH_COMPLETE", nodePtr.i);
#endif
          }
        }
        else // if (trans_ptr.p->m_master_recovery_state == SchemaTrans::TRS_ROLLBACK)
        {
          jam();
          if (SchemaOp::weight(nodePtr.p->takeOverConf.rollback_op_state) <
              SchemaOp::weight(trans_ptr.p->m_rollback_op_state) ||
              nodePtr.p->takeOverConf.rollback_op <
              trans_ptr.p->m_rollback_op)
          {
            /*
              Slave is behind. Other nodes have further
              progress, or has already started aborting.
              Node needs to be partially rolled back.
            */
            jam();
            nodePtr.p->recoveryState = NodeRecord::RS_PARTIAL_ROLLBACK;
            nodePtr.p->start_op = nodePtr.p->takeOverConf.rollback_op;
            nodePtr.p->start_op_state = nodePtr.p->takeOverConf.rollback_op_state;
#ifdef VM_TRACE
            ndbout_c("Node %u will be partially rolled back from operation %u, state %u", nodePtr.i, nodePtr.p->start_op, nodePtr.p->start_op_state);
#endif
            if (i == c_masterNodeId &&
                (SchemaTrans::weight(trans_ptr.p->m_state) <=
                 SchemaTrans::weight(SchemaTrans::TS_PREPARING)))
            {
              /*
                New master is behind of other slaves
                Change operation state forward to rollback
                other slaves.
               */
              jam();
              SchemaOpPtr op_ptr;
              ndbrequire(findSchemaOp(op_ptr,
                                      trans_ptr.p->m_rollback_op));
#ifdef VM_TRACE
              ndbout_c("Changed op %u from state %u to %u", trans_ptr.p->m_rollback_op, op_ptr.p->m_state, trans_ptr.p->m_rollback_op_state);
#endif
              op_ptr.p->m_state = trans_ptr.p->m_rollback_op_state;
            }
          }
        }
      }
    }
    /*
      Set current op to the lowest/highest reported by slaves
      depending on if decision is to rollforward/rollback.
    */
    if (trans_ptr.p->m_master_recovery_state == SchemaTrans::TRS_ROLLFORWARD)
    {
      jam();
      SchemaOpPtr rollforward_op_ptr;
      ndbrequire(findSchemaOp(rollforward_op_ptr, trans_ptr.p->m_rollforward_op));
      trans_ptr.p->m_curr_op_ptr_i = rollforward_op_ptr.i;
#ifdef VM_TRACE
      ndbout_c("execDICT_TAKEOVER_CONF: Transaction %u rolled forward starting at %u(%u)", trans_ptr.p->trans_key,  trans_ptr.p->m_rollforward_op, trans_ptr.p->m_curr_op_ptr_i);
#endif
    }
    else // if (trans_ptr.p->master_recovery_state == SchemaTrans::TRS_ROLLBACK)
    {
      jam();
      if (trans_ptr.p->m_state >= SchemaTrans::TS_PARSING)
      {
        /*
          Some slave had at least started parsing operations
         */
        jam();
        SchemaOpPtr rollback_op_ptr;
        ndbrequire(findSchemaOp(rollback_op_ptr, trans_ptr.p->m_rollback_op));
        trans_ptr.p->m_curr_op_ptr_i = rollback_op_ptr.i;
#ifdef VM_TRACE
        ndbout_c("execDICT_TAKEOVER_CONF: Transaction %u rolled back starting at %u(%u)", trans_ptr.p->trans_key,  trans_ptr.p->m_rollback_op, trans_ptr.p->m_curr_op_ptr_i);
#endif
      }
    }

    trans_recover(signal, trans_ptr);
    pending_trans = c_schemaTransList.next(trans_ptr);
  }
}


// NF handling

void
Dbdict::removeStaleDictLocks(Signal* signal, const Uint32* theFailedNodes)
{
  LockQueue::Iterator iter;
  if (m_dict_lock.first(this, m_dict_lock_pool, iter))
  {
#ifdef MARTIN
    infoEvent("Iterating lock queue");
#endif
    do {
      if (NodeBitmask::get(theFailedNodes,
                           refToNode(iter.m_curr.p->m_req.senderRef)))
      {
        if (iter.m_curr.p->m_req.requestInfo & UtilLockReq::Granted)
        {
          jam();
          infoEvent("Removed lock for node %u", refToNode(iter.m_curr.p->m_req.senderRef));
          sendDictLockInfoEvent(signal, &iter.m_curr.p->m_req,
                                "remove lock by failed node");
        }
        else
        {
          jam();
          infoEvent("Removed lock request for node %u", refToNode(iter.m_curr.p->m_req.senderRef));
          sendDictLockInfoEvent(signal, &iter.m_curr.p->m_req,
                                "remove lock request by failed node");
        }
        DictUnlockOrd* ord = (DictUnlockOrd*)signal->getDataPtrSend();
        ord->senderRef = iter.m_curr.p->m_req.senderRef;
        ord->senderData = iter.m_curr.p->m_req.senderData;
        ord->lockPtr = iter.m_curr.p->m_req.senderData;
        ord->lockType = iter.m_curr.p->m_req.extra;
        sendSignal(reference(), GSN_DICT_UNLOCK_ORD, signal,
                   DictUnlockOrd::SignalLength, JBB);
      }
    } while (m_dict_lock.next(iter));
  }
}

Uint32
Dbdict::dict_lock_trylock(const DictLockReq* _req)
{
  UtilLockReq req;
  const UtilLockReq *lockOwner;
  req.senderData = _req->userPtr;
  req.senderRef = _req->userRef;
  req.extra = _req->lockType;
  req.requestInfo = UtilLockReq::TryLock | UtilLockReq::Notify;

  Uint32 res = m_dict_lock.lock(this, m_dict_lock_pool, &req, &lockOwner);
  switch(res){
  case UtilLockRef::OK:
    jam();
    return 0;
  case UtilLockRef::LockAlreadyHeld:
    jam();
    if (lockOwner->extra == DictLockReq::NodeRestartLock)
    {
      jam();
      return SchemaTransBeginRef::BusyWithNR;
    }
    break;
  case UtilLockRef::OutOfLockRecords:
    jam();
    break;
  case UtilLockRef::InLockQueue:
    jam();
    /**
     * Should not happen with trylock
     */
    ndbassert(false);
    break;
  }
#ifdef MARTIN
  infoEvent("Busy with schema transaction");
#endif
  if (g_trace)
    m_dict_lock.dump_queue(m_dict_lock_pool, this);
  
  return SchemaTransBeginRef::Busy;
}

Uint32
Dbdict::dict_lock_unlock(Signal* signal, const DictLockReq* _req,
                         DictLockReq::LockType* type)
{
  UtilUnlockReq req;
  req.senderData = _req->userPtr;
  req.senderRef = _req->userRef;

  UtilLockReq lockReq;
  Uint32 res = m_dict_lock.unlock(this, m_dict_lock_pool, &req, 
                                  &lockReq);
  switch(res){
  case UtilUnlockRef::OK:
    if (type)
    {
      *type = (DictLockReq::LockType) lockReq.extra;
    }
    /* Fall through */
  case UtilUnlockRef::NotLockOwner:
    break;
  case UtilUnlockRef::NotInLockQueue:
    ndbassert(false);
    return res;
  }

  LockQueue::Iterator iter;
  if (m_dict_lock.first(this, m_dict_lock_pool, iter))
  {
    int res;
    while ((res = m_dict_lock.checkLockGrant(iter, &lockReq)) > 0)
    {
      jam();
      /**
       *
       */
      if (res == 2)
      {
        jam();
        DictLockConf* conf = (DictLockConf*)signal->getDataPtrSend();
        conf->userPtr = lockReq.senderData;
        conf->lockPtr = lockReq.senderData;
        conf->lockType = lockReq.extra;
        sendSignal(lockReq.senderRef, GSN_DICT_LOCK_CONF, signal,
                   DictLockConf::SignalLength, JBB);

        sendDictLockInfoEvent(signal, &lockReq, 
                              "queued lock request granted for node");
      }

      if (!m_dict_lock.next(iter))
        break;
    }
  }

  return res;
}

void
Dbdict::execBACKUP_LOCK_TAB_REQ(Signal* signal)
{
  jamEntry();
  BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
  Uint32 senderRef = req->m_senderRef;
  Uint32 tableId = req->m_tableId;
  Uint32 lock = req->m_lock_unlock;

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, tableId);
  Uint32 err = 0;
  if (!ok)
  {
    jam();
    err = GetTabInfoRef::InvalidTableId;
  }
  else if(lock == BackupLockTab::LOCK_TABLE)
  {
    jam();
    if ((err = check_write_obj(tableId)) == 0)
    {
      jam();
      tablePtr.p->m_read_locked = 1;
    }
  }
  else
  {
    jam();
    tablePtr.p->m_read_locked = 0;
  }

  req->errorCode = err;
  sendSignal(senderRef, GSN_BACKUP_LOCK_TAB_CONF, signal,
             BackupLockTab::SignalLength, JBB);
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          STORE/RESTORE SCHEMA FILE---------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* General module used to store the schema file on disk and         */
/* similar function to restore it from disk.                        */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

void
Dbdict::initSchemaFile(XSchemaFile * xsf, Uint32 firstPage, Uint32 lastPage,
                       bool initEntries)
{
  ndbrequire(lastPage <= xsf->noOfPages);
  for (Uint32 n = firstPage; n < lastPage; n++) {
    SchemaFile * sf = &xsf->schemaPage[n];
    if (initEntries)
      memset(sf, 0, NDB_SF_PAGE_SIZE);

    Uint32 ndb_version = NDB_VERSION;
    if (ndb_version < NDB_SF_VERSION_5_0_6)
      ndb_version = NDB_SF_VERSION_5_0_6;

    memcpy(sf->Magic, NDB_SF_MAGIC, sizeof(sf->Magic));
    sf->ByteOrder = 0x12345678;
    sf->NdbVersion =  ndb_version;
    sf->FileSize = xsf->noOfPages * NDB_SF_PAGE_SIZE;
    sf->PageNumber = n;
    sf->CheckSum = 0;
    sf->NoOfTableEntries = NDB_SF_PAGE_ENTRIES;

    computeChecksum(xsf, n);
  }
}

void
Dbdict::resizeSchemaFile(XSchemaFile * xsf, Uint32 noOfPages)
{
  ndbrequire(noOfPages <= NDB_SF_MAX_PAGES);
  if (xsf->noOfPages < noOfPages) {
    jam();
    Uint32 firstPage = xsf->noOfPages;
    xsf->noOfPages = noOfPages;
    initSchemaFile(xsf, 0, firstPage, false);
    initSchemaFile(xsf, firstPage, xsf->noOfPages, true);
  }
  if (xsf->noOfPages > noOfPages) {
    jam();
    Uint32 tableId = noOfPages * NDB_SF_PAGE_ENTRIES;
    while (tableId < xsf->noOfPages * NDB_SF_PAGE_ENTRIES) {
      SchemaFile::TableEntry * te = getTableEntry(xsf, tableId);
      if (te->m_tableState != SchemaFile::SF_UNUSED)
      {
        ndbrequire(false);
      }
      tableId++;
    }
    xsf->noOfPages = noOfPages;
    initSchemaFile(xsf, 0, xsf->noOfPages, false);
  }
}

void
Dbdict::computeChecksum(XSchemaFile * xsf, Uint32 pageNo){
  SchemaFile * sf = &xsf->schemaPage[pageNo];
  sf->CheckSum = 0;
  sf->CheckSum = computeChecksum((Uint32*)sf, NDB_SF_PAGE_SIZE_IN_WORDS);
}

bool
Dbdict::validateChecksum(const XSchemaFile * xsf){

  for (Uint32 n = 0; n < xsf->noOfPages; n++) {
    SchemaFile * sf = &xsf->schemaPage[n];
    Uint32 c = computeChecksum((Uint32*)sf, NDB_SF_PAGE_SIZE_IN_WORDS);
    if ( c != 0)
      return false;
  }
  return true;
}

Uint32
Dbdict::computeChecksum(const Uint32 * src, Uint32 len){
  Uint32 ret = 0;
  for(Uint32 i = 0; i<len; i++)
    ret ^= src[i];
  return ret;
}

SchemaFile::TableEntry *
Dbdict::getTableEntry(Uint32 tableId)
{
  return getTableEntry(&c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE], tableId);
}

SchemaFile::TableEntry *
Dbdict::getTableEntry(XSchemaFile * xsf, Uint32 tableId)
{
  Uint32 n = tableId / NDB_SF_PAGE_ENTRIES;
  Uint32 i = tableId % NDB_SF_PAGE_ENTRIES;
  ndbrequire(n < xsf->noOfPages);

  SchemaFile * sf = &xsf->schemaPage[n];
  return &sf->TableEntries[i];
}

const SchemaFile::TableEntry *
Dbdict::getTableEntry(const XSchemaFile * xsf, Uint32 tableId)
{
  Uint32 n = tableId / NDB_SF_PAGE_ENTRIES;
  Uint32 i = tableId % NDB_SF_PAGE_ENTRIES;
  ndbrequire(n < xsf->noOfPages);

  SchemaFile * sf = &xsf->schemaPage[n];
  return &sf->TableEntries[i];
}

//******************************************

// MODULE: CreateFile

const Dbdict::OpInfo
Dbdict::CreateFileRec::g_opInfo = {
  { 'C', 'F', 'l', 0 },
  ~RT_DBDICT_CREATE_FILE,
  GSN_CREATE_FILE_IMPL_REQ,
  CreateFileImplReq::SignalLength,
  //
  &Dbdict::createFile_seize,
  &Dbdict::createFile_release,
  //
  &Dbdict::createFile_parse,
  &Dbdict::createFile_subOps,
  &Dbdict::createFile_reply,
  //
  &Dbdict::createFile_prepare,
  &Dbdict::createFile_commit,
  &Dbdict::createFile_complete,
  //
  &Dbdict::createFile_abortParse,
  &Dbdict::createFile_abortPrepare
};

void
Dbdict::execCREATE_FILE_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CreateFileReq req_copy =
    *(const CreateFileReq*)signal->getDataPtr();
  const CreateFileReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateFileRecPtr createFilePtr;
    CreateFileImplReq* impl_req;

    startClientReq(op_ptr, createFilePtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->file_id = RNIL;
    impl_req->file_version = 0;
    impl_req->requestInfo = CreateFileImplReq::Create;
    if (req->requestInfo & CreateFileReq::ForceCreateFile)
    {
      jam();
      impl_req->requestInfo = CreateFileImplReq::CreateForce;
    }

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateFileRef* ref = (CreateFileRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_CREATE_FILE_REF, signal,
	     CreateFileRef::SignalLength, JBB);
}

bool
Dbdict::createFile_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateFileRec>(op_ptr);
}

void
Dbdict::createFile_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateFileRec>(op_ptr);
}

// CreateFile: PARSE

void
Dbdict::createFile_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateFileRecPtr createFilePtr;
  getOpRec(op_ptr, createFilePtr);
  CreateFileImplReq* impl_req = &createFilePtr.p->m_request;

  SegmentedSectionPtr objInfoPtr;
  {
    bool ok = handle.getSection(objInfoPtr, 0);
    if (!ok)
    {
      jam();
      setError(error, CreateTableRef::InvalidFormat, __LINE__);
      return;
    }
  }
  SimplePropertiesSectionReader it(objInfoPtr, getSectionSegmentPool());

  DictObjectPtr obj_ptr; obj_ptr.setNull();
  FilePtr filePtr; filePtr.setNull();

  DictFilegroupInfo::File f; f.init();
  SimpleProperties::UnpackStatus status;
  status = SimpleProperties::unpack(it, &f,
				    DictFilegroupInfo::FileMapping,
				    DictFilegroupInfo::FileMappingSize,
				    true, true);

  if (status != SimpleProperties::Eof)
  {
    jam();
    setError(error, CreateFileRef::InvalidFormat, __LINE__);
    return;
  }

  // Get Filegroup
  FilegroupPtr fg_ptr;
  if (!find_object(fg_ptr, f.FilegroupId))
  {
    jam();
    setError(error, CreateFileRef::NoSuchFilegroup, __LINE__, f.FileName);
    return;
  }

  if(fg_ptr.p->m_version != f.FilegroupVersion)
  {
    jam();
    setError(error, CreateFileRef::InvalidFilegroupVersion, __LINE__,
             f.FileName);
    return;
  }

  switch(f.FileType){
  case DictTabInfo::Datafile:
  {
    if(fg_ptr.p->m_type != DictTabInfo::Tablespace)
    {
      jam();
      setError(error, CreateFileRef::InvalidFileType, __LINE__, f.FileName);
      return;
    }
    break;
  }
  case DictTabInfo::Undofile:
  {
    if(fg_ptr.p->m_type != DictTabInfo::LogfileGroup)
    {
      jam();
      setError(error, CreateFileRef::InvalidFileType, __LINE__, f.FileName);
      return;
    }
    break;
  }
  default:
    jam();
    setError(error, CreateFileRef::InvalidFileType, __LINE__, f.FileName);
    return;
  }

  Uint32 len = Uint32(strlen(f.FileName) + 1);
  Uint32 hash = LocalRope::hash(f.FileName, len);
  if(get_object(f.FileName, len, hash) != 0)
  {
    jam();
    setError(error, CreateFileRef::FilenameAlreadyExists, __LINE__, f.FileName);
    return;
  }

  {
    Uint32 dl;
    const ndb_mgm_configuration_iterator * p =
      m_ctx.m_config.getOwnConfigIterator();
    if(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &dl) && dl)
    {
      jam();
      setError(error, CreateFileRef::NotSupportedWhenDiskless, __LINE__,
               f.FileName);
      return;
    }
  }

  if (fg_ptr.p->m_type == DictTabInfo::Tablespace &&
      f.FileSizeHi == 0 &&
      f.FileSizeLo < fg_ptr.p->m_tablespace.m_extent_size)
  {
    jam();
    setError(error, CreateFileRef::FileSizeTooSmall, __LINE__, f.FileName);
    return;
  }

  if(!c_obj_pool.seize(obj_ptr))
  {
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__, f.FileName);
    goto error;
  }
  new (obj_ptr.p) DictObject;

  if (! c_file_pool.seize(filePtr))
  {
    jam();
    setError(error, CreateFileRef::OutOfFileRecords, __LINE__, f.FileName);
    goto error;
  }

  new (filePtr.p) File();

  {
    LocalRope name(c_rope_pool, obj_ptr.p->m_name);
    if(!name.assign(f.FileName, len, hash))
    {
      jam();
      setError(error, CreateTableRef::OutOfStringBuffer, __LINE__, f.FileName);
      goto error;
    }
  }

  if (master)
  {
    jam();

    Uint32 objId = getFreeObjId();
    if (objId == RNIL)
    {
      jam();
      setError(error, CreateFilegroupRef::NoMoreObjectRecords, __LINE__,
               f.FileName);
      goto error;
    }
    Uint32 version = getTableEntry(objId)->m_tableVersion;

    impl_req->file_id = objId;
    impl_req->file_version = create_obj_inc_schema_version(version);
  }
  else if (op_ptr.p->m_restart)
  {
    jam();
    impl_req->file_id = c_restartRecord.activeTable;
    impl_req->file_version = c_restartRecord.m_entry.m_tableVersion;
    switch(op_ptr.p->m_restart){
    case 1:
      jam();
      impl_req->requestInfo = CreateFileImplReq::Open;
      break;
    case 2:
      impl_req->requestInfo = CreateFileImplReq::CreateForce;
      break;
    }
  }

  /**
   * Init file
   */
  filePtr.p->key = impl_req->file_id;
  filePtr.p->m_file_size = ((Uint64)f.FileSizeHi) << 32 | f.FileSizeLo;
  if (fg_ptr.p->m_type == DictTabInfo::Tablespace)
  {
    // round down to page size and up to extent size - Tsman::open_file
    const Uint64 page_size = (Uint64)File_formats::NDB_PAGE_SIZE;
    const Uint64 extent_size = (Uint64)fg_ptr.p->m_tablespace.m_extent_size;
    ndbrequire(extent_size != 0);
    if (filePtr.p->m_file_size % page_size != 0 &&
        !ERROR_INSERTED(6030))
    {
      jam();
      filePtr.p->m_file_size /= page_size;
      filePtr.p->m_file_size *= page_size;
      createFilePtr.p->m_warningFlags |= CreateFileConf::WarnDatafileRoundDown;
    }
    if (filePtr.p->m_file_size % extent_size != 0 &&
        !ERROR_INSERTED(6030))
    {
      jam();
      filePtr.p->m_file_size +=
        extent_size - filePtr.p->m_file_size % extent_size;
      createFilePtr.p->m_warningFlags |= CreateFileConf::WarnDatafileRoundUp;
    }
  }
  if (fg_ptr.p->m_type == DictTabInfo::LogfileGroup)
  {
    // round down to page size - Lgman::Undofile::Undofile
    const Uint64 page_size = (Uint64)File_formats::NDB_PAGE_SIZE;
    if (filePtr.p->m_file_size % page_size != 0 &&
        !ERROR_INSERTED(6030))
    {
      jam();
      filePtr.p->m_file_size /= page_size;
      filePtr.p->m_file_size *= page_size;
      createFilePtr.p->m_warningFlags |= CreateFileConf::WarnUndofileRoundDown;
    }
  }
  filePtr.p->m_path = obj_ptr.p->m_name;
  filePtr.p->m_obj_ptr_i = obj_ptr.i;
  filePtr.p->m_filegroup_id = f.FilegroupId;
  filePtr.p->m_type = f.FileType;
  filePtr.p->m_version = impl_req->file_version;

  obj_ptr.p->m_id = impl_req->file_id;
  obj_ptr.p->m_type = f.FileType;
  obj_ptr.p->m_ref_count = 0;

  ndbrequire(link_object(obj_ptr, filePtr));

  {
    SchemaFile::TableEntry te; te.init();
    te.m_tableState = SchemaFile::SF_CREATE;
    te.m_tableVersion = filePtr.p->m_version;
    te.m_tableType = filePtr.p->m_type;
    te.m_info_words = objInfoPtr.sz;
    te.m_gcp = 0;
    te.m_transId = trans_ptr.p->m_transId;

    Uint32 err = trans_log_schema_op(op_ptr, impl_req->file_id, &te);
    if (err)
    {
      jam();
      setError(error, err, __LINE__);
      goto error;
    }
  }

  c_obj_name_hash.add(obj_ptr);
  c_obj_id_hash.add(obj_ptr);

  // save sections to DICT memory
  saveOpSection(op_ptr, handle, 0);

  switch(fg_ptr.p->m_type){
  case DictTabInfo::Tablespace:
  {
    jam();
    increase_ref_count(fg_ptr.p->m_obj_ptr_i);
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    jam();
    Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
    list.add(filePtr);
    break;
  }
  default:
    ndbrequire(false);
  }

  createFilePtr.p->m_parsed = true;


  if (g_trace)
  {
    g_eventLogger->info("Dbdict: %u: create name=%s,id=%u,obj_ptr_i=%d,"
                        "type=%s,bytes=%llu,warn=0x%x",__LINE__,
                        f.FileName,
                        impl_req->file_id,
                        filePtr.p->m_obj_ptr_i,
                        f.FileType == DictTabInfo::Datafile ? "datafile" :
                        f.FileType == DictTabInfo::Undofile ? "undofile" :
                        "<unknown>",
                        filePtr.p->m_file_size,
                        createFilePtr.p->m_warningFlags);
  }

  send_event(signal, trans_ptr,
             NDB_LE_CreateSchemaObject,
             impl_req->file_id,
             impl_req->file_version,
             f.FileType);

  return;
error:
  if (!filePtr.isNull())
  {
    jam();
    c_file_pool.release(filePtr);
  }

  if (!obj_ptr.isNull())
  {
    jam();
    release_object(obj_ptr.i, obj_ptr.p);
  }
}

void
Dbdict::createFile_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateFileRecPtr createFilePtr;
  getOpRec(op_ptr, createFilePtr);
  CreateFileImplReq* impl_req = &createFilePtr.p->m_request;

  if (createFilePtr.p->m_parsed)
  {
    FilePtr f_ptr;
    FilegroupPtr fg_ptr;
    ndbrequire(find_object(f_ptr, impl_req->file_id));
    ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));
    if (f_ptr.p->m_type == DictTabInfo::Datafile)
    {
      jam();
      decrease_ref_count(fg_ptr.p->m_obj_ptr_i);
    }
    else if (f_ptr.p->m_type == DictTabInfo::Undofile)
    {
      jam();
      Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
      list.remove(f_ptr);
    }

    release_object(f_ptr.p->m_obj_ptr_i);
    c_file_pool.release(f_ptr);
  }

  sendTransConf(signal, op_ptr);
}

bool
Dbdict::createFile_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::createFile_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateFileRecPtr createFileRecPtr;
  getOpRec(op_ptr, createFileRecPtr);
  CreateFileImplReq* impl_req = &createFileRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    CreateFileConf* conf = (CreateFileConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->fileId = impl_req->file_id;
    conf->fileVersion = impl_req->file_version;
    conf->warningFlags = createFileRecPtr.p->m_warningFlags;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_FILE_CONF, signal,
               CreateFileConf::SignalLength, JBB);
  }
  else
  {
    jam();
    CreateFileRef* ref = (CreateFileRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_FILE_REF, signal,
               CreateFileRef::SignalLength, JBB);
  }
}

// CreateFile: PREPARE

void
Dbdict::createFile_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateFileRecPtr createFileRecPtr;
  getOpRec(op_ptr, createFileRecPtr);
  CreateFileImplReq* impl_req = &createFileRecPtr.p->m_request;

  Callback cb =  {
    safe_cast(&Dbdict::createFile_fromWriteObjInfo), op_ptr.p->op_key
  };

  if (ZRESTART_NO_WRITE_AFTER_READ && op_ptr.p->m_restart == 1)
  {
    jam();
    /**
     * We read obj from disk, no need to rewrite it
     */
    execute(signal, cb, 0);
    return;
  }

  const OpSection& objInfoSec = getOpSection(op_ptr, 0);
  writeTableFile(signal, op_ptr, impl_req->file_id, objInfoSec, &cb);
}

void
Dbdict::createFile_fromWriteObjInfo(Signal* signal,
                                    Uint32 op_key,
                                    Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateFileRecPtr createFileRecPtr;
  ndbrequire(findSchemaOp(op_ptr, createFileRecPtr, op_key));
  CreateFileImplReq* impl_req = &createFileRecPtr.p->m_request;

  if (ret)
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  /**
   * CONTACT TSMAN LGMAN PGMAN
   */
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  ndbrequire(find_object(f_ptr, impl_req->file_id));
  ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op_ptr.p->op_key;
  req->senderRef = reference();

  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;
  req->file_size_hi = (Uint32)(f_ptr.p->m_file_size >> 32);
  req->file_size_lo = (Uint32)(f_ptr.p->m_file_size & 0xFFFFFFFF);
  req->requestInfo = impl_req->requestInfo;

  Uint32 ref= 0;
  Uint32 len= 0;
  switch(f_ptr.p->m_type){
  case DictTabInfo::Datafile:
  {
    jam();
    ref = TSMAN_REF;
    len = CreateFileImplReq::DatafileLength;
    req->tablespace.extent_size = fg_ptr.p->m_tablespace.m_extent_size;
    break;
  }
  case DictTabInfo::Undofile:
  {
    jam();
    ref = LGMAN_REF;
    len = CreateFileImplReq::UndofileLength;
    break;
  }
  default:
    ndbrequire(false);
  }

  char name[PATH_MAX];
  ConstRope tmp(c_rope_pool, f_ptr.p->m_path);
  tmp.copy(name);
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)&name[0];
  ptr[0].sz = Uint32(strlen(name)+1+3)/4;
  sendSignal(ref, GSN_CREATE_FILE_IMPL_REQ, signal, len, JBB, ptr, 1);

  Callback c =  {
    safe_cast(&Dbdict::createFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;
}

void
Dbdict::createFile_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  CreateFileRecPtr createFileRecPtr;
  getOpRec(op_ptr, createFileRecPtr);
  CreateFileImplReq* impl_req = &createFileRecPtr.p->m_request;

  ndbrequire(find_object(f_ptr, impl_req->file_id));
  ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op_ptr.p->op_key;
  req->senderRef = reference();
  req->requestInfo = CreateFileImplReq::Abort;
  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;

  Uint32 ref= 0;
  switch(f_ptr.p->m_type){
  case DictTabInfo::Datafile:
  {
    jam();
    ref = TSMAN_REF;
    break;
  }
  case DictTabInfo::Undofile:
  {
    jam();
    ref = LGMAN_REF;
    break;
  }
  default:
    ndbrequire(false);
  }

  sendSignal(ref, GSN_CREATE_FILE_IMPL_REQ, signal,
             CreateFileImplReq::AbortLength, JBB);

  Callback c =  {
    safe_cast(&Dbdict::createFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;
}

// CreateFile: COMMIT

void
Dbdict::createFile_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateFileRecPtr createFileRecPtr;
  getOpRec(op_ptr, createFileRecPtr);
  CreateFileImplReq* impl_req = &createFileRecPtr.p->m_request;

  /**
   * CONTACT TSMAN LGMAN PGMAN
   */
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  ndbrequire(find_object(f_ptr, impl_req->file_id));
  ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op_ptr.p->op_key;
  req->senderRef = reference();
  req->requestInfo = CreateFileImplReq::Commit;

  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;

  Uint32 ref= 0;
  switch(f_ptr.p->m_type){
  case DictTabInfo::Datafile:
  {
    jam();
    ref = TSMAN_REF;
    break;
  }
  case DictTabInfo::Undofile:
  {
    jam();
    ref = LGMAN_REF;
    break;
  }
  default:
    ndbrequire(false);
  }
  sendSignal(ref, GSN_CREATE_FILE_IMPL_REQ, signal,
	     CreateFileImplReq::CommitLength, JBB);

  Callback c =  {
    safe_cast(&Dbdict::createFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;
}

// CreateFile: COMPLETE

void
Dbdict::createFile_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

void
Dbdict::createFile_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  CreateFileRecPtr createFilePtr;
  ndbrequire(findSchemaOp(op_ptr, createFilePtr, op_key));

  if (ret == 0)
  {
    jam();
    sendTransConf(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::execCREATE_FILE_IMPL_REF(Signal* signal)
{
  jamEntry();
  CreateFileImplRef * ref = (CreateFileImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCREATE_FILE_IMPL_CONF(Signal* signal)
{
  jamEntry();
  CreateFileImplConf * conf = (CreateFileImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

// CreateFile: END

// MODULE: CreateFilegroup

const Dbdict::OpInfo
Dbdict::CreateFilegroupRec::g_opInfo = {
  { 'C', 'F', 'G', 0 },
  ~RT_DBDICT_CREATE_FILEGROUP,
  GSN_CREATE_FILEGROUP_IMPL_REQ,
  CreateFilegroupImplReq::SignalLength,
  //
  &Dbdict::createFilegroup_seize,
  &Dbdict::createFilegroup_release,
  //
  &Dbdict::createFilegroup_parse,
  &Dbdict::createFilegroup_subOps,
  &Dbdict::createFilegroup_reply,
  //
  &Dbdict::createFilegroup_prepare,
  &Dbdict::createFilegroup_commit,
  &Dbdict::createFilegroup_complete,
  //
  &Dbdict::createFilegroup_abortParse,
  &Dbdict::createFilegroup_abortPrepare
};

void
Dbdict::execCREATE_FILEGROUP_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CreateFilegroupReq req_copy =
    *(const CreateFilegroupReq*)signal->getDataPtr();
  const CreateFilegroupReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateFilegroupRecPtr createFilegroupPtr;
    CreateFilegroupImplReq* impl_req;

    startClientReq(op_ptr, createFilegroupPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->filegroup_id = RNIL;
    impl_req->filegroup_version = 0;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateFilegroupRef* ref = (CreateFilegroupRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_CREATE_FILEGROUP_REF, signal,
	     CreateFilegroupRef::SignalLength, JBB);
}

bool
Dbdict::createFilegroup_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateFilegroupRec>(op_ptr);
}

void
Dbdict::createFilegroup_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateFilegroupRec>(op_ptr);
}

// CreateFilegroup: PARSE

void
Dbdict::createFilegroup_parse(Signal* signal, bool master,
                              SchemaOpPtr op_ptr,
                              SectionHandle& handle, ErrorInfo& error)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateFilegroupRecPtr createFilegroupPtr;
  getOpRec(op_ptr, createFilegroupPtr);
  CreateFilegroupImplReq* impl_req = &createFilegroupPtr.p->m_request;

  SegmentedSectionPtr objInfoPtr;
  {
    bool ok = handle.getSection(objInfoPtr, 0);
    if (!ok)
    {
      jam();
      setError(error, CreateTableRef::InvalidFormat, __LINE__);
      return;
    }
  }
  SimplePropertiesSectionReader it(objInfoPtr, getSectionSegmentPool());

  DictObjectPtr obj_ptr; obj_ptr.setNull();
  FilegroupPtr fg_ptr; fg_ptr.setNull();

  DictFilegroupInfo::Filegroup fg; fg.init();
  SimpleProperties::UnpackStatus status =
    SimpleProperties::unpack(it, &fg,
                             DictFilegroupInfo::Mapping,
                             DictFilegroupInfo::MappingSize,
                             true, true);

  if(status != SimpleProperties::Eof)
  {
    jam();
    setError(error, CreateTableRef::InvalidFormat, __LINE__);
    return;
  }

  if(fg.FilegroupType == DictTabInfo::Tablespace)
  {
    if(!fg.TS_ExtentSize)
    {
      jam();
      setError(error, CreateFilegroupRef::InvalidExtentSize, __LINE__);
      return;
    }
  }
  else if(fg.FilegroupType == DictTabInfo::LogfileGroup)
  {
    /**
     * undo_buffer_size can't be less than 96KB in LGMAN block
     */
    if(fg.LF_UndoBufferSize < 3 * File_formats::NDB_PAGE_SIZE)
    {
      jam();
      setError(error, CreateFilegroupRef::InvalidUndoBufferSize, __LINE__);
      return;
    }
  }

  Uint32 len = Uint32(strlen(fg.FilegroupName) + 1);
  Uint32 hash = LocalRope::hash(fg.FilegroupName, len);
  if(get_object(fg.FilegroupName, len, hash) != 0)
  {
    jam();
    setError(error, CreateTableRef::TableAlreadyExist, __LINE__);
    return;
  }

  if(!c_obj_pool.seize(obj_ptr))
  {
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__);
    return;
  }
  new (obj_ptr.p) DictObject;

  Uint32 inc_obj_ptr_i = RNIL;
  if(!c_filegroup_pool.seize(fg_ptr))
  {
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__);
    goto error;
  }

  new (fg_ptr.p) Filegroup();

  {
    LocalRope name(c_rope_pool, obj_ptr.p->m_name);
    if(!name.assign(fg.FilegroupName, len, hash))
    {
      jam();
      setError(error, CreateTableRef::OutOfStringBuffer, __LINE__);
      goto error;
    }
  }

  switch(fg.FilegroupType){
  case DictTabInfo::Tablespace:
  {
    //fg.TS_DataGrow = group.m_grow_spec;
    fg_ptr.p->m_tablespace.m_extent_size = fg.TS_ExtentSize;
    // round up to page size - Tsman::Tablespace::Tablespace
    const Uint32 page_size = (Uint32)File_formats::NDB_PAGE_SIZE;
    if (fg_ptr.p->m_tablespace.m_extent_size % page_size != 0 &&
        !ERROR_INSERTED(6030))
    {
      jam();
      fg_ptr.p->m_tablespace.m_extent_size +=
        page_size - fg_ptr.p->m_tablespace.m_extent_size % page_size;
      createFilegroupPtr.p->m_warningFlags |= CreateFilegroupConf::WarnExtentRoundUp;
    }
#if defined VM_TRACE || defined ERROR_INSERT
    ndbout << "DD dict: ts id:" << "?" << " extent bytes:" << fg_ptr.p->m_tablespace.m_extent_size << " warn:" << hex << createFilegroupPtr.p->m_warningFlags << endl;
#endif
    fg_ptr.p->m_tablespace.m_default_logfile_group_id = fg.TS_LogfileGroupId;

    FilegroupPtr lg_ptr;
    if (!find_object(lg_ptr, fg.TS_LogfileGroupId))
    {
      jam();
      setError(error, CreateFilegroupRef::NoSuchLogfileGroup, __LINE__);
      goto error;
    }

    if (lg_ptr.p->m_version != fg.TS_LogfileGroupVersion)
    {
      jam();
      setError(error, CreateFilegroupRef::InvalidFilegroupVersion, __LINE__);
      goto error;
    }
    inc_obj_ptr_i = lg_ptr.p->m_obj_ptr_i;
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    jam();
    fg_ptr.p->m_logfilegroup.m_undo_buffer_size = fg.LF_UndoBufferSize;
    // round up to page size - Lgman::alloc_logbuffer_memory
    const Uint32 page_size = (Uint32)File_formats::NDB_PAGE_SIZE;
    if (fg_ptr.p->m_logfilegroup.m_undo_buffer_size % page_size != 0 &&
        !ERROR_INSERTED(6030))
    {
      jam();
      fg_ptr.p->m_logfilegroup.m_undo_buffer_size +=
        page_size - fg_ptr.p->m_logfilegroup.m_undo_buffer_size % page_size;
      createFilegroupPtr.p->m_warningFlags |= CreateFilegroupConf::WarnUndobufferRoundUp;
    }
#if defined VM_TRACE || defined ERROR_INSERT
    ndbout << "DD dict: fg id:" << "?" << " undo buffer bytes:" << fg_ptr.p->m_logfilegroup.m_undo_buffer_size << " warn:" << hex << createFilegroupPtr.p->m_warningFlags << endl;
#endif
    fg_ptr.p->m_logfilegroup.m_files.init();
    //fg.LF_UndoGrow = ;
    break;
  }
  default:
    ndbrequire(false);
  }

  if (master)
  {
    jam();

    Uint32 objId = getFreeObjId();
    if (objId == RNIL)
    {
      jam();
      setError(error, CreateFilegroupRef::NoMoreObjectRecords, __LINE__);
      goto error;
    }
    Uint32 version = getTableEntry(objId)->m_tableVersion;

    impl_req->filegroup_id = objId;
    impl_req->filegroup_version = create_obj_inc_schema_version(version);
  }
  else if (op_ptr.p->m_restart)
  {
    jam();
    impl_req->filegroup_id = c_restartRecord.activeTable;
    impl_req->filegroup_version = c_restartRecord.m_entry.m_tableVersion;
  }

  fg_ptr.p->key = impl_req->filegroup_id;
  fg_ptr.p->m_type = fg.FilegroupType;
  fg_ptr.p->m_version = impl_req->filegroup_version;
  fg_ptr.p->m_name = obj_ptr.p->m_name;

  obj_ptr.p->m_id = impl_req->filegroup_id;
  obj_ptr.p->m_type = fg.FilegroupType;
  obj_ptr.p->m_ref_count = 0;

  ndbrequire(link_object(obj_ptr, fg_ptr));

  if (master)
  {
    jam();
    releaseSections(handle);
    SimplePropertiesSectionWriter w(*this);
    packFilegroupIntoPages(w, fg_ptr, 0, 0);
    w.getPtr(objInfoPtr);
    handle.m_ptr[0] = objInfoPtr;
    handle.m_cnt = 1;
  }

  {
    SchemaFile::TableEntry te; te.init();
    te.m_tableState = SchemaFile::SF_CREATE;
    te.m_tableVersion = fg_ptr.p->m_version;
    te.m_tableType = fg_ptr.p->m_type;
    te.m_info_words = objInfoPtr.sz;
    te.m_gcp = 0;
    te.m_transId = trans_ptr.p->m_transId;

    Uint32 err = trans_log_schema_op(op_ptr, impl_req->filegroup_id, &te);
    if (err)
    {
      jam();
      setError(error, err, __LINE__);
      goto error;
    }
  }

  c_obj_name_hash.add(obj_ptr);
  c_obj_id_hash.add(obj_ptr);

  // save sections to DICT memory
  saveOpSection(op_ptr, handle, 0);

  if (inc_obj_ptr_i != RNIL)
  {
    jam();
    increase_ref_count(inc_obj_ptr_i);
  }
  createFilegroupPtr.p->m_parsed = true;

#if defined VM_TRACE || defined ERROR_INSERT
  ndbout_c("Dbdict: %u: create name=%s,id=%u,obj_ptr_i=%d",__LINE__,
           fg.FilegroupName, impl_req->filegroup_id, fg_ptr.p->m_obj_ptr_i);
#endif

  return;

error:
  jam();
  if (!fg_ptr.isNull())
  {
    jam();
    c_filegroup_pool.release(fg_ptr);
  }

  if (!obj_ptr.isNull())
  {
    jam();
    release_object(obj_ptr.i, obj_ptr.p);
  }
}

void
Dbdict::createFilegroup_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateFilegroupRecPtr createFilegroupPtr;
  getOpRec(op_ptr, createFilegroupPtr);

  if (createFilegroupPtr.p->m_parsed)
  {
    jam();
    CreateFilegroupImplReq* impl_req = &createFilegroupPtr.p->m_request;

    FilegroupPtr fg_ptr;
    ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

    if (fg_ptr.p->m_type == DictTabInfo::Tablespace)
    {
      jam();
      FilegroupPtr lg_ptr;
      ndbrequire(find_object
                 (lg_ptr, fg_ptr.p->m_tablespace.m_default_logfile_group_id));
      decrease_ref_count(lg_ptr.p->m_obj_ptr_i);
    }

    release_object(fg_ptr.p->m_obj_ptr_i);
    c_filegroup_pool.release(fg_ptr);
  }

  sendTransConf(signal, op_ptr);
}


bool
Dbdict::createFilegroup_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::createFilegroup_reply(Signal* signal,
                              SchemaOpPtr op_ptr,
                              ErrorInfo error)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateFilegroupRecPtr createFilegroupRecPtr;
  getOpRec(op_ptr, createFilegroupRecPtr);
  CreateFilegroupImplReq* impl_req = &createFilegroupRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    CreateFilegroupConf* conf = (CreateFilegroupConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->filegroupId = impl_req->filegroup_id;
    conf->filegroupVersion = impl_req->filegroup_version;
    conf->warningFlags = createFilegroupRecPtr.p->m_warningFlags;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_FILEGROUP_CONF, signal,
               CreateFilegroupConf::SignalLength, JBB);
  } else {
    jam();
    CreateFilegroupRef* ref = (CreateFilegroupRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_FILEGROUP_REF, signal,
               CreateFilegroupRef::SignalLength, JBB);
  }
}

// CreateFilegroup: PREPARE

void
Dbdict::createFilegroup_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateFilegroupRecPtr createFilegroupRecPtr;
  getOpRec(op_ptr, createFilegroupRecPtr);
  CreateFilegroupImplReq* impl_req = &createFilegroupRecPtr.p->m_request;

  Callback cb =  {
    safe_cast(&Dbdict::createFilegroup_fromWriteObjInfo), op_ptr.p->op_key
  };

  if (ZRESTART_NO_WRITE_AFTER_READ && op_ptr.p->m_restart == 1)
  {
    jam();
    /**
     * We read obj from disk, no need to rewrite it
     */
    execute(signal, cb, 0);
    return;
  }

  const OpSection& objInfoSec = getOpSection(op_ptr, 0);
  writeTableFile(signal, op_ptr, impl_req->filegroup_id, objInfoSec, &cb);
}

void
Dbdict::createFilegroup_fromWriteObjInfo(Signal* signal,
                                         Uint32 op_key,
                                         Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateFilegroupRecPtr createFilegroupRecPtr;
  ndbrequire(findSchemaOp(op_ptr, createFilegroupRecPtr, op_key));

  if (ret)
  {
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  /**
   * CONTACT TSMAN LGMAN PGMAN
   */
  CreateFilegroupImplReq* impl_req = &createFilegroupRecPtr.p->m_request;

  CreateFilegroupImplReq* req =
    (CreateFilegroupImplReq*)signal->getDataPtrSend();
  jam();
  req->senderData = op_ptr.p->op_key;
  req->senderRef = reference();
  req->filegroup_id = impl_req->filegroup_id;
  req->filegroup_version = impl_req->filegroup_version;

  FilegroupPtr fg_ptr;

  ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

  Uint32 ref= 0;
  Uint32 len= 0;
  switch(fg_ptr.p->m_type){
  case DictTabInfo::Tablespace:
  {
    jam();
    ref = TSMAN_REF;
    len = CreateFilegroupImplReq::TablespaceLength;
    req->tablespace.extent_size = fg_ptr.p->m_tablespace.m_extent_size;
    req->tablespace.logfile_group_id =
      fg_ptr.p->m_tablespace.m_default_logfile_group_id;
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    jam();
    ref = LGMAN_REF;
    len = CreateFilegroupImplReq::LogfileGroupLength;
    req->logfile_group.buffer_size =
      fg_ptr.p->m_logfilegroup.m_undo_buffer_size;
    break;
  }
  default:
    ndbrequire(false);
  }

  sendSignal(ref, GSN_CREATE_FILEGROUP_IMPL_REQ, signal, len, JBB);

  Callback c =  {
    safe_cast(&Dbdict::createFilegroup_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;
}

// CreateFilegroup: COMMIT

void
Dbdict::createFilegroup_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  /**
   * cheat...only create...abort is implemented as DROP
   */
  sendTransConf(signal, op_ptr);
}

// CreateFilegroup: COMPLETE

void
Dbdict::createFilegroup_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

void
Dbdict::createFilegroup_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateFilegroupRecPtr createFilegroupRecPtr;
  getOpRec(op_ptr, createFilegroupRecPtr);
  CreateFilegroupImplReq* impl_req = &createFilegroupRecPtr.p->m_request;

  if (createFilegroupRecPtr.p->m_prepared)
  {
    Callback c =  {
      safe_cast(&Dbdict::createFilegroup_fromLocal), op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    send_drop_fg(signal, op_ptr.p->op_key, impl_req->filegroup_id,
                 DropFilegroupImplReq::Prepare);
  }

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createFilegroup_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  CreateFilegroupRecPtr createFilegroupPtr;
  ndbrequire(findSchemaOp(op_ptr, createFilegroupPtr, op_key));
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0)
  {
    jam();
    createFilegroupPtr.p->m_prepared = true;
    sendTransConf(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::execCREATE_FILEGROUP_IMPL_REF(Signal* signal)
{
  jamEntry();
  CreateFilegroupImplRef * ref = (CreateFilegroupImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCREATE_FILEGROUP_IMPL_CONF(Signal* signal)
{
  jamEntry();
  CreateFilegroupImplConf * conf =
    (CreateFilegroupImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

// CreateFilegroup: END

// MODULE: DropFile

const Dbdict::OpInfo
Dbdict::DropFileRec::g_opInfo = {
  { 'D', 'F', 'l', 0 },
  ~RT_DBDICT_DROP_FILE,
  GSN_DROP_FILE_IMPL_REQ,
  DropFileImplReq::SignalLength,
  //
  &Dbdict::dropFile_seize,
  &Dbdict::dropFile_release,
  //
  &Dbdict::dropFile_parse,
  &Dbdict::dropFile_subOps,
  &Dbdict::dropFile_reply,
  //
  &Dbdict::dropFile_prepare,
  &Dbdict::dropFile_commit,
  &Dbdict::dropFile_complete,
  //
  &Dbdict::dropFile_abortParse,
  &Dbdict::dropFile_abortPrepare
};

void
Dbdict::execDROP_FILE_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const DropFileReq req_copy =
    *(const DropFileReq*)signal->getDataPtr();
  const DropFileReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropFileRecPtr dropFilePtr;
    DropFileImplReq* impl_req;

    startClientReq(op_ptr, dropFilePtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->file_id = req->file_id;
    impl_req->file_version = req->file_version;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropFileRef* ref = (DropFileRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_DROP_FILE_REF, signal,
	     DropFileRef::SignalLength, JBB);
}

bool
Dbdict::dropFile_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropFileRec>(op_ptr);
}

void
Dbdict::dropFile_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropFileRec>(op_ptr);
}

// DropFile: PARSE

void
Dbdict::dropFile_parse(Signal* signal, bool master,
                       SchemaOpPtr op_ptr,
                       SectionHandle& handle, ErrorInfo& error)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFileRecPtr dropFileRecPtr;
  getOpRec(op_ptr, dropFileRecPtr);
  DropFileImplReq* impl_req = &dropFileRecPtr.p->m_request;

  FilePtr f_ptr;
  if (!find_object(f_ptr, impl_req->file_id))
  {
    jam();
    setError(error, DropFileRef::NoSuchFile, __LINE__);
    return;
  }

  if (f_ptr.p->m_version != impl_req->file_version)
  {
    jam();
    setError(error, DropFileRef::InvalidSchemaObjectVersion, __LINE__);
    return;
  }

  if (f_ptr.p->m_type == DictTabInfo::Undofile)
  {
    jam();
    setError(error, DropFileRef::DropUndoFileNotSupported, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->file_id,
                      trans_ptr.p->m_transId,
                      SchemaFile::SF_DROP, error))
  {
    jam();
    return;
  }

  SchemaFile::TableEntry te; te.init();
  te.m_tableState = SchemaFile::SF_DROP;
  te.m_transId = trans_ptr.p->m_transId;
  Uint32 err = trans_log_schema_op(op_ptr, impl_req->file_id, &te);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }

#if defined VM_TRACE || defined ERROR_INSERT
  {
    char buf[1024];
    LocalRope name(c_rope_pool, f_ptr.p->m_path);
    name.copy(buf);
    ndbout_c("Dbdict: drop name=%s,id=%u,obj_id=%u", buf,
             impl_req->file_id,
             f_ptr.p->m_obj_ptr_i);
  }
#endif
}

void
Dbdict::dropFile_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

bool
Dbdict::dropFile_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::dropFile_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFileRecPtr dropFileRecPtr;
  getOpRec(op_ptr, dropFileRecPtr);
  DropFileImplReq* impl_req = &dropFileRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    DropFileConf* conf = (DropFileConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->fileId = impl_req->file_id;
    conf->fileVersion = impl_req->file_version;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_FILE_CONF, signal,
               DropFileConf::SignalLength, JBB);
  }
  else
  {
    jam();
    DropFileRef* ref = (DropFileRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_FILE_REF, signal,
               DropFileRef::SignalLength, JBB);
  }
}

// DropFile: PREPARE

void
Dbdict::dropFile_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  DropFileRecPtr dropFileRecPtr;
  getOpRec(op_ptr, dropFileRecPtr);
  DropFileImplReq* impl_req = &dropFileRecPtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  send_drop_file(signal, op_ptr.p->op_key, impl_req->file_id,
                 DropFileImplReq::Prepare);
}

void
Dbdict::dropFile_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFileRecPtr dropFilePtr;
  getOpRec(op_ptr, dropFilePtr);
  DropFileImplReq* impl_req = &dropFilePtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  send_drop_file(signal, op_ptr.p->op_key, impl_req->file_id,
                 DropFileImplReq::Abort);
}

// DropFile: COMMIT

void
Dbdict::dropFile_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  DropFileRecPtr dropFileRecPtr;
  getOpRec(op_ptr, dropFileRecPtr);
  DropFileImplReq* impl_req = &dropFileRecPtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFile_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;


  send_drop_file(signal, op_ptr.p->op_key, impl_req->file_id,
                 DropFileImplReq::Commit);
}

// DropFile: COMPLETE

void
Dbdict::dropFile_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  DropFileRecPtr dropFileRecPtr;
  getOpRec(op_ptr, dropFileRecPtr);
  DropFileImplReq* impl_req = &dropFileRecPtr.p->m_request;

  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  ndbrequire(find_object(f_ptr, impl_req->file_id));
  ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));
  decrease_ref_count(fg_ptr.p->m_obj_ptr_i);
  release_object(f_ptr.p->m_obj_ptr_i);
  c_file_pool.release(f_ptr);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropFile_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  DropFileRecPtr dropFilePtr;
  ndbrequire(findSchemaOp(op_ptr, dropFilePtr, op_key));

  if (ret == 0)
  {
    jam();
    sendTransConf(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::execDROP_FILE_IMPL_REF(Signal* signal)
{
  jamEntry();
  DropFileImplRef * ref = (DropFileImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execDROP_FILE_IMPL_CONF(Signal* signal)
{
  jamEntry();
  DropFileImplConf * conf = (DropFileImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::send_drop_file(Signal* signal, Uint32 op_key, Uint32 fileId,
		       DropFileImplReq::RequestInfo type)
{
  DropFileImplReq* req = (DropFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  ndbrequire(find_object(f_ptr, fileId));
  ndbrequire(find_object(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op_key;
  req->senderRef = reference();
  req->requestInfo = type;

  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;

  Uint32 ref= 0;
  switch(f_ptr.p->m_type){
  case DictTabInfo::Datafile:
  {
    jam();
    ref = TSMAN_REF;
    break;
  }
  case DictTabInfo::Undofile:
  {
    jam();
    ref = LGMAN_REF;
    break;
  }
  default:
    ndbrequire(false);
  }
  sendSignal(ref, GSN_DROP_FILE_IMPL_REQ, signal,
	     DropFileImplReq::SignalLength, JBB);
}

// DropFile: END

// MODULE: DropFilegroup

const Dbdict::OpInfo
Dbdict::DropFilegroupRec::g_opInfo = {
  { 'D', 'F', 'g', 0 },
  ~RT_DBDICT_DROP_FILEGROUP,
  GSN_DROP_FILEGROUP_IMPL_REQ,
  DropFilegroupImplReq::SignalLength,
  //
  &Dbdict::dropFilegroup_seize,
  &Dbdict::dropFilegroup_release,
  //
  &Dbdict::dropFilegroup_parse,
  &Dbdict::dropFilegroup_subOps,
  &Dbdict::dropFilegroup_reply,
  //
  &Dbdict::dropFilegroup_prepare,
  &Dbdict::dropFilegroup_commit,
  &Dbdict::dropFilegroup_complete,
  //
  &Dbdict::dropFilegroup_abortParse,
  &Dbdict::dropFilegroup_abortPrepare
};

void
Dbdict::execDROP_FILEGROUP_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const DropFilegroupReq req_copy =
    *(const DropFilegroupReq*)signal->getDataPtr();
  const DropFilegroupReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropFilegroupRecPtr dropFilegroupPtr;
    DropFilegroupImplReq* impl_req;

    startClientReq(op_ptr, dropFilegroupPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->filegroup_id = req->filegroup_id;
    impl_req->filegroup_version = req->filegroup_version;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropFilegroupRef* ref = (DropFilegroupRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_DROP_FILEGROUP_REF, signal,
	     DropFilegroupRef::SignalLength, JBB);
}

bool
Dbdict::dropFilegroup_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropFilegroupRec>(op_ptr);
}

void
Dbdict::dropFilegroup_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropFilegroupRec>(op_ptr);
}

// DropFilegroup: PARSE

void
Dbdict::dropFilegroup_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupRecPtr;
  getOpRec(op_ptr, dropFilegroupRecPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupRecPtr.p->m_request;

  FilegroupPtr fg_ptr;
  if (!find_object(fg_ptr, impl_req->filegroup_id))
  {
    jam();
    setError(error, DropFilegroupRef::NoSuchFilegroup, __LINE__);
    return;
  }

  if (fg_ptr.p->m_version != impl_req->filegroup_version)
  {
    jam();
    setError(error, DropFilegroupRef::InvalidSchemaObjectVersion, __LINE__);
    return;
  }

  DictObject * obj = c_obj_pool.getPtr(fg_ptr.p->m_obj_ptr_i);
  if (obj->m_ref_count)
  {
    jam();
    setError(error, DropFilegroupRef::FilegroupInUse, __LINE__);
    return;
  }

  if (check_write_obj(impl_req->filegroup_id,
                      trans_ptr.p->m_transId,
                      SchemaFile::SF_DROP, error))
  {
    jam();
    return;
  }

  SchemaFile::TableEntry te; te.init();
  te.m_tableState = SchemaFile::SF_DROP;
  te.m_transId = trans_ptr.p->m_transId;
  Uint32 err = trans_log_schema_op(op_ptr, impl_req->filegroup_id, &te);
  if (err)
  {
    jam();
    setError(error, err, __LINE__);
    return;
  }

#if defined VM_TRACE || defined ERROR_INSERT
  {
    char buf[1024];
    LocalRope name(c_rope_pool, fg_ptr.p->m_name);
    name.copy(buf);
    ndbout_c("Dbdict: drop name=%s,id=%u,obj_id=%u", buf,
             impl_req->filegroup_id,
             fg_ptr.p->m_obj_ptr_i);
  }
#endif
}

void
Dbdict::dropFilegroup_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

bool
Dbdict::dropFilegroup_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::dropFilegroup_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupRecPtr;
  getOpRec(op_ptr, dropFilegroupRecPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    DropFilegroupConf* conf = (DropFilegroupConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->filegroupId = impl_req->filegroup_id;
    conf->filegroupVersion = impl_req->filegroup_version;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_FILEGROUP_CONF, signal,
               DropFilegroupConf::SignalLength, JBB);
  }
  else
  {
    jam();
    DropFilegroupRef* ref = (DropFilegroupRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_FILEGROUP_REF, signal,
               DropFilegroupRef::SignalLength, JBB);
  }
}

// DropFilegroup: PREPARE

void
Dbdict::dropFilegroup_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupRecPtr;
  getOpRec(op_ptr, dropFilegroupRecPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupRecPtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFilegroup_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  send_drop_fg(signal, op_ptr.p->op_key, impl_req->filegroup_id,
               DropFilegroupImplReq::Prepare);

  FilegroupPtr fg_ptr;
  ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

  if (fg_ptr.p->m_type == DictTabInfo::LogfileGroup)
  {
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    FilePtr filePtr;
    Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
    for(list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
    {
      jam();

      DictObjectPtr objPtr;
      c_obj_pool.getPtr(objPtr, filePtr.p->m_obj_ptr_i);
      SchemaFile::TableEntry * entry = getTableEntry(xsf, objPtr.p->m_id);
      entry->m_tableState = SchemaFile::SF_DROP;
      entry->m_transId = trans_ptr.p->m_transId;
    }
  }
}

void
Dbdict::dropFilegroup_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  ndbrequire(false);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupPtr;
  getOpRec(op_ptr, dropFilegroupPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupPtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFilegroup_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  send_drop_fg(signal, op_ptr.p->op_key, impl_req->filegroup_id,
               DropFilegroupImplReq::Abort);

  FilegroupPtr fg_ptr;
  ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

  if (fg_ptr.p->m_type == DictTabInfo::LogfileGroup)
  {
    jam();
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    FilePtr filePtr;
    Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
    for(list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
    {
      jam();

      DictObjectPtr objPtr;
      c_obj_pool.getPtr(objPtr, filePtr.p->m_obj_ptr_i);
      SchemaFile::TableEntry * entry = getTableEntry(xsf, objPtr.p->m_id);
      entry->m_tableState = SchemaFile::SF_IN_USE;
      entry->m_transId = 0;
    }
  }
}

// DropFilegroup: COMMIT

void
Dbdict::dropFilegroup_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupRecPtr;
  getOpRec(op_ptr, dropFilegroupRecPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupRecPtr.p->m_request;

  Callback c =  {
    safe_cast(&Dbdict::dropFilegroup_fromLocal), op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  send_drop_fg(signal, op_ptr.p->op_key, impl_req->filegroup_id,
               DropFilegroupImplReq::Commit);

  FilegroupPtr fg_ptr;
  ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

  if (fg_ptr.p->m_type == DictTabInfo::LogfileGroup)
  {
    jam();
    /**
     * Mark all undofiles as dropped
     */
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];

    FilePtr filePtr;
    Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
    for(list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
    {
      jam();

      DictObjectPtr objPtr;
      c_obj_pool.getPtr(objPtr, filePtr.p->m_obj_ptr_i);
      SchemaFile::TableEntry * entry = getTableEntry(xsf, objPtr.p->m_id);
      entry->m_tableState = SchemaFile::SF_UNUSED;
      entry->m_transId = 0;

      release_object(objPtr.i, objPtr.p);
    }
    list.release();
  }
  else if(fg_ptr.p->m_type == DictTabInfo::Tablespace)
  {
    jam();
    FilegroupPtr lg_ptr;
    ndbrequire(find_object(lg_ptr,
		    fg_ptr.p->m_tablespace.m_default_logfile_group_id));

    decrease_ref_count(lg_ptr.p->m_obj_ptr_i);
  }
}

// DropFilegroup: COMPLETE

void
Dbdict::dropFilegroup_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropFilegroupRecPtr dropFilegroupRecPtr;
  getOpRec(op_ptr, dropFilegroupRecPtr);
  DropFilegroupImplReq* impl_req = &dropFilegroupRecPtr.p->m_request;

  FilegroupPtr fg_ptr;
  ndbrequire(find_object(fg_ptr, impl_req->filegroup_id));

  release_object(fg_ptr.p->m_obj_ptr_i);
  c_filegroup_pool.release(fg_ptr);

  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropFilegroup_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  DropFilegroupRecPtr dropFilegroupPtr;
  ndbrequire(findSchemaOp(op_ptr, dropFilegroupPtr, op_key));

  if (ret == 0)
  {
    jam();
    sendTransConf(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::execDROP_FILEGROUP_IMPL_REF(Signal* signal)
{
  jamEntry();
  DropFilegroupImplRef * ref = (DropFilegroupImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execDROP_FILEGROUP_IMPL_CONF(Signal* signal)
{
  jamEntry();
  DropFilegroupImplConf * conf = (DropFilegroupImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}


void
Dbdict::send_drop_fg(Signal* signal, Uint32 op_key, Uint32 filegroupId,
		     DropFilegroupImplReq::RequestInfo type)
{
  DropFilegroupImplReq* req = (DropFilegroupImplReq*)signal->getDataPtrSend();

  FilegroupPtr fg_ptr;
  ndbrequire(find_object(fg_ptr, filegroupId));

  req->senderData = op_key;
  req->senderRef = reference();
  req->requestInfo = type;

  req->filegroup_id = fg_ptr.p->key;
  req->filegroup_version = fg_ptr.p->m_version;

  Uint32 ref= 0;
  switch(fg_ptr.p->m_type){
  case DictTabInfo::Tablespace:
    ref = TSMAN_REF;
    break;
  case DictTabInfo::LogfileGroup:
    ref = LGMAN_REF;
    break;
  default:
    ndbrequire(false);
  }

  sendSignal(ref, GSN_DROP_FILEGROUP_IMPL_REQ, signal,
	     DropFilegroupImplReq::SignalLength, JBB);
}

// DropFilegroup: END

// MODULE: CreateNodegroup

const Dbdict::OpInfo
Dbdict::CreateNodegroupRec::g_opInfo = {
  { 'C', 'N', 'G', 0 },
  ~RT_DBDICT_CREATE_NODEGROUP,
  GSN_CREATE_NODEGROUP_IMPL_REQ,
  CreateNodegroupImplReq::SignalLength,
  //
  &Dbdict::createNodegroup_seize,
  &Dbdict::createNodegroup_release,
  //
  &Dbdict::createNodegroup_parse,
  &Dbdict::createNodegroup_subOps,
  &Dbdict::createNodegroup_reply,
  //
  &Dbdict::createNodegroup_prepare,
  &Dbdict::createNodegroup_commit,
  &Dbdict::createNodegroup_complete,
  //
  &Dbdict::createNodegroup_abortParse,
  &Dbdict::createNodegroup_abortPrepare
};

void
Dbdict::execCREATE_NODEGROUP_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CreateNodegroupReq req_copy =
    *(const CreateNodegroupReq*)signal->getDataPtr();
  const CreateNodegroupReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateNodegroupRecPtr createNodegroupRecPtr;
    CreateNodegroupImplReq* impl_req;

    startClientReq(op_ptr, createNodegroupRecPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->nodegroupId = req->nodegroupId;
    for (Uint32 i = 0; i<NDB_ARRAY_SIZE(req->nodes) &&
           i<NDB_ARRAY_SIZE(impl_req->nodes); i++)
    {
      impl_req->nodes[i] = req->nodes[i];
    }

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateNodegroupRef* ref = (CreateNodegroupRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_REF, signal,
	     CreateNodegroupRef::SignalLength, JBB);
}

bool
Dbdict::createNodegroup_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateNodegroupRec>(op_ptr);
}

void
Dbdict::createNodegroup_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateNodegroupRec>(op_ptr);
}

// CreateNodegroup: PARSE

void
Dbdict::createNodegroup_parse(Signal* signal, bool master,
                              SchemaOpPtr op_ptr,
                              SectionHandle& handle, ErrorInfo& error)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  jam();

  Uint32 save = impl_req->requestType;
  impl_req->requestType = CreateNodegroupImplReq::RT_PARSE;
  memcpy(signal->theData, impl_req, 4*CreateNodegroupImplReq::SignalLength);
  impl_req->requestType = save;

  EXECUTE_DIRECT(DBDIH, GSN_CREATE_NODEGROUP_IMPL_REQ, signal,
                 CreateNodegroupImplReq::SignalLength);
  jamEntry();

  Uint32 ret = signal->theData[0];
  if (ret)
  {
    jam();
    setError(error, ret, __LINE__);
    return ;
  }

  impl_req->senderRef = reference();
  impl_req->senderData = op_ptr.p->op_key;
  impl_req->nodegroupId = signal->theData[1];

  /**
   * createNodegroup blocks gcp
   *   so trans_ptr can *not* do this (endless loop)
   */
  trans_ptr.p->m_wait_gcp_on_commit = false;
}

void
Dbdict::createNodegroup_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

bool
Dbdict::createNodegroup_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  //CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  if (createNodegroupRecPtr.p->m_map_created == false)
  {
    jam();
    createNodegroupRecPtr.p->m_map_created = true;

    /**
     * This is a bit cheating...
     *   it would be better to handle "object-exists"
     *   and still continue transaction
     *   but that i dont know how
     */
    Uint32 buckets = NDB_DEFAULT_HASHMAP_BUCKETS;
    Uint32 fragments = get_default_fragments(signal, 1);
    char buf[MAX_TAB_NAME_SIZE+1];
    BaseString::snprintf(buf, sizeof(buf), "DEFAULT-HASHMAP-%u-%u",
                         buckets,
                         fragments);

    if (get_object(buf) != 0)
    {
      jam();
      return false;
    }


    Callback c = {
      safe_cast(&Dbdict::createNodegroup_fromCreateHashMap),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    CreateHashMapReq* const req = (CreateHashMapReq*)signal->getDataPtrSend();
    req->clientRef = reference();
    req->clientData = op_ptr.p->op_key;
    req->requestInfo = 0;
    req->transId = trans_ptr.p->m_transId;
    req->transKey = trans_ptr.p->trans_key;
    req->buckets = buckets;
    req->fragments = fragments;
    sendSignal(DBDICT_REF, GSN_CREATE_HASH_MAP_REQ, signal,
               CreateHashMapReq::SignalLength, JBB);
    return true;
  }

  return false;
}

void
Dbdict::createNodegroup_fromCreateHashMap(Signal* signal,
                                          Uint32 op_key,
                                          Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  findSchemaOp(op_ptr, createNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (ret == 0)
  {
    jam();
    createSubOps(signal, op_ptr);
  }
  else
  {
    jam();
    const CreateHashMapRef* ref = (const CreateHashMapRef*)signal->getDataPtr();

    ErrorInfo error;
    setError(error, ref);
    abortSubOps(signal, op_ptr, error);
  }
}

void
Dbdict::createNodegroup_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    CreateNodegroupConf* conf = (CreateNodegroupConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->nodegroupId = impl_req->nodegroupId;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_NODEGROUP_CONF, signal,
               CreateNodegroupConf::SignalLength, JBB);
  }
  else
  {
    jam();
    CreateNodegroupRef* ref = (CreateNodegroupRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_NODEGROUP_REF, signal,
               CreateNodegroupRef::SignalLength, JBB);
  }
}

// CreateNodegroup: PREPARE

void
Dbdict::createNodegroup_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  impl_req->requestType = CreateNodegroupImplReq::RT_PREPARE;
  createNodegroupRecPtr.p->m_blockCnt = 2;
  createNodegroupRecPtr.p->m_blockIndex = 0;
  createNodegroupRecPtr.p->m_blockNo[0] = DBDIH_REF;
  createNodegroupRecPtr.p->m_blockNo[1] = SUMA_REF;

  Callback c = {
    safe_cast(&Dbdict::createNodegroup_fromBlockSubStartStop),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  block_substartstop(signal, op_ptr);
}

void
Dbdict::createNodegroup_fromBlockSubStartStop(Signal* signal,
                                              Uint32 op_key,
                                              Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  findSchemaOp(op_ptr, createNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  //CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  if (ret == 0)
  {
    jam();
    createNodegroupRecPtr.p->m_substartstop_blocked = true;
    createNodegroup_toLocal(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::createNodegroup_toLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  const Uint32 blockIndex = createNodegroupRecPtr.p->m_blockIndex;
  if (blockIndex == createNodegroupRecPtr.p->m_blockCnt)
  {
    jam();

    if (op_ptr.p->m_state == SchemaOp::OS_PREPARING)
    {
      jam();

      /**
       * Block GCP as last part of prepare
       */
      Callback c = {
        safe_cast(&Dbdict::createNodegroup_fromWaitGCP),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      createNodegroupRecPtr.p->m_cnt_waitGCP = 0;
      createNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::BlockStartGcp;
      wait_gcp(signal, op_ptr, WaitGCPReq::BlockStartGcp);
      return;
    }

    if (op_ptr.p->m_state == SchemaOp::OS_COMPLETING)
    {
      jam();
      /**
       * Unblock GCP as last step of complete
       */
      Callback c = {
        safe_cast(&Dbdict::createNodegroup_fromWaitGCP),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      createNodegroupRecPtr.p->m_cnt_waitGCP = 0;
      createNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::UnblockStartGcp;
      wait_gcp(signal, op_ptr, WaitGCPReq::UnblockStartGcp);
      return;
    }

    sendTransConf(signal, op_ptr);
    return;
  }

  Callback c = {
    safe_cast(&Dbdict::createNodegroup_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  Uint32 ref = createNodegroupRecPtr.p->m_blockNo[blockIndex];
  CreateNodegroupImplReq * req = (CreateNodegroupImplReq*)signal->getDataPtrSend();
  memcpy(req, impl_req, 4*CreateNodegroupImplReq::SignalLength);
  sendSignal(ref, GSN_CREATE_NODEGROUP_IMPL_REQ, signal,
             CreateNodegroupImplReq::SignalLength, JBB);
}

void
Dbdict::createNodegroup_fromLocal(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  findSchemaOp(op_ptr, createNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  Uint32 blockIndex = createNodegroupRecPtr.p->m_blockIndex;
  ndbrequire(blockIndex < createNodegroupRecPtr.p->m_blockCnt);
  if (ret)
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  Uint32 idx = createNodegroupRecPtr.p->m_blockIndex;
  if (op_ptr.p->m_state == SchemaOp::OS_COMPLETING &&
      createNodegroupRecPtr.p->m_blockNo[idx] == DBDIH_REF)
  {
    jam();

    CreateNodegroupImplConf * conf =
      (CreateNodegroupImplConf*)signal->getDataPtr();
    impl_req->gci_hi = conf->gci_hi;
    impl_req->gci_lo = conf->gci_lo;
  }

  createNodegroupRecPtr.p->m_blockIndex++;
  createNodegroup_toLocal(signal, op_ptr);
}

void
Dbdict::createNodegroup_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  //CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  if (createNodegroupRecPtr.p->m_substartstop_blocked)
  {
    jam();
    unblock_substartstop();
  }

  if (createNodegroupRecPtr.p->m_gcp_blocked)
  {
    jam();

    Callback c = {
      safe_cast(&Dbdict::createNodegroup_fromWaitGCP),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    createNodegroupRecPtr.p->m_cnt_waitGCP = 0;
    createNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::UnblockStartGcp;
    wait_gcp(signal, op_ptr, WaitGCPReq::UnblockStartGcp);
    return;
  }

  sendTransConf(signal, op_ptr);
}

// CreateNodegroup: COMMIT

void
Dbdict::createNodegroup_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  impl_req->requestType = CreateNodegroupImplReq::RT_COMMIT;
  createNodegroupRecPtr.p->m_blockCnt = 2;
  createNodegroupRecPtr.p->m_blockIndex = 0;
  createNodegroupRecPtr.p->m_blockNo[0] = DBDIH_REF;
  createNodegroupRecPtr.p->m_blockNo[1] = NDBCNTR_REF;

  Callback c = {
    safe_cast(&Dbdict::createNodegroup_fromWaitGCP),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  createNodegroupRecPtr.p->m_cnt_waitGCP = 0;
  createNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::WaitEpoch;
  wait_gcp(signal, op_ptr, WaitGCPReq::WaitEpoch);
}

void
Dbdict::createNodegroup_fromWaitGCP(Signal* signal,
                                    Uint32 op_key,
                                    Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  findSchemaOp(op_ptr, createNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  if (ret == 0)
  {
    jam();

    Uint32 wait_type = createNodegroupRecPtr.p->m_wait_gcp_type;
    if (op_ptr.p->m_state == SchemaOp::OS_ABORTED_PREPARE)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::UnblockStartGcp);
      sendTransConf(signal, op_ptr);
      return;
    }
    else if (op_ptr.p->m_state == SchemaOp::OS_PREPARING)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::BlockStartGcp);
      createNodegroupRecPtr.p->m_gcp_blocked = true;
      sendTransConf(signal, op_ptr);
      return;
    }
    else if (op_ptr.p->m_state == SchemaOp::OS_COMMITTING)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::WaitEpoch);
      createNodegroup_toLocal(signal, op_ptr);
      return;
    }
    else
    {
      jam();
      ndbrequire(op_ptr.p->m_state == SchemaOp::OS_COMPLETING);

      if (wait_type == WaitGCPReq::UnblockStartGcp)
      {
        jam();
        wait_type = WaitGCPReq::CompleteForceStart;
        createNodegroupRecPtr.p->m_wait_gcp_type = wait_type;
        goto retry;
      }
      else
      {
        jam();
        ndbrequire(wait_type == WaitGCPReq::CompleteForceStart);
        unblock_substartstop();
      }

      sendTransConf(signal, op_ptr);
      return;
    }
  }

  createNodegroupRecPtr.p->m_cnt_waitGCP++;
  switch(ret){
  case WaitGCPRef::NoWaitGCPRecords:
    jam();
  case WaitGCPRef::NF_CausedAbortOfProcedure:
    jam();
  case WaitGCPRef::NF_MasterTakeOverInProgress:
    jam();
  }

retry:
  wait_gcp(signal, op_ptr, createNodegroupRecPtr.p->m_wait_gcp_type);
}

// CreateNodegroup: COMPLETE

void
Dbdict::createNodegroup_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateNodegroupRecPtr createNodegroupRecPtr;
  getOpRec(op_ptr, createNodegroupRecPtr);
  CreateNodegroupImplReq* impl_req = &createNodegroupRecPtr.p->m_request;

  impl_req->requestType = CreateNodegroupImplReq::RT_COMPLETE;
  createNodegroupRecPtr.p->m_blockCnt = 2;
  createNodegroupRecPtr.p->m_blockIndex = 0;
  createNodegroupRecPtr.p->m_blockNo[0] = DBDIH_REF;
  createNodegroupRecPtr.p->m_blockNo[1] = SUMA_REF;

  createNodegroup_toLocal(signal, op_ptr);
}

void
Dbdict::execCREATE_NODEGROUP_IMPL_REF(Signal* signal)
{
  jamEntry();
  CreateNodegroupImplRef * ref = (CreateNodegroupImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCREATE_NODEGROUP_IMPL_CONF(Signal* signal)
{
  jamEntry();
  CreateNodegroupImplConf * conf = (CreateNodegroupImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execCREATE_HASH_MAP_REF(Signal* signal)
{
  jamEntry();
  CreateHashMapRef * ref = (CreateHashMapRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execCREATE_HASH_MAP_CONF(Signal* signal)
{
  jamEntry();
  CreateHashMapConf * conf = (CreateHashMapConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

// CreateNodegroup: END

// MODULE: DropNodegroup

const Dbdict::OpInfo
Dbdict::DropNodegroupRec::g_opInfo = {
  { 'D', 'N', 'G', 0 },
  ~RT_DBDICT_DROP_NODEGROUP,
  GSN_DROP_NODEGROUP_IMPL_REQ,
  DropNodegroupImplReq::SignalLength,
  //
  &Dbdict::dropNodegroup_seize,
  &Dbdict::dropNodegroup_release,
  //
  &Dbdict::dropNodegroup_parse,
  &Dbdict::dropNodegroup_subOps,
  &Dbdict::dropNodegroup_reply,
  //
  &Dbdict::dropNodegroup_prepare,
  &Dbdict::dropNodegroup_commit,
  &Dbdict::dropNodegroup_complete,
  //
  &Dbdict::dropNodegroup_abortParse,
  &Dbdict::dropNodegroup_abortPrepare
};

void
Dbdict::execDROP_NODEGROUP_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const DropNodegroupReq req_copy =
    *(const DropNodegroupReq*)signal->getDataPtr();
  const DropNodegroupReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    DropNodegroupRecPtr dropNodegroupRecPtr;
    DropNodegroupImplReq* impl_req;

    startClientReq(op_ptr, dropNodegroupRecPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->nodegroupId = req->nodegroupId;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  DropNodegroupRef* ref = (DropNodegroupRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transId = req->transId;
  ref->senderData = req->senderData;
  getError(error, ref);

  sendSignal(req->senderRef, GSN_DROP_NODEGROUP_REF, signal,
	     DropNodegroupRef::SignalLength, JBB);
}

bool
Dbdict::dropNodegroup_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<DropNodegroupRec>(op_ptr);
}

void
Dbdict::dropNodegroup_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<DropNodegroupRec>(op_ptr);
}

// DropNodegroup: PARSE

void
Dbdict::dropNodegroup_parse(Signal* signal, bool master,
                          SchemaOpPtr op_ptr,
                          SectionHandle& handle, ErrorInfo& error)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  impl_req->senderRef = reference();
  impl_req->senderData = op_ptr.p->op_key;

  /**
   * dropNodegroup blocks gcp
   *   so trans_ptr can *not* do this (endless loop)
   */
  trans_ptr.p->m_wait_gcp_on_commit = false;
}

void
Dbdict::dropNodegroup_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

bool
Dbdict::dropNodegroup_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::dropNodegroup_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  //DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  if (!hasError(error))
  {
    jam();
    DropNodegroupConf* conf = (DropNodegroupConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_NODEGROUP_CONF, signal,
               DropNodegroupConf::SignalLength, JBB);
  }
  else
  {
    jam();
    DropNodegroupRef* ref = (DropNodegroupRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_DROP_NODEGROUP_REF, signal,
               DropNodegroupRef::SignalLength, JBB);
  }
}

// DropNodegroup: PREPARE

void
Dbdict::dropNodegroup_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  impl_req->requestType = DropNodegroupImplReq::RT_PREPARE;
  dropNodegroupRecPtr.p->m_blockCnt = 2;
  dropNodegroupRecPtr.p->m_blockIndex = 0;
  dropNodegroupRecPtr.p->m_blockNo[0] = DBDIH_REF;
  dropNodegroupRecPtr.p->m_blockNo[1] = SUMA_REF;

  Callback c = {
    safe_cast(&Dbdict::dropNodegroup_fromBlockSubStartStop),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  block_substartstop(signal, op_ptr);
}

void
Dbdict::dropNodegroup_fromBlockSubStartStop(Signal* signal,
                                              Uint32 op_key,
                                              Uint32 ret)
{
  SchemaOpPtr op_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  findSchemaOp(op_ptr, dropNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  //DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  if (ret == 0)
  {
    jam();
    dropNodegroupRecPtr.p->m_substartstop_blocked = true;
    dropNodegroup_toLocal(signal, op_ptr);
  }
  else
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::dropNodegroup_toLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  const Uint32 blockIndex = dropNodegroupRecPtr.p->m_blockIndex;
  if (blockIndex == dropNodegroupRecPtr.p->m_blockCnt)
  {
    jam();

    if (op_ptr.p->m_state == SchemaOp::OS_PREPARING)
    {
      jam();

      /**
       * Block GCP as last part of prepare
       */
      Callback c = {
        safe_cast(&Dbdict::dropNodegroup_fromWaitGCP),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      dropNodegroupRecPtr.p->m_cnt_waitGCP = 0;
      dropNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::BlockStartGcp;
      wait_gcp(signal, op_ptr, WaitGCPReq::BlockStartGcp);
      return;
    }

    if (op_ptr.p->m_state == SchemaOp::OS_COMPLETING)
    {
      jam();
      /**
       * Unblock GCP as last step of complete
       */
      Callback c = {
        safe_cast(&Dbdict::dropNodegroup_fromWaitGCP),
        op_ptr.p->op_key
      };
      op_ptr.p->m_callback = c;

      dropNodegroupRecPtr.p->m_cnt_waitGCP = 0;
      dropNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::UnblockStartGcp;
      wait_gcp(signal, op_ptr, WaitGCPReq::UnblockStartGcp);
      return;
    }

    sendTransConf(signal, op_ptr);
    return;
  }

  Callback c = {
    safe_cast(&Dbdict::dropNodegroup_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  Uint32 ref = dropNodegroupRecPtr.p->m_blockNo[blockIndex];
  DropNodegroupImplReq * req = (DropNodegroupImplReq*)signal->getDataPtrSend();
  memcpy(req, impl_req, 4*DropNodegroupImplReq::SignalLength);
  sendSignal(ref, GSN_DROP_NODEGROUP_IMPL_REQ, signal,
             DropNodegroupImplReq::SignalLength, JBB);
}

void
Dbdict::dropNodegroup_fromLocal(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  SchemaOpPtr op_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  findSchemaOp(op_ptr, dropNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  Uint32 blockIndex = dropNodegroupRecPtr.p->m_blockIndex;
  ndbrequire(blockIndex < dropNodegroupRecPtr.p->m_blockCnt);

  if (ret)
  {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
    return;
  }

  Uint32 idx = dropNodegroupRecPtr.p->m_blockIndex;
  if (op_ptr.p->m_state == SchemaOp::OS_COMMITTING &&
      dropNodegroupRecPtr.p->m_blockNo[idx] == DBDIH_REF)
  {
    jam();

    DropNodegroupImplConf * conf =
      (DropNodegroupImplConf*)signal->getDataPtr();
    impl_req->gci_hi = conf->gci_hi;
    impl_req->gci_lo = conf->gci_lo;
  }


  dropNodegroupRecPtr.p->m_blockIndex++;
  dropNodegroup_toLocal(signal, op_ptr);
}


void
Dbdict::dropNodegroup_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  //DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  if (dropNodegroupRecPtr.p->m_substartstop_blocked)
  {
    jam();
    unblock_substartstop();
  }

  if (dropNodegroupRecPtr.p->m_gcp_blocked)
  {
    jam();

    Callback c = {
      safe_cast(&Dbdict::dropNodegroup_fromWaitGCP),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;

    dropNodegroupRecPtr.p->m_cnt_waitGCP = 0;
    dropNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::UnblockStartGcp;
    wait_gcp(signal, op_ptr, WaitGCPReq::UnblockStartGcp);
    return;
  }

  sendTransConf(signal, op_ptr);
}

// DropNodegroup: COMMIT

void
Dbdict::dropNodegroup_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  impl_req->requestType = DropNodegroupImplReq::RT_COMMIT;

  dropNodegroupRecPtr.p->m_blockIndex = 0;
  dropNodegroupRecPtr.p->m_blockCnt = 1;
  dropNodegroupRecPtr.p->m_blockNo[0] = DBDIH_REF;

  Callback c = {
    safe_cast(&Dbdict::dropNodegroup_fromWaitGCP),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  dropNodegroupRecPtr.p->m_cnt_waitGCP = 0;
  dropNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::WaitEpoch;
  wait_gcp(signal, op_ptr, WaitGCPReq::WaitEpoch);
}

void
Dbdict::dropNodegroup_fromWaitGCP(Signal* signal,
                                  Uint32 op_key,
                                  Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  findSchemaOp(op_ptr, dropNodegroupRecPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  //DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  if (ret == 0)
  {
    jam();

    Uint32 wait_type = dropNodegroupRecPtr.p->m_wait_gcp_type;
    if (op_ptr.p->m_state == SchemaOp::OS_ABORTING_PREPARE)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::UnblockStartGcp);
      sendTransConf(signal, op_ptr);
      return;
    }
    else if (op_ptr.p->m_state == SchemaOp::OS_PREPARING)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::BlockStartGcp);
      dropNodegroupRecPtr.p->m_gcp_blocked = true;
      sendTransConf(signal, op_ptr);
      return;
    }
    else if (op_ptr.p->m_state == SchemaOp::OS_COMMITTING)
    {
      jam();
      ndbrequire(wait_type == WaitGCPReq::WaitEpoch);
      dropNodegroup_toLocal(signal, op_ptr);
      return;
    }
    else
    {
      jam();

      ndbrequire(op_ptr.p->m_state == SchemaOp::OS_COMPLETING);
      if (wait_type == WaitGCPReq::UnblockStartGcp)
      {
        /**
         * Also wait for the epoch to complete
         */
        jam();
        dropNodegroupRecPtr.p->m_wait_gcp_type = WaitGCPReq::CompleteForceStart;
        goto retry;
      }
      else
      {
        jam();
        ndbrequire(wait_type == WaitGCPReq::CompleteForceStart);
        unblock_substartstop();
      }
      sendTransConf(signal, op_ptr);
      return;
    }
  }

  dropNodegroupRecPtr.p->m_cnt_waitGCP++;
  switch(ret){
  case WaitGCPRef::NoWaitGCPRecords:
    jam();
  case WaitGCPRef::NF_CausedAbortOfProcedure:
    jam();
  case WaitGCPRef::NF_MasterTakeOverInProgress:
    jam();
  }

retry:
  wait_gcp(signal, op_ptr, dropNodegroupRecPtr.p->m_wait_gcp_type);
}

// DropNodegroup: COMPLETE

void
Dbdict::dropNodegroup_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropNodegroupRecPtr dropNodegroupRecPtr;
  getOpRec(op_ptr, dropNodegroupRecPtr);
  DropNodegroupImplReq* impl_req = &dropNodegroupRecPtr.p->m_request;

  impl_req->requestType = DropNodegroupImplReq::RT_COMPLETE;
  dropNodegroupRecPtr.p->m_blockIndex = 0;
  dropNodegroupRecPtr.p->m_blockCnt = 3;
  dropNodegroupRecPtr.p->m_blockNo[0] = SUMA_REF;
  dropNodegroupRecPtr.p->m_blockNo[1] = DBDIH_REF;
  dropNodegroupRecPtr.p->m_blockNo[2] = NDBCNTR_REF;

  dropNodegroup_toLocal(signal, op_ptr);
}

void
Dbdict::execDROP_NODEGROUP_IMPL_REF(Signal* signal)
{
  jamEntry();
  DropNodegroupImplRef * ref = (DropNodegroupImplRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

void
Dbdict::execDROP_NODEGROUP_IMPL_CONF(Signal* signal)
{
  jamEntry();
  DropNodegroupImplConf * conf = (DropNodegroupImplConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

// DropNodegroup: END

/*
  return 1 if all of the below is true
  a) node in single user mode
  b) senderRef is not a db node
  c) senderRef nodeid is not the singleUserApi
*/
int Dbdict::checkSingleUserMode(Uint32 senderRef)
{
  Uint32 nodeId = refToNode(senderRef);
  return
    getNodeState().getSingleUserMode() &&
    (getNodeInfo(nodeId).m_type != NodeInfo::DB) &&
    (nodeId != getNodeState().getSingleUserApi());
}

// MODULE: SchemaTrans

// ErrorInfo

void
Dbdict::setError(ErrorInfo& e,
                 Uint32 code,
                 Uint32 line,
                 Uint32 nodeId,
                 Uint32 status,
                 Uint32 key,
                 const char * name)
{
  D("setError" << V(code) << V(line) << V(nodeId) << V(e.errorCount));

  // can only store details for first error
  if (e.errorCount == 0) {
    e.errorCode = code;
    e.errorLine = line;
    e.errorNodeId = nodeId ? nodeId : getOwnNodeId();
    e.errorStatus = status;
    e.errorKey = key;
    BaseString::snprintf(e.errorObjectName, sizeof(e.errorObjectName), "%s",
                         name ? name : "");
  }
  e.errorCount++;
}

void
Dbdict::setError(ErrorInfo& e,
                 Uint32 code,
                 Uint32 line,
                 const char * name)
{
  setError(e, code, line, 0, 0, 0, name);
}

void
Dbdict::setError(ErrorInfo& e, const ErrorInfo& e2)
{
  setError(e, e2.errorCode, e2.errorLine, e2.errorNodeId);
}

void
Dbdict::setError(ErrorInfo& e, const ParseDictTabInfoRecord& e2)
{
  setError(e, e2.errorCode, e2.errorLine, 0, e2.status, e2.errorKey);
}

void
Dbdict::setError(SchemaOpPtr op_ptr, Uint32 code, Uint32 line, Uint32 nodeId)
{
  D("setError" << *op_ptr.p << V(code) << V(line) << V(nodeId));
  setError(op_ptr.p->m_error, code, line, nodeId);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  // wl3600_todo remove the check
  if (!trans_ptr.isNull()) {
    setError(trans_ptr.p->m_error, op_ptr.p->m_error);
  }
}

void
Dbdict::setError(SchemaOpPtr op_ptr, const ErrorInfo& e2)
{
  setError(op_ptr, e2.errorCode, e2.errorLine, e2.errorNodeId);
}

void
Dbdict::setError(SchemaTransPtr trans_ptr, Uint32 code, Uint32 line, Uint32 nodeId)
{
  D("setError" << *trans_ptr.p << V(code) << V(line) << V(nodeId));
  setError(trans_ptr.p->m_error, code, line, nodeId);
}

void
Dbdict::setError(SchemaTransPtr trans_ptr, const ErrorInfo& e2)
{
  setError(trans_ptr, e2.errorCode, e2.errorLine, e2.errorNodeId);
}

void
Dbdict::setError(TxHandlePtr tx_ptr, Uint32 code, Uint32 line, Uint32 nodeId)
{
  D("setError" << *tx_ptr.p << V(code) << V(line) << V(nodeId));
  setError(tx_ptr.p->m_error, code, line, nodeId);
}

void
Dbdict::setError(TxHandlePtr tx_ptr, const ErrorInfo& e2)
{
  setError(tx_ptr, e2.errorCode, e2.errorLine, e2.errorNodeId);
}

bool
Dbdict::hasError(const ErrorInfo& e)
{
  return e.errorCount != 0;
}

void
Dbdict::resetError(ErrorInfo& e)
{
  new (&e) ErrorInfo();
}

void
Dbdict::resetError(SchemaOpPtr op_ptr)
{
  if (hasError(op_ptr.p->m_error)) {
    jam();
    D("resetError" << *op_ptr.p);
    resetError(op_ptr.p->m_error);
  }
}

void
Dbdict::resetError(SchemaTransPtr trans_ptr)
{
  if (hasError(trans_ptr.p->m_error)) {
    jam();
    D("resetError" << *trans_ptr.p);
    resetError(trans_ptr.p->m_error);
  }
}

void
Dbdict::resetError(TxHandlePtr tx_ptr)
{
  if (hasError(tx_ptr.p->m_error)) {
    jam();
    D("resetError" << *tx_ptr.p);
    resetError(tx_ptr.p->m_error);
  }
}

// OpInfo

const Dbdict::OpInfo*
Dbdict::g_opInfoList[] = {
  &Dbdict::CreateTableRec::g_opInfo,
  &Dbdict::DropTableRec::g_opInfo,
  &Dbdict::AlterTableRec::g_opInfo,
  &Dbdict::CreateTriggerRec::g_opInfo,
  &Dbdict::DropTriggerRec::g_opInfo,
  &Dbdict::CreateIndexRec::g_opInfo,
  &Dbdict::DropIndexRec::g_opInfo,
  &Dbdict::AlterIndexRec::g_opInfo,
  &Dbdict::BuildIndexRec::g_opInfo,
  &Dbdict::IndexStatRec::g_opInfo,
  &Dbdict::CreateFilegroupRec::g_opInfo,
  &Dbdict::CreateFileRec::g_opInfo,
  &Dbdict::DropFilegroupRec::g_opInfo,
  &Dbdict::DropFileRec::g_opInfo,
  &Dbdict::CreateHashMapRec::g_opInfo,
  &Dbdict::CopyDataRec::g_opInfo,
  &Dbdict::CreateNodegroupRec::g_opInfo,
  &Dbdict::DropNodegroupRec::g_opInfo,
  0
};

const Dbdict::OpInfo*
Dbdict::findOpInfo(Uint32 gsn)
{
  Uint32 i = 0;
  while (g_opInfoList[i]) {
    const OpInfo* info = g_opInfoList[i];
    if (info->m_impl_req_gsn == 0)
      break;
    if (info->m_impl_req_gsn == gsn)
      return info;
    i++;
  }
  ndbrequire(false);
  return 0;
}

// OpRec

// OpSection

bool
Dbdict::copyIn(OpSectionBufferPool& pool, OpSection& op_sec, const SegmentedSectionPtr& ss_ptr)
{
  const Uint32 size = 1024;
  Uint32 buf[size];

  SegmentedSectionPtr tmp = ss_ptr;
  SectionReader reader(tmp, getSectionSegmentPool());
  Uint32 len = ss_ptr.sz;
  while (len > size)
  {
    jam();
    ndbrequire(reader.getWords(buf, size));
    if (!copyIn(pool, op_sec, buf, size))
    {
      jam();
      return false;
    }
    len -= size;
  }

  ndbrequire(reader.getWords(buf, len));
  if (!copyIn(pool, op_sec, buf, len))
  {
    jam();
    return false;
  }

  return true;
}

bool
Dbdict::copyIn(OpSectionBufferPool& pool, OpSection& op_sec, const Uint32* src, Uint32 srcSize)
{
  OpSectionBuffer buffer(pool, op_sec.m_head);
  if (!buffer.append(src, srcSize)) {
    jam();
    return false;
  }
  return true;
}

bool
Dbdict::copyOut(Dbdict::OpSectionBuffer & buffer,
                Dbdict::OpSectionBufferConstIterator & iter,
                Uint32 * dst,
                Uint32 len)
{
  Uint32 n = 0;
  for(; !iter.isNull() && n < len; buffer.next(iter))
  {
    dst[n] = *iter.data;
    n++;
  }

  return n == len;
}

bool
Dbdict::copyOut(OpSectionBufferPool& pool, const OpSection& op_sec, SegmentedSectionPtr& ss_ptr)
{
  const Uint32 size = 1024;
  Uint32 buf[size];

  Uint32 len = op_sec.getSize();
  OpSectionBufferHead tmp_head = op_sec.m_head;

  OpSectionBuffer buffer(pool, tmp_head);

  OpSectionBufferConstIterator iter;
  buffer.first(iter);
  Uint32 ptrI = RNIL;
  while (len > size)
  {
    if (!copyOut(buffer, iter, buf, size))
    {
      jam();
      goto fail;
    }
    if (!appendToSection(ptrI, buf, size))
    {
      jam();
      goto fail;
    }
    len -= size;
  }

  if (!copyOut(buffer, iter, buf, len))
  {
    jam();
    goto fail;
  }

  if (!appendToSection(ptrI, buf, len))
  {
    jam();
    goto fail;
  }

  getSection(ss_ptr, ptrI);
  return true;

fail:
  releaseSection(ptrI);
  return false;
}

bool
Dbdict::copyOut(OpSectionBufferPool& pool, const OpSection& op_sec, Uint32* dst, Uint32 dstSize)
{
  if (op_sec.getSize() > dstSize) {
    jam();
    return false;
  }

  // there is no const version of LocalDataBuffer
  OpSectionBufferHead tmp_head = op_sec.m_head;
  OpSectionBuffer buffer(pool, tmp_head);

  OpSectionBufferConstIterator iter;
  Uint32 n = 0;
  for(buffer.first(iter); !iter.isNull(); buffer.next(iter)) {
    jam();
    dst[n] = *iter.data;
    n++;
  }
  ndbrequire(n == op_sec.getSize());
  return true;
}

void
Dbdict::release(OpSectionBufferPool& pool, OpSection& op_sec)
{
  OpSectionBuffer buffer(pool, op_sec.m_head);
  buffer.release();
}

// SchemaOp

const Dbdict::OpInfo&
Dbdict::getOpInfo(SchemaOpPtr op_ptr)
{
  ndbrequire(!op_ptr.isNull());
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(!oprec_ptr.isNull());
  return oprec_ptr.p->m_opInfo;
}

bool
Dbdict::seizeSchemaOp(SchemaTransPtr trans_ptr, SchemaOpPtr& op_ptr, Uint32 op_key, const OpInfo& info, bool linked)
{
  if ((ERROR_INSERTED(6111) &&
       (info.m_impl_req_gsn == GSN_CREATE_TAB_REQ ||
        info.m_impl_req_gsn == GSN_DROP_TAB_REQ ||
        info.m_impl_req_gsn == GSN_ALTER_TAB_REQ)) ||
      (ERROR_INSERTED(6112) &&
       (info.m_impl_req_gsn == GSN_CREATE_INDX_IMPL_REQ ||
        info.m_impl_req_gsn == GSN_DROP_INDX_IMPL_REQ)) ||
      (ERROR_INSERTED(6113) &&
       (info.m_impl_req_gsn == GSN_ALTER_INDX_IMPL_REQ)) ||
      (ERROR_INSERTED(6114) &&
       (info.m_impl_req_gsn == GSN_CREATE_TRIG_IMPL_REQ ||
        info.m_impl_req_gsn == GSN_DROP_TRIG_IMPL_REQ)) ||
      (ERROR_INSERTED(6116) &&
       (info.m_impl_req_gsn == GSN_BUILD_INDX_IMPL_REQ)))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    op_ptr.setNull();
    return false;
  }

  if (!findSchemaOp(op_ptr, op_key)) {
    jam();
    if (c_schemaOpPool.seize(trans_ptr.p->m_arena, op_ptr)) {
      jam();
      new (op_ptr.p) SchemaOp();
      op_ptr.p->op_key = op_key;
      op_ptr.p->m_trans_ptr = trans_ptr;
      if ((this->*(info.m_seize))(op_ptr)) {
        jam();

        if(!linked) {
          jam();
          addSchemaOp(op_ptr);
        }

        c_schemaOpHash.add(op_ptr);
        D("seizeSchemaOp" << V(op_key) << V(info.m_opType));
        return true;
      }
      c_schemaOpPool.release(op_ptr);
    }
  }
  op_ptr.setNull();
  return false;
}

bool
Dbdict::findSchemaOp(SchemaOpPtr& op_ptr, Uint32 op_key)
{
  SchemaOp op_rec(op_key);
  if (c_schemaOpHash.find(op_ptr, op_rec)) {
    jam();
    const OpRecPtr& oprec_ptr = op_ptr.p->m_oprec_ptr;
    ndbrequire(!oprec_ptr.isNull());
    ndbrequire(op_ptr.p->m_magic == SchemaOp::DICT_MAGIC);
    D("findSchemaOp" << V(op_key));
    return true;
  }
  return false;
}

void
Dbdict::releaseSchemaOp(SchemaOpPtr& op_ptr)
{
  D("releaseSchemaOp" << V(op_ptr.p->op_key));

  const OpInfo& info = getOpInfo(op_ptr);
  (this->*(info.m_release))(op_ptr);

  while (op_ptr.p->m_sections != 0) {
    jam();
    releaseOpSection(op_ptr, op_ptr.p->m_sections - 1);
  }

  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  Uint32 obj_ptr_i = oprec_ptr.p->m_obj_ptr_i;
  if (obj_ptr_i != RNIL) {
    jam();
    unlinkDictObject(op_ptr);
  }

  if (!op_ptr.p->m_oplnk_ptr.isNull()) {
    jam();
    releaseSchemaOp(op_ptr.p->m_oplnk_ptr);
  }

  ndbrequire(op_ptr.p->m_magic == SchemaOp::DICT_MAGIC);
  c_schemaOpHash.remove(op_ptr);
  c_schemaOpPool.release(op_ptr);
  op_ptr.setNull();
}

// save signal sections

const Dbdict::OpSection&
Dbdict::getOpSection(SchemaOpPtr op_ptr, Uint32 ss_no)
{
  ndbrequire(ss_no < op_ptr.p->m_sections);
  return op_ptr.p->m_section[ss_no];
}

bool
Dbdict::saveOpSection(SchemaOpPtr op_ptr,
                      SectionHandle& handle, Uint32 ss_no)
{
  SegmentedSectionPtr ss_ptr;
  bool ok = handle.getSection(ss_ptr, ss_no);
  ndbrequire(ok);
  return saveOpSection(op_ptr, ss_ptr, ss_no);
}

bool
Dbdict::saveOpSection(SchemaOpPtr op_ptr,
                      SegmentedSectionPtr ss_ptr, Uint32 ss_no)
{
  ndbrequire(ss_no <= 2 && op_ptr.p->m_sections == ss_no);
  OpSection& op_sec = op_ptr.p->m_section[ss_no];
  op_ptr.p->m_sections++;

  LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
  bool ok =  copyIn(op_sec_pool, op_sec, ss_ptr);
  ndbrequire(ok);
  return true;
}

void
Dbdict::releaseOpSection(SchemaOpPtr op_ptr, Uint32 ss_no)
{
  ndbrequire(ss_no + 1 == op_ptr.p->m_sections);
  OpSection& op_sec = op_ptr.p->m_section[ss_no];
  LocalArenaPoolImpl op_sec_pool(op_ptr.p->m_trans_ptr.p->m_arena, c_opSectionBufferPool);
  release(op_sec_pool, op_sec);
  op_ptr.p->m_sections = ss_no;
}

// add schema op to trans during parse phase

void
Dbdict::addSchemaOp(SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
  list.addLast(op_ptr);

  // jonas_todo REMOVE side effect
  // add global flags from trans
  const Uint32& src_info = trans_ptr.p->m_requestInfo;
  DictSignal::addRequestFlagsGlobal(op_ptr.p->m_requestInfo, src_info);
}

// update op step after successful execution

// the link SchemaOp -> DictObject -> SchemaTrans

// check if link to dict object exists
bool
Dbdict::hasDictObject(SchemaOpPtr op_ptr)
{
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  return oprec_ptr.p->m_obj_ptr_i != RNIL;
}

// get dict object for existing link
void
Dbdict::getDictObject(SchemaOpPtr op_ptr, DictObjectPtr& obj_ptr)
{
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(oprec_ptr.p->m_obj_ptr_i != RNIL);
  c_obj_pool.getPtr(obj_ptr, oprec_ptr.p->m_obj_ptr_i);
}

// create link from schema op to dict object
void
Dbdict::linkDictObject(SchemaOpPtr op_ptr, DictObjectPtr obj_ptr)
{
  ndbrequire(!obj_ptr.isNull());
  D("linkDictObject" << V(op_ptr.p->op_key) << V(obj_ptr.i));

  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(oprec_ptr.p->m_obj_ptr_i == RNIL);
  oprec_ptr.p->m_obj_ptr_i = obj_ptr.i;

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  if (!trans_ptr.isNull()) {
    jam();
    ndbrequire(trans_ptr.p->trans_key != 0);
    if (obj_ptr.p->m_op_ref_count == 0) {
      jam();
      obj_ptr.p->m_trans_key = trans_ptr.p->trans_key;
    } else {
      ndbrequire(obj_ptr.p->m_trans_key == trans_ptr.p->trans_key);
    }
  } else {
    jam();
    // restart table uses no trans (yet)
    ndbrequire(memcmp(oprec_ptr.p->m_opType, "CTa", 4) == 0);
    ndbrequire(getNodeState().startLevel != NodeState::SL_STARTED);
  }

  obj_ptr.p->m_op_ref_count += 1;
  D("linkDictObject done" << *obj_ptr.p);
}

// drop link from schema op to dict object
void
Dbdict::unlinkDictObject(SchemaOpPtr op_ptr)
{
  DictObjectPtr obj_ptr;
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(oprec_ptr.p->m_obj_ptr_i != RNIL);
  c_obj_pool.getPtr(obj_ptr, oprec_ptr.p->m_obj_ptr_i);

  D("unlinkDictObject" << V(op_ptr.p->op_key) << V(obj_ptr.i));

  oprec_ptr.p->m_obj_ptr_i = RNIL;
  ndbrequire(obj_ptr.p->m_op_ref_count != 0);
  obj_ptr.p->m_op_ref_count -= 1;
  ndbrequire(obj_ptr.p->m_trans_key != 0);
  if (obj_ptr.p->m_op_ref_count == 0) {
    jam();
    obj_ptr.p->m_trans_key = 0;
  }
  D("unlinkDictObject done" << *obj_ptr.p);
}

// add new dict object and link schema op to it
void
Dbdict::seizeDictObject(SchemaOpPtr op_ptr,
                        DictObjectPtr& obj_ptr,
                        const RopeHandle& name)
{
  D("seizeDictObject" << *op_ptr.p);

  bool ok = c_obj_pool.seize(obj_ptr);
  ndbrequire(ok);
  new (obj_ptr.p) DictObject();

  obj_ptr.p->m_name = name;
  c_obj_name_hash.add(obj_ptr);
  obj_ptr.p->m_ref_count = 0;

  linkDictObject(op_ptr, obj_ptr);
  D("seizeDictObject done" << *obj_ptr.p);
}

// find dict object by name and link schema op to it
bool
Dbdict::findDictObject(SchemaOpPtr op_ptr,
                       DictObjectPtr& obj_ptr,
                       const char* name)
{
  D("findDictObject" << *op_ptr.p << V(name));
  if (get_object(obj_ptr, name)) {
    jam();
    linkDictObject(op_ptr, obj_ptr);
    return true;
  }
  return false;
}

// find dict object by i-value and link schema op to it
bool
Dbdict::findDictObject(SchemaOpPtr op_ptr,
                       DictObjectPtr& obj_ptr,
                       Uint32 obj_ptr_i)
{
  D("findDictObject" << *op_ptr.p << V(obj_ptr.i));
  if (obj_ptr_i != RNIL) {
    jam();
    c_obj_pool.getPtr(obj_ptr, obj_ptr_i);
    linkDictObject(op_ptr, obj_ptr);
    return true;
  }
  return false;
}

// remove link to dict object and release the dict object
void
Dbdict::releaseDictObject(SchemaOpPtr op_ptr)
{
  DictObjectPtr obj_ptr;
  getDictObject(op_ptr, obj_ptr);

  D("releaseDictObject" << *op_ptr.p << V(obj_ptr.i) << *obj_ptr.p);

  unlinkDictObject(op_ptr);

  // check no other object or operation references it
  ndbrequire(obj_ptr.p->m_ref_count == 0);
  if (obj_ptr.p->m_op_ref_count == 0) {
    jam();
    release_object(obj_ptr.i);
  }
}

// find last op on dict object
void
Dbdict::findDictObjectOp(SchemaOpPtr& op_ptr, DictObjectPtr obj_ptr)
{
  D("findDictObjectOp" << *obj_ptr.p);
  op_ptr.setNull();

  Uint32 trans_key = obj_ptr.p->m_trans_key;
  do {
    if (trans_key == 0) {
      jam();
      D("no trans_key");
      break;
    }

    SchemaTransPtr trans_ptr;
    findSchemaTrans(trans_ptr, trans_key);
    if (trans_ptr.isNull()) {
      jam();
      D("trans not found");
      break;
    }
    D("found" << *trans_ptr.p);

    {
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      SchemaOpPtr loop_ptr;
      list.first(loop_ptr);
      while (!loop_ptr.isNull()) {
        jam();
        OpRecPtr oprec_ptr = loop_ptr.p->m_oprec_ptr;
        if (oprec_ptr.p->m_obj_ptr_i == obj_ptr.i) {
          jam();
          op_ptr = loop_ptr;
          D("found candidate" << *op_ptr.p);
        }
        list.next(loop_ptr);
      }
    }
  } while (0);
}

// trans state


// SchemaTrans

bool
Dbdict::seizeSchemaTrans(SchemaTransPtr& trans_ptr, Uint32 trans_key)
{
  if (ERROR_INSERTED(6101)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    trans_ptr.setNull();
    return false;
  }
  if (!findSchemaTrans(trans_ptr, trans_key)) {
    jam();
    ArenaHead arena;
    bool ok = c_arenaAllocator.seize(arena);
    ndbrequire(ok); // TODO: report error
    if (c_schemaTransPool.seize(arena, trans_ptr)) {
      jam();
      new (trans_ptr.p) SchemaTrans();
      trans_ptr.p->trans_key = trans_key;
      trans_ptr.p->m_arena = arena;
      c_schemaTransHash.add(trans_ptr);
      c_schemaTransList.addLast(trans_ptr);
      c_schemaTransCount++;
      D("seizeSchemaTrans" << V(trans_key));
      return true;
    }
    c_arenaAllocator.release(arena);
  }
  trans_ptr.setNull();
  return false;
}

bool
Dbdict::seizeSchemaTrans(SchemaTransPtr& trans_ptr)
{
  Uint32 trans_key = c_opRecordSequence + 1;
  if (seizeSchemaTrans(trans_ptr, trans_key)) {
#ifdef MARTIN
    ndbout_c("Dbdict::seizeSchemaTrans: Seized schema trans %u", trans_key);
#endif
    c_opRecordSequence = trans_key;
    return true;
  }
#ifdef MARTIN
  ndbout_c("Dbdict::seizeSchemaTrans: Failed to seize schema trans");
#endif
  return false;
}

bool
Dbdict::findSchemaTrans(SchemaTransPtr& trans_ptr, Uint32 trans_key)
{
  SchemaTrans trans_rec(trans_key);
  if (c_schemaTransHash.find(trans_ptr, trans_rec)) {
    jam();
    ndbrequire(trans_ptr.p->m_magic == SchemaTrans::DICT_MAGIC);
    D("findSchemaTrans" << V(trans_key));
    return true;
  }
  trans_ptr.setNull();
  return false;
}

void
Dbdict::releaseSchemaTrans(SchemaTransPtr& trans_ptr)
{
  D("releaseSchemaTrans" << V(trans_ptr.p->trans_key));

  {
    /**
     * Put in own scope...since LocalSchemaOp_list stores back head
     *   in destructor
     */
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    SchemaOpPtr op_ptr;
    while (list.first(op_ptr)) {
      list.remove(op_ptr);
      releaseSchemaOp(op_ptr);
    }
  }
  ndbrequire(trans_ptr.p->m_magic == SchemaTrans::DICT_MAGIC);
  ndbrequire(c_schemaTransCount != 0);
  c_schemaTransCount--;
  c_schemaTransList.remove(trans_ptr);
  c_schemaTransHash.remove(trans_ptr);
  ArenaHead arena = trans_ptr.p->m_arena;
  c_schemaTransPool.release(trans_ptr);
  c_arenaAllocator.release(arena);
  trans_ptr.setNull();

  if (c_schemaTransCount == 0)
  {
    jam();

    Resource_limit rl;
    m_ctx.m_mm.get_resource_limit(RG_SCHEMA_TRANS_MEMORY, rl);
    ndbrequire(rl.m_curr <= 1); // ArenaAllocator can keep one page for empty pool
#ifdef VM_TRACE
    if (getNodeState().startLevel == NodeState::SL_STARTED)
      check_consistency();
#endif
  }
}

// client requests

void
Dbdict::execSCHEMA_TRANS_BEGIN_REQ(Signal* signal)
{
  jamEntry();
  const SchemaTransBeginReq* req =
    (const SchemaTransBeginReq*)signal->getDataPtr();
  Uint32 clientRef = req->clientRef;
#ifdef MARTIN
  ndbout_c("Dbdict::execSCHEMA_TRANS_BEGIN_REQ: received GSN_SCHEMA_TRANS_BEGIN_REQ from 0x%8x", clientRef);
#endif

  Uint32 transId = req->transId;
  Uint32 requestInfo = req->requestInfo;

  bool localTrans = (requestInfo & DictSignal::RF_LOCAL_TRANS);

  SchemaTransPtr trans_ptr;
  ErrorInfo error;
  do {
    if (getOwnNodeId() != c_masterNodeId && !localTrans) {
      jam();
      setError(error, SchemaTransBeginRef::NotMaster, __LINE__);
      break;
    }

    if (c_takeOverInProgress)
    {
      /**
       * There is a dict takeover in progress. There may thus another
       * transaction that should be rolled backward or forward before we
       * can allow another transaction to start.
       */
      jam();
      setError(error, SchemaTransBeginRef::Busy, __LINE__);
      break;
    }

    if (!check_ndb_versions() && !localTrans)
    {
      jam();
      setError(error, SchemaTransBeginRef::IncompatibleVersions, __LINE__);
      break;
    }

    if (!seizeSchemaTrans(trans_ptr)) {
      jam();
      // future when more than 1 tx allowed
      setError(error, SchemaTransBeginRef::TooManySchemaTrans, __LINE__);
      break;
    }

    trans_ptr.p->m_isMaster = true;
    trans_ptr.p->m_masterRef = reference();
    trans_ptr.p->m_clientRef = clientRef;
    trans_ptr.p->m_transId = transId;
    trans_ptr.p->m_requestInfo = requestInfo;
    trans_ptr.p->m_obj_id = getFreeObjId();
    if (localTrans)
    {
      /**
       * TODO...use better mechanism...
       *
       * During restart...we need to check both old/new
       *   schema file so that we don't accidently allocate
       *   an objectId that should be used to recreate an object
       */
      trans_ptr.p->m_obj_id = getFreeObjId(true);
    }

    if (!localTrans)
    {
      jam();
      trans_ptr.p->m_nodes = c_aliveNodes;
    }
    else
    {
      jam();
      trans_ptr.p->m_nodes.clear();
      trans_ptr.p->m_nodes.set(getOwnNodeId());
    }
    trans_ptr.p->m_clientState = TransClient::BeginReq;

    // lock
    DictLockReq& lockReq = trans_ptr.p->m_lockReq;
    lockReq.userPtr = trans_ptr.p->trans_key;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::SchemaTransLock;
    int lockError = dict_lock_trylock(&lockReq);
    debugLockInfo(signal,
                  "SCHEMA_TRANS_BEGIN_REQ trylock",
                  lockError);
    if (lockError != 0)
    {
      // remove the trans
      releaseSchemaTrans(trans_ptr);
      setError(error, lockError, __LINE__);
      break;
    }

    // begin tx on all participants
    trans_ptr.p->m_state = SchemaTrans::TS_STARTING;

    /**
     * Send RT_START
     */
    {
      trans_ptr.p->m_ref_nodes.clear();
      NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
      {
        SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
        bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
        ndbrequire(ok);
      }

      if (ERROR_INSERTED(6140))
      {
        /*
          Simulate slave missing start
        */
        jam();
	Uint32 nodeId = rand() % MAX_NDB_NODES;
	while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
	  nodeId = rand() % MAX_NDB_NODES;

	infoEvent("Simulating node %u missing RT_START", nodeId);
        rg.m_nodes.clear(nodeId);
        signal->theData[0] = 9999;
        signal->theData[1] = ERROR_INSERT_VALUE;
        CLEAR_ERROR_INSERT_VALUE;
        sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                            5000, 2);
      }

      SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
      req->senderRef = reference();
      req->transKey = trans_ptr.p->trans_key;
      req->opKey = RNIL;
      req->requestInfo = SchemaTransImplReq::RT_START;
      req->start.clientRef = trans_ptr.p->m_clientRef;
      req->start.objectId = trans_ptr.p->m_obj_id;
      req->transId = trans_ptr.p->m_transId;
      sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
                 SchemaTransImplReq::SignalLengthStart, JBB);
    }

    if (ERROR_INSERTED(6102)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      signal->theData[0] = refToNode(clientRef);
      sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBB);
    }
    return;
  } while(0);

  SchemaTrans tmp_trans;
  trans_ptr.i = RNIL;
  trans_ptr.p = &tmp_trans;
  trans_ptr.p->trans_key = 0;
  trans_ptr.p->m_clientRef = clientRef;
  trans_ptr.p->m_transId = transId;
  trans_ptr.p->m_clientState = TransClient::BeginReq;
  setError(trans_ptr.p->m_error, error);
  sendTransClientReply(signal, trans_ptr);
}

void
Dbdict::trans_start_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();

  switch(trans_ptr.p->m_state){
  case SchemaTrans::TS_STARTING:
    if (hasError(trans_ptr.p->m_error))
    {
      jam();

      /**
       * Clear nodes that did not reply START-CONF
       */
      trans_ptr.p->m_nodes.bitANDC(trans_ptr.p->m_ref_nodes);

      /**
       * Abort before replying to client
       */
      trans_end_start(signal, trans_ptr);
      return;
    }
    else
    {
      jam();
      sendTransClientReply(signal, trans_ptr);
      return;
    }
    break;
  default:
    jamLine(trans_ptr.p->m_state);
    ndbrequire(false);
  }
}

void
Dbdict::execSCHEMA_TRANS_END_REQ(Signal* signal)
{
  jamEntry();
  const SchemaTransEndReq* req =
    (const SchemaTransEndReq*)signal->getDataPtr();
  Uint32 clientRef = req->clientRef;
  Uint32 transId = req->transId;
  Uint32 trans_key = req->transKey;
  Uint32 requestInfo = req->requestInfo;
  Uint32 flags = req->flags;

  SchemaTransPtr trans_ptr;
  ErrorInfo error;
  do {
    findSchemaTrans(trans_ptr, trans_key);
    if (trans_ptr.isNull()) {
      jam();
      setError(error, SchemaTransEndRef::InvalidTransKey, __LINE__);
      break;
    }

    if (trans_ptr.p->m_transId != transId) {
      jam();
      setError(error, SchemaTransEndRef::InvalidTransId, __LINE__);
      break;
    }

    bool localTrans = (trans_ptr.p->m_requestInfo & DictSignal::RF_LOCAL_TRANS);

    if (getOwnNodeId() != c_masterNodeId && !localTrans) {
      jam();
      // future when MNF is handled
      //ndbassert(false);
      setError(error, SchemaTransEndRef::NotMaster, __LINE__);
      break;
    }

    if (c_takeOverInProgress)
    {
      /**
       * There is a dict takeover in progress, and the transaction may thus
       * be in an inconsistent state. Therefore we cannot process this request
       * now.
       */
      jam();
      setError(error, SchemaTransEndRef::Busy, __LINE__);
      break;
    }
#ifdef MARTIN
    ndbout_c("Dbdict::execSCHEMA_TRANS_END_REQ: trans %u, state %u", trans_ptr.i, trans_ptr.p->m_state);
#endif

    //XXX Check state

    if (hasError(trans_ptr.p->m_error))
    {
      jam();
      ndbassert(false);
      setError(error, SchemaTransEndRef::InvalidTransState, __LINE__);
      break;
    }

    bool localTrans2 = requestInfo & DictSignal::RF_LOCAL_TRANS;
    if (localTrans != localTrans2)
    {
      jam();
      ndbassert(false);
      setError(error, SchemaTransEndRef::InvalidTransState, __LINE__);
      break;
    }

    trans_ptr.p->m_clientState = TransClient::EndReq;

    const bool doBackground = flags & SchemaTransEndReq::SchemaTransBackground;
    if (doBackground)
    {
      jam();
      // send reply to original client and restore EndReq state
      sendTransClientReply(signal, trans_ptr);
      trans_ptr.p->m_clientState = TransClient::EndReq;

      // take over client role via internal trans
      trans_ptr.p->m_clientFlags |= TransClient::Background;
      takeOverTransClient(signal, trans_ptr);
    }

    if (flags & SchemaTransEndReq::SchemaTransAbort)
    {
      jam();
      trans_abort_prepare_start(signal, trans_ptr);
      return;
    }
    else if ((flags & SchemaTransEndReq::SchemaTransPrepare) == 0)
    {
      jam();
      trans_ptr.p->m_clientFlags |= TransClient::Commit;
    }

    trans_prepare_start(signal, trans_ptr);
    return;
  } while (0);

  SchemaTrans tmp_trans;
  trans_ptr.i = RNIL;
  trans_ptr.p = &tmp_trans;
  trans_ptr.p->trans_key = trans_key;
  trans_ptr.p->m_clientRef = clientRef;
  trans_ptr.p->m_transId = transId;
  trans_ptr.p->m_clientState = TransClient::EndReq;
  setError(trans_ptr.p->m_error, error);
  sendTransClientReply(signal, trans_ptr);
}

// coordinator

void
Dbdict::handleClientReq(Signal* signal, SchemaOpPtr op_ptr,
                        SectionHandle& handle)
{
  D("handleClientReq" << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (trans_ptr.p->m_state == SchemaTrans::TS_SUBOP)
  {
    jam();
    SchemaOpPtr baseOp;
    c_schemaOpPool.getPtr(baseOp, trans_ptr.p->m_curr_op_ptr_i);
    op_ptr.p->m_base_op_ptr_i = baseOp.i;
  }

  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;
  op_ptr.p->m_state = SchemaOp::OS_PARSE_MASTER;

  ErrorInfo error;
  const OpInfo& info = getOpInfo(op_ptr);

  if (checkSingleUserMode(trans_ptr.p->m_clientRef))
  {
    jam();
    setError(error, AlterTableRef::SingleUser, __LINE__);
  }
  else
  {
    jam();
    (this->*(info.m_parse))(signal, true, op_ptr, handle, error);
  }

  if (hasError(error))
  {
    jam();
    setError(trans_ptr, error);
    releaseSections(handle);
    trans_rollback_sp_start(signal, trans_ptr);
    return;
  }

  trans_ptr.p->m_state = SchemaTrans::TS_PARSING;
  op_ptr.p->m_state = SchemaOp::OS_PARSING;

  Uint32 gsn = info.m_impl_req_gsn;
  const Uint32* src = op_ptr.p->m_oprec_ptr.p->m_impl_req_data;

  Uint32* data = signal->getDataPtrSend();
  Uint32 skip = SchemaTransImplReq::SignalLength;
  Uint32 extra_length = info.m_impl_req_length;
  ndbrequire(skip + extra_length <= 25);

  Uint32 i;
  for (i = 0; i < extra_length; i++)
    data[skip + i] = src[i];

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, SchemaTransImplReq::RT_PARSE);
  DictSignal::addRequestFlags(requestInfo, op_ptr.p->m_requestInfo);
  DictSignal::addRequestExtra(requestInfo, op_ptr.p->m_requestInfo);

  trans_ptr.p->m_ref_nodes.clear();
  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;

  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6141))
  {
    /*
      Simulate slave missing parsing last (this) op
     */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_PARSE", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = requestInfo;
  req->transId = trans_ptr.p->m_transId;
  req->parse.gsn = gsn;
  sendFragmentedSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
                       SchemaTransImplReq::SignalLength + extra_length, JBB,
                       &handle);
}

void
Dbdict::trans_parse_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  op_ptr.p->m_state = SchemaOp::OS_PARSED;

  if (hasError(trans_ptr.p->m_error))
  {
    jam();
    trans_rollback_sp_start(signal, trans_ptr);
    return;
  }

  const OpInfo& info = getOpInfo(op_ptr);
  if ((this->*(info.m_subOps))(signal, op_ptr))
  {
    jam();
    // more sub-ops on the way
    trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;
    trans_ptr.p->m_state = SchemaTrans::TS_SUBOP;
    return;
  }

  /**
   * Reply to client
   */
  ErrorInfo error;
  (this->*(info.m_reply))(signal, op_ptr, error);

  trans_ptr.p->m_clientState = TransClient::ParseReply;
  trans_ptr.p->m_state = SchemaTrans::TS_STARTED;
}

void
Dbdict::execSCHEMA_TRANS_IMPL_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);

  if (c_takeOverInProgress)
  {
    /**
     * The new master will rebuild the transaction state from the
     * DICT_TAKEOVER_CONF signals. Therefore we ignore this signal during 
     * takeover.
     */
    jam();
    return;
  }

  const SchemaTransImplConf* conf =
    (const SchemaTransImplConf*)signal->getDataPtr();

  SchemaTransPtr trans_ptr;
  ndbrequire(findSchemaTrans(trans_ptr, conf->transKey));

  Uint32 senderRef = conf->senderRef;
  Uint32 nodeId = refToNode(senderRef);

  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    if (!sc.clearWaitingFor(nodeId)) {
      jam();
      return;
    }
  }

  trans_recv_reply(signal, trans_ptr);
}

void
Dbdict::execSCHEMA_TRANS_IMPL_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);

  if (c_takeOverInProgress)
  {
    /**
     * The new master will rebuild the transaction state from the
     * DICT_TAKEOVER_CONF signals. Therefore we ignore this signal during 
     * takeover.
     */
    jam();
    return;
  }

  SchemaTransImplRef refCopy =
    *(SchemaTransImplRef*)signal->getDataPtr();
  SchemaTransImplRef * ref = &refCopy;

  SchemaTransPtr trans_ptr;
  ndbrequire(findSchemaTrans(trans_ptr, ref->transKey));

  Uint32 senderRef = ref->senderRef;
  Uint32 nodeId = refToNode(senderRef);

#ifdef MARTIN
  ndbout_c("Got SCHEMA_TRANS_IMPL_REF from node %u, error %u", nodeId, ref->errorCode);
#endif
  if (ref->errorCode == SchemaTransImplRef::NF_FakeErrorREF)
  {
    jam();
    // trans_ptr.p->m_nodes.clear(nodeId);
    // No need to clear, will be cleared when next REQ is set
    if (!trans_ptr.p->m_abort_on_node_fail)
    {
      jam();
      ref->errorCode = 0;
    }
    else
    {
      jam();
      ref->errorCode = SchemaTransBeginRef::Nodefailure;
    }
  }

  if (ref->errorCode)
  {
    jam();
    ErrorInfo error;
    setError(error, ref);
    setError(trans_ptr, error);
    switch(trans_ptr.p->m_state){
    case SchemaTrans::TS_STARTING:
      jam();
      trans_ptr.p->m_ref_nodes.set(nodeId);
      break;
    case SchemaTrans::TS_PARSING:
      jam();
      if (ref->errorCode == SchemaTransImplRef::SeizeFailed)
      {
        jam();
        trans_ptr.p->m_ref_nodes.set(nodeId);
      }
      break;
    default:
      jam();
    }
  }

  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    if (!sc.clearWaitingFor(nodeId)) {
      jam();
      return;
    }
  }

  trans_recv_reply(signal, trans_ptr);
}

void
Dbdict::trans_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  switch(trans_ptr.p->m_state){
  case SchemaTrans::TS_INITIAL:
    ndbrequire(false);
  case SchemaTrans::TS_STARTING:
    jam();
    trans_start_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_PARSING:
    jam();
    trans_parse_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_SUBOP:
    ndbrequire(false);
  case SchemaTrans::TS_ROLLBACK_SP:
    trans_rollback_sp_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_FLUSH_PREPARE:
    trans_prepare_first(signal, trans_ptr);
    return;
  case SchemaTrans::TS_PREPARING:
    jam();
    trans_prepare_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_ABORTING_PREPARE:
    jam();
    trans_abort_prepare_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_ABORTING_PARSE:
    jam();
    trans_abort_parse_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_FLUSH_COMMIT:
    jam();
    trans_commit_first(signal, trans_ptr);
    return;
  case SchemaTrans::TS_COMMITTING:
    trans_commit_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_FLUSH_COMPLETE:
    jam();
    trans_complete_first(signal, trans_ptr);
    return;
  case SchemaTrans::TS_COMPLETING:
    trans_complete_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_ENDING:
    trans_end_recv_reply(signal, trans_ptr);
    return;
  case SchemaTrans::TS_STARTED:   // These states are waiting for client
    jam();                        // And should not get a "internal" reply
    ndbrequire(false);
  }
}

#if 0
void
Dbdict::handleTransReply(Signal* signal, SchemaTransPtr trans_ptr)
{
  TransLoc& tLoc = trans_ptr.p->m_transLoc;
  SchemaOpPtr op_ptr;
  getOpPtr(tLoc, op_ptr);

  //const Uint32 trans_key = trans_ptr.p->trans_key;
  //const Uint32 clientRef = trans_ptr.p->m_clientRef;
  //const Uint32 transId = trans_ptr.p->m_transId;

  D("handleTransReply" << tLoc);
  if (!op_ptr.isNull())
    D("have op" << *op_ptr.p);

  if (hasError(trans_ptr.p->m_error)) {
    jam();
    if (tLoc.m_mode == TransMode::Normal) {
      if (tLoc.m_phase == TransPhase::Parse) {
        jam();
        setTransMode(trans_ptr, TransMode::Rollback, true);
      } else {
        jam();
        setTransMode(trans_ptr, TransMode::Abort, true);
      }
    }
  }

  if (tLoc.m_mode == TransMode::Normal) {
    if (tLoc.m_phase == TransPhase::Begin) {
      jam();
      sendTransClientReply(signal, trans_ptr);
    }
    else if (tLoc.m_phase == TransPhase::Parse) {
      jam();
      /*
       * Create any sub-operations via client signals.  This is
       * a recursive process.  When done at current level, sends reply
       * to client.  On inner levels the client is us (dict master).
       */
      createSubOps(signal, op_ptr, true);
    }
    else if (tLoc.m_phase == TransPhase::Prepare) {
      jam();
      runTransMaster(signal, trans_ptr);
    }
    else if (tLoc.m_phase == TransPhase::Commit) {
      jam();
      runTransMaster(signal, trans_ptr);
    }
    else if (tLoc.m_phase == TransPhase::End)
    {
      jam();
      trans_commit_done(signal, trans_ptr);
      return;
    }
    else {
      ndbrequire(false);
    }
  }
  else if (tLoc.m_mode == TransMode::Rollback) {
    if (tLoc.m_phase == TransPhase::Parse) {
      jam();
      /*
       * Rolling back current client op and its sub-ops.
       * We do not follow the signal train of master REQs back.
       * Instead we simply run backwards in abort mode until
       * (and including) first op of depth zero.  All ops involved
       * are removed.  On master this is done here.
       */
      ndbrequire(hasError(trans_ptr.p->m_error));
      ndbrequire(!op_ptr.isNull());
      if (tLoc.m_hold) {
        jam();
        // error seen first time, re-run current op
        runTransMaster(signal, trans_ptr);
      }
      else {
        if (op_ptr.p->m_opDepth != 0) {
          jam();
          runTransMaster(signal, trans_ptr);
        }
        else {
          jam();
          // reached original client op
          const OpInfo& info = getOpInfo(op_ptr);
          (this->*(info.m_reply))(signal, op_ptr, trans_ptr.p->m_error);
          resetError(trans_ptr);

          // restore depth counter
          trans_ptr.p->m_opDepth = 0;
          trans_ptr.p->m_clientState = TransClient::ParseReply;
        }
        iteratorRemoveLastOp(tLoc, op_ptr);
      }
    }
    else {
      ndbrequire(false);
    }
  }
  else if (tLoc.m_mode == TransMode::Abort) {
    if (tLoc.m_phase == TransPhase::Begin) {
      jam();
      sendTransClientReply(signal, trans_ptr);
      // unlock
      const DictLockReq& lockReq = trans_ptr.p->m_lockReq;
      Uint32 rc = dict_lock_unlock(signal, &lockReq);
      debugLockInfo(signal,
                    "handleTransReply unlock",
                    rc);
      releaseSchemaTrans(trans_ptr);
    }
    else if (tLoc.m_phase == TransPhase::Parse) {
      jam();
      runTransMaster(signal, trans_ptr);
    }
    else if (tLoc.m_phase == TransPhase::Prepare) {
      jam();
      runTransMaster(signal, trans_ptr);
    }
    else {
      ndbrequire(false);
    }
  }
  else {
    ndbrequire(false);
  }
}
#endif

void
Dbdict::createSubOps(Signal* signal, SchemaOpPtr op_ptr, bool first)
{
  D("createSubOps" << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  const OpInfo& info = getOpInfo(op_ptr);
  if ((this->*(info.m_subOps))(signal, op_ptr)) {
    jam();
    // more sub-ops on the way
    trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;
    trans_ptr.p->m_state = SchemaTrans::TS_SUBOP;
    return;
  }

  ErrorInfo error;
  (this->*(info.m_reply))(signal, op_ptr, error);

  trans_ptr.p->m_clientState = TransClient::ParseReply;
  trans_ptr.p->m_state = SchemaTrans::TS_STARTED;
}

// a sub-op create failed, roll back and send REF to client
void
Dbdict::abortSubOps(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  D("abortSubOps" << *op_ptr.p << error);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  setError(trans_ptr, error);
  trans_rollback_sp_start(signal, trans_ptr);
}

void
Dbdict::trans_prepare_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_PREPARE;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6013))
  {
    jam();
    CRASH_INSERTION(6013);
  }

  if (ERROR_INSERTED(6022))
  {
    jam();
    NodeReceiverGroup rg(CMVMI, c_aliveNodes);
    signal->theData[0] = 9999;
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

  if (ERROR_INSERTED(6142))
  {
    /*
      Simulate slave missing flush prepare
     */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_FLUSH_PREPARE", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = RNIL;
  req->requestInfo = SchemaTransImplReq::RT_FLUSH_PREPARE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_prepare_first(Signal* signal, SchemaTransPtr trans_ptr)
{
  if (ERROR_INSERTED(6021))
  {
    jam();
    NodeReceiverGroup rg(CMVMI, c_aliveNodes);
    signal->theData[0] = 9999;
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

  trans_ptr.p->m_state = SchemaTrans::TS_PREPARING;

  SchemaOpPtr op_ptr;
  {
    bool first;
    {
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      first = list.first(op_ptr);
    }
    if (first)
    {
      jam();
      trans_prepare_next(signal, trans_ptr, op_ptr);
      return;
    }
  }

  trans_prepare_done(signal, trans_ptr);
}

void
Dbdict::trans_prepare_next(Signal* signal,
                           SchemaTransPtr trans_ptr,
                           SchemaOpPtr op_ptr)
{
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_PREPARING);

  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;
  op_ptr.p->m_state = SchemaOp::OS_PREPARING;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6143))
  {
    jam();
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    if (!list.hasNext(op_ptr))
    {
      /*
        Simulate slave missing preparing last op
      */
      jam();
      Uint32 nodeId = rand() % MAX_NDB_NODES;
      while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
        nodeId = rand() % MAX_NDB_NODES;

      infoEvent("Simulating node %u missing RT_PREPARE", nodeId);
      rg.m_nodes.clear(nodeId);
      signal->theData[0] = 9999;
      signal->theData[1] = ERROR_INSERT_VALUE;
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          5000, 2);
    }
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_PREPARE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_prepare_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();

  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);
  if (hasError(trans_ptr.p->m_error))
  {
    jam();
    trans_ptr.p->m_state = SchemaTrans::TS_ABORTING_PREPARE;
    trans_abort_prepare_next(signal, trans_ptr, op_ptr);
    return;
  }

  {
    bool next;
    {
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      next = list.next(op_ptr);
    }
    if (next)
    {
      jam();
      trans_prepare_next(signal, trans_ptr, op_ptr);
      return;
    }
  }

  trans_prepare_done(signal, trans_ptr);
  return;
}

void
Dbdict::trans_prepare_done(Signal* signal, SchemaTransPtr trans_ptr)
{
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_PREPARING);

  if (ERROR_INSERTED(6145))
  {
    jam();
    trans_abort_prepare_start(signal, trans_ptr);
    return;
  }

  if (trans_ptr.p->m_clientFlags & TransClient::Commit)
  {
    jam();
    trans_commit_start(signal, trans_ptr);
    return;
  }

  // prepare not currently implemted (fully)
  ndbrequire(false);
}

void
Dbdict::trans_abort_parse_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  trans_ptr.p->m_state = SchemaTrans::TS_ABORTING_PARSE;

  SchemaOpPtr op_ptr;
  bool last = false;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    last =  list.last(op_ptr);
  }

  if (last)
  {
    jam();
    trans_abort_parse_next(signal, trans_ptr, op_ptr);
    return;
  }

  trans_abort_parse_done(signal, trans_ptr);

}

void
Dbdict::trans_abort_parse_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  {
    SchemaOpPtr last_op = op_ptr;
    bool prev = false;
    {
      LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
      prev = list.prev(op_ptr);
      list.remove(last_op);         // Release aborted op
    }
    releaseSchemaOp(last_op);

    if (prev)
    {
      jam();
      trans_abort_parse_next(signal, trans_ptr, op_ptr);
      return;
    }
  }

  trans_abort_parse_done(signal, trans_ptr);
}

void
Dbdict::check_partial_trans_abort_parse_next(SchemaTransPtr trans_ptr,
                                             NdbNodeBitmask &nodes,
                                             SchemaOpPtr op_ptr)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
  {
    /*
      A new master is in the process of aborting a
      transaction taken over from the failed master.
      Check if any nodes should be skipped because they
      have not parsed the operation to be aborted
    */
    jam();
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      NodeRecordPtr nodePtr;
      if (trans_ptr.p->m_nodes.get(i))
      {
        jam();
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Checking node %u(%u), %u(%u)<%u", nodePtr.i, nodePtr.p->recoveryState, nodePtr.p->start_op, nodePtr.p->start_op_state, op_ptr.p->op_key);
#endif
        if (nodePtr.p->recoveryState == NodeRecord::RS_PARTIAL_ROLLBACK &&
            //nodePtr.p->start_op_state == SchemaOp::OS_PARSED &&
            nodePtr.p->start_op < op_ptr.p->op_key)
        {
          jam();
#ifdef VM_TRACE
          ndbout_c("Skip aborting operation %u on node %u", op_ptr.p->op_key, i);
#endif
          nodes.clear(i);
          nodePtr.p->recoveryState = NodeRecord::RS_NORMAL;
        }
      }
    }
  }
}

void
Dbdict::trans_abort_parse_next(Signal* signal,
                               SchemaTransPtr trans_ptr,
                               SchemaOpPtr op_ptr)
{
  jam();
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_ABORTING_PARSE);
#ifdef MARTIN
  ndbout_c("Dbdict::trans_abort_parse_next: op %u state %u", op_ptr.i,op_ptr.p->m_state);
#endif
  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;
  op_ptr.p->m_state = SchemaOp::OS_ABORTING_PARSE;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  check_partial_trans_abort_parse_next(trans_ptr, nodes, op_ptr);
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6144))
  {
    jam();
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    if (!list.hasNext(op_ptr))
    {
      /*
        Simulate slave missing aborting parse for last op
      */
      jam();
      Uint32 nodeId = rand() % MAX_NDB_NODES;
      while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
        nodeId = rand() % MAX_NDB_NODES;

      infoEvent("Simulating node %u missing RT_ABORT_PARSE", nodeId);
      rg.m_nodes.clear(nodeId);
      signal->theData[0] = 9999;
      signal->theData[1] = ERROR_INSERT_VALUE;
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          5000, 2);
    }
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_ABORT_PARSE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_abort_parse_done(Signal* signal, SchemaTransPtr trans_ptr)
{
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_ABORTING_PARSE);

  trans_end_start(signal, trans_ptr);
}

void
Dbdict::trans_abort_prepare_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  trans_ptr.p->m_state = SchemaTrans::TS_ABORTING_PREPARE;

  bool last = false;
  SchemaOpPtr op_ptr;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    last = list.last(op_ptr);
  }

  if (last)
  {
    jam();
    trans_abort_prepare_next(signal, trans_ptr, op_ptr);
  }
  else
  {
    jam();
    trans_abort_prepare_done(signal, trans_ptr);
  }
}

void
Dbdict::trans_abort_prepare_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  // XXX error states

  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  op_ptr.p->m_state = SchemaOp::OS_ABORTED_PREPARE;

  bool prev = false;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    prev = list.prev(op_ptr);
  }

  if (prev)
  {
    jam();
    trans_abort_prepare_next(signal, trans_ptr, op_ptr);
  }
  else
  {
    jam();
    trans_abort_prepare_done(signal, trans_ptr);
  }
}

void
Dbdict::check_partial_trans_abort_prepare_next(SchemaTransPtr trans_ptr,
                                               NdbNodeBitmask &nodes,
                                               SchemaOpPtr op_ptr)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
  {
    /*
      A new master is in the process of aborting a
      transaction taken over from the failed master.
      Check if any nodes should be skipped because they
      have already aborted the operation.
    */
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      NodeRecordPtr nodePtr;
      if (trans_ptr.p->m_nodes.get(i))
      {
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Checking node %u(%u), %u(%u)<%u", nodePtr.i, nodePtr.p->recoveryState, nodePtr.p->start_op, nodePtr.p->start_op_state, op_ptr.p->op_key);
#endif
        if (nodePtr.p->recoveryState == NodeRecord::RS_PARTIAL_ROLLBACK &&
            ((nodePtr.p->start_op_state == SchemaOp::OS_PARSED &&
              nodePtr.p->start_op <= op_ptr.p->op_key) ||
             (nodePtr.p->start_op_state == SchemaOp::OS_PREPARED &&
              nodePtr.p->start_op < op_ptr.p->op_key) ||
             (nodePtr.p->start_op_state == SchemaOp::OS_ABORTED_PREPARE &&
              nodePtr.p->start_op >= op_ptr.p->op_key)))
        {
#ifdef VM_TRACE
          ndbout_c("Skip aborting operation %u on node %u", op_ptr.p->op_key, i);
#endif
          nodes.clear(i);
          nodePtr.p->recoveryState = NodeRecord::RS_NORMAL;
        }
      }
    }
  }
}

void
Dbdict::trans_abort_prepare_next(Signal* signal,
                                 SchemaTransPtr trans_ptr,
                                 SchemaOpPtr op_ptr)
{
  jam();
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_ABORTING_PREPARE);
#ifdef MARTIN
  ndbout_c("Dbdict::trans_abort_prepare_next: op %u state %u", op_ptr.p->op_key, op_ptr.p->m_state);
#endif
  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;

  switch(op_ptr.p->m_state){
  case SchemaOp::OS_PARSED:
    jam();
    /**
     * Operation has not beed prepared...
     *   continue with next
     */
    trans_abort_prepare_recv_reply(signal, trans_ptr);
    return;
  case SchemaOp::OS_PREPARING:
  case SchemaOp::OS_PREPARED:
    break;
  case SchemaOp::OS_INITIAL:
  case SchemaOp::OS_PARSE_MASTER:
  case SchemaOp::OS_PARSING:
  case SchemaOp::OS_ABORTING_PREPARE:
  case SchemaOp::OS_ABORTED_PREPARE:
  case SchemaOp::OS_ABORTING_PARSE:
    //case SchemaOp::OS_ABORTED_PARSE:
  case SchemaOp::OS_COMMITTING:
  case SchemaOp::OS_COMMITTED:
#ifndef VM_TRACE
  default:
#endif
    jamLine(op_ptr.p->m_state);
    ndbrequire(false);
  }

  op_ptr.p->m_state = SchemaOp::OS_ABORTING_PREPARE;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  check_partial_trans_abort_prepare_next(trans_ptr, nodes, op_ptr);
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6145))
  {
    jam();
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    if (!list.hasPrev(op_ptr))
    {
      /*
        Simulate slave missing aborting prepare of last op
      */
      jam();
      Uint32 nodeId = rand() % MAX_NDB_NODES;
      while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
        nodeId = rand() % MAX_NDB_NODES;

      infoEvent("Simulating node %u missing RT_ABORT_PREPARE", nodeId);
      rg.m_nodes.clear(nodeId);
      signal->theData[0] = 9999;
      signal->theData[1] = ERROR_INSERT_VALUE;
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          5000, 2);
    }
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_ABORT_PREPARE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_abort_prepare_done(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();
  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_ABORTING_PREPARE);
#ifdef MARTIN
  ndbout_c("Dbdict::trans_abort_prepare_done");
#endif
  /**
   * Now run abort parse
   */
  trans_abort_parse_start(signal, trans_ptr);
}

/**
 * FLOW: Rollback SP
 *   abort parse each operation (backwards) until op which is not subop
 */
void
Dbdict::trans_rollback_sp_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  SchemaOpPtr op_ptr;

  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    ndbrequire(list.last(op_ptr));
  }

  trans_ptr.p->m_state = SchemaTrans::TS_ROLLBACK_SP;

  if (op_ptr.p->m_state == SchemaOp::OS_PARSE_MASTER)
  {
    jam();
    /**
     * This op is only parsed at master..
     *
     */
    NdbNodeBitmask nodes;
    nodes.set(getOwnNodeId());
    NodeReceiverGroup rg(DBDICT, nodes);
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);

    const OpInfo& info = getOpInfo(op_ptr);
    (this->*(info.m_abortParse))(signal, op_ptr);
    trans_log_schema_op_abort(op_ptr);
    return;
  }

  trans_rollback_sp_next(signal, trans_ptr, op_ptr);
}

void
Dbdict::trans_rollback_sp_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  // TODO split trans error from op_error...?

  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  if (op_ptr.p->m_base_op_ptr_i == RNIL)
  {
    /**
     * SP
     */
    trans_rollback_sp_done(signal, trans_ptr, op_ptr);
    return;
  }

  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);

    SchemaOpPtr last_op = op_ptr;
    ndbrequire(list.prev(op_ptr)); // Must have prev, as not SP
    list.remove(last_op);         // Release aborted op
    releaseSchemaOp(last_op);
  }

  trans_rollback_sp_next(signal, trans_ptr, op_ptr);
}

void
Dbdict::trans_rollback_sp_next(Signal* signal,
                               SchemaTransPtr trans_ptr,
                               SchemaOpPtr op_ptr)
{
  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  nodes.bitANDC(trans_ptr.p->m_ref_nodes);
  trans_ptr.p->m_ref_nodes.clear();
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6144))
  {
    jam();
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    if (!list.hasPrev(op_ptr))
    {
      /*
        Simulate slave missing aborting parsing of last op
      */
      jam();
      Uint32 nodeId = rand() % MAX_NDB_NODES;
      while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
        nodeId = rand() % MAX_NDB_NODES;

      infoEvent("Simulating node %u missing RT_ABORT_PARSE", nodeId);
      rg.m_nodes.clear(nodeId);
      signal->theData[0] = 9999;
      signal->theData[1] = ERROR_INSERT_VALUE;
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          5000, 2);
    }
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_ABORT_PARSE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);

}

void
Dbdict::trans_rollback_sp_done(Signal* signal,
                               SchemaTransPtr trans_ptr,
                               SchemaOpPtr op_ptr)
{

  ErrorInfo error = trans_ptr.p->m_error;
  const OpInfo info = getOpInfo(op_ptr);
  (this->*(info.m_reply))(signal, op_ptr, error);

  LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
  list.remove(op_ptr);
  releaseSchemaOp(op_ptr);

  resetError(trans_ptr);
  trans_ptr.p->m_clientState = TransClient::ParseReply;
  trans_ptr.p->m_state = SchemaTrans::TS_STARTED;
}

void Dbdict::check_partial_trans_commit_start(SchemaTransPtr trans_ptr,
                                              NdbNodeBitmask &nodes)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
  {
    /*
      A new master is in the process of commiting a
      transaction taken over from the failed master.
      Check if some slave have already flushed the commit.
     */
    jam();
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      NodeRecordPtr nodePtr;
      if (trans_ptr.p->m_nodes.get(i))
      {
        jam();
        c_nodes.getPtr(nodePtr, i);
        if (nodePtr.p->recoveryState == NodeRecord::RS_PARTIAL_ROLLFORWARD)
        {
          jam();
#ifdef VM_TRACE
          ndbout_c("Skip flushing commit on node %u", i);
#endif
          nodes.clear(i);
          nodePtr.p->recoveryState = NodeRecord::RS_NORMAL;
        }
      }
    }
  }
}

void
Dbdict::trans_commit_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  if (ERROR_INSERTED(6016) || ERROR_INSERTED(6017))
  {
    jam();
    signal->theData[0] = 9999;
    NdbNodeBitmask mask = c_aliveNodes;
    if (c_masterNodeId == getOwnNodeId())
    {
      jam();
      mask.clear(getOwnNodeId());
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
      if (mask.isclear())
      {
        return;
      }
    }
    NodeReceiverGroup rg(CMVMI, mask);
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

  trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_COMMIT;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  check_partial_trans_commit_start(trans_ptr, nodes);
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6146))
  {
    jam();
    /*
      Simulate slave missing flushing commit
    */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_FLUSH_COMMIT", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
			5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = RNIL;
  req->requestInfo = SchemaTransImplReq::RT_FLUSH_COMMIT;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_commit_first(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();

  if (ERROR_INSERTED(6018))
  {
    jam();
    NodeReceiverGroup rg(CMVMI, c_aliveNodes);
    signal->theData[0] = 9999;
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

#ifdef MARTIN
  ndbout_c("trans_commit");
#endif

  trans_ptr.p->m_state = SchemaTrans::TS_COMMITTING;

  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER &&
      ownNodePtr.p->takeOverConf.trans_state >= SchemaTrans::TS_COMMITTING)
  {
    /*
      Master take-over, new master already has lock.
     */
    jam();
    trans_commit_mutex_locked(signal, trans_ptr.i, 0);
  }
  else if (trans_ptr.p->m_wait_gcp_on_commit)
  {
    jam();

    signal->theData[0] = 0; // user ptr
    signal->theData[1] = 0; // Execute direct
    signal->theData[2] = 1; // Current
    EXECUTE_DIRECT(DBDIH, GSN_GETGCIREQ, signal, 3);

    jamEntry();
    Uint32 gci_hi = signal->theData[1];
    Uint32 gci_lo = signal->theData[2];

    signal->theData[0] = ZCOMMIT_WAIT_GCI;
    signal->theData[1] = trans_ptr.i;
    signal->theData[2] = gci_hi;
    signal->theData[3] = gci_lo;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 4);

    signal->theData[0] = 6099;
    sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);
  }
  else
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, trans_ptr.p->m_commit_mutex);
    Callback c = { safe_cast(&Dbdict::trans_commit_mutex_locked), trans_ptr.i };

    // Todo should alloc mutex on SCHEMA_BEGIN
    bool ok = mutex.lock(c);
    ndbrequire(ok);
  }
}

void
Dbdict::trans_commit_wait_gci(Signal* signal)
{
  jam();
  SchemaTransPtr trans_ptr;
  c_schemaTransPool.getPtr(trans_ptr, signal->theData[1]);

  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_COMMITTING);

  Uint32 gci_hi = signal->theData[2];
  Uint32 gci_lo = signal->theData[3];

  signal->theData[0] = 0; // user ptr
  signal->theData[1] = 0; // Execute direct
  signal->theData[2] = 1; // Current
  EXECUTE_DIRECT(DBDIH, GSN_GETGCIREQ, signal, 3);

  jamEntry();
  Uint32 curr_gci_hi = signal->theData[1];
  Uint32 curr_gci_lo = signal->theData[2];

  if (!getNodeState().getStarted())
  {
    jam();
    /**
     * node is starting
     */
  }
  else if (curr_gci_hi == gci_hi && curr_gci_lo == gci_lo)
  {
    jam();
    signal->theData[0] = ZCOMMIT_WAIT_GCI;
    signal->theData[1] = trans_ptr.i;
    signal->theData[2] = gci_hi;
    signal->theData[3] = gci_lo;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 4);
    return;
  }

  Mutex mutex(signal, c_mutexMgr, trans_ptr.p->m_commit_mutex);
  Callback c = { safe_cast(&Dbdict::trans_commit_mutex_locked), trans_ptr.i };

  // Todo should alloc mutex on SCHEMA_BEGIN
  bool ok = mutex.lock(c);
  ndbrequire(ok);
}

void
Dbdict::trans_commit_mutex_locked(Signal* signal,
                                  Uint32 transPtrI,
                                  Uint32 ret)
{
  jamEntry();
#ifdef MARTIN
  ndbout_c("trans_commit_mutex_locked");
#endif
  SchemaTransPtr trans_ptr;
  c_schemaTransPool.getPtr(trans_ptr, transPtrI);

  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_COMMITTING);

  bool first = false;
  SchemaOpPtr op_ptr;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    first = list.first(op_ptr);
  }

  if (first)
  {
    jam();
    trans_commit_next(signal, trans_ptr, op_ptr);
    if (ERROR_INSERTED(6014)) {
      jam();
      CRASH_INSERTION(6014);
    }
  }
  else
  {
    jam();
    trans_commit_done(signal, trans_ptr);
    if (ERROR_INSERTED(6015)) {
      jam();
      CRASH_INSERTION(6015);
    }
  }
}

void Dbdict::check_partial_trans_commit_next(SchemaTransPtr trans_ptr,
                                             NdbNodeBitmask &nodes,
                                             SchemaOpPtr op_ptr)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER &&
      trans_ptr.p->check_partial_rollforward)
  {
    /*
      A new master is in the process of committing a
      transaction taken over from the failed master.
      Check if any nodes should be skipped because they
      have already commited the operation
    */
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      NodeRecordPtr nodePtr;
#ifdef VM_TRACE
      ndbout_c("Node %u", i);
#endif
      if (trans_ptr.p->m_nodes.get(i))
      {
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Checking node %u(%u), %u<%u", nodePtr.i, nodePtr.p->recoveryState, nodePtr.p->start_op, op_ptr.p->op_key);
#endif
        if (nodePtr.p->recoveryState == NodeRecord::RS_PARTIAL_ROLLFORWARD &&
            (nodePtr.p->start_op > op_ptr.p->op_key ||
             nodePtr.p->start_op_state > op_ptr.p->m_state))
        {
#ifdef VM_TRACE
          ndbout_c("Skipping commit of operation %u on node %u", op_ptr.p->op_key, i);
#endif
          nodes.clear(i);
          nodePtr.p->recoveryState = NodeRecord::RS_NORMAL;
        }
      }
    }
    trans_ptr.p->check_partial_rollforward = false;
  }

}
void
Dbdict::trans_commit_next(Signal* signal,
                          SchemaTransPtr trans_ptr,
                          SchemaOpPtr op_ptr)
{
  jam();
#ifdef MARTIN
  ndbout_c("Dbdict::trans_commit_next: op %u state %u", op_ptr.i,op_ptr.p->m_state);
#endif
  op_ptr.p->m_state = SchemaOp::OS_COMMITTING;
  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  check_partial_trans_commit_next(trans_ptr, nodes, op_ptr);
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6147))
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    if (!list.hasNext(op_ptr))
    {
      jam();
      /*
        Simulate slave missing committing last op
      */
      jam();
      Uint32 nodeId = rand() % MAX_NDB_NODES;
      while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
        nodeId = rand() % MAX_NDB_NODES;

      infoEvent("Simulating node %u missing RT_COMMIT", nodeId);
      rg.m_nodes.clear(nodeId);
      signal->theData[0] = 9999;
      signal->theData[1] = ERROR_INSERT_VALUE;
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          5000, 2);
    }
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_COMMIT;
  req->transId = trans_ptr.p->m_transId;

  if (rg.m_nodes.get(getOwnNodeId()))
  {
    /*
      To ensure that slave participants register the commit
      first we mask out the master node and send commit signal
      to master last. This is necessary to handle master node
      failure where one of the slaves take over and need to know
      that transaction is to be committed.
    */
    rg.m_nodes.clear(getOwnNodeId());

    sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
               SchemaTransImplReq::SignalLength, JBB);
    sendSignal(reference(), GSN_SCHEMA_TRANS_IMPL_REQ, signal,
               SchemaTransImplReq::SignalLength, JBB);
  }
  else
  {
    /*
      New master had already committed operation
     */
    sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
               SchemaTransImplReq::SignalLength, JBB);
  }
}

void
Dbdict::trans_commit_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  if (hasError(trans_ptr.p->m_error))
  {
    jam();
    // kill nodes that failed COMMIT
    ndbrequire(false);
    return;
  }

  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  bool next = false;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    next = list.next(op_ptr);
  }

  if (next)
  {
    jam();
    trans_commit_next(signal, trans_ptr, op_ptr);
    if (ERROR_INSERTED(6014)) {
      jam();
      CRASH_INSERTION(6014);
    }
    return;
  }
  else
  {
    jam();
    trans_commit_done(signal, trans_ptr);
    if (ERROR_INSERTED(6015)) {
      jam();
      CRASH_INSERTION(6015);
    }
  }
  return;
}

void
Dbdict::trans_commit_done(Signal* signal, SchemaTransPtr trans_ptr)
{
#ifdef MARTIN
  ndbout_c("trans_commit_done");
#endif

  Mutex mutex(signal, c_mutexMgr, trans_ptr.p->m_commit_mutex);
  Callback c = { safe_cast(&Dbdict::trans_commit_mutex_unlocked), trans_ptr.i };
  mutex.unlock(c);
}

void
Dbdict::trans_commit_mutex_unlocked(Signal* signal,
                                    Uint32 transPtrI,
                                    Uint32 ret)
{
  jamEntry();
#ifdef MARTIN
  ndbout_c("trans_commit_mutex_unlocked");
#endif
  SchemaTransPtr trans_ptr;
  c_schemaTransPool.getPtr(trans_ptr, transPtrI);

  trans_ptr.p->m_commit_mutex.release(c_mutexMgr);

  /**
   * Here we should wait for SCHEMA_TRANS_COMMIT_ACK
   *
   * But for now, we proceed and complete transaction
   */
  trans_complete_start(signal, trans_ptr);
}

void
Dbdict::check_partial_trans_complete_start(SchemaTransPtr trans_ptr,
                                   NdbNodeBitmask &nodes)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
  {
    /*
      A new master is in the process of committing a
      transaction taken over from the failed master.
      Check if any nodes should be skipped because they
      have already completed the operation
    */
    for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
      NodeRecordPtr nodePtr;
#ifdef VM_TRACE
      ndbout_c("Node %u", i);
#endif
      if (trans_ptr.p->m_nodes.get(i))
      {
        c_nodes.getPtr(nodePtr, i);
#ifdef VM_TRACE
        ndbout_c("Checking node %u(%u,%u)", nodePtr.i, nodePtr.p->recoveryState, nodePtr.p->takeOverConf.trans_state);
#endif
        if (nodePtr.p->takeOverConf.trans_state >= SchemaTrans::TS_FLUSH_COMPLETE)
        {
#ifdef VM_TRACE
          ndbout_c("Skipping TS_FLUSH_COMPLETE of node %u", i);
#endif
          nodes.clear(i);
        }
      }
    }
  }
}

void
Dbdict::trans_complete_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();

  if (ERROR_INSERTED(6019))
  {
    jam();
    NodeReceiverGroup rg(CMVMI, c_aliveNodes);
    signal->theData[0] = 9999;
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

#ifdef MARTIN
  ndbout_c("trans_complete_start %u", trans_ptr.p->trans_key);
#endif
  trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_COMPLETE;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  check_partial_trans_complete_start(trans_ptr, nodes);
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6148))
  {
    jam();
    /*
      Simulate slave missing flushing complete last op
    */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_FLUSH_COMPLETE", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
			5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = RNIL;
  req->requestInfo = SchemaTransImplReq::RT_FLUSH_COMPLETE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_complete_first(Signal * signal, SchemaTransPtr trans_ptr)
{
  jam();

  if (ERROR_INSERTED(6020))
  {
    jam();
    NodeReceiverGroup rg(CMVMI, c_aliveNodes);
    signal->theData[0] = 9999;
    sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

  trans_ptr.p->m_state = SchemaTrans::TS_COMPLETING;

  bool first = false;
  SchemaOpPtr op_ptr;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    first = list.first(op_ptr);
  }

  if (first)
  {
    jam();
    trans_complete_next(signal, trans_ptr, op_ptr);
  }
  else
  {
    jam();
    trans_complete_done(signal, trans_ptr);
  }
}

void
Dbdict::trans_complete_next(Signal* signal,
                            SchemaTransPtr trans_ptr, SchemaOpPtr op_ptr)
{
  op_ptr.p->m_state = SchemaOp::OS_COMPLETING;
  trans_ptr.p->m_curr_op_ptr_i = op_ptr.i;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6149))
  {
    jam();
    /*
      Simulate slave missing completing last op
    */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_COMPLETE", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
			5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = op_ptr.p->op_key;
  req->requestInfo = SchemaTransImplReq::RT_COMPLETE;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::trans_complete_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  if (hasError(trans_ptr.p->m_error))
  {
    jam();
    // kill nodes that failed COMMIT
    ndbrequire(false);
    return;
  }

  SchemaOpPtr op_ptr;
  c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);

  bool next = false;
  {
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    next = list.next(op_ptr);
  }

  if (next)
  {
    jam();
    trans_complete_next(signal, trans_ptr, op_ptr);
    return;
  }
  else
  {
    jam();
    trans_complete_done(signal, trans_ptr);
  }
  return;
}

void
Dbdict::trans_complete_done(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();
  trans_end_start(signal, trans_ptr);
}

void
Dbdict::trans_end_start(Signal* signal, SchemaTransPtr trans_ptr)
{
  ndbrequire(trans_ptr.p->m_state != SchemaTrans::TS_ENDING);
  trans_ptr.p->m_state = SchemaTrans::TS_ENDING;

  trans_ptr.p->m_nodes.bitAND(c_aliveNodes);
  NdbNodeBitmask nodes = trans_ptr.p->m_nodes;
  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  if (ERROR_INSERTED(6150))
  {
    jam();
    /*
      Simulate slave missing ending transaction
    */
    jam();
    Uint32 nodeId = rand() % MAX_NDB_NODES;
    while(nodeId == c_masterNodeId || (!rg.m_nodes.get(nodeId)))
      nodeId = rand() % MAX_NDB_NODES;

    infoEvent("Simulating node %u missing RT_END", nodeId);
    rg.m_nodes.clear(nodeId);
    signal->theData[0] = 9999;
    signal->theData[1] = ERROR_INSERT_VALUE;
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
			5000, 2);
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = RNIL;
  req->requestInfo = SchemaTransImplReq::RT_END;
  req->transId = trans_ptr.p->m_transId;
  sendSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
             SchemaTransImplReq::SignalLength, JBB);
}

void
Dbdict::check_partial_trans_end_recv_reply(SchemaTransPtr trans_ptr)
{
  jam();
  NodeRecordPtr ownNodePtr;
  c_nodes.getPtr(ownNodePtr, getOwnNodeId());
  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER &&
      trans_ptr.p->check_partial_rollforward &&
      trans_ptr.p->ressurected_op)
  {
    /*
      We created an operation in new master just to able to
      complete operation on other slaves. We need to release
      this ressurected operation explictely.
     */
    jam();
    SchemaOpPtr op_ptr;
    LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
    list.remove(op_ptr);
#ifdef VM_TRACE
    ndbout_c("Releasing ressurected op %u", op_ptr.p->op_key);
#endif
    releaseSchemaOp(op_ptr);
    trans_ptr.p->check_partial_rollforward = false;
  }
}

void
Dbdict::trans_end_recv_reply(Signal* signal, SchemaTransPtr trans_ptr)
{
  // unlock
  const DictLockReq& lockReq = trans_ptr.p->m_lockReq;
  Uint32 rc = dict_lock_unlock(signal, &lockReq);
  debugLockInfo(signal,
                "trans_end_recv_reply unlock",
                rc);

  sendTransClientReply(signal, trans_ptr);
  check_partial_trans_end_recv_reply(trans_ptr);
  releaseSchemaTrans(trans_ptr);
}

void Dbdict::trans_recover(Signal* signal, SchemaTransPtr trans_ptr)
{
  ErrorInfo error;

  jam();
#ifdef VM_TRACE
  ndbout_c("Dbdict::trans_recover trans %u, state %u", trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif

  switch(trans_ptr.p->m_state) {
  case SchemaTrans::TS_INITIAL:
    jam();
  case SchemaTrans::TS_STARTING:
    jam();
  case SchemaTrans::TS_STARTED:
    jam();
    if (trans_ptr.p->m_rollback_op == 0)
    {
      /*
        No parsed operations found
       */
      jam();
#ifdef VM_TRACE
      ndbout_c("Dbdict::trans_recover: ENDING START, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
      setError(trans_ptr.p->m_error, SchemaTransEndRep::TransAborted, __LINE__);
      trans_end_start(signal, trans_ptr);
      return;
    }
  case SchemaTrans::TS_PARSING:
    jam();
    setError(trans_ptr.p->m_error, SchemaTransEndRep::TransAborted, __LINE__);
    trans_abort_parse_start(signal, trans_ptr);
    return;
  case SchemaTrans::TS_ABORTING_PARSE:
  {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict::trans_recover: ABORTING_PARSE, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    setError(trans_ptr.p->m_error, SchemaTransEndRep::TransAborted, __LINE__);
    SchemaOpPtr op_ptr;
    c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);
    // Revert operation state to restart abort
    op_ptr.p->m_state = SchemaOp::OS_PREPARED;
    trans_abort_parse_next(signal, trans_ptr, op_ptr);
    return;
  }
  case SchemaTrans::TS_PREPARING:
    jam();
    if (trans_ptr.p->m_master_recovery_state == SchemaTrans::TRS_ROLLFORWARD)
      goto flush_commit;
    setError(trans_ptr.p->m_error, SchemaTransEndRep::TransAborted, __LINE__);
    trans_abort_prepare_start(signal, trans_ptr);
    return;
  case SchemaTrans::TS_ABORTING_PREPARE:
  {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict::trans_recover: ABORTING PREPARE, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    setError(trans_ptr.p->m_error, SchemaTransEndRep::TransAborted, __LINE__);
    SchemaOpPtr op_ptr;
    c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);
    // Revert operation state to restart abort
    op_ptr.p->m_state = SchemaOp::OS_PREPARED;
    trans_abort_prepare_next(signal, trans_ptr, op_ptr);
    return;
  }
  case SchemaTrans::TS_FLUSH_COMMIT:
    flush_commit:
    /*
       Flush commit any unflushed slaves
    */
    jam();
    trans_commit_start(signal, trans_ptr);
    return;
  case SchemaTrans::TS_COMMITTING:
  {
    if (trans_ptr.p->m_highest_trans_state <= SchemaTrans::TS_COMMITTING)
    {
      jam();
      /*
        Commit any uncommited operations
      */
      jam();
      SchemaOpPtr op_ptr;
      c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);
      if (op_ptr.p->m_state < SchemaOp::OS_COMMITTED)
      {
        jam();
        trans_commit_next(signal, trans_ptr, op_ptr);
        return;
      }
    }
    /*
      We have started flushing commits
    */
    jam();
    NodeRecordPtr masterNodePtr;
    c_nodes.getPtr(masterNodePtr, c_masterNodeId);
    if (masterNodePtr.p->recoveryState
        == NodeRecord::RS_PARTIAL_ROLLFORWARD)
    {
      /*
        New master has flushed commit and
        thus have released commit mutex
      */
      jam();
      trans_commit_mutex_unlocked(signal, trans_ptr.i, 0);
      return;
    }
    else
    {
      /*
        New master has not flushed commit and
        thus have to release commit mutex
      */
      jam();
      trans_commit_done(signal, trans_ptr);
      return;
    }
  }
  case SchemaTrans::TS_FLUSH_COMPLETE:
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict::trans_recover: COMMITTING DONE, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    trans_complete_done(signal, trans_ptr);
    return;
  case SchemaTrans::TS_COMPLETING:
  {
    /*
      Complete any uncommited operations
    */
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict::trans_recover: COMPLETING, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    SchemaOpPtr op_ptr;
    c_schemaOpPool.getPtr(op_ptr, trans_ptr.p->m_curr_op_ptr_i);
    if (op_ptr.p->m_state < SchemaOp::OS_COMPLETED)
    {
      jam();
      trans_complete_next(signal, trans_ptr, op_ptr);
      return;
    }
  }
  case SchemaTrans::TS_ENDING:
    /*
      End any pending slaves
     */
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict::trans_recover: ENDING, trans %u(0x%8x), state %u", trans_ptr.i, (uint)trans_ptr.p->trans_key, trans_ptr.p->m_state);
#endif
    trans_end_start(signal, trans_ptr);
    return;
  default:
    jam();
  }
  ndbassert(false);
}


// participant

void
Dbdict::execSCHEMA_TRANS_IMPL_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SchemaTransImplReq reqCopy =
    *(const SchemaTransImplReq*)signal->getDataPtr();
  const SchemaTransImplReq *req = &reqCopy;
  const Uint32 rt = DictSignal::getRequestType(req->requestInfo);

  if (rt == SchemaTransImplReq::RT_START)
  {
    jam();
    if (signal->getLength() < SchemaTransImplReq::SignalLengthStart)
    {
      jam();
      reqCopy.start.objectId = getFreeObjId();
    }
    slave_run_start(signal, req);
    return;
  }

  ErrorInfo error;
  SchemaTransPtr trans_ptr;
  const Uint32 trans_key = req->transKey;
  if (!findSchemaTrans(trans_ptr, trans_key))
  {
    jam();
    setError(error, SchemaTransImplRef::InvalidTransKey, __LINE__);
    goto err;
  }

#ifdef MARTIN
  char buf[256];
  switch(rt) {
  case(SchemaTransImplReq::RT_START):
    sprintf(buf, " RequestType: RT_START");
    break;
  case(SchemaTransImplReq::RT_PARSE):
    sprintf(buf, " RequestType: RT_PARSE");
    break;
  case(SchemaTransImplReq::RT_FLUSH_PREPARE):
    sprintf(buf, " RequestType: RT_FLUSH_PREPARE");
    break;
  case(SchemaTransImplReq::RT_PREPARE):
    sprintf(buf, " RequestType: RT_PREPARE");
    break;
  case(SchemaTransImplReq::RT_ABORT_PARSE):
    sprintf(buf, " RequestType: RT_ABORT_PARSE");
    break;
  case(SchemaTransImplReq::RT_ABORT_PREPARE):
    sprintf(buf, " RequestType: RT_ABORT_PREPARE");
    break;
  case(SchemaTransImplReq::RT_FLUSH_COMMIT):
    sprintf(buf, " RequestType: RT_FLUSH_COMMIT");
    break;
  case(SchemaTransImplReq::RT_COMMIT):
    sprintf(buf, " RequestType: RT_COMMIT");
    break;
  case(SchemaTransImplReq::RT_FLUSH_COMPLETE):
    sprintf(buf, " RequestType: RT_FLUSH_COMPLETE");
    break;
  case(SchemaTransImplReq::RT_COMPLETE):
    sprintf(buf, " RequestType: RT_COMPLETE");
    break;
  case(SchemaTransImplReq::RT_END):
    sprintf(buf, " RequestType: RT_END");
    break;
  }
  infoEvent("Dbdict::execSCHEMA_TRANS_IMPL_REQ: %s", buf);
#endif

  /**
   * Check *transaction* request
   */
  switch(rt){
  case SchemaTransImplReq::RT_PARSE:
    jam();
    slave_run_parse(signal, trans_ptr, req);
    return;
  case SchemaTransImplReq::RT_END:
  case SchemaTransImplReq::RT_FLUSH_PREPARE:
  case SchemaTransImplReq::RT_FLUSH_COMMIT:
  case SchemaTransImplReq::RT_FLUSH_COMPLETE:
    jam();
    jamLine(rt);
    slave_run_flush(signal, trans_ptr, req);
    return;
  default:
    break;
  }

  SchemaOpPtr op_ptr;
  if (!findSchemaOp(op_ptr, req->opKey))
  {
    jam();
    // wl3600_todo better error no
    setError(error, SchemaTransImplRef::InvalidTransKey, __LINE__);
    goto err;
  }

  {
    const OpInfo info = getOpInfo(op_ptr);
    switch(rt){
    case SchemaTransImplReq::RT_START:
    case SchemaTransImplReq::RT_PARSE:
    case SchemaTransImplReq::RT_FLUSH_PREPARE:
    case SchemaTransImplReq::RT_FLUSH_COMMIT:
    case SchemaTransImplReq::RT_FLUSH_COMPLETE:
      ndbrequire(false); // handled above
    case SchemaTransImplReq::RT_PREPARE:
      jam();
      op_ptr.p->m_state = SchemaOp::OS_PREPARING;
      (this->*(info.m_prepare))(signal, op_ptr);
      return;
    case SchemaTransImplReq::RT_ABORT_PARSE:
      jam();
      ndbrequire(op_ptr.p->nextList == RNIL);
      op_ptr.p->m_state = SchemaOp::OS_ABORTING_PARSE;
      (this->*(info.m_abortParse))(signal, op_ptr);
      trans_log_schema_op_abort(op_ptr);
      if (!trans_ptr.p->m_isMaster)
      {
        trans_ptr.p->m_state = SchemaTrans::TS_ABORTING_PARSE;
        /**
         * Remove op (except at coordinator
         */
        LocalSchemaOp_list list(c_schemaOpPool, trans_ptr.p->m_op_list);
        list.remove(op_ptr);
        releaseSchemaOp(op_ptr);
      }
      return;
    case SchemaTransImplReq::RT_ABORT_PREPARE:
      jam();
      op_ptr.p->m_state = SchemaOp::OS_ABORTING_PREPARE;
      (this->*(info.m_abortPrepare))(signal, op_ptr);
      if (!trans_ptr.p->m_isMaster)
        trans_ptr.p->m_state = SchemaTrans::TS_ABORTING_PREPARE;
      return;
    case SchemaTransImplReq::RT_COMMIT:
      jam();
      op_ptr.p->m_state = SchemaOp::OS_COMMITTING;
      (this->*(info.m_commit))(signal, op_ptr);
      return;
    case SchemaTransImplReq::RT_COMPLETE:
      jam();
      op_ptr.p->m_state = SchemaOp::OS_COMPLETING;
      (this->*(info.m_complete))(signal, op_ptr);
      trans_log_schema_op_complete(op_ptr);
      return;
    }
  }

  return;
err:
  ndbrequire(false);
}

void
Dbdict::slave_run_start(Signal *signal, const SchemaTransImplReq* req)
{
  ErrorInfo error;
  SchemaTransPtr trans_ptr;
  const Uint32 trans_key = req->transKey;

  Uint32 objId = req->start.objectId;
  if (check_read_obj(objId,0) == 0)
  { /* schema file id already in use */
    jam();
    setError(error, CreateTableRef::NoMoreTableRecords, __LINE__);
    goto err;
  }

  if (req->senderRef != reference())
  {
    jam();
    if (!seizeSchemaTrans(trans_ptr, trans_key))
    {
      jam();
      setError(error, SchemaTransImplRef::TooManySchemaTrans, __LINE__);
      goto err;
    }
    trans_ptr.p->m_clientRef = req->start.clientRef;
    trans_ptr.p->m_transId = req->transId;
    trans_ptr.p->m_isMaster = false;
    trans_ptr.p->m_masterRef = req->senderRef;
    trans_ptr.p->m_requestInfo = req->requestInfo;
    trans_ptr.p->m_state = SchemaTrans::TS_STARTED;

    ndbrequire((req->requestInfo & DictSignal::RF_LOCAL_TRANS) == 0);
    trans_ptr.p->m_nodes = c_aliveNodes;
  }
  else
  {
    jam();
    // this branch does nothing but is convenient for signal pong
    ndbrequire(findSchemaTrans(trans_ptr, req->transKey));
  }

  trans_ptr.p->m_obj_id = objId;
  trans_log(trans_ptr);

  sendTransConf(signal, trans_ptr);
  return;

err:
  SchemaTrans tmp_trans;
  trans_ptr.i = RNIL;
  trans_ptr.p = &tmp_trans;
  trans_ptr.p->trans_key = trans_key;
  trans_ptr.p->m_masterRef = req->senderRef;
  setError(trans_ptr.p->m_error, error);
  sendTransRef(signal, trans_ptr);
}

void
Dbdict::slave_run_parse(Signal *signal,
                        SchemaTransPtr trans_ptr,
                        const SchemaTransImplReq* req)
{
  SchemaOpPtr op_ptr;
  D("slave_run_parse");

  const Uint32 op_key = req->opKey;
  const Uint32 gsn = req->parse.gsn;
  const Uint32 requestInfo = req->requestInfo;
  const OpInfo& info = *findOpInfo(gsn);

  // signal data contains impl_req
  const Uint32* src = signal->getDataPtr() + SchemaTransImplReq::SignalLength;
  const Uint32 len = info.m_impl_req_length;

  SectionHandle handle(this, signal);

  ndbrequire(op_key != RNIL);
  ErrorInfo error;
  if (trans_ptr.p->m_isMaster)
  {
    jam();
    // this branch does nothing but is convenient for signal pong

    //XXX Check if op == last op in trans
    findSchemaOp(op_ptr, op_key);
    ndbrequire(!op_ptr.isNull());

    OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
    const Uint32* dst = oprec_ptr.p->m_impl_req_data;
    ndbrequire(memcmp(dst, src, len << 2) == 0);
  }
  else
  {
    if (checkSingleUserMode(trans_ptr.p->m_clientRef))
    {
      jam();
      setError(error, AlterTableRef::SingleUser, __LINE__);
    }
    else if (seizeSchemaOp(trans_ptr, op_ptr, op_key, info))
    {
      jam();

      DictSignal::addRequestExtra(op_ptr.p->m_requestInfo, requestInfo);
      DictSignal::addRequestFlags(op_ptr.p->m_requestInfo, requestInfo);

      OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
      Uint32* dst = oprec_ptr.p->m_impl_req_data;
      memcpy(dst, src, len << 2);

      op_ptr.p->m_state = SchemaOp::OS_PARSING;
      (this->*(info.m_parse))(signal, false, op_ptr, handle, error);
    } else {
      jam();
      setError(error, SchemaTransImplRef::TooManySchemaOps, __LINE__);
    }
  }

  // parse must consume but not release signal sections
  releaseSections(handle);

  if (hasError(error))
  {
    jam();
    setError(trans_ptr, error);
    sendTransRef(signal, trans_ptr);
    return;
  }
  sendTransConf(signal, op_ptr);
}

void
Dbdict::slave_run_flush(Signal *signal,
                        SchemaTransPtr trans_ptr,
                        const SchemaTransImplReq* req)
{
  bool do_flush = false;
  const Uint32 rt = DictSignal::getRequestType(req->requestInfo);
  const bool master = trans_ptr.p->m_isMaster;

  jamLine(trans_ptr.p->m_state);
  switch(rt){
  case SchemaTransImplReq::RT_FLUSH_PREPARE:
    if (master)
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_FLUSH_PREPARE);
    }
    else
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_STARTED ||
                 trans_ptr.p->m_state == SchemaTrans::TS_ABORTING_PARSE);
      trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_PREPARE;
    }
    do_flush = trans_ptr.p->m_flush_prepare;
    break;
  case SchemaTransImplReq::RT_FLUSH_COMMIT:
    if (master)
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_FLUSH_COMMIT);
    }
    else
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_PREPARING);
      trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_COMMIT;
    }
    do_flush = trans_ptr.p->m_flush_commit;
    break;
  case SchemaTransImplReq::RT_FLUSH_COMPLETE:
    if (master)
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_FLUSH_COMPLETE);
    }
    else
    {
      jam();
      ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_COMMITTING);
      trans_ptr.p->m_state = SchemaTrans::TS_FLUSH_COMPLETE;
    }
    do_flush = trans_ptr.p->m_flush_complete;
    break;
  case SchemaTransImplReq::RT_END:
    /**
     * No state check here, cause we get here regardless if transaction
     *   succeded or not...
     */
    trans_ptr.p->m_state = SchemaTrans::TS_ENDING;
    do_flush = trans_ptr.p->m_flush_end;
    break;
  default:
    ndbrequire(false);
  }

  trans_log(trans_ptr);

#if 1
  if (do_flush == false)
  {
    /**
     * If no operations needs durable complete phase...
     *   skip it and leave pending trans...
     *   will be flushed on next schema trans
     *   or will be found in state COMMIT
     */
    slave_writeSchema_conf(signal, trans_ptr.p->trans_key, 0);
    return;
  }
#endif

  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  ndbrequire(c_writeSchemaRecord.inUse == false);
  c_writeSchemaRecord.inUse = true;
  c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
  c_writeSchemaRecord.newFile = false;
  c_writeSchemaRecord.firstPage = 0;
  c_writeSchemaRecord.noOfPages = xsf->noOfPages;

  c_writeSchemaRecord.m_callback.m_callbackData = trans_ptr.p->trans_key;
  c_writeSchemaRecord.m_callback.m_callbackFunction =
    safe_cast(&Dbdict::slave_writeSchema_conf);

  for(Uint32 i = 0; i<xsf->noOfPages; i++)
    computeChecksum(xsf, i);

  startWriteSchemaFile(signal);
}

void
Dbdict::slave_writeSchema_conf(Signal* signal,
                               Uint32 trans_key,
                               Uint32 ret)
{
  jamEntry();
  ndbrequire(ret == 0);
  SchemaTransPtr trans_ptr;
  ndbrequire(findSchemaTrans(trans_ptr, trans_key));

  bool release = false;
  if (!trans_ptr.p->m_isMaster)
  {
    switch(trans_ptr.p->m_state){
    case SchemaTrans::TS_FLUSH_PREPARE:
      jam();
      trans_ptr.p->m_state = SchemaTrans::TS_PREPARING;
      break;
    case SchemaTrans::TS_FLUSH_COMMIT:
    {
      jam();
      trans_ptr.p->m_state = SchemaTrans::TS_COMMITTING;
      // Take commit lock
      Mutex mutex(signal, c_mutexMgr, trans_ptr.p->m_commit_mutex);
      Callback c = { safe_cast(&Dbdict::slave_commit_mutex_locked), trans_ptr.i };
      bool ok = mutex.lock(c);
      ndbrequire(ok);
      return;
    }
    case SchemaTrans::TS_FLUSH_COMPLETE:
    {
      jam();
      trans_ptr.p->m_state = SchemaTrans::TS_COMPLETING;
     // Release commit lock
      Mutex mutex(signal, c_mutexMgr, trans_ptr.p->m_commit_mutex);
      Callback c = { safe_cast(&Dbdict::slave_commit_mutex_unlocked), trans_ptr.i };
      mutex.unlock(c);
      return;
    }
    case SchemaTrans::TS_ENDING:
      jam();
      release = true;
      break;
    default:
      jamLine(trans_ptr.p->m_state);
      ndbrequire(false);
    }
  }

  sendTransConfRelease(signal, trans_ptr);
}

void
Dbdict::slave_commit_mutex_locked(Signal* signal,
                                  Uint32 transPtrI,
                                  Uint32 ret)
{
  jamEntry();
#ifdef MARTIN
  ndbout_c("slave_commit_mutex_locked");
#endif
  SchemaTransPtr trans_ptr;
  c_schemaTransPool.getPtr(trans_ptr, transPtrI);

  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_COMMITTING);
  sendTransConfRelease(signal, trans_ptr);
}

void
Dbdict::slave_commit_mutex_unlocked(Signal* signal,
                                    Uint32 transPtrI,
                                    Uint32 ret)
{
  jamEntry();
#ifdef MARTIN
  ndbout_c("slave_commit_mutex_unlocked");
#endif
  SchemaTransPtr trans_ptr;
  c_schemaTransPool.getPtr(trans_ptr, transPtrI);

  trans_ptr.p->m_commit_mutex.release(c_mutexMgr);

  ndbrequire(trans_ptr.p->m_state == SchemaTrans::TS_COMPLETING);
  sendTransConfRelease(signal, trans_ptr);
}

void Dbdict::sendTransConfRelease(Signal*signal, SchemaTransPtr trans_ptr)
{
  jam();
  sendTransConf(signal, trans_ptr);

  if ((!trans_ptr.p->m_isMaster) &&
      trans_ptr.p->m_state == SchemaTrans::TS_ENDING)
  {
    jam();
    releaseSchemaTrans(trans_ptr);
  }
}

void
Dbdict::update_op_state(SchemaOpPtr op_ptr)
{
  switch(op_ptr.p->m_state){
  case SchemaOp::OS_PARSE_MASTER:
    break;
  case SchemaOp::OS_PARSING:
    op_ptr.p->m_state = SchemaOp::OS_PARSED;
    break;
  case SchemaOp::OS_PARSED:
    ndbrequire(false);
  case SchemaOp::OS_PREPARING:
    op_ptr.p->m_state = SchemaOp::OS_PREPARED;
    break;
  case SchemaOp::OS_PREPARED:
    ndbrequire(false);
  case SchemaOp::OS_ABORTING_PREPARE:
    op_ptr.p->m_state = SchemaOp::OS_ABORTED_PREPARE;
    break;
  case SchemaOp::OS_ABORTED_PREPARE:
    ndbrequire(false);
  case SchemaOp::OS_ABORTING_PARSE:
    break;
    //case SchemaOp::OS_ABORTED_PARSE:  // Not used, op released
  case SchemaOp::OS_COMMITTING:
    op_ptr.p->m_state = SchemaOp::OS_COMMITTED;
    break;
  case SchemaOp::OS_COMMITTED:
    ndbrequire(false);
  case SchemaOp::OS_COMPLETING:
    op_ptr.p->m_state = SchemaOp::OS_COMPLETED;
    break;
  case SchemaOp::OS_COMPLETED:
    ndbrequire(false);
  }
}

void
Dbdict::sendTransConf(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  update_op_state(op_ptr);
  sendTransConf(signal, trans_ptr);
}

void
Dbdict::sendTransConf(Signal* signal, SchemaTransPtr trans_ptr)
{
  ndbrequire(!trans_ptr.isNull());
  ndbrequire(signal->getNoOfSections() == 0);

  SchemaTransImplConf* conf =
    (SchemaTransImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->transKey = trans_ptr.p->trans_key;

  const Uint32 masterRef = trans_ptr.p->m_masterRef;

  if (ERROR_INSERTED(6103)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;

    // delay CONF

    Uint32* data = &signal->theData[0];
    memmove(&data[2], &data[0], SchemaTransImplConf::SignalLength << 2);
    data[0] = 6103;
    data[1] = masterRef;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        5000, 2 + SchemaTransImplConf::SignalLength);
    return;
  }

  sendSignal(masterRef, GSN_SCHEMA_TRANS_IMPL_CONF, signal,
             SchemaTransImplConf::SignalLength, JBB);
}

void
Dbdict::sendTransRef(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  // must have error and already propagated to trans
  ndbrequire(hasError(op_ptr.p->m_error));
  ndbrequire(hasError(trans_ptr.p->m_error));

  update_op_state(op_ptr);

  sendTransRef(signal, trans_ptr);
}

void
Dbdict::sendTransRef(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("sendTransRef");
  ndbrequire(hasError(trans_ptr.p->m_error));

  // trans is not defined on RT_BEGIN REF

  SchemaTransImplRef* ref =
    (SchemaTransImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->transKey = trans_ptr.p->trans_key;
  getError(trans_ptr.p->m_error, ref);

  // erro has been reported, clear it
  resetError(trans_ptr.p->m_error);

  const Uint32 masterRef = trans_ptr.p->m_masterRef;
  sendSignal(masterRef, GSN_SCHEMA_TRANS_IMPL_REF, signal,
             SchemaTransImplRef::SignalLength, JBB);
}

void
Dbdict::trans_log(SchemaTransPtr trans_ptr)
{
  Uint32 objectId = trans_ptr.p->m_obj_id;
  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  SchemaFile::TableEntry * entry = getTableEntry(xsf, objectId);

  jam();
  jamLine(trans_ptr.p->m_state);
  jamLine(entry->m_tableState);
  switch(trans_ptr.p->m_state){
  case SchemaTrans::TS_STARTED:
  case SchemaTrans::TS_STARTING:{
    jam();
    Uint32 version = entry->m_tableVersion;
    Uint32 new_version = create_obj_inc_schema_version(version);
    entry->init();
    entry->m_tableState = SchemaFile::SF_STARTED;
    entry->m_tableVersion = new_version;
    entry->m_tableType = DictTabInfo::SchemaTransaction;
    entry->m_info_words = 0;
    entry->m_gcp = 0;
    entry->m_transId = trans_ptr.p->m_transId;
    break;
  }
  case SchemaTrans::TS_FLUSH_PREPARE:
    jam();
    ndbrequire(entry->m_tableState == SchemaFile::SF_STARTED);
    entry->m_tableState = SchemaFile::SF_PREPARE;
    break;
  case SchemaTrans::TS_ABORTING_PREPARE:
    jam();
    ndbrequire(entry->m_tableState == SchemaFile::SF_PREPARE);
    entry->m_tableState = SchemaFile::SF_ABORT;
    break;
  case SchemaTrans::TS_FLUSH_COMMIT:
    jam();
    ndbrequire(entry->m_tableState == SchemaFile::SF_PREPARE);
    entry->m_tableState = SchemaFile::SF_COMMIT;
    break;
  case SchemaTrans::TS_FLUSH_COMPLETE:
    jam();
    ndbrequire(entry->m_tableState == SchemaFile::SF_COMMIT);
    entry->m_tableState = SchemaFile::SF_COMPLETE;
    break;
  case SchemaTrans::TS_ENDING:
    jam();
    entry->m_transId = 0;
    entry->m_tableState = SchemaFile::SF_UNUSED;
    break;
  default:
    ndbrequire(false);
  }
}

/**
 * Schema operation logging (SchemaFile)
 */
Uint32
Dbdict::trans_log_schema_op(SchemaOpPtr op_ptr,
                            Uint32 objectId,
                            const SchemaFile::TableEntry * newEntry)
{
  jam();

  XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
  SchemaFile::TableEntry * oldEntry = getTableEntry(xsf, objectId);

  if (oldEntry->m_transId != 0)
  {
    jam();
    return DropTableRef::ActiveSchemaTrans; // XXX todo add general code
  }

  SchemaFile::TableEntry tmp = * newEntry;
  bool restart = op_ptr.p->m_restart;
  switch((SchemaFile::EntryState)newEntry->m_tableState){
  case SchemaFile::SF_CREATE:
    ndbrequire(restart || oldEntry->m_tableState == SchemaFile::SF_UNUSED);
    break;
  case SchemaFile::SF_ALTER:
    ndbrequire(restart || oldEntry->m_tableState == SchemaFile::SF_IN_USE);
    tmp = * oldEntry;
    tmp.m_info_words = newEntry->m_info_words;
    tmp.m_tableVersion = newEntry->m_tableVersion;
    break;
  case SchemaFile::SF_DROP:
    ndbrequire(restart || oldEntry->m_tableState == SchemaFile::SF_IN_USE);
    tmp = *oldEntry;
    tmp.m_tableState = SchemaFile::SF_DROP;
    break;
  default:
    jamLine(newEntry->m_tableState);
    ndbrequire(false);
  }

  tmp.m_tableState = newEntry->m_tableState;
  tmp.m_transId = newEntry->m_transId;
  op_ptr.p->m_orig_entry_id = objectId;
  op_ptr.p->m_orig_entry = * oldEntry;

  * oldEntry = tmp;

  if (op_ptr.p->m_restart != 1)
  {
    jam();

    SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
    trans_ptr.p->m_flush_prepare |= true;
    trans_ptr.p->m_flush_commit |= true;
  }

  return 0;
}

void
Dbdict::trans_log_schema_op_abort(SchemaOpPtr op_ptr)
{
  Uint32 objectId = op_ptr.p->m_orig_entry_id;
  if (objectId != RNIL)
  {
    jam();
    op_ptr.p->m_orig_entry_id = RNIL;
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    SchemaFile::TableEntry * entry = getTableEntry(xsf, objectId);
    * entry = op_ptr.p->m_orig_entry;
  }
}

void
Dbdict::trans_log_schema_op_complete(SchemaOpPtr op_ptr)
{
  Uint32 objectId = op_ptr.p->m_orig_entry_id;
  if (objectId != RNIL)
  {
    jam();
    op_ptr.p->m_orig_entry_id = RNIL;
    XSchemaFile * xsf = &c_schemaFile[SchemaRecord::NEW_SCHEMA_FILE];
    SchemaFile::TableEntry * entry = getTableEntry(xsf, objectId);
    switch((SchemaFile::EntryState)entry->m_tableState){
    case SchemaFile::SF_CREATE:
      entry->m_tableState = SchemaFile::SF_IN_USE;
      break;
    case SchemaFile::SF_ALTER:
      entry->m_tableState = SchemaFile::SF_IN_USE;
      break;
    case SchemaFile::SF_DROP:
      entry->m_tableState = SchemaFile::SF_UNUSED;
      break;
    default:
      jamLine(entry->m_tableState);
      ndbrequire(false);
    }
    entry->m_transId = 0;
  }
}

// reply to trans client for begin/end trans

void
Dbdict::sendTransClientReply(Signal* signal, SchemaTransPtr trans_ptr)
{
  const Uint32 clientFlags = trans_ptr.p->m_clientFlags;
  D("sendTransClientReply" << hex << V(clientFlags));
  D("c_rope_pool free: " << c_rope_pool.getNoOfFree());

  Uint32 receiverRef = 0;
  Uint32 transId = 0;
  NodeRecordPtr ownNodePtr;

  c_nodes.getPtr(ownNodePtr, getOwnNodeId());

  if (!(clientFlags & TransClient::TakeOver)) {
    receiverRef = trans_ptr.p->m_clientRef;
    transId = trans_ptr.p->m_transId;
  } else {
    jam();
    receiverRef = reference();
    transId = trans_ptr.p->m_takeOverTxKey;
  }

  if (trans_ptr.p->m_clientState == TransClient::BeginReq) {
    if (!hasError(trans_ptr.p->m_error)) {
      jam();
      SchemaTransBeginConf* conf =
        (SchemaTransBeginConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->transId = transId;
      conf->transKey = trans_ptr.p->trans_key;
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_BEGIN_CONF, signal,
                 SchemaTransBeginConf::SignalLength, JBB);
    } else {
      jam();
      SchemaTransBeginRef* ref =
        (SchemaTransBeginRef*)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->transId = transId;
      getError(trans_ptr.p->m_error, ref);
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_BEGIN_REF, signal,
                 SchemaTransBeginRef::SignalLength, JBB);
    }
    resetError(trans_ptr);
    trans_ptr.p->m_clientState = TransClient::BeginReply;
    return;
  }

  if (trans_ptr.p->m_clientState == TransClient::EndReq) {
    if (!hasError(trans_ptr.p->m_error)) {
      jam();
      SchemaTransEndConf* conf =
        (SchemaTransEndConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->transId = transId;
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_END_CONF, signal,
                 SchemaTransEndConf::SignalLength, JBB);
    } else {
      jam();
      SchemaTransEndRef* ref =
        (SchemaTransEndRef*)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->transId = transId;
      getError(trans_ptr.p->m_error, ref);
      ref->masterNodeId = c_masterNodeId;
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_END_REF, signal,
                 SchemaTransEndRef::SignalLength, JBB);
    }
    resetError(trans_ptr);
    trans_ptr.p->m_clientState = TransClient::EndReply;
    if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
    {
      /*
        New master was taking over transaction
       */
      jam();
      goto send_node_fail_rep;
    }
    return;
  }

  if (ownNodePtr.p->nodeState == NodeRecord::NDB_MASTER_TAKEOVER)
  {
    /*
      New master was taking over transaction
      Report transaction outcome to client
     */
    jam();
    {
      SchemaTransEndRep* rep =
        (SchemaTransEndRep*)signal->getDataPtrSend();
      rep->senderRef = reference();
      rep->transId = transId;
      if (hasError(trans_ptr.p->m_error))
        getError(trans_ptr.p->m_error, rep);
      else
      {
        rep->errorCode = 0;
        rep->errorLine = 0;
        rep->errorNodeId = 0;
      }
      rep->masterNodeId = c_masterNodeId;
#ifdef VM_TRACE
      ndbout_c("Dbdict::sendTransClientReply: sending GSN_SCHEMA_TRANS_END_REP to 0x%8x", receiverRef);
#endif
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_END_REP, signal,
                 SchemaTransEndRep::SignalLength, JBB);
    }
send_node_fail_rep:
    /*
      Continue with NODE_FAILREP
    */
    send_nf_complete_rep(signal, &ownNodePtr.p->nodeFailRep);
  }
}


// DICT as schema trans client

bool
Dbdict::seizeTxHandle(TxHandlePtr& tx_ptr)
{
  Uint32 tx_key = c_opRecordSequence + 1;
  if (c_txHandleHash.seize(tx_ptr)) {
    jam();
    new (tx_ptr.p) TxHandle();
    tx_ptr.p->tx_key = tx_key;
    c_txHandleHash.add(tx_ptr);
    tx_ptr.p->m_magic = TxHandle::DICT_MAGIC;
    D("seizeTxHandle" << V(tx_key));
    c_opRecordSequence = tx_key;
    return true;
  }
  tx_ptr.setNull();
  return false;
}

bool
Dbdict::findTxHandle(TxHandlePtr& tx_ptr, Uint32 tx_key)
{
  TxHandle tx_rec(tx_key);
  if (c_txHandleHash.find(tx_ptr, tx_rec)) {
    jam();
    ndbrequire(tx_ptr.p->m_magic == TxHandle::DICT_MAGIC);
    D("findTxHandle" << V(tx_key));
    return true;
  }
  tx_ptr.setNull();
  return false;
}

void
Dbdict::releaseTxHandle(TxHandlePtr& tx_ptr)
{
  D("releaseTxHandle" << V(tx_ptr.p->tx_key));

  ndbrequire(tx_ptr.p->m_magic == TxHandle::DICT_MAGIC);
  tx_ptr.p->m_magic = 0;
  c_txHandleHash.release(tx_ptr);
}

void
Dbdict::beginSchemaTrans(Signal* signal, TxHandlePtr tx_ptr)
{
  D("beginSchemaTrans");
  tx_ptr.p->m_transId = tx_ptr.p->tx_key;

  SchemaTransBeginReq* req =
    (SchemaTransBeginReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::addRequestFlags(requestInfo, tx_ptr.p->m_requestInfo);

  const Uint32 clientFlags = tx_ptr.p->m_clientFlags;
  ndbrequire(!(clientFlags & TransClient::TakeOver));
  req->clientRef = reference();
  req->transId = tx_ptr.p->m_transId;
  req->requestInfo = requestInfo;
  sendSignal(reference(), GSN_SCHEMA_TRANS_BEGIN_REQ, signal,
             SchemaTransBeginReq::SignalLength, JBB);
}

void
Dbdict::endSchemaTrans(Signal* signal, TxHandlePtr tx_ptr, Uint32 flags)
{
  D("endSchemaTrans" << hex << V(flags));

  SchemaTransEndReq* req =
    (SchemaTransEndReq*)signal->getDataPtrSend();

  Uint32 requestInfo = 0;
  DictSignal::addRequestFlags(requestInfo, tx_ptr.p->m_requestInfo);

  const Uint32 clientFlags = tx_ptr.p->m_clientFlags;
  Uint32 transId = 0;
  if (!(clientFlags & TransClient::TakeOver)) {
    transId = tx_ptr.p->m_transId;
  } else {
    jam();
    transId = tx_ptr.p->m_takeOverTransId;
    D("take over mode" << hex << V(transId));
  }

  req->clientRef = reference();
  req->transId = transId;
  req->transKey = tx_ptr.p->m_transKey;
  req->requestInfo = requestInfo;
  req->flags = flags;
  sendSignal(reference(), GSN_SCHEMA_TRANS_END_REQ, signal,
             SchemaTransEndReq::SignalLength, JBB);
}

void
Dbdict::execSCHEMA_TRANS_BEGIN_CONF(Signal* signal)
{
  jamEntry();
  const SchemaTransBeginConf* conf =
    (const SchemaTransBeginConf*)signal->getDataPtr();

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, conf->transId);
  ndbrequire(!tx_ptr.isNull());

  // record trans key
  tx_ptr.p->m_transKey = conf->transKey;

  execute(signal, tx_ptr.p->m_callback, 0);
}

void
Dbdict::execSCHEMA_TRANS_BEGIN_REF(Signal* signal)
{
  jamEntry();
  const SchemaTransBeginRef* ref =
    (const SchemaTransBeginRef*)signal->getDataPtr();

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, ref->transId);
  ndbrequire(!tx_ptr.isNull());

  setError(tx_ptr.p->m_error, ref);
  ndbrequire(ref->errorCode != 0);
  execute(signal, tx_ptr.p->m_callback, ref->errorCode);
}

void
Dbdict::execSCHEMA_TRANS_END_CONF(Signal* signal)
{
  jamEntry();
  const SchemaTransEndConf* conf =
    (const SchemaTransEndConf*)signal->getDataPtr();

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, conf->transId);
  ndbrequire(!tx_ptr.isNull());

  execute(signal, tx_ptr.p->m_callback, 0);
}

void
Dbdict::execSCHEMA_TRANS_END_REF(Signal* signal)
{
  jamEntry();
  const SchemaTransEndRef* ref =
    (const SchemaTransEndRef*)signal->getDataPtr();

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, ref->transId);
  ndbrequire(!tx_ptr.isNull());

  setError(tx_ptr.p->m_error, ref);
  ndbrequire(ref->errorCode != 0);
  execute(signal, tx_ptr.p->m_callback, ref->errorCode);
}

void
Dbdict::execSCHEMA_TRANS_END_REP(Signal* signal)
{
  jamEntry();
  const SchemaTransEndRep* rep =
    (const SchemaTransEndRep*)signal->getDataPtr();

  TxHandlePtr tx_ptr;
  findTxHandle(tx_ptr, rep->transId);
  ndbrequire(!tx_ptr.isNull());

  if (rep->errorCode != 0)
    setError(tx_ptr.p->m_error, rep);
  execute(signal, tx_ptr.p->m_callback, rep->errorCode);
}

// trans client takeover

/*
 * API node failure.  Each trans is taken over by a new TxHandle,
 * unless already running in background by api request (unlikely).
 * All transes are marked with ApiFail.  API node failure handling
 * is complete when last such trans completes.  This is checked
 * in finishApiFail.
 */
void
Dbdict::handleApiFail(Signal* signal,
                      Uint32 failedApiNode)
{
  D("handleApiFail" << V(failedApiNode));

  Uint32 takeOvers = 0;
  SchemaTransPtr trans_ptr;
  c_schemaTransList.first(trans_ptr);
  while (trans_ptr.i != RNIL) {
    jam();
    D("check" << *trans_ptr.p);
    Uint32 clientRef = trans_ptr.p->m_clientRef;

    if (refToNode(clientRef) == failedApiNode)
    {
      jam();
      D("failed" << hex << V(clientRef));

      ndbrequire(!(trans_ptr.p->m_clientFlags & TransClient::ApiFail));
      trans_ptr.p->m_clientFlags |= TransClient::ApiFail;

      if (trans_ptr.p->m_isMaster)
      {
        jam();
        if (trans_ptr.p->m_clientFlags & TransClient::TakeOver)
        {
          // maybe already running in background
          jam();
          ndbrequire(trans_ptr.p->m_clientFlags & TransClient::Background);
        }
        else
        {
          takeOverTransClient(signal, trans_ptr);
        }

        TxHandlePtr tx_ptr;
        bool ok = findTxHandle(tx_ptr, trans_ptr.p->m_takeOverTxKey);
        ndbrequire(ok);

        tx_ptr.p->m_clientFlags |= TransClient::ApiFail;
        takeOvers++;
      }
    }

    c_schemaTransList.next(trans_ptr);
  }

  D("handleApiFail" << V(takeOvers));

  if (takeOvers == 0) {
    jam();
    apiFailBlockHandling(signal, failedApiNode);
  }
}

/*
 * Take over client trans, either by background request or at API
 * node failure.  Continue to the callback routine.
 */
void
Dbdict::takeOverTransClient(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("takeOverTransClient" << *trans_ptr.p);

  TxHandlePtr tx_ptr;
  bool ok = seizeTxHandle(tx_ptr);
  ndbrequire(ok);

  ndbrequire(!(trans_ptr.p->m_clientFlags & TransClient::TakeOver));
  trans_ptr.p->m_clientFlags |= TransClient::TakeOver;
  trans_ptr.p->m_takeOverTxKey = tx_ptr.p->tx_key;

  tx_ptr.p->m_transId = tx_ptr.p->tx_key;
  tx_ptr.p->m_transKey = trans_ptr.p->trans_key;
  // start with SchemaTrans point of view
  tx_ptr.p->m_clientState = trans_ptr.p->m_clientState;
  tx_ptr.p->m_clientFlags = trans_ptr.p->m_clientFlags;
  tx_ptr.p->m_takeOverRef = trans_ptr.p->m_clientRef;
  tx_ptr.p->m_takeOverTransId = trans_ptr.p->m_transId;

  runTransClientTakeOver(signal, tx_ptr.p->tx_key, 0);
}

/*
 * Run one step in client takeover.  If not waiting for a signal,
 * send end trans abort.  The only commit case is when takeover
 * takes place in client state EndReq.  At the end, send an info
 * event and check if the trans was from API node failure.
 */
void
Dbdict::runTransClientTakeOver(Signal* signal,
                               Uint32 tx_key,
                               Uint32 ret)
{
  TxHandlePtr tx_ptr;
  bool ok = findTxHandle(tx_ptr, tx_key);
  ndbrequire(ok);
  D("runClientTakeOver" << *tx_ptr.p << V(ret));

  if (ret != 0) {
    jam();
    setError(tx_ptr, ret, __LINE__);
  }

  const TransClient::State oldState = tx_ptr.p->m_clientState;
  const Uint32 clientFlags = tx_ptr.p->m_clientFlags;
  ndbrequire(clientFlags & TransClient::TakeOver);

  TransClient::State newState = oldState;
  bool wait_sig = false;
  bool at_end = false;

  switch (oldState) {
  case TransClient::BeginReq:
    jam();
    newState = TransClient::BeginReply;
    wait_sig = true;
    break;
  case TransClient::BeginReply:
    jam();
    newState = TransClient::EndReply;
    break;
  case TransClient::ParseReq:
    jam();
    newState = TransClient::ParseReply;
    wait_sig = true;
    break;
  case TransClient::ParseReply:
    jam();
    newState = TransClient::EndReply;
    break;
  case TransClient::EndReq:
    jam();
    newState = TransClient::EndReply;
    wait_sig = true;
    break;
  case TransClient::EndReply:
    jam();
    newState = TransClient::StateUndef;
    at_end = true;
    break;
  default:
    ndbrequire(false);
    break;
  }

  D("set" << V(oldState) << " -> " << V(newState));
  tx_ptr.p->m_clientState = newState;

  if (!at_end) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::runTransClientTakeOver),
      tx_ptr.p->tx_key
    };
    tx_ptr.p->m_callback = c;

    if (!wait_sig) {
      jam();
      Uint32 flags = 0;
      flags |= SchemaTransEndReq::SchemaTransAbort;
      endSchemaTrans(signal, tx_ptr, flags);
    }
    return;
  }

  if (!hasError(tx_ptr.p->m_error)) {
    jam();
    infoEvent("DICT: api:0x%8x trans:0x%8x takeover completed",
              tx_ptr.p->m_takeOverRef, tx_ptr.p->m_takeOverTransId);
  } else {
    jam();
    infoEvent("DICT: api:0x%8x trans:0x%8x takeover failed, error:%u line:%u",
              tx_ptr.p->m_takeOverRef, tx_ptr.p->m_takeOverTransId,
              tx_ptr.p->m_error.errorCode, tx_ptr.p->m_error.errorLine);
  }

  // if from API fail, check if finished
  if (clientFlags & TransClient::ApiFail) {
    jam();
    finishApiFail(signal, tx_ptr);
  }
  releaseTxHandle(tx_ptr);
}

void
Dbdict::finishApiFail(Signal* signal, TxHandlePtr tx_ptr)
{
  D("finishApiFail" << *tx_ptr.p);
  const Uint32 failedApiNode = refToNode(tx_ptr.p->m_takeOverRef);

  Uint32 takeOvers = 0;
  SchemaTransPtr trans_ptr;
  c_schemaTransList.first(trans_ptr);
  while (trans_ptr.i != RNIL) {
    jam();
    D("check" << *trans_ptr.p);
    const BlockReference clientRef = trans_ptr.p->m_clientRef;

    if (refToNode(clientRef) == failedApiNode) {
      jam();
      const Uint32 clientFlags = trans_ptr.p->m_clientFlags;
      D("failed" << hex << V(clientRef) << dec << V(clientFlags));
      ndbrequire(clientFlags & TransClient::ApiFail);
      takeOvers++;
    }
    c_schemaTransList.next(trans_ptr);
  }

  D("finishApiFail" << V(takeOvers));

  if (takeOvers == 0) {
    jam();
    apiFailBlockHandling(signal, failedApiNode);
  }
}

void
Dbdict::apiFailBlockHandling(Signal* signal,
                             Uint32 failedApiNode)
{
  Callback cb = { safe_cast(&Dbdict::handleApiFailureCallback),
                  failedApiNode };
  simBlockNodeFailure(signal, failedApiNode, cb);
}

// find callback for any key

bool
Dbdict::findCallback(Callback& callback, Uint32 any_key)
{
  SchemaOpPtr op_ptr;
  SchemaTransPtr trans_ptr;
  TxHandlePtr tx_ptr;

  bool ok1 = findSchemaOp(op_ptr, any_key);
  bool ok2 = findSchemaTrans(trans_ptr, any_key);
  bool ok3 = findTxHandle(tx_ptr, any_key);
  ndbrequire(ok1 + ok2 + ok3 <= 1);

  if (ok1) {
    callback = op_ptr.p->m_callback;
    return true;
  }
  if (ok2) {
    callback = trans_ptr.p->m_callback;
    return true;
  }
  if (ok3) {
    callback = tx_ptr.p->m_callback;
    return true;
  }
  callback.m_callbackFunction = 0;
  callback.m_callbackData = 0;
  return false;
}

// MODULE: CreateHashMap

ArrayPool<Hash2FragmentMap> g_hash_map;

const Dbdict::OpInfo
Dbdict::CreateHashMapRec::g_opInfo = {
  { 'C', 'H', 'M', 0 },
  ~RT_DBDICT_CREATE_HASH_MAP,
  GSN_CREATE_HASH_MAP_REQ,
  CreateHashMapReq::SignalLength,
  //
  &Dbdict::createHashMap_seize,
  &Dbdict::createHashMap_release,
  //
  &Dbdict::createHashMap_parse,
  &Dbdict::createHashMap_subOps,
  &Dbdict::createHashMap_reply,
  //
  &Dbdict::createHashMap_prepare,
  &Dbdict::createHashMap_commit,
  &Dbdict::createHashMap_complete,
  //
  &Dbdict::createHashMap_abortParse,
  &Dbdict::createHashMap_abortPrepare
};

bool
Dbdict::createHashMap_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<CreateHashMapRec>(op_ptr);
}

void
Dbdict::createHashMap_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<CreateHashMapRec>(op_ptr);
}

void
Dbdict::execCREATE_HASH_MAP_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  SectionHandle handle(this, signal);

  const CreateHashMapReq req_copy =
    *(const CreateHashMapReq*)signal->getDataPtr();
  const CreateHashMapReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    CreateHashMapRecPtr createHashMapRecordPtr;
    CreateHashMapImplReq* impl_req;

    startClientReq(op_ptr, createHashMapRecordPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->objectId = RNIL;
    impl_req->objectVersion = 0;
    impl_req->buckets = req->buckets;
    impl_req->fragments = req->fragments;

    handleClientReq(signal, op_ptr, handle);
    return;
  } while (0);

  releaseSections(handle);

  CreateHashMapRef* ref = (CreateHashMapRef*)signal->getDataPtrSend();

  ref->senderRef = reference();
  ref->senderData= req->clientData;
  ref->transId = req->transId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_CREATE_HASH_MAP_REF, signal,
             CreateHashMapRef::SignalLength, JBB);
}

// CreateHashMap: PARSE

Uint32
Dbdict::get_default_fragments(Signal* signal, Uint32 extranodegroups)
{
  jam();

  CheckNodeGroups * sd = CAST_PTR(CheckNodeGroups, signal->getDataPtrSend());
  sd->extraNodeGroups = extranodegroups;
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::GetDefaultFragments;
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal,
		 CheckNodeGroups::SignalLength);
  jamEntry();
  return sd->output;
}

void
Dbdict::createHashMap_parse(Signal* signal, bool master,
                            SchemaOpPtr op_ptr,
                            SectionHandle& handle, ErrorInfo& error)
{

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateHashMapRecPtr createHashMapRecordPtr;
  getOpRec(op_ptr, createHashMapRecordPtr);
  CreateHashMapImplReq* impl_req = &createHashMapRecordPtr.p->m_request;

  jam();

  SegmentedSectionPtr objInfoPtr;
  DictHashMapInfo::HashMap hm; hm.init();
  if (handle.m_cnt)
  {
    SimpleProperties::UnpackStatus status;

    handle.getSection(objInfoPtr, CreateHashMapReq::INFO);
    SimplePropertiesSectionReader it(objInfoPtr, getSectionSegmentPool());
    status = SimpleProperties::unpack(it, &hm,
				      DictHashMapInfo::Mapping,
				      DictHashMapInfo::MappingSize,
				      true, true);

    if (ERROR_INSERTED(6204))
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      setError(error, 1, __LINE__);
      return;
    }

    if (status != SimpleProperties::Eof)
    {
      jam();
      setError(error, CreateTableRef::InvalidFormat, __LINE__);
      return;
    }
  }
  else if (!master)
  {
    jam();
    setError(error, CreateTableRef::InvalidFormat, __LINE__);
    return;
  }
  else
  {
    /**
     * Convienienc branch...(only master)
     * No info, create "default"
     */
    jam();
    if (impl_req->requestType & CreateHashMapReq::CreateDefault)
    {
      jam();
      impl_req->buckets = NDB_DEFAULT_HASHMAP_BUCKETS;
      impl_req->fragments = 0;
    }

    Uint32 buckets = impl_req->buckets;
    Uint32 fragments = impl_req->fragments;
    if (fragments == 0)
    {
      jam();

      fragments = get_default_fragments(signal);
    }

    if (fragments > MAX_NDB_PARTITIONS)
    {
      jam();
      setError(error, CreateTableRef::TooManyFragments, __LINE__);
      return;
    }

    BaseString::snprintf(hm.HashMapName, sizeof(hm.HashMapName),
                         "DEFAULT-HASHMAP-%u-%u",
                         buckets,
                         fragments);

    if (buckets == 0 || buckets > Hash2FragmentMap::MAX_MAP)
    {
      jam();
      setError(error, CreateTableRef::InvalidFormat, __LINE__);
      return;
    }

    hm.HashMapBuckets = buckets;
    for (Uint32 i = 0; i<buckets; i++)
    {
      hm.HashMapValues[i] = (i % fragments);
    }

    /**
     * pack is stupid...and requires bytes!
     * we store shorts...so multiply by 2
     */
    hm.HashMapBuckets *= sizeof(Uint16);
    SimpleProperties::UnpackStatus s;
    SimplePropertiesSectionWriter w(* this);
    s = SimpleProperties::pack(w,
                               &hm,
                               DictHashMapInfo::Mapping,
                               DictHashMapInfo::MappingSize, true);
    ndbrequire(s == SimpleProperties::Eof);
    w.getPtr(objInfoPtr);

    handle.m_cnt = 1;
    handle.m_ptr[CreateHashMapReq::INFO] = objInfoPtr;
  }

  Uint32 len = Uint32(strlen(hm.HashMapName) + 1);
  Uint32 hash = LocalRope::hash(hm.HashMapName, len);

  if (ERROR_INSERTED(6205))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    return;
  }

  DictObject * objptr = get_object(hm.HashMapName, len, hash);
  if(objptr != 0)
  {
    jam();

    if (! (impl_req->requestType & CreateHashMapReq::CreateIfNotExists))
    {
      jam();
      setError(error, CreateTableRef::TableAlreadyExist, __LINE__);
      return;
    }

    /**
     * verify object found
     */

    if (objptr->m_type != DictTabInfo::HashMap)
    {
      jam();
      setError(error, CreateTableRef::TableAlreadyExist, __LINE__);
      return;
    }

    if (check_write_obj(objptr->m_id,
                        trans_ptr.p->m_transId,
                        SchemaFile::SF_CREATE, error))
    {
      jam();
      return;
    }

    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, objptr->m_id));

    impl_req->objectId = objptr->m_id;
    impl_req->objectVersion = hm_ptr.p->m_object_version;
    return;
  }
  else
  {
    jam();
    /**
     * Clear the IfNotExistsFlag
     */
    impl_req->requestType &= ~Uint32(CreateHashMapReq::CreateIfNotExists);
  }

  if (ERROR_INSERTED(6206))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    return;
  }

  RopeHandle name;
  {
    LocalRope tmp(c_rope_pool, name);
    if(!tmp.assign(hm.HashMapName, len, hash))
    {
      jam();
      setError(error, CreateTableRef::OutOfStringBuffer, __LINE__);
      return;
    }
  }

  Uint32 objId = RNIL;
  Uint32 objVersion = RNIL;
  Uint32 errCode = 0;
  Uint32 errLine = 0;
  DictObjectPtr obj_ptr; obj_ptr.setNull();
  HashMapRecordPtr hm_ptr; hm_ptr.setNull();
  Ptr<Hash2FragmentMap> map_ptr; map_ptr.setNull();

  if (master)
  {
    jam();

    if (ERROR_INSERTED(6207))
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      setError(error, 1, __LINE__);
      goto error;
    }

    objId = impl_req->objectId = getFreeObjId();
    if (objId == RNIL)
    {
      jam();
      errCode = CreateTableRef::NoMoreTableRecords;
      errLine = __LINE__;
      goto error;
    }

    Uint32 version = getTableEntry(impl_req->objectId)->m_tableVersion;
    impl_req->objectVersion = create_obj_inc_schema_version(version);
  }
  else if (op_ptr.p->m_restart)
  {
    impl_req->objectId = c_restartRecord.activeTable;
    impl_req->objectVersion=c_restartRecord.m_entry.m_tableVersion;
  }

  objId = impl_req->objectId;
  objVersion = impl_req->objectVersion;

  if (ERROR_INSERTED(6208))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    goto error;
  }

  if(!c_obj_pool.seize(obj_ptr))
  {
    jam();
    errCode = CreateTableRef::NoMoreTableRecords;
    errLine = __LINE__;
    goto error;
  }

  new (obj_ptr.p) DictObject;
  obj_ptr.p->m_id = objId;
  obj_ptr.p->m_type = DictTabInfo::HashMap;
  obj_ptr.p->m_ref_count = 0;
  obj_ptr.p->m_name = name;
  c_obj_name_hash.add(obj_ptr);
  c_obj_id_hash.add(obj_ptr);

  if (ERROR_INSERTED(6209))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    goto error;
  }

  if (!g_hash_map.seize(map_ptr))
  {
    jam();
    errCode = CreateTableRef::NoMoreTableRecords;
    errLine = __LINE__;
    goto error;
  }

  if (ERROR_INSERTED(6210))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    goto error;
  }

  if(!c_hash_map_pool.seize(hm_ptr))
  {
    jam();
    errCode = CreateTableRef::NoMoreTableRecords;
    errLine = __LINE__;
    goto error;
  }

  new (hm_ptr.p) HashMapRecord();

  hm_ptr.p->m_object_id = objId;
  hm_ptr.p->m_object_version = objVersion;
  hm_ptr.p->m_name = name;
  hm_ptr.p->m_map_ptr_i = map_ptr.i;
  link_object(obj_ptr, hm_ptr);

  /**
   * pack is stupid...and requires bytes!
   * we store shorts...so divide by 2
   */
  hm.HashMapBuckets /= sizeof(Uint16);

  map_ptr.p->m_cnt = hm.HashMapBuckets;
  map_ptr.p->m_object_id = objId;
  {
    Uint32 tmp = 0;
    for (Uint32 i = 0; i<hm.HashMapBuckets; i++)
    {
      map_ptr.p->m_map[i] = hm.HashMapValues[i];
      if (hm.HashMapValues[i] > tmp)
        tmp = hm.HashMapValues[i];
    }
    map_ptr.p->m_fragments = tmp + 1;
  }
  if (map_ptr.p->m_fragments > MAX_NDB_PARTITIONS)
  {
    jam();
    setError(error, CreateTableRef::TooManyFragments, __LINE__);
    goto error;
  }

  if (ERROR_INSERTED(6211))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    setError(error, 1, __LINE__);
    goto error;
  }

  {
    SchemaFile::TableEntry te; te.init();
    te.m_tableState = SchemaFile::SF_CREATE;
    te.m_tableVersion = objVersion;
    te.m_tableType = obj_ptr.p->m_type;
    te.m_info_words = objInfoPtr.sz;
    te.m_gcp = 0;
    te.m_transId = trans_ptr.p->m_transId;

    Uint32 err = trans_log_schema_op(op_ptr, objId, &te);
    ndbrequire(err == 0);
  }

  saveOpSection(op_ptr, objInfoPtr, 0);
  handle.m_ptr[CreateHashMapReq::INFO] = objInfoPtr;
  handle.m_cnt = 1;

#if defined VM_TRACE || defined ERROR_INSERT
  ndbout_c("Dbdict: %u: create name=%s,id=%u,obj_ptr_i=%d",__LINE__,
           hm.HashMapName, objId, hm_ptr.p->m_obj_ptr_i);
#endif

  return;

error:
  ndbrequire(hasError(error));

  if (!hm_ptr.isNull())
  {
    jam();
    c_hash_map_pool.release(hm_ptr);
  }

  if (!map_ptr.isNull())
  {
    jam();
    g_hash_map.release(map_ptr);
  }

  if (!obj_ptr.isNull())
  {
    jam();
    release_object(obj_ptr.i);
  }
  else
  {
    jam();
    LocalRope tmp(c_rope_pool, name);
    tmp.erase();
  }
}

void
Dbdict::createHashMap_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createHashMap_abortParse" << *op_ptr.p);

  CreateHashMapRecPtr createHashMapRecordPtr;
  getOpRec(op_ptr, createHashMapRecordPtr);
  CreateHashMapImplReq* impl_req = &createHashMapRecordPtr.p->m_request;

  if (impl_req->requestType & CreateHashMapReq::CreateIfNotExists)
  {
    jam();
    ndbrequire(op_ptr.p->m_orig_entry_id == RNIL);
  }

  if (op_ptr.p->m_orig_entry_id != RNIL)
  {
    jam();

    HashMapRecordPtr hm_ptr;
    ndbrequire(find_object(hm_ptr, impl_req->objectId));

    release_object(hm_ptr.p->m_obj_ptr_i);
    g_hash_map.release(hm_ptr.p->m_map_ptr_i);
    c_hash_map_pool.release(hm_ptr);
  }

  // wl3600_todo probably nothing..

  sendTransConf(signal, op_ptr);
}

bool
Dbdict::createHashMap_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  return false;
}

void
Dbdict::createHashMap_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();
  D("createHashMap_reply");

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  CreateHashMapRecPtr createHashMapRecordPtr;
  getOpRec(op_ptr, createHashMapRecordPtr);
  const CreateHashMapImplReq* impl_req = &createHashMapRecordPtr.p->m_request;

  if (!hasError(error)) {
    CreateHashMapConf* conf = (CreateHashMapConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->objectId = impl_req->objectId;
    conf->objectVersion = impl_req->objectVersion;

    D(V(conf->objectId) << V(conf->objectVersion));

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_HASH_MAP_CONF, signal,
               CreateHashMapConf::SignalLength, JBB);
  } else {
    jam();
    CreateHashMapRef* ref = (CreateHashMapRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_CREATE_HASH_MAP_REF, signal,
               CreateHashMapRef::SignalLength, JBB);
  }
}

// CreateHashMap: PREPARE

void
Dbdict::createHashMap_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("createHashMap_prepare");

  CreateHashMapRecPtr createHashMapRecordPtr;
  getOpRec(op_ptr, createHashMapRecordPtr);
  CreateHashMapImplReq* impl_req = &createHashMapRecordPtr.p->m_request;

  if (impl_req->requestType & CreateHashMapReq::CreateIfNotExists)
  {
    jam();
    sendTransConf(signal, op_ptr);
    return;
  }

  Callback cb;
  cb.m_callbackData = op_ptr.p->op_key;
  cb.m_callbackFunction = safe_cast(&Dbdict::createHashMap_writeObjConf);

  const OpSection& tabInfoSec = getOpSection(op_ptr, 0);
  writeTableFile(signal, op_ptr, impl_req->objectId, tabInfoSec, &cb);
}

void
Dbdict::createHashMap_writeObjConf(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  CreateHashMapRecPtr createHashMapRecordPtr;
  findSchemaOp(op_ptr, createHashMapRecordPtr, op_key);

  ndbrequire(!op_ptr.isNull());

  sendTransConf(signal, op_ptr);
}

// CreateHashMap: COMMIT

void
Dbdict::createHashMap_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("createHashMap_commit");

  CreateHashMapRecPtr createHashMapRecordPtr;
  getOpRec(op_ptr, createHashMapRecordPtr);

  sendTransConf(signal, op_ptr);
}

// CreateHashMap: COMPLETE

void
Dbdict::createHashMap_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  sendTransConf(signal, op_ptr);
}

// CreateHashMap: ABORT

void
Dbdict::createHashMap_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createHashMap_abortPrepare" << *op_ptr.p);
  // wl3600_todo
  sendTransConf(signal, op_ptr);
}

void
Dbdict::packHashMapIntoPages(SimpleProperties::Writer & w,
                             HashMapRecordPtr hm_ptr)
{
  DictHashMapInfo::HashMap hm; hm.init();

  Ptr<Hash2FragmentMap> map_ptr;
  g_hash_map.getPtr(map_ptr, hm_ptr.p->m_map_ptr_i);

  ConstRope r(c_rope_pool, hm_ptr.p->m_name);
  r.copy(hm.HashMapName);
  hm.HashMapBuckets = map_ptr.p->m_cnt;
  hm.HashMapObjectId = hm_ptr.p->m_object_id;
  hm.HashMapVersion = hm_ptr.p->m_object_version;

  for (Uint32 i = 0; i<hm.HashMapBuckets; i++)
  {
    hm.HashMapValues[i] = map_ptr.p->m_map[i];
  }

  /**
   * pack is stupid...and requires bytes!
   * we store shorts...so multiply by 2
   */
  hm.HashMapBuckets *= sizeof(Uint16);
  SimpleProperties::UnpackStatus s;
  s = SimpleProperties::pack(w,
			     &hm,
			     DictHashMapInfo::Mapping,
			     DictHashMapInfo::MappingSize, true);

  ndbrequire(s == SimpleProperties::Eof);
}

// CreateHashMap: END


// MODULE: debug

// ErrorInfo

NdbOut&
operator<<(NdbOut& out, const Dbdict::ErrorInfo& a)
{
  a.print(out);
  return out;
}

void
Dbdict::ErrorInfo::print(NdbOut& out) const
{
  out << "[";
  out << " code: " << errorCode;
  out << " line: " << errorLine;
  out << " node: " << errorNodeId;
  out << " count: " << errorCount;
  out << " status: " << errorStatus;
  out << " key: " << errorKey;
  out << " name: '" << errorObjectName << "'";
  out << " ]";
}

#ifdef VM_TRACE

// DictObject

NdbOut&
operator<<(NdbOut& out, const Dbdict::DictObject& a)
{
  a.print(out);
  return out;
}

void
Dbdict::DictObject::print(NdbOut& out) const
{
  Dbdict* dict = (Dbdict*)globalData.getBlock(DBDICT);
  out << " (DictObject";
  out << dec << V(m_id);
  out << dec << V(m_type);
  out << " name:" << dict->copyRope<PATH_MAX>(m_name);
  out << dec << V(m_ref_count);
  out << dec << V(m_trans_key);
  out << dec << V(m_op_ref_count);
  out << ")";
}


// SchemaOp

NdbOut&
operator<<(NdbOut& out, const Dbdict::SchemaOp& a)
{
  a.print(out);
  return out;
}

void
Dbdict::SchemaOp::print(NdbOut& out) const
{
  //Dbdict* dict = (Dbdict*)globalData.getBlock(DBDICT);
  const Dbdict::OpInfo& info = m_oprec_ptr.p->m_opInfo;
  out << " (SchemaOp";
  out << " " << info.m_opType;
  out << dec << V(op_key);
  if (m_error.errorCode != 0)
    out << m_error;
  if (m_sections != 0)
    out << dec << V(m_sections);
  if (m_base_op_ptr_i == RNIL)
    out << "-> RNIL";
  else
  {
    out << "-> " << m_base_op_ptr_i;
  }

  out << ")";
}



// SchemaTrans

NdbOut&
operator<<(NdbOut& out, const Dbdict::SchemaTrans& a)
{
  a.print(out);
  return out;
}

void
Dbdict::SchemaTrans::print(NdbOut& out) const
{
  out << " (SchemaTrans";
  out << dec << V(m_state);
  out << dec << V(trans_key);
  out << dec << V(m_isMaster);
  out << hex << V(m_clientRef);
  out << hex << V(m_transId);
  out << dec << V(m_clientState);
  out << hex << V(m_clientFlags);
  out << ")";
}

// TxHandle

NdbOut&
operator<<(NdbOut& out, const Dbdict::TxHandle& a)
{
  a.print(out);
  return out;
}

void
Dbdict::TxHandle::print(NdbOut& out) const
{
  out << " (TxHandle";
  out << dec << V(tx_key);
  out << hex << V(m_transId);
  out << dec << V(m_transKey);
  out << dec << V(m_clientState);
  out << hex << V(m_clientFlags);
  out << hex << V(m_takeOverRef);
  out << hex << V(m_takeOverTransId);
  out << ")";
}

// check consistency when no schema trans is active

#undef SZ
#define SZ PATH_MAX

void
Dbdict::check_consistency()
{
  D("check_consistency");

#if 0
  // schema file entries // mis-named "tables"
  TableRecordPtr tablePtr;
  for (tablePtr.i = 0;
      tablePtr.i < c_noOfMetaTables;
      tablePtr.i++) {
    if (check_read_obj(tablePtr.i,

    c_tableRecordPool_.getPtr(tablePtr);

    switch (tablePtr.p->tabState) {
    case TableRecord::NOT_DEFINED:
      continue;
    default:
      break;
    }
    check_consistency_entry(tablePtr);
  }
#endif

  // triggers // should be in schema file
  TriggerRecordPtr triggerPtr;
  for (Uint32 id = 0;
      id < c_triggerRecordPool_.getSize();
      id++) {
    bool ok = find_object(triggerPtr, id);
    if (!ok) continue;
    switch (triggerPtr.p->triggerState) {
    case TriggerRecord::TS_NOT_DEFINED:
      continue;
    default:
      break;
    }
    check_consistency_trigger(triggerPtr);
  }
}

void
Dbdict::check_consistency_entry(TableRecordPtr tablePtr)
{
  switch (tablePtr.p->tableType) {
  case DictTabInfo::SystemTable:
    jam();
    check_consistency_table(tablePtr);
    break;
  case DictTabInfo::UserTable:
    jam();
    check_consistency_table(tablePtr);
    break;
  case DictTabInfo::UniqueHashIndex:
    jam();
    check_consistency_index(tablePtr);
    break;
  case DictTabInfo::OrderedIndex:
    jam();
    check_consistency_index(tablePtr);
    break;
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::check_consistency_table(TableRecordPtr tablePtr)
{
  D("table " << copyRope<SZ>(tablePtr.p->tableName));

  switch (tablePtr.p->tableType) {
  case DictTabInfo::SystemTable: // should just be "Table"
    jam();
    break;
  case DictTabInfo::UserTable: // should just be "Table"
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  DictObjectPtr obj_ptr;
  obj_ptr.i = tablePtr.p->m_obj_ptr_i;
  ndbrequire(obj_ptr.i != RNIL);
  c_obj_pool.getPtr(obj_ptr);
  check_consistency_object(obj_ptr);

  ndbrequire(obj_ptr.p->m_id == tablePtr.p->tableId);
  ndbrequire(!strcmp(
        copyRope<SZ>(obj_ptr.p->m_name),
        copyRope<SZ>(tablePtr.p->tableName)));
}

void
Dbdict::check_consistency_index(TableRecordPtr indexPtr)
{
  D("index " << copyRope<SZ>(indexPtr.p->tableName));
  ndbrequire(indexPtr.p->tableId == indexPtr.i);

  switch (indexPtr.p->indexState) { // these states are non-sense
  case TableRecord::IS_ONLINE:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, indexPtr.p->primaryTableId);
  ndbrequire(ok);
  check_consistency_table(tablePtr);

  bool is_unique_index = false;
  switch (indexPtr.p->tableType) {
  case DictTabInfo::UniqueHashIndex:
    jam();
    is_unique_index = true;
    break;
  case DictTabInfo::OrderedIndex:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  TriggerRecordPtr triggerPtr;
  ok = find_object(triggerPtr, indexPtr.p->triggerId);
  ndbrequire(ok);
  ndbrequire(triggerPtr.p->tableId == tablePtr.p->tableId);
  ndbrequire(triggerPtr.p->indexId == indexPtr.p->tableId);

  check_consistency_trigger(triggerPtr);

  TriggerInfo ti;
  TriggerInfo::unpackTriggerInfo(triggerPtr.p->triggerInfo, ti);
  ndbrequire(ti.triggerEvent == TriggerEvent::TE_CUSTOM);

  DictObjectPtr obj_ptr;
  obj_ptr.i = triggerPtr.p->m_obj_ptr_i;
  ndbrequire(obj_ptr.i != RNIL);
  c_obj_pool.getPtr(obj_ptr);
  check_consistency_object(obj_ptr);

  ndbrequire(obj_ptr.p->m_id == triggerPtr.p->triggerId);
  ndbrequire(!strcmp(copyRope<SZ>(obj_ptr.p->m_name),
		     copyRope<SZ>(triggerPtr.p->triggerName)));
}

void
Dbdict::check_consistency_trigger(TriggerRecordPtr triggerPtr)
{
  if (! (triggerPtr.p->triggerState == TriggerRecord::TS_FAKE_UPGRADE))
  {
    ndbrequire(triggerPtr.p->triggerState == TriggerRecord::TS_ONLINE);
  }

  TableRecordPtr tablePtr;
  bool ok = find_object(tablePtr, triggerPtr.p->tableId);
  ndbrequire(ok);
  check_consistency_table(tablePtr);

  if (triggerPtr.p->indexId != RNIL)
  {
    jam();
    TableRecordPtr indexPtr;
    ndbrequire(check_read_obj(triggerPtr.p->indexId) == 0);
    bool ok = find_object(indexPtr, triggerPtr.p->indexId);
    ndbrequire(ok);
    ndbrequire(indexPtr.p->indexState == TableRecord::IS_ONLINE);
    TriggerInfo ti;
    TriggerInfo::unpackTriggerInfo(triggerPtr.p->triggerInfo, ti);
    switch (ti.triggerEvent) {
    case TriggerEvent::TE_CUSTOM:
      if (! (triggerPtr.p->triggerState == TriggerRecord::TS_FAKE_UPGRADE))
      {
        ndbrequire(triggerPtr.p->triggerId == indexPtr.p->triggerId);
      }
      break;
    default:
      ndbrequire(false);
      break;
    }
  } else {
    TriggerInfo ti;
    TriggerInfo::unpackTriggerInfo(triggerPtr.p->triggerInfo, ti);
    ndbrequire(ti.triggerType == TriggerType::REORG_TRIGGER);
  }
}

void
Dbdict::check_consistency_object(DictObjectPtr obj_ptr)
{
  ndbrequire(obj_ptr.p->m_trans_key == 0);
  ndbrequire(obj_ptr.p->m_op_ref_count == 0);
}

#endif

void
Dbdict::send_event(Signal* signal,
                   SchemaTransPtr& trans_ptr,
                   Uint32 ev,
                   Uint32 id,
                   Uint32 version,
                   Uint32 type)
{
  if (!trans_ptr.p->m_isMaster)
  {
    return;
  }

  switch(ev){
  case NDB_LE_CreateSchemaObject:
  case NDB_LE_AlterSchemaObject:
  case NDB_LE_DropSchemaObject:
    break;
  default:
    ndbassert(false);
    return;
  }
  signal->theData[0] = ev;
  signal->theData[1] = id;
  signal->theData[2] = version;
  signal->theData[3] = type;
  signal->theData[4] = refToNode(trans_ptr.p->m_clientRef);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);
}
