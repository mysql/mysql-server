/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <my_sys.h>

#define DBDICT_C
#include "Dbdict.hpp"
#include "diskpage.hpp"

#include <ndb_limits.h>
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
#include <signaldata/AlterTrig.hpp>
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

#include <signaldata/DropObj.hpp>
#include <signaldata/CreateObj.hpp>
#include <SLList.hpp>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

#include <signaldata/SchemaTrans.hpp>
#include <DebuggerNames.hpp>

// TransporterCallback.cpp
extern void release(SegmentedSectionPtr&);

#define ZNOT_FOUND 626
#define ZALREADYEXIST 630

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
struct {
  Uint32 m_gsn_user_req;
  Uint32 m_gsn_req;
  Uint32 m_gsn_ref;
  Uint32 m_gsn_conf;
  void (Dbdict::* m_trans_commit_start)(Signal*, Dbdict::SchemaTransaction*);
  void (Dbdict::* m_trans_commit_complete)(Signal*,Dbdict::SchemaTransaction*);
  void (Dbdict::* m_trans_abort_start)(Signal*, Dbdict::SchemaTransaction*);
  void (Dbdict::* m_trans_abort_complete)(Signal*, Dbdict::SchemaTransaction*);

  void (Dbdict::* m_prepare_start)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_prepare_complete)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_commit)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_commit_start)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_commit_complete)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_abort)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_abort_start)(Signal*, Dbdict::SchemaOperation*);
  void (Dbdict::* m_abort_complete)(Signal*, Dbdict::SchemaOperation*);
  
} f_dict_op[] = {
  /**
   * Create filegroup
   */
  { 
    GSN_CREATE_FILEGROUP_REQ,
    GSN_CREATE_OBJ_REQ, GSN_CREATE_OBJ_REF, GSN_CREATE_OBJ_CONF,
    0, 0, 0, 0,
    &Dbdict::create_fg_prepare_start, &Dbdict::create_fg_prepare_complete,
    &Dbdict::createObj_commit,
    0, 0,
    &Dbdict::createObj_abort,
    &Dbdict::create_fg_abort_start, &Dbdict::create_fg_abort_complete,
  }

  /**
   * Create file
   */
  ,{ 
    GSN_CREATE_FILE_REQ,
    GSN_CREATE_OBJ_REQ, GSN_CREATE_OBJ_REF, GSN_CREATE_OBJ_CONF,
    0, 0, 0, 0,
    &Dbdict::create_file_prepare_start, &Dbdict::create_file_prepare_complete,
    &Dbdict::createObj_commit,
    &Dbdict::create_file_commit_start, 0,
    &Dbdict::createObj_abort,
    &Dbdict::create_file_abort_start, &Dbdict::create_file_abort_complete,
  }

  /**
   * Drop file
   */
  ,{ 
    GSN_DROP_FILE_REQ,
    GSN_DROP_OBJ_REQ, GSN_DROP_OBJ_REF, GSN_DROP_OBJ_CONF,
    0, 0, 0, 0,
    &Dbdict::drop_file_prepare_start, 0,
    &Dbdict::dropObj_commit,
    &Dbdict::drop_file_commit_start, &Dbdict::drop_file_commit_complete,
    &Dbdict::dropObj_abort,
    &Dbdict::drop_file_abort_start, 0
  }

  /**
   * Drop filegroup
   */
  ,{ 
    GSN_DROP_FILEGROUP_REQ,
    GSN_DROP_OBJ_REQ, GSN_DROP_OBJ_REF, GSN_DROP_OBJ_CONF,
    0, 0, 0, 0,
    &Dbdict::drop_fg_prepare_start, 0,
    &Dbdict::dropObj_commit,
    &Dbdict::drop_fg_commit_start, &Dbdict::drop_fg_commit_complete,
    &Dbdict::dropObj_abort,
    &Dbdict::drop_fg_abort_start, 0
  }

  /**
   * Drop undofile
   */
  ,{ 
    GSN_DROP_FILE_REQ,
    GSN_DROP_OBJ_REQ, GSN_DROP_OBJ_REF, GSN_DROP_OBJ_CONF,
    0, 0, 0, 0,
    &Dbdict::drop_undofile_prepare_start, 0,
    0,
    0, &Dbdict::drop_undofile_commit_complete,
    0, 0, 0
  }
};

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
    c_tableRecordPool.getPtr(tabRecPtr, tab);
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
    DLHashTable<DictObject>::Iterator iter;
    bool ok = c_obj_hash.first(iter);
    for(; ok; ok = c_obj_hash.next(iter))
    {
      Rope name(c_rope_pool, iter.curr.p->m_name);
      char buf[1024];
      name.copy(buf);
      ndbout_c("%s m_ref_count: %d", buf, iter.curr.p->m_ref_count); 
      if (iter.curr.p->m_trans_key != 0)
        ndbout_c("- m_trans_key: %u m_op_ref_count: %u",
                 iter.curr.p->m_trans_key, iter.curr.p->m_op_ref_count);
    }
  }    
  
  return;
}//Dbdict::execDUMP_STATE_ORD()

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
    c_tableRecordPool.getPtr(tablePtr, tableId);
    packTableIntoPages(w, tablePtr, signal);
    break;
  }
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:{
    FilegroupPtr fg_ptr;
    ndbrequire(c_filegroup_hash.find(fg_ptr, tableId));
    const Uint32 free_hi= signal->theData[4];
    const Uint32 free_lo= signal->theData[5];
    packFilegroupIntoPages(w, fg_ptr, free_hi, free_lo);
    break;
  }
  case DictTabInfo::Datafile:{
    FilePtr fg_ptr;
    ndbrequire(c_file_hash.find(fg_ptr, tableId));
    const Uint32 free_extents= signal->theData[4];
    packFileIntoPages(w, fg_ptr, free_extents);
    break;
  }
  case DictTabInfo::Undofile:{
    FilePtr fg_ptr;
    ndbrequire(c_file_hash.find(fg_ptr, tableId));
    packFileIntoPages(w, fg_ptr, 0);
    break;
  }
  case DictTabInfo::UndefTableType:
  case DictTabInfo::HashIndexTrigger:
  case DictTabInfo::SubscriptionTrigger:
  case DictTabInfo::ReadOnlyConstraint:
  case DictTabInfo::IndexTrigger:
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
    char tsData[2*2*MAX_NDB_PARTITIONS];
    char defaultValue[MAX_ATTR_DEFAULT_VALUE_SIZE];
    char attributeName[MAX_ATTR_NAME_SIZE];
  };
  ConstRope r(c_rope_pool, tablePtr.p->tableName);
  r.copy(tableName);
  w.add(DictTabInfo::TableName, tableName);
  w.add(DictTabInfo::TableId, tablePtr.i);
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

  if(signal)
  {
    /* This branch is run at GET_TABINFOREQ */

    Uint32 * theData = signal->getDataPtrSend();
    CreateFragmentationReq * const req = (CreateFragmentationReq*)theData;
    req->senderRef = 0;
    req->senderData = RNIL;
    req->fragmentationType = tablePtr.p->fragmentType;
    req->noOfFragments = 0;
    req->primaryTableId = tablePtr.i;
    EXECUTE_DIRECT(DBDICT, GSN_CREATE_FRAGMENTATION_REQ, signal,
                   CreateFragmentationReq::SignalLength);
    ndbrequire(signal->theData[0] == 0);
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
  
  if (tablePtr.p->primaryTableId != RNIL){
    TableRecordPtr primTab;
    c_tableRecordPool.getPtr(primTab, tablePtr.p->primaryTableId);
    ConstRope r2(c_rope_pool, primTab.p->tableName);
    r2.copy(tableName);
    w.add(DictTabInfo::PrimaryTable, tableName);
    w.add(DictTabInfo::PrimaryTableId, tablePtr.p->primaryTableId);
    w.add(DictTabInfo::IndexState, tablePtr.p->indexState);
    w.add(DictTabInfo::InsertTriggerId, tablePtr.p->insertTriggerId);
    w.add(DictTabInfo::UpdateTriggerId, tablePtr.p->updateTriggerId);
    w.add(DictTabInfo::DeleteTriggerId, tablePtr.p->deleteTriggerId);
    w.add(DictTabInfo::CustomTriggerId, tablePtr.p->customTriggerId);
  }

  ConstRope frm(c_rope_pool, tablePtr.p->frmData);
  frm.copy(frmData);
  w.add(DictTabInfo::FrmLen, frm.size());
  w.add(DictTabInfo::FrmData, frmData, frm.size());

  {
    jam();
    ConstRope ts(c_rope_pool, tablePtr.p->tsData);
    ts.copy(tsData);
    w.add(DictTabInfo::TablespaceDataLen, ts.size());
    w.add(DictTabInfo::TablespaceData, tsData, ts.size());

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
    ndbrequire(c_filegroup_hash.find(tsPtr, tablePtr.p->m_tablespace_id));
    w.add(DictTabInfo::TablespaceVersion, tsPtr.p->m_version);
  }

  AttributeRecordPtr attrPtr;
  LocalDLFifoList<AttributeRecord> list(c_attributeRecordPool, 
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
    w.add(DictTabInfo::AttributeDefaultValue, defaultValue);
    
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
    ndbrequire(c_filegroup_hash.find(lfg_ptr, fg.TS_LogfileGroupId));
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
  f.FileSizeHi = (f_ptr.p->m_file_size >> 32);
  f.FileSizeLo = (f_ptr.p->m_file_size & 0xFFFFFFFF);
  f.FileFreeExtents= free_extents;
  f.FileId =  f_ptr.p->key;
  f.FileVersion = f_ptr.p->m_version;

  FilegroupPtr lfg_ptr;
  ndbrequire(c_filegroup_hash.find(lfg_ptr, f.FilegroupId));
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
  const CreateFragmentationReq* req =
    (const CreateFragmentationReq*)signal->getDataPtr();

  if (req->primaryTableId == RNIL) {
    jam();
    EXECUTE_DIRECT(DBDIH, GSN_CREATE_FRAGMENTATION_REQ, signal,
                   CreateFragmentationReq::SignalLength);
    return;
  }

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, req->primaryTableId);
  if (tablePtr.p->tabState == TableRecord::DEFINED ||
      tablePtr.p->tabState == TableRecord::BACKUP_ONGOING) {
    jam();
    EXECUTE_DIRECT(DBDIH, GSN_CREATE_FRAGMENTATION_REQ, signal,
                   CreateFragmentationReq::SignalLength);
    return;
  }

  DictObjectPtr obj_ptr;
  c_obj_pool.getPtr(obj_ptr, tablePtr.p->m_obj_ptr_i);

  SchemaOpPtr op_ptr;
  findDictObjectOp(op_ptr, obj_ptr);
  ndbrequire(!op_ptr.isNull());
  OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
  ndbrequire(memcmp(oprec_ptr.p->m_opType, "CTa", 4) == 0);

  Uint32 *theData = &signal->theData[0];
  const OpSection& fragSection =
    getOpSection(op_ptr, CreateTabReq::FRAGMENTATION);
  copyOut(fragSection, &theData[25], ZNIL);
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
    if(ERROR_INSERTED(6007)){
      jam();
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
Dbdict::writeTableFile(Signal* signal, Uint32 tableId,
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
    ndbrequire(pages == 1);

    PageRecordPtr pageRecPtr;
    c_pageRecordArray.getPtr(pageRecPtr, c_writeTableRecord.pageId);

    Uint32* dst = &pageRecPtr.p->word[ZPAGE_HEADER_SIZE];
    Uint32 dstSize = ZSIZE_OF_PAGES_IN_WORDS - ZPAGE_HEADER_SIZE;
    bool ok = copyOut(tabInfoSec, dst, dstSize);
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
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
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
      snprintf(reason_msg, sizeof(reason_msg),
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
        (c_tableRecordPool.getSize() + NDB_SF_PAGE_ENTRIES - 1) /
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
  Uint32 page_old[pageSize_old >> 2];
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
      if (te_old.m_tableState == SchemaFile::INIT ||
          te_old.m_tableState == SchemaFile::DROP_TABLE_COMMITTED ||
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
  c_file_hash(c_file_pool),
  c_filegroup_hash(c_filegroup_pool),
  c_obj_hash(c_obj_pool),
  c_schemaOpHash(c_schemaOpPool),
  c_schemaTransHash(c_schemaTransPool),
  c_schemaTransList(c_schemaTransPool),
  c_schemaTransCount(0),
  c_txHandleHash(c_txHandlePool),
  c_opCreateIndex(c_opRecordPool),
  c_opDropIndex(c_opRecordPool),
  c_opAlterIndex(c_opRecordPool),
  c_opBuildIndex(c_opRecordPool),
  c_opCreateEvent(c_opRecordPool),
  c_opSubEvent(c_opRecordPool),
  c_opDropEvent(c_opRecordPool),
  c_opSignalUtil(c_opRecordPool),
  c_schemaOperation(c_opRecordPool),
  c_Trans(c_opRecordPool),
  c_opCreateObj(c_schemaOperation),
  c_opDropObj(c_schemaOperation),
  c_opRecordSequence(0)
#ifdef VM_TRACE
  ,debugOut(*new NullOutputStream())
#endif
{
  BLOCK_CONSTRUCTOR(Dbdict);
  
  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbdict::execDUMP_STATE_ORD);
  addRecSignal(GSN_GET_TABINFOREQ, &Dbdict::execGET_TABINFOREQ);
  addRecSignal(GSN_GET_TABLEID_REQ, &Dbdict::execGET_TABLEDID_REQ);
  addRecSignal(GSN_GET_TABINFO_CONF, &Dbdict::execGET_TABINFO_CONF);
  addRecSignal(GSN_CONTINUEB, &Dbdict::execCONTINUEB);

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

  // Index signals
  addRecSignal(GSN_CREATE_INDX_REQ, &Dbdict::execCREATE_INDX_REQ);
  addRecSignal(GSN_CREATE_INDX_CONF, &Dbdict::execCREATE_INDX_CONF);
  addRecSignal(GSN_CREATE_INDX_REF, &Dbdict::execCREATE_INDX_REF);

  addRecSignal(GSN_ALTER_INDX_REQ, &Dbdict::execALTER_INDX_REQ);
  addRecSignal(GSN_ALTER_INDX_CONF, &Dbdict::execALTER_INDX_CONF);
  addRecSignal(GSN_ALTER_INDX_REF, &Dbdict::execALTER_INDX_REF);

  addRecSignal(GSN_CREATE_TABLE_CONF, &Dbdict::execCREATE_TABLE_CONF);
  addRecSignal(GSN_CREATE_TABLE_REF, &Dbdict::execCREATE_TABLE_REF);

  addRecSignal(GSN_DROP_INDX_REQ, &Dbdict::execDROP_INDX_REQ);
  addRecSignal(GSN_DROP_INDX_CONF, &Dbdict::execDROP_INDX_CONF);
  addRecSignal(GSN_DROP_INDX_REF, &Dbdict::execDROP_INDX_REF);

  addRecSignal(GSN_DROP_TABLE_CONF, &Dbdict::execDROP_TABLE_CONF);
  addRecSignal(GSN_DROP_TABLE_REF, &Dbdict::execDROP_TABLE_REF);

  addRecSignal(GSN_BUILDINDXREQ, &Dbdict::execBUILDINDXREQ);
  addRecSignal(GSN_BUILDINDXCONF, &Dbdict::execBUILDINDXCONF);
  addRecSignal(GSN_BUILDINDXREF, &Dbdict::execBUILDINDXREF);

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
  addRecSignal(GSN_ALTER_TRIG_REQ, &Dbdict::execALTER_TRIG_REQ);
  addRecSignal(GSN_ALTER_TRIG_CONF, &Dbdict::execALTER_TRIG_CONF);
  addRecSignal(GSN_ALTER_TRIG_REF, &Dbdict::execALTER_TRIG_REF);
  addRecSignal(GSN_DROP_TRIG_REQ, &Dbdict::execDROP_TRIG_REQ);
  addRecSignal(GSN_DROP_TRIG_CONF, &Dbdict::execDROP_TRIG_CONF);
  addRecSignal(GSN_DROP_TRIG_REF, &Dbdict::execDROP_TRIG_REF);
  // Impl
  addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &Dbdict::execCREATE_TRIG_IMPL_CONF);
  addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &Dbdict::execCREATE_TRIG_IMPL_REF);
  addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &Dbdict::execDROP_TRIG_IMPL_CONF);
  addRecSignal(GSN_DROP_TRIG_IMPL_REF, &Dbdict::execDROP_TRIG_IMPL_REF);

  // Received signals
  addRecSignal(GSN_HOT_SPAREREP, &Dbdict::execHOT_SPAREREP);
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
  addRecSignal(GSN_DROP_FILE_REF, &Dbdict::execDROP_FILE_REF);
  addRecSignal(GSN_DROP_FILE_CONF, &Dbdict::execDROP_FILE_CONF);

  addRecSignal(GSN_DROP_FILEGROUP_REQ, &Dbdict::execDROP_FILEGROUP_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_REF, &Dbdict::execDROP_FILEGROUP_REF);
  addRecSignal(GSN_DROP_FILEGROUP_CONF, &Dbdict::execDROP_FILEGROUP_CONF);
  
  addRecSignal(GSN_CREATE_OBJ_REQ, &Dbdict::execCREATE_OBJ_REQ);
  addRecSignal(GSN_CREATE_OBJ_REF, &Dbdict::execCREATE_OBJ_REF);
  addRecSignal(GSN_CREATE_OBJ_CONF, &Dbdict::execCREATE_OBJ_CONF);
  addRecSignal(GSN_DROP_OBJ_REQ, &Dbdict::execDROP_OBJ_REQ);
  addRecSignal(GSN_DROP_OBJ_REF, &Dbdict::execDROP_OBJ_REF);
  addRecSignal(GSN_DROP_OBJ_CONF, &Dbdict::execDROP_OBJ_CONF);

  addRecSignal(GSN_CREATE_FILE_REF, &Dbdict::execCREATE_FILE_REF);
  addRecSignal(GSN_CREATE_FILE_CONF, &Dbdict::execCREATE_FILE_CONF);
  addRecSignal(GSN_CREATE_FILEGROUP_REF, &Dbdict::execCREATE_FILEGROUP_REF);
  addRecSignal(GSN_CREATE_FILEGROUP_CONF, &Dbdict::execCREATE_FILEGROUP_CONF);

  addRecSignal(GSN_BACKUP_FRAGMENT_REQ, &Dbdict::execBACKUP_FRAGMENT_REQ);

  addRecSignal(GSN_DICT_COMMIT_REQ, &Dbdict::execDICT_COMMIT_REQ);
  addRecSignal(GSN_DICT_COMMIT_REF, &Dbdict::execDICT_COMMIT_REF);
  addRecSignal(GSN_DICT_COMMIT_CONF, &Dbdict::execDICT_COMMIT_CONF);

  addRecSignal(GSN_DICT_ABORT_REQ, &Dbdict::execDICT_ABORT_REQ);
  addRecSignal(GSN_DICT_ABORT_REF, &Dbdict::execDICT_ABORT_REF);
  addRecSignal(GSN_DICT_ABORT_CONF, &Dbdict::execDICT_ABORT_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REQ, &Dbdict::execSCHEMA_TRANS_BEGIN_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_CONF, &Dbdict::execSCHEMA_TRANS_BEGIN_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REF, &Dbdict::execSCHEMA_TRANS_BEGIN_REF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REQ, &Dbdict::execSCHEMA_TRANS_END_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_END_CONF, &Dbdict::execSCHEMA_TRANS_END_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REF, &Dbdict::execSCHEMA_TRANS_END_REF);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_REQ, &Dbdict::execSCHEMA_TRANS_IMPL_REQ);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_CONF, &Dbdict::execSCHEMA_TRANS_IMPL_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_IMPL_REF, &Dbdict::execSCHEMA_TRANS_IMPL_REF);

  addRecSignal(GSN_DICT_LOCK_REQ, &Dbdict::execDICT_LOCK_REQ);
  addRecSignal(GSN_DICT_UNLOCK_ORD, &Dbdict::execDICT_UNLOCK_ORD);
}//Dbdict::Dbdict()

Dbdict::~Dbdict() 
{
}//Dbdict::~Dbdict()

BLOCK_FUNCTIONS(Dbdict)

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
}//Dbdict::initCommonData()

void Dbdict::initRecords() 
{
  initNodeRecords();
  initPageRecords();
  initTableRecords();
  initTriggerRecords();
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

void Dbdict::initRestartRecord() 
{
  c_restartRecord.gciToRestart = 0;
  c_restartRecord.activeTable = ZNIL;
  c_restartRecord.m_pass = 0;
}//Dbdict::initRestartRecord()

void Dbdict::initNodeRecords() 
{
  jam();
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    NodeRecordPtr nodePtr;
    c_nodes.getPtr(nodePtr, i);
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

void Dbdict::initTableRecords() 
{
  TableRecordPtr tablePtr;
  while (1) {
    jam();
    refresh_watch_dog();
    c_tableRecordPool.seize(tablePtr);
    if (tablePtr.i == RNIL) {
      jam();
      break;
    }//if
    initialiseTableRecord(tablePtr);
  }//while
}//Dbdict::initTableRecords()

void Dbdict::initialiseTableRecord(TableRecordPtr tablePtr) 
{
  new (tablePtr.p) TableRecord();
  tablePtr.p->activePage = RNIL;
  tablePtr.p->filePtr[0] = RNIL;
  tablePtr.p->filePtr[1] = RNIL;
  tablePtr.p->firstPage = RNIL;
  tablePtr.p->tableId = tablePtr.i;
  tablePtr.p->tableVersion = (Uint32)-1;
  tablePtr.p->tabState = TableRecord::NOT_DEFINED;
  tablePtr.p->tabReturnState = TableRecord::TRS_IDLE;
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
  tablePtr.p->insertTriggerId = RNIL;
  tablePtr.p->updateTriggerId = RNIL;
  tablePtr.p->deleteTriggerId = RNIL;
  tablePtr.p->customTriggerId = RNIL;
  tablePtr.p->buildTriggerId = RNIL;
  tablePtr.p->indexLocal = 0;
}//Dbdict::initialiseTableRecord()

void Dbdict::initTriggerRecords()
{
  TriggerRecordPtr triggerPtr;
  while (1) {
    jam();
    refresh_watch_dog();
    c_triggerRecordPool.seize(triggerPtr);
    if (triggerPtr.i == RNIL) {
      jam();
      break;
    }//if
    initialiseTriggerRecord(triggerPtr);
  }//while
}

void Dbdict::initialiseTriggerRecord(TriggerRecordPtr triggerPtr)
{
  new (triggerPtr.p) TriggerRecord();
  triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;
  triggerPtr.p->triggerId = RNIL;
  triggerPtr.p->tableId = RNIL;
  triggerPtr.p->attributeMask.clear();
  triggerPtr.p->indexId = RNIL;
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
Uint32 Dbdict::getFreeObjId(Uint32 minId)
{
  const XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  Uint32 noOfPages = xsf->noOfPages;
  Uint32 n, i;
  for (n = 0; n < noOfPages; n++) {
    jam();
    const SchemaFile * sf = &xsf->schemaPage[n];
    for (i = 0; i < NDB_SF_PAGE_ENTRIES; i++) {
      const SchemaFile::TableEntry& te = sf->TableEntries[i];
      if (te.m_tableState == (Uint32)SchemaFile::INIT ||
          te.m_tableState == (Uint32)SchemaFile::DROP_TABLE_COMMITTED) {
        // minId is obsolete anyway
        if (minId <= n * NDB_SF_PAGE_ENTRIES + i)
          return n * NDB_SF_PAGE_ENTRIES + i;
      }
    }
  }
  return RNIL;
}

Uint32 Dbdict::getFreeTableRecord(Uint32 primaryTableId) 
{
  Uint32 minId = (primaryTableId == RNIL ? 0 : primaryTableId + 1);
  Uint32 i = getFreeObjId(minId);
  if (i == RNIL) {
    jam();
    return RNIL;
  }
  if (i >= c_tableRecordPool.getSize()) {
    jam();
    return RNIL;
  }
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, i);
  ndbrequire(tablePtr.p->tabState == TableRecord::NOT_DEFINED);
  initialiseTableRecord(tablePtr);
  tablePtr.p->tabState = TableRecord::DEFINING;
  return i;
}

Uint32 Dbdict::getFreeTriggerRecord()
{
  const Uint32 size = c_triggerRecordPool.getSize();
  TriggerRecordPtr triggerPtr;
  for (triggerPtr.i = 0; triggerPtr.i < size; triggerPtr.i++) {
    jam();
    c_triggerRecordPool.getPtr(triggerPtr);
    if (triggerPtr.p->triggerState == TriggerRecord::TS_NOT_DEFINED) {
      jam();
      initialiseTriggerRecord(triggerPtr);
      return triggerPtr.i;
    }
  }
  return RNIL;
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
  signal->theData[5] = ZNOMOREPHASES;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
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
  
  Uint32 attributesize, tablerecSize;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, 
					&c_maxNoOfTriggers));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_ATTRIBUTE,&attributesize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &tablerecSize));

  c_attributeRecordPool.setSize(attributesize);
  c_attributeRecordHash.setSize(64);
  c_fsConnectRecordPool.setSize(ZFS_CONNECT_SIZE);
  c_nodes.setSize(MAX_NDB_NODES);
  c_pageRecordArray.setSize(ZNUMBER_OF_PAGES);
  c_schemaPageRecordArray.setSize(2 * NDB_SF_MAX_PAGES);
  c_tableRecordPool.setSize(tablerecSize);
  g_key_descriptor_pool.setSize(tablerecSize);
  c_triggerRecordPool.setSize(c_maxNoOfTriggers);

  c_opSectionBufferPool.setSize(1024); // units OpSectionSegmentSize
  c_schemaOpPool.setSize(256);
  c_schemaOpHash.setSize(256);
  c_schemaTransPool.setSize(1);
  c_schemaTransHash.setSize(2);
  c_txHandlePool.setSize(2);
  c_txHandleHash.setSize(2);

  c_obj_pool.setSize(tablerecSize+c_maxNoOfTriggers);
  c_obj_hash.setSize((tablerecSize+c_maxNoOfTriggers+1)/2);
  m_dict_lock_pool.setSize(MAX_NDB_NODES);

  Pool_context pc;
  pc.m_block = this;

  c_file_hash.setSize(16);
  c_filegroup_hash.setSize(16);

  c_file_pool.init(RT_DBDICT_FILE, pc);
  c_filegroup_pool.init(RT_DBDICT_FILEGROUP, pc);

  // new OpRec pools
  c_createTableRecPool.setSize(32);
  c_dropTableRecPool.setSize(32);
  c_alterTableRecPool.setSize(32);
  c_createTriggerRecPool.setSize(32);
  c_dropTriggerRecPool.setSize(32);
  c_alterTriggerRecPool.setSize(32);
  
  c_opRecordPool.setSize(256);   // XXX need config params
  c_opCreateIndex.setSize(8);
  c_opCreateEvent.setSize(2);
  c_opSubEvent.setSize(2);
  c_opDropEvent.setSize(2);
  c_opSignalUtil.setSize(8);
  c_opDropIndex.setSize(8);
  c_opAlterIndex.setSize(8);
  c_opBuildIndex.setSize(8);

  // Initialize schema file copies
  c_schemaFile[0].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(0 * NDB_SF_MAX_PAGES);
  c_schemaFile[0].noOfPages = 0;
  c_schemaFile[1].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(1 * NDB_SF_MAX_PAGES);
  c_schemaFile[1].noOfPages = 0;

  c_schemaOperation.setSize(8);
  //c_opDropObj.setSize(8);
  c_Trans.setSize(8);

  Uint32 rps = 0;
  rps += tablerecSize * (MAX_TAB_NAME_SIZE + MAX_FRM_DATA_SIZE);
  rps += attributesize * (MAX_ATTR_NAME_SIZE + MAX_ATTR_DEFAULT_VALUE_SIZE);
  rps += c_maxNoOfTriggers * MAX_TAB_NAME_SIZE;
  rps += (10 + 10) * MAX_TAB_NAME_SIZE;

  Uint32 sm = 5;
  ndb_mgm_get_int_parameter(p, CFG_DB_STRING_MEMORY, &sm);
  if (sm == 0)
    sm = 5;
  
  Uint32 sb = 0;
  if (sm < 100)
  {
    sb = (rps * sm) / 100;
  }
  else
  {
    sb = sm;
  }
  
  c_rope_pool.setSize(sb/28 + 100);
  
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
    Ptr<DictObject> ptr;
    SLList<DictObject> objs(c_obj_pool);
    while(objs.seize(ptr))
      new (ptr.p) DictObject();
    objs.release();
  }

#ifdef VM_TRACE
  {
    FILE* debugFile = globalSignalLoggers.getOutputStream();
    if (debugFile != NULL)
      debugOut = *new NdbOut(*new FileOutputStream(debugFile));
  }
#endif
}//execSIZEALT_REP()

#ifdef VM_TRACE
bool
Dbdict::debugOutOn() const
{
  SignalLoggerManager::LogMode mask = SignalLoggerManager::LogInOut;
  return globalSignalLoggers.logMatch(DBDICT, mask);
}
#endif

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
    // uses c_restartType
    if(restartType == NodeState::ST_SYSTEM_RESTART &&
       c_masterNodeId == getOwnNodeId()){
      rebuildIndexes(signal, 0);
      return;
    }
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

/* ---------------------------------------------------------------- */
// HOT_SPAREREP informs DBDICT about which nodes that have become
// hot spare nodes.
/* ---------------------------------------------------------------- */
void Dbdict::execHOT_SPAREREP(Signal* signal) 
{
  Uint32 hotSpareNodes = 0;
  jamEntry();
  HotSpareRep * const hotSpare = (HotSpareRep*)&signal->theData[0];
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    if (NdbNodeBitmask::get(hotSpare->theHotSpareNodes, i)) {
      NodeRecordPtr nodePtr;
      c_nodes.getPtr(nodePtr, i);
      nodePtr.p->hotSpare = true;
      hotSpareNodes++;
    }//if
  }//for
  ndbrequire(hotSpareNodes == hotSpare->noHotSpareNodes);
  c_noHotSpareNodes = hotSpareNodes;
  return;
}//execHOT_SPAREREP()

void Dbdict::initSchemaFile(Signal* signal) 
{
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  xsf->noOfPages = (c_tableRecordPool.getSize() + NDB_SF_PAGE_ENTRIES - 1)
                   / NDB_SF_PAGE_ENTRIES;
  initSchemaFile(xsf, 0, xsf->noOfPages, true);
  // init alt copy too for INR
  XSchemaFile * oldxsf = &c_schemaFile[c_schemaRecord.oldSchemaPage != 0];
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
Dbdict::activateIndexes(Signal* signal, Uint32 i)
{
  AlterIndxReq* req = (AlterIndxReq*)signal->getDataPtrSend();
  TableRecordPtr tablePtr;
  for (; i < c_tableRecordPool.getSize(); i++) {
    tablePtr.i = i;
    c_tableRecordPool.getPtr(tablePtr);
    if (tablePtr.p->tabState != TableRecord::DEFINED)
      continue;
    if (! tablePtr.p->isIndex())
      continue;
    jam();
    req->setUserRef(reference());
    req->setConnectionPtr(i);
    req->setTableId(tablePtr.p->primaryTableId);
    req->setIndexId(tablePtr.i);
    req->setIndexVersion(tablePtr.p->tableVersion);
    req->setOnline(true);
    if (c_restartType == NodeState::ST_SYSTEM_RESTART) {
      if (c_masterNodeId != getOwnNodeId())
        continue;
      // from file index state is not defined currently
      req->setRequestType(AlterIndxReq::RT_SYSTEMRESTART);
      req->addRequestFlag((Uint32)RequestFlag::RF_NOBUILD);
    }
    else if (
        c_restartType == NodeState::ST_NODE_RESTART ||
        c_restartType == NodeState::ST_INITIAL_NODE_RESTART) {
      // from master index must be online
      if (tablePtr.p->indexState != TableRecord::IS_ONLINE)
        continue;
      req->setRequestType(AlterIndxReq::RT_NODERESTART);
      // activate locally, rebuild not needed
      req->addRequestFlag((Uint32)RequestFlag::RF_LOCAL);
      req->addRequestFlag((Uint32)RequestFlag::RF_NOBUILD);
    } else {
      ndbrequire(false);
    }
    sendSignal(reference(), GSN_ALTER_INDX_REQ,
      signal, AlterIndxReq::SignalLength, JBB);
    return;
  }
  signal->theData[0] = reference();
  sendSignal(c_restartRecord.returnBlockRef, GSN_DICTSTARTCONF,
	     signal, 1, JBB);
}

void
Dbdict::rebuildIndexes(Signal* signal, Uint32 i){
  BuildIndxReq* const req = (BuildIndxReq*)signal->getDataPtrSend();
  
  TableRecordPtr indexPtr;
  for (; i < c_tableRecordPool.getSize(); i++) {
    indexPtr.i = i;
    c_tableRecordPool.getPtr(indexPtr);
    if (indexPtr.p->tabState != TableRecord::DEFINED)
      continue;
    if (! indexPtr.p->isIndex())
      continue;

    jam();

    req->setUserRef(reference());
    req->setConnectionPtr(i);
    req->setRequestType(BuildIndxReq::RT_SYSTEMRESTART);
    req->setBuildId(0);   // not used
    req->setBuildKey(0);  // not used
    req->setIndexType(indexPtr.p->tableType);
    req->setIndexId(indexPtr.i);
    req->setTableId(indexPtr.p->primaryTableId);
    req->setParallelism(16);

    // from file index state is not defined currently
    if (indexPtr.p->m_bits & TableRecord::TR_Logged) {
      // rebuild not needed
      req->addRequestFlag((Uint32)RequestFlag::RF_NOBUILD);
    }
    
    // send
    sendSignal(reference(), GSN_BUILDINDXREQ,
	       signal, BuildIndxReq::SignalLength, JBB);
    return;
  }
  sendNDB_STTORRY(signal);
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

  c_restartRecord.m_pass = 0;
  c_restartRecord.activeTable = 0;
  c_schemaRecord.schemaPage = c_schemaRecord.oldSchemaPage; // ugly
  checkSchemaStatus(signal);
}//execDICTSTARTREQ()

void
Dbdict::masterRestart_checkSchemaStatusComplete(Signal* signal,
						Uint32 callbackData,
						Uint32 returnCode){

  c_schemaRecord.schemaPage = 0; // ugly
  XSchemaFile * oldxsf = &c_schemaFile[c_schemaRecord.oldSchemaPage != 0];
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

  XSchemaFile * newxsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
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
  
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
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

  SegmentedSectionPtr schemaDataPtr;
  signal->getSection(schemaDataPtr, 0);

  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  ndbrequire(schemaDataPtr.sz % NDB_SF_PAGE_SIZE_IN_WORDS == 0);
  xsf->noOfPages = schemaDataPtr.sz / NDB_SF_PAGE_SIZE_IN_WORDS;
  copy((Uint32*)&xsf->schemaPage[0], schemaDataPtr);
  releaseSections(signal);
  
  SchemaFile * sf0 = &xsf->schemaPage[0];
  if (sf0->NdbVersion < NDB_SF_VERSION_5_0_6) {
    bool ok = convertSchemaFileTo_5_0_6(xsf);
    ndbrequire(ok);
  }
    
  validateChecksum(xsf);

  XSchemaFile * oldxsf = &c_schemaFile[c_schemaRecord.oldSchemaPage != 0];
  resizeSchemaFile(xsf, oldxsf->noOfPages);

  ndbrequire(signal->getSendersBlockRef() != reference());
    
  /* ---------------------------------------------------------------- */
  // Synchronise our view on data with other nodes in the cluster.
  // This is an important part of restart handling where we will handle
  // cases where the table have been added but only partially, where
  // tables have been deleted but not completed the deletion yet and
  // other scenarios needing synchronisation.
  /* ---------------------------------------------------------------- */
  c_schemaRecord.m_callback.m_callbackData = 0;
  c_schemaRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restart_checkSchemaStatusComplete);

  c_restartRecord.m_pass= 0;
  c_restartRecord.activeTable = 0;
  checkSchemaStatus(signal);
}//execSCHEMA_INFO()

void
Dbdict::restart_checkSchemaStatusComplete(Signal * signal, 
					  Uint32 callbackData,
					  Uint32 returnCode)
{
  jam();
  D("restart_checkSchemaStatusComplete");

  ndbrequire(c_writeSchemaRecord.inUse == false);
  c_writeSchemaRecord.inUse = true;
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
  c_writeSchemaRecord.newFile = true;
  c_writeSchemaRecord.firstPage = 0;
  c_writeSchemaRecord.noOfPages = xsf->noOfPages;
  c_writeSchemaRecord.m_callback.m_callbackData = 0;
  c_writeSchemaRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restart_writeSchemaConf);
  
  for(Uint32 i = 0; i<xsf->noOfPages; i++)
    computeChecksum(xsf, i);  

  startWriteSchemaFile(signal);
}

void
Dbdict::restart_writeSchemaConf(Signal * signal, 
				Uint32 callbackData,
				Uint32 returnCode)
{
  jam();
  D("restart_writeSchemaConf");

  if(c_systemRestart){
    jam();
    signal->theData[0] = getOwnNodeId();
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_SCHEMA_INFOCONF,
	       signal, 1, JBB);
    return;
  }
  
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
    return pass == 0 || pass == 9 || pass == 10;
  case DictTabInfo::Tablespace:
    return pass == 1 || pass == 8 || pass == 11;
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    return pass == 2 || pass == 7 || pass == 12;
  case DictTabInfo::SystemTable:
  case DictTabInfo::UserTable:
    return /* pass == 3 || pass == 6 || */ pass == 13;
  case DictTabInfo::UniqueHashIndex:
  case DictTabInfo::HashIndex:
  case DictTabInfo::UniqueOrderedIndex:
  case DictTabInfo::OrderedIndex:
    return /* pass == 4 || pass == 5 || */ pass == 14;
  }
  
  return false;
}

static const Uint32 CREATE_OLD_PASS = 4;
static const Uint32 DROP_OLD_PASS = 9;
static const Uint32 CREATE_NEW_PASS = 14;
static const Uint32 LAST_PASS = 14;

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

/**
 * Pass 0  Create old LogfileGroup
 * Pass 1  Create old Tablespace
 * Pass 2  Create old Datafile/Undofile
 * Pass 3  Create old Table           // NOT DONE DUE TO DIH
 * Pass 4  Create old Index           // NOT DONE DUE TO DIH
 
 * Pass 5  Drop old Index             // NOT DONE DUE TO DIH
 * Pass 6  Drop old Table             // NOT DONE DUE TO DIH
 * Pass 7  Drop old Datafile/Undofile
 * Pass 8  Drop old Tablespace
 * Pass 9  Drop old Logfilegroup
 
 * Pass 10 Create new LogfileGroup
 * Pass 11 Create new Tablespace
 * Pass 12 Create new Datafile/Undofile
 * Pass 13 Create new Table
 * Pass 14 Create new Index
 */

void Dbdict::checkSchemaStatus(Signal* signal) 
{
  XSchemaFile * newxsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  XSchemaFile * oldxsf = &c_schemaFile[c_schemaRecord.oldSchemaPage != 0];
  ndbrequire(newxsf->noOfPages == oldxsf->noOfPages);
  const Uint32 noOfEntries = newxsf->noOfPages * NDB_SF_PAGE_ENTRIES;

  for (; c_restartRecord.activeTable < noOfEntries;
       c_restartRecord.activeTable++) {
    jam();

    Uint32 tableId = c_restartRecord.activeTable;
    SchemaFile::TableEntry *newEntry = getTableEntry(newxsf, tableId);
    SchemaFile::TableEntry *oldEntry = getTableEntry(oldxsf, tableId);
    SchemaFile::TableState newSchemaState = 
      (SchemaFile::TableState)newEntry->m_tableState;
    SchemaFile::TableState oldSchemaState = 
      (SchemaFile::TableState)oldEntry->m_tableState;

    if (c_restartRecord.activeTable >= c_tableRecordPool.getSize()) {
      jam();
      ndbrequire(newSchemaState == SchemaFile::INIT);
      ndbrequire(oldSchemaState == SchemaFile::INIT);
      continue;
    }//if

//#define PRINT_SCHEMA_RESTART
#ifdef PRINT_SCHEMA_RESTART
    char buf[100];
    snprintf(buf, sizeof(buf), "checkSchemaStatus: pass: %d table: %d", 
             c_restartRecord.m_pass, tableId);
#endif
    
    if (c_restartRecord.m_pass <= CREATE_OLD_PASS)
    {
      if (!::checkSchemaStatus(oldEntry->m_tableType, c_restartRecord.m_pass))
        continue;

      switch(oldSchemaState){
      case SchemaFile::INIT: jam();
      case SchemaFile::DROP_TABLE_COMMITTED: jam();
      case SchemaFile::ADD_STARTED: jam();
      case SchemaFile::DROP_TABLE_STARTED: jam();
      case SchemaFile::TEMPORARY_TABLE_COMMITTED: jam();
	continue;
      case SchemaFile::TABLE_ADD_COMMITTED: jam();
      case SchemaFile::ALTER_TABLE_COMMITTED: jam();
	jam();
#ifdef PRINT_SCHEMA_RESTART
        ndbout_c("%s -> restartCreateTab", buf);
        ndbout << *newEntry << " " << *oldEntry << endl;
#endif
	restartCreateTab(signal, tableId, oldEntry, oldEntry, true);        
        return;
      }
    }

    if (c_restartRecord.m_pass <= DROP_OLD_PASS)
    {
      if (!::checkSchemaStatus(oldEntry->m_tableType, c_restartRecord.m_pass))
        continue;

      switch(oldSchemaState){
      case SchemaFile::INIT: jam();
      case SchemaFile::DROP_TABLE_COMMITTED: jam();
      case SchemaFile::TEMPORARY_TABLE_COMMITTED: jam();
        continue;
      case SchemaFile::ADD_STARTED: jam();
      case SchemaFile::DROP_TABLE_STARTED: jam();
#ifdef PRINT_SCHEMA_RESTART
        ndbout_c("%s -> restartDropTab", buf);
        ndbout << *newEntry << " " << *oldEntry << endl;
#endif
	restartDropTab(signal, tableId, oldEntry, newEntry);
        return;
      case SchemaFile::TABLE_ADD_COMMITTED: jam();
      case SchemaFile::ALTER_TABLE_COMMITTED: jam();
        if (! (* oldEntry == * newEntry))
        {
#ifdef PRINT_SCHEMA_RESTART
          ndbout_c("%s -> restartDropTab", buf);
          ndbout << *newEntry << " " << *oldEntry << endl;
#endif
          restartDropTab(signal, tableId, oldEntry, newEntry);
          return;
        }
        continue;
      }
    }

    if (c_restartRecord.m_pass <= CREATE_NEW_PASS)
    {
      if (!::checkSchemaStatus(newEntry->m_tableType, c_restartRecord.m_pass))
        continue;
      
      switch(newSchemaState){
      case SchemaFile::INIT: jam();
      case SchemaFile::DROP_TABLE_COMMITTED: jam();
      case SchemaFile::TEMPORARY_TABLE_COMMITTED: jam();
        * oldEntry = * newEntry;
        continue;
      case SchemaFile::ADD_STARTED: jam();
      case SchemaFile::DROP_TABLE_STARTED: jam();
        ndbrequire(DictTabInfo::isTable(newEntry->m_tableType) ||
                   DictTabInfo::isIndex(newEntry->m_tableType));
        newEntry->m_tableState = SchemaFile::INIT;
        continue;
      case SchemaFile::TABLE_ADD_COMMITTED: jam();
      case SchemaFile::ALTER_TABLE_COMMITTED: jam();
        if (DictTabInfo::isIndex(newEntry->m_tableType) ||
            DictTabInfo::isTable(newEntry->m_tableType))
        {
          bool file = * oldEntry == *newEntry &&
            (!DictTabInfo::isIndex(newEntry->m_tableType) || c_systemRestart);

#ifdef PRINT_SCHEMA_RESTART          
          ndbout_c("%s -> restartCreateTab (file: %d)", buf, file);
          ndbout << *newEntry << " " << *oldEntry << endl;
#endif
          restartCreateTab(signal, tableId, newEntry, newEntry, file);        
          * oldEntry = * newEntry;
          return;
        }
        else if (! (* oldEntry == *newEntry))
        {
#ifdef PRINT_SCHEMA_RESTART
          ndbout_c("%s -> restartCreateTab", buf);
          ndbout << *newEntry << " " << *oldEntry << endl;
#endif
          restartCreateTab(signal, tableId, oldEntry, newEntry, false);        
          * oldEntry = * newEntry;
          return;
        }
        * oldEntry = * newEntry;
        continue;
      }
    }
  }
  
  c_restartRecord.m_pass++;
  c_restartRecord.activeTable= 0;
  if(c_restartRecord.m_pass <= LAST_PASS)
  {
    checkSchemaStatus(signal);
  }
  else
  {
    execute(signal, c_schemaRecord.m_callback, 0);
  }
}//checkSchemaStatus()

void
Dbdict::restartCreateTab(Signal* signal, Uint32 tableId, 
			 const SchemaFile::TableEntry * old_entry, 
			 const SchemaFile::TableEntry * new_entry, 
			 bool file)
{
  jam();
  D("restartCreateTab");

  switch(new_entry->m_tableType){
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
    restartCreateObj(signal, tableId, old_entry, new_entry, file);
    return;
  }
  
  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;  
  seizeSchemaOp(op_ptr, createTabPtr);
  ndbrequire(!op_ptr.isNull());
  CreateTabReq* impl_req = &createTabPtr.p->m_request;

  impl_req->senderRef = reference();
  impl_req->senderData = op_ptr.p->op_key;
  impl_req->requestType = 0;
  impl_req->tableId = tableId;
  impl_req->tableVersion = 0;
  impl_req->gci = 0;

  if(file && !ERROR_INSERTED(6002)){
    jam();
    
    c_readTableRecord.no_of_words = old_entry->m_info_words;
    c_readTableRecord.pageId = 0;
    c_readTableRecord.m_callback.m_callbackData = op_ptr.p->op_key;
    c_readTableRecord.m_callback.m_callbackFunction = 
      safe_cast(&Dbdict::restartCreateTab_readTableConf);
    
    startReadTableFile(signal, tableId);
    return;
  } else {
    
    ndbrequire(c_masterNodeId != getOwnNodeId());
    
    /**
     * Get from master
     */
    GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = op_ptr.p->op_key;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_GET_TABINFOREQ, signal,
	       GetTabInfoReq::SignalLength, JBB);

    if(ERROR_INSERTED(6002)){
      NdbSleep_MilliSleep(10);
      CRASH_INSERTION(6002);
    }
  }
}

void
Dbdict::restartCreateTab_readTableConf(Signal* signal, 
				       Uint32 op_key,
				       Uint32 ret)
{
  jam();
  D("restartCreateTab_readTableConf");
  
  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_readTableRecord.pageId);

  ParseDictTabInfoRecord parseRecord;
  parseRecord.requestType = DictTabInfo::GetTabInfoConf;
  parseRecord.errorCode = 0;
  
  Uint32 sz = c_readTableRecord.no_of_words;
  SimplePropertiesLinearReader r(pageRecPtr.p->word+ZPAGE_HEADER_SIZE, sz);
  handleTabInfoInit(r, &parseRecord);
  if (parseRecord.errorCode != 0)
  {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), 
                         "Failed to create table %u during restart,"
                         " error: %u line: %u."
			 " Most likely change of configuration",
			 c_readTableRecord.tableId,
			 parseRecord.errorCode,
                         parseRecord.errorLine);
    progError(__LINE__, 
	      NDBD_EXIT_INVALID_CONFIG,
	      buf);
    ndbrequire(parseRecord.errorCode == 0);
  }

  /* ---------------------------------------------------------------- */
  // We have read the table description from disk as part of system restart.
  // We will also write it back again to ensure that both copies are ok.
  /* ---------------------------------------------------------------- */
  ndbrequire(c_writeTableRecord.tableWriteState == WriteTableRecord::IDLE);
  c_writeTableRecord.no_of_words = c_readTableRecord.no_of_words;
  c_writeTableRecord.pageId = c_readTableRecord.pageId;
  c_writeTableRecord.tableWriteState = WriteTableRecord::TWR_CALLBACK;
  c_writeTableRecord.m_callback.m_callbackData = op_key;
  c_writeTableRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_writeTableConf);
  startWriteTableFile(signal, c_readTableRecord.tableId);
}

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
      ndbrequire(c_file_hash.find(fg_ptr, conf->tableId));
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
      ndbrequire(c_filegroup_hash.find(fg_ptr, conf->tableId));
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
      restartCreateObj_getTabInfoConf(signal);
    }
    return;
  }
  
  const Uint32 tableId = conf->tableId;
  const Uint32 senderData = conf->senderData;

  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;  
  findSchemaOp(op_ptr, createTabPtr, senderData);
  ndbrequire(!op_ptr.isNull());
  ndbrequire(createTabPtr.p->m_request.tableId == tableId);

  /**
   * Put data into table record
   */
  ParseDictTabInfoRecord parseRecord;
  parseRecord.requestType = DictTabInfo::GetTabInfoConf;
  parseRecord.errorCode = 0;
  
  SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());
  handleTabInfoInit(r, &parseRecord);
  ndbrequire(parseRecord.errorCode == 0);

  // save to disk

  ndbrequire(tableId < c_tableRecordPool.getSize());
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);
  tableEntry->m_info_words= tabInfoPtr.sz;
  
  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_writeTableConf);
  
  signal->header.m_noOfSections = 0;
  writeTableFile(signal, tableId, tabInfoPtr, &callback);
  signal->setSection(tabInfoPtr, 0);
  releaseSections(signal);
}

void
Dbdict::restartCreateTab_writeTableConf(Signal* signal, 
					Uint32 op_key,
					Uint32 ret)
{
  jam();
  D("restartCreateTab_writeTableConf");

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;  
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Callback callback;
  callback.m_callbackData = op_key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_dihComplete);

  OpSection& fragSection = op_ptr.p->m_section[CreateTabReq::FRAGMENTATION];
  new (&fragSection) OpSection;
  createTab_dih(signal, op_ptr, fragSection, &callback);
}

void
Dbdict::restartCreateTab_dihComplete(Signal* signal, 
				     Uint32 op_key,
				     Uint32 ret)
{
  jam();
  D("restartCreateTab_dihComplete");
  
  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;  
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  if (hasError(op_ptr.p->m_error))
  {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf),
                         "Failed to create table %u during restart,"
                         " error: %u line: %u.",
                         impl_req->tableId,
                         op_ptr.p->m_error.errorCode,
                         op_ptr.p->m_error.errorLine);
    progError(__LINE__, NDBD_EXIT_RESOURCE_ALLOC_ERROR, buf);
  }

  Callback callback;
  callback.m_callbackData = op_key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_activateComplete);
  
  createTab_activate(signal, op_ptr, &callback);
}

void
Dbdict::restartCreateTab_activateComplete(Signal* signal, 
					  Uint32 op_key,
					  Uint32 ret)
{
  jam();
  D("restartCreateTab_activateComplete");
  
  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;  
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, impl_req->tableId);
  tabPtr.p->tabState = TableRecord::DEFINED;
  
  releaseSchemaOp(op_ptr);

  c_restartRecord.activeTable++;
  checkSchemaStatus(signal);
}

void
Dbdict::restartDropTab(Signal* signal, Uint32 tableId,
                       const SchemaFile::TableEntry * old_entry, 
                       const SchemaFile::TableEntry * new_entry)
{
  switch(old_entry->m_tableType){
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
    restartDropObj(signal, tableId, old_entry);
    return;
  }

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;  
  seizeSchemaOp(op_ptr, dropTabPtr);
  ndbrequire(!op_ptr.isNull());
  DropTabReq* impl_req = &dropTabPtr.p->m_request;

  impl_req->senderRef = reference();
  impl_req->senderData = op_ptr.p->op_key;
  impl_req->requestType = DropTabReq::RestartDropTab;
  impl_req->tableId = tableId;
  impl_req->tableVersion = 0;

  dropTabPtr.p->m_block = 0;
  dropTabPtr.p->m_callback.m_callbackData = op_ptr.p->op_key;
  dropTabPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartDropTab_complete);
  dropTab_nextStep(signal, op_ptr);  
}

void
Dbdict::restartDropTab_complete(Signal* signal, 
				Uint32 op_key,
				Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;  
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  //@todo check error

  releaseTableObject(c_restartRecord.activeTable);
  releaseSchemaOp(op_ptr);

  c_restartRecord.activeTable++;
  checkSchemaStatus(signal);
}

/**
 * Create Obj during NR/SR
 */
void
Dbdict::restartCreateObj(Signal* signal, 
			 Uint32 tableId, 
			 const SchemaFile::TableEntry * old_entry,
			 const SchemaFile::TableEntry * new_entry,
			 bool file){
  jam();
  
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.seize(createObjPtr));
  
  const Uint32 key = ++c_opRecordSequence;
  createObjPtr.p->key = key;
  c_opCreateObj.add(createObjPtr);
  createObjPtr.p->m_errorCode = 0;
  createObjPtr.p->m_senderRef = reference();
  createObjPtr.p->m_senderData = tableId;
  createObjPtr.p->m_clientRef = reference();
  createObjPtr.p->m_clientData = tableId;
  
  createObjPtr.p->m_obj_id = tableId;
  createObjPtr.p->m_obj_type = new_entry->m_tableType;
  createObjPtr.p->m_obj_version = new_entry->m_tableVersion;

  createObjPtr.p->m_callback.m_callbackData = key;
  createObjPtr.p->m_callback.m_callbackFunction= 
    safe_cast(&Dbdict::restartCreateObj_prepare_start_done);
  
  createObjPtr.p->m_restart= file ? 1 : 2;
  switch(new_entry->m_tableType){
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
    createObjPtr.p->m_vt_index = 0;
    break;
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    createObjPtr.p->m_vt_index = 1;
    break;
  default:
    ndbrequire(false);
  }
  
  createObjPtr.p->m_obj_info_ptr_i = RNIL;
  if(file)
  {
    c_readTableRecord.no_of_words = old_entry->m_info_words;
    c_readTableRecord.pageId = 0;
    c_readTableRecord.m_callback.m_callbackData = key;
    c_readTableRecord.m_callback.m_callbackFunction = 
      safe_cast(&Dbdict::restartCreateObj_readConf);
    
    startReadTableFile(signal, tableId);
  }
  else
  {
    /**
     * Get from master
     */
    GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = key;
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

  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();

  const Uint32 objId = conf->tableId;
  const Uint32 senderData = conf->senderData;
  
  SegmentedSectionPtr objInfoPtr;
  signal->getSection(objInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, senderData));
  ndbrequire(createObjPtr.p->m_obj_id == objId);
  
  createObjPtr.p->m_obj_info_ptr_i= objInfoPtr.i;
  signal->header.m_noOfSections = 0;
  
  (this->*f_dict_op[createObjPtr.p->m_vt_index].m_prepare_start)
    (signal, createObjPtr.p);
}

void
Dbdict::restartCreateObj_readConf(Signal* signal,
				  Uint32 callbackData, 
				  Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);

  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_readTableRecord.pageId);
  
  Uint32 sz = c_readTableRecord.no_of_words;

  Ptr<SectionSegment> ptr;
  ndbrequire(import(ptr, pageRecPtr.p->word+ZPAGE_HEADER_SIZE, sz));
  createObjPtr.p->m_obj_info_ptr_i= ptr.i;  
  
  if (f_dict_op[createObjPtr.p->m_vt_index].m_prepare_start)
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_prepare_start)
      (signal, createObjPtr.p);
  else
    execute(signal, createObjPtr.p->m_callback, 0);
}

void
Dbdict::restartCreateObj_prepare_start_done(Signal* signal,
					    Uint32 callbackData, 
					    Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);

  Callback callback;
  callback.m_callbackData = callbackData;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateObj_write_complete);
  
  SegmentedSectionPtr objInfoPtr;
  getSection(objInfoPtr, createObjPtr.p->m_obj_info_ptr_i);

  writeTableFile(signal, createObjPtr.p->m_obj_id, objInfoPtr, &callback);
}

void
Dbdict::restartCreateObj_write_complete(Signal* signal,
					Uint32 callbackData, 
					Uint32 returnCode)
{
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);
  
  SegmentedSectionPtr objInfoPtr;
  getSection(objInfoPtr, createObjPtr.p->m_obj_info_ptr_i);
  signal->setSection(objInfoPtr, 0);
  releaseSections(signal);
  createObjPtr.p->m_obj_info_ptr_i = RNIL;
  
  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateObj_prepare_complete_done);
  
  if (f_dict_op[createObjPtr.p->m_vt_index].m_prepare_complete)
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_prepare_complete)
      (signal, createObjPtr.p);
  else
    execute(signal, createObjPtr.p->m_callback, 0);
}

void
Dbdict::restartCreateObj_prepare_complete_done(Signal* signal,
					       Uint32 callbackData, 
					       Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);

  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateObj_commit_start_done);

  if (f_dict_op[createObjPtr.p->m_vt_index].m_commit_start)
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_commit_start)
      (signal, createObjPtr.p);
  else
    execute(signal, createObjPtr.p->m_callback, 0);
}

void
Dbdict::restartCreateObj_commit_start_done(Signal* signal,
					   Uint32 callbackData, 
					   Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);

  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateObj_commit_complete_done);

  if (f_dict_op[createObjPtr.p->m_vt_index].m_commit_complete)
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_commit_complete)
      (signal, createObjPtr.p);
  else
    execute(signal, createObjPtr.p->m_callback, 0);
}  


void
Dbdict::restartCreateObj_commit_complete_done(Signal* signal,
					      Uint32 callbackData, 
					      Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  ndbrequire(createObjPtr.p->m_errorCode == 0);
  
  c_opCreateObj.release(createObjPtr);

  c_restartRecord.activeTable++;
  checkSchemaStatus(signal);
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
  
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.seize(dropObjPtr));
  
  const Uint32 key = ++c_opRecordSequence;
  dropObjPtr.p->key = key;
  c_opDropObj.add(dropObjPtr);
  dropObjPtr.p->m_errorCode = 0;
  dropObjPtr.p->m_senderRef = reference();
  dropObjPtr.p->m_senderData = tableId;
  dropObjPtr.p->m_clientRef = reference();
  dropObjPtr.p->m_clientData = tableId;
  
  dropObjPtr.p->m_obj_id = tableId;
  dropObjPtr.p->m_obj_type = entry->m_tableType;
  dropObjPtr.p->m_obj_version = entry->m_tableVersion;

  dropObjPtr.p->m_callback.m_callbackData = key;
  dropObjPtr.p->m_callback.m_callbackFunction= 
    safe_cast(&Dbdict::restartDropObj_prepare_start_done);

  ndbout_c("Dropping %d %d", tableId, entry->m_tableType);
  switch(entry->m_tableType){
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:{
    jam();
    Ptr<Filegroup> fg_ptr;
    ndbrequire(c_filegroup_hash.find(fg_ptr, tableId));
    dropObjPtr.p->m_obj_ptr_i = fg_ptr.i;
    dropObjPtr.p->m_vt_index = 3;
    break;
  }
  case DictTabInfo::Datafile:{
    jam();
    Ptr<File> file_ptr;
    dropObjPtr.p->m_vt_index = 2;
    ndbrequire(c_file_hash.find(file_ptr, tableId));
    dropObjPtr.p->m_obj_ptr_i = file_ptr.i;
    break;
  }
  case DictTabInfo::Undofile:{
    jam();    
    Ptr<File> file_ptr;
    dropObjPtr.p->m_vt_index = 4;
    ndbrequire(c_file_hash.find(file_ptr, tableId));
    dropObjPtr.p->m_obj_ptr_i = file_ptr.i;

    /**
     * Undofiles are only removed from logfile groups file list
     *   as drop undofile is currently not supported...
     *   file will be dropped by lgman when dropping filegroup
     */
    dropObjPtr.p->m_callback.m_callbackFunction= 
      safe_cast(&Dbdict::restartDropObj_commit_complete_done);
    
    if (f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
      (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
        (signal, dropObjPtr.p);
    else
      execute(signal, dropObjPtr.p->m_callback, 0);
    return;
  }
  default:
    jamLine(entry->m_tableType);
    ndbrequire(false);
  }
  
  if (f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_start)
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_start)
      (signal, dropObjPtr.p);
  else
    execute(signal, dropObjPtr.p->m_callback, 0);
}

void
Dbdict::restartDropObj_prepare_start_done(Signal* signal,
                                          Uint32 callbackData, 
                                          Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  ndbrequire(dropObjPtr.p->m_errorCode == 0);
  
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartDropObj_prepare_complete_done);
  
  if (f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_complete)
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_complete)
      (signal, dropObjPtr.p);
  else
    execute(signal, dropObjPtr.p->m_callback, 0);
}

void
Dbdict::restartDropObj_prepare_complete_done(Signal* signal,
                                             Uint32 callbackData, 
                                             Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  ndbrequire(dropObjPtr.p->m_errorCode == 0);
  
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartDropObj_commit_start_done);
  
  if (f_dict_op[dropObjPtr.p->m_vt_index].m_commit_start)
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_commit_start)
      (signal, dropObjPtr.p);
  else
    execute(signal, dropObjPtr.p->m_callback, 0);
}

void
Dbdict::restartDropObj_commit_start_done(Signal* signal,
                                         Uint32 callbackData, 
                                         Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  ndbrequire(dropObjPtr.p->m_errorCode == 0);
  
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartDropObj_commit_complete_done);

  if (f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
      (signal, dropObjPtr.p);
  else
    execute(signal, dropObjPtr.p->m_callback, 0);
}  


void
Dbdict::restartDropObj_commit_complete_done(Signal* signal,
                                            Uint32 callbackData, 
                                            Uint32 returnCode)
{
  jam();
  ndbrequire(returnCode == 0);
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  ndbrequire(dropObjPtr.p->m_errorCode == 0);
  
  c_opDropObj.release(dropObjPtr);

  c_restartRecord.activeTable++;
  checkSchemaStatus(signal);
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

/* ---------------------------------------------------------------- */
// We receive a report of an API that failed.
/* ---------------------------------------------------------------- */
void Dbdict::execAPI_FAILREQ(Signal* signal) 
{
  jamEntry();
  Uint32 failedApiNode = signal->theData[0];
  BlockReference retRef = signal->theData[1];

#if 0
  Uint32 userNode = refToNode(c_connRecord.userBlockRef);
  if (userNode == failedApiNode) {
    jam();
    c_connRecord.userBlockRef = (Uint32)-1;
  }//if
#endif

  // sends API_FAILCONF when done
  handleApiFail(signal, failedApiNode, retRef);
}

/* ---------------------------------------------------------------- */
// We receive a report of one or more node failures of kernel nodes.
/* ---------------------------------------------------------------- */
void Dbdict::execNODE_FAILREP(Signal* signal) 
{
  jamEntry();
  NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

  c_failureNr    = nodeFail->failNo;
  const Uint32 numberOfFailedNodes  = nodeFail->noOfNodes;
  const bool masterFailed = (c_masterNodeId != nodeFail->masterNodeId);
  c_masterNodeId = nodeFail->masterNodeId;

  c_noNodesFailed += numberOfFailedNodes;
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  memcpy(theFailedNodes, nodeFail->theNodes, sizeof(theFailedNodes));

  c_counterMgr.execNODE_FAILREP(signal);

  if (masterFailed)
  {
    jam();
    if(c_opRecordPool.getSize() != 
       (c_opRecordPool.getNoOfFree() + 
	c_opSubEvent.get_count() + c_opCreateEvent.get_count() +
	c_opDropEvent.get_count() + c_opSignalUtil.get_count()))
    {
      jam();
      UtilLockReq lockReq;
      lockReq.senderRef = reference();
      lockReq.senderData = 1;
      lockReq.lockId = 0;
      lockReq.requestInfo = UtilLockReq::SharedLock;
      lockReq.extra = DictLockReq::NodeFailureLock;
      m_dict_lock.lock(m_dict_lock_pool, &lockReq, 0);
    }
  }
  
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, i)) {
      jam();
      NodeRecordPtr nodePtr;
      c_nodes.getPtr(nodePtr, i);

      // update schema transactions (does not time-slice)
      handleTransSlaveFail(signal, i);

      nodePtr.p->nodeState = NodeRecord::NDB_NODE_DEAD;
      NFCompleteRep * const nfCompRep = (NFCompleteRep *)&signal->theData[0];
      nfCompRep->blockNo      = DBDICT;
      nfCompRep->nodeId       = getOwnNodeId();
      nfCompRep->failedNodeId = nodePtr.i;
      sendSignal(DBDIH_REF, GSN_NF_COMPLETEREP, signal, 
		 NFCompleteRep::SignalLength, JBB);
      
      c_aliveNodes.clear(i);
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

}//execNODE_FAILREP()


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
  DLHashTable<DictObject>::Iterator iter;
  bool moreTables = c_obj_hash.first(iter);
  printf("OBJECTS IN DICT:\n");
  char name[MAX_TAB_NAME_SIZE];
  while (moreTables) {
    Ptr<DictObject> tablePtr = iter.curr;
    ConstRope r(c_rope_pool, tablePtr.p->m_name);
    r.copy(name);
    printf("%s ", name); 
    moreTables = c_obj_hash.next(iter);
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
  DictObject key;
  key.m_key.m_name_ptr = name;
  key.m_key.m_name_len = len;
  key.m_key.m_pool = &c_rope_pool;
  key.m_name.m_hash = hash;
  Ptr<DictObject> old_ptr;
  c_obj_hash.find(old_ptr, key);
  return old_ptr.p;
}

//wl3600_todo remove the duplication
bool
Dbdict::get_object(DictObjectPtr& obj_ptr, const char* name, Uint32 len, Uint32 hash)
{
  DictObject key;
  key.m_key.m_name_ptr = name;
  key.m_key.m_name_len = len;
  key.m_key.m_pool = &c_rope_pool;
  key.m_name.m_hash = hash;
  return c_obj_hash.find(obj_ptr, key);
}

void
Dbdict::release_object(Uint32 obj_ptr_i, DictObject* obj_ptr_p){
  Rope name(c_rope_pool, obj_ptr_p->m_name);
  name.erase();

  Ptr<DictObject> ptr = { obj_ptr_p, obj_ptr_i };
  c_obj_hash.release(ptr);
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

void Dbdict::handleTabInfoInit(SimpleProperties::Reader & it,
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
  const Uint32 tableNameLength = strlen(c_tableDesc.TableName) + 1;
  const Uint32 name_hash = Rope::hash(c_tableDesc.TableName, tableNameLength);

  if(checkExist){
    jam();
    tabRequire(get_object(c_tableDesc.TableName, tableNameLength) == 0, 
	       CreateTableRef::TableAlreadyExist);
  }
  
  TableRecordPtr tablePtr;
  switch (parseP->requestType) {
  case DictTabInfo::CreateTableFromAPI: {
    jam();
  }
  case DictTabInfo::AlterTableFromAPI:{
    jam();
    tablePtr.i = getFreeTableRecord(c_tableDesc.PrimaryTableId);
    /* ---------------------------------------------------------------- */
    // Check if no free tables existed.
    /* ---------------------------------------------------------------- */
    tabRequire(tablePtr.i != RNIL, CreateTableRef::NoMoreTableRecords);
    
    c_tableRecordPool.getPtr(tablePtr);
    break;
  }
  case DictTabInfo::AddTableFromDict:
  case DictTabInfo::ReadTableFromDiskSR:
  case DictTabInfo::GetTabInfoConf:
  {
/* ---------------------------------------------------------------- */
// Get table id and check that table doesn't already exist
/* ---------------------------------------------------------------- */
    tablePtr.i = c_tableDesc.TableId;
    
    if (parseP->requestType == DictTabInfo::ReadTableFromDiskSR) {
      ndbrequire(tablePtr.i == c_restartRecord.activeTable);
    }//if
    if (parseP->requestType == DictTabInfo::GetTabInfoConf) {
      ndbrequire(tablePtr.i == c_restartRecord.activeTable);
    }//if
    
    c_tableRecordPool.getPtr(tablePtr);
    ndbrequire(tablePtr.p->tabState == TableRecord::NOT_DEFINED);
    
    //Uint32 oldTableVersion = tablePtr.p->tableVersion;
    initialiseTableRecord(tablePtr);
    if (parseP->requestType == DictTabInfo::AddTableFromDict) {
      jam();
      tablePtr.p->tabState = TableRecord::DEFINING;
    }//if

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
  parseP->tablePtr = tablePtr;
  
  { 
    Rope name(c_rope_pool, tablePtr.p->tableName);
    tabRequire(name.assign(c_tableDesc.TableName, tableNameLength, name_hash),
	       CreateTableRef::OutOfStringBuffer);
  }

  Ptr<DictObject> obj_ptr;
  if (parseP->requestType != DictTabInfo::AlterTableFromAPI) {
    jam();
    ndbrequire(c_obj_hash.seize(obj_ptr));
    new (obj_ptr.p) DictObject;
    obj_ptr.p->m_id = tablePtr.i;
    obj_ptr.p->m_type = c_tableDesc.TableType;
    obj_ptr.p->m_name = tablePtr.p->tableName;
    obj_ptr.p->m_ref_count = 0;
    c_obj_hash.add(obj_ptr);
    tablePtr.p->m_obj_ptr_i = obj_ptr.i;

#if defined VM_TRACE || defined ERROR_INSERT
    ndbout_c("Dbdict: create name=%s,id=%u,obj_ptr_i=%d", 
	     c_tableDesc.TableName, tablePtr.i, tablePtr.p->m_obj_ptr_i);
#endif
  }
  
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
  tablePtr.p->m_bits |= 
    (c_tableDesc.TableTemporaryFlag ? TableRecord::TR_Temporary : 0);
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
  
  {
    Rope frm(c_rope_pool, tablePtr.p->frmData);
    tabRequire(frm.assign(c_tableDesc.FrmData, c_tableDesc.FrmLen),
	       CreateTableRef::OutOfStringBuffer);
    Rope range(c_rope_pool, tablePtr.p->rangeData);
    tabRequire(range.assign(c_tableDesc.RangeListData,
               c_tableDesc.RangeListDataLen),
	      CreateTableRef::OutOfStringBuffer);
    Rope fd(c_rope_pool, tablePtr.p->ngData);
    tabRequire(fd.assign((const char*)c_tableDesc.FragmentData,
                         c_tableDesc.FragmentDataLen),
	       CreateTableRef::OutOfStringBuffer);
    Rope ts(c_rope_pool, tablePtr.p->tsData);
    tabRequire(ts.assign((const char*)c_tableDesc.TablespaceData,
                         c_tableDesc.TablespaceDataLen),
	       CreateTableRef::OutOfStringBuffer);
  }
  
  c_fragDataLen = c_tableDesc.FragmentDataLen;
  memcpy(c_fragData, c_tableDesc.FragmentData,
         c_tableDesc.FragmentDataLen);

  if(c_tableDesc.PrimaryTableId != RNIL) {
    
    tablePtr.p->primaryTableId = c_tableDesc.PrimaryTableId;
    tablePtr.p->indexState = (TableRecord::IndexState)c_tableDesc.IndexState;
    tablePtr.p->insertTriggerId = c_tableDesc.InsertTriggerId;
    tablePtr.p->updateTriggerId = c_tableDesc.UpdateTriggerId;
    tablePtr.p->deleteTriggerId = c_tableDesc.DeleteTriggerId;
    tablePtr.p->customTriggerId = c_tableDesc.CustomTriggerId;
  } else {
    tablePtr.p->primaryTableId = RNIL;
    tablePtr.p->indexState = TableRecord::IS_UNDEFINED;
    tablePtr.p->insertTriggerId = RNIL;
    tablePtr.p->updateTriggerId = RNIL;
    tablePtr.p->deleteTriggerId = RNIL;
    tablePtr.p->customTriggerId = RNIL;
  }
  tablePtr.p->buildTriggerId = RNIL;
  tablePtr.p->indexLocal = 0;
  
  handleTabInfo(it, parseP, c_tableDesc);
  
  if(parseP->errorCode != 0)
  {
    /**
     * Release table
     */
    releaseTableObject(tablePtr.i, checkExist);
    return;
  }

  if (checkExist && tablePtr.p->m_tablespace_id != RNIL)
  {
    /**
     * Increase ref count
     */
    FilegroupPtr ptr;
    ndbrequire(c_filegroup_hash.find(ptr, tablePtr.p->m_tablespace_id));
    increase_ref_count(ptr.p->m_obj_ptr_i);
  }
}//handleTabInfoInit()

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

  LocalDLFifoList<AttributeRecord> list(c_attributeRecordPool, 
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
    const size_t len = strlen(attrDesc.AttributeName)+1;
    const Uint32 name_hash = Rope::hash(attrDesc.AttributeName, len);
    {
      AttributeRecord key;
      key.m_key.m_name_ptr = attrDesc.AttributeName;
      key.m_key.m_name_len = len;
      key.attributeName.m_hash = name_hash;
      key.m_key.m_pool = &c_rope_pool;
      Ptr<AttributeRecord> old_ptr;
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
      Rope name(c_rope_pool, attrPtr.p->attributeName);
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
    
    // XXX old test option, remove
    if(!attrDesc.AttributeKeyFlag && 
       tablePtr.i > 1 &&
       !tablePtr.p->isIndex())
    {
      //attrDesc.AttributeStorageType= NDB_STORAGETYPE_DISK;
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
      Rope defaultValue(c_rope_pool, attrPtr.p->defaultValue);
      defaultValue.assign(attrDesc.AttributeDefaultValue);
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

  if(tablePtr.p->m_tablespace_id != RNIL || counts[3] || counts[4])
  {
    FilegroupPtr tablespacePtr;
    if(!c_filegroup_hash.find(tablespacePtr, tablePtr.p->m_tablespace_id))
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

/* ---------------------------------------------------------------- */
// New global checkpoint created.
/* ---------------------------------------------------------------- */
void Dbdict::execWAIT_GCP_CONF(Signal* signal) 
{
#if 0
  TableRecordPtr tablePtr;
  jamEntry();
  WaitGCPConf* const conf = (WaitGCPConf*)&signal->theData[0];
  c_tableRecordPool.getPtr(tablePtr, c_connRecord.connTableId);
  tablePtr.p->gciTableCreated = conf->gcp;
  sendUpdateSchemaState(signal,
                        tablePtr.i,
                        SchemaFile::TABLE_ADD_COMMITTED,
                        c_connRecord.noOfPagesForTable,
                        conf->gcp);
#endif
}//execWAIT_GCP_CONF()

/* ---------------------------------------------------------------- */
// Refused new global checkpoint.
/* ---------------------------------------------------------------- */
void Dbdict::execWAIT_GCP_REF(Signal* signal) 
{
  jamEntry();
  WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];
/* ---------------------------------------------------------------- */
// Error Handling code needed
/* ---------------------------------------------------------------- */
  char buf[32];
  BaseString::snprintf(buf, sizeof(buf), "WAIT_GCP_REF ErrorCode=%d",
		       ref->errorCode);
  progError(__LINE__, NDBD_EXIT_NDBREQUIRE, buf);
}//execWAIT_GCP_REF()

// MODULE: CreateTable

const Dbdict::OpInfo
Dbdict::CreateTableRec::g_opInfo = {
  { 'C', 'T', 'a', 0 },
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
    ::release(ss_ptr);
    createTabPtr.p->m_tabInfoPtrI = RNIL;
  }
  if (createTabPtr.p->m_fragmentsPtrI != RNIL) {
    jam();
    SegmentedSectionPtr ss_ptr;
    getSection(ss_ptr, createTabPtr.p->m_fragmentsPtrI);
    ::release(ss_ptr);
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

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

  releaseSections(signal);

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

void
Dbdict::createTable_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("createTable_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  CreateTabReq* impl_req = &createTabPtr.p->m_request;

  if (checkSingleUserMode(trans_ptr.p->m_clientRef)) {
    jam();
    setError(error, CreateTableRef::SingleUser, __LINE__);
    return;
  }

  /*
   * Master parses client DictTabInfo (sec 0) into new table record.
   * DIH is called to create fragmentation.  The table record is
   * then dumped back into DictTabInfo to produce a canonical format.
   * The new DictTabInfo (sec 0) and the fragmentation (sec 1) are
   * sent to all participants when master parse returns.
   */
  if (trans_ptr.p->m_isMaster) {
    jam();

    // parse client DictTabInfo into new TableRecord

    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::CreateTableFromAPI;
    parseRecord.errorCode = 0;

    SegmentedSectionPtr ss_ptr;
    signal->getSection(ss_ptr, CreateTableReq::DICT_TAB_INFO);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());

    handleTabInfoInit(r, &parseRecord);
    releaseSections(signal);

    if (parseRecord.errorCode != 0) {
      jam();
      setError(error, parseRecord);
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

    // fill in table id and version
    impl_req->tableId = tabPtr.i;
    impl_req->tableVersion = tabPtr.p->tableVersion;

    // create fragmentation via DIH (no changes in DIH)

    CreateFragmentationReq* frag_req =
      (CreateFragmentationReq*)signal->getDataPtrSend();
    frag_req->senderRef = 0; // direct conf
    frag_req->senderData = RNIL;
    frag_req->primaryTableId = tabPtr.p->primaryTableId;
    frag_req->noOfFragments = tabPtr.p->fragmentCount;
    frag_req->fragmentationType = tabPtr.p->fragmentType;

    Uint32* frag_data32 = &signal->theData[25];
    Uint16* frag_data = (Uint16*)frag_data32;
    MEMCOPY_NO_WORDS(frag_data, c_fragData, c_fragDataLen);

    if (tabPtr.p->isOrderedIndex()) {
      jam();
      // ordered index has same fragmentation as the table
      frag_req->primaryTableId = tabPtr.p->primaryTableId;
      frag_req->fragmentationType = DictTabInfo::DistrKeyOrderedIndex;
    } else if (tabPtr.p->isHashIndex()) {
      jam();
      /*
       * Unique hash indexes has same amount of fragments as primary table
       * and distributed in the same manner but has always a normal hash
       * fragmentation.
       */
      frag_req->primaryTableId = tabPtr.p->primaryTableId;
      frag_req->fragmentationType = DictTabInfo::DistrKeyUniqueHashIndex;
    } else {
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
    if (signal->theData[0] != 0) {
      jam();
      setError(error, signal->theData[0], __LINE__);
      return;
    }

    // save fragmentation in long signal memory
    {
      // wl3600_todo make a method for this magic stuff
      Uint32 count = 2 + (1 + frag_data[0]) * frag_data[1];

      Ptr<SectionSegment> frag_ptr;
      bool ok = ::import(frag_ptr, frag_data32, (count + 1) / 2);
      ndbrequire(ok);
      createTabPtr.p->m_fragmentsPtrI = frag_ptr.i;

      // save fragment count
      tabPtr.p->fragmentCount = frag_data[1];
    }

    // update table version
    {
      XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
      SchemaFile::TableEntry * tabEntry = getTableEntry(xsf, tabPtr.i);

      impl_req->tableVersion =
        tabPtr.p->tableVersion =
        create_obj_inc_schema_version(tabEntry->m_tableVersion);
    }

    // dump table record back into DictTabInfo
    {
      SimplePropertiesSectionWriter w(getSectionSegmentPool());
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

      ndbrequire(signal->getNoOfSections() == 0);
      signal->setSection(ss0_ptr, CreateTabReq::DICT_TAB_INFO);
      signal->setSection(ss1_ptr, CreateTabReq::FRAGMENTATION);

      // signal owns the memory now
      createTabPtr.p->m_tabInfoPtrI = RNIL;
      createTabPtr.p->m_fragmentsPtrI = RNIL;
    }
  }

  const Uint32 gci = impl_req->gci;
  const Uint32 tableId = impl_req->tableId;
  const Uint32 tableVersion = impl_req->tableVersion;

  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, CreateTabReq::DICT_TAB_INFO);

  // wl3600_todo parse the rewritten DictTabInfo in master too
  if (!trans_ptr.p->m_isMaster) {
    jam();
    createTabPtr.p->m_request.tableId = tableId;

    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::AddTableFromDict;
    parseRecord.errorCode = 0;

    SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());

    bool checkExist = true;
    handleTabInfoInit(r, &parseRecord, checkExist);

    if (parseRecord.errorCode != 0) {
      jam();
      setError(error, parseRecord);
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

  // save sections to DICT memory
  saveOpSection(op_ptr, signal, CreateTabReq::DICT_TAB_INFO);
  saveOpSection(op_ptr, signal, CreateTabReq::FRAGMENTATION);

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, tableId);
  tabPtr.p->packedSize = tabInfoPtr.sz;
  // wl3600_todo verify version on slave
  tabPtr.p->tableVersion = tableVersion;
  tabPtr.p->gciTableCreated = gci;

  // save original schema file entry
  {
    XSchemaFile* xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    SchemaFile::TableEntry* tableEntry = getTableEntry(xsf, tableId);
    createTabPtr.p->m_orig_entry = *tableEntry;
  }

  // update schema file entry in memory
  {
    SchemaFile::TableEntry& te = createTabPtr.p->m_curr_entry;
    te.init();
    te.m_tableState = SchemaFile::CREATE_PARSED;
    te.m_tableVersion = tableVersion;
    te.m_tableType = tabPtr.p->tableType;
    te.m_info_words = tabInfoPtr.sz;
    te.m_gcp = gci;
    te.m_transId = trans_ptr.p->m_transId;

    bool savetodisk = false;
    updateSchemaState(signal, tableId, &te, (Callback*)0, savetodisk, 1);
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
  c_tableRecordPool.getPtr(tabPtr, tableId);

  D("createTable_prepare" << *op_ptr.p);

  // update schema file entry on disk
  SchemaFile::TableEntry& te = createTabPtr.p->m_curr_entry;
  te.m_tableState = SchemaFile::ADD_STARTED;

  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction =
    safe_cast(&Dbdict::createTab_writeSchemaConf1);

  bool savetodisk = !(tabPtr.p->m_bits & TableRecord::TR_Temporary);
  updateSchemaState(signal, tableId, &te, &callback, savetodisk, 1);
}

void
Dbdict::createTab_writeSchemaConf1(Signal* signal,
                                   Uint32 op_key,
                                   Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction =
    safe_cast(&Dbdict::createTab_writeTableConf);

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_request.tableId);
  bool savetodisk = !(tabPtr.p->m_bits & TableRecord::TR_Temporary);
  if (savetodisk)
  {
    const OpSection& tabInfoSec =
      getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);
    writeTableFile(signal, createTabPtr.p->m_request.tableId,
                   tabInfoSec, &callback);
  }
  else
  {
    execute(signal, callback, 0);
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
  const OpSection& fragSec =
    getOpSection(op_ptr, CreateTabReq::FRAGMENTATION);
  ndbrequire(fragSec.getSize() > 0);

  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction =
    safe_cast(&Dbdict::createTab_dihComplete);

  createTab_dih(signal, op_ptr, fragSec, &callback);
}

void
Dbdict::createTab_dih(Signal* signal,
		      SchemaOpPtr op_ptr,
		      OpSection fragSec,
		      Callback * c)
{
  jam();
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  D("createTab_dih" << *op_ptr.p);

  createTabPtr.p->m_callback = * c;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_request.tableId);

  DiAddTabReq * req = (DiAddTabReq*)signal->getDataPtrSend();
  req->connectPtr = op_ptr.p->op_key;
  req->tableId = tabPtr.i;
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

  // fragmentation in long signal section
  {
    Uint32 page[1024];
    LinearSectionPtr ptr[3];
    Uint32 noOfSections = 0;

    const Uint32 size = fragSec.getSize();

    // wl3600_todo add ndbrequire on SR, NR
    if (size != 0) {
      jam();
      bool ok = copyOut(fragSec, page, 1024);
      ndbrequire(ok);
      ptr[noOfSections].sz = size;
      ptr[noOfSections].p = page;
      noOfSections++;
    }

    sendSignal(DBDIH_REF, GSN_DIADDTABREQ, signal,
               DiAddTabReq::SignalLength, JBB,
               ptr, noOfSections);
  }
  /**
   * Create KeyDescriptor
   */
  KeyDescriptor* desc= g_key_descriptor_pool.getPtr(tabPtr.i);
  new (desc) KeyDescriptor();

  Uint32 key = 0;
  Ptr<AttributeRecord> attrPtr;
  LocalDLFifoList<AttributeRecord> list(c_attributeRecordPool,
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

static
void
calcLHbits(Uint32 * lhPageBits, Uint32 * lhDistrBits,
	   Uint32 fid, Uint32 totalFragments)
{
  Uint32 distrBits = 0;
  Uint32 pageBits = 0;

  Uint32 tmp = 1;
  while (tmp < totalFragments) {
    jam();
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

  ndbrequire(node == getOwnNodeId());

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, senderData);
  ndbrequire(!op_ptr.isNull());

  createTabPtr.p->m_dihAddFragPtr = dihPtr;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, tableId);

#if 0
  tabPtr.p->gciTableCreated = (startGci > tabPtr.p->gciTableCreated ? startGci:
			       startGci > tabPtr.p->gciTableCreated);
#endif

  /**
   * Calc lh3PageBits
   */
  Uint32 lhDistrBits = 0;
  Uint32 lhPageBits = 0;
  ::calcLHbits(&lhPageBits, &lhDistrBits, fragId, fragCount);

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
    req->noOfAttributes = tabPtr.p->noOfAttributes;
    req->noOfNullAttributes = tabPtr.p->noOfNullBits;
    req->maxRowsLow = maxRows & 0xFFFFFFFF;
    req->maxRowsHigh = maxRows >> 32;
    req->minRowsLow = minRows & 0xFFFFFFFF;
    req->minRowsHigh = minRows >> 32;
    req->schemaVersion = tabPtr.p->tableVersion;
    Uint32 keyLen = tabPtr.p->tupKeyLength;
    req->keyLength = keyLen; // wl-2066 no more "long keys"
    req->nextLCP = lcpNo;

    req->noOfKeyAttr = tabPtr.p->noOfPrimkey;
    req->noOfCharsets = tabPtr.p->noOfCharsets;
    req->checksumIndicator = 1;
    req->GCPIndicator = 1;
    req->startGci = startGci;
    req->tableType = tabPtr.p->tableType;
    req->primaryTableId = tabPtr.p->primaryTableId;
    req->tablespace_id= tabPtr.p->m_tablespace_id;
    req->tablespace_id = tabPtr.p->m_tablespace_id;
    req->logPartId = logPart;
    req->forceVarPartFlag = !!(tabPtr.p->m_bits& TableRecord::TR_ForceVarPart);
    sendSignal(DBLQH_REF, GSN_LQHFRAGREQ, signal,
	       LqhFragReq::SignalLength, JBB);
  }
}

void
Dbdict::execLQHFRAGCONF(Signal * signal)
{
  jamEntry();
  LqhFragConf * const conf = (LqhFragConf*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, conf->senderData);
  ndbrequire(!op_ptr.isNull());

  createTabPtr.p->m_lqhFragPtr = conf->lqhFragPtr;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_request.tableId);
  sendLQHADDATTRREQ(signal, op_ptr, tabPtr.p->m_attributes.firstItem);
}

void
Dbdict::execLQHFRAGREF(Signal * signal)
{
  jamEntry();
  LqhFragRef * const ref = (LqhFragRef*)signal->getDataPtr();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, ref->senderData);
  ndbrequire(!op_ptr.isNull());

  setError(op_ptr, ref->errorCode, __LINE__);

  {
    AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();
    ref->dihPtr = createTabPtr.p->m_dihAddFragPtr;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGREF, signal,
	       AddFragRef::SignalLength, JBB);
  }
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
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_request.tableId);
  LqhAddAttrReq * const req = (LqhAddAttrReq*)signal->getDataPtrSend();
  Uint32 i = 0;
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
    attributePtrI = attrPtr.p->nextList;
  }
  req->lqhFragPtr = createTabPtr.p->m_lqhFragPtr;
  req->senderData = op_ptr.p->op_key;
  req->senderAttrPtr = attributePtrI;
  req->noOfAttributes = i;

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

  const Uint32 fragId = conf->fragId;
  const Uint32 nextAttrPtr = conf->senderAttrPtr;
  if(nextAttrPtr != RNIL){
    jam();
    sendLQHADDATTRREQ(signal, op_ptr, nextAttrPtr);
    return;
  }

  {
    AddFragConf * const conf = (AddFragConf*)signal->getDataPtr();
    conf->dihPtr = createTabPtr.p->m_dihAddFragPtr;
    conf->fragId = fragId;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGCONF, signal,
	       AddFragConf::SignalLength, JBB);
  }
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

  {
    AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();
    ref->dihPtr = createTabPtr.p->m_dihAddFragPtr;
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

  if(createTabPtr.p->m_dihAddFragPtr != RNIL){
    jam();

    /**
     * We did perform at least one LQHFRAGREQ
     */
    sendSignal(DBLQH_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
    return;
  } else {
    /**
     * No local fragment (i.e. no LQHFRAGREQ)
     */
    execute(signal, createTabPtr.p->m_callback, 0);
    return;
    //sendSignal(DBDIH_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
  }
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
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_request.tableId);

  if (refToBlock(signal->getSendersBlockRef()) == DBLQH) {
    jam();
    // prepare table in DBTC
    signal->theData[0] = tabPtr.i;
    signal->theData[1] = tabPtr.p->tableVersion;
    signal->theData[2] = (Uint32)!!(tabPtr.p->m_bits & TableRecord::TR_Logged);
    signal->theData[3] = reference();
    signal->theData[4] = (Uint32)tabPtr.p->tableType;
    signal->theData[5] = op_ptr.p->op_key;
    signal->theData[6] = (Uint32)tabPtr.p->noOfPrimkey;
    signal->theData[7] = (Uint32)tabPtr.p->singleUserMode;

    sendSignal(DBTC_REF, GSN_TC_SCHVERREQ, signal, 8, JBB);
    return;
  }

  if (refToBlock(signal->getSendersBlockRef()) == DBDIH) {
    jam();
    // commit table in DBTC
    signal->theData[0] = op_ptr.p->op_key;
    signal->theData[1] = reference();
    signal->theData[2] = tabPtr.i;

    sendSignal(DBTC_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
    return;
  }

  if (refToBlock(signal->getSendersBlockRef()) == DBTC) {
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
Dbdict::createTab_dihComplete(Signal* signal,
			      Uint32 op_key,
			      Uint32 ret)
{
  jam();
  D("createTab_dihComplete");

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;

  //@todo check for master failed

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
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);

  Uint32 tableId = createTabPtr.p->m_request.tableId;
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, tableId);

  D("createTable_commit" << V(itRepeat) << *op_ptr.p);

  if (itRepeat == 0) {
    jam();

    // take mutex at commit start

    if (trans_ptr.p->m_isMaster) {
      jam();
      Mutex mutex(signal, c_mutexMgr, createTabPtr.p->m_startLcpMutex);
      Callback c = {
        safe_cast(&Dbdict::createTab_startLcpMutex_locked),
        op_ptr.p->op_key
      };
      bool ok = mutex.lock(c);
      ndbrequire(ok);
      return;
    }

    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
    return;
  }

  if (itRepeat == 1) {
    jam();
    bool savetodisk = !(tabPtr.p->m_bits & TableRecord::TR_Temporary);

    SchemaFile::TableEntry& te = createTabPtr.p->m_curr_entry;
    if (savetodisk)
      te.m_tableState = SchemaFile::TABLE_ADD_COMMITTED;
    else
      te.m_tableState = SchemaFile::TEMPORARY_TABLE_COMMITTED;
    te.m_transId = 0;

    Callback callback;
    callback.m_callbackData = op_ptr.p->op_key;
    callback.m_callbackFunction =
      safe_cast(&Dbdict::createTab_writeSchemaConf2);

    updateSchemaState(signal, tabPtr.i, &te, &callback, savetodisk, 1);
    return;
  }

  ndbrequire(itRepeat == 2);
  {
    jam();

    // release mutex at commit end

    if (trans_ptr.p->m_isMaster) {
      jam();
      Mutex mutex(signal, c_mutexMgr, createTabPtr.p->m_startLcpMutex);
      Callback c = {
        safe_cast(&Dbdict::createTab_startLcpMutex_unlocked),
        op_ptr.p->op_key
      };
      mutex.unlock(c);
      return;
    }

    sendTransConf(signal, trans_ptr);
    return;
  }
}

void
Dbdict::createTab_startLcpMutex_locked(Signal* signal,
				       Uint32 op_key,
				       Uint32 ret)
{
  jamEntry();
  D("createTab_startLcpMutex_locked");

  ndbrequire(ret == 0);

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
  sendTransConf(signal, op_ptr, itFlags);
}

void
Dbdict::createTab_writeSchemaConf2(Signal* signal,
				   Uint32 op_key,
				   Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Callback c;
  c.m_callbackData = op_ptr.p->op_key;
  c.m_callbackFunction = safe_cast(&Dbdict::createTab_alterComplete);
  createTab_activate(signal, op_ptr, &c);
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
  c_tableRecordPool.getPtr(tabPtr, impl_req->tableId);
  tabPtr.p->tabState = TableRecord::DEFINED;

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

  Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
  sendTransConf(signal, op_ptr, itFlags);
}

void
Dbdict::createTab_startLcpMutex_unlocked(Signal* signal,
					 Uint32 op_key,
					 Uint32 ret)
{
  jamEntry();
  D("createTab_startLcpMutex_unlocked");

  ndbrequire(ret == 0);

  SchemaOpPtr op_ptr;
  CreateTableRecPtr createTabPtr;
  findSchemaOp(op_ptr, createTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  createTabPtr.p->m_startLcpMutex.release(c_mutexMgr);

  sendTransConf(signal, op_ptr);
}

// CreateTable: ABORT

void
Dbdict::createTable_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;
  const Uint32 tableId = impl_req->tableId;

  D("createTable_abortParse" << V(tableId) << *op_ptr.p);

  do {
    if (op_ptr.p->m_abortPrepareDone) {
      jam();
      D("done by abort prepare");
      break;
    }

    const TransPhase::Value phase = op_ptr.p->m_step.m_phase;
    ndbrequire(phase <= TransPhase::Parse);

    if (tableId == RNIL) {
      jam();
      D("no table allocated");
      ndbrequire(phase < TransPhase::Parse);
      break;
    }

    TableRecordPtr tabPtr;
    c_tableRecordPool.getPtr(tabPtr, tableId);

    ndbrequire(tabPtr.p->tabState == TableRecord::DEFINING);
    tabPtr.p->tabState = TableRecord::NOT_DEFINED;

    // any link was to a new object
    if (hasDictObject(op_ptr)) {
      jam();
      unlinkDictObject(op_ptr);
      releaseTableObject(tableId, true);
    }

    if (phase == TransPhase::Parse) {
      jam();
      // parse was completed so schema entry must be restored
      SchemaFile::TableEntry& te = createTabPtr.p->m_orig_entry;
      bool savetodisk = false;
      updateSchemaState(signal, tableId, &te, (Callback*)0, savetodisk, 1);
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
  const Uint32 tableId = impl_req->tableId;

  D("createTable_abortPrepare" << *op_ptr.p);

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, impl_req->tableId);
  tabPtr.p->tabState = TableRecord::DROPPING;

  // create drop table operation  wl3600_todo must pre-allocate

  SchemaOpPtr& oplnk_ptr = op_ptr.p->m_oplnk_ptr;
  ndbrequire(oplnk_ptr.isNull());
  DropTableRecPtr dropTabPtr;
  seizeSchemaOp(oplnk_ptr, dropTabPtr);
  ndbrequire(!oplnk_ptr.isNull());
  DropTabReq* aux_impl_req = &dropTabPtr.p->m_request;

  aux_impl_req->senderRef = impl_req->senderRef;
  aux_impl_req->senderData = impl_req->senderData;
  aux_impl_req->requestType = DropTabReq::CreateTabDrop;
  aux_impl_req->tableId = impl_req->tableId;
  aux_impl_req->tableVersion = impl_req->tableVersion;

  // link other way too
  oplnk_ptr.p->m_opbck_ptr = op_ptr;

  // wl3600_todo use ref count
  unlinkDictObject(op_ptr);

  dropTabPtr.p->m_block = 0;
  dropTabPtr.p->m_callback.m_callbackData =
    oplnk_ptr.p->op_key;
  dropTabPtr.p->m_callback.m_callbackFunction =
    safe_cast(&Dbdict::createTable_abortLocalConf);

  // invoke the "commit" phase of drop table
  dropTab_nextStep(signal, oplnk_ptr);

  if (tabPtr.p->m_tablespace_id != RNIL) {
    FilegroupPtr ptr;
    ndbrequire(c_filegroup_hash.find(ptr, tabPtr.p->m_tablespace_id));
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

  // write schema file  wl3600_todo why not use updateSchemaState

  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);
  tableEntry->m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
  tableEntry->m_transId = 0;
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);
  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);

  Callback callback = {
    safe_cast(&Dbdict::createTable_abortWriteSchemaConf),
    oplnk_ptr.p->op_key
  };

  // wl3600_todo too much repeated code
  if (savetodisk) {
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;

    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.firstPage = tableId / NDB_SF_PAGE_ENTRIES;
    c_writeSchemaRecord.noOfPages = 1;
    c_writeSchemaRecord.m_callback = callback;
    startWriteSchemaFile(signal);
  } else {
    execute(signal, callback, 0);
  }
}

void
Dbdict::createTable_abortWriteSchemaConf(Signal* signal,
                                         Uint32 oplnk_key,
                                         Uint32 ret)
{
  jam();
  D("createTable_abortWriteSchemaConf" << V(oplnk_key));

  SchemaOpPtr oplnk_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(oplnk_ptr, dropTabPtr, oplnk_key);
  ndbrequire(!oplnk_ptr.isNull());

  SchemaOpPtr op_ptr = oplnk_ptr.p->m_opbck_ptr;
  CreateTableRecPtr createTabPtr;
  getOpRec(op_ptr, createTabPtr);
  const CreateTabReq* impl_req = &createTabPtr.p->m_request;
  Uint32 tableId = impl_req->tableId;

  releaseTableObject(tableId);

  sendTransConf(signal, op_ptr);
}

// CreateTable: MISC

void
Dbdict::execBACKUP_FRAGMENT_REQ(Signal* signal)
{
  jamEntry();
  Uint32 tableId = signal->theData[0];
  Uint32 lock = signal->theData[1];

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId, true);

  if(lock)
  {
    ndbrequire(tablePtr.p->tabState == TableRecord::DEFINED);
    tablePtr.p->tabState = TableRecord::BACKUP_ONGOING;
  }
  else if(tablePtr.p->tabState == TableRecord::BACKUP_ONGOING)
  {
    tablePtr.p->tabState = TableRecord::DEFINED;
  }
}

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
Dbdict::execCREATE_TAB_REF(Signal* signal)
{
  // no longer received
  ndbrequire(false);
}

void
Dbdict::execCREATE_TAB_CONF(Signal* signal)
{
  // no longer received
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

void Dbdict::releaseTableObject(Uint32 tableId, bool removeFromHash) 
{
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);
  if (removeFromHash)
  {
    jam();
    release_object(tablePtr.p->m_obj_ptr_i);
  }
  else
  {
    Rope tmp(c_rope_pool, tablePtr.p->tableName);
    tmp.erase();
  }
  
  {
    Rope tmp(c_rope_pool, tablePtr.p->frmData);
    tmp.erase();
  }

  {
    Rope tmp(c_rope_pool, tablePtr.p->tsData);
    tmp.erase();
  }

  {
    Rope tmp(c_rope_pool, tablePtr.p->ngData);
    tmp.erase();
  }

  {
    Rope tmp(c_rope_pool, tablePtr.p->rangeData);
    tmp.erase();
  }

  tablePtr.p->tabState = TableRecord::NOT_DEFINED;
  
  LocalDLFifoList<AttributeRecord> list(c_attributeRecordPool, 
					tablePtr.p->m_attributes);
  AttributeRecordPtr attrPtr;
  for(list.first(attrPtr); !attrPtr.isNull(); list.next(attrPtr)){
    Rope name(c_rope_pool, attrPtr.p->attributeName);
    Rope def(c_rope_pool, attrPtr.p->defaultValue);
    name.erase();
    def.erase();
  }
  list.release();
}//releaseTableObject()

// CreateTable: END

// MODULE: DropTable

const Dbdict::OpInfo
Dbdict::DropTableRec::g_opInfo = {
  { 'D', 'T', 'a', 0 },
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
  ndbrequire(signal->getNoOfSections() == 0);

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

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

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
Dbdict::dropTable_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("dropTable_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  DropTabReq* impl_req = &dropTabPtr.p->m_request;
  Uint32 tableId = impl_req->tableId;

  if (checkSingleUserMode(trans_ptr.p->m_clientRef)) {
    jam();
    setError(error, DropTableRef::SingleUser, __LINE__);
    return;
  }

  TableRecordPtr tablePtr;
  if (!(tableId < c_tableRecordPool.getSize())) {
    jam();
    setError(error, DropTableRef::NoSuchTable, __LINE__);
    return;
  }
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);

  // check version first (api will retry)
  if (tablePtr.p->tableVersion != impl_req->tableVersion) {
    jam();
    setError(error, DropTableRef::InvalidTableVersion, __LINE__);
    return;
  }

  // check and save original schema file entry
  {
    XSchemaFile* xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    SchemaFile::TableEntry* te = getTableEntry(xsf, tableId);

    if (te->m_transId != 0 && te->m_transId != trans_ptr.p->m_transId) {
      jam();
      setError(error, DropTableRef::ActiveSchemaTrans, __LINE__);
      return;
    }
    if (te->m_tableState == SchemaFile::DROP_PARSED) {
      jam();
      setError(error, DropTableRef::DropInProgress, __LINE__);
      return;
    }
    dropTabPtr.p->m_orig_entry = *te;
  }

  const TableRecord::TabState tabState = tablePtr.p->tabState;
  bool ok = false;
  switch (tabState) {
  case TableRecord::NOT_DEFINED:
  case TableRecord::REORG_TABLE_PREPARED:
  case TableRecord::DEFINING:
  case TableRecord::CHECKED:
    jam();
    setError(error, DropTableRef::NoSuchTable, __LINE__);
    return;
  case TableRecord::DEFINED:
    jam();
    ok = true;
    break;
  case TableRecord::PREPARE_DROPPING:
  case TableRecord::DROPPING:
    jam();
    setError(error, DropTableRef::DropInProgress, __LINE__);
    return;
  case TableRecord::BACKUP_ONGOING:
    jam();
    setError(error, DropTableRef::BackupInProgress, __LINE__);
    return;
  }
  ndbrequire(ok);

  // link operation to object
  {
    DictObjectPtr obj_ptr;
    Uint32 obj_ptr_i = tablePtr.p->m_obj_ptr_i;
    bool ok = findDictObject(op_ptr, obj_ptr, obj_ptr_i);
    ndbrequire(ok);
  }

  // update schema state in memory
  {
    SchemaFile::TableEntry& te = dropTabPtr.p->m_curr_entry;
    te = dropTabPtr.p->m_orig_entry;
    te.m_tableState = SchemaFile::DROP_PARSED;
    te.m_transId = trans_ptr.p->m_transId;

    bool savetodisk = false;
    updateSchemaState(signal, tableId, &te, (Callback*)0, savetodisk, 1);
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
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  D("dropTable_prepare" << V(itRepeat) << *op_ptr.p);

  if (itRepeat == 0) {
    jam();
    dropTabPtr.p->m_block = 0;

    // master checks table state and all sync via next repeat

    if (trans_ptr.p->m_isMaster) {
      jam();
      Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
      Callback c = {
        safe_cast(&Dbdict::dropTable_backup_mutex_locked),
        op_ptr.p->op_key
      };
      bool ok = mutex.lock(c);
      ndbrequire(ok);
      return;
    }

    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
    return;
  }

  prepDropTab_nextStep(signal, op_ptr);
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
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);

  Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
  mutex.unlock(); // ignore response

  if (tablePtr.p->tabState == TableRecord::BACKUP_ONGOING) {
    jam();
    setError(op_ptr, DropTableRef::BackupInProgress, __LINE__);
    sendTransRef(signal, op_ptr);
  } else {
    jam();
    tablePtr.p->tabState = TableRecord::PREPARE_DROPPING;
    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
  }
}

void
Dbdict::prepDropTab_nextStep(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  /**
   * No errors currently allowed
   */
  ndbrequire(!hasError(op_ptr.p->m_error));

  Uint32& block = dropTabPtr.p->m_block; // ref
  D("prepDropTab_nextStep" << hex << V(block) << *op_ptr.p);

  switch (block) {
  case 0:
    jam();
    block = DBDICT;
    prepDropTab_writeSchema(signal, op_ptr);
    return;
  case DBDICT:
    jam();
    block = DBLQH;
    break;
  case DBLQH:
    jam();
    block = DBTC;
    break;
  case DBTC:
    jam();
    block = DBDIH;
    break;
  case DBDIH:
    jam();
    prepDropTab_complete(signal, op_ptr);
    return;
  default:
    ndbrequire(false);
    break;
  }

  PrepDropTabReq* prep = (PrepDropTabReq*)signal->getDataPtrSend();
  prep->senderRef = reference();
  prep->senderData = op_ptr.p->op_key;
  prep->tableId = impl_req->tableId;
  prep->requestType = impl_req->requestType;

  BlockReference ref = numberToRef(block, getOwnNodeId());
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
Dbdict::prepDropTab_writeSchema(Signal* signal, SchemaOpPtr op_ptr)
{
  jamEntry();

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);
  tablePtr.p->tabState = TableRecord::PREPARE_DROPPING;

  /**
   * Modify schema  wl3600_todo use updateSchemaState
   */
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tablePtr.i);
  SchemaFile::TableState tabState =
    (SchemaFile::TableState)tableEntry->m_tableState;
  ndbrequire(tabState == SchemaFile::DROP_PARSED);
  //ndbrequire(tabState == SchemaFile::TABLE_ADD_COMMITTED ||
             //tabState == SchemaFile::ALTER_TABLE_COMMITTED ||
             //tabState == SchemaFile::TEMPORARY_TABLE_COMMITTED);
  tableEntry->m_tableState = SchemaFile::DROP_TABLE_STARTED;
  computeChecksum(xsf, tablePtr.i / NDB_SF_PAGE_ENTRIES);

  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);
  Callback callback;
  callback.m_callbackData = op_ptr.p->op_key;
  callback.m_callbackFunction = safe_cast(&Dbdict::prepDropTab_writeSchemaConf);
  if (savetodisk)
  {
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;

    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.newFile = false;
    c_writeSchemaRecord.firstPage = tablePtr.i / NDB_SF_PAGE_ENTRIES;
    c_writeSchemaRecord.noOfPages = 1;
    c_writeSchemaRecord.m_callback = callback;
    startWriteSchemaFile(signal);
  }
  else
  {
    execute(signal, callback, 0);
  }
}

void
Dbdict::prepDropTab_writeSchemaConf(Signal* signal,
                                    Uint32 op_key,
                                    Uint32 ret)
{
  jam();
  D("prepDropTab_writeSchemaConf");
  prepDropTab_fromLocal(signal, op_key, ret);
}

void
Dbdict::execPREP_DROP_TAB_CONF(Signal * signal)
{
  jamEntry();
  const PrepDropTabConf* conf = (const PrepDropTabConf*)signal->getDataPtr();

  Uint32 nodeId = refToNode(conf->senderRef);
  Uint32 block = refToBlock(conf->senderRef);
  ndbrequire(nodeId == getOwnNodeId() && block != DBDICT);

  prepDropTab_fromLocal(signal, conf->senderData, 0);
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

  if (errorCode == PrepDropTabRef::NoSuchTable && block == DBLQH) {
    jam();
    /**
     * Ignore errors:
     * 1) no such table and LQH, it might not exists in different LQH's
     */
    errorCode = 0;
  }
  prepDropTab_fromLocal(signal, ref->senderData, errorCode);
}

void
Dbdict::prepDropTab_fromLocal(Signal* signal, Uint32 op_key, Uint32 errorCode)
{
  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (errorCode != 0) {
    jam();
    setError(op_ptr, errorCode, __LINE__);
  }

  // one local block done, coordinator will order next
  Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
  sendTransConf(signal, op_ptr, itFlags);
}

void
Dbdict::prepDropTab_complete(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("prepDropTab_complete");

  sendTransConf(signal, op_ptr);
}

// DropTable: COMMIT

void
Dbdict::dropTable_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);

  D("dropTable_commit" << V(itRepeat) << *op_ptr.p);

  if (itRepeat == 0) {
    jam();

    // on first round do like old execDROP_TAB_REQ

    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, dropTabPtr.p->m_request.tableId);
    tablePtr.p->tabState = TableRecord::DROPPING;

    dropTabPtr.p->m_block = 0;
    dropTabPtr.p->m_callback.m_callbackData =
      op_ptr.p->op_key;
    dropTabPtr.p->m_callback.m_callbackFunction =
      safe_cast(&Dbdict::dropTab_complete);

    if (tablePtr.p->m_tablespace_id != RNIL)
    {
      FilegroupPtr ptr;
      ndbrequire(c_filegroup_hash.find(ptr, tablePtr.p->m_tablespace_id));
      decrease_ref_count(ptr.p->m_obj_ptr_i);
    }

#if defined VM_TRACE || defined ERROR_INSERT
    // from a newer execDROP_TAB_REQ version
    {
      char buf[1024];
      Rope name(c_rope_pool, tablePtr.p->tableName);
      name.copy(buf);
      ndbout_c("Dbdict: drop name=%s,id=%u,obj_id=%u", buf, tablePtr.i, 
               tablePtr.p->m_obj_ptr_i);
    }
#endif
  }

  dropTab_nextStep(signal, op_ptr);
}

void
Dbdict::dropTab_nextStep(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  /**
   * No errors currently allowed
   */
  ndbrequire(!hasError(op_ptr.p->m_error));

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);

  Uint32& block = dropTabPtr.p->m_block; // ref
  D("dropTab_nextStep" << hex << V(block) << *op_ptr.p);

  switch (block) {
  case 0:
    jam();
    block = DBTC;
    break;
  case DBTC:
    jam();
    if (tablePtr.p->isTable() || tablePtr.p->isHashIndex())
      block = DBACC;
    if (tablePtr.p->isOrderedIndex())
      block = DBTUP;
    break;
  case DBACC:
    jam();
    block = DBTUP;
    break;
  case DBTUP:
    jam();
    if (tablePtr.p->isTable() || tablePtr.p->isHashIndex())
      block = DBLQH;
    if (tablePtr.p->isOrderedIndex())
      block = DBTUX;
    break;
  case DBTUX:
    jam();
    block = DBLQH;
    break;
  case DBLQH:
    jam();
    block = DBDIH;
    break;
  case DBDIH:
    jam();
    execute(signal, dropTabPtr.p->m_callback, 0);
    return;
  default:
    ndbrequire(false);
    break;
  }

  DropTabReq* req = (DropTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->tableId = impl_req->tableId;
  req->requestType = impl_req->requestType;

  BlockReference ref = numberToRef(block, getOwnNodeId());
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

  dropTab_fromLocal(signal, conf->senderData);
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

  dropTab_fromLocal(signal, ref->senderData);
}

void
Dbdict::dropTab_fromLocal(Signal* signal, Uint32 op_key)
{
  jamEntry();

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;

  D("dropTab_fromLocal" << *op_ptr.p);

  Uint32 requestType = impl_req->requestType;
  if (requestType == DropTabReq::CreateTabDrop ||
      requestType == DropTabReq::RestartDropTab) {
    jam();
    dropTab_nextStep(signal, op_ptr);
    return;
  }

  // one local block done, coordinator will order next
  Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
  sendTransConf(signal, op_ptr, itFlags);
}

void
Dbdict::dropTab_complete(Signal* signal,
                         Uint32 op_key,
                         Uint32 ret)
{
  jam();

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  Uint32 tableId = dropTabPtr.p->m_request.tableId;

  /**
   * Write to schema file
   */
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);
  SchemaFile::TableState tabState =
    (SchemaFile::TableState)tableEntry->m_tableState;
  ndbrequire(tabState == SchemaFile::DROP_TABLE_STARTED);
  tableEntry->m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
  tableEntry->m_transId = 0;
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);
  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);
  Callback callback;
  callback.m_callbackData = op_key;
  callback.m_callbackFunction = safe_cast(&Dbdict::dropTab_writeSchemaConf);
  if (savetodisk)
  {
    ndbrequire(c_writeSchemaRecord.inUse == false);
    c_writeSchemaRecord.inUse = true;

    c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
    c_writeSchemaRecord.firstPage = tableId / NDB_SF_PAGE_ENTRIES;
    c_writeSchemaRecord.noOfPages = 1;
    c_writeSchemaRecord.m_callback = callback;
    startWriteSchemaFile(signal);
  }
  else
  {
    execute(signal, callback, 0);
  }
}

void
Dbdict::dropTab_writeSchemaConf(Signal* signal,
                                Uint32 op_key,
                                Uint32 ret)
{
  jam();
  D("dropTab_writeSchemaConf");

  SchemaOpPtr op_ptr;
  DropTableRecPtr dropTabPtr;
  findSchemaOp(op_ptr, dropTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const DropTabReq* impl_req = &dropTabPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  unlinkDictObject(op_ptr);
  releaseTableObject(dropTabPtr.p->m_request.tableId);

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
    conf->tableId = impl_req->tableId;

    EXECUTE_DIRECT(SUMA, GSN_DROP_TAB_CONF, signal,
		   DropTabConf::SignalLength);
    jamEntry();
  }

  sendTransConf(signal, trans_ptr);
}

// DropTable: ABORT

void
Dbdict::dropTable_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTable_abortParse" << *op_ptr.p);

  DropTableRecPtr dropTabPtr;
  getOpRec(op_ptr, dropTabPtr);
  DropTabReq* impl_req = &dropTabPtr.p->m_request;
  Uint32 tableId = impl_req->tableId;

  // update schema state in memory  XXX temp
  if (dropTabPtr.p->m_orig_entry.m_tableVersion != 0) {
    XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);
    *tableEntry = dropTabPtr.p->m_orig_entry;
    computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);
  }

  sendTransConf(signal, op_ptr);
}

void
Dbdict::dropTable_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTable_abortPrepare" << *op_ptr.p);

  // no errors currently allowed...
  ndbrequire(false);

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
    Rope r(c_rope_pool, alterTabPtr.p->m_oldTableName);
    r.erase();
  }
  {
    Rope r(c_rope_pool, alterTabPtr.p->m_oldFrmData);
    r.erase();
  }
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

void
Dbdict::execALTER_TABLE_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
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

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

  releaseSections(signal);

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
Dbdict::alterTable_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("alterTable_parse");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  // check that all db nodes run same version
  if (!check_ndb_versions()) {
    jam();
    setError(error, AlterTableRef::IncompatibleVersions, __LINE__);
    return;
  }

  if (checkSingleUserMode(trans_ptr.p->m_clientRef)) {
    jam();
    setError(error, AlterTableRef::SingleUser, __LINE__);
    return;
  }

  // get table definition
  TableRecordPtr tablePtr;
  if (!(impl_req->tableId < c_tableRecordPool.getSize())) {
    jam();
    setError(error, AlterTableRef::NoSuchTable, __LINE__);
    return;
  }
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);

  const TableRecord::TabState tabState = tablePtr.p->tabState;
  bool ok = false;
  switch (tabState) {
  case TableRecord::NOT_DEFINED:
  case TableRecord::REORG_TABLE_PREPARED:
  case TableRecord::DEFINING:
  case TableRecord::CHECKED:
    jam();
    setError(error, AlterTableRef::NoSuchTable, __LINE__);
    return;
  case TableRecord::DEFINED:
    ok = true;
    jam();
    break;
  case TableRecord::BACKUP_ONGOING:
    jam();
    setError(error, AlterTableRef::BackupInProgress, __LINE__);
    return;
  case TableRecord::PREPARE_DROPPING:
  case TableRecord::DROPPING:
    jam();
    setError(error, AlterTableRef::DropInProgress, __LINE__);
    return;
  }
  ndbrequire(ok);

  // save it for abort code
  alterTabPtr.p->m_tablePtr = tablePtr;

  if (tablePtr.p->tableVersion != impl_req->tableVersion) {
    jam();
    setError(error, AlterTableRef::InvalidTableVersion, __LINE__);
    return;
  }

  // parse new table definition into new table record
  TableRecordPtr& newTablePtr = alterTabPtr.p->m_newTablePtr; // ref
  {
    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::AlterTableFromAPI;
    parseRecord.errorCode = 0;

    SegmentedSectionPtr ptr;
    signal->getSection(ptr, AlterTableReq::DICT_TAB_INFO);
    SimplePropertiesSectionReader r(ptr, getSectionSegmentPool());

    handleTabInfoInit(r, &parseRecord, false); // Will not save info

    if (parseRecord.errorCode != 0) {
      jam();
      setError(error, parseRecord);
      return;
    }

    // the new temporary table record seized from pool
    newTablePtr = parseRecord.tablePtr;
  }

  // set the new version now
  impl_req->newTableVersion =
    newTablePtr.p->tableVersion =
    alter_obj_inc_schema_version(tablePtr.p->tableVersion);

  // add attribute stuff
  {
    const Uint32 noOfNewAttr =
      newTablePtr.p->noOfAttributes - tablePtr.p->noOfAttributes;
    const bool addAttrFlag =
      AlterTableReq::getAddAttrFlag(impl_req->changeMask);

    if (!(newTablePtr.p->noOfAttributes >= tablePtr.p->noOfAttributes) ||
        (noOfNewAttr != 0) != addAttrFlag) {
      jam();
      setError(error, AlterTableRef::UnsupportedChange, __LINE__);
      return;
    }

    if (trans_ptr.p->m_isMaster) {
      jam();
      impl_req->noOfNewAttr = noOfNewAttr;
      impl_req->newNoOfCharsets = newTablePtr.p->noOfCharsets;
      impl_req->newNoOfKeyAttrs = newTablePtr.p->noOfPrimkey;
    } else {
      ndbrequire(impl_req->noOfNewAttr == noOfNewAttr);
    }

    LocalDLFifoList<AttributeRecord>
      list(c_attributeRecordPool, newTablePtr.p->m_attributes);
    AttributeRecordPtr attrPtr;
    list.first(attrPtr);
    Uint32 i = 0;
    for (i = 0; i < newTablePtr.p->noOfAttributes; i++) {
      if (i >= tablePtr.p->noOfAttributes) {
        jam();
        Uint32 j = 2 * (i - tablePtr.p->noOfAttributes);
        alterTabPtr.p->m_newAttrData[j + 0] = attrPtr.p->attributeDescriptor;
        alterTabPtr.p->m_newAttrData[j + 1] = attrPtr.p->extPrecision & ~0xFFFF;
      }
      list.next(attrPtr);
    }
  }

  D("alterTable_parse " << V(newTablePtr.i) << hex << V(newTablePtr.p->tableVersion));

  // save original schema file entry
  {
    XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, impl_req->tableId);
    alterTabPtr.p->m_oldTableEntry = *tableEntry;
  }

  // master rewrites DictTabInfo (it is re-parsed only on slaves)
  if (trans_ptr.p->m_isMaster) {
    jam();
    releaseSections(signal);
    SimplePropertiesSectionWriter w(getSectionSegmentPool());
    packTableIntoPages(w, alterTabPtr.p->m_newTablePtr);

    SegmentedSectionPtr tabInfoPtr;
    w.getPtr(tabInfoPtr);
    signal->setSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);
  }

  // save sections
  saveOpSection(op_ptr, signal, AlterTabReq::DICT_TAB_INFO);
}

bool
Dbdict::alterTable_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTable_subOps" << V(op_ptr.i) << *op_ptr.p);
  return false;
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

// AlterTable: PREPARE

void
Dbdict::alterTable_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  D("alterTable_prepare" << V(itRepeat) << *op_ptr.p);

  if (itRepeat == 0) {
    jam();

    // master checks table state and all sync via next repeat

    if (trans_ptr.p->m_isMaster) {
      jam();
      Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_define_backup_mutex);
      Callback c = {
        safe_cast(&Dbdict::alterTable_backup_mutex_locked),
        op_ptr.p->op_key
      };
      bool ok = mutex.lock(c);
      ndbrequire(ok);
      return;
    }

    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
    return;
  }

  if (itRepeat == 1) {
    jam();

    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);
    TableRecordPtr newTablePtr = alterTabPtr.p->m_newTablePtr;

    const Uint32 changeMask = impl_req->changeMask;
    Uint32& changeMaskDone = alterTabPtr.p->m_changeMaskDone; // ref

    if (AlterTableReq::getNameFlag(changeMask)) {
      jam();
      const Uint32 sz = MAX_TAB_NAME_SIZE;
      D("alter name:"
        << " old=" << copyRope<sz>(tablePtr.p->tableName)
        << " new=" << copyRope<sz>(newTablePtr.p->tableName));

      Ptr<DictObject> obj_ptr;
      c_obj_pool.getPtr(obj_ptr, tablePtr.p->m_obj_ptr_i);

      // remove old name from hash
      c_obj_hash.remove(obj_ptr);

      // save old name and replace it by new
      bool ok =
        copyRope<sz>(alterTabPtr.p->m_oldTableName, tablePtr.p->tableName) &&
        copyRope<sz>(tablePtr.p->tableName, newTablePtr.p->tableName);
      ndbrequire(ok);

      // add new name to object hash
      obj_ptr.p->m_name = tablePtr.p->tableName;
      c_obj_hash.add(obj_ptr);

      AlterTableReq::setNameFlag(changeMaskDone, true);
    }

    if (AlterTableReq::getFrmFlag(changeMask)) {
      jam();
      // save old frm and replace it by new
      const Uint32 sz = MAX_FRM_DATA_SIZE;
      bool ok =
        copyRope<sz>(alterTabPtr.p->m_oldFrmData, tablePtr.p->frmData) &&
        copyRope<sz>(tablePtr.p->frmData, newTablePtr.p->frmData);
      ndbrequire(ok);

      AlterTableReq::setFrmFlag(changeMaskDone, true);
    }

    if (AlterTableReq::getAddAttrFlag(changeMask)) {
      jam();

      /* Move the column definitions to the real table definitions. */
      LocalDLFifoList<AttributeRecord>
        list(c_attributeRecordPool, tablePtr.p->m_attributes);
      LocalDLFifoList<AttributeRecord>
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

    /*
      TODO RONM:  Some new code for FragmentData and RangeOrListData
    */

    // repeat to do local blocks
    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
    return;
  }

  ndbrequire(itRepeat >= 2);
  {
    jam();
    ndbrequire(2 + alterTabPtr.p->m_blockIndex == itRepeat);
    alterTable_toLocal(signal, op_ptr);
    return;
  }
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
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);

  Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_define_backup_mutex);
  mutex.unlock(); // ignore response

  if (tablePtr.p->tabState == TableRecord::BACKUP_ONGOING) {
    jam();
    setError(op_ptr, AlterTableRef::BackupInProgress, __LINE__);
    sendTransRef(signal, op_ptr);
  } else {
    jam();
    // wl3600_todo fix state
    tablePtr.p->tabState = TableRecord::PREPARE_DROPPING;
    Uint32 itFlags = SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, trans_ptr, itFlags);
  }
}

void
Dbdict::alterTable_toLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  const Uint32 blockIndex = alterTabPtr.p->m_blockIndex;
  ndbrequire(blockIndex < AlterTableRec::BlockCount);
  const Uint32 blockNo = alterTabPtr.p->m_blockNo[blockIndex];

  const char* const blockName = getBlockName(blockNo);
  D("alterTable_toLocal" << V(blockIndex) << V(blockName));

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTablePrepare;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->connectPtr = blockNo == DBTUP ? alterTabPtr.p->m_tupAlterTabPtr : RNIL;
  req->noOfNewAttr = impl_req->noOfNewAttr;
  req->newNoOfCharsets = impl_req->newNoOfCharsets;
  req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

  Callback c = {
    safe_cast(&Dbdict::alterTable_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  BlockReference blockRef = numberToRef(blockNo, getOwnNodeId());
  const bool sendNewAttrData = req->noOfNewAttr > 0 && blockNo == DBTUP;

  if (!sendNewAttrData) {
    jam();
    sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB);
  } else {
    jam();
    LinearSectionPtr ptr[3];
    ptr[0].p = alterTabPtr.p->m_newAttrData;
    ptr[0].sz = 2 * impl_req->noOfNewAttr;
    sendSignal(blockRef, GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB, ptr, 1);
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

  if (ret == 0) {
    jam();
    blockIndex += 1;

    // save TUP operation record for commit/abort
    const AlterTabConf* conf = (const AlterTabConf*)signal->getDataPtr();
    if (blockNo == DBTUP) {
      jam();
      alterTabPtr.p->m_tupAlterTabPtr = conf->connectPtr;
    }

    Uint32 itFlags = 0;
    if (blockIndex < AlterTableRec::BlockCount)
      itFlags |= SchemaTransImplConf::IT_REPEAT;

    sendTransConf(signal, op_ptr, itFlags);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

// AlterTable: COMMIT

void
Dbdict::alterTable_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  D("alterTable_commit" << V(itRepeat) << *op_ptr.p);

  // wl3600_todo hmm there is no prepare to disk

  ndbrequire(signal->getNoOfSections() == 0);

  // on first round commit in TUP
  if (itRepeat == 0) {
    jam();
    alterTable_toTupCommit(signal, op_ptr);
    return;
  }

  // write schema entry for altered table to disk
  ndbrequire(itRepeat == 1);

  const Uint32 tableId = impl_req->tableId;
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);

  const OpSection& tabInfoSec =
    getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);
  const Uint32 size = tabInfoSec.getSize();
  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);

  // update table record
  tablePtr.p->packedSize = size;
  tablePtr.p->tableVersion = impl_req->newTableVersion;
  tablePtr.p->gciTableCreated = impl_req->gci;

  SchemaFile::TableEntry tabEntry;
  tabEntry.init();
  tabEntry.m_tableVersion = impl_req->newTableVersion;
  tabEntry.m_tableType = tablePtr.p->tableType;
  if (savetodisk)
    tabEntry.m_tableState = SchemaFile::ALTER_TABLE_COMMITTED;
  else
    tabEntry.m_tableState = SchemaFile::TEMPORARY_TABLE_COMMITTED;
  tabEntry.m_gcp = impl_req->gci;
  tabEntry.m_info_words = size;
  tabEntry.m_transId = 0;

  Callback callback = {
    safe_cast(&Dbdict::alterTab_writeSchemaConf),
    op_ptr.p->op_key
  };

  updateSchemaState(signal, tableId, &tabEntry, &callback, savetodisk);
}

void
Dbdict::alterTable_toTupCommit(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTable_toTupCommit");

  AlterTableRecPtr alterTabPtr;
  getOpRec(op_ptr, alterTabPtr);
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTableCommit;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->connectPtr = alterTabPtr.p->m_tupAlterTabPtr;
  req->noOfNewAttr = impl_req->noOfNewAttr;
  req->newNoOfCharsets = impl_req->newNoOfCharsets;
  req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

  Callback c = {
    safe_cast(&Dbdict::alterTable_fromTupCommit),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  sendSignal(DBTUP_REF, GSN_ALTER_TAB_REQ, signal,
             AlterTabReq::SignalLength, JBB);
}

void
Dbdict::alterTable_fromTupCommit(Signal* signal,
                                 Uint32 op_key,
                                 Uint32 ret)
{
  D("alterTable_fromTupCommit");

  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());

  if (ret == 0) {
    jam();
    Uint32 itFlags = 0;
    itFlags |= SchemaTransImplConf::IT_REPEAT;
    sendTransConf(signal, op_ptr, itFlags);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

void
Dbdict::alterTab_writeSchemaConf(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);
  bool savetodisk = !(tablePtr.p->m_bits & TableRecord::TR_Temporary);

  const OpSection& tabInfoSec =
    getOpSection(op_ptr, CreateTabReq::DICT_TAB_INFO);

  Callback callback = {
    safe_cast(&Dbdict::alterTab_writeTableConf),
    callback.m_callbackData = op_ptr.p->op_key
  };

  if (savetodisk) {
    writeTableFile(signal, impl_req->tableId, tabInfoSec, &callback);
  } else {
    execute(signal, callback, 0);
  }
}

void
Dbdict::alterTab_writeTableConf(Signal* signal,
                                Uint32 op_key,
                                Uint32 ret)
{
  jam();
  SchemaOpPtr op_ptr;
  AlterTableRecPtr alterTabPtr;
  findSchemaOp(op_ptr, alterTabPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  const AlterTabReq* impl_req = &alterTabPtr.p->m_request;
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, impl_req->tableId);

  tabPtr.p->tabState = TableRecord::DEFINED;

  // inform Suma so it can send events to any subscribers of the table
  {
    AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();

    // special use of senderRef
    if (trans_ptr.p->m_isMaster)
      req->senderRef = trans_ptr.p->m_clientRef;
    else
      req->senderRef = 0;
    req->senderData = op_key;
    req->tableId = impl_req->tableId;
    req->tableVersion = impl_req->tableVersion;
    req->newTableVersion = impl_req->newTableVersion;
    req->gci = tabPtr.p->gciTableCreated;
    req->requestType = 0;
    req->changeMask = impl_req->changeMask;
    req->connectPtr = RNIL;
    req->noOfNewAttr = impl_req->noOfNewAttr;
    req->newNoOfCharsets = impl_req->newNoOfCharsets;
    req->newNoOfKeyAttrs = impl_req->newNoOfKeyAttrs;

    const OpSection& tabInfoSec =
      getOpSection(op_ptr, AlterTabReq::DICT_TAB_INFO);
    SegmentedSectionPtr tabInfoPtr;
    bool ok = copyOut(tabInfoSec, tabInfoPtr);
    ndbrequire(ok);

    signal->setSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);
    EXECUTE_DIRECT(SUMA, GSN_ALTER_TAB_REQ, signal,
                   AlterTabReq::SignalLength);
    releaseSections(signal);
  }

  // older way to notify  wl3600_todo disable to find SUMA problems
  {
    ApiBroadcastRep* api= (ApiBroadcastRep*)signal->getDataPtrSend();
    api->gsn = GSN_ALTER_TABLE_REP;
    api->minVersion = MAKE_VERSION(4,1,15);

    AlterTableRep* rep = (AlterTableRep*)api->theData;
    rep->tableId = tabPtr.p->tableId;
    // wl3600_todo wants old version?
    rep->tableVersion = impl_req->tableVersion;
    rep->changeType = AlterTableRep::CT_ALTERED;

    char oldTableName[MAX_TAB_NAME_SIZE];
    memset(oldTableName, 0, sizeof(oldTableName));
    {
      const RopeHandle& rh =
        AlterTableReq::getNameFlag(impl_req->changeMask)
          ? alterTabPtr.p->m_oldTableName
          : tabPtr.p->tableName;
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

  releaseTableObject(alterTabPtr.p->m_newTablePtr.i, false);
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

  TableRecordPtr& newTablePtr = alterTabPtr.p->m_newTablePtr; // ref
  if (!newTablePtr.isNull()) {
    jam();
    // release the temporary work table
    releaseTableObject(newTablePtr.i, false);
    newTablePtr.setNull();
  }

  TableRecordPtr& tablePtr = alterTabPtr.p->m_tablePtr; // ref
  if (!tablePtr.isNull()) {
    jam();
    // wl3600_todo tabState should be rationalized
    if (tablePtr.p->tabState != TableRecord::BACKUP_ONGOING) {
      jam();
      tablePtr.p->tabState = TableRecord::DEFINED;
    }
    tablePtr.setNull();
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

  if (alterTabPtr.p->m_blockIndex > 0) {
    jam();
    /*
     * Local blocks have only updated table version.
     * Reset it to original in reverse block order.
     */
    alterTable_abortToLocal(signal, op_ptr);
    return;
  }

  // reset DICT memory changes (there was no prepare to disk)
  {
    jam();

    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, impl_req->tableId);
    TableRecordPtr newTablePtr = alterTabPtr.p->m_newTablePtr;

    const Uint32 changeMask = alterTabPtr.p->m_changeMaskDone;

    if (AlterTableReq::getNameFlag(changeMask)) {
      jam();
      const Uint32 sz = MAX_TAB_NAME_SIZE;
      D("reset name:"
        << " new=" << copyRope<sz>(tablePtr.p->tableName)
        << " old=" << copyRope<sz>(alterTabPtr.p->m_oldTableName));

      Ptr<DictObject> obj_ptr;
      c_obj_pool.getPtr(obj_ptr, tablePtr.p->m_obj_ptr_i);

      // remove new name from hash
      c_obj_hash.remove(obj_ptr);

      // copy old name back
      bool ok =
        copyRope<sz>(tablePtr.p->tableName, alterTabPtr.p->m_oldTableName);
      ndbrequire(ok);

      // add old name to object hash
      obj_ptr.p->m_name = tablePtr.p->tableName;
      c_obj_hash.add(obj_ptr);
    }

    if (AlterTableReq::getFrmFlag(changeMask)) {
      jam();
      const Uint32 sz = MAX_FRM_DATA_SIZE;

      // copy old frm back
      bool ok =
        copyRope<sz>(tablePtr.p->frmData, alterTabPtr.p->m_oldFrmData);
      ndbrequire(ok);
    }

    if (AlterTableReq::getAddAttrFlag(changeMask)) {
      jam();

      /* Release the extra columns, not to be used anyway. */
      LocalDLFifoList<AttributeRecord>
        list(c_attributeRecordPool, tablePtr.p->m_attributes);

      const Uint32 noOfNewAttr = impl_req->noOfNewAttr;
      ndbrequire(noOfNewAttr > 0);
      Uint32 i;

      for (i= 0; i < noOfNewAttr; i++) {
        AttributeRecordPtr pPtr;
        ndbrequire(list.last(pPtr));
        list.release(pPtr);
      }
    }

    /*
      TODO RONM: Lite ny kod för FragmentData och RangeOrListData
    */

    // there is no mutex to unlock so we are done
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

  const char* const blockName = getBlockName(blockNo);
  D("alterTable_abortToLocal" << V(blockIndex) << V(blockName));

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = AlterTabReq::AlterTableRevert;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->newTableVersion = impl_req->newTableVersion;
  req->gci = impl_req->gci;
  req->changeMask = impl_req->changeMask;
  req->connectPtr = blockNo == DBTUP ? alterTabPtr.p->m_tupAlterTabPtr : RNIL;
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

  if (ret == 0) {
    jam();
    alterTabPtr.p->m_blockIndex = blockIndex;

    // continue to next block or DICT
    Uint32 itFlags = 0;
    itFlags |= SchemaTransImplConf::IT_REPEAT;

    sendTransConf(signal, op_ptr, itFlags);
  } else {
    jam();
    // abort is not allowed to fail
    ndbrequire(false);
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

  if(len>MAX_TAB_NAME_SIZE)
  {
    jam();
    sendGET_TABLEID_REF((Signal*)signal, 
			(GetTableIdReq *)req, 
			GetTableIdRef::TableNameTooLong);
    return;
  }

  char tableName[MAX_TAB_NAME_SIZE];
  SegmentedSectionPtr ssPtr;
  signal->getSection(ssPtr,GetTableIdReq::TABLE_NAME);
  copy((Uint32*)tableName, ssPtr);
  releaseSections(signal);
    
  DictObject * obj_ptr_p = get_object(tableName, len);
  if(obj_ptr_p == 0 || !DictTabInfo::isTable(obj_ptr_p->m_type)){
    jam();
    sendGET_TABLEID_REF(signal, 
			(GetTableIdReq *)req, 
			GetTableIdRef::TableNotDefined);
    return;
  }

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, obj_ptr_p->m_id); 
  
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

  /**
   * If I get a GET_TABINFO_REQ from myself
   * it's is a one from the time queue
   */
  bool fromTimeQueue = (signal->senderBlockRef() == reference());
  
  if (c_retrieveRecord.busyState && fromTimeQueue == true) {
    jam();
    
    sendSignalWithDelay(reference(), GSN_GET_TABINFOREQ, signal, 30, 
			signal->length());
    return;
  }//if

  const Uint32 MAX_WAITERS = 5;
  
  if(c_retrieveRecord.busyState && fromTimeQueue == false){
    jam();
    if(c_retrieveRecord.noOfWaiters < MAX_WAITERS){
      jam();
      c_retrieveRecord.noOfWaiters++;
      
      sendSignalWithDelay(reference(), GSN_GET_TABINFOREQ, signal, 30, 
			  signal->length());
      return;
    }
    
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
    ndbrequire(signal->getNoOfSections() == 1);  
    const Uint32 len = req->tableNameLen;
    
    if(len > MAX_TAB_NAME_SIZE){
      jam();
      releaseSections(signal);
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNameTooLong, __LINE__);
      return;
    }

    char tableName[MAX_TAB_NAME_SIZE];
    SegmentedSectionPtr ssPtr;
    signal->getSection(ssPtr,GetTabInfoReq::TABLE_NAME);
    SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
    r0.reset(); // undo implicit first()
    if(!r0.getWords((Uint32*)tableName, (len+3)/4)){
      jam();
      releaseSections(signal);
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
      return;
    }
    releaseSections(signal);
    
    DictObject * old_ptr_p = old_ptr_p = get_object(tableName, len);
    if(old_ptr_p)
      obj_id = old_ptr_p->m_id;
  } else {
    jam();
    obj_id = req->tableId;
  }

  SchemaFile::TableEntry *objEntry = 0;
  if(obj_id != RNIL){
    XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    objEntry = getTableEntry(xsf, obj_id);      
  }

  // The table seached for was not found
  if(objEntry == 0){
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
    return;
  }//if

  // If istable/index, allow ADD_STARTED (not to ref)

  D("execGET_TABINFOREQ" << V(transId) << " " << *objEntry);

  if (transId != 0 && transId == objEntry->m_transId) {
    jam();
    // see own trans always
  } else {
    if (objEntry->m_transId != 0) {
      jam();
      // cannot see another uncommitted trans
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
      return;
    }
    if (objEntry->m_tableState != SchemaFile::TABLE_ADD_COMMITTED &&
        objEntry->m_tableState != SchemaFile::ALTER_TABLE_COMMITTED &&
        objEntry->m_tableState != SchemaFile::TEMPORARY_TABLE_COMMITTED){
      jam();
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
      return;
    }//if

    if (DictTabInfo::isTable(objEntry->m_tableType) || 
        DictTabInfo::isIndex(objEntry->m_tableType))
    {
      jam();
      TableRecordPtr tabPtr;
      c_tableRecordPool.getPtr(tabPtr, obj_id);
      if (tabPtr.p->tabState != TableRecord::DEFINED &&
          tabPtr.p->tabState != TableRecord::BACKUP_ONGOING)
      {
        jam();
        sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined, __LINE__);
        return;
      }
      ndbrequire(objEntry->m_tableState == SchemaFile::TEMPORARY_TABLE_COMMITTED ||
                 !(tabPtr.p->m_bits & TableRecord::TR_Temporary));
    }
  }
  
  c_retrieveRecord.busyState = true;
  c_retrieveRecord.blockRef = req->senderRef;
  c_retrieveRecord.m_senderData = req->senderData;
  c_retrieveRecord.tableId = obj_id;
  c_retrieveRecord.currentSent = 0;
  c_retrieveRecord.m_useLongSig = useLongSig;
  c_retrieveRecord.m_table_type = objEntry->m_tableType;
  c_packTable.m_state = PackTable::PTS_GET_TAB;

  if(objEntry->m_tableType==DictTabInfo::Datafile)
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
  }
  else if(objEntry->m_tableType==DictTabInfo::LogfileGroup)
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
  }
  else
  {
    jam();
    signal->theData[0] = ZPACK_TABLE_INTO_PAGES;
    signal->theData[1] = obj_id;
    signal->theData[2] = objEntry->m_tableType;
    signal->theData[3] = c_retrieveRecord.retrievePage;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  }
  jam();
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
  Uint32 senderData = req->senderData;
  // save req flags
  const Uint32 reqTableId = req->getTableId();
  const Uint32 reqTableType = req->getTableType();
  const bool reqListNames = req->getListNames();
  const bool reqListIndexes = req->getListIndexes();
  // init the confs
  ListTablesConf * conf = (ListTablesConf *)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->counter = 0;
  Uint32 pos = 0;

  DLHashTable<DictObject>::Iterator iter;
  bool ok = c_obj_hash.first(iter);
  for(; ok; ok = c_obj_hash.next(iter)){
    Uint32 type = iter.curr.p->m_type;
    if ((reqTableType != (Uint32)0) && (reqTableType != type))
      continue;

    if (reqListIndexes && !DictTabInfo::isIndex(type))
      continue;

    TableRecordPtr tablePtr;
    if (DictTabInfo::isTable(type) || DictTabInfo::isIndex(type)){
      c_tableRecordPool.getPtr(tablePtr, iter.curr.p->m_id);

      if(reqListIndexes && (reqTableId != tablePtr.p->primaryTableId))
	continue;
      
      conf->tableData[pos] = 0;
      conf->setTableId(pos, tablePtr.i); // id
      conf->setTableType(pos, type); // type
      // state

      if(DictTabInfo::isTable(type)){
	switch (tablePtr.p->tabState) {
	case TableRecord::DEFINING:
	case TableRecord::CHECKED:
	  conf->setTableState(pos, DictTabInfo::StateBuilding);
	  break;
	case TableRecord::PREPARE_DROPPING:
	case TableRecord::DROPPING:
	  conf->setTableState(pos, DictTabInfo::StateDropping);
	  break;
	case TableRecord::DEFINED:
	  conf->setTableState(pos, DictTabInfo::StateOnline);
	  break;
	case TableRecord::BACKUP_ONGOING:
	  conf->setTableState(pos, DictTabInfo::StateBackup);
	  break;
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
      c_triggerRecordPool.getPtr(triggerPtr, iter.curr.p->m_id);

      conf->tableData[pos] = 0;
      conf->setTableId(pos, triggerPtr.i);
      conf->setTableType(pos, type);
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
    
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    
    if (! reqListNames)
      continue;
    
    Rope name(c_rope_pool, iter.curr.p->m_name);
    const Uint32 size = name.size();
    conf->tableData[pos] = size;
    pos++;
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    Uint32 i = 0;
    char tmp[MAX_TAB_NAME_SIZE];
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
      if (pos >= ListTablesConf::DataLength) {
	sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		   ListTablesConf::SignalLength, JBB);
	conf->counter++;
	pos = 0;
      }
    }
  }
  // last signal must have less than max length
  sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
	     ListTablesConf::HeaderLength + pos, JBB);
}

/**
 * MODULE: Create index
 *
 * Create index in DICT via create table operation.  Then invoke alter
 * index opearation to online the index.
 *
 * Request type in CREATE_INDX signals:
 *
 * RT_USER - from API to DICT master
 * RT_DICT_PREPARE - prepare participants
 * RT_DICT_COMMIT - commit participants
 * RT_TC - create index in TC (part of alter index operation)
 */

void
Dbdict::execCREATE_INDX_REQ(Signal* signal)
{
  jamEntry();
  CreateIndxReq* const req = (CreateIndxReq*)signal->getDataPtrSend();
  OpCreateIndexPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const CreateIndxReq::RequestType requestType = req->getRequestType();
  if (requestType == CreateIndxReq::RT_USER) {
    jam();
    if (! assembleFragments(signal)) {
      jam();
      return;
    }
    if (signal->getLength() == CreateIndxReq::SignalLength) {
      jam();
      CreateIndxRef::ErrorCode tmperr = CreateIndxRef::NoError;
      if (getOwnNodeId() != c_masterNodeId) {
        jam();
        tmperr = CreateIndxRef::NotMaster;
      } 
      else if (checkSingleUserMode(senderRef))
      {
        jam();
        tmperr = CreateIndxRef::SingleUser;
      }
      else
      {
        DictLockReq lockReq;
        lockReq.userPtr = RNIL;
        lockReq.userRef = reference();
        lockReq.lockType = DictLockReq::CreateIndexLock;
        tmperr = (CreateIndxRef::ErrorCode)dict_lock_trylock(&lockReq);

        if (tmperr == 0)
        {
          jam();
          dict_lock_unlock(0, &lockReq);
        }
      }

      if (tmperr != CreateIndxRef::NoError) {
	releaseSections(signal);
	OpCreateIndex opBusy;
	opPtr.p = &opBusy;
	opPtr.p->save(req);
	opPtr.p->m_isMaster = (senderRef == reference());
	opPtr.p->key = 0;
	opPtr.p->m_requestType = CreateIndxReq::RT_DICT_PREPARE;
	opPtr.p->m_errorCode = tmperr;
	opPtr.p->m_errorLine = __LINE__;
	opPtr.p->m_errorNode = c_masterNodeId;
	createIndex_sendReply(signal, opPtr, true);
	return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, c_aliveNodes);
      sendSignal(rg, GSN_CREATE_INDX_REQ,
          signal, CreateIndxReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == CreateIndxReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpCreateIndex opBusy;
    if (! c_opCreateIndex.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = CreateIndxReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      createIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opCreateIndex.add(opPtr);
    // save attribute list
    SegmentedSectionPtr ssPtr;
    signal->getSection(ssPtr, CreateIndxReq::ATTRIBUTE_LIST_SECTION);
    SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
    r0.reset(); // undo implicit first()
    if (! r0.getWord(&opPtr.p->m_attrList.sz) ||
        ! r0.getWords(opPtr.p->m_attrList.id, opPtr.p->m_attrList.sz)) {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::InvalidName;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      createIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    // save name and index table properties
    signal->getSection(ssPtr, CreateIndxReq::INDEX_NAME_SECTION);
    SimplePropertiesSectionReader r1(ssPtr, getSectionSegmentPool());
    c_tableDesc.init();
    SimpleProperties::UnpackStatus status = SimpleProperties::unpack(
        r1, &c_tableDesc,
        DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
        true, true);
    if (status != SimpleProperties::Eof) {
      opPtr.p->m_errorCode = CreateIndxRef::InvalidName;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      createIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    memcpy(opPtr.p->m_indexName, c_tableDesc.TableName, MAX_TAB_NAME_SIZE);
    opPtr.p->m_loggedIndex = c_tableDesc.TableLoggedFlag;
    opPtr.p->m_temporaryIndex = c_tableDesc.TableTemporaryFlag;
    releaseSections(signal);
    // master expects to hear from all
    if (opPtr.p->m_isMaster)
      opPtr.p->m_signalCounter = c_aliveNodes;
    createIndex_slavePrepare(signal, opPtr);
    createIndex_sendReply(signal, opPtr, false);
    return;
  }
  c_opCreateIndex.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == CreateIndxReq::RT_DICT_COMMIT ||
        requestType == CreateIndxReq::RT_DICT_ABORT) {
      jam();
      if (requestType == CreateIndxReq::RT_DICT_COMMIT) {
        opPtr.p->m_request.setIndexId(req->getIndexId());
        opPtr.p->m_request.setIndexVersion(req->getIndexVersion());
        createIndex_slaveCommit(signal, opPtr);
      } else {
        createIndex_slaveAbort(signal, opPtr);
      }
      createIndex_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opCreateIndex.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  releaseSections(signal);
  OpCreateIndex opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = CreateIndxRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  createIndex_sendReply(signal, opPtr, true);
}

void
Dbdict::execCREATE_INDX_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  CreateIndxConf* conf = (CreateIndxConf*)signal->getDataPtrSend();
  createIndex_recvReply(signal, conf, 0);
}

void
Dbdict::execCREATE_INDX_REF(Signal* signal) 
{
  jamEntry();      
  CreateIndxRef* ref = (CreateIndxRef*)signal->getDataPtrSend();
  createIndex_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::createIndex_recvReply(Signal* signal, const CreateIndxConf* conf,
    const CreateIndxRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const CreateIndxReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == CreateIndxReq::RT_TC) {
    jam();
    // part of alter index operation
    OpAlterIndexPtr opPtr;
    c_opAlterIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterIndex_fromCreateTc(signal, opPtr);
    return;
  }
  OpCreateIndexPtr opPtr;
  c_opCreateIndex.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == CreateIndxReq::RT_DICT_COMMIT ||
      requestType == CreateIndxReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    createIndex_sendReply(signal, opPtr, true);
    c_opCreateIndex.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = CreateIndxReq::RT_DICT_ABORT;
    createIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  if (requestType == CreateIndxReq::RT_DICT_PREPARE) {
    jam();
    // start index table create
    createIndex_toCreateTable(signal, opPtr);
    if (opPtr.p->hasError()) {
      jam();
      opPtr.p->m_requestType = CreateIndxReq::RT_DICT_ABORT;
      createIndex_sendSlaveReq(signal, opPtr);
      return;
    }
    return;
  }
  ndbrequire(false);
}

void
Dbdict::createIndex_slavePrepare(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  if (ERROR_INSERTED(6006) && ! opPtr.p->m_isMaster) {
    ndbrequire(false);
  }
}

void
Dbdict::createIndex_toCreateTable(Signal* signal, OpCreateIndexPtr opPtr)
{
  union {
    char tableName[MAX_TAB_NAME_SIZE];
    char attributeName[MAX_ATTR_NAME_SIZE];
  };
  Uint32 k;
  Uint32 attrid_map[MAX_ATTRIBUTES_IN_INDEX];

  jam();
  const CreateIndxReq* const req = &opPtr.p->m_request;
  // signal data writer
  Uint32* wbuffer = &c_indexPage.word[0];
  LinearWriter w(wbuffer, sizeof(c_indexPage) >> 2);
  w.first();
  // get table being indexed
  if (! (req->getTableId() < c_tableRecordPool.getSize())) {
    jam();
    opPtr.p->m_errorCode = CreateIndxRef::InvalidPrimaryTable;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, req->getTableId());
  if (tablePtr.p->tabState != TableRecord::DEFINED &&
      tablePtr.p->tabState != TableRecord::BACKUP_ONGOING) {
    jam();
    opPtr.p->m_errorCode = CreateIndxRef::InvalidPrimaryTable;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  if (! tablePtr.p->isTable()) {
    jam();
    opPtr.p->m_errorCode = CreateIndxRef::InvalidPrimaryTable;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }

  // Check that the temporary status of index is compatible with table.
  if (!opPtr.p->m_temporaryIndex &&
      tablePtr.p->m_bits & TableRecord::TR_Temporary)
  {
    jam();
    opPtr.p->m_errorCode= CreateIndxRef::TableIsTemporary;
    opPtr.p->m_errorLine= __LINE__;
    return;
  }
  if (opPtr.p->m_temporaryIndex &&
      !(tablePtr.p->m_bits & TableRecord::TR_Temporary))
  {
    // This could be implemented later, but mysqld does currently not detect
    // that the index disappears after SR, and it appears not too useful.
    jam();
    opPtr.p->m_errorCode= CreateIndxRef::TableIsNotTemporary;
    opPtr.p->m_errorLine= __LINE__;
    return;
  }
  if (opPtr.p->m_temporaryIndex && opPtr.p->m_loggedIndex)
  {
    jam();
    opPtr.p->m_errorCode= CreateIndxRef::NoLoggingTemporaryIndex;
    opPtr.p->m_errorLine= __LINE__;
    return;
  }

  // compute index table record
  TableRecord indexRec;
  TableRecordPtr indexPtr;
  indexPtr.i = RNIL;            // invalid
  indexPtr.p = &indexRec;
  initialiseTableRecord(indexPtr);
  indexPtr.p->m_bits = TableRecord::TR_RowChecksum;
  if (req->getIndexType() == DictTabInfo::UniqueHashIndex) {
    indexPtr.p->m_bits |= (opPtr.p->m_loggedIndex ? TableRecord::TR_Logged:0);
    indexPtr.p->m_bits |=
      (opPtr.p->m_temporaryIndex ? TableRecord::TR_Temporary : 0);
    indexPtr.p->fragmentType = DictTabInfo::DistrKeyUniqueHashIndex;
  } else if (req->getIndexType() == DictTabInfo::OrderedIndex) {
    // first version will not supported logging
    if (opPtr.p->m_loggedIndex) {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::InvalidIndexType;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
    indexPtr.p->m_bits |=
      (opPtr.p->m_temporaryIndex ? TableRecord::TR_Temporary : 0);
    indexPtr.p->fragmentType = DictTabInfo::DistrKeyOrderedIndex;
  } else {
    jam();
    opPtr.p->m_errorCode = CreateIndxRef::InvalidIndexType;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  indexPtr.p->tableType = (DictTabInfo::TableType)req->getIndexType();
  indexPtr.p->primaryTableId = req->getTableId();
  indexPtr.p->noOfAttributes = opPtr.p->m_attrList.sz;
  indexPtr.p->tupKeyLength = 0;
  if (indexPtr.p->noOfAttributes == 0) {
    jam();
    opPtr.p->m_errorCode = CreateIndxRef::InvalidIndexType;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }

  if (indexPtr.p->isOrderedIndex()) {
    // tree node size in words (make configurable later)
    indexPtr.p->tupKeyLength = MAX_TTREE_NODE_SIZE;
  }

  AttributeMask mask;
  mask.clear();
  for (k = 0; k < opPtr.p->m_attrList.sz; k++) {
    jam();
    unsigned current_id= opPtr.p->m_attrList.id[k];
    Uint32 tAttr = tablePtr.p->m_attributes.firstItem;
    AttributeRecord* aRec = NULL;
    for (; tAttr != RNIL; )
    {
      aRec = c_attributeRecordPool.getPtr(tAttr);
      if (aRec->attributeId != current_id)
      {
	tAttr= aRec->nextList;
	continue;
      }
      jam();
      break;
    }
    if (tAttr == RNIL) {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::BadRequestType;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
    if (mask.get(current_id))
    {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::DuplicateAttributes;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
    const Uint32 a = aRec->attributeDescriptor;

    if (AttributeDescriptor::getDiskBased(a))
    {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::IndexOnDiskAttributeError;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
    
    mask.set(current_id);
    unsigned kk= k;
    if (indexPtr.p->isHashIndex()) {
      const Uint32 s1 = AttributeDescriptor::getSize(a);
      const Uint32 s2 = AttributeDescriptor::getArraySize(a);
      indexPtr.p->tupKeyLength += ((1 << s1) * s2 + 31) >> 5;

      for (; kk > 0 && current_id < attrid_map[kk-1]>>16; kk--)
	attrid_map[kk]= attrid_map[kk-1];
    }
    attrid_map[kk]= k | (current_id << 16);
  }

  indexPtr.p->noOfPrimkey = indexPtr.p->noOfAttributes;
  // plus concatenated primary table key attribute
  indexPtr.p->noOfAttributes += 1;
  indexPtr.p->noOfNullAttr = 0;
  // write index table
  w.add(DictTabInfo::TableName, opPtr.p->m_indexName);
  w.add(DictTabInfo::TableLoggedFlag, !!(indexPtr.p->m_bits & TableRecord::TR_Logged));
  w.add(DictTabInfo::TableTemporaryFlag, !!(indexPtr.p->m_bits & TableRecord::TR_Temporary));
  w.add(DictTabInfo::FragmentTypeVal, indexPtr.p->fragmentType);
  w.add(DictTabInfo::TableTypeVal, indexPtr.p->tableType);
  Rope name(c_rope_pool, tablePtr.p->tableName);
  name.copy(tableName);
  w.add(DictTabInfo::PrimaryTable, tableName);
  w.add(DictTabInfo::PrimaryTableId, tablePtr.i);
  w.add(DictTabInfo::NoOfAttributes, indexPtr.p->noOfAttributes);
  w.add(DictTabInfo::NoOfKeyAttr, indexPtr.p->noOfPrimkey);
  w.add(DictTabInfo::NoOfNullable, indexPtr.p->noOfNullAttr);
  w.add(DictTabInfo::KeyLength, indexPtr.p->tupKeyLength);
  w.add(DictTabInfo::SingleUserMode, (Uint32)NDB_SUM_READ_WRITE);
  // write index key attributes
  for (k = 0; k < opPtr.p->m_attrList.sz; k++) {
    // insert the attributes in the order decided above in attrid_map
    // k is new order, current_id is in previous order
    // ToDo: make sure "current_id" is stored with the table and
    // passed up to NdbDictionary
    unsigned current_id= opPtr.p->m_attrList.id[attrid_map[k] & 0xffff];
    jam();
    for (Uint32 tAttr = tablePtr.p->m_attributes.firstItem; tAttr != RNIL; ) {
      AttributeRecord* aRec = c_attributeRecordPool.getPtr(tAttr);
      tAttr = aRec->nextList;
      if (aRec->attributeId != current_id)
        continue;
      jam();
      const Uint32 a = aRec->attributeDescriptor;
      bool isNullable = AttributeDescriptor::getNullable(a);
      Uint32 arrayType = AttributeDescriptor::getArrayType(a);
      Rope attrName(c_rope_pool, aRec->attributeName);
      attrName.copy(attributeName);
      w.add(DictTabInfo::AttributeName, attributeName);
      Uint32 attrType = AttributeDescriptor::getType(a);
      // computed
      w.add(DictTabInfo::AttributeId, k);
      if (indexPtr.p->isHashIndex()) {
        w.add(DictTabInfo::AttributeKeyFlag, (Uint32)true);
        w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
      }
      if (indexPtr.p->isOrderedIndex()) {
        w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
        w.add(DictTabInfo::AttributeNullableFlag, (Uint32)isNullable);
      }
      w.add(DictTabInfo::AttributeArrayType, arrayType);
      w.add(DictTabInfo::AttributeExtType, attrType);
      w.add(DictTabInfo::AttributeExtPrecision, aRec->extPrecision);
      w.add(DictTabInfo::AttributeExtScale, aRec->extScale);
      w.add(DictTabInfo::AttributeExtLength, aRec->extLength);
      w.add(DictTabInfo::AttributeEnd, (Uint32)true);
    }
  }
  if (indexPtr.p->isHashIndex()) {
    jam();
    
    Uint32 key_type = NDB_ARRAYTYPE_FIXED;
    AttributeRecordPtr attrPtr;
    LocalDLFifoList<AttributeRecord> alist(c_attributeRecordPool,
					   tablePtr.p->m_attributes);
    for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr)) 
    {
      const Uint32 desc = attrPtr.p->attributeDescriptor;
      if (AttributeDescriptor::getPrimaryKey(desc) &&
	  AttributeDescriptor::getArrayType(desc) != NDB_ARRAYTYPE_FIXED)
      {
	key_type = NDB_ARRAYTYPE_MEDIUM_VAR;
	break;
      }
    }
    
    // write concatenated primary table key attribute i.e. keyinfo
    w.add(DictTabInfo::AttributeName, "NDB$PK");
    w.add(DictTabInfo::AttributeId, opPtr.p->m_attrList.sz);
    w.add(DictTabInfo::AttributeArrayType, key_type);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeExtType, (Uint32)DictTabInfo::ExtUnsigned);
    w.add(DictTabInfo::AttributeExtLength, tablePtr.p->tupKeyLength+1);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  if (indexPtr.p->isOrderedIndex()) {
    jam();
    // write index tree node as Uint32 array attribute
    w.add(DictTabInfo::AttributeName, "NDB$TNODE");
    w.add(DictTabInfo::AttributeId, opPtr.p->m_attrList.sz);
    // should not matter but VAR crashes in TUP
    w.add(DictTabInfo::AttributeArrayType, (Uint32)NDB_ARRAYTYPE_FIXED);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)true);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeExtType, (Uint32)DictTabInfo::ExtUnsigned);
    w.add(DictTabInfo::AttributeExtLength, indexPtr.p->tupKeyLength);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  // finish
  w.add(DictTabInfo::TableEnd, (Uint32)true);
  // remember to...
  releaseSections(signal);
  // send create index table request
  CreateTableReq * const cre = (CreateTableReq*)signal->getDataPtrSend();
  cre->senderRef = reference();
  cre->senderData = opPtr.p->key;
  LinearSectionPtr lsPtr[3];
  lsPtr[0].p = wbuffer;
  lsPtr[0].sz = w.getWordsUsed();
  sendSignal(DBDICT_REF, GSN_CREATE_TABLE_REQ,
      signal, CreateTableReq::SignalLength, JBB, lsPtr, 1);
}

void
Dbdict::createIndex_fromCreateTable(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = CreateIndxReq::RT_DICT_ABORT;
    createIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  if (! opPtr.p->m_request.getOnline()) {
    jam();
    opPtr.p->m_requestType = CreateIndxReq::RT_DICT_COMMIT;
    createIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  createIndex_toAlterIndex(signal, opPtr);
}

void
Dbdict::createIndex_toAlterIndex(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  AlterIndxReq* const req = (AlterIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(AlterIndxReq::RT_CREATE_INDEX);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setIndexId(opPtr.p->m_request.getIndexId());
  req->setIndexVersion(opPtr.p->m_request.getIndexVersion());
  req->setOnline(true);
  sendSignal(reference(), GSN_ALTER_INDX_REQ,
      signal, AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::createIndex_fromAlterIndex(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = CreateIndxReq::RT_DICT_ABORT;
    createIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = CreateIndxReq::RT_DICT_COMMIT;
  createIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::createIndex_slaveCommit(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  const Uint32 indexId = opPtr.p->m_request.getIndexId();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, indexId);
  if (! opPtr.p->m_request.getOnline()) {
    ndbrequire(indexPtr.p->indexState == TableRecord::IS_UNDEFINED);
    indexPtr.p->indexState = TableRecord::IS_OFFLINE;
  } else {
    ndbrequire(indexPtr.p->indexState == TableRecord::IS_ONLINE);
  }
}

void
Dbdict::createIndex_slaveAbort(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  CreateIndxReq* const req = &opPtr.p->m_request;
  const Uint32 indexId = req->getIndexId();
  if (indexId >= c_tableRecordPool.getSize()) {
    jam();
    return;
  }
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, indexId);
  if (! indexPtr.p->isIndex()) {
    jam();
    return;
  }
  indexPtr.p->indexState = TableRecord::IS_BROKEN;
}

void
Dbdict::createIndex_sendSlaveReq(Signal* signal, OpCreateIndexPtr opPtr)
{
  jam();
  CreateIndxReq* const req = (CreateIndxReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  opPtr.p->m_signalCounter = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  sendSignal(rg, GSN_CREATE_INDX_REQ,
      signal, CreateIndxReq::SignalLength, JBB);
}

void
Dbdict::createIndex_sendReply(Signal* signal, OpCreateIndexPtr opPtr,
    bool toUser)
{
  CreateIndxRef* rep = (CreateIndxRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_CREATE_INDX_CONF;
  Uint32 length = CreateIndxConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == CreateIndxReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = CreateIndxConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  rep->setIndexVersion(opPtr.p->m_request.getIndexVersion());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0)
      opPtr.p->m_errorNode = getOwnNodeId();
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_CREATE_INDX_REF;
    length = CreateIndxRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/**
 * MODULE: Drop index.
 *
 * Drop index.  First alters the index offline (i.e.  drops metadata in
 * other blocks) and then drops the index table.
 */

void
Dbdict::execDROP_INDX_REQ(Signal* signal)
{
  jamEntry();
  DropIndxReq* const req = (DropIndxReq*)signal->getDataPtrSend();
  OpDropIndexPtr opPtr;

  int err = DropIndxRef::BadRequestType;
  const Uint32 senderRef = signal->senderBlockRef();
  const DropIndxReq::RequestType requestType = req->getRequestType();
  if (requestType == DropIndxReq::RT_USER) {
    jam();
    if (signal->getLength() == DropIndxReq::SignalLength) {
      jam();
      DropIndxRef::ErrorCode tmperr = DropIndxRef::NoError;
      if (getOwnNodeId() != c_masterNodeId) {
        jam();
        tmperr = DropIndxRef::NotMaster;
      }
      else if (checkSingleUserMode(senderRef))
      {
        jam();
        tmperr = DropIndxRef::SingleUser;
      }
      else 
      {
        DictLockReq lockReq;
        lockReq.userPtr = RNIL;
        lockReq.userRef = reference();
        lockReq.lockType = DictLockReq::DropIndexLock;
        tmperr = (DropIndxRef::ErrorCode)dict_lock_trylock(&lockReq);

        if (tmperr == 0)
        {
          jam();
          dict_lock_unlock(0, &lockReq);
        }
      }

      if (tmperr != DropIndxRef::NoError) {
	err = tmperr;
	goto error;
      }

      // forward initial request plus operation key to all
      Uint32 indexId= req->getIndexId();
      Uint32 indexVersion= req->getIndexVersion();

      if(indexId >= c_tableRecordPool.getSize())
      {
	err = DropIndxRef::IndexNotFound;
	goto error;
      }

      TableRecordPtr tmp;
      c_tableRecordPool.getPtr(tmp, indexId);
      if(tmp.p->tabState == TableRecord::NOT_DEFINED ||
	 tmp.p->tableVersion != indexVersion)
      {
	err = DropIndxRef::InvalidIndexVersion;
	goto error;
      }
      
      if (! tmp.p->isIndex()) {
	jam();
	err = DropIndxRef::NotAnIndex;
	goto error;
      }
      
      if (tmp.p->indexState != TableRecord::IS_ONLINE)
        req->addRequestFlag(RequestFlag::RF_FORCE);

      tmp.p->indexState = TableRecord::IS_DROPPING;

      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, c_aliveNodes);
      sendSignal(rg, GSN_DROP_INDX_REQ,
		 signal, DropIndxReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == DropIndxReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpDropIndex opBusy;
    if (! c_opDropIndex.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = DropIndxReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = DropIndxRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      dropIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opDropIndex.add(opPtr);
    // master expects to hear from all
    if (opPtr.p->m_isMaster)
      opPtr.p->m_signalCounter = c_aliveNodes;
    dropIndex_slavePrepare(signal, opPtr);
    dropIndex_sendReply(signal, opPtr, false);
    return;
  }
  c_opDropIndex.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == DropIndxReq::RT_DICT_COMMIT ||
        requestType == DropIndxReq::RT_DICT_ABORT) {
      jam();
      if (requestType == DropIndxReq::RT_DICT_COMMIT)
        dropIndex_slaveCommit(signal, opPtr);
      else
        dropIndex_slaveAbort(signal, opPtr);
      dropIndex_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opDropIndex.release(opPtr);
      return;
    }
  }
error:
  jam();
  // return to sender
  OpDropIndex opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = (DropIndxRef::ErrorCode)err;
  opPtr.p->m_errorLine = __LINE__;
  opPtr.p->m_errorNode = c_masterNodeId;
  dropIndex_sendReply(signal, opPtr, true);
}

void
Dbdict::execDROP_INDX_CONF(Signal* signal)
{
  jamEntry();
  DropIndxConf* conf = (DropIndxConf*)signal->getDataPtrSend();
  dropIndex_recvReply(signal, conf, 0);
}

void
Dbdict::execDROP_INDX_REF(Signal* signal) 
{
  jamEntry();
  DropIndxRef* ref = (DropIndxRef*)signal->getDataPtrSend();
  dropIndex_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::dropIndex_recvReply(Signal* signal, const DropIndxConf* conf,
    const DropIndxRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const DropIndxReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == DropIndxReq::RT_TC) {
    jam();
    // part of alter index operation
    OpAlterIndexPtr opPtr;
    c_opAlterIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterIndex_fromDropTc(signal, opPtr);
    return;
  }
  OpDropIndexPtr opPtr;
  c_opDropIndex.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == DropIndxReq::RT_DICT_COMMIT ||
      requestType == DropIndxReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    dropIndex_sendReply(signal, opPtr, true);
    c_opDropIndex.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = DropIndxReq::RT_DICT_ABORT;
    dropIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  if (requestType == DropIndxReq::RT_DICT_PREPARE) {
    jam();
    // start alter offline
    dropIndex_toAlterIndex(signal, opPtr);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::dropIndex_slavePrepare(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  DropIndxReq* const req = &opPtr.p->m_request;
  // check index exists
  TableRecordPtr indexPtr;
  if (! (req->getIndexId() < c_tableRecordPool.getSize())) {
    jam();
    opPtr.p->m_errorCode = DropIndxRef::IndexNotFound;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  c_tableRecordPool.getPtr(indexPtr, req->getIndexId());
  if (indexPtr.p->tabState != TableRecord::DEFINED) {
    jam();
    opPtr.p->m_errorCode = DropIndxRef::IndexNotFound;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  if (! indexPtr.p->isIndex()) {
    jam();
    opPtr.p->m_errorCode = DropIndxRef::NotAnIndex;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  // ignore incoming primary table id
  req->setTableId(indexPtr.p->primaryTableId);
}

void
Dbdict::dropIndex_toAlterIndex(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  AlterIndxReq* const req = (AlterIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(AlterIndxReq::RT_DROP_INDEX);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setIndexId(opPtr.p->m_request.getIndexId());
  req->setIndexVersion(opPtr.p->m_request.getIndexVersion());
  req->setOnline(false);
  sendSignal(reference(), GSN_ALTER_INDX_REQ,
      signal, AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::dropIndex_fromAlterIndex(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = DropIndxReq::RT_DICT_ABORT;
    dropIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  dropIndex_toDropTable(signal, opPtr);
}

void
Dbdict::dropIndex_toDropTable(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  DropTableReq* const req = (DropTableReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = opPtr.p->key;
  req->tableId = opPtr.p->m_request.getIndexId();
  req->tableVersion = opPtr.p->m_request.getIndexVersion();
  sendSignal(reference(), GSN_DROP_TABLE_REQ,
      signal,DropTableReq::SignalLength, JBB);
}

void
Dbdict::dropIndex_fromDropTable(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = DropIndxReq::RT_DICT_ABORT;
    dropIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = DropIndxReq::RT_DICT_COMMIT;
  dropIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::dropIndex_slaveCommit(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
}

void
Dbdict::dropIndex_slaveAbort(Signal* signal, OpDropIndexPtr opPtr)
{
  jam();
  DropIndxReq* const req = &opPtr.p->m_request;
  const Uint32 indexId = req->getIndexId();
  if (indexId >= c_tableRecordPool.getSize()) {
    jam();
    return;
  }
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, indexId);
  indexPtr.p->indexState = TableRecord::IS_BROKEN;
}

void
Dbdict::dropIndex_sendSlaveReq(Signal* signal, OpDropIndexPtr opPtr)
{
  DropIndxReq* const req = (DropIndxReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  opPtr.p->m_signalCounter = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  sendSignal(rg, GSN_DROP_INDX_REQ,
      signal, DropIndxReq::SignalLength, JBB);
}

void
Dbdict::dropIndex_sendReply(Signal* signal, OpDropIndexPtr opPtr,
    bool toUser)
{
  DropIndxRef* rep = (DropIndxRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_DROP_INDX_CONF;
  Uint32 length = DropIndxConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == DropIndxReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = DropIndxConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  rep->setIndexVersion(opPtr.p->m_request.getIndexVersion());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0)
      opPtr.p->m_errorNode = getOwnNodeId();
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_DROP_INDX_REF;
    length = DropIndxRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/*****************************************************
 *
 * Util signalling
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
  EVENT_TRACE;
  ndbrequire(recvSignalUtilReq(signal, 1) == 0);
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

const Uint32 Dbdict::sysTab_NDBEVENTS_0_szs[EVENT_SYSTEM_TABLE_LENGTH] = {
  sizeof(((sysTab_NDBEVENTS_0*)0)->NAME),
  sizeof(((sysTab_NDBEVENTS_0*)0)->EVENT_TYPE),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLEID),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLEVERSION),
  sizeof(((sysTab_NDBEVENTS_0*)0)->TABLE_NAME),
  sizeof(((sysTab_NDBEVENTS_0*)0)->ATTRIBUTE_MASK),
  sizeof(((sysTab_NDBEVENTS_0*)0)->SUBID),
  sizeof(((sysTab_NDBEVENTS_0*)0)->SUBKEY)
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
  c_tableRecordPool.getPtr(tablePtr, opj_ptr_p->m_id);
  ndbrequire(tablePtr.i != RNIL); // system table must exist
  
  Uint32 tableId = tablePtr.p->tableId; /* System table */
  Uint32 noAttr = tablePtr.p->noOfAttributes;
  ndbrequire(noAttr == EVENT_SYSTEM_TABLE_LENGTH);
  
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

  if (! assembleFragments(signal)) {
    jam();
    return;
  }

  CreateEvntReq *req = (CreateEvntReq*)signal->getDataPtr();
  const CreateEvntReq::RequestType requestType = req->getRequestType();
  const Uint32                     requestFlag = req->getRequestFlag();

  if (refToBlock(signal->senderBlockRef()) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    jam();
    releaseSections(signal);
    
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
    releaseSections(signal);

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
    createEvent_RT_DICT_AFTER_GET(signal, evntRecPtr);
    return;
  }
  if (requestType == CreateEvntReq::RT_USER_GET) {
    jam();
    EVENT_TRACE;
    createEvent_RT_USER_GET(signal, evntRecPtr);
    return;
  }
  if (requestType == CreateEvntReq::RT_USER_CREATE) {
    jam();
    EVENT_TRACE;
    createEvent_RT_USER_CREATE(signal, evntRecPtr);
    return;
  }

#ifdef EVENT_DEBUG
  ndbout << "Dbdict.cpp: Dbdict::execCREATE_EVNT_REQ other" << endl;
#endif
  jam();
  releaseSections(signal);
    
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
Dbdict::createEvent_RT_USER_CREATE(Signal* signal, OpCreateEventPtr evntRecPtr)
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
  signal->getSection(ssPtr, CreateEvntReq::EVENT_NAME_SECTION);

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
    // event name
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(signal);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  {
    int len = strlen(evntRecPtr.p->m_eventRec.NAME);
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
    releaseSections(signal);
    
    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();
    
    createEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.TABLE_NAME);
  {
    int len = strlen(evntRecPtr.p->m_eventRec.TABLE_NAME);
    memset(evntRecPtr.p->m_eventRec.TABLE_NAME+len, 0, MAX_TAB_NAME_SIZE-len);
  }

  releaseSections(signal);
  
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

  releaseSections(signal);

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
				   Uint32& error, Uint32& line)
{
  DBUG_ENTER("interpretUtilPrepareErrorCode");
  switch (errorCode) {
  case UtilPrepareRef::NO_ERROR:
    jam();
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARE_SEIZE_ERROR:
    jam();
    error = 748;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARE_PAGES_SEIZE_ERROR:
    jam();
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR:
    jam();
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::DICT_TAB_INFO_ERROR:
    jam();
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  case UtilPrepareRef::MISSING_PROPERTIES_SECTION:
    jam();
    error = 1;
    line = __LINE__;
    DBUG_VOID_RETURN;
  default:
    jam();
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
	AttributeMask m = evntRecPtr.p->m_request.getAttrListBitmask();
	memcpy(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK, &m,
	       sizeof(evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK));
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
				  evntRecPtr.p->m_errorLine);
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
  }
}

void Dbdict::executeTransEventSysTable(Callback *pcallback, Signal *signal,
				       const Uint32 ptrI,
				       sysTab_NDBEVENTS_0& m_eventRec,
				       const Uint32 prepareId,
				       UtilPrepareReq::OperationTypeValue prepReq)
{
  jam();
  const Uint32 noAttr = EVENT_SYSTEM_TABLE_LENGTH;
  Uint32 total_len = 0;

  Uint32* attrHdr = signal->theData + 25;
  Uint32* attrPtr = attrHdr;

  Uint32 id=0;
  // attribute 0 event name: Primary Key
  {
    AttributeHeader::init(attrPtr, id, sysTab_NDBEVENTS_0_szs[id]);
    total_len += sysTab_NDBEVENTS_0_szs[id];
    attrPtr++; id++;
  }

  switch (prepReq) {
  case UtilPrepareReq::Read:
    jam();
    EVENT_TRACE;
    // no more
    while ( id < noAttr )
      AttributeHeader::init(attrPtr++, id++, 0);
    ndbrequire(id == (Uint32) noAttr);
    break;
  case UtilPrepareReq::Insert:
    jam();
    EVENT_TRACE;
    while ( id < noAttr ) {
      AttributeHeader::init(attrPtr, id, sysTab_NDBEVENTS_0_szs[id]);
      total_len += sysTab_NDBEVENTS_0_szs[id];
      attrPtr++; id++;
    }
    ndbrequire(id == (Uint32) noAttr);
    break;
  case UtilPrepareReq::Delete:
    ndbrequire(id == 1);
    break;
  default:
    ndbrequire(false);
  }
    
  LinearSectionPtr headerPtr;
  LinearSectionPtr dataPtr;
    
  headerPtr.p = attrHdr;
  headerPtr.sz = noAttr;
    
  dataPtr.p = (Uint32*)&m_eventRec;
  dataPtr.sz = total_len/4;

  ndbrequire((total_len == sysTab_NDBEVENTS_0_szs[0]) ||
	     (total_len == sizeof(sysTab_NDBEVENTS_0)));

#if 0
    printf("Header size %u\n", headerPtr.sz);
    for(int i = 0; i < (int)headerPtr.sz; i++)
      printf("H'%.8x ", attrHdr[i]);
    printf("\n");
    
    printf("Data size %u\n", dataPtr.sz);
    for(int i = 0; i < (int)dataPtr.sz; i++)
      printf("H'%.8x ", dataPage[i]);
    printf("\n");
#endif

  executeTransaction(pcallback, signal, 
		     ptrI,
		     prepareId,
		     id,
		     headerPtr,
		     dataPtr);
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
  SegmentedSectionPtr headerPtr, dataPtr;
  jam();
  signal->getSection(headerPtr, UtilExecuteReq::HEADER_SECTION);
  SectionReader headerReader(headerPtr, getSectionSegmentPool());
      
  signal->getSection(dataPtr, UtilExecuteReq::DATA_SECTION);
  SectionReader dataReader(dataPtr, getSectionSegmentPool());
  
  AttributeHeader header;
  Uint32 *dst = (Uint32*)&m_eventRec;

  for (int i = 0; i < EVENT_SYSTEM_TABLE_LENGTH; i++) {
    headerReader.getWord((Uint32 *)&header);
    int sz = header.getDataSize();
    for (int i=0; i < sz; i++)
      dataReader.getWord(dst++);
  }

  ndbrequire( ((char*)dst-(char*)&m_eventRec) == sizeof(m_eventRec) );

  releaseSections(signal);
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
      evntRec->m_request.setAttrListBitmask(*(AttributeMask*)
					    evntRecPtr.p->m_eventRec.ATTRIBUTE_MASK);
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
      c_tableRecordPool.getPtr(tablePtr, obj_ptr_p->m_id);
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
Dbdict::createEvent_RT_USER_GET(Signal* signal, OpCreateEventPtr evntRecPtr){
  jam();
  EVENT_TRACE;
#ifdef EVENT_PH2_DEBUG
  ndbout_c("DBDICT(Coordinator) got GSN_CREATE_EVNT_REQ::RT_USER_GET evntRecPtr.i = (%d), ref = %u", evntRecPtr.i, evntRecPtr.p->m_request.getUserRef());
#endif

  SegmentedSectionPtr ssPtr;

  signal->getSection(ssPtr, 0);

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(signal);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    createEvent_sendReply(signal, evntRecPtr);
    return;
  }

  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  int len = strlen(evntRecPtr.p->m_eventRec.NAME);
  memset(evntRecPtr.p->m_eventRec.NAME+len, 0, MAX_TAB_NAME_SIZE-len);
  
  releaseSections(signal);
  
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

  if (ref->errorCode == CreateEvntRef::NF_FakeErrorREF){
    jam();
    evntRecPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(ref->senderRef));
  } else {
    jam();
    evntRecPtr.p->m_reqTracker.reportRef(c_counterMgr, refToNode(ref->senderRef));
  }
  createEvent_sendReply(signal, evntRecPtr);

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
  LinearSectionPtr ptr[1];
  ptr[0].p = (Uint32 *)evntRecPtr.p->m_eventRec.TABLE_NAME;
  ptr[0].sz =
    (strlen(evntRecPtr.p->m_eventRec.TABLE_NAME)+4)/4; // to make sure we have a null

  createEvent_sendReply(signal, evntRecPtr, ptr, 1);
    
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
  sumaReq->tableId          = evntRecPtr.p->m_request.getTableId();
    
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

  if (ref->errorCode == 1415) {
    jam();
    createEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }

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
    } else 
      jam();
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

  LockQueue::Iterator iter;
  if (m_dict_lock.first(m_dict_lock_pool, iter))
  {
    if (iter.m_curr.p->m_req.extra == DictLockReq::NodeRestartLock)
    {
      jam();
      errCode = 1405;
      goto busy;
    }
  }

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

    sendSignal(origSenderRef, GSN_SUB_START_REF, signal,
	       SubStartRef::SignalLength2, JBB);
    return;
  }

  {
    const SubStartReq* req = (SubStartReq*) signal->getDataPtr();
    subbPtr.p->m_senderRef = req->senderRef;
    subbPtr.p->m_senderData = req->senderData;
    subbPtr.p->m_errorCode = 0;
  }
  
  if (refToBlock(origSenderRef) != DBDICT) {
    /*
     * Coordinator
     */
    jam();
    
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
                 SubStartReq::SignalLength2, JBB);
      sendSignalWithDelay(reference(),
                          GSN_SUB_START_REQ,
                          signal, 5000, SubStartReq::SignalLength2);
    }
    else
    {
      sendSignal(rg, GSN_SUB_START_REQ, signal,
                 SubStartReq::SignalLength2, JBB);
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
    sendSignal(SUMA_REF, GSN_SUB_START_REQ, signal, SubStartReq::SignalLength2, JBB);
  }
}

void Dbdict::execSUB_START_REF(Signal* signal)
{
  jamEntry();

  const SubStartRef* ref = (SubStartRef*) signal->getDataPtr();
  Uint32 senderRef  = ref->senderRef;
  Uint32 err = ref->errorCode;

  OpSubEventPtr subbPtr;
  c_opSubEvent.getPtr(subbPtr, ref->senderData);

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();

#ifdef EVENT_PH3_DEBUG
    ndbout_c("DBDICT(Participant) got GSN_SUB_START_REF = (%d)", subbPtr.i);
#endif

    jam();
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
  if (err == SubStartRef::NF_FakeErrorREF){
    jam();
    subbPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  } else {
    jam();
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

    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_CONF,
	       signal, SubStartConf::SignalLength2, JBB);
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

  if (subbPtr.p->m_reqTracker.hasRef()) {
    jam();
#ifdef EVENT_DEBUG
    ndbout_c("SUB_START_REF");
#endif
    SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = subbPtr.p->m_errorCode;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_START_REF,
	       signal, SubStartRef::SignalLength, JBB);
    if (subbPtr.p->m_reqTracker.hasConf()) {
      //  stopStartedNodes(signal);
    }
    c_opSubEvent.release(subbPtr);
    return;
  }
#ifdef EVENT_DEBUG
  ndbout_c("SUB_START_CONF");
#endif
  
  SubStartConf* conf = (SubStartConf*)signal->getDataPtrSend();
  * conf = subbPtr.p->m_sub_start_conf;
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

  if (refToBlock(origSenderRef) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    /*
     * Coordinator but not master
     */
    SubStopRef * ref = (SubStopRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = SubStopRef::NotMaster;
    ref->m_masterNodeId = c_masterNodeId;
    sendSignal(origSenderRef, GSN_SUB_STOP_REF, signal,
	       SubStopRef::SignalLength2, JBB);
    return;
  }
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

    sendSignal(origSenderRef, GSN_SUB_STOP_REF, signal,
	       SubStopRef::SignalLength, JBB);
    return;
  }

  {
    const SubStopReq* req = (SubStopReq*) signal->getDataPtr();
    subbPtr.p->m_senderRef = req->senderRef;
    subbPtr.p->m_senderData = req->senderData;
    subbPtr.p->m_errorCode = 0;
  }
  
  if (refToBlock(origSenderRef) != DBDICT) {
    /*
     * Coordinator
     */
    jam();
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

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();
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
  if (err == SubStopRef::NF_FakeErrorREF){
    jam();
    subbPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  } else {
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
  subbPtr.p->m_sub_stop_conf = *conf;
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

  if (refToBlock(senderRef) != DBDICT &&
      getOwnNodeId() != c_masterNodeId)
  {
    jam();
    releaseSections(signal);

    DropEvntRef * ref = (DropEvntRef *)signal->getDataPtrSend();
    ref->setUserRef(reference());
    ref->setErrorCode(DropEvntRef::NotMaster);
    ref->setErrorLine(__LINE__);
    ref->setErrorNode(reference());
    ref->setMasterNode(c_masterNodeId);
    sendSignal(senderRef, GSN_DROP_EVNT_REF, signal,
	       DropEvntRef::SignalLength2, JBB);
    return;
  }

  // Seize a Create Event record
  if (!c_opDropEvent.seize(evntRecPtr)) {
    // Failed to allocate event record
    jam();
    releaseSections(signal);
 
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

  signal->getSection(ssPtr, 0);

  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
#ifdef EVENT_DEBUG
  r0.printAll(ndbout);
#endif
  // event name
  if ((!r0.first()) ||
      (r0.getValueType() != SimpleProperties::StringValue) ||
      (r0.getValueLen() <= 0)) {
    jam();
    releaseSections(signal);

    evntRecPtr.p->m_errorCode = 1;
    evntRecPtr.p->m_errorLine = __LINE__;
    evntRecPtr.p->m_errorNode = reference();

    dropEvent_sendReply(signal, evntRecPtr);
    DBUG_VOID_RETURN;
  }
  r0.getString(evntRecPtr.p->m_eventRec.NAME);
  {
    int len = strlen(evntRecPtr.p->m_eventRec.NAME);
    memset(evntRecPtr.p->m_eventRec.NAME+len, 0, MAX_TAB_NAME_SIZE-len);
#ifdef EVENT_DEBUG
    printf("DropEvntReq; EventName %s, len %u\n",
	   evntRecPtr.p->m_eventRec.NAME, len);
    for(int i = 0; i < MAX_TAB_NAME_SIZE/4; i++)
      printf("H'%.8x ", ((Uint32*)evntRecPtr.p->m_eventRec.NAME)[i]);
    printf("\n");
#endif
  }
  
  releaseSections(signal);

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

  if (refToBlock(senderRef) == SUMA) {
    /*
     * Participant
     */
    jam();
    OpSubEventPtr subbPtr;
    c_opSubEvent.getPtr(subbPtr, ref->senderData);
    if (err == 1407) {
      // conf this since this may occur if a nodefailure has occured
      // earlier so that the systable was not cleared
      SubRemoveConf* conf = (SubRemoveConf*) signal->getDataPtrSend();
      conf->senderRef  = reference();
      conf->senderData = subbPtr.p->m_senderData;
      sendSignal(subbPtr.p->m_senderRef, GSN_SUB_REMOVE_CONF,
		 signal, SubRemoveConf::SignalLength, JBB);
    } else {
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
  if (err == SubRemoveRef::NF_FakeErrorREF){
    jam();
    eventRecPtr.p->m_reqTracker.ignoreRef(c_counterMgr, refToNode(senderRef));
  } else {
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
				evntRecPtr.p->m_errorCode, evntRecPtr.p->m_errorLine);
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

/**
 * MODULE: Alter index
 *
 * Alter index state.  Alter online creates the index in each TC and
 * then invokes create trigger and alter trigger protocols to activate
 * the 3 triggers.  Alter offline does the opposite.
 *
 * Request type received in REQ and returned in CONF/REF:
 *
 * RT_USER - from API to DICT master
 * RT_CREATE_INDEX - part of create index operation
 * RT_DROP_INDEX - part of drop index operation
 * RT_NODERESTART - node restart, activate locally only
 * RT_SYSTEMRESTART - system restart, activate and build if not logged
 * RT_DICT_PREPARE - prepare participants
 * RT_DICT_TC - to local TC via each participant
 * RT_DICT_COMMIT - commit in each participant
 */

void
Dbdict::execALTER_INDX_REQ(Signal* signal)
{
  jamEntry();
  AlterIndxReq* const req = (AlterIndxReq*)signal->getDataPtrSend();
  OpAlterIndexPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const AlterIndxReq::RequestType requestType = req->getRequestType();
  if (requestType == AlterIndxReq::RT_USER ||
      requestType == AlterIndxReq::RT_CREATE_INDEX ||
      requestType == AlterIndxReq::RT_DROP_INDEX ||
      requestType == AlterIndxReq::RT_NODERESTART ||
      requestType == AlterIndxReq::RT_SYSTEMRESTART) {
    jam();
    const bool isLocal = req->getRequestFlag() & RequestFlag::RF_LOCAL;
    NdbNodeBitmask receiverNodes = c_aliveNodes;
    if (isLocal) {
      receiverNodes.clear();
      receiverNodes.set(getOwnNodeId());
    }
    if (signal->getLength() == AlterIndxReq::SignalLength) {
      jam();
      if (! isLocal && getOwnNodeId() != c_masterNodeId) {
        jam();

	releaseSections(signal);
	OpAlterIndex opBad;
	opPtr.p = &opBad;
	opPtr.p->save(req);
	opPtr.p->m_errorCode = AlterIndxRef::NotMaster;
	opPtr.p->m_errorLine = __LINE__;
	opPtr.p->m_errorNode = c_masterNodeId;
	alterIndex_sendReply(signal, opPtr, true);
        return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, receiverNodes);
      sendSignal(rg, GSN_ALTER_INDX_REQ,
          signal, AlterIndxReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == AlterIndxReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpAlterIndex opBusy;
    if (! c_opAlterIndex.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = AlterIndxRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      alterIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opAlterIndex.add(opPtr);
    // master expects to hear from all
    if (opPtr.p->m_isMaster)
      opPtr.p->m_signalCounter = receiverNodes;
    // check request in all participants
    alterIndex_slavePrepare(signal, opPtr);
    alterIndex_sendReply(signal, opPtr, false);
    return;
  }
  c_opAlterIndex.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == AlterIndxReq::RT_DICT_TC) {
      jam();
      if (opPtr.p->m_request.getOnline())
        alterIndex_toCreateTc(signal, opPtr);
      else
        alterIndex_toDropTc(signal, opPtr);
      return;
    }
    if (requestType == AlterIndxReq::RT_DICT_COMMIT ||
        requestType == AlterIndxReq::RT_DICT_ABORT) {
      jam();
      if (requestType == AlterIndxReq::RT_DICT_COMMIT)
        alterIndex_slaveCommit(signal, opPtr);
      else
        alterIndex_slaveAbort(signal, opPtr);
      alterIndex_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opAlterIndex.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  OpAlterIndex opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = AlterIndxRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  alterIndex_sendReply(signal, opPtr, true);
}

void
Dbdict::execALTER_INDX_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  AlterIndxConf* conf = (AlterIndxConf*)signal->getDataPtrSend();
  alterIndex_recvReply(signal, conf, 0);
}

void
Dbdict::execALTER_INDX_REF(Signal* signal)
{
  jamEntry();
  AlterIndxRef* ref = (AlterIndxRef*)signal->getDataPtrSend();
  alterIndex_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::alterIndex_recvReply(Signal* signal, const AlterIndxConf* conf,
    const AlterIndxRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const AlterIndxReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == AlterIndxReq::RT_CREATE_INDEX) {
    jam();
    // part of create index operation
    OpCreateIndexPtr opPtr;
    c_opCreateIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    createIndex_fromAlterIndex(signal, opPtr);
    return;
  }
  if (requestType == AlterIndxReq::RT_DROP_INDEX) {
    jam();
    // part of drop index operation
    OpDropIndexPtr opPtr;
    c_opDropIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    dropIndex_fromAlterIndex(signal, opPtr);
    return;
  }
  if (requestType == AlterIndxReq::RT_TC ||
      requestType == AlterIndxReq::RT_TUX) {
    jam();
    // part of build index operation
    OpBuildIndexPtr opPtr;
    c_opBuildIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    buildIndex_fromOnline(signal, opPtr);
    return;
  }
  if (requestType == AlterIndxReq::RT_NODERESTART) {
    jam();
    if (ref == 0) {
      infoEvent("DICT: index %u activated", (unsigned)key);
    } else {
      warningEvent("DICT: index %u activation failed: code=%d line=%d",
          (unsigned)key,
          ref->getErrorCode(), ref->getErrorLine());
    }
    activateIndexes(signal, key + 1);
    return;
  }
  if (requestType == AlterIndxReq::RT_SYSTEMRESTART) {
    jam();
    if (ref == 0) {
      infoEvent("DICT: index %u activated done", (unsigned)key);
    } else {
      warningEvent("DICT: index %u activated failed: code=%d line=%d node=%d",
          (unsigned)key,
          ref->getErrorCode(), ref->getErrorLine(), ref->getErrorNode());
    }
    activateIndexes(signal, key + 1);
    return;
  }
  OpAlterIndexPtr opPtr;
  c_opAlterIndex.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == AlterIndxReq::RT_DICT_COMMIT ||
      requestType == AlterIndxReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    alterIndex_sendReply(signal, opPtr, true);
    c_opAlterIndex.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_ABORT;
    alterIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  if (indexPtr.p->isHashIndex()) {
    if (requestType == AlterIndxReq::RT_DICT_PREPARE) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        opPtr.p->m_requestType = AlterIndxReq::RT_DICT_TC;
        alterIndex_sendSlaveReq(signal, opPtr);
      } else {
        // start drop triggers
        alterIndex_toDropTrigger(signal, opPtr);
      }
      return;
    }
    if (requestType == AlterIndxReq::RT_DICT_TC) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        // start create triggers
        alterIndex_toCreateTrigger(signal, opPtr);
      } else {
        opPtr.p->m_requestType = AlterIndxReq::RT_DICT_COMMIT;
        alterIndex_sendSlaveReq(signal, opPtr);
      }
      return;
    }
  }
  if (indexPtr.p->isOrderedIndex()) {
    if (requestType == AlterIndxReq::RT_DICT_PREPARE) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        // start create triggers
        alterIndex_toCreateTrigger(signal, opPtr);
      } else {
        // start drop triggers
        alterIndex_toDropTrigger(signal, opPtr);
      }
      return;
    }
  }
  ndbrequire(false);
}

void
Dbdict::alterIndex_slavePrepare(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  const AlterIndxReq* const req = &opPtr.p->m_request;
  if (! (req->getIndexId() < c_tableRecordPool.getSize())) {
    jam();
    opPtr.p->m_errorCode = AlterIndxRef::Inconsistency;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, req->getIndexId());
  if (indexPtr.p->tabState != TableRecord::DEFINED) {
    jam();
    opPtr.p->m_errorCode = AlterIndxRef::IndexNotFound;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  if (! indexPtr.p->isIndex()) {
    jam();
    opPtr.p->m_errorCode = AlterIndxRef::NotAnIndex;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  if (req->getOnline())
    indexPtr.p->indexState = TableRecord::IS_BUILDING;
  else
    indexPtr.p->indexState = TableRecord::IS_DROPPING;
}

void
Dbdict::alterIndex_toCreateTc(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // request to create index in local TC
  CreateIndxReq* const req = (CreateIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(CreateIndxReq::RT_TC);
  req->setIndexType(indexPtr.p->tableType);
  req->setTableId(indexPtr.p->primaryTableId);
  req->setIndexId(indexPtr.i);
  req->setOnline(true);
  getIndexAttrList(indexPtr, opPtr.p->m_attrList);
  // send
  LinearSectionPtr lsPtr[3];
  lsPtr[0].p = (Uint32*)&opPtr.p->m_attrList;
  lsPtr[0].sz = 1 + opPtr.p->m_attrList.sz;
  sendSignal(calcTcBlockRef(getOwnNodeId()), GSN_CREATE_INDX_REQ,
      signal, CreateIndxReq::SignalLength, JBB, lsPtr, 1);
}

void
Dbdict::alterIndex_fromCreateTc(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  // mark created in local TC
  if (! opPtr.p->hasLastError()) {
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
    indexPtr.p->indexLocal |= TableRecord::IL_CREATED_TC;
  }
  // forward CONF or REF to master
  ndbrequire(opPtr.p->m_requestType == AlterIndxReq::RT_DICT_TC);
  alterIndex_sendReply(signal, opPtr, false);
}

void
Dbdict::alterIndex_toDropTc(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // broken index allowed if force
  if (! (indexPtr.p->indexLocal & TableRecord::IL_CREATED_TC)) {
    jam();
    ndbassert(opPtr.p->m_requestFlag & RequestFlag::RF_FORCE);
    alterIndex_sendReply(signal, opPtr, false);
    return;
  }
  // request to drop in local TC
  DropIndxReq* const req = (DropIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(DropIndxReq::RT_TC);
  req->setTableId(indexPtr.p->primaryTableId);
  req->setIndexId(indexPtr.i);
  req->setIndexVersion(indexPtr.p->tableVersion);
  // send
  sendSignal(calcTcBlockRef(getOwnNodeId()), GSN_DROP_INDX_REQ,
      signal, DropIndxReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromDropTc(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  ndbrequire(opPtr.p->m_requestType == AlterIndxReq::RT_DICT_TC);
  // mark dropped locally
  if (! opPtr.p->hasLastError()) {
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
    indexPtr.p->indexLocal &= ~TableRecord::IL_CREATED_TC;
  }
  // forward CONF or REF to master
  alterIndex_sendReply(signal, opPtr, false);
}

void
Dbdict::alterIndex_toCreateTrigger(Signal* signal, OpAlterIndexPtr opPtr)
{
#if wl3600 // remove in index patch
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // start creation of index triggers
  CreateTrigReq* const req = (CreateTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(CreateTrigReq::RT_ALTER_INDEX);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setIndexId(opPtr.p->m_request.getIndexId());
  req->setTriggerId(RNIL);
  req->setTriggerActionTime(TriggerActionTime::TA_AFTER);
  req->setMonitorAllAttributes(false);
  req->setOnline(true);         // alter online after create
  req->setReceiverRef(0);       // implicit for index triggers
  getIndexAttrMask(indexPtr, req->getAttributeMask());
  // name section
  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  LinearSectionPtr lsPtr[3];
  if (indexPtr.p->isHashIndex()) {
    req->setTriggerType(TriggerType::SECONDARY_INDEX);
    req->setMonitorReplicas(false);
    req->setReportAllMonitoredAttributes(true);
    // insert
    if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL)
      req->setTriggerId(indexPtr.p->insertTriggerId);
    req->setTriggerEvent(TriggerEvent::TE_INSERT);
    sprintf(triggerName, "NDB$INDEX_%u_INSERT", opPtr.p->m_request.getIndexId());
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = buffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(reference(), GSN_CREATE_TRIG_REQ, 
        signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
    // update
    if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL)
      req->setTriggerId(indexPtr.p->updateTriggerId);
    req->setTriggerEvent(TriggerEvent::TE_UPDATE);
    sprintf(triggerName, "NDB$INDEX_%u_UPDATE", opPtr.p->m_request.getIndexId());
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = buffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(reference(), GSN_CREATE_TRIG_REQ, 
        signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
    // delete
    if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL)
      req->setTriggerId(indexPtr.p->deleteTriggerId);
    req->setTriggerEvent(TriggerEvent::TE_DELETE);
    sprintf(triggerName, "NDB$INDEX_%u_DELETE", opPtr.p->m_request.getIndexId());
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = buffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(reference(), GSN_CREATE_TRIG_REQ, 
        signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
    // triggers left to create
    opPtr.p->m_triggerCounter = 3;
    return;
  }
  if (indexPtr.p->isOrderedIndex()) {
    req->addRequestFlag(RequestFlag::RF_NOTCTRIGGER);
    req->setTriggerType(TriggerType::ORDERED_INDEX);
    req->setTriggerActionTime(TriggerActionTime::TA_CUSTOM);
    req->setMonitorReplicas(true);
    req->setReportAllMonitoredAttributes(true);
    // one trigger for 5 events (insert, update, delete, commit, abort)
    if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL)
      req->setTriggerId(indexPtr.p->customTriggerId);
    req->setTriggerEvent(TriggerEvent::TE_CUSTOM);
    sprintf(triggerName, "NDB$INDEX_%u_CUSTOM", opPtr.p->m_request.getIndexId());
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = buffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(reference(), GSN_CREATE_TRIG_REQ, 
        signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
    // triggers left to create
    opPtr.p->m_triggerCounter = 1;
    return;
  }
  ndbrequire(false);
#endif
}

void
Dbdict::alterIndex_fromCreateTrigger(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  ndbrequire(opPtr.p->m_triggerCounter != 0);
  if (--opPtr.p->m_triggerCounter != 0) {
    jam();
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_ABORT;
    alterIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  if(opPtr.p->m_requestType != AlterIndxReq::RT_SYSTEMRESTART){
    // send build request
    alterIndex_toBuildIndex(signal, opPtr);
    return;
  }
  
  /**
   * During system restart, 
   *   leave index in activated but not build state.
   *
   * Build a bit later when REDO has been run
   */
  alterIndex_sendReply(signal, opPtr, true);
}

void
Dbdict::alterIndex_toDropTrigger(Signal* signal, OpAlterIndexPtr opPtr)
{
#if wl3600 // remove in index patch
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // start drop of index triggers
  DropTrigReq* const req = (DropTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(DropTrigReq::RT_ALTER_INDEX);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setIndexId(opPtr.p->m_request.getIndexId());
  req->setTriggerInfo(0);       // not used
  opPtr.p->m_triggerCounter = 0;
  if (indexPtr.p->isHashIndex()) {
    // insert
    req->setTriggerId(indexPtr.p->insertTriggerId);
    sendSignal(reference(), GSN_DROP_TRIG_REQ, 
        signal, DropTrigReq::SignalLength, JBB);
    opPtr.p->m_triggerCounter++;
    // update
    req->setTriggerId(indexPtr.p->updateTriggerId);
    sendSignal(reference(), GSN_DROP_TRIG_REQ, 
        signal, DropTrigReq::SignalLength, JBB);
    opPtr.p->m_triggerCounter++;
    // delete
    req->setTriggerId(indexPtr.p->deleteTriggerId);
    sendSignal(reference(), GSN_DROP_TRIG_REQ, 
        signal, DropTrigReq::SignalLength, JBB);
    opPtr.p->m_triggerCounter++;
    // build
    if (indexPtr.p->buildTriggerId != RNIL) {
      req->setTriggerId(indexPtr.p->buildTriggerId);
      sendSignal(reference(), GSN_DROP_TRIG_REQ, 
          signal, DropTrigReq::SignalLength, JBB);
      opPtr.p->m_triggerCounter++;
    }
    return;
  }
  if (indexPtr.p->isOrderedIndex()) {
    // custom
    req->addRequestFlag(RequestFlag::RF_NOTCTRIGGER);
    req->setTriggerId(indexPtr.p->customTriggerId);
    sendSignal(reference(), GSN_DROP_TRIG_REQ, 
        signal, DropTrigReq::SignalLength, JBB);
    opPtr.p->m_triggerCounter++;
    return;
  }
  ndbrequire(false);
#endif
}

void
Dbdict::alterIndex_fromDropTrigger(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  ndbrequire(opPtr.p->m_triggerCounter != 0);
  if (--opPtr.p->m_triggerCounter != 0) {
    jam();
    return;
  }
  // finally drop index in each TC
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  const bool isHashIndex = indexPtr.p->isHashIndex();
  const bool isOrderedIndex = indexPtr.p->isOrderedIndex();
  ndbrequire(isHashIndex != isOrderedIndex);    // xor
  if (isHashIndex)
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_TC;
  if (isOrderedIndex)
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_COMMIT;
  alterIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::alterIndex_toBuildIndex(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  // get index and table records
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  // build request to self (short signal)
  BuildIndxReq* const req = (BuildIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(BuildIndxReq::RT_ALTER_INDEX);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setBuildId(0);   // not used
  req->setBuildKey(0);  // not used
  req->setIndexType(indexPtr.p->tableType);
  req->setIndexId(indexPtr.i);
  req->setTableId(indexPtr.p->primaryTableId);
  req->setParallelism(16);
  // send
  sendSignal(reference(), GSN_BUILDINDXREQ,
      signal, BuildIndxReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_fromBuildIndex(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = AlterIndxReq::RT_DICT_ABORT;
    alterIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = AlterIndxReq::RT_DICT_COMMIT;
  alterIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::alterIndex_slaveCommit(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  // get index record
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  indexPtr.p->indexState = TableRecord::IS_ONLINE;
}

void
Dbdict::alterIndex_slaveAbort(Signal* signal, OpAlterIndexPtr opPtr)
{
  jam();
  // find index record
  const Uint32 indexId = opPtr.p->m_request.getIndexId();
  if (indexId >= c_tableRecordPool.getSize())
    return;
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, indexId);
  if (! indexPtr.p->isIndex())
    return;
  // mark broken
  indexPtr.p->indexState = TableRecord::IS_BROKEN;
}

void
Dbdict::alterIndex_sendSlaveReq(Signal* signal, OpAlterIndexPtr opPtr)
{
  AlterIndxReq* const req = (AlterIndxReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  NdbNodeBitmask receiverNodes = c_aliveNodes;
  if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL) {
    receiverNodes.clear();
    receiverNodes.set(getOwnNodeId());
  }
  opPtr.p->m_signalCounter = receiverNodes;
  NodeReceiverGroup rg(DBDICT, receiverNodes);
  sendSignal(rg, GSN_ALTER_INDX_REQ,
      signal, AlterIndxReq::SignalLength, JBB);
}

void
Dbdict::alterIndex_sendReply(Signal* signal, OpAlterIndexPtr opPtr,
    bool toUser)
{
  AlterIndxRef* rep = (AlterIndxRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_ALTER_INDX_CONF;
  Uint32 length = AlterIndxConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == AlterIndxReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = AlterIndxConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0)
      opPtr.p->m_errorNode = getOwnNodeId();
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_ALTER_INDX_REF;
    length = AlterIndxRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/**
 * MODULE: Build index
 *
 * Build index or all indexes on a table.  Request type:
 *
 * RT_USER - normal user request, not yet used
 * RT_ALTER_INDEX - from alter index
 * RT_SYSTEM_RESTART - 
 * RT_DICT_PREPARE - prepare participants
 * RT_DICT_TRIX - to participant on way to local TRIX
 * RT_DICT_COMMIT - commit in each participant
 * RT_DICT_ABORT - abort
 * RT_TRIX - to local TRIX
 */

void
Dbdict::execBUILDINDXREQ(Signal* signal)
{
  jamEntry();
  BuildIndxReq* const req = (BuildIndxReq*)signal->getDataPtrSend();
  OpBuildIndexPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const BuildIndxReq::RequestType requestType = req->getRequestType();
  if (requestType == BuildIndxReq::RT_USER ||
      requestType == BuildIndxReq::RT_ALTER_INDEX ||
      requestType == BuildIndxReq::RT_SYSTEMRESTART) {
    jam();

    const bool isLocal = req->getRequestFlag() & RequestFlag::RF_LOCAL;
    NdbNodeBitmask receiverNodes = c_aliveNodes;
    if (isLocal) {
      receiverNodes.clear();
      receiverNodes.set(getOwnNodeId());
    }
    
    if (signal->getLength() == BuildIndxReq::SignalLength) {
      jam();
      
      if (!isLocal && getOwnNodeId() != c_masterNodeId) {
        jam();
	
	releaseSections(signal);
	OpBuildIndex opBad;
	opPtr.p = &opBad;
	opPtr.p->save(req);
	opPtr.p->m_errorCode = BuildIndxRef::NotMaster;
	opPtr.p->m_errorLine = __LINE__;
	opPtr.p->m_errorNode = c_masterNodeId;
	buildIndex_sendReply(signal, opPtr, true);
        return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, receiverNodes);
      sendSignal(rg, GSN_BUILDINDXREQ,
		 signal, BuildIndxReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == BuildIndxReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpBuildIndex opBusy;
    if (! c_opBuildIndex.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = BuildIndxReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = BuildIndxRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      buildIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opBuildIndex.add(opPtr);
    // master expects to hear from all
    opPtr.p->m_signalCounter = receiverNodes;
    buildIndex_sendReply(signal, opPtr, false);
    return;
  }
  c_opBuildIndex.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == BuildIndxReq::RT_DICT_TRIX) {
      jam();
      buildIndex_buildTrix(signal, opPtr);
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_TC ||
        requestType == BuildIndxReq::RT_DICT_TUX) {
      jam();
      buildIndex_toOnline(signal, opPtr);
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_COMMIT ||
        requestType == BuildIndxReq::RT_DICT_ABORT) {
      jam();
      buildIndex_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opBuildIndex.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  OpBuildIndex opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = BuildIndxRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  buildIndex_sendReply(signal, opPtr, true);
}

void
Dbdict::execBUILDINDXCONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  BuildIndxConf* conf = (BuildIndxConf*)signal->getDataPtrSend();
  buildIndex_recvReply(signal, conf, 0);
}

void
Dbdict::execBUILDINDXREF(Signal* signal)
{
  jamEntry();
  BuildIndxRef* ref = (BuildIndxRef*)signal->getDataPtrSend();
  buildIndex_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::buildIndex_recvReply(Signal* signal, const BuildIndxConf* conf,
    const BuildIndxRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const BuildIndxReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == BuildIndxReq::RT_ALTER_INDEX) {
    jam();
    // part of alter index operation
    OpAlterIndexPtr opPtr;
    c_opAlterIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterIndex_fromBuildIndex(signal, opPtr);
    return;
  }

  if (requestType == BuildIndxReq::RT_SYSTEMRESTART) {
    jam();
    if (ref == 0) {
      infoEvent("DICT: index %u rebuild done", (unsigned)key);
    } else {
      warningEvent("DICT: index %u rebuild failed: code=%d line=%d node=%d",
		   (unsigned)key, ref->getErrorCode());
    }
    rebuildIndexes(signal, key + 1);
    return;
  }

  OpBuildIndexPtr opPtr;
  c_opBuildIndex.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  opPtr.p->setError(ref);
  if (requestType == BuildIndxReq::RT_TRIX) {
    jam();
    // forward to master
    opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TRIX;
    buildIndex_sendReply(signal, opPtr, false);
    return;
  }
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == BuildIndxReq::RT_DICT_COMMIT ||
      requestType == BuildIndxReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    buildIndex_sendReply(signal, opPtr, true);
    c_opBuildIndex.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = BuildIndxReq::RT_DICT_ABORT;
    buildIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  if (indexPtr.p->isHashIndex()) {
    if (requestType == BuildIndxReq::RT_DICT_PREPARE) {
      jam();
      if (! (opPtr.p->m_requestFlag & RequestFlag::RF_NOBUILD)) {
        buildIndex_toCreateConstr(signal, opPtr);
      } else {
        opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TC;
        buildIndex_sendSlaveReq(signal, opPtr);
      }
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_TRIX) {
      jam();
      ndbrequire(! (opPtr.p->m_requestFlag & RequestFlag::RF_NOBUILD));
      buildIndex_toDropConstr(signal, opPtr);
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_TC) {
      jam();
      opPtr.p->m_requestType = BuildIndxReq::RT_DICT_COMMIT;
      buildIndex_sendSlaveReq(signal, opPtr);
      return;
    }
  }
  if (indexPtr.p->isOrderedIndex()) {
    if (requestType == BuildIndxReq::RT_DICT_PREPARE) {
      jam();
      if (! (opPtr.p->m_requestFlag & RequestFlag::RF_NOBUILD)) {
        opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TRIX;
        buildIndex_sendSlaveReq(signal, opPtr);
      } else {
        opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TUX;
        buildIndex_sendSlaveReq(signal, opPtr);
      }
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_TRIX) {
      jam();
      ndbrequire(! (opPtr.p->m_requestFlag & RequestFlag::RF_NOBUILD));
      opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TUX;
      buildIndex_sendSlaveReq(signal, opPtr);
      return;
    }
    if (requestType == BuildIndxReq::RT_DICT_TUX) {
      jam();
      opPtr.p->m_requestType = BuildIndxReq::RT_DICT_COMMIT;
      buildIndex_sendSlaveReq(signal, opPtr);
      return;
    }
  }
  ndbrequire(false);
} 

void
Dbdict::buildIndex_toCreateConstr(Signal* signal, OpBuildIndexPtr opPtr)
{
#if wl3600 // remove in index patch
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // request to create constraint trigger
  CreateTrigReq* req = (CreateTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(CreateTrigReq::RT_BUILD_INDEX);
  req->addRequestFlag(0);       // none
  req->setTableId(indexPtr.i);
  req->setIndexId(RNIL);
  req->setTriggerId(RNIL);
  req->setTriggerType(TriggerType::READ_ONLY_CONSTRAINT);
  req->setTriggerActionTime(TriggerActionTime::TA_AFTER);
  req->setTriggerEvent(TriggerEvent::TE_UPDATE);
  req->setMonitorReplicas(false);
  req->setMonitorAllAttributes(false);
  req->setReportAllMonitoredAttributes(true);
  req->setOnline(true);         // alter online after create
  req->setReceiverRef(0);       // no receiver, REF-ed by TUP
  req->getAttributeMask().clear();
  // NDB$PK is last attribute
  req->getAttributeMask().set(indexPtr.p->noOfAttributes - 1);
  // name section
  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 buffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];    // SP string
  LinearWriter w(buffer, sizeof(buffer) >> 2);
  LinearSectionPtr lsPtr[3];
  sprintf(triggerName, "NDB$INDEX_%u_BUILD", indexPtr.i);
  w.reset();
  w.add(CreateTrigReq::TriggerNameKey, triggerName);
  lsPtr[0].p = buffer;
  lsPtr[0].sz = w.getWordsUsed();
  sendSignal(reference(), GSN_CREATE_TRIG_REQ,
      signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
#endif
}

void
Dbdict::buildIndex_fromCreateConstr(Signal* signal, OpBuildIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = BuildIndxReq::RT_DICT_ABORT;
    buildIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TRIX;
  buildIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::buildIndex_buildTrix(Signal* signal, OpBuildIndexPtr opPtr)
{
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  // build request
  BuildIndxReq* const req = (BuildIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(BuildIndxReq::RT_TRIX);
  req->setBuildId(0);   // not yet..
  req->setBuildKey(0);  // ..in use
  req->setIndexType(indexPtr.p->tableType);
  req->setIndexId(indexPtr.i);
  req->setTableId(indexPtr.p->primaryTableId);
  req->setParallelism(16);
  if (indexPtr.p->isHashIndex()) {
    jam();
    getIndexAttrList(indexPtr, opPtr.p->m_attrList);
    getTableKeyList(tablePtr, opPtr.p->m_tableKeyList);
    // send
    LinearSectionPtr lsPtr[3];
    lsPtr[0].sz = opPtr.p->m_attrList.sz;
    lsPtr[0].p = opPtr.p->m_attrList.id;
    lsPtr[1].sz = opPtr.p->m_tableKeyList.sz;
    lsPtr[1].p = opPtr.p->m_tableKeyList.id;
    sendSignal(calcTrixBlockRef(getOwnNodeId()), GSN_BUILDINDXREQ,
        signal, BuildIndxReq::SignalLength, JBB, lsPtr, 2);
    return;
  }
  if (indexPtr.p->isOrderedIndex()) {
    jam();
    sendSignal(calcTupBlockRef(getOwnNodeId()), GSN_BUILDINDXREQ,
        signal, BuildIndxReq::SignalLength, JBB);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::buildIndex_toDropConstr(Signal* signal, OpBuildIndexPtr opPtr)
{
#if wl3600 // remove in index patch
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  // request to drop constraint trigger
  DropTrigReq* req = (DropTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(DropTrigReq::RT_BUILD_INDEX);
  req->addRequestFlag(0);       // none
  req->setTableId(indexPtr.i);
  req->setIndexId(RNIL);
  req->setTriggerId(opPtr.p->m_constrTriggerId);
  req->setTriggerInfo(0);       // not used
  sendSignal(reference(), GSN_DROP_TRIG_REQ,
      signal, DropTrigReq::SignalLength, JBB);
#endif
}

void
Dbdict::buildIndex_fromDropConstr(Signal* signal, OpBuildIndexPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = BuildIndxReq::RT_DICT_ABORT;
    buildIndex_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = BuildIndxReq::RT_DICT_TC;
  buildIndex_sendSlaveReq(signal, opPtr);
}

void
Dbdict::buildIndex_toOnline(Signal* signal, OpBuildIndexPtr opPtr)
{
  jam();
  TableRecordPtr indexPtr;
  c_tableRecordPool.getPtr(indexPtr, opPtr.p->m_request.getIndexId());
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  // request to set index online in TC or TUX
  AlterIndxReq* const req = (AlterIndxReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  if (opPtr.p->m_requestType == BuildIndxReq::RT_DICT_TC) {
    jam();
    req->setRequestType(AlterIndxReq::RT_TC);
  } else if (opPtr.p->m_requestType == BuildIndxReq::RT_DICT_TUX) {
    jam();
    req->setRequestType(AlterIndxReq::RT_TUX);
  } else {
    ndbrequire(false);
  }
  req->setTableId(tablePtr.i);
  req->setIndexId(indexPtr.i);
  req->setIndexVersion(indexPtr.p->tableVersion);
  req->setOnline(true);
  BlockReference blockRef = 0;
  if (opPtr.p->m_requestType == BuildIndxReq::RT_DICT_TC) {
    jam();
    blockRef = calcTcBlockRef(getOwnNodeId());
  } else if (opPtr.p->m_requestType == BuildIndxReq::RT_DICT_TUX) {
    jam();
    blockRef = calcTuxBlockRef(getOwnNodeId());
  } else {
    ndbrequire(false);
  }
  // send
  sendSignal(blockRef, GSN_ALTER_INDX_REQ,
      signal, BuildIndxReq::SignalLength, JBB);
}

void
Dbdict::buildIndex_fromOnline(Signal* signal, OpBuildIndexPtr opPtr)
{
  jam();
  // forward to master
  buildIndex_sendReply(signal, opPtr, false);
}

void
Dbdict::buildIndex_sendSlaveReq(Signal* signal, OpBuildIndexPtr opPtr)
{
  BuildIndxReq* const req = (BuildIndxReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  if(opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL) {
    jam();
    opPtr.p->m_signalCounter.clearWaitingFor();
    opPtr.p->m_signalCounter.setWaitingFor(getOwnNodeId());
    sendSignal(reference(), GSN_BUILDINDXREQ,
	       signal, BuildIndxReq::SignalLength, JBB);
  } else {
    jam();
    opPtr.p->m_signalCounter = c_aliveNodes;
    NodeReceiverGroup rg(DBDICT, c_aliveNodes);
    sendSignal(rg, GSN_BUILDINDXREQ,
	       signal, BuildIndxReq::SignalLength, JBB);
  }
}

void
Dbdict::buildIndex_sendReply(Signal* signal, OpBuildIndexPtr opPtr,
    bool toUser)
{
  BuildIndxRef* rep = (BuildIndxRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_BUILDINDXCONF;
  Uint32 length = BuildIndxConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == BuildIndxReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = BuildIndxConf::SignalLength;
  }
  rep->setIndexType(opPtr.p->m_request.getIndexType());
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  if (sendRef) {
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->masterNodeId = opPtr.p->m_errorNode;
    gsn = GSN_BUILDINDXREF;
    length = BuildIndxRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

// MODULE: CreateTrigger

const Dbdict::OpInfo
Dbdict::CreateTriggerRec::g_opInfo = {
  { 'C', 'T', 'r', 0 },
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
    impl_req->attributeMask = req->attributeMask;

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

  releaseSections(signal);

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
Dbdict::createTrigger_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("createTrigger_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch (requestType) {
  case CreateTrigImplReq::CreateTriggerOnline:
  case CreateTrigImplReq::CreateTriggerOffline:
    break;
  default:
    setError(error, CreateTrigRef::BadRequestType, __LINE__);
    return;
  }

  // save name
  {
    SegmentedSectionPtr ss_ptr;
    signal->getSection(ss_ptr, CreateTrigReq::TRIGGER_NAME_SECTION);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
    DictTabInfo::Table tableDesc;
    tableDesc.init();
    SimpleProperties::UnpackStatus status =
      SimpleProperties::unpack(
          r, &tableDesc,
          DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
          true, true);

    if (status != SimpleProperties::Eof ||
        tableDesc.TableName[0] == 0) {
      jam();
      setError(error, CreateTrigRef::InvalidName, __LINE__);
      return;
    }
    const Uint32 bytesize = sizeof(createTriggerPtr.p->m_triggerName);
    memcpy(createTriggerPtr.p->m_triggerName, tableDesc.TableName, bytesize);
    D("trigger " << createTriggerPtr.p->m_triggerName);
  }

  // check name is unique
  if (get_object(createTriggerPtr.p->m_triggerName) != 0) {
    jam();
    D("duplicate trigger name " << createTriggerPtr.p->m_triggerName);
    setError(error, CreateTrigRef::TriggerExists, __LINE__);
    return;
  }

  // check the table
  {
    const Uint32 tableId = impl_req->tableId;
    if (! (tableId < c_tableRecordPool.getSize())) {
      jam();
      setError(error, CreateTrigRef::InvalidTable, __LINE__);
      return;
    }
    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, tableId);
#if wl3600_todo // no PARSE state
    if (tablePtr.p->tabState != TableRecord::PARSE &&
        tablePtr.p->tabState != TableRecord::DEFINED &&
        tablePtr.p->tabState != TableRecord::BACKUP_ONGOING) {
      jam();
      setError(error, CreateTrigRef::InvalidTable, __LINE__);
      return;
    }
#endif
  }

  TriggerRecordPtr triggerPtr;
  if (trans_ptr.p->m_isMaster) {
    jam();
    if (impl_req->triggerId == RNIL) {
      jam();
      impl_req->triggerId = getFreeTriggerRecord();
      if (impl_req->triggerId == RNIL) {
        jam();
        setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
        return;
      }
      c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);
      ndbrequire(triggerPtr.p->triggerState == TriggerRecord::TS_NOT_DEFINED);
      D("master allocated triggerId " << impl_req->triggerId);
    } else {
      if (!(impl_req->triggerId < c_triggerRecordPool.getSize())) {
        jam();
        setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
        return;
      }
      c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);
      if (triggerPtr.p->triggerState != TriggerRecord::TS_NOT_DEFINED) {
        jam();
        setError(error, CreateTrigRef::TriggerExists, __LINE__);
        return;
      }
      D("master forced triggerId " << impl_req->triggerId);
    }
  }
  else {
    jam();
    // slave receives trigger id from master
    if (! (impl_req->triggerId < c_triggerRecordPool.getSize())) {
      jam();
      setError(error, CreateTrigRef::TooManyTriggers, __LINE__);
      return;
    }
    c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);
    if (triggerPtr.p->triggerState != TriggerRecord::TS_NOT_DEFINED) {
      jam();
      setError(error, CreateTrigRef::TriggerExists, __LINE__);
      return;
    }
    D("slave allocated triggerId " << hex << impl_req->triggerId);
  }

  initialiseTriggerRecord(triggerPtr);

  triggerPtr.p->triggerId = impl_req->triggerId;
  triggerPtr.p->tableId = impl_req->tableId;
  triggerPtr.p->indexId = RNIL; // feedback method connects to index
  triggerPtr.p->triggerInfo = impl_req->triggerInfo;
  triggerPtr.p->receiverRef = impl_req->receiverRef;
  triggerPtr.p->attributeMask = impl_req->attributeMask;
  triggerPtr.p->triggerState = TriggerRecord::TS_DEFINING;
  {
    Rope name(c_rope_pool, triggerPtr.p->triggerName);
    if (!name.assign(createTriggerPtr.p->m_triggerName)) {
      jam();
      setError(error, CreateTrigRef::OutOfStringBuffer, __LINE__);
      return;
    }
  }

  // connect to new DictObject
  {
    Ptr<DictObject> obj_ptr;
    seizeDictObject(op_ptr, obj_ptr, triggerPtr.p->triggerName);

    obj_ptr.p->m_id = impl_req->triggerId; // wl3600_todo id
    obj_ptr.p->m_type = TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
    triggerPtr.p->m_obj_ptr_i = obj_ptr.i;
  }
}

bool
Dbdict::createTrigger_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_subOps" << V(op_ptr.i) << *op_ptr.p);

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);

  // op to alter trigger online  wl3600_todo missing check for online
  if (!createTriggerPtr.p->m_sub_alter_trigger) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::createTrigger_fromAlterTrigger),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    createTrigger_toAlterTrigger(signal, op_ptr);
    return true;
  }
  return false;
}

void
Dbdict::createTrigger_toAlterTrigger(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_toAlterTrigger");

  AlterTrigReq* req = (AlterTrigReq*)signal->getDataPtrSend();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, AlterTrigReq::AlterTriggerOnline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->triggerId = impl_req->triggerId;

  sendSignal(reference(), GSN_ALTER_TRIG_REQ, signal,
             AlterTrigReq::SignalLength, JBB);
}

void
Dbdict::createTrigger_fromAlterTrigger(Signal* signal, Uint32 op_key, Uint32 ret)
{
  D("createTrigger_fromAlterTrigger");

  SchemaOpPtr op_ptr;
  CreateTriggerRecPtr createTriggerPtr;

  findSchemaOp(op_ptr, createTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  if (ret == 0) {
    jam();
    const AlterTrigConf* conf =
      (const AlterTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    createTriggerPtr.p->m_sub_alter_trigger = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterTrigRef* ref =
      (const AlterTrigRef*)signal->getDataPtr();
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
  const CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);

  D("createTrigger_prepare");

  sendTransConf(signal, op_ptr);
}

// CreateTrigger: COMMIT

void
Dbdict::createTrigger_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();

  CreateTriggerRecPtr createTriggerPtr;
  getOpRec(op_ptr, createTriggerPtr);
  CreateTrigImplReq* impl_req = &createTriggerPtr.p->m_request;
  Uint32 triggerId = impl_req->triggerId;

  D("createTrigger_commit" << V(triggerId));

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  triggerPtr.p->triggerState = TriggerRecord::TS_OFFLINE;

  unlinkDictObject(op_ptr);
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

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);

  if (triggerPtr.p->triggerState == TriggerRecord::TS_DEFINING) {
    jam();
    triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;
  }

  // ignore Feedback for now (referencing object will be dropped too)

  if (hasDictObject(op_ptr)) {
    jam();
    releaseDictObject(op_ptr);
  }

  sendTransConf(signal, op_ptr);
}

void
Dbdict::createTrigger_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  D("createTrigger_abortPrepare" << *op_ptr.p);

  // wl3600_todo

  sendTransConf(signal, op_ptr);
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

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

  releaseSections(signal);

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
Dbdict::dropTrigger_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("dropTrigger_parse" << V(op_ptr.i) << *op_ptr.p);

  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  bool reqByName = false;
  if (signal->getNoOfSections() > 0) { // wl3600_todo use requestType
    jam();
    ndbrequire(signal->getNoOfSections() == 1);
    reqByName = true;
  }

  if (reqByName) {
    jam();
    SegmentedSectionPtr ss_ptr;
    signal->getSection(ss_ptr, DropTrigImplReq::TRIGGER_NAME_SECTION);
    SimplePropertiesSectionReader r(ss_ptr, getSectionSegmentPool());
    DictTabInfo::Table tableDesc;
    tableDesc.init();
    SimpleProperties::UnpackStatus status =
      SimpleProperties::unpack(
          r, &tableDesc,
          DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
          true, true);

    if (status != SimpleProperties::Eof ||
        tableDesc.TableName[0] == 0) {
      jam();
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
    if (!(impl_req->triggerId < c_triggerRecordPool.getSize())) {
      jam();
      setError(error, DropTrigImplRef::TriggerNotFound, __LINE__);
      return;
    }
    c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);
    // wl3600_todo state check
  }
  
  D("trigger " << copyRope<MAX_TAB_NAME_SIZE>(triggerPtr.p->triggerName));

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
}

bool
Dbdict::dropTrigger_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTrigger_subOps" << V(op_ptr.i) << *op_ptr.p);

  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);

  // op to alter trigger offline
  if (!dropTriggerPtr.p->m_sub_alter_trigger) {
    jam();
    Callback c = {
      safe_cast(&Dbdict::dropTrigger_fromAlterTrigger),
      op_ptr.p->op_key
    };
    op_ptr.p->m_callback = c;
    dropTrigger_toAlterTrigger(signal, op_ptr);
    return true;
  }
  return false;
}

void
Dbdict::dropTrigger_toAlterTrigger(Signal* signal, SchemaOpPtr op_ptr)
{
  D("dropTrigger_toAlterTrigger");

  AlterTrigReq* req = (AlterTrigReq*)signal->getDataPtrSend();

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;

  Uint32 requestInfo = 0;
  DictSignal::setRequestType(requestInfo, AlterTrigReq::AlterTriggerOffline);
  DictSignal::addRequestFlagsGlobal(requestInfo, op_ptr.p->m_requestInfo);

  req->clientRef = reference();
  req->clientData = op_ptr.p->op_key;
  req->transId = trans_ptr.p->m_transId;
  req->transKey = trans_ptr.p->trans_key;
  req->requestInfo = requestInfo;
  req->tableId = impl_req->tableId;
  req->tableVersion = impl_req->tableVersion;
  req->triggerId = impl_req->triggerId;

  sendSignal(reference(), GSN_ALTER_TRIG_REQ, signal,
             AlterTrigReq::SignalLength, JBB);
}

void
Dbdict::dropTrigger_fromAlterTrigger(Signal* signal, Uint32 op_key, Uint32 ret)
{
  D("dropTrigger_fromAlterTrigger");

  SchemaOpPtr op_ptr;
  DropTriggerRecPtr dropTriggerPtr;

  findSchemaOp(op_ptr, dropTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  Uint32 errorCode = 0;
  if (ret == 0) {
    jam();
    const AlterTrigConf* conf =
      (const AlterTrigConf*)signal->getDataPtr();

    ndbrequire(conf->transId == trans_ptr.p->m_transId);
    dropTriggerPtr.p->m_sub_alter_trigger = true;
    createSubOps(signal, op_ptr);
  } else {
    jam();
    const AlterTrigRef* ref =
      (const AlterTrigRef*)signal->getDataPtr();

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

  sendTransConf(signal, op_ptr);
}

// DropTrigger: COMMIT

void
Dbdict::dropTrigger_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;
  Uint32 triggerId = impl_req->triggerId;

  D("dropTrigger_commit" << V(triggerId));

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);

  // remove trigger
  releaseDictObject(op_ptr);
  triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;

  sendTransConf(signal, op_ptr);
}

// DropTrigger: ABORT

void
Dbdict::dropTrigger_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  DropTriggerRecPtr dropTriggerPtr;
  getOpRec(op_ptr, dropTriggerPtr);
  const DropTrigImplReq* impl_req = &dropTriggerPtr.p->m_request;
  const Uint32 triggerId = impl_req->triggerId;

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
  ndbrequire(false);
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

// MODULE: AlterTrigger

const Dbdict::OpInfo
Dbdict::AlterTriggerRec::g_opInfo = {
  { 'A', 'T', 'r', 0 },
  GSN_ALTER_TRIG_IMPL_REQ,
  AlterTrigImplReq::SignalLength,
  //
  &Dbdict::alterTrigger_seize,
  &Dbdict::alterTrigger_release,
  //
  &Dbdict::alterTrigger_parse,
  &Dbdict::alterTrigger_subOps,
  &Dbdict::alterTrigger_reply,
  //
  &Dbdict::alterTrigger_prepare,
  &Dbdict::alterTrigger_commit,
  //
  &Dbdict::alterTrigger_abortParse,
  &Dbdict::alterTrigger_abortPrepare
};

bool
Dbdict::alterTrigger_seize(SchemaOpPtr op_ptr)
{
  return seizeOpRec<AlterTriggerRec>(op_ptr);
}

void
Dbdict::alterTrigger_release(SchemaOpPtr op_ptr)
{
  releaseOpRec<AlterTriggerRec>(op_ptr);
}

void
Dbdict::execALTER_TRIG_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  const AlterTrigReq req_copy =
    *(const AlterTrigReq*)signal->getDataPtr();
  const AlterTrigReq* req = &req_copy;

  ErrorInfo error;
  do {
    SchemaOpPtr op_ptr;
    AlterTriggerRecPtr alterTriggerPtr;
    AlterTrigImplReq* impl_req;

    startClientReq(op_ptr, alterTriggerPtr, req, impl_req, error);
    if (hasError(error)) {
      jam();
      break;
    }

    impl_req->tableId = req->tableId;
    impl_req->tableVersion = req->tableVersion;
    impl_req->triggerId = req->triggerId;

    handleClientReq(signal, op_ptr);
    return;
  } while (0);

  releaseSections(signal);

  AlterTrigRef* ref = (AlterTrigRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->clientData = req->clientData;
  ref->transId = req->transId;
  ref->tableId = req->tableId;
  ref->triggerId = req->triggerId;
  getError(error, ref);

  sendSignal(req->clientRef, GSN_ALTER_TRIG_REF, signal,
             AlterTrigRef::SignalLength, JBB);
}

// AlterTrigger: PARSE

void
Dbdict::alterTrigger_parse(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo& error)
{
  D("alterTrigger_parse" << V(op_ptr.i) << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  // check request type
  Uint32 requestType = impl_req->requestType;
  switch (requestType) {
  case AlterTrigImplReq::AlterTriggerOnline:
  case AlterTrigImplReq::AlterTriggerOffline:
    break;
  default:
    setError(error, AlterTrigRef::BadRequestType, __LINE__);
    return;
  }

  // wl3600_todo look up optional name

  // check the table wl3600_todo
  // check the index if any wl3600_todo

  // check the trigger
  TriggerRecordPtr triggerPtr;
  {
    const Uint32 triggerId = impl_req->triggerId;
    if (! (impl_req->triggerId < c_triggerRecordPool.getSize())) {
      jam();
      setError(error, AlterTrigRef::TriggerNotFound, __LINE__);
      return;
    }

    c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);

    if (triggerPtr.p->triggerState == TriggerRecord::TS_NOT_DEFINED) {
      jam();
      setError(error, AlterTrigRef::TriggerNotFound, __LINE__);
      return;
    }
  }

  // set local triggers wl3600_todo maybe use requestType
  {
    TriggerType::Value type =
      TriggerInfo::getTriggerType(triggerPtr.p->triggerInfo);
    if (type == TriggerType::SECONDARY_INDEX) {
      jam();
      alterTriggerPtr.p->m_triggerCount = 2;
      alterTriggerPtr.p->m_triggerBlock[0] = DBTC_REF;
      alterTriggerPtr.p->m_triggerBlock[1] = DBLQH_REF;
    } else {
      jam();
      alterTriggerPtr.p->m_triggerCount = 1;
      alterTriggerPtr.p->m_triggerBlock[0] = DBLQH_REF;
    }
  }

  // number of triggers to do (abort code may change it)
  alterTriggerPtr.p->m_triggerMax = alterTriggerPtr.p->m_triggerCount;
}

bool
Dbdict::alterTrigger_subOps(Signal* signal, SchemaOpPtr op_ptr)
{
  D("alterTrigger_subOps" << V(op_ptr.i) << *op_ptr.p);
  return false;
}

void
Dbdict::alterTrigger_reply(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  jam();

  SchemaTransPtr& trans_ptr = op_ptr.p->m_trans_ptr;
  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  D("alterTrigger_reply");

  if (!hasError(error)) {
    AlterTrigConf* conf = (AlterTrigConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->clientData = op_ptr.p->m_clientData;
    conf->transId = trans_ptr.p->m_transId;
    conf->tableId = impl_req->tableId;
    conf->triggerId = impl_req->triggerId;

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_TRIG_CONF, signal,
               AlterTrigConf::SignalLength, JBB);
  } else {
    jam();
    AlterTrigRef* ref = (AlterTrigRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->clientData = op_ptr.p->m_clientData;
    ref->transId = trans_ptr.p->m_transId;
    ref->tableId = impl_req->tableId;
    ref->triggerId = impl_req->triggerId;
    getError(error, ref);

    Uint32 clientRef = op_ptr.p->m_clientRef;
    sendSignal(clientRef, GSN_ALTER_TRIG_REF, signal,
               AlterTrigRef::SignalLength, JBB);
  }
}

// AlterTrigger: PREPARE

void
Dbdict::alterTrigger_prepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  D("alterTrigger_prepare" << V(itRepeat) << *op_ptr.p);

  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  // do all TC's first (or LQH's if drop)
  ndbrequire(alterTriggerPtr.p->m_triggerNo == itRepeat);

  Callback c = {
    safe_cast(&Dbdict::alterTrigger_fromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  switch (requestType) {
  case AlterTrigImplReq::AlterTriggerOnline:
    alterTrigger_toCreateLocal(signal, op_ptr);
    break;
  case AlterTrigImplReq::AlterTriggerOffline:
    alterTrigger_toDropLocal(signal, op_ptr);
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::alterTrigger_toCreateLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);

  const Uint32 triggerCount = alterTriggerPtr.p->m_triggerCount;
  const Uint32 triggerMax = alterTriggerPtr.p->m_triggerMax;
  Uint32 triggerNo = alterTriggerPtr.p->m_triggerNo;
  ndbrequire(triggerNo < triggerMax && triggerMax <= triggerCount);

  D("alterTrigger_toCreateLocal" << V(triggerNo) << *op_ptr.p);

  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = impl_req->requestType;
  req->tableId = triggerPtr.p->tableId;
  req->tableVersion = 0; // not used
  req->indexId = triggerPtr.p->indexId;
  req->indexVersion = 0; // not used
  req->triggerNo = triggerNo; // not used
  req->triggerId = triggerPtr.p->triggerId;
  req->triggerInfo = triggerPtr.p->triggerInfo;
  req->receiverRef = triggerPtr.p->receiverRef;
  req->attributeMask = triggerPtr.p->attributeMask;

  const Uint32 i = triggerNo;
  BlockReference ref = alterTriggerPtr.p->m_triggerBlock[i];
  sendSignal(ref, GSN_CREATE_TRIG_IMPL_REQ, signal,
             CreateTrigImplReq::SignalLength, JBB);
}

void
Dbdict::alterTrigger_toDropLocal(Signal* signal, SchemaOpPtr op_ptr)
{
  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);

  const Uint32 triggerCount = alterTriggerPtr.p->m_triggerCount;
  const Uint32 triggerMax = alterTriggerPtr.p->m_triggerMax;
  Uint32 triggerNo = alterTriggerPtr.p->m_triggerNo;
  ndbrequire(triggerNo < triggerMax && triggerMax <= triggerCount);

  D("alterTrigger_toDropLocal" << V(triggerNo) << *op_ptr.p);

  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();

  req->senderRef = reference();
  req->senderData = op_ptr.p->op_key;
  req->requestType = impl_req->requestType;
  req->tableId = triggerPtr.p->tableId;
  req->tableVersion = 0; // not used
  req->indexId = triggerPtr.p->indexId;
  req->indexVersion = 0; // not used
  req->triggerNo = triggerNo; // not used
  req->triggerId = triggerPtr.p->triggerId;
  req->triggerInfo = triggerPtr.p->triggerInfo;

  const Uint32 i = triggerMax - (triggerNo + 1);
  BlockReference ref = alterTriggerPtr.p->m_triggerBlock[i];
  sendSignal(ref, GSN_DROP_TRIG_IMPL_REQ, signal,
             DropTrigImplReq::SignalLength, JBB);
}

void
Dbdict::alterTrigger_fromLocal(Signal* signal, Uint32 op_key, Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterTriggerRecPtr alterTriggerPtr;
  findSchemaOp(op_ptr, alterTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  const Uint32 triggerMax = alterTriggerPtr.p->m_triggerMax;
  Uint32 triggerNo = alterTriggerPtr.p->m_triggerNo;
  ndbrequire(triggerNo < triggerMax);
  D("alterTrigger_fromLocal" << V(triggerNo) << V(triggerMax));

  if (ret == 0) {
    jam();
    triggerNo = ++alterTriggerPtr.p->m_triggerNo;
    Uint32 itFlags = 0;
    if (triggerNo < triggerMax)
      itFlags |= SchemaTransImplConf::IT_REPEAT;

    sendTransConf(signal, op_ptr, itFlags);
  } else {
    jam();
    setError(op_ptr, ret, __LINE__);
    sendTransRef(signal, op_ptr);
  }
}

// AlterTrigger: COMMIT

void
Dbdict::alterTrigger_commit(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("alterTrigger_commit");

  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, impl_req->triggerId);

  switch (requestType) {
  case AlterTrigImplReq::AlterTriggerOnline:
    if (triggerPtr.p->triggerState == TriggerRecord::TS_OFFLINE)
      triggerPtr.p->triggerState = TriggerRecord::TS_ONLINE;
    break;
  case AlterTrigImplReq::AlterTriggerOffline:
    if (triggerPtr.p->triggerState == TriggerRecord::TS_ONLINE)
      triggerPtr.p->triggerState = TriggerRecord::TS_OFFLINE;
    break;
  default:
    ndbrequire(false);
    break;
  }

  sendTransConf(signal, op_ptr);
}

// AlterTrigger: ABORT

void
Dbdict::alterTrigger_abortParse(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  D("alterTrigger_abortParse" << *op_ptr.p);

  // wl3600_todo probably nothing
  sendTransConf(signal, op_ptr);
}

void
Dbdict::alterTrigger_abortPrepare(Signal* signal, SchemaOpPtr op_ptr)
{
  jam();
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  D("alterTrigger_abortPrepare" << V(itRepeat) << *op_ptr.p);

  AlterTriggerRecPtr alterTriggerPtr;
  getOpRec(op_ptr, alterTriggerPtr);
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;
  Uint32 requestType = impl_req->requestType;

  // set trigger counters on first round
  if (itRepeat == 0) {
    jam();
    alterTriggerPtr.p->m_triggerMax = alterTriggerPtr.p->m_triggerNo;
    alterTriggerPtr.p->m_triggerNo = 0;

    if (alterTriggerPtr.p->m_triggerMax == 0) {
      jam();
      // no triggers were done
      sendTransConf(signal, op_ptr);
      return;
    }
  }

  Callback c = {
    safe_cast(&Dbdict::alterTrigger_abortFromLocal),
    op_ptr.p->op_key
  };
  op_ptr.p->m_callback = c;

  // wl3600_todo
  switch (requestType) {
  case AlterTrigImplReq::AlterTriggerOnline:
    jam();
    alterTrigger_toDropLocal(signal, op_ptr);
    break;
  case AlterTrigImplReq::AlterTriggerOffline:
    jam();
    alterTrigger_toCreateLocal(signal, op_ptr);
    break;
  default:
    ndbrequire(false);
    break;
  }
}

void
Dbdict::alterTrigger_abortFromLocal(Signal* signal,
                                    Uint32 op_key,
                                    Uint32 ret)
{
  SchemaOpPtr op_ptr;
  AlterTriggerRecPtr alterTriggerPtr;
  findSchemaOp(op_ptr, alterTriggerPtr, op_key);
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const AlterTrigImplReq* impl_req = &alterTriggerPtr.p->m_request;

  const Uint32 triggerMax = alterTriggerPtr.p->m_triggerMax;
  Uint32 triggerNo = alterTriggerPtr.p->m_triggerNo;
  ndbrequire(triggerNo < triggerMax);
  D("alterTrigger_abortFromLocal" << V(triggerNo) << V(triggerMax));

  if (ret == 0) {
    jam();
    triggerNo = ++alterTriggerPtr.p->m_triggerNo;
    ndbrequire(triggerNo <= triggerMax);
    Uint32 itFlags = 0;
    if (triggerNo < triggerMax)
      itFlags |= SchemaTransImplConf::IT_REPEAT;

    sendTransConf(signal, op_ptr, itFlags);
  } else {
    // abort is not allowed to fail
    ndbrequire(false);
  }
}

// AlterTrigger: MISC

void
Dbdict::execALTER_TRIG_CONF(Signal* signal)
{
  jamEntry();
  const AlterTrigConf* conf = (const AlterTrigConf*)signal->getDataPtr();
  handleDictConf(signal, conf);
}

void
Dbdict::execALTER_TRIG_REF(Signal* signal)
{
  jamEntry();
  const AlterTrigRef* ref = (const AlterTrigRef*)signal->getDataPtr();
  handleDictRef(signal, ref);
}

// AlterTrigger: END

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
  LocalDLFifoList<AttributeRecord> alist(c_attributeRecordPool,
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

  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  AttributeRecord* iaRec = c_attributeRecordPool.getPtr(itAttr);
  {
    ConstRope tmp(c_rope_pool, iaRec->attributeName);
    tmp.copy(name);
    len = tmp.size();
  }
  LocalDLFifoList<AttributeRecord> alist(c_attributeRecordPool, 
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
Dbdict::getIndexAttrList(TableRecordPtr indexPtr, AttributeList& list)
{
  jam();
  list.sz = 0;
  memset(list.id, 0, sizeof(list.id));
  ndbrequire(indexPtr.p->noOfAttributes >= 2);

  LocalDLFifoList<AttributeRecord> alist(c_attributeRecordPool,
                                         indexPtr.p->m_attributes);
  AttributeRecordPtr attrPtr;
  for (alist.first(attrPtr); !attrPtr.isNull(); alist.next(attrPtr)) {
    // skip last
    AttributeRecordPtr tempPtr = attrPtr;
    if (! alist.next(tempPtr))
      break;
    getIndexAttr(indexPtr, attrPtr.i, &list.id[list.sz++]);
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
  LocalDLFifoList<AttributeRecord> alist(c_attributeRecordPool, 
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
    ,{ DictLockReq::CreateIndexLock, "CreateIndex" }
    ,{ DictLockReq::DropIndexLock,   "DropIndex" }
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
  const DictLockReq* req = (const DictLockReq*)&signal->theData[0];

  UtilLockReq lockReq;
  lockReq.senderRef = req->userRef;
  lockReq.senderData = req->userPtr;
  lockReq.lockId = 0;
  lockReq.requestInfo = 0;
  lockReq.extra = req->lockType;

  const DictLockType* lt = getDictLockType(req->lockType);

  if (req->lockType == DictLockReq::NodeRestartLock)
  {
    jam();
    lockReq.requestInfo |= UtilLockReq::SharedLock;
  }

  // make sure bad request crashes slave, not master (us)
  Uint32 err, res;
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

  if (req->userRef != signal->getSendersBlockRef() ||
      getNodeInfo(refToNode(req->userRef)).m_type != NodeInfo::DB) 
  {
    jam();
    err = DictLockRef::BadUserRef;
    goto ref;
  }

  if (c_aliveNodes.get(refToNode(req->userRef))) 
  {
    jam();
    err = DictLockRef::TooLate;
    goto ref;
  }
  
  res = m_dict_lock.lock(m_dict_lock_pool, &lockReq, 0);
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
    sendDictLockInfoEvent(signal, &lockReq, "lock request by node");    
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
  
  UtilLockReq lockReq;
  lockReq.senderData = req.userPtr;
  lockReq.senderRef = req.userRef;
  lockReq.extra = DictLockReq::NodeRestartLock; // Should check...
  Uint32 res = dict_lock_unlock(signal, &req);
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

// NF handling

void
Dbdict::removeStaleDictLocks(Signal* signal, const Uint32* theFailedNodes)
{
  LockQueue::Iterator iter;
  if (m_dict_lock.first(m_dict_lock_pool, iter))
  {
    do {
      if (NodeBitmask::get(theFailedNodes, 
                           refToNode(iter.m_curr.p->m_req.senderRef)))
      {
        if (iter.m_curr.p->m_req.requestInfo & UtilLockReq::Granted)
        {
          jam();
          sendDictLockInfoEvent(signal, &iter.m_curr.p->m_req, 
                                "remove lock by failed node");
        } 
        else 
        {
          jam();
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

  Uint32 res = m_dict_lock.lock(m_dict_lock_pool, &req, &lockOwner);
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
  
  return SchemaTransBeginRef::Busy;
}

Uint32
Dbdict::dict_lock_unlock(Signal* signal, const DictLockReq* _req)
{
  UtilUnlockReq req;
  req.senderData = _req->userPtr;
  req.senderRef = _req->userRef;
  
  Uint32 res = m_dict_lock.unlock(m_dict_lock_pool, &req);
  switch(res){
  case UtilUnlockRef::OK:
  case UtilUnlockRef::NotLockOwner:
    break;
  case UtilUnlockRef::NotInLockQueue:
    ndbassert(false);
    return res;
  }

  UtilLockReq lockReq;
  LockQueue::Iterator iter;
  if (m_dict_lock.first(m_dict_lock_pool, iter))
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
      }        
      
      if (!m_dict_lock.next(iter))
        break;
    }
  }

  return res;
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
      if (te->m_tableState != SchemaFile::INIT &&
          te->m_tableState != SchemaFile::DROP_TABLE_COMMITTED) {
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
Dbdict::getTableEntry(XSchemaFile * xsf, Uint32 tableId)
{
  Uint32 n = tableId / NDB_SF_PAGE_ENTRIES;
  Uint32 i = tableId % NDB_SF_PAGE_ENTRIES;
  ndbrequire(n < xsf->noOfPages);

  SchemaFile * sf = &xsf->schemaPage[n];
  return &sf->TableEntries[i];
}

//******************************************
void
Dbdict::execCREATE_FILE_REQ(Signal* signal)
{
  jamEntry();
  
  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  CreateFileReq * req = (CreateFileReq*)signal->getDataPtr();
  CreateFileRef * ref = (CreateFileRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 type = req->objType;
  Uint32 requestInfo = req->requestInfo;
  
  do {
    if(getOwnNodeId() != c_masterNodeId){
      jam();
      ref->errorCode = CreateFileRef::NotMaster;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }
    
    if (checkSingleUserMode(senderRef))
    {
      ref->errorCode = CreateFileRef::SingleUser;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    Ptr<SchemaTransaction> trans_ptr;
    if (! c_Trans.seize(trans_ptr)){
      jam();
      ref->errorCode = CreateFileRef::Busy;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    DictLockReq lockReq;
    lockReq.userPtr = trans_ptr.i;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::CreateFileLock;
    if ((ref->errorCode = dict_lock_trylock(&lockReq)))
    {
      jam();
      ref->errorLine = __LINE__;      
      break;
    }

    jam(); 
    const Uint32 trans_key = ++c_opRecordSequence;
    trans_ptr.p->key = trans_key;
    trans_ptr.p->m_senderRef = senderRef;
    trans_ptr.p->m_senderData = senderData;
    trans_ptr.p->m_nodes = c_aliveNodes;
    trans_ptr.p->m_errorCode = 0;
//    trans_ptr.p->m_nodes.clear(); 
//    trans_ptr.p->m_nodes.set(getOwnNodeId());
    c_Trans.add(trans_ptr);
    
    const Uint32 op_key = ++c_opRecordSequence;
    trans_ptr.p->m_op.m_key = op_key;
    trans_ptr.p->m_op.m_vt_index = 1;
    trans_ptr.p->m_op.m_state = DictObjOp::Preparing;
    
    CreateObjReq* create_obj = (CreateObjReq*)signal->getDataPtrSend();
    create_obj->op_key = op_key;
    create_obj->senderRef = reference();
    create_obj->senderData = trans_key;
    create_obj->clientRef = senderRef;
    create_obj->clientData = senderData;
    
    create_obj->objType = type;
    create_obj->requestInfo = requestInfo;

    {
      Uint32 objId = getFreeObjId(0);
      if (objId == RNIL) {
        jam();
        ref->errorCode = CreateFileRef::NoMoreObjectRecords;
        ref->status    = 0;
        ref->errorKey  = 0;
        ref->errorLine = __LINE__;

        dict_lock_unlock(0, &lockReq);
        break;
      }
      
      create_obj->objId = objId;
      trans_ptr.p->m_op.m_obj_id = objId;
      create_obj->gci = 0;
      
      XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
      SchemaFile::TableEntry *objEntry = getTableEntry(xsf, objId);      
      create_obj->objVersion = 
	create_obj_inc_schema_version(objEntry->m_tableVersion);
    }
    
    NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
    SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
    tmp.init<CreateObjRef>(rg, GSN_CREATE_OBJ_REF, trans_key);
    sendSignal(rg, GSN_CREATE_OBJ_REQ, signal, 
	       CreateObjReq::SignalLength, JBB);

    return;
  } while(0);
  
  ref->senderData = senderData;
  ref->masterNodeId = c_masterNodeId;
  sendSignal(senderRef, GSN_CREATE_FILE_REF,signal,
	     CreateFileRef::SignalLength, JBB);
}

void
Dbdict::execCREATE_FILEGROUP_REQ(Signal* signal)
{
  jamEntry();
  
  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  CreateFilegroupReq * req = (CreateFilegroupReq*)signal->getDataPtr();
  CreateFilegroupRef * ref = (CreateFilegroupRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 type = req->objType;
  
  do {
    if(getOwnNodeId() != c_masterNodeId){
      jam();
      ref->errorCode = CreateFilegroupRef::NotMaster;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }
    
    if (checkSingleUserMode(senderRef))
    {
      ref->errorCode = CreateFilegroupRef::SingleUser;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    Ptr<SchemaTransaction> trans_ptr;
    if (! c_Trans.seize(trans_ptr)){
      jam();
      ref->errorCode = CreateFilegroupRef::Busy;
      ref->status    = 0;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    DictLockReq lockReq;
    lockReq.userPtr = trans_ptr.i;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::CreateFilegroupLock;
    if ((ref->errorCode = dict_lock_trylock(&lockReq)))
    {
      jam();
      ref->errorLine = __LINE__;      
      break;
    }

    jam(); 
    const Uint32 trans_key = ++c_opRecordSequence;
    trans_ptr.p->key = trans_key;
    trans_ptr.p->m_senderRef = senderRef;
    trans_ptr.p->m_senderData = senderData;
    trans_ptr.p->m_nodes = c_aliveNodes;
    trans_ptr.p->m_errorCode = 0;
    c_Trans.add(trans_ptr);
    
    const Uint32 op_key = ++c_opRecordSequence;
    trans_ptr.p->m_op.m_key = op_key;
    trans_ptr.p->m_op.m_vt_index = 0;
    trans_ptr.p->m_op.m_state = DictObjOp::Preparing;
    
    CreateObjReq* create_obj = (CreateObjReq*)signal->getDataPtrSend();
    create_obj->op_key = op_key;
    create_obj->senderRef = reference();
    create_obj->senderData = trans_key;
    create_obj->clientRef = senderRef;
    create_obj->clientData = senderData;
    
    create_obj->objType = type;
    
    {
      Uint32 objId = getFreeObjId(0);
      if (objId == RNIL) {
        jam();
        ref->errorCode = CreateFilegroupRef::NoMoreObjectRecords;
        ref->status    = 0;
        ref->errorKey  = 0;
        ref->errorLine = __LINE__;

        dict_lock_unlock(0, &lockReq);
        break;
      }
      
      create_obj->objId = objId;
      trans_ptr.p->m_op.m_obj_id = objId;
      create_obj->gci = 0;
      
      XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
      SchemaFile::TableEntry *objEntry = getTableEntry(xsf, objId);      
      create_obj->objVersion = 
	create_obj_inc_schema_version(objEntry->m_tableVersion);
    }
    
    NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
    SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
    tmp.init<CreateObjRef>(rg, GSN_CREATE_OBJ_REF, trans_key);
    sendSignal(rg, GSN_CREATE_OBJ_REQ, signal, 
	       CreateObjReq::SignalLength, JBB);

    return;
  } while(0);
  
  ref->senderData = senderData;
  ref->masterNodeId = c_masterNodeId;
  sendSignal(senderRef, GSN_CREATE_FILEGROUP_REF,signal,
	     CreateFilegroupRef::SignalLength, JBB);
}

void
Dbdict::execDROP_FILE_REQ(Signal* signal)
{
  jamEntry();
  
  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  DropFileReq * req = (DropFileReq*)signal->getDataPtr();
  DropFileRef * ref = (DropFileRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 objId = req->file_id;
  Uint32 version = req->file_version;
  
  do {
    if(getOwnNodeId() != c_masterNodeId){
      jam();
      ref->errorCode = DropFileRef::NotMaster;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }
    
    if (checkSingleUserMode(senderRef))
    {
      jam();
      ref->errorCode = DropFileRef::SingleUser;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    Ptr<File> file_ptr;
    if (!c_file_hash.find(file_ptr, objId))
    {
      jam();
      ref->errorCode = DropFileRef::NoSuchFile;
      ref->errorLine = __LINE__;
      break;
    }
    
    if (file_ptr.p->m_version != version)
    {
      jam();
      ref->errorCode = DropFileRef::InvalidSchemaObjectVersion;
      ref->errorLine = __LINE__;
      break;
    }

    Ptr<SchemaTransaction> trans_ptr;
    if (! c_Trans.seize(trans_ptr))
    {
      jam();
      ref->errorCode = DropFileRef::Busy;
      ref->errorLine = __LINE__;
      break;
    }

    DictLockReq lockReq;
    lockReq.userPtr = trans_ptr.i;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::DropFileLock;
    if ((ref->errorCode = dict_lock_trylock(&lockReq)))
    {
      jam();
      ref->errorLine = __LINE__;      
      break;
    }

    jam();
    
    const Uint32 trans_key = ++c_opRecordSequence;
    trans_ptr.p->key = trans_key;
    trans_ptr.p->m_senderRef = senderRef;
    trans_ptr.p->m_senderData = senderData;
    trans_ptr.p->m_nodes = c_aliveNodes;
    trans_ptr.p->m_errorCode = 0;
    c_Trans.add(trans_ptr);
    
    const Uint32 op_key = ++c_opRecordSequence;
    trans_ptr.p->m_op.m_key = op_key;
    trans_ptr.p->m_op.m_vt_index = 2;
    trans_ptr.p->m_op.m_state = DictObjOp::Preparing;
    
    DropObjReq* drop_obj = (DropObjReq*)signal->getDataPtrSend();
    drop_obj->op_key = op_key;
    drop_obj->objVersion = version;
    drop_obj->objId = objId;
    drop_obj->objType = file_ptr.p->m_type;
    trans_ptr.p->m_op.m_obj_id = objId;

    drop_obj->senderRef = reference();
    drop_obj->senderData = trans_key;
    drop_obj->clientRef = senderRef;
    drop_obj->clientData = senderData;

    drop_obj->requestInfo = 0;
    
    NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
    SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
    tmp.init<CreateObjRef>(rg, GSN_DROP_OBJ_REF, trans_key);
    sendSignal(rg, GSN_DROP_OBJ_REQ, signal, 
	       DropObjReq::SignalLength, JBB);

    return;
  } while(0);
  
  ref->senderData = senderData;
  ref->masterNodeId = c_masterNodeId;
  sendSignal(senderRef, GSN_DROP_FILE_REF,signal,
	     DropFileRef::SignalLength, JBB);
}

void
Dbdict::execDROP_FILEGROUP_REQ(Signal* signal)
{
  jamEntry();
  
  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  DropFilegroupReq * req = (DropFilegroupReq*)signal->getDataPtr();
  DropFilegroupRef * ref = (DropFilegroupRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 objId = req->filegroup_id;
  Uint32 version = req->filegroup_version;
  
  do {
    if(getOwnNodeId() != c_masterNodeId)
    {
      jam();
      ref->errorCode = DropFilegroupRef::NotMaster;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }
    
    if (checkSingleUserMode(senderRef))
    {
      jam();
      ref->errorCode = DropFilegroupRef::SingleUser;
      ref->errorKey  = 0;
      ref->errorLine = __LINE__;
      break;
    }

    Ptr<Filegroup> filegroup_ptr;
    if (!c_filegroup_hash.find(filegroup_ptr, objId))
    {
      jam();
      ref->errorCode = DropFilegroupRef::NoSuchFilegroup;
      ref->errorLine = __LINE__;
      break;
    }

    if (filegroup_ptr.p->m_version != version)
    {
      jam();
      ref->errorCode = DropFilegroupRef::InvalidSchemaObjectVersion;
      ref->errorLine = __LINE__;
      break;
    }
    
    Ptr<SchemaTransaction> trans_ptr;
    if (! c_Trans.seize(trans_ptr))
    {
      jam();
      ref->errorCode = DropFilegroupRef::Busy;
      ref->errorLine = __LINE__;
      break;
    }

    DictLockReq lockReq;
    lockReq.userPtr = trans_ptr.i;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::DropFilegroupLock;
    if ((ref->errorCode = dict_lock_trylock(&lockReq)))
    {
      jam();
      ref->errorLine = __LINE__;      
      break;
    }

    jam();
    
    const Uint32 trans_key = ++c_opRecordSequence;
    trans_ptr.p->key = trans_key;
    trans_ptr.p->m_senderRef = senderRef;
    trans_ptr.p->m_senderData = senderData;
    trans_ptr.p->m_nodes = c_aliveNodes;
    trans_ptr.p->m_errorCode = 0;
    c_Trans.add(trans_ptr);
    
    const Uint32 op_key = ++c_opRecordSequence;
    trans_ptr.p->m_op.m_key = op_key;
    trans_ptr.p->m_op.m_vt_index = 3;
    trans_ptr.p->m_op.m_state = DictObjOp::Preparing;
    
    DropObjReq* drop_obj = (DropObjReq*)signal->getDataPtrSend();
    drop_obj->op_key = op_key;
    drop_obj->objVersion = version;
    drop_obj->objId = objId;
    drop_obj->objType = filegroup_ptr.p->m_type;
    trans_ptr.p->m_op.m_obj_id = objId;
    
    drop_obj->senderRef = reference();
    drop_obj->senderData = trans_key;
    drop_obj->clientRef = senderRef;
    drop_obj->clientData = senderData;
    
    drop_obj->requestInfo = 0;
    
    NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
    SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
    tmp.init<CreateObjRef>(rg, GSN_DROP_OBJ_REF, trans_key);
    sendSignal(rg, GSN_DROP_OBJ_REQ, signal, 
	       DropObjReq::SignalLength, JBB);

    return;
  } while(0);
  
  ref->senderData = senderData;
  ref->masterNodeId = c_masterNodeId;
  sendSignal(senderRef, GSN_DROP_FILEGROUP_REF,signal,
	     DropFilegroupRef::SignalLength, JBB);
}

void
Dbdict::execCREATE_OBJ_REF(Signal* signal)
{
  CreateObjRef * const ref = (CreateObjRef*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, ref->senderData));
  if(ref->errorCode != CreateObjRef::NF_FakeErrorREF){
    jam();
    trans_ptr.p->setErrorCode(ref->errorCode);
  }
  Uint32 node = refToNode(ref->senderRef);
  schemaOperation_reply(signal, trans_ptr.p, node);
}

void
Dbdict::execCREATE_OBJ_CONF(Signal* signal)
{
  Ptr<SchemaTransaction> trans_ptr;
  CreateObjConf * const conf = (CreateObjConf*)signal->getDataPtr();

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, conf->senderData));
  schemaOperation_reply(signal, trans_ptr.p, refToNode(conf->senderRef));
}

void
Dbdict::schemaOperation_reply(Signal* signal,
		              SchemaTransaction * trans_ptr_p,
		              Uint32 nodeId)
{
  jam();
  {
    SafeCounter tmp(c_counterMgr, trans_ptr_p->m_counter);
    if(!tmp.clearWaitingFor(nodeId)){
      jam();
      return;
    }
  }
  
  switch(trans_ptr_p->m_op.m_state){
  case DictObjOp::Preparing:{
    if(trans_ptr_p->m_errorCode != 0)
    {
      /**
       * Failed to prepare on atleast one node -> abort on all
       */
      trans_ptr_p->m_op.m_state = DictObjOp::Aborting;
      trans_ptr_p->m_callback.m_callbackData = trans_ptr_p->key;
      trans_ptr_p->m_callback.m_callbackFunction= 
	safe_cast(&Dbdict::trans_abort_start_done);
      
      if(f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_abort_start)
      {
        jam();
	(this->*f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_abort_start)
	  (signal, trans_ptr_p);
      }
      else
      {
        jam();
	execute(signal, trans_ptr_p->m_callback, 0);
      }
      return;
    }
    
    trans_ptr_p->m_op.m_state = DictObjOp::Prepared;
    trans_ptr_p->m_callback.m_callbackData = trans_ptr_p->key;
    trans_ptr_p->m_callback.m_callbackFunction= 
      safe_cast(&Dbdict::trans_commit_start_done);
    
    if(f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_commit_start)
    {
      jam();
      (this->*f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_commit_start)
	(signal, trans_ptr_p);
    }
    else
    {
      jam();
      execute(signal, trans_ptr_p->m_callback, 0);
    }
    return;
  }
  case DictObjOp::Committing: {
    ndbrequire(trans_ptr_p->m_errorCode == 0);

    trans_ptr_p->m_op.m_state = DictObjOp::Committed;
    trans_ptr_p->m_callback.m_callbackData = trans_ptr_p->key;
    trans_ptr_p->m_callback.m_callbackFunction= 
      safe_cast(&Dbdict::trans_commit_complete_done);
    
    if(f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_commit_complete)
    {
      jam();
      (this->*f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_commit_complete)
	(signal, trans_ptr_p);
    }
    else
    {
      jam();
      execute(signal, trans_ptr_p->m_callback, 0);
    }
    return;
  }
  case DictObjOp::Aborting:{
    trans_ptr_p->m_op.m_state = DictObjOp::Committed;
    trans_ptr_p->m_callback.m_callbackData = trans_ptr_p->key;
    trans_ptr_p->m_callback.m_callbackFunction= 
      safe_cast(&Dbdict::trans_abort_complete_done);

    if(f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_abort_complete)
    {
      jam();
      (this->*f_dict_op[trans_ptr_p->m_op.m_vt_index].m_trans_abort_complete)
	(signal, trans_ptr_p);
    }
    else
    {
      jam();
      execute(signal, trans_ptr_p->m_callback, 0);
    }
    return;
  }
  case DictObjOp::Defined:
  case DictObjOp::Prepared:
  case DictObjOp::Committed:
  case DictObjOp::Aborted:
    jam();
    break;
  }
  ndbrequire(false);
}

void
Dbdict::trans_commit_start_done(Signal* signal, 
				Uint32 callbackData,
				Uint32 retValue)
{
  Ptr<SchemaTransaction> trans_ptr;

  jam();
  ndbrequire(retValue == 0);
  ndbrequire(c_Trans.find(trans_ptr, callbackData));
  NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
  SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
  tmp.init<DictCommitRef>(rg, GSN_DICT_COMMIT_REF, trans_ptr.p->key);
  
  DictCommitReq * const req = (DictCommitReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = trans_ptr.p->key;
  req->op_key = trans_ptr.p->m_op.m_key;
  sendSignal(rg, GSN_DICT_COMMIT_REQ, signal, DictCommitReq::SignalLength, 
	     JBB);
  trans_ptr.p->m_op.m_state = DictObjOp::Committing;
}

void
Dbdict::trans_commit_complete_done(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 retValue)
{
  Ptr<SchemaTransaction> trans_ptr;

  jam();
  ndbrequire(retValue == 0);
  ndbrequire(c_Trans.find(trans_ptr, callbackData));

  DictLockReq lockReq;
  lockReq.userRef = reference();
  lockReq.userPtr = trans_ptr.i;

  switch(f_dict_op[trans_ptr.p->m_op.m_vt_index].m_gsn_user_req){
  case GSN_CREATE_FILEGROUP_REQ:{
    FilegroupPtr fg_ptr;
    jam();
    ndbrequire(c_filegroup_hash.find(fg_ptr, trans_ptr.p->m_op.m_obj_id));
    
    CreateFilegroupConf * conf = (CreateFilegroupConf*)signal->getDataPtr();
    conf->senderRef = reference();
    conf->senderData = trans_ptr.p->m_senderData;
    conf->filegroupId = fg_ptr.p->key;
    conf->filegroupVersion = fg_ptr.p->m_version;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_CREATE_FILEGROUP_CONF, signal, 
	       CreateFilegroupConf::SignalLength, JBB);

    lockReq.lockType = DictLockReq::CreateFilegroupLock;
    break;
  }
  case GSN_CREATE_FILE_REQ:{
    FilePtr f_ptr;
    jam();
    ndbrequire(c_file_hash.find(f_ptr, trans_ptr.p->m_op.m_obj_id));
    CreateFileConf * conf = (CreateFileConf*)signal->getDataPtr();
    conf->senderRef = reference();
    conf->senderData = trans_ptr.p->m_senderData;
    conf->fileId = f_ptr.p->key;
    conf->fileVersion = f_ptr.p->m_version;

    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_CREATE_FILE_CONF, signal, 
	       CreateFileConf::SignalLength, JBB);

    lockReq.lockType = DictLockReq::CreateFileLock;
    break;
  }
  case GSN_DROP_FILE_REQ:{
    DropFileConf * conf = (DropFileConf*)signal->getDataPtr();
    jam();
    conf->senderRef = reference();
    conf->senderData = trans_ptr.p->m_senderData;
    conf->fileId = trans_ptr.p->m_op.m_obj_id;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_DROP_FILE_CONF, signal, 
	       DropFileConf::SignalLength, JBB);

    lockReq.lockType = DictLockReq::DropFileLock;
    break;
  }
  case GSN_DROP_FILEGROUP_REQ:{
    DropFilegroupConf * conf = (DropFilegroupConf*)signal->getDataPtr();
    jam();
    conf->senderRef = reference();
    conf->senderData = trans_ptr.p->m_senderData;
    conf->filegroupId = trans_ptr.p->m_op.m_obj_id;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_DROP_FILEGROUP_CONF, signal, 
	       DropFilegroupConf::SignalLength, JBB);

    lockReq.lockType = DictLockReq::DropFilegroupLock;
    break;
  }
  default:
    ndbrequire(false);
  }
  
  dict_lock_unlock(signal, &lockReq);
  c_Trans.release(trans_ptr);
  return;
}

void
Dbdict::trans_abort_start_done(Signal* signal, 
			       Uint32 callbackData,
			       Uint32 retValue)
{
  Ptr<SchemaTransaction> trans_ptr;

  jam();
  ndbrequire(retValue == 0);
  ndbrequire(c_Trans.find(trans_ptr, callbackData));
  
  NodeReceiverGroup rg(DBDICT, trans_ptr.p->m_nodes);
  SafeCounter tmp(c_counterMgr, trans_ptr.p->m_counter);
  ndbrequire(tmp.init<DictAbortRef>(rg, trans_ptr.p->key));
  
  DictAbortReq * const req = (DictAbortReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = trans_ptr.p->key;
  req->op_key = trans_ptr.p->m_op.m_key;
  
  sendSignal(rg, GSN_DICT_ABORT_REQ, signal, DictAbortReq::SignalLength, JBB);
}

void
Dbdict::trans_abort_complete_done(Signal* signal, 
				  Uint32 callbackData,
				  Uint32 retValue)
{
  Ptr<SchemaTransaction> trans_ptr;

  jam();
  ndbrequire(retValue == 0);
  ndbrequire(c_Trans.find(trans_ptr, callbackData));

  DictLockReq lockReq;
  lockReq.userRef = reference();
  lockReq.userPtr = trans_ptr.i;

  switch(f_dict_op[trans_ptr.p->m_op.m_vt_index].m_gsn_user_req){
  case GSN_CREATE_FILEGROUP_REQ:
  {
    //
    CreateFilegroupRef * ref = (CreateFilegroupRef*)signal->getDataPtr();
    jam();
    ref->senderRef = reference();
    ref->senderData = trans_ptr.p->m_senderData;
    ref->masterNodeId = c_masterNodeId;
    ref->errorCode = trans_ptr.p->m_errorCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->status = 0;

    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_CREATE_FILEGROUP_REF, signal, 
	       CreateFilegroupRef::SignalLength, JBB);

    lockReq.lockType = DictLockReq::CreateFilegroupLock;
    break;
  }
  case GSN_CREATE_FILE_REQ:
  {
    CreateFileRef * ref = (CreateFileRef*)signal->getDataPtr();
    jam();
    ref->senderRef = reference();
    ref->senderData = trans_ptr.p->m_senderData;
    ref->masterNodeId = c_masterNodeId;
    ref->errorCode = trans_ptr.p->m_errorCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->status = 0;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_CREATE_FILE_REF, signal, 
	       CreateFileRef::SignalLength, JBB);

    lockReq.lockType = DictLockReq::CreateFileLock;
    break;
  }
  case GSN_DROP_FILE_REQ:
  {
    DropFileRef * ref = (DropFileRef*)signal->getDataPtr();
    jam();
    ref->senderRef = reference();
    ref->senderData = trans_ptr.p->m_senderData;
    ref->masterNodeId = c_masterNodeId;
    ref->errorCode = trans_ptr.p->m_errorCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_DROP_FILE_REF, signal, 
	       DropFileRef::SignalLength, JBB);

    lockReq.lockType = DictLockReq::DropFileLock;
    break;
  }
  case GSN_DROP_FILEGROUP_REQ:
  {
    //
    DropFilegroupRef * ref = (DropFilegroupRef*)signal->getDataPtr();
    jam();
    ref->senderRef = reference();
    ref->senderData = trans_ptr.p->m_senderData;
    ref->masterNodeId = c_masterNodeId;
    ref->errorCode = trans_ptr.p->m_errorCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    
    //@todo check api failed
    sendSignal(trans_ptr.p->m_senderRef, GSN_DROP_FILEGROUP_REF, signal, 
	       DropFilegroupRef::SignalLength, JBB);

    lockReq.lockType = DictLockReq::DropFilegroupLock;
    break;
  }
  default:
    ndbrequire(false);
  }
  
  dict_lock_unlock(signal, &lockReq);
  c_Trans.release(trans_ptr);
  return;
}

void
Dbdict::execCREATE_OBJ_REQ(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  CreateObjReq * const req = (CreateObjReq*)signal->getDataPtr();
  const Uint32 gci = req->gci;
  const Uint32 objId = req->objId;
  const Uint32 objVersion = req->objVersion;
  const Uint32 objType = req->objType;
  const Uint32 requestInfo = req->requestInfo;

  SegmentedSectionPtr objInfoPtr;
  signal->getSection(objInfoPtr, CreateObjReq::DICT_OBJ_INFO);
  
  CreateObjRecordPtr createObjPtr;  
  ndbrequire(c_opCreateObj.seize(createObjPtr));
  
  const Uint32 key = req->op_key;
  createObjPtr.p->key = key;
  c_opCreateObj.add(createObjPtr);
  createObjPtr.p->m_errorCode = 0;
  createObjPtr.p->m_senderRef = req->senderRef;
  createObjPtr.p->m_senderData = req->senderData;
  createObjPtr.p->m_clientRef = req->clientRef;
  createObjPtr.p->m_clientData = req->clientData;
  
  createObjPtr.p->m_gci = gci;
  createObjPtr.p->m_obj_id = objId;
  createObjPtr.p->m_obj_type = objType;
  createObjPtr.p->m_obj_version = objVersion;
  createObjPtr.p->m_obj_info_ptr_i = objInfoPtr.i;
  createObjPtr.p->m_obj_ptr_i = RNIL;

  createObjPtr.p->m_callback.m_callbackData = key;
  createObjPtr.p->m_callback.m_callbackFunction= 
    safe_cast(&Dbdict::createObj_prepare_start_done);

  createObjPtr.p->m_restart= 0;
  switch(objType){
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
    jam();
    createObjPtr.p->m_vt_index = 0;
    break;
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    /**
     * Use restart code to impl. ForceCreateFile
     */
    if (requestInfo & CreateFileReq::ForceCreateFile)
    {
      jam();
      createObjPtr.p->m_restart= 2;
    }
    jam();
    createObjPtr.p->m_vt_index = 1;
    break;
  default:
    ndbrequire(false);
  }
  
  signal->header.m_noOfSections = 0;
  (this->*f_dict_op[createObjPtr.p->m_vt_index].m_prepare_start)
    (signal, createObjPtr.p);
}

void
Dbdict::execDICT_COMMIT_REQ(Signal* signal)
{
  DictCommitReq* req = (DictCommitReq*)signal->getDataPtr();
  Ptr<SchemaOperation> op;

  jamEntry();
  ndbrequire(c_schemaOperation.find(op, req->op_key));
  (this->*f_dict_op[op.p->m_vt_index].m_commit)(signal, op.p);
}

void
Dbdict::execDICT_ABORT_REQ(Signal* signal)
{
  DictAbortReq* req = (DictAbortReq*)signal->getDataPtr();
  Ptr<SchemaOperation> op;

  jamEntry();
  ndbrequire(c_schemaOperation.find(op, req->op_key));
  (this->*f_dict_op[op.p->m_vt_index].m_abort)(signal, op.p);
}

void
Dbdict::execDICT_COMMIT_REF(Signal* signal)
{
  DictCommitRef * const ref = (DictCommitRef*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, ref->senderData));
  if(ref->errorCode != DictCommitRef::NF_FakeErrorREF){
    jam();
    trans_ptr.p->setErrorCode(ref->errorCode);
  }
  Uint32 node = refToNode(ref->senderRef);
  schemaOperation_reply(signal, trans_ptr.p, node);
}

void
Dbdict::execDICT_COMMIT_CONF(Signal* signal)
{
  Ptr<SchemaTransaction> trans_ptr;
  DictCommitConf * const conf = (DictCommitConf*)signal->getDataPtr();

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, conf->senderData));
  schemaOperation_reply(signal, trans_ptr.p, refToNode(conf->senderRef));
}

void
Dbdict::execDICT_ABORT_REF(Signal* signal)
{
  DictAbortRef * const ref = (DictAbortRef*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, ref->senderData));
  if(ref->errorCode != DictAbortRef::NF_FakeErrorREF){
    jam();
    trans_ptr.p->setErrorCode(ref->errorCode);
  }
  Uint32 node = refToNode(ref->senderRef);
  schemaOperation_reply(signal, trans_ptr.p, node);
}

void
Dbdict::execDICT_ABORT_CONF(Signal* signal)
{
  DictAbortConf * const conf = (DictAbortConf*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, conf->senderData));
  schemaOperation_reply(signal, trans_ptr.p, refToNode(conf->senderRef));
}

void
Dbdict::createObj_prepare_start_done(Signal* signal,
				     Uint32 callbackData, 
				     Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;  
  SegmentedSectionPtr objInfoPtr;
  
  ndbrequire(returnCode == 0);
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  jam();
  getSection(objInfoPtr, createObjPtr.p->m_obj_info_ptr_i);
  if(createObjPtr.p->m_errorCode != 0){
    jam();
    createObjPtr.p->m_obj_info_ptr_i= RNIL;
    signal->setSection(objInfoPtr, 0);
    releaseSections(signal);
    createObj_prepare_complete_done(signal, callbackData, 0);
    return;
  }
  
  SchemaFile::TableEntry tabEntry;
  bzero(&tabEntry, sizeof(tabEntry));
  tabEntry.m_tableVersion = createObjPtr.p->m_obj_version;
  tabEntry.m_tableType    = createObjPtr.p->m_obj_type;
  tabEntry.m_tableState   = SchemaFile::ADD_STARTED;
  tabEntry.m_gcp          = createObjPtr.p->m_gci;
  tabEntry.m_info_words   = objInfoPtr.sz;
  
  Callback cb;
  cb.m_callbackData = createObjPtr.p->key;
  cb.m_callbackFunction = safe_cast(&Dbdict::createObj_writeSchemaConf1);
  
  updateSchemaState(signal, createObjPtr.p->m_obj_id, &tabEntry, &cb);
}

void
Dbdict::createObj_writeSchemaConf1(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;  
  Callback callback;
  SegmentedSectionPtr objInfoPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  
  callback.m_callbackData = createObjPtr.p->key;
  callback.m_callbackFunction = safe_cast(&Dbdict::createObj_writeObjConf);
  
  getSection(objInfoPtr, createObjPtr.p->m_obj_info_ptr_i);
  writeTableFile(signal, createObjPtr.p->m_obj_id, objInfoPtr, &callback);
  
  signal->setSection(objInfoPtr, 0);
  releaseSections(signal);
  createObjPtr.p->m_obj_info_ptr_i = RNIL;
}

void
Dbdict::createObj_writeObjConf(Signal* signal, 
			       Uint32 callbackData,
			       Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_prepare_complete_done);
  (this->*f_dict_op[createObjPtr.p->m_vt_index].m_prepare_complete)
    (signal, createObjPtr.p);
}

void
Dbdict::createObj_prepare_complete_done(Signal* signal, 
					Uint32 callbackData,
					Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  
  //@todo check for master failed
  
  if(createObjPtr.p->m_errorCode == 0){
    jam();
    
    CreateObjConf * const conf = (CreateObjConf*)signal->getDataPtr();
    conf->senderRef = reference();
    conf->senderData = createObjPtr.p->m_senderData;
    sendSignal(createObjPtr.p->m_senderRef, GSN_CREATE_OBJ_CONF,
	       signal, CreateObjConf::SignalLength, JBB);
    return;
  }
  
  CreateObjRef * const ref = (CreateObjRef*)signal->getDataPtr();
  ref->senderRef = reference();
  ref->senderData = createObjPtr.p->m_senderData;
  ref->errorCode = createObjPtr.p->m_errorCode;
  ref->errorLine = 0;
  ref->errorKey = 0;
  ref->errorStatus = 0;
  
  sendSignal(createObjPtr.p->m_senderRef, GSN_CREATE_OBJ_REF,
	     signal, CreateObjRef::SignalLength, JBB);
}

void
Dbdict::createObj_commit(Signal * signal, SchemaOperation * op)
{
  OpCreateObj * createObj = (OpCreateObj*)op;

  createObj->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_commit_start_done);
  if (f_dict_op[createObj->m_vt_index].m_commit_start)
  {
    jam();
    (this->*f_dict_op[createObj->m_vt_index].m_commit_start)(signal, createObj);
  }
  else
  {
    jam();
    execute(signal, createObj->m_callback, 0);
  }
}

void
Dbdict::createObj_commit_start_done(Signal* signal, 
				    Uint32 callbackData,
				    Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;  
  
  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  
  Uint32 objId = createObjPtr.p->m_obj_id;
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry objEntry = * getTableEntry(xsf, objId);
  objEntry.m_tableState = SchemaFile::TABLE_ADD_COMMITTED;
  
  Callback callback;
  callback.m_callbackData = createObjPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_writeSchemaConf2);
  
  updateSchemaState(signal, createObjPtr.p->m_obj_id, &objEntry, &callback);

}

void
Dbdict::createObj_writeSchemaConf2(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_commit_complete_done);
  if (f_dict_op[createObjPtr.p->m_vt_index].m_commit_complete)
  {
    jam();
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_commit_complete)
      (signal, createObjPtr.p);
  }
  else
  {
    jam();
    execute(signal, createObjPtr.p->m_callback, 0);
  }
  
}

void
Dbdict::createObj_commit_complete_done(Signal* signal, 
				       Uint32 callbackData,
				       Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  jam();
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  
  //@todo check error
  //@todo check master failed
  
  DictCommitConf * const conf = (DictCommitConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = createObjPtr.p->m_senderData;
  sendSignal(createObjPtr.p->m_senderRef, GSN_DICT_COMMIT_CONF,
	     signal, DictCommitConf::SignalLength, JBB);
  
  c_opCreateObj.release(createObjPtr);
}

void
Dbdict::createObj_abort(Signal* signal, SchemaOperation* op)
{
  OpCreateObj * createObj = (OpCreateObj*)op;
  
  createObj->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_abort_start_done);
  if (f_dict_op[createObj->m_vt_index].m_abort_start)
  {
    jam();
    (this->*f_dict_op[createObj->m_vt_index].m_abort_start)(signal, createObj);
  }
  else
  {
    jam();
    execute(signal, createObj->m_callback, 0);
  }
}

void
Dbdict::createObj_abort_start_done(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  jam();
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry objEntry = * getTableEntry(xsf, 
						    createObjPtr.p->m_obj_id);
  objEntry.m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
  
  Callback callback;
  callback.m_callbackData = createObjPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_abort_writeSchemaConf);
  
  updateSchemaState(signal, createObjPtr.p->m_obj_id, &objEntry, &callback);
}

void
Dbdict::createObj_abort_writeSchemaConf(Signal* signal, 
					Uint32 callbackData,
					Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));
  createObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createObj_abort_complete_done);
  
  if (f_dict_op[createObjPtr.p->m_vt_index].m_abort_complete)
  {
    jam();
    (this->*f_dict_op[createObjPtr.p->m_vt_index].m_abort_complete)
      (signal, createObjPtr.p);
  }
  else
  {
    jam();
    execute(signal, createObjPtr.p->m_callback, 0);
  }
}

void
Dbdict::createObj_abort_complete_done(Signal* signal, 
				      Uint32 callbackData,
				      Uint32 returnCode)
{
  CreateObjRecordPtr createObjPtr;

  jam();
  ndbrequire(c_opCreateObj.find(createObjPtr, callbackData));

  DictAbortConf * const conf = (DictAbortConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = createObjPtr.p->m_senderData;
  sendSignal(createObjPtr.p->m_senderRef, GSN_DICT_ABORT_CONF,
	     signal, DictAbortConf::SignalLength, JBB);
  
  c_opCreateObj.release(createObjPtr);
}

void
Dbdict::execDROP_OBJ_REQ(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  DropObjReq * const req = (DropObjReq*)signal->getDataPtr();

  const Uint32 objId = req->objId;
  const Uint32 objVersion = req->objVersion;
  const Uint32 objType = req->objType;
  
  DropObjRecordPtr dropObjPtr;  
  ndbrequire(c_opDropObj.seize(dropObjPtr));
  
  const Uint32 key = req->op_key;
  dropObjPtr.p->key = key;
  c_opDropObj.add(dropObjPtr);
  dropObjPtr.p->m_errorCode = 0;
  dropObjPtr.p->m_senderRef = req->senderRef;
  dropObjPtr.p->m_senderData = req->senderData;
  dropObjPtr.p->m_clientRef = req->clientRef;
  dropObjPtr.p->m_clientData = req->clientData;
  
  dropObjPtr.p->m_obj_id = objId;
  dropObjPtr.p->m_obj_type = objType;
  dropObjPtr.p->m_obj_version = objVersion;

  dropObjPtr.p->m_callback.m_callbackData = key;
  dropObjPtr.p->m_callback.m_callbackFunction= 
    safe_cast(&Dbdict::dropObj_prepare_start_done);

  switch(objType){
  case DictTabInfo::Tablespace:
  case DictTabInfo::LogfileGroup:
  {
    Ptr<Filegroup> fg_ptr;
    jam();
    dropObjPtr.p->m_vt_index = 3;
    ndbrequire(c_filegroup_hash.find(fg_ptr, objId));
    dropObjPtr.p->m_obj_ptr_i = fg_ptr.i;
    break;
  
  }
  case DictTabInfo::Datafile:
  {
    Ptr<File> file_ptr;
    jam();
    dropObjPtr.p->m_vt_index = 2;
    ndbrequire(c_file_hash.find(file_ptr, objId));
    dropObjPtr.p->m_obj_ptr_i = file_ptr.i;
    break;
  }
  case DictTabInfo::Undofile:
  {
    jam();
    dropObjPtr.p->m_vt_index = 4;
    return;
  }
  default:
    ndbrequire(false);
  }
  
  signal->header.m_noOfSections = 0;
  (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_start)
    (signal, dropObjPtr.p);
}

void
Dbdict::dropObj_prepare_start_done(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;
  Callback cb;

  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));

  cb.m_callbackData = callbackData;
  cb.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_prepare_writeSchemaConf);

  if(dropObjPtr.p->m_errorCode != 0)
  {
    jam();
    dropObj_prepare_complete_done(signal, callbackData, 0);
    return;
  }
  jam();  
  Uint32 objId = dropObjPtr.p->m_obj_id;
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry objEntry = *getTableEntry(xsf, objId);
  objEntry.m_tableState = SchemaFile::DROP_TABLE_STARTED;
  updateSchemaState(signal, objId, &objEntry, &cb);
}

void
Dbdict::dropObj_prepare_writeSchemaConf(Signal* signal, 
					Uint32 callbackData,
					Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_prepare_complete_done);
  if(f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_complete)
  {
    jam();
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_prepare_complete)
      (signal, dropObjPtr.p);
  }
  else
  {
    jam();
    execute(signal, dropObjPtr.p->m_callback, 0);
  }
}

void
Dbdict::dropObj_prepare_complete_done(Signal* signal, 
				      Uint32 callbackData,
				      Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  jam();
  
  //@todo check for master failed
  
  if(dropObjPtr.p->m_errorCode == 0){
    jam();
    
    DropObjConf * const conf = (DropObjConf*)signal->getDataPtr();
    conf->senderRef = reference();
    conf->senderData = dropObjPtr.p->m_senderData;
    sendSignal(dropObjPtr.p->m_senderRef, GSN_DROP_OBJ_CONF,
	       signal, DropObjConf::SignalLength, JBB);
    return;
  }
  
  DropObjRef * const ref = (DropObjRef*)signal->getDataPtr();
  ref->senderRef = reference();
  ref->senderData = dropObjPtr.p->m_senderData;
  ref->errorCode = dropObjPtr.p->m_errorCode;
  
  sendSignal(dropObjPtr.p->m_senderRef, GSN_DROP_OBJ_REF,
	     signal, DropObjRef::SignalLength, JBB);

}

void
Dbdict::dropObj_commit(Signal * signal, SchemaOperation * op)
{
  OpDropObj * dropObj = (OpDropObj*)op;

  dropObj->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_commit_start_done);
  if (f_dict_op[dropObj->m_vt_index].m_commit_start)
  {
    jam();
    (this->*f_dict_op[dropObj->m_vt_index].m_commit_start)(signal, dropObj);
  }
  else
  {
    jam();
    execute(signal, dropObj->m_callback, 0);
  }
}

void
Dbdict::dropObj_commit_start_done(Signal* signal, 
				  Uint32 callbackData,
				  Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  
  Uint32 objId = dropObjPtr.p->m_obj_id;
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry objEntry = * getTableEntry(xsf, objId);
  objEntry.m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
  
  Callback callback;
  callback.m_callbackData = dropObjPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_commit_writeSchemaConf);
  
  updateSchemaState(signal, objId, &objEntry, &callback);
}

void
Dbdict::dropObj_commit_writeSchemaConf(Signal* signal, 
				       Uint32 callbackData,
				       Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_commit_complete_done);
  
  if(f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
  {
    jam();
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_commit_complete)
      (signal, dropObjPtr.p);
  }
  else
  {
    jam();
    execute(signal, dropObjPtr.p->m_callback, 0);
  }
}

void
Dbdict::dropObj_commit_complete_done(Signal* signal, 
				     Uint32 callbackData,
				     Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  jam();
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  
  //@todo check error
  //@todo check master failed
  
  DictCommitConf * const conf = (DictCommitConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = dropObjPtr.p->m_senderData;
  sendSignal(dropObjPtr.p->m_senderRef, GSN_DICT_COMMIT_CONF,
	     signal, DictCommitConf::SignalLength, JBB);
  c_opDropObj.release(dropObjPtr);
}

void
Dbdict::dropObj_abort(Signal * signal, SchemaOperation * op)
{
  OpDropObj * dropObj = (OpDropObj*)op;

  dropObj->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_abort_start_done);
  if (f_dict_op[dropObj->m_vt_index].m_abort_start)
  {
    jam();
    (this->*f_dict_op[dropObj->m_vt_index].m_abort_start)(signal, dropObj);
  }
  else
  {
    jam();
    execute(signal, dropObj->m_callback, 0);
  }
}

void
Dbdict::dropObj_abort_start_done(Signal* signal, 
				 Uint32 callbackData,
				 Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  jam();
  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry objEntry = * getTableEntry(xsf, 
						    dropObjPtr.p->m_obj_id);

  Callback callback;
  callback.m_callbackData = dropObjPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_abort_writeSchemaConf);
  
  if (objEntry.m_tableState == SchemaFile::DROP_TABLE_STARTED)
  {
    jam();
    objEntry.m_tableState = SchemaFile::TABLE_ADD_COMMITTED;
        
    updateSchemaState(signal, dropObjPtr.p->m_obj_id, &objEntry, &callback);
  }
  else
  {
    jam();
    execute(signal, callback, 0);
  }
}

void
Dbdict::dropObj_abort_writeSchemaConf(Signal* signal, 
				      Uint32 callbackData,
				      Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;

  ndbrequire(returnCode == 0);
  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  dropObjPtr.p->m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropObj_abort_complete_done);
  
  if(f_dict_op[dropObjPtr.p->m_vt_index].m_abort_complete)
  {
    jam();
    (this->*f_dict_op[dropObjPtr.p->m_vt_index].m_abort_complete)
      (signal, dropObjPtr.p);
  }
  else
  {
    jam();
    execute(signal, dropObjPtr.p->m_callback, 0);
  }
}

void
Dbdict::dropObj_abort_complete_done(Signal* signal, 
				    Uint32 callbackData,
				    Uint32 returnCode)
{
  DropObjRecordPtr dropObjPtr;
  DictAbortConf * const conf = (DictAbortConf*)signal->getDataPtr();

  ndbrequire(c_opDropObj.find(dropObjPtr, callbackData));
  jam();
  conf->senderRef = reference();
  conf->senderData = dropObjPtr.p->m_senderData;
  sendSignal(dropObjPtr.p->m_senderRef, GSN_DICT_ABORT_CONF,
	     signal, DictAbortConf::SignalLength, JBB);
  c_opDropObj.release(dropObjPtr);
}

void 
Dbdict::create_fg_prepare_start(Signal* signal, SchemaOperation* op)
{
  /**
   * Put data into table record
   */
  SegmentedSectionPtr objInfoPtr;
  jam();
  getSection(objInfoPtr, ((OpCreateObj*)op)->m_obj_info_ptr_i);
  SimplePropertiesSectionReader it(objInfoPtr, getSectionSegmentPool());

  Ptr<DictObject> obj_ptr; obj_ptr.setNull();
  FilegroupPtr fg_ptr; fg_ptr.setNull();
  
  SimpleProperties::UnpackStatus status;
  DictFilegroupInfo::Filegroup fg; fg.init();
  do {
    status = SimpleProperties::unpack(it, &fg, 
				      DictFilegroupInfo::Mapping, 
				      DictFilegroupInfo::MappingSize, 
				      true, true);
    
    if(status != SimpleProperties::Eof)
    {
      jam();
      op->m_errorCode = CreateTableRef::InvalidFormat;
      break;
    }

    if(fg.FilegroupType == DictTabInfo::Tablespace)
    {
      if(!fg.TS_ExtentSize)
      {
        jam();
	op->m_errorCode = CreateFilegroupRef::InvalidExtentSize;
	break;
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
	op->m_errorCode = CreateFilegroupRef::InvalidUndoBufferSize;
	break;
      }
    }
    
    Uint32 len = strlen(fg.FilegroupName) + 1;
    Uint32 hash = Rope::hash(fg.FilegroupName, len);
    if(get_object(fg.FilegroupName, len, hash) != 0){
      jam();
      op->m_errorCode = CreateTableRef::TableAlreadyExist;
      break;
    }
    
    if(!c_obj_pool.seize(obj_ptr)){
      jam();
      op->m_errorCode = CreateTableRef::NoMoreTableRecords;
      break;
    }
    new (obj_ptr.p) DictObject;
    
    if(!c_filegroup_pool.seize(fg_ptr)){
      jam();
      op->m_errorCode = CreateTableRef::NoMoreTableRecords;
      break;
    }
  
    new (fg_ptr.p) Filegroup();

    {
      Rope name(c_rope_pool, obj_ptr.p->m_name);
      if(!name.assign(fg.FilegroupName, len, hash)){
        jam();
	op->m_errorCode = CreateTableRef::OutOfStringBuffer;
	break;
      }
    }
    
    fg_ptr.p->key = op->m_obj_id;
    fg_ptr.p->m_obj_ptr_i = obj_ptr.i;
    fg_ptr.p->m_type = fg.FilegroupType;
    fg_ptr.p->m_version = op->m_obj_version;
    fg_ptr.p->m_name = obj_ptr.p->m_name;

    switch(fg.FilegroupType){
    case DictTabInfo::Tablespace:
    {
      //fg.TS_DataGrow = group.m_grow_spec;
      fg_ptr.p->m_tablespace.m_extent_size = fg.TS_ExtentSize;
      fg_ptr.p->m_tablespace.m_default_logfile_group_id = fg.TS_LogfileGroupId;

      Ptr<Filegroup> lg_ptr;
      if (!c_filegroup_hash.find(lg_ptr, fg.TS_LogfileGroupId))
      {
        jam();
	op->m_errorCode = CreateFilegroupRef::NoSuchLogfileGroup;
	goto error;
      }

      if (lg_ptr.p->m_version != fg.TS_LogfileGroupVersion)
      {
        jam();
	op->m_errorCode = CreateFilegroupRef::InvalidFilegroupVersion;
	goto error;
      }
      increase_ref_count(lg_ptr.p->m_obj_ptr_i);
      break;
    }
    case DictTabInfo::LogfileGroup:
    {
      jam();
      fg_ptr.p->m_logfilegroup.m_undo_buffer_size = fg.LF_UndoBufferSize;
      fg_ptr.p->m_logfilegroup.m_files.init();
      //fg.LF_UndoGrow = ;
      break;
    }
    default:
      ndbrequire(false);
    }

    obj_ptr.p->m_id = op->m_obj_id;
    obj_ptr.p->m_type = fg.FilegroupType;
    obj_ptr.p->m_ref_count = 0;
    c_obj_hash.add(obj_ptr);
    c_filegroup_hash.add(fg_ptr);
    
    op->m_obj_ptr_i = fg_ptr.i;
  } while(0);

error:
  if (op->m_errorCode)
  {
    jam();
    if (!fg_ptr.isNull())
    {
      jam();
      c_filegroup_pool.release(fg_ptr);
    }

    if (!obj_ptr.isNull())
    {
      jam();
      c_obj_pool.release(obj_ptr);
    }
  }
  
  execute(signal, op->m_callback, 0);
}

void
Dbdict::create_fg_prepare_complete(Signal* signal, SchemaOperation* op)
{
  /**
   * CONTACT TSMAN LGMAN PGMAN 
   */
  CreateFilegroupImplReq* req = 
    (CreateFilegroupImplReq*)signal->getDataPtrSend();
  jam();
  req->senderData = op->key;
  req->senderRef = reference();
  req->filegroup_id = op->m_obj_id;
  req->filegroup_version = op->m_obj_version;

  FilegroupPtr fg_ptr;
  c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);
  
  Uint32 ref= 0;
  Uint32 len= 0;
  switch(op->m_obj_type){
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
  
  sendSignal(ref, GSN_CREATE_FILEGROUP_REQ, signal, len, JBB);
}

void
Dbdict::execCREATE_FILEGROUP_REF(Signal* signal)
{
  CreateFilegroupImplRef * ref = (CreateFilegroupImplRef*)signal->getDataPtr();
  CreateObjRecordPtr op_ptr;
  jamEntry();
  ndbrequire(c_opCreateObj.find(op_ptr, ref->senderData));
  op_ptr.p->m_errorCode = ref->errorCode;

  execute(signal, op_ptr.p->m_callback, 0);  
}  

void
Dbdict::execCREATE_FILEGROUP_CONF(Signal* signal)
{
  CreateFilegroupImplConf * rep = 
    (CreateFilegroupImplConf*)signal->getDataPtr();
  CreateObjRecordPtr op_ptr;
  jamEntry();
  ndbrequire(c_opCreateObj.find(op_ptr, rep->senderData));
  
  execute(signal, op_ptr.p->m_callback, 0);  
}

void
Dbdict::create_fg_abort_start(Signal* signal, SchemaOperation* op){
  (void) signal->getDataPtrSend();

  if (op->m_obj_ptr_i != RNIL)
  {
    jam();
    send_drop_fg(signal, op, DropFilegroupImplReq::Commit);
    return;
  }
  jam();  
  execute(signal, op->m_callback, 0);  
}

void
Dbdict::create_fg_abort_complete(Signal* signal, SchemaOperation* op)
{
  if (op->m_obj_ptr_i != RNIL)
  {
    jam();
    FilegroupPtr fg_ptr;
    c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);
    
    release_object(fg_ptr.p->m_obj_ptr_i);
    c_filegroup_hash.release(fg_ptr);
  }
  jam();  
  execute(signal, op->m_callback, 0);
}

void 
Dbdict::create_file_prepare_start(Signal* signal, SchemaOperation* op)
{
  /**
   * Put data into table record
   */
  SegmentedSectionPtr objInfoPtr;
  getSection(objInfoPtr, ((OpCreateObj*)op)->m_obj_info_ptr_i);
  SimplePropertiesSectionReader it(objInfoPtr, getSectionSegmentPool());
  
  Ptr<DictObject> obj_ptr; obj_ptr.setNull();
  FilePtr filePtr; filePtr.setNull();

  DictFilegroupInfo::File f; f.init();
  SimpleProperties::UnpackStatus status;
  status = SimpleProperties::unpack(it, &f, 
				    DictFilegroupInfo::FileMapping, 
				    DictFilegroupInfo::FileMappingSize, 
				    true, true);
  
  do {
    if(status != SimpleProperties::Eof){
      jam();
      op->m_errorCode = CreateFileRef::InvalidFormat;
      break;
    }

    // Get Filegroup
    FilegroupPtr fg_ptr;
    if(!c_filegroup_hash.find(fg_ptr, f.FilegroupId)){
      jam();
      op->m_errorCode = CreateFileRef::NoSuchFilegroup;
      break;
    }
    
    if(fg_ptr.p->m_version != f.FilegroupVersion){
      jam();
      op->m_errorCode = CreateFileRef::InvalidFilegroupVersion;
      break;
    }

    switch(f.FileType){
    case DictTabInfo::Datafile:
    {
      if(fg_ptr.p->m_type != DictTabInfo::Tablespace)
      {
        jam();
	op->m_errorCode = CreateFileRef::InvalidFileType;
      }
      jam();
      break;
    }
    case DictTabInfo::Undofile:
    {
      if(fg_ptr.p->m_type != DictTabInfo::LogfileGroup)
      {
        jam();
	op->m_errorCode = CreateFileRef::InvalidFileType;
      }
      jam();
      break;
    }
    default:
      jam();
      op->m_errorCode = CreateFileRef::InvalidFileType;
    }
    
    if(op->m_errorCode)
    {
      jam();
      break;
    }

    Uint32 len = strlen(f.FileName) + 1;
    Uint32 hash = Rope::hash(f.FileName, len);
    if(get_object(f.FileName, len, hash) != 0){
      jam();
      op->m_errorCode = CreateFileRef::FilenameAlreadyExists;
      break;
    }
    
    {
      Uint32 dl;
      const ndb_mgm_configuration_iterator * p = 
	m_ctx.m_config.getOwnConfigIterator();
      if(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &dl) && dl)
      {
        jam();
	op->m_errorCode = CreateFileRef::NotSupportedWhenDiskless;
	break;
      }
    }
    
    // Loop through all filenames...
    if(!c_obj_pool.seize(obj_ptr)){
      jam();
      op->m_errorCode = CreateTableRef::NoMoreTableRecords;
      break;
    }
    new (obj_ptr.p) DictObject;
    
    if (! c_file_pool.seize(filePtr)){
      jam();
      op->m_errorCode = CreateFileRef::OutOfFileRecords;
      break;
    }

    new (filePtr.p) File();

    {
      Rope name(c_rope_pool, obj_ptr.p->m_name);
      if(!name.assign(f.FileName, len, hash)){
        jam();
	op->m_errorCode = CreateTableRef::OutOfStringBuffer;
	break;
      }
    }

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
    
    /**
     * Init file
     */
    filePtr.p->key = op->m_obj_id;
    filePtr.p->m_file_size = ((Uint64)f.FileSizeHi) << 32 | f.FileSizeLo;
    filePtr.p->m_path = obj_ptr.p->m_name;
    filePtr.p->m_obj_ptr_i = obj_ptr.i;
    filePtr.p->m_filegroup_id = f.FilegroupId;
    filePtr.p->m_type = f.FileType;
    filePtr.p->m_version = op->m_obj_version;
    
    obj_ptr.p->m_id = op->m_obj_id;
    obj_ptr.p->m_type = f.FileType;
    obj_ptr.p->m_ref_count = 0;
    c_obj_hash.add(obj_ptr);
    c_file_hash.add(filePtr);

    op->m_obj_ptr_i = filePtr.i;
  } while(0);

  if (op->m_errorCode)
  {
    jam();
    if (!filePtr.isNull())
    {
      jam();
      c_file_pool.release(filePtr);
    }

    if (!obj_ptr.isNull())
    {
      jam();
      c_obj_pool.release(obj_ptr);
    }
  }
  execute(signal, op->m_callback, 0);
}


void
Dbdict::create_file_prepare_complete(Signal* signal, SchemaOperation* op)
{
  /**
   * CONTACT TSMAN LGMAN PGMAN 
   */
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
  ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op->key;
  req->senderRef = reference();
  switch(((OpCreateObj*)op)->m_restart){
  case 0:
  {
    jam();
    req->requestInfo = CreateFileImplReq::Create;
    break;
  }
  case 1:
  {
    jam();
    req->requestInfo = CreateFileImplReq::Open;
    break;
  }
  case 2:
  {
    jam();
    req->requestInfo = CreateFileImplReq::CreateForce;
    break;
  }
  }
	 
  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;
  req->file_size_hi = f_ptr.p->m_file_size >> 32;
  req->file_size_lo = f_ptr.p->m_file_size & 0xFFFFFFFF;

  Uint32 ref= 0;
  Uint32 len= 0;
  switch(op->m_obj_type){
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
  
  char name[MAX_TAB_NAME_SIZE];
  ConstRope tmp(c_rope_pool, f_ptr.p->m_path);
  tmp.copy(name);
  LinearSectionPtr ptr[3];
  ptr[0].p = (Uint32*)&name[0];
  ptr[0].sz = (strlen(name)+1+3)/4;
  sendSignal(ref, GSN_CREATE_FILE_REQ, signal, len, JBB, ptr, 1);
}

void
Dbdict::execCREATE_FILE_REF(Signal* signal)
{
  CreateFileImplRef * ref = (CreateFileImplRef*)signal->getDataPtr();
  CreateObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opCreateObj.find(op_ptr, ref->senderData));
  op_ptr.p->m_errorCode = ref->errorCode;
  execute(signal, op_ptr.p->m_callback, 0);  
}  

void
Dbdict::execCREATE_FILE_CONF(Signal* signal)
{
  CreateFileImplConf * rep = 
    (CreateFileImplConf*)signal->getDataPtr();
  CreateObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opCreateObj.find(op_ptr, rep->senderData));
  execute(signal, op_ptr.p->m_callback, 0);  
}

void
Dbdict::create_file_commit_start(Signal* signal, SchemaOperation* op)
{
  /**
   * CONTACT TSMAN LGMAN PGMAN 
   */
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
  ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));

  req->senderData = op->key;
  req->senderRef = reference();
  req->requestInfo = CreateFileImplReq::Commit;
  
  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;

  Uint32 ref= 0;
  switch(op->m_obj_type){
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
  sendSignal(ref, GSN_CREATE_FILE_REQ, signal, 
	     CreateFileImplReq::CommitLength, JBB);
}

void
Dbdict::create_file_abort_start(Signal* signal, SchemaOperation* op)
{
  CreateFileImplReq* req = (CreateFileImplReq*)signal->getDataPtrSend();

  if (op->m_obj_ptr_i != RNIL)
  {
    FilePtr f_ptr;
    FilegroupPtr fg_ptr;

    jam();
    c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
    
    ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));
    
    req->senderData = op->key;
    req->senderRef = reference();
    req->requestInfo = CreateFileImplReq::Abort;
    
    req->file_id = f_ptr.p->key;
    req->filegroup_id = f_ptr.p->m_filegroup_id;
    req->filegroup_version = fg_ptr.p->m_version;
    
    Uint32 ref= 0;
    switch(op->m_obj_type){
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
    sendSignal(ref, GSN_CREATE_FILE_REQ, signal, 
	       CreateFileImplReq::AbortLength, JBB);
    return;
  }
  execute(signal, op->m_callback, 0);
}

void
Dbdict::create_file_abort_complete(Signal* signal, SchemaOperation* op)
{
  if (op->m_obj_ptr_i != RNIL)
  {
    FilePtr f_ptr;
    FilegroupPtr fg_ptr;

    jam();
    c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
    ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));
    switch(fg_ptr.p->m_type){
    case DictTabInfo::Tablespace:
    {
      jam();
      decrease_ref_count(fg_ptr.p->m_obj_ptr_i);
      break;
    }
    case DictTabInfo::LogfileGroup:
    {
      jam();
      Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
      list.remove(f_ptr);
      break;
    }
    default:
      ndbrequire(false);
    }
    
    release_object(f_ptr.p->m_obj_ptr_i);
    c_file_hash.release(f_ptr);
  }
  execute(signal, op->m_callback, 0);
}

void
Dbdict::drop_file_prepare_start(Signal* signal, SchemaOperation* op)
{
  jam();
  send_drop_file(signal, op, DropFileImplReq::Prepare);
}

void
Dbdict::drop_undofile_prepare_start(Signal* signal, SchemaOperation* op)
{
  jam();
  op->m_errorCode = DropFileRef::DropUndoFileNotSupported;
  execute(signal, op->m_callback, 0);  
}

void
Dbdict::drop_file_commit_start(Signal* signal, SchemaOperation* op)
{
  jam();
  send_drop_file(signal, op, DropFileImplReq::Commit);
}

void
Dbdict::drop_file_commit_complete(Signal* signal, SchemaOperation* op)
{
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
  ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));
  decrease_ref_count(fg_ptr.p->m_obj_ptr_i);
  release_object(f_ptr.p->m_obj_ptr_i);
  c_file_hash.release(f_ptr);
  execute(signal, op->m_callback, 0);
}

void
Dbdict::drop_undofile_commit_complete(Signal* signal, SchemaOperation* op)
{
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
  ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));
  Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
  list.remove(f_ptr);
  release_object(f_ptr.p->m_obj_ptr_i);
  c_file_hash.release(f_ptr);
  execute(signal, op->m_callback, 0);
}

void
Dbdict::drop_file_abort_start(Signal* signal, SchemaOperation* op)
{
  jam();
  send_drop_file(signal, op, DropFileImplReq::Abort);
}

void
Dbdict::send_drop_file(Signal* signal, SchemaOperation* op,
		       DropFileImplReq::RequestInfo type)
{
  DropFileImplReq* req = (DropFileImplReq*)signal->getDataPtrSend();
  FilePtr f_ptr;
  FilegroupPtr fg_ptr;

  jam();
  c_file_pool.getPtr(f_ptr, op->m_obj_ptr_i);
  ndbrequire(c_filegroup_hash.find(fg_ptr, f_ptr.p->m_filegroup_id));
  
  req->senderData = op->key;
  req->senderRef = reference();
  req->requestInfo = type;
  
  req->file_id = f_ptr.p->key;
  req->filegroup_id = f_ptr.p->m_filegroup_id;
  req->filegroup_version = fg_ptr.p->m_version;
  
  Uint32 ref= 0;
  switch(op->m_obj_type){
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
  sendSignal(ref, GSN_DROP_FILE_REQ, signal, 
	     DropFileImplReq::SignalLength, JBB);
}

void
Dbdict::execDROP_OBJ_REF(Signal* signal)
{
  DropObjRef * const ref = (DropObjRef*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, ref->senderData));
  if(ref->errorCode != DropObjRef::NF_FakeErrorREF){
    jam();
    trans_ptr.p->setErrorCode(ref->errorCode);
  }
  Uint32 node = refToNode(ref->senderRef);
  schemaOperation_reply(signal, trans_ptr.p, node);
}

void
Dbdict::execDROP_OBJ_CONF(Signal* signal)
{
  DropObjConf * const conf = (DropObjConf*)signal->getDataPtr();
  Ptr<SchemaTransaction> trans_ptr;

  jamEntry();
  ndbrequire(c_Trans.find(trans_ptr, conf->senderData));
  schemaOperation_reply(signal, trans_ptr.p, refToNode(conf->senderRef));
}

void
Dbdict::execDROP_FILE_REF(Signal* signal)
{
  DropFileImplRef * ref = (DropFileImplRef*)signal->getDataPtr();
  DropObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opDropObj.find(op_ptr, ref->senderData));
  op_ptr.p->m_errorCode = ref->errorCode;
  execute(signal, op_ptr.p->m_callback, 0);  
}  

void
Dbdict::execDROP_FILE_CONF(Signal* signal)
{
  DropFileImplConf * rep = 
    (DropFileImplConf*)signal->getDataPtr();
  DropObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opDropObj.find(op_ptr, rep->senderData));
  execute(signal, op_ptr.p->m_callback, 0);  
}

void
Dbdict::execDROP_FILEGROUP_REF(Signal* signal)
{
  DropFilegroupImplRef * ref = (DropFilegroupImplRef*)signal->getDataPtr();
  DropObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opDropObj.find(op_ptr, ref->senderData));
  op_ptr.p->m_errorCode = ref->errorCode;
  execute(signal, op_ptr.p->m_callback, 0);  
}  

void
Dbdict::execDROP_FILEGROUP_CONF(Signal* signal)
{
  DropFilegroupImplConf * rep = 
    (DropFilegroupImplConf*)signal->getDataPtr();
  DropObjRecordPtr op_ptr;

  jamEntry();
  ndbrequire(c_opDropObj.find(op_ptr, rep->senderData));
  execute(signal, op_ptr.p->m_callback, 0);  
}

void
Dbdict::drop_fg_prepare_start(Signal* signal, SchemaOperation* op)
{
  FilegroupPtr fg_ptr;
  c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);
  
  DictObject * obj = c_obj_pool.getPtr(fg_ptr.p->m_obj_ptr_i);
  if (obj->m_ref_count)
  {
    jam();
    op->m_errorCode = DropFilegroupRef::FilegroupInUse;
    execute(signal, op->m_callback, 0);  
  }
  else
  {
    jam();
    send_drop_fg(signal, op, DropFilegroupImplReq::Prepare);
  }
}

void
Dbdict::drop_fg_commit_start(Signal* signal, SchemaOperation* op)
{
  FilegroupPtr fg_ptr;
  c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);
  if (op->m_obj_type == DictTabInfo::LogfileGroup)
  {
    jam(); 
    /**
     * Mark all undofiles as dropped
     */
    Ptr<File> filePtr;
    Local_file_list list(c_file_pool, fg_ptr.p->m_logfilegroup.m_files);
    XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
    for(list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
    {
      jam();
      Uint32 objId = filePtr.p->key;
      SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, objId);
      tableEntry->m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
      computeChecksum(xsf, objId / NDB_SF_PAGE_ENTRIES);
      release_object(filePtr.p->m_obj_ptr_i);
      c_file_hash.remove(filePtr);
    }
    list.release();
  }
  else if(op->m_obj_type == DictTabInfo::Tablespace)
  {
    FilegroupPtr lg_ptr;
    jam();
    ndbrequire(c_filegroup_hash.
	       find(lg_ptr, 
		    fg_ptr.p->m_tablespace.m_default_logfile_group_id));
    
    decrease_ref_count(lg_ptr.p->m_obj_ptr_i);
  }
  jam(); 
  send_drop_fg(signal, op, DropFilegroupImplReq::Commit);
}

void
Dbdict::drop_fg_commit_complete(Signal* signal, SchemaOperation* op)
{
  FilegroupPtr fg_ptr;
  c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);

  jam();
  release_object(fg_ptr.p->m_obj_ptr_i);
  c_filegroup_hash.release(fg_ptr);
  execute(signal, op->m_callback, 0);
}

void
Dbdict::drop_fg_abort_start(Signal* signal, SchemaOperation* op)
{
  jam();
  send_drop_fg(signal, op, DropFilegroupImplReq::Abort);
}

void
Dbdict::send_drop_fg(Signal* signal, SchemaOperation* op,
		     DropFilegroupImplReq::RequestInfo type)
{
  DropFilegroupImplReq* req = (DropFilegroupImplReq*)signal->getDataPtrSend();
  
  FilegroupPtr fg_ptr;
  c_filegroup_pool.getPtr(fg_ptr, op->m_obj_ptr_i);
  
  req->senderData = op->key;
  req->senderRef = reference();
  req->requestInfo = type;
  
  req->filegroup_id = fg_ptr.p->key;
  req->filegroup_version = fg_ptr.p->m_version;
  
  Uint32 ref= 0;
  switch(op->m_obj_type){
  case DictTabInfo::Tablespace:
    ref = TSMAN_REF;
    break;
  case DictTabInfo::LogfileGroup:
    ref = LGMAN_REF;
    break;
  default:
    ndbrequire(false);
  }
  
  sendSignal(ref, GSN_DROP_FILEGROUP_REQ, signal, 
	     DropFilegroupImplReq::SignalLength, JBB);
}

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
                 Uint32 key)
{
  D("setError" << V(code) << V(line) << V(nodeId) << V(e.errorCount));

  // can only store details for first error
  if (e.errorCount == 0) {
    e.errorCode = code;
    e.errorLine = line;
    e.errorNodeId = nodeId ? nodeId : getOwnNodeId();
    e.errorStatus = status;
    e.errorKey = key;
  }
  e.errorCount++;
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
  &Dbdict::AlterTriggerRec::g_opInfo,
  0
};

const Dbdict::OpInfo*
Dbdict::findOpInfo(Uint32 gsn)
{
  Uint32 i = 0;
  while (1) {
    const OpInfo* info = g_opInfoList[i];
    if (info->m_impl_req_gsn == 0)
      break;
    if (info->m_impl_req_gsn == gsn)
      return info;
    i++;
  }
  return 0;
}

// FeedbackTable

const Dbdict::FeedbackEntry
Dbdict::g_feedbackTable[] = {
  { 0, TransPhase::Undef, 0 }
};

const Dbdict::FeedbackEntry*
Dbdict::findFeedbackEntry(Uint32 id, TransPhase::Value phase)
{
  Uint32 i = 0;
  while (1) {
    const FeedbackEntry& e = g_feedbackTable[i];
    if (e.m_id == 0)
      break;
    if (e.m_id == id && e.m_phase == phase)
      return &e;
    i++;
  }
  return 0;
}

void
Dbdict::runFeedbackMethod(Signal* signal, SchemaOpPtr op_ptr)
{
  ndbrequire(!op_ptr.isNull());
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const TransLoc& tLoc = trans_ptr.p->m_transLoc;

  Uint32 id = DictSignal::getRequestExtra(op_ptr.p->m_requestInfo);
  if (id != 0) {
    jam();
    const FeedbackEntry* e = findFeedbackEntry(id, tLoc.m_phase);
    if (e != 0) {
      jam();
      (this->*(e->m_method))(signal, op_ptr);
    }
  }
}

// OpRec

// OpSection

bool
Dbdict::copyIn(OpSection& op_sec, const SegmentedSectionPtr& ss_ptr)
{
  const Uint32 size = ZSIZE_OF_PAGES_IN_WORDS;
  Uint32 buf[size];

  if (size < ss_ptr.sz) {
    jam();
    return false;
  }
  ::copy(buf, ss_ptr);
  if (!copyIn(op_sec, buf, ss_ptr.sz)) {
    jam();
    return false;
  }
  return true;
}

bool
Dbdict::copyIn(OpSection& op_sec, const Uint32* src, Uint32 srcSize)
{
  OpSectionBuffer buffer(c_opSectionBufferPool, op_sec.m_head);
  if (!buffer.append(src, srcSize)) {
    jam();
    return false;
  }
  return true;
}

bool
Dbdict::copyOut(const OpSection& op_sec, SegmentedSectionPtr& ss_ptr)
{
  const Uint32 size = ZSIZE_OF_PAGES_IN_WORDS;
  Uint32 buf[size];

  if (!copyOut(op_sec, buf, size)) {
    jam();
    return false;
  }
  Ptr<SectionSegment> ptr;
  if (!::import(ptr, buf, op_sec.getSize())) {
    jam();
    return false;
  }
  ss_ptr.i = ptr.i;
  ss_ptr.p = ptr.p;
  ss_ptr.sz = op_sec.getSize();
  return true;
}

bool
Dbdict::copyOut(const OpSection& op_sec, Uint32* dst, Uint32 dstSize)
{
  if (op_sec.getSize() > dstSize) {
    jam();
    return false;
  }

  // there is no const version of LocalDataBuffer
  OpSectionBufferHead tmp_head = op_sec.m_head;
  OpSectionBuffer buffer(c_opSectionBufferPool, tmp_head);

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
Dbdict::release(OpSection& op_sec)
{
  OpSectionBuffer buffer(c_opSectionBufferPool, op_sec.m_head);
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
Dbdict::seizeSchemaOp(SchemaOpPtr& op_ptr, Uint32 op_key, const OpInfo& info)
{
  if (!findSchemaOp(op_ptr, op_key)) {
    jam();
    if (c_schemaOpHash.seize(op_ptr)) {
      jam();
      new (op_ptr.p) SchemaOp();
      op_ptr.p->op_key = op_key;
      if ((this->*(info.m_seize))(op_ptr)) {
        jam();
        c_schemaOpHash.add(op_ptr);
        op_ptr.p->m_magic = SchemaOp::DICT_MAGIC;
        const char* opType = info.m_opType;
        D("seizeSchemaOp" << V(op_key) << V(opType));
        return true;
      }
      c_schemaOpHash.release(op_ptr);
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
  Uint32 op_key = op_ptr.p->op_key;
  D("releaseSchemaOp" << V(op_key));

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
  op_ptr.p->m_magic = 0;
  c_schemaOpHash.release(op_ptr);
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
                      Signal* signal, Uint32 ss_no)
{
  SegmentedSectionPtr ss_ptr;
  bool ok = signal->getSection(ss_ptr, ss_no);
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

  bool ok = copyIn(op_sec, ss_ptr);
  ndbrequire(ok);
  return true;
}

void
Dbdict::releaseOpSection(SchemaOpPtr op_ptr, Uint32 ss_no)
{
  ndbrequire(ss_no + 1 == op_ptr.p->m_sections);
  OpSection& op_sec = op_ptr.p->m_section[ss_no];
  release(op_sec);
  op_ptr.p->m_sections = ss_no;
}

// add schema op to trans during parse phase

void
Dbdict::addSchemaOp(SchemaTransPtr trans_ptr, SchemaOpPtr& op_ptr)
{
  TransLoc& tLoc = trans_ptr.p->m_transLoc;
  iteratorAddOp(tLoc, op_ptr);

  const Uint32 opDepth = trans_ptr.p->m_opDepth;
  op_ptr.p->m_trans_ptr = trans_ptr;
  op_ptr.p->m_opDepth = opDepth;

  // add global flags from trans
  const Uint32& src_info = trans_ptr.p->m_requestInfo;
  DictSignal::addRequestFlagsGlobal(op_ptr.p->m_requestInfo, src_info);

  D("addSchemaOp" << *op_ptr.p);
}

// update op step after successful execution

void
Dbdict::updateSchemaOpStep(SchemaTransPtr trans_ptr, SchemaOpPtr op_ptr)
{
  ndbrequire(!op_ptr.isNull());
  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  D("updateSchemaOpStep" << *op_ptr.p << tLoc);
  new (&op_ptr.p->m_step) TransStep(tLoc);
}

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

  bool ok = c_obj_hash.seize(obj_ptr);
  ndbrequire(ok);
  new (obj_ptr.p) DictObject();

  obj_ptr.p->m_name = name;
  c_obj_hash.add(obj_ptr);
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
    TransLoc& tLoc = trans_ptr.p->m_transLoc;

    {
      LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
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

bool
Dbdict::getOpPtr(const TransLoc& tLoc, SchemaOpPtr& op_ptr)
{
  if (tLoc.m_opKey == 0) {
    jam();
    op_ptr.setNull();
  } else {
    jam();
    bool ok = findSchemaOp(op_ptr, tLoc.m_opKey);
    ndbrequire(ok);
  }
  return !op_ptr.isNull();
}

const Dbdict::TransPhaseEntry&
Dbdict::getPhaseEntry(const TransLoc& tLoc)
{
  ndbrequire(tLoc.m_phaseList != 0);
  const TransPhaseList& phaseList = *tLoc.m_phaseList;
  const Uint32 phaseIndex = tLoc.m_phaseIndex;
  ndbrequire(phaseIndex > 0 && phaseIndex < phaseList.m_length);
  const TransPhaseEntry& phaseEntry = phaseList.m_phaseEntry[phaseIndex];
  return phaseEntry;
}

void
Dbdict::iteratorAddOp(TransLoc& tLoc, SchemaOpPtr op_ptr)
{
  LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
#ifdef VM_TRACE
  // check for duplicates
  {
    SchemaOpPtr loop_ptr;
    Uint32 size = 0;
    list.first(loop_ptr);
    while (!loop_ptr.isNull()) {
      jam();
      ndbrequire(loop_ptr.i != op_ptr.i);
      ndbrequire(loop_ptr.p->op_key != op_ptr.p->op_key);
      list.next(loop_ptr);
      size++;
    }
    ndbrequire(tLoc.m_opList.m_size == size);
  }
#endif
  // add it to end
  list.addLast(op_ptr);
  op_ptr.p->m_opIndex = tLoc.m_opList.m_size;
  tLoc.m_opList.m_size += 1;
  // position iterator at the new op
  tLoc.m_opKey = op_ptr.p->op_key;
}

void
Dbdict::iteratorRemoveLastOp(TransLoc& tLoc, SchemaOpPtr& op_ptr)
{
  D("iteratorRemoveOp" << *op_ptr.p);
  if (tLoc.m_opKey == op_ptr.p->op_key) {
    jam();
    // move to previous op or start of list
    iteratorNext(tLoc, -1);
  }
  // verify op is last and remove it
  LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
  bool isLast = !list.hasNext(op_ptr);
  ndbrequire(isLast);
  list.remove(op_ptr);
  ndbrequire(tLoc.m_opList.m_size == op_ptr.p->m_opIndex + 1);
  tLoc.m_opList.m_size -= 1;
  releaseSchemaOp(op_ptr);
}

bool
Dbdict::iteratorFirstOp(TransLoc& tLoc, int dir)
{
  if (tLoc.m_opList.m_size == 0) {
    jam();
    return false;
  }
  LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
  SchemaOpPtr op_ptr;
  bool ok = dir > 0 ? list.first(op_ptr) : list.last(op_ptr);
  ndbrequire(ok);
  ndbrequire(op_ptr.p->op_key != 0);
  tLoc.m_opKey = op_ptr.p->op_key;
  return true;
}

bool
Dbdict::iteratorNextOp(TransLoc& tLoc, int dir)
{
  if (tLoc.m_opList.m_size == 0) {
    jam();
    return false;
  }
  LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
  SchemaOpPtr op_ptr;
  ndbrequire(tLoc.m_opKey != 0);
  bool ok = findSchemaOp(op_ptr, tLoc.m_opKey);
  ndbrequire(ok);
  if (!(dir > 0 ? list.next(op_ptr) : list.prev(op_ptr))) {
    jam();
    return false;
  }
  ndbrequire(op_ptr.p->op_key != 0);
  tLoc.m_opKey = op_ptr.p->op_key;
  return true;
}

bool
Dbdict::iteratorFirst(TransLoc& tLoc, int dir)
{
  ndbrequire(tLoc.m_phaseList != 0);
  const TransPhaseList& phaseList = *tLoc.m_phaseList;
  ndbrequire(phaseList.m_length >= 2);
  const Uint32 i = dir > 0 ? 1 : phaseList.m_length - 1;
  const TransPhaseEntry& phaseEntry = phaseList.m_phaseEntry[i];
  // end points must be simple (no ops)
  ndbrequire(phaseEntry.m_phaseFlags & TransPhaseFlag::Simple);
  tLoc.m_phaseIndex = i;
  tLoc.m_phase = phaseEntry.m_phase;
  tLoc.m_subphase = phaseEntry.m_subphase;
  tLoc.m_repeat = 0;
  tLoc.m_opKey = 0;
  return true;
}

bool
Dbdict::iteratorNext(TransLoc& tLoc, int dir)
{
  ndbrequire(tLoc.m_phaseList != 0);
  const TransPhaseList& phaseList = *tLoc.m_phaseList;
  bool loop_one = true;
  Uint32 i = tLoc.m_phaseIndex;
  for (; i > 0 && i < phaseList.m_length; i += dir, loop_one = false) {
    const TransPhaseEntry& phaseEntry = phaseList.m_phaseEntry[i];
    if (phaseEntry.m_phaseFlags & TransPhaseFlag::Simple ||
        tLoc.m_opList.m_size == 0) {
      jam();
      tLoc.m_opKey = 0;
      if (loop_one) {
        jam();
        continue;
      }
    } else {
      if (tLoc.m_opKey == 0) {
        jam();
        bool ok = iteratorFirstOp(tLoc, dir);
        ndbrequire(ok);
      } else {
        jam();
        if (!iteratorNextOp(tLoc, dir)) {
          jam();
          tLoc.m_opKey = 0;
          continue;
        }
      }
    }
    tLoc.m_phaseIndex = i;
    tLoc.m_phase = phaseEntry.m_phase;
    tLoc.m_subphase = phaseEntry.m_subphase;
    tLoc.m_repeat = 0;
    return true;
  }
  return false;
}

bool
Dbdict::iteratorInit(TransLoc& tLoc)
{
  tLoc.m_phaseList = &g_defaultPhaseList;
  tLoc.m_mode = TransMode::Normal;
  return iteratorFirst(tLoc, +1);
}

bool
Dbdict::iteratorMove(TransLoc& tLoc, bool repeatFlag)
{
  D("iteratorMove <" << tLoc << V(repeatFlag));
  bool ret = false;
  if (repeatFlag) {
    jam();
    tLoc.m_repeat += 1;
    ret = true;
  }
  else {
    if (tLoc.m_hold) {
      jam();
      tLoc.m_hold = false;
      ret = true;
    }
    else if (tLoc.m_mode == TransMode::Normal) {
      jam();
      ret = iteratorNext(tLoc, +1);
    }
    else if (tLoc.m_mode == TransMode::Rollback) {
      jam();
      ret = iteratorNext(tLoc, -1);
    }
    else if (tLoc.m_mode == TransMode::Abort) {
      jam();
      ret = iteratorNext(tLoc, -1);
    }
    else {
      ndbrequire(false);
    }
    tLoc.m_repeat = 0;
  }
  D("iteratorMove >" << tLoc << V(ret));
  return ret;
}

void
Dbdict::iteratorCopy(TransLoc& tLoc, Uint32 mode,
                     Uint32 phaseIndex, Uint32 op_key, Uint32 repeat)
{
  D("iteratorCopy <" << tLoc);
  tLoc.m_phaseList = &g_defaultPhaseList;
  tLoc.m_mode = (TransMode::Value)mode;
  tLoc.m_phaseIndex = phaseIndex;
  const TransPhaseEntry& phaseEntry = getPhaseEntry(tLoc);
  tLoc.m_phase = phaseEntry.m_phase;
  tLoc.m_subphase = phaseEntry.m_subphase;
  tLoc.m_repeat = repeat;
  tLoc.m_opKey = op_key;
  D("iteratorCopy >" << tLoc);
}

Dbdict::TransGlob&
Dbdict::getTransGlob(SchemaTransPtr trans_ptr, Uint32 nodeId)
{
  ndbrequire(nodeId > 0 && nodeId < MAX_NDB_NODES);
  TransGlob& tGlob = trans_ptr.p->m_transGlob[nodeId];
  return tGlob;
}

void
Dbdict::setGlobState(SchemaTransPtr trans_ptr,
                     Uint32 nodeId, TransGlob::State state)
{
  TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
  TransGlob tGlob2 = tGlob;
  tGlob2.state(state);
  D("setGlobState " << nodeId << ":" << tGlob << " ->" << tGlob2);
  tGlob = tGlob2;
}

void
Dbdict::setGlobState(SchemaTransPtr trans_ptr,
                     const NdbNodeBitmask& nodes, TransGlob::State state)
{
  Uint32 nodeId;
  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (nodes.get(nodeId)) {
      setGlobState(trans_ptr, nodeId, state);
    }
  }
}

void
Dbdict::setGlobState(SchemaTransPtr trans_ptr,
                     TransGlob::State oldstate, TransGlob::State state)
{
  Uint32 nodeId;
  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (trans_ptr.p->m_initNodes.get(nodeId)) {
      const TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
      if (tGlob.state() == oldstate) {
        jam();
        setGlobState(trans_ptr, nodeId, state);
      }
    }
  }
}

void
Dbdict::setGlobFlags(SchemaTransPtr trans_ptr,
                     Uint32 nodeId, Uint32 flags, int op)
{
  TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
  TransGlob tGlob2 = tGlob;
  if (op < 0)
    tGlob2.m_flags &= ~flags;
  else if (op > 0)
    tGlob2.m_flags |= flags;
  else
    tGlob2.m_flags = flags;
  D("setGlobFlags " << nodeId << ":" << tGlob << " ->" << tGlob2);
  tGlob = tGlob2;
}

void
Dbdict::setGlobFlags(SchemaTransPtr trans_ptr,
                     const NdbNodeBitmask& nodes, Uint32 flags, int op)
{
  Uint32 nodeId;
  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (nodes.get(nodeId)) {
      setGlobFlags(trans_ptr, nodeId, flags, op);
    }
  }
}

Uint32
Dbdict::getGlobFlags(SchemaTransPtr trans_ptr,
                     const NdbNodeBitmask& nodes)
{
  Uint32 flags = 0;
  Uint32 nodeId;
  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (nodes.get(nodeId)) {
      const TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
      flags |= tGlob.m_flags;
    }
  }
  return flags;
}

const Dbdict::TransPhaseEntry
Dbdict::g_defaultPhaseEntry[] = {
  { TransPhase::Undef,
    TransSubphase::Undef,
    0,
    0,
    0
  },
  {
    TransPhase::Begin,
    TransSubphase::Main,
    TransPhaseFlag::Simple,
    0,
    0
  },
  {
    TransPhase::Parse,
    TransSubphase::Main,
    0,
    0,
    0
  },
  {
    TransPhase::Prepare,
    TransSubphase::Main,
    0,
    0,
    0
  },
  {
    TransPhase::Commit,
    TransSubphase::Main,
    0,
    0,
    0
  },
  {
    TransPhase::End,
    TransSubphase::Main,
    TransPhaseFlag::Simple,
    0,
    0
  }
};

const Dbdict::TransPhaseList
Dbdict::g_defaultPhaseList = {
  0,
  sizeof(g_defaultPhaseEntry) / sizeof(g_defaultPhaseEntry[0]),
  g_defaultPhaseEntry
};

void
Dbdict::dummySimplePhase(Signal* signal, SchemaTransPtr trans_ptr)
{
  jam();
  Uint32 itRepeat = getIteratorRepeat(trans_ptr);

  D("dummySimplePhase");
  ndbrequire(itRepeat == 0);
  sendTransConf(signal, trans_ptr);
}

// SchemaTrans

bool
Dbdict::seizeSchemaTrans(SchemaTransPtr& trans_ptr, Uint32 trans_key)
{
  if (!findSchemaTrans(trans_ptr, trans_key)) {
    jam();
    if (c_schemaTransHash.seize(trans_ptr)) {
      jam();
      new (trans_ptr.p) SchemaTrans();
      trans_ptr.p->trans_key = trans_key;
      c_schemaTransHash.add(trans_ptr);
      c_schemaTransList.addLast(trans_ptr);
      c_schemaTransCount++;
      trans_ptr.p->m_magic = SchemaTrans::DICT_MAGIC;
      D("seizeSchemaTrans" << V(trans_key));
      return true;
    }
  }
  trans_ptr.setNull();
  return false;
}

bool
Dbdict::seizeSchemaTrans(SchemaTransPtr& trans_ptr)
{
  Uint32 trans_key = c_opRecordSequence + 1;
  if (seizeSchemaTrans(trans_ptr, trans_key)) {
    c_opRecordSequence = trans_key;
    return true;
  }
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
  Uint32 trans_key = trans_ptr.p->trans_key;
  D("releaseSchemaTrans" << V(trans_key));

  TransLoc& tLoc = trans_ptr.p->m_transLoc;
  LocalDLFifoList<SchemaOp> list(c_schemaOpPool, tLoc.m_opList.m_head);
  SchemaOpPtr op_ptr;
  while (list.first(op_ptr)) {
    list.remove(op_ptr);
    releaseSchemaOp(op_ptr);
  }
  ndbrequire(trans_ptr.p->m_magic == SchemaTrans::DICT_MAGIC);
  trans_ptr.p->m_magic = 0;
  ndbrequire(c_schemaTransCount != 0);
  c_schemaTransCount--;
  c_schemaTransList.remove(trans_ptr);
  c_schemaTransHash.release(trans_ptr);
  trans_ptr.setNull();

  // only 1 trans at time so it is easy to check for leaks
  ndbrequire(c_schemaOpPool.getNoOfFree() == c_schemaOpPool.getSize());
  ndbrequire(c_opSectionBufferPool.getNoOfFree() == c_opSectionBufferPool.getSize());
}

Uint32
Dbdict::getIteratorRepeat(SchemaTransPtr trans_ptr)
{
  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  return tLoc.m_repeat;
}

// client requests

void
Dbdict::execSCHEMA_TRANS_BEGIN_REQ(Signal* signal)
{
  jamEntry();
  const SchemaTransBeginReq* req =
    (const SchemaTransBeginReq*)signal->getDataPtr();
  Uint32 clientRef = req->clientRef;
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
    if (!localTrans) {
      trans_ptr.p->m_initNodes = c_aliveNodes;
    } else {
      trans_ptr.p->m_initNodes.clear();
      trans_ptr.p->m_initNodes.set(getOwnNodeId());
    }
    trans_ptr.p->m_clientState = TransClient::BeginReq;

    // initialize local state and iterator
    TransLoc& tLoc = trans_ptr.p->m_transLoc;
    iteratorInit(tLoc);

    // initialize global state
    const Uint32 ownNodeId = getOwnNodeId();
    setGlobState(trans_ptr, ownNodeId, TransGlob::Ok);

    // other nodes have no trans record yet
    NdbNodeBitmask nodes = trans_ptr.p->m_initNodes;
    nodes.bitANDC(trans_ptr.p->m_failedNodes);
    nodes.clear(ownNodeId);
    setGlobState(trans_ptr, nodes, TransGlob::NeedTrans);

    // lock
    DictLockReq& lockReq = trans_ptr.p->m_lockReq;
    lockReq.userPtr = trans_ptr.p->trans_key;
    lockReq.userRef = reference();
    lockReq.lockType = DictLockReq::SchemaTransLock;
    int lockError = dict_lock_trylock(&lockReq);
    if (lockError != 0) {
      // remove the trans
      releaseSchemaTrans(trans_ptr);
      setError(error, lockError, __LINE__);
      break;
    }

    // begin tx on all participants
    sendTransReq(signal, trans_ptr);
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

  bool localTrans = (requestInfo & DictSignal::RF_LOCAL_TRANS);

  SchemaTransPtr trans_ptr;
  ErrorInfo error;
  do {
    if (getOwnNodeId() != c_masterNodeId && !localTrans) {
      jam();
      // future when MNF is handled
      setError(error, SchemaTransEndRef::NotMaster, __LINE__);
      break;
    }

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

    if (trans_ptr.p->m_clientState != TransClient::BeginReply &&
        trans_ptr.p->m_clientState != TransClient::ParseReply) {
      jam();
      setError(error, SchemaTransEndRef::InvalidTransState, __LINE__);
      break;
    }

    {
      Uint32 requestInfo2 = trans_ptr.p->m_requestInfo;
      bool localTrans2 = (requestInfo2 & DictSignal::RF_LOCAL_TRANS);
      ndbrequire(localTrans == localTrans2);
    }

    ndbrequire(!hasError(trans_ptr.p->m_error));
    trans_ptr.p->m_clientState = TransClient::EndReq;

    const TransLoc& tLoc = trans_ptr.p->m_transLoc;
    if (tLoc.m_mode == TransMode::Rollback) {
      jam();
      D("continuing after rollback");
      resetError(trans_ptr);
      setTransMode(trans_ptr, TransMode::Normal, false);
    }
    ndbrequire(tLoc.m_mode == TransMode::Normal);

    D("execSCHEMA_TRANS_END_REQ" << *trans_ptr.p << tLoc.m_opList);

    setGlobState(trans_ptr, TransGlob::NeedOp, TransGlob::NoOp);
    setGlobState(trans_ptr, TransGlob::NeedTrans, TransGlob::NoTrans);

    const bool doBackground = flags & SchemaTransEndReq::SchemaTransBackground;
    if (doBackground) {
      jam();
      // send reply to original client and restore EndReq state
      sendTransClientReply(signal, trans_ptr);
      trans_ptr.p->m_clientState = TransClient::EndReq;

      // take over client role via internal trans
      trans_ptr.p->m_clientFlags |= TransClient::Background;
      takeOverTransClient(signal, trans_ptr);
    }

    const bool doAbort = flags & SchemaTransEndReq::SchemaTransAbort;
    if (doAbort) {
      jam();
      setTransMode(trans_ptr, TransMode::Abort, true);
    }

    ndbrequire(!hasError(trans_ptr.p->m_error));
    runTransMaster(signal, trans_ptr);
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
Dbdict::handleClientReq(Signal* signal, SchemaOpPtr op_ptr)
{
  D("handleClientReq" << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;

  ErrorInfo error;
  const OpInfo& info = getOpInfo(op_ptr);
  (this->*(info.m_parse))(signal, op_ptr, error);

  if (hasError(error)) {
    jam();
    setError(op_ptr, error);
    setTransMode(trans_ptr, TransMode::Rollback, true);

    // other nodes will not acquire an operation record
    const Uint32 ownNodeId = getOwnNodeId();
    NdbNodeBitmask nodes = trans_ptr.p->m_initNodes;
    nodes.bitANDC(trans_ptr.p->m_failedNodes);
    nodes.clear(ownNodeId);
    setGlobState(trans_ptr, nodes, TransGlob::NoOp);

    releaseSections(signal);
    runTransMaster(signal, trans_ptr);
    return;
  }

  updateSchemaOpStep(trans_ptr, op_ptr);
  sendTransParseReq(signal, op_ptr);
}

void
Dbdict::sendTransReq(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("sendTransReq" << *trans_ptr.p);

  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  SchemaOpPtr op_ptr;
  getOpPtr(tLoc, op_ptr);
  Uint32 gsn = 0;

  // piggy-backed signal in parse phase
  Uint32 extra_length = 0;
  if (tLoc.m_mode == TransMode::Normal &&
      tLoc.m_phase == TransPhase::Parse) {
    jam();
    const OpInfo& info = getOpInfo(op_ptr);
    gsn = info.m_impl_req_gsn;
    const Uint32* src = op_ptr.p->m_oprec_ptr.p->m_impl_req_data;

    Uint32* data = signal->getDataPtrSend();
    Uint32 skip = SchemaTransImplReq::SignalLength;
    extra_length = info.m_impl_req_length;
    ndbrequire(skip + extra_length <= 25);

    Uint32 i;
    for (i = 0; i < extra_length; i++)
      data[skip + i] = src[i];
  }

  SchemaTransImplReq* req = (SchemaTransImplReq*)signal->getDataPtrSend();

  Uint32 phaseInfo = 0;
  SchemaTransImplReq::setMode(phaseInfo, tLoc.m_mode);
  SchemaTransImplReq::setPhase(phaseInfo, tLoc.m_phase);
  SchemaTransImplReq::setSubphase(phaseInfo, tLoc.m_subphase);
  SchemaTransImplReq::setGsn(phaseInfo, gsn);

  Uint32 requestInfo = 0;
  if (!op_ptr.isNull())
    requestInfo = op_ptr.p->m_requestInfo;
  else
    requestInfo = trans_ptr.p->m_requestInfo;

  Uint32 operationInfo = 0;
  if (!op_ptr.isNull()) {
    SchemaTransImplReq::setOpIndex(operationInfo, op_ptr.p->m_opIndex);
    SchemaTransImplReq::setOpDepth(operationInfo, op_ptr.p->m_opDepth);
  }

  Uint32 iteratorInfo = 0;
  SchemaTransImplReq::setListId(iteratorInfo, tLoc.m_phaseList->m_id);
  SchemaTransImplReq::setListIndex(iteratorInfo, tLoc.m_phaseIndex);
  SchemaTransImplReq::setItRepeat(iteratorInfo, tLoc.m_repeat);

  req->senderRef = reference();
  req->transKey = trans_ptr.p->trans_key;
  req->opKey = !op_ptr.isNull() ? op_ptr.p->op_key : 0;
  req->phaseInfo = phaseInfo;
  req->requestInfo = requestInfo;
  req->operationInfo = operationInfo;
  req->iteratorInfo = iteratorInfo;
  req->clientRef = trans_ptr.p->m_clientRef;
  req->transId = trans_ptr.p->m_transId;

  NdbNodeBitmask nodes;
  Uint32 nodeId;

  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++) {
    if (!trans_ptr.p->m_initNodes.get(nodeId)) {
      jam();
      continue;
    }

    const TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
    bool itRepeat = tGlob.m_flags & SchemaTransImplConf::IT_REPEAT;
    setGlobFlags(trans_ptr, nodeId, SchemaTransImplConf::IT_REPEAT, -1);

    {
      bool b1 = tGlob.state() == TransGlob::NodeFail;
      bool b2 = trans_ptr.p->m_failedNodes.get(nodeId);
      ndbrequire(b1 == b2);
    }

    if (tGlob.state() == TransGlob::NodeFail ||
        tGlob.state() == TransGlob::NoTrans ||
        tGlob.state() == TransGlob::NoOp) {
      jam();
      D("skip send (state)" << V(nodeId) << tGlob);
      continue;
    }

    if (tLoc.m_repeat && !itRepeat) {
      D("skip send (no repeat)" << V(nodeId));
      jam();
      continue;
    }

    jam();
    nodes.set(nodeId);
  }

  ndbrequire(!nodes.isclear());//wl3600_todo got crash here after NF

  NodeReceiverGroup rg(DBDICT, nodes);
  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    bool ok = sc.init<SchemaTransImplRef>(rg, trans_ptr.p->trans_key);
    ndbrequire(ok);
  }

  sendFragmentedSignal(rg, GSN_SCHEMA_TRANS_IMPL_REQ, signal,
                       SchemaTransImplReq::SignalLength + extra_length, JBB);
}

void
Dbdict::sendTransParseReq(Signal* signal, SchemaOpPtr op_ptr)
{
  D("sendTransParseReq");

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  ndbrequire(tLoc.m_mode == TransMode::Normal &&
             tLoc.m_phase == TransPhase::Parse);
  sendTransReq(signal, trans_ptr);
  releaseSections(signal);
}

void
Dbdict::execSCHEMA_TRANS_IMPL_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  recvTransReply(signal, true);
}

void
Dbdict::execSCHEMA_TRANS_IMPL_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  recvTransReply(signal, false);
}

void
Dbdict::recvTransReply(Signal* signal, bool isConf)
{
  Uint32 trans_key;
  Uint32 senderRef;
  Uint32 itFlags = 0;
  ErrorInfo error;

  if (isConf) {
    jam();
    const SchemaTransImplConf* conf =
      (const SchemaTransImplConf*)signal->getDataPtr();
    senderRef = conf->senderRef;
    trans_key = conf->transKey;
    itFlags = conf->itFlags;
  } else {
    jam();
    const SchemaTransImplRef* ref =
      (const SchemaTransImplRef*)signal->getDataPtr();
    senderRef = ref->senderRef;
    trans_key = ref->transKey;
    setError(error, ref);
  }

  Uint32 nodeId = refToNode(senderRef);
  D("recvTransReply" << V(isConf) << V(nodeId) << V(trans_key));

  SchemaTransPtr trans_ptr;
  findSchemaTrans(trans_ptr, trans_key);
  ndbrequire(!trans_ptr.isNull());
  ndbrequire(trans_ptr.p->m_initNodes.get(nodeId));

  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  const TransGlob& tGlob = getTransGlob(trans_ptr, nodeId);
  setGlobFlags(trans_ptr, nodeId, itFlags, +1);

  // clear previous round

  setGlobState(trans_ptr, TransGlob::NoTrans, TransGlob::Ok);
  setGlobState(trans_ptr, TransGlob::NoOp, TransGlob::Ok);

  // set remote state according to error type

  if (!hasError(error)) {
    jam();
    setGlobState(trans_ptr, nodeId, TransGlob::Ok);
  }
  else if (error.errorCode == SchemaTransImplRef::NF_FakeErrorREF) {
    jam();
    // exclude the node permanently from this tx
    D("node marked dead" << V(nodeId));
    setGlobState(trans_ptr, nodeId, TransGlob::NodeFail);
    trans_ptr.p->m_failedNodes.set(nodeId);
    resetError(error);
  }
  else if (error.errorCode == SchemaTransImplRef::TooManySchemaTrans) {
    jam();
    D("node failed to seize trans record" << V(nodeId));
    ndbrequire(tLoc.m_mode == TransMode::Normal);
    ndbrequire(tLoc.m_phase == TransPhase::Begin);
    ndbrequire(tGlob.state() == TransGlob::NeedTrans);
    setGlobState(trans_ptr, nodeId, TransGlob::NoTrans);
    setError(trans_ptr, error);
  }
  else if (error.errorCode == SchemaTransImplRef::TooManySchemaOps) {
    jam();
    D("node failed to seize op record" << V(nodeId));
    ndbrequire(tLoc.m_mode == TransMode::Normal);
    ndbrequire(tLoc.m_phase == TransPhase::Parse);
    ndbrequire(tGlob.state() == TransGlob::NeedOp);
    setGlobState(trans_ptr, nodeId, TransGlob::NoOp);
    setError(trans_ptr, error);
  }
  else {
    jam();
    D("node returned error" << V(nodeId) << V(error.errorCode));
    setGlobState(trans_ptr, nodeId, TransGlob::Error);
    setError(trans_ptr, error);
  }

  {
    SafeCounter sc(c_counterMgr, trans_ptr.p->m_counter);
    if (!sc.clearWaitingFor(nodeId)) {
      jam();
      return;
    }
  }

  // all participants have replied to this round
  handleTransReply(signal, trans_ptr);
}

void
Dbdict::handleTransReply(Signal* signal, SchemaTransPtr trans_ptr)
{
  TransLoc& tLoc = trans_ptr.p->m_transLoc;
  SchemaOpPtr op_ptr;
  getOpPtr(tLoc, op_ptr);

  const Uint32 trans_key = trans_ptr.p->trans_key;
  const Uint32 clientRef = trans_ptr.p->m_clientRef;
  const Uint32 transId = trans_ptr.p->m_transId;

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
    else if (tLoc.m_phase == TransPhase::End) {
      jam();
      sendTransClientReply(signal, trans_ptr);
      // unlock
      const DictLockReq& lockReq = trans_ptr.p->m_lockReq;
      dict_lock_unlock(signal, &lockReq);
      releaseSchemaTrans(trans_ptr);
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
      dict_lock_unlock(signal, &lockReq);
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

void
Dbdict::createSubOps(Signal* signal, SchemaOpPtr op_ptr, bool first)
{
  D("createSubOps" << *op_ptr.p);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  if (first) {
    jam();
    // raise operation depth for any sub-ops which may be created
    trans_ptr.p->m_opDepth += 1;
  }

  const OpInfo& info = getOpInfo(op_ptr);
  if ((this->*(info.m_subOps))(signal, op_ptr)) {
    jam();
    // more sub-ops on the way
    return;
  }

  ErrorInfo error;
  (this->*(info.m_reply))(signal, op_ptr, error);

  // restore operation depth
  ndbrequire(trans_ptr.p->m_opDepth != 0);
  trans_ptr.p->m_opDepth -= 1;

  if (trans_ptr.p->m_opDepth == 0) {
    jam();
    resetError(trans_ptr);
    trans_ptr.p->m_clientState = TransClient::ParseReply;
  }
}

// a sub-op create failed, roll back and send REF to client
void
Dbdict::abortSubOps(Signal* signal, SchemaOpPtr op_ptr, ErrorInfo error)
{
  D("abortSubOps" << *op_ptr.p << error);

  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  const OpInfo& info = getOpInfo(op_ptr);

  // restore operation depth
  ndbrequire(trans_ptr.p->m_opDepth != 0);
  trans_ptr.p->m_opDepth -= 1;

  // roll back all subops and this op (the parent)
  ndbrequire(hasError(error));
  setError(op_ptr, error);
  setTransMode(trans_ptr, TransMode::Rollback, true);
  runTransMaster(signal, trans_ptr);
}

void
Dbdict::runTransMaster(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("runTransMaster");
  TransLoc& tLoc = trans_ptr.p->m_transLoc;

  if (tLoc.m_phase == TransPhase::Begin) {
    jam();
    // skip trans with no operations
    if (tLoc.m_mode == TransMode::Normal) {
      while (tLoc.m_phase != TransPhase::End) {
        jam();
        iteratorMove(tLoc, false);
      }
    }
  } else {
    jam();
    NdbNodeBitmask nodes = trans_ptr.p->m_initNodes;
    nodes.bitANDC(trans_ptr.p->m_failedNodes);
    Uint32 flags = getGlobFlags(trans_ptr, nodes);
    bool repeatFlag = (flags & SchemaTransImplConf::IT_REPEAT);
    bool found = iteratorMove(tLoc, repeatFlag);
    ndbrequire(found);
  }

  // always send to all in order to keep iterators synced
  sendTransReq(signal, trans_ptr);
}

void
Dbdict::setTransMode(SchemaTransPtr trans_ptr,
                     TransMode::Value new_mode,
                     bool hold)
{
  TransLoc& tLoc = trans_ptr.p->m_transLoc;
  const TransMode::Value old_mode = tLoc.m_mode;

  D("setTransMode"
    << *trans_ptr.p
    << " old:" << DictSignal::getTransModeName(old_mode)
    << " new:" << DictSignal::getTransModeName(new_mode)
    << V(hold));

  tLoc.m_mode = new_mode;
  tLoc.m_hold = hold;
  NdbNodeBitmask nodes = trans_ptr.p->m_initNodes;
  setGlobFlags(trans_ptr, nodes, SchemaTransImplConf::IT_REPEAT, -1);
  tLoc.m_repeat = 0;
}

// participant

void
Dbdict::execSCHEMA_TRANS_IMPL_REQ(Signal* signal)
{
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  jamEntry();
  recvTransReq(signal);
}

void
Dbdict::recvTransReq(Signal* signal)
{
  const SchemaTransImplReq* req =
    (const SchemaTransImplReq*)signal->getDataPtr();

  D("recvTransReq" << V(signal->getLength()));

  const Uint32 senderRef = req->senderRef;
  const Uint32 trans_key = req->transKey;
  const Uint32 op_key = req->opKey;
  const Uint32 phaseInfo = req->phaseInfo;
  const Uint32 requestInfo = req->requestInfo;
  const Uint32 operationInfo = req->operationInfo;
  const Uint32 iteratorInfo = req->iteratorInfo;
  const Uint32 clientRef = req->clientRef;
  const Uint32 transId = req->transId;

  // unpack phaseInfo
  const Uint32 mode = SchemaTransImplReq::getMode(phaseInfo);
  const Uint32 phase = SchemaTransImplReq::getPhase(phaseInfo);
  const Uint32 subphase = SchemaTransImplReq::getSubphase(phaseInfo);
  const Uint32 gsn = SchemaTransImplReq::getGsn(phaseInfo);

  // unpack operation info
  const Uint32 opDepth = SchemaTransImplReq::getOpDepth(operationInfo);

  // unpack iterator info
  const Uint32 phaseIndex = SchemaTransImplReq::getListIndex(iteratorInfo);
  const Uint32 repeat = SchemaTransImplReq::getItRepeat(iteratorInfo);

  // cannot receive a new master before this signal arrives
  // enable when RF_LOCAL_TRANS is in req
  //ndbrequire(c_masterNodeId == refToNode(senderRef));

  SchemaTransPtr trans_ptr;
  ErrorInfo error;
  do {
    const bool isMaster = (senderRef == reference());

    const bool seizeTrans =
      !isMaster && mode == TransMode::Normal && phase == TransPhase::Begin;

    const bool releaseTrans =
      !isMaster && mode == TransMode::Normal && phase == TransPhase::End ||
      !isMaster && mode != TransMode::Normal && phase == TransPhase::Begin;

    const bool sendConf =
      phase == TransPhase::Begin || phase == TransPhase::End;

    if (seizeTrans) {
      jam();
      if (!seizeSchemaTrans(trans_ptr, trans_key)) {
        jam();
        setError(error, SchemaTransImplRef::TooManySchemaTrans, __LINE__);
        break;
      }
      trans_ptr.p->m_clientRef = clientRef;
      trans_ptr.p->m_transId = transId;
    } else {
      jam();
      findSchemaTrans(trans_ptr, trans_key);
      ndbrequire(!trans_ptr.isNull());
    }

    TransLoc& tLoc = trans_ptr.p->m_transLoc;

    if (isMaster) {
      ndbrequire(trans_ptr.p->m_isMaster == true);
      ndbrequire(trans_ptr.p->m_masterRef == senderRef);
      ndbrequire(tLoc.m_mode == mode);
      ndbrequire(tLoc.m_phase == phase);
      ndbrequire(tLoc.m_subphase == subphase);
    } else {
      jam();
      if (trans_ptr.p->m_masterRef == 0) {
        jam();
        // first time
        trans_ptr.p->m_isMaster = false;
        trans_ptr.p->m_masterRef = senderRef;
      } else {
        // until MNF anyway
        ndbrequire(trans_ptr.p->m_isMaster == false);
        ndbrequire(trans_ptr.p->m_masterRef == senderRef);
      }
      trans_ptr.p->m_opDepth = opDepth;
      iteratorCopy(tLoc, mode, phaseIndex, op_key, repeat);
      ndbrequire(tLoc.m_phase == phase && tLoc.m_subphase == subphase);
    }

    if (tLoc.m_mode == TransMode::Normal) {
      if (tLoc.m_phase == TransPhase::Begin) {
        jam();
        DictSignal::addRequestFlags(trans_ptr.p->m_requestInfo, requestInfo);
      }
      else if (tLoc.m_phase == TransPhase::Parse) {
        jam();
        const OpInfo* info = findOpInfo(gsn);
        ndbrequire(info != 0);
        Uint32 length = info->m_impl_req_length;
        Uint32 skip = SchemaTransImplReq::SignalLength;
        ndbrequire(skip + length <= 25);

        Uint32* data = signal->getDataPtrSend();
        Uint32 i = 0;
        for (i = 0; i < length; i++)
          data[i] = data[skip + i];

        recvTransParseReq(signal, trans_ptr, op_key, *info, requestInfo);
      }
      else if (tLoc.m_phase == TransPhase::Prepare) {
        jam();
        runTransSlave(signal, trans_ptr);
      }
      else if (tLoc.m_phase == TransPhase::Commit) {
        jam();
        runTransSlave(signal, trans_ptr);
      }
      else if (tLoc.m_phase == TransPhase::End) {
        jam();
      }
      else {
        ndbrequire(false);
      }
    }
    else if (tLoc.m_mode == TransMode::Rollback) {
      if (tLoc.m_phase == TransPhase::Parse) {
        jam();
        /*
         * Rolling back sub-operation on participant.
         * Non-master removes the op before replying.
         */
        runTransSlave(signal, trans_ptr);
      }
      else {
        ndbrequire(false);
      }
    }
    else if (tLoc.m_mode == TransMode::Abort) {
      if (tLoc.m_phase == TransPhase::Begin) {
        jam();
      }
      else if (tLoc.m_phase == TransPhase::Parse) {
        jam();
        runTransSlave(signal, trans_ptr);
      }
      else if (tLoc.m_phase == TransPhase::Prepare) {
        jam();
        runTransSlave(signal, trans_ptr);
      }
      else {
        ndbrequire(false);
      }
    }
    else {
      ndbrequire(false);
    }

    if (sendConf) {
      jam();
      sendTransConf(signal, trans_ptr);
    }
    if (releaseTrans) {
      jam();
      releaseSchemaTrans(trans_ptr);
    }
    return;
  } while (0);

  SchemaTrans tmp_trans;
  trans_ptr.i = RNIL;
  trans_ptr.p = &tmp_trans;
  trans_ptr.p->trans_key = trans_key;
  trans_ptr.p->m_masterRef = senderRef;
  setError(trans_ptr.p->m_error, error);
  sendTransRef(signal, trans_ptr);
}

/*
 * Parse on master has been done before this request was sent.
 * Create and parse the operation on slave participant.
 */
void
Dbdict::recvTransParseReq(Signal* signal, SchemaTransPtr trans_ptr,
                          Uint32 op_key, const OpInfo& info,
                          Uint32 requestInfo)
{
  SchemaOpPtr op_ptr;
  D("recvTransParseReq");

  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  D("before add op" << *trans_ptr.p << tLoc << tLoc.m_opList);

  // signal data contains impl_req
  const Uint32* src = signal->getDataPtr();
  const Uint32 len = info.m_impl_req_length;

  ndbrequire(op_key != RNIL);
  ErrorInfo error;
  if (trans_ptr.p->m_isMaster) {
    jam();
    // this branch does nothing but is convenient for signal pong
    findSchemaOp(op_ptr, op_key);
    ndbrequire(!op_ptr.isNull());

    OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
    const Uint32* dst = oprec_ptr.p->m_impl_req_data;
    ndbrequire(memcmp(dst, src, len << 2) == 0);
  } else {
    if (seizeSchemaOp(op_ptr, op_key, info)) {
      jam();

      DictSignal::addRequestExtra(op_ptr.p->m_requestInfo, requestInfo);
      DictSignal::addRequestFlags(op_ptr.p->m_requestInfo, requestInfo);

      addSchemaOp(trans_ptr, op_ptr);

      OpRecPtr oprec_ptr = op_ptr.p->m_oprec_ptr;
      Uint32* dst = oprec_ptr.p->m_impl_req_data;
      memcpy(dst, src, len << 2);

      (this->*(info.m_parse))(signal, op_ptr, error);
      if (!hasError(error)) {
        jam();
        updateSchemaOpStep(trans_ptr, op_ptr);
      }
    } else {
      jam();
      setError(error, SchemaTransImplRef::TooManySchemaOps, __LINE__);
    }
  }

  // parse must consume but not release signal sections
  releaseSections(signal);

  if (hasError(error)) {
    jam();
    setError(trans_ptr, error);
    sendTransRef(signal, trans_ptr);
    return;
  }
  sendTransConf(signal, op_ptr);
}

void
Dbdict::runTransSlave(Signal* signal, SchemaTransPtr trans_ptr)
{
  const TransLoc& tLoc = trans_ptr.p->m_transLoc;
  SchemaOpPtr op_ptr;
  getOpPtr(tLoc, op_ptr);

  D("runTransSlave");

  const TransPhaseEntry& phaseEntry = getPhaseEntry(tLoc);
  const bool isMaster = (trans_ptr.p->m_masterRef == reference());

  // master phase on slave does nothing more
  if (!isMaster && (phaseEntry.m_phaseFlags & TransPhaseFlag::Master)) {
      jam();
      sendTransConf(signal, trans_ptr);
      return;
  }

  if (phaseEntry.m_phaseFlags & TransPhaseFlag::Simple) {
    jam();
    ndbrequire(op_ptr.isNull());
    if (tLoc.m_mode == TransMode::Normal) {
      jam();
      ndbrequire(phaseEntry.m_run != 0);
      (this->*(phaseEntry.m_run))(signal, trans_ptr);
    } else {
      jam();
      ndbrequire(phaseEntry.m_abort != 0);
      (this->*(phaseEntry.m_abort))(signal, trans_ptr);
    }
  } else {
    jam();
    ndbrequire(!op_ptr.isNull());
    // wl3600_todo verify op_key
    const OpInfo& info = getOpInfo(op_ptr);
    if (tLoc.m_mode == TransMode::Normal) {
      switch (tLoc.m_phase) {
      case TransPhase::Prepare:
        (this->*(info.m_prepare))(signal, op_ptr);
        break;
      case TransPhase::Commit:
        (this->*(info.m_commit))(signal, op_ptr);
        break;
      default:
        ndbrequire(false);
        break;
      }
    } else {
      switch (tLoc.m_phase) {
      case TransPhase::Parse:
        (this->*(info.m_abortParse))(signal, op_ptr);
        break;
      case TransPhase::Prepare:
        (this->*(info.m_abortPrepare))(signal, op_ptr);
        // mark it in advance
        op_ptr.p->m_abortPrepareDone = true;
        break;
      default:
        ndbrequire(false);
        break;
      }
    }
  }
}

void
Dbdict::sendTransConf(Signal* signal, SchemaOpPtr op_ptr, Uint32 itFlags)
{
  SchemaTransPtr trans_ptr = op_ptr.p->m_trans_ptr;
  TransLoc& tLoc = trans_ptr.p->m_transLoc;

  if (!op_ptr.isNull() &&
      tLoc.m_mode == TransMode::Normal) {
    jam();
    runFeedbackMethod(signal, op_ptr);
  }

  if (tLoc.m_mode == TransMode::Rollback &&
      !trans_ptr.p->m_isMaster) {
    jam();
    iteratorRemoveLastOp(tLoc, op_ptr);
  }

  sendTransConf(signal, trans_ptr, itFlags);
}

void
Dbdict::sendTransConf(Signal* signal, SchemaTransPtr trans_ptr, Uint32 itFlags)
{
  ndbrequire(!trans_ptr.isNull());
  ndbrequire(signal->getNoOfSections() == 0);

  SchemaTransImplConf* conf =
    (SchemaTransImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->transKey = trans_ptr.p->trans_key;
  conf->itFlags = itFlags;

  const Uint32 masterRef = trans_ptr.p->m_masterRef;
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
  sendTransRef(signal, trans_ptr);
}

void
Dbdict::sendTransRef(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("sendTransRef");
  ndbrequire(hasError(trans_ptr.p->m_error));

  // trans is not defined on RT_BEGIN REF
  if (trans_ptr.i != RNIL) {
    const TransLoc& tLoc = trans_ptr.p->m_transLoc;

    if (tLoc.m_mode == TransMode::Normal) {
      switch (tLoc.m_phase) {
      case TransPhase::Commit:
      case TransPhase::End:
        // fail commit must crash
        ndbrequire(false);
        break;
      default:
        break;
      }
    } else {
      // fail abort must crash
      ndbrequire(false);
    }
  }

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

// reply to trans client for begin/end trans

void
Dbdict::sendTransClientReply(Signal* signal, SchemaTransPtr trans_ptr)
{
  const Uint32 clientFlags = trans_ptr.p->m_clientFlags;
  D("sendTransClientReply" << hex << V(clientFlags));
  D("c_rope_pool free: " << c_rope_pool.getNoOfFree());

  Uint32 receiverRef = 0;
  Uint32 transId = 0;
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
      sendSignal(receiverRef, GSN_SCHEMA_TRANS_END_REF, signal,
                 SchemaTransEndRef::SignalLength, JBB);
    }
    resetError(trans_ptr);
    trans_ptr.p->m_clientState = TransClient::EndReply;
    return;
  }

  ndbrequire(false);
}

/*
 * Slave node failure.  If we are waiting for reply then we get it
 * via NF_FakeErrorREF.  Otherwise we get it only via NODE_FAILREP.
 * This second case is handled here.
 */

void
Dbdict::handleTransSlaveFail(Signal* signal, Uint32 failedNode)
{
  D("handleTransSlaveFail" << V(failedNode));
  ndbrequire(failedNode < MAX_NDB_NODES);

  SchemaTransPtr trans_ptr;
  c_schemaTransList.first(trans_ptr);
  while (trans_ptr.i != RNIL) {
    jam();
    const TransGlob& tGlob = getTransGlob(trans_ptr, failedNode);
    D("check" << *trans_ptr.p << tGlob);

    switch (tGlob.state()) {
    case TransGlob::Undef:
      D("not part of this trans");
      break;
    case TransGlob::NodeFail:
      D("node failure seen already");
      ndbrequire(trans_ptr.p->m_failedNodes.get(failedNode));
      break;
    default:
      setGlobState(trans_ptr, failedNode, TransGlob::NodeFail);
      trans_ptr.p->m_failedNodes.set(failedNode);
      break;
    }

    c_schemaTransList.next(trans_ptr);
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
  Uint32 tx_key = tx_ptr.p->tx_key;
  D("releaseTxHandle" << V(tx_key));

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
                      Uint32 failedApiNode,
                      BlockReference retRef)
{
  D("handleApiFail" << V(failedApiNode));

  Uint32 takeOvers = 0;
  SchemaTransPtr trans_ptr;
  c_schemaTransList.first(trans_ptr);
  while (trans_ptr.i != RNIL) {
    jam();
    const TransLoc& tLoc = trans_ptr.p->m_transLoc;
    D("check" << *trans_ptr.p << tLoc << tLoc.m_opList);
    Uint32 clientRef = trans_ptr.p->m_clientRef;

    if (refToNode(clientRef) == failedApiNode) {
      jam();
      D("failed" << hex << V(clientRef));

      ndbrequire(!(trans_ptr.p->m_clientFlags & TransClient::ApiFail));
      trans_ptr.p->m_clientFlags |= TransClient::ApiFail;

      if (trans_ptr.p->m_isMaster) {
        jam();
        if (trans_ptr.p->m_clientFlags & TransClient::TakeOver) {
          // maybe already running in background
          jam();
          ndbrequire(trans_ptr.p->m_clientFlags & TransClient::Background);
        } else {
          takeOverTransClient(signal, trans_ptr);
        }

        TxHandlePtr tx_ptr;
        bool ok = findTxHandle(tx_ptr, trans_ptr.p->m_takeOverTxKey);
        ndbrequire(ok);

        tx_ptr.p->m_clientFlags |= TransClient::ApiFail;
        tx_ptr.p->m_apiFailRetRef = retRef;
        takeOvers++;
      }
    }

    c_schemaTransList.next(trans_ptr);
  }

  D("handleApiFail" << V(takeOvers));

  if (takeOvers == 0) {
    jam();
    sendApiFailConf(signal, failedApiNode, retRef);
  }
}

/*
 * Take over client trans, either by background request or at API
 * node failure.  Continue to the callback routine.
 */
void
Dbdict::takeOverTransClient(Signal* signal, SchemaTransPtr trans_ptr)
{
  D("takeOverClientTrans" << *trans_ptr.p);

  TxHandlePtr tx_ptr;
  bool ok = seizeTxHandle(tx_ptr);
  ndbrequire(ok);

  Uint32 clientFlags = trans_ptr.p->m_clientFlags;
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
    Uint32 retRef = tx_ptr.p->m_apiFailRetRef;
    sendApiFailConf(signal, failedApiNode, retRef);
  }
}

void
Dbdict::sendApiFailConf(Signal* signal,
                        Uint32 failedApiNode,
                        BlockReference retRef)
{
  signal->theData[0] = failedApiNode;
  signal->theData[1] = reference();
  sendSignal(retRef, GSN_API_FAILCONF, signal, 2, JBB);
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

// MODULE: debug

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
  out << " name:" << dict->copyRope<MAX_TAB_NAME_SIZE>(m_name);
  out << dec << V(m_ref_count);
  out << dec << V(m_trans_key);
  out << dec << V(m_op_ref_count);
  out << ")";
}

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
  out << " (ErrorInfo";
  out << dec << V(errorCode);
  out << dec << V(errorLine);
  out << dec << V(errorNodeId);
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
  Dbdict* dict = (Dbdict*)globalData.getBlock(DBDICT);
  const Dbdict::OpInfo& info = m_oprec_ptr.p->m_opInfo;
  out << " (SchemaOp";
  out << " " << info.m_opType;
  out << dec << V(op_key);
  out << dec << V(m_opIndex);
  out << dec << V(m_opDepth);
  if (m_error.errorCode != 0)
    out << m_error;
  if (m_sections != 0)
    out << dec << V(m_sections);
  out << ")";
}

// OpList

NdbOut&
operator<<(NdbOut& out, const Dbdict::OpList& a)
{
  a.print(out);
  return out;
}

void
Dbdict::OpList::print(NdbOut& out) const
{
  Dbdict* dict = (Dbdict*)globalData.getBlock(DBDICT);
  Dbdict::OpList a_copy = *this;
  LocalDLFifoList<SchemaOp> list(dict->c_schemaOpPool, a_copy.m_head);
  Dbdict::SchemaOpPtr op_ptr;
  out << " (OpList";
  Uint32 depth = 0;
  out << " [";
  list.first(op_ptr);
  while (!op_ptr.isNull()) {
    if (op_ptr.p->m_opDepth > depth) {
      assert(op_ptr.p->m_opDepth > depth);
      out << " [";
    }
    if (op_ptr.p->m_opDepth < depth) {
      Uint32 i;
      for (i = op_ptr.p->m_opDepth; i < depth; i++)
        out << "]";
    }
    const Dbdict::OpInfo& info = dict->getOpInfo(op_ptr);
    out << " " << dec << op_ptr.p->op_key << "-" << info.m_opType;
    depth = op_ptr.p->m_opDepth;
    list.next(op_ptr);
  }
  while (depth != 0) {
    out << "]";
    depth--;
  }
  out << "]";
  out << ")";
}

// TransLoc

NdbOut&
operator<<(NdbOut& out, const Dbdict::TransLoc& a)
{
  a.print(out);
  return out;
}

void
Dbdict::TransLoc::print(NdbOut& out) const
{
  out << " (TransLoc";
  out << dec << V(m_mode)
      << " [" << DictSignal::getTransModeName(m_mode) << "]";
  out << dec << V(m_phaseIndex);
  out << dec << V(m_phase)
      << " [" << DictSignal::getTransPhaseName(m_phase) << "]";
  out << dec << V(m_subphase)
      << " [" << DictSignal::getTransSubphaseName(m_subphase) << "]";
  out << dec << V(m_repeat);
  out << dec << V(m_opKey);
  out << ")";
}

// TransGlob

NdbOut&
operator<<(NdbOut& out, const Dbdict::TransGlob& a)
{
  a.print(out);
  return out;
}

void
Dbdict::TransGlob::print(NdbOut& out) const
{
  out << " ";
  out << dec << (Uint32)m_state;
  if (m_flags != 0)
    out << dec << "," << m_flags;
  out << " [" << DictSignal::getTransStateName(m_state) << "]";
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
  out << dec << V(trans_key);
  out << dec << V(m_isMaster);
  out << hex << V(m_clientRef);
  out << hex << V(m_transId);
  out << dec << V(m_clientState);
  out << hex << V(m_clientFlags);
  out << dec << V(m_opDepth);
  out << " State";
  Uint32 i;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    if (m_initNodes.get(i)) {
      out << dec << " " << i << ":";
      TransGlob::State state = m_transGlob[i].state();
      out << DictSignal::getTransStateName(state);
    }
  }
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

#endif
