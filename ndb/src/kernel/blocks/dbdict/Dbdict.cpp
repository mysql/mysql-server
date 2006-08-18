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

#include <ndb_global.h>
#include <my_sys.h>

#define DBDICT_C
#include "Dbdict.hpp"

#include <ndb_limits.h>
#include <NdbOut.hpp>
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

#include <signaldata/CreateEvnt.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilRelease.hpp>
#include <signaldata/SumaImpl.hpp> 
#include <GrepError.hpp>
//#include <signaldata/DropEvnt.hpp>

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

#define ZNOT_FOUND 626
#define ZALREADYEXIST 630

//#define EVENT_PH2_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG

#define EVENT_TRACE \
//  ndbout_c("Event debug trace: File: %s Line: %u", __FILE__, __LINE__)

#define DIV(x,y) (((x)+(y)-1)/(y))
#include <ndb_version.h>

static
Uint32
alter_table_inc_schema_version(Uint32 old)
{
   return (old & 0x00FFFFFF) + ((old + 0x1000000) & 0xFF000000);
}

static
Uint32
alter_table_dec_schema_version(Uint32 old)
{
  return (old & 0x00FFFFFF) + ((old - 0x1000000) & 0xFF000000);
}

static
Uint32
create_table_inc_schema_version(Uint32 old)
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
    packTableIntoPages(signal, signal->theData[1], signal->theData[2]);
    break;

  case ZSEND_GET_TAB_RESPONSE :
    jam();
    sendGetTabResponse(signal);
    break;

  case ZDICT_LOCK_POLL:
    jam();
    checkDictLockQueue(signal, true);
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

void Dbdict::packTableIntoPages(Signal* signal, Uint32 tableId, Uint32 pageId)
{

  PageRecordPtr pagePtr;
  TableRecordPtr tablePtr;
  c_pageRecordArray.getPtr(pagePtr, pageId);
  
  memset(&pagePtr.p->word[0], 0, 4 * ZPAGE_HEADER_SIZE);
  c_tableRecordPool.getPtr(tablePtr, tableId);
  LinearWriter w(&pagePtr.p->word[ZPAGE_HEADER_SIZE], 
		 8 * ZSIZE_OF_PAGES_IN_WORDS);

  w.first();
  packTableIntoPagesImpl(w, tablePtr, signal);
    
  Uint32 wordsOfTable = w.getWordsUsed();
  Uint32 pagesUsed = 
    DIV(wordsOfTable + ZPAGE_HEADER_SIZE, ZSIZE_OF_PAGES_IN_WORDS);
  pagePtr.p->word[ZPOS_CHECKSUM] = 
    computeChecksum(&pagePtr.p->word[0], pagesUsed * ZSIZE_OF_PAGES_IN_WORDS);
  
  switch (c_packTable.m_state) {
  case PackTable::PTS_IDLE:
  case PackTable::PTS_ADD_TABLE_MASTER:
  case PackTable::PTS_ADD_TABLE_SLAVE:
  case PackTable::PTS_RESTART:
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
Dbdict::packTableIntoPagesImpl(SimpleProperties::Writer & w,
			       TableRecordPtr tablePtr,
			       Signal* signal){
  
  w.add(DictTabInfo::TableName, tablePtr.p->tableName);
  w.add(DictTabInfo::TableId, tablePtr.i);
#ifdef HAVE_TABLE_REORG
  w.add(DictTabInfo::SecondTableId, tablePtr.p->secondTable);
#else
  w.add(DictTabInfo::SecondTableId, (Uint32)0);
#endif
  w.add(DictTabInfo::TableVersion, tablePtr.p->tableVersion);
  w.add(DictTabInfo::NoOfKeyAttr, tablePtr.p->noOfPrimkey);
  w.add(DictTabInfo::NoOfAttributes, tablePtr.p->noOfAttributes);
  w.add(DictTabInfo::NoOfNullable, tablePtr.p->noOfNullAttr);
  w.add(DictTabInfo::NoOfVariable, (Uint32)0);
  w.add(DictTabInfo::KeyLength, tablePtr.p->tupKeyLength);
  
  w.add(DictTabInfo::TableLoggedFlag, tablePtr.p->storedTable);
  w.add(DictTabInfo::MinLoadFactor, tablePtr.p->minLoadFactor);
  w.add(DictTabInfo::MaxLoadFactor, tablePtr.p->maxLoadFactor);
  w.add(DictTabInfo::TableKValue, tablePtr.p->kValue);
  w.add(DictTabInfo::FragmentTypeVal, tablePtr.p->fragmentType);
  w.add(DictTabInfo::TableTypeVal, tablePtr.p->tableType);
  w.add(DictTabInfo::MaxRowsLow, tablePtr.p->maxRowsLow);
  w.add(DictTabInfo::MaxRowsHigh, tablePtr.p->maxRowsHigh);
  w.add(DictTabInfo::MinRowsLow, tablePtr.p->minRowsLow);
  w.add(DictTabInfo::MinRowsHigh, tablePtr.p->minRowsHigh);
  
  if(!signal)
  {
    w.add(DictTabInfo::FragmentCount, tablePtr.p->fragmentCount);
  }
  else
  {
    Uint32 * theData = signal->getDataPtrSend();
    CreateFragmentationReq * const req = (CreateFragmentationReq*)theData;
    req->senderRef = 0;
    req->senderData = RNIL;
    req->fragmentationType = tablePtr.p->fragmentType;
    req->noOfFragments = 0;
    req->fragmentNode = 0;
    req->primaryTableId = tablePtr.i;
    EXECUTE_DIRECT(DBDIH, GSN_CREATE_FRAGMENTATION_REQ, signal,
		   CreateFragmentationReq::SignalLength);
    if(signal->theData[0] == 0)
    {
      Uint16 *data = (Uint16*)&signal->theData[25];
      Uint32 count = 2 + data[0] * data[1];
      w.add(DictTabInfo::FragmentDataLen, 2*count);
      w.add(DictTabInfo::FragmentData, data, 2*count);
    }
  }
  
  if (tablePtr.p->primaryTableId != RNIL){
    TableRecordPtr primTab;
    c_tableRecordPool.getPtr(primTab, tablePtr.p->primaryTableId);
    w.add(DictTabInfo::PrimaryTable, primTab.p->tableName);
    w.add(DictTabInfo::PrimaryTableId, tablePtr.p->primaryTableId);
    w.add(DictTabInfo::IndexState, tablePtr.p->indexState);
    w.add(DictTabInfo::InsertTriggerId, tablePtr.p->insertTriggerId);
    w.add(DictTabInfo::UpdateTriggerId, tablePtr.p->updateTriggerId);
    w.add(DictTabInfo::DeleteTriggerId, tablePtr.p->deleteTriggerId);
    w.add(DictTabInfo::CustomTriggerId, tablePtr.p->customTriggerId);
  }
  w.add(DictTabInfo::FrmLen, tablePtr.p->frmLen);
  w.add(DictTabInfo::FrmData, tablePtr.p->frmData, tablePtr.p->frmLen);
  
  Uint32 nextAttribute = tablePtr.p->firstAttribute;
  AttributeRecordPtr attrPtr;
  do {
    jam();
    c_attributeRecordPool.getPtr(attrPtr, nextAttribute);
    
    w.add(DictTabInfo::AttributeName, attrPtr.p->attributeName);
    w.add(DictTabInfo::AttributeId, attrPtr.p->attributeId);
    w.add(DictTabInfo::AttributeKeyFlag, attrPtr.p->tupleKey > 0);
    
    const Uint32 desc = attrPtr.p->attributeDescriptor;
    const Uint32 attrType = AttributeDescriptor::getType(desc);
    const Uint32 attrSize = AttributeDescriptor::getSize(desc);
    const Uint32 arraySize = AttributeDescriptor::getArraySize(desc);
    const Uint32 nullable = AttributeDescriptor::getNullable(desc);
    const Uint32 DKey = AttributeDescriptor::getDKey(desc);

    // AttributeType deprecated
    w.add(DictTabInfo::AttributeSize, attrSize);
    w.add(DictTabInfo::AttributeArraySize, arraySize);
    w.add(DictTabInfo::AttributeNullableFlag, nullable);
    w.add(DictTabInfo::AttributeDKey, DKey);
    w.add(DictTabInfo::AttributeExtType, attrType);
    w.add(DictTabInfo::AttributeExtPrecision, attrPtr.p->extPrecision);
    w.add(DictTabInfo::AttributeExtScale, attrPtr.p->extScale);
    w.add(DictTabInfo::AttributeExtLength, attrPtr.p->extLength);
    w.add(DictTabInfo::AttributeAutoIncrement, 
	  (Uint32)attrPtr.p->autoIncrement);
    w.add(DictTabInfo::AttributeDefaultValue, attrPtr.p->defaultValue);
    
    w.add(DictTabInfo::AttributeEnd, 1);
    nextAttribute = attrPtr.p->nextAttrInTable;
  } while (nextAttribute != RNIL);
  
  w.add(DictTabInfo::TableEnd, 1);
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
  
  Uint32 sz = tabInfoPtr.sz + ZPAGE_HEADER_SIZE;

  c_writeTableRecord.noOfPages = DIV(sz, ZSIZE_OF_PAGES_IN_WORDS);
  c_writeTableRecord.tableWriteState = WriteTableRecord::TWR_CALLBACK;
  c_writeTableRecord.m_callback = * callback;

  c_writeTableRecord.pageId = 0;
  ndbrequire(c_writeTableRecord.noOfPages < 8);

  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_writeTableRecord.pageId);
  copy(&pageRecPtr.p->word[ZPAGE_HEADER_SIZE], tabInfoPtr);
  
  memset(&pageRecPtr.p->word[0], 0, 4 * ZPAGE_HEADER_SIZE);
  pageRecPtr.p->word[ZPOS_CHECKSUM] = 
    computeChecksum(&pageRecPtr.p->word[0], 
		    c_writeTableRecord.noOfPages * ZSIZE_OF_PAGES_IN_WORDS);
  
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
  TableRecordPtr tablePtr;
  FsOpenReq * const fsOpenReq = (FsOpenReq *)&signal->theData[0];
  c_tableRecordPool.getPtr(tablePtr, tableId);

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
  FsOpenReq::v1_setS(fsOpenReq->fileNumber, tablePtr.p->tableVersion);
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
  fsRWReq->numberOfPages = c_writeTableRecord.noOfPages;
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
  fsRWReq->numberOfPages = c_readTableRecord.noOfPages;
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
  Uint32 sz = c_readTableRecord.noOfPages * ZSIZE_OF_PAGES_IN_WORDS;
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
  fsPtr.p->fsState = FsConnectRecord::OPEN_READ_TAB_FILE2;
  openTableFile(signal, 1, fsPtr.i, c_readTableRecord.tableId, false);
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
void
Dbdict::updateSchemaState(Signal* signal, Uint32 tableId, 
			  SchemaFile::TableEntry* te, Callback* callback){

  jam();
  ndbrequire(tableId < c_tableRecordPool.getSize());
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tableId);
  
  SchemaFile::TableState newState = 
    (SchemaFile::TableState)te->m_tableState;
  SchemaFile::TableState oldState = 
    (SchemaFile::TableState)tableEntry->m_tableState;
  
  Uint32 newVersion = te->m_tableVersion;
  Uint32 oldVersion = tableEntry->m_tableVersion;
  
  bool ok = false;
  switch(newState){
  case SchemaFile::ADD_STARTED:
    jam();
    ok = true;
    ndbrequire(create_table_inc_schema_version(oldVersion) == newVersion);
    ndbrequire(oldState == SchemaFile::INIT ||
	       oldState == SchemaFile::DROP_TABLE_COMMITTED);
    break;
  case SchemaFile::TABLE_ADD_COMMITTED:
    jam();
    ok = true;
    ndbrequire(newVersion == oldVersion);
    ndbrequire(oldState == SchemaFile::ADD_STARTED);
    break;
  case SchemaFile::ALTER_TABLE_COMMITTED:
    jam();
    ok = true;
    ndbrequire(alter_table_inc_schema_version(oldVersion) == newVersion);
    ndbrequire(oldState == SchemaFile::TABLE_ADD_COMMITTED ||
	       oldState == SchemaFile::ALTER_TABLE_COMMITTED);
    break;
  case SchemaFile::DROP_TABLE_STARTED:
    jam();
  case SchemaFile::DROP_TABLE_COMMITTED:
    jam();
    ok = true;
    ndbrequire(false);
    break;
  case SchemaFile::INIT:
    jam();
    ok = true;
    ndbrequire((oldState == SchemaFile::ADD_STARTED));
  }//if
  ndbrequire(ok);
  
  * tableEntry = * te;
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);

  ndbrequire(c_writeSchemaRecord.inUse == false);
  c_writeSchemaRecord.inUse = true;
  
  c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
  c_writeSchemaRecord.newFile = false;
  c_writeSchemaRecord.firstPage = tableId / NDB_SF_PAGE_ENTRIES;
  c_writeSchemaRecord.noOfPages = 1;
  c_writeSchemaRecord.m_callback = * callback;

  startWriteSchemaFile(signal);
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
    bool ok =
      memcmp(sf->Magic, NDB_SF_MAGIC, sizeof(sf->Magic)) == 0 &&
      sf->FileSize != 0 &&
      sf->FileSize % NDB_SF_PAGE_SIZE == 0 &&
      sf->FileSize == sf0->FileSize &&
      sf->PageNumber == n &&
      computeChecksum((Uint32*)sf, NDB_SF_PAGE_SIZE_IN_WORDS) == 0;
    ndbrequire(ok || !crashInd);
    if (! ok) {
      jam();
      ndbrequire(fsPtr.p->fsState == FsConnectRecord::READ_SCHEMA1);
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

Dbdict::Dbdict(const class Configuration & conf):
  SimulatedBlock(DBDICT, conf),
  c_tableRecordHash(c_tableRecordPool),
  c_attributeRecordHash(c_attributeRecordPool),
  c_triggerRecordHash(c_triggerRecordPool),
  c_opCreateTable(c_opRecordPool),
  c_opDropTable(c_opRecordPool),
  c_opCreateIndex(c_opRecordPool),
  c_opDropIndex(c_opRecordPool),
  c_opAlterIndex(c_opRecordPool),
  c_opBuildIndex(c_opRecordPool),
  c_opCreateTrigger(c_opRecordPool),
  c_opDropTrigger(c_opRecordPool),
  c_opAlterTrigger(c_opRecordPool),
  c_opRecordSequence(0),
  c_dictLockQueue(c_dictLockPool),
  c_dictLockPoll(false)
{
  BLOCK_CONSTRUCTOR(Dbdict);
  
  const ndb_mgm_configuration_iterator * p = conf.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, &c_maxNoOfTriggers);
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
  addRecSignal(GSN_CREATE_FRAGMENTATION_REF, &Dbdict::execCREATE_FRAGMENTATION_REF);
  addRecSignal(GSN_CREATE_FRAGMENTATION_CONF, &Dbdict::execCREATE_FRAGMENTATION_CONF);
  addRecSignal(GSN_DIADDTABCONF, &Dbdict::execDIADDTABCONF);
  addRecSignal(GSN_DIADDTABREF, &Dbdict::execDIADDTABREF);
  addRecSignal(GSN_ADD_FRAGREQ, &Dbdict::execADD_FRAGREQ);
  addRecSignal(GSN_TAB_COMMITCONF, &Dbdict::execTAB_COMMITCONF);
  addRecSignal(GSN_TAB_COMMITREF, &Dbdict::execTAB_COMMITREF);
  addRecSignal(GSN_ALTER_TABLE_REQ, &Dbdict::execALTER_TABLE_REQ);
  addRecSignal(GSN_ALTER_TAB_REQ, &Dbdict::execALTER_TAB_REQ);
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
  
  addRecSignal(GSN_DROP_TAB_REQ, &Dbdict::execDROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REF, &Dbdict::execDROP_TAB_REF);
  addRecSignal(GSN_DROP_TAB_CONF, &Dbdict::execDROP_TAB_CONF);

  addRecSignal(GSN_BACKUP_FRAGMENT_REQ, &Dbdict::execBACKUP_FRAGMENT_REQ);

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
  c_blockState = BS_IDLE;
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
  c_readTableRecord.noOfPages = (Uint32)-1;
  c_readTableRecord.pageId = RNIL;
  c_readTableRecord.tableId = ZNIL;
  c_readTableRecord.inUse = false;
}//initReadTableRecord()

void Dbdict::initWriteTableRecord() 
{
  c_writeTableRecord.noOfPages = (Uint32)-1;
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
}//Dbdict::initRestartRecord()

void Dbdict::initNodeRecords() 
{
  jam();
  for (unsigned i = 1; i < MAX_NODES; i++) {
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
  tablePtr.p->activePage = RNIL;
  tablePtr.p->filePtr[0] = RNIL;
  tablePtr.p->filePtr[1] = RNIL;
  tablePtr.p->firstAttribute = RNIL;
  tablePtr.p->firstPage = RNIL;
  tablePtr.p->lastAttribute = RNIL;
  tablePtr.p->tableId = tablePtr.i;
  tablePtr.p->tableVersion = (Uint32)-1;
  tablePtr.p->tabState = TableRecord::NOT_DEFINED;
  tablePtr.p->tabReturnState = TableRecord::TRS_IDLE;
  tablePtr.p->fragmentType = DictTabInfo::AllNodesSmallTable;
  memset(tablePtr.p->tableName, 0, sizeof(tablePtr.p->tableName));
  tablePtr.p->gciTableCreated = 0;
  tablePtr.p->noOfAttributes = ZNIL;
  tablePtr.p->noOfNullAttr = 0;
  tablePtr.p->frmLen = 0;
  memset(tablePtr.p->frmData, 0, sizeof(tablePtr.p->frmData));
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
  tablePtr.p->minRowsLow = 0;
  tablePtr.p->minRowsHigh = 0;
  tablePtr.p->storedTable = true;
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
  triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;
  triggerPtr.p->triggerLocal = 0;
  memset(triggerPtr.p->triggerName, 0, sizeof(triggerPtr.p->triggerName));
  triggerPtr.p->triggerId = RNIL;
  triggerPtr.p->tableId = RNIL;
  triggerPtr.p->triggerType = (TriggerType::Value)~0;
  triggerPtr.p->triggerActionTime = (TriggerActionTime::Value)~0;
  triggerPtr.p->triggerEvent = (TriggerEvent::Value)~0;
  triggerPtr.p->monitorReplicas = false;
  triggerPtr.p->monitorAllAttributes = false;
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

Uint32 Dbdict::getFreeTableRecord(Uint32 primaryTableId) 
{
  Uint32 minId = (primaryTableId == RNIL ? 0 : primaryTableId + 1);
  TableRecordPtr tablePtr;
  TableRecordPtr firstTablePtr;
  bool firstFound = false;
  Uint32 tabSize = c_tableRecordPool.getSize();
  for (tablePtr.i = minId; tablePtr.i < tabSize ; tablePtr.i++) {
    jam();
    c_tableRecordPool.getPtr(tablePtr);
    if (tablePtr.p->tabState == TableRecord::NOT_DEFINED) {
      jam();
      initialiseTableRecord(tablePtr);
      tablePtr.p->tabState = TableRecord::DEFINING;
      firstFound = true;
      firstTablePtr.i = tablePtr.i;
      firstTablePtr.p = tablePtr.p;
      break;
    }//if
  }//for
  if (!firstFound) {
    jam();
    return RNIL;
  }//if
#ifdef HAVE_TABLE_REORG
  bool secondFound = false;
  for (tablePtr.i = firstTablePtr.i + 1; tablePtr.i < tabSize ; tablePtr.i++) {
    jam();
    c_tableRecordPool.getPtr(tablePtr);
    if (tablePtr.p->tabState == TableRecord::NOT_DEFINED) {
      jam();
      initialiseTableRecord(tablePtr);
      tablePtr.p->tabState = TableRecord::REORG_TABLE_PREPARED;
      tablePtr.p->secondTable = firstTablePtr.i;
      firstTablePtr.p->secondTable = tablePtr.i;
      secondFound = true;
      break;
    }//if
  }//for
  if (!secondFound) {
    jam();
    firstTablePtr.p->tabState = TableRecord::NOT_DEFINED;
    return RNIL;
  }//if
#endif
  return firstTablePtr.i;
}//Dbdict::getFreeTableRecord()

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

bool
Dbdict::getNewAttributeRecord(TableRecordPtr tablePtr, 
			      AttributeRecordPtr & attrPtr) 
{
  c_attributeRecordPool.seize(attrPtr);
  if(attrPtr.i == RNIL){
    return false;
  }
  
  memset(attrPtr.p->attributeName, 0, sizeof(attrPtr.p->attributeName));
  attrPtr.p->attributeDescriptor = 0x00012255; //Default value
  attrPtr.p->attributeId = ZNIL;
  attrPtr.p->nextAttrInTable = RNIL;
  attrPtr.p->tupleKey = 0;
  memset(attrPtr.p->defaultValue, 0, sizeof(attrPtr.p->defaultValue));
  
  /* ---------------------------------------------------------------- */
  // A free attribute record has been acquired. We will now link it
  // to the table record.
  /* ---------------------------------------------------------------- */
  if (tablePtr.p->lastAttribute == RNIL) {
    jam();
    tablePtr.p->firstAttribute = attrPtr.i;
  } else {
    jam();
    AttributeRecordPtr lastAttrPtr;
    c_attributeRecordPool.getPtr(lastAttrPtr, tablePtr.p->lastAttribute);
    lastAttrPtr.p->nextAttrInTable = attrPtr.i;
  }//if
  tablePtr.p->lastAttribute = attrPtr.i;
  return true;
}//Dbdict::getNewAttributeRecord()

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
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  Uint32 attributesize, tablerecSize;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_ATTRIBUTE,&attributesize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &tablerecSize));

  c_attributeRecordPool.setSize(attributesize);
  c_attributeRecordHash.setSize(64);
  c_fsConnectRecordPool.setSize(ZFS_CONNECT_SIZE);
  c_nodes.setSize(MAX_NODES);
  c_pageRecordArray.setSize(ZNUMBER_OF_PAGES);
  c_schemaPageRecordArray.setSize(2 * NDB_SF_MAX_PAGES);
  c_tableRecordPool.setSize(tablerecSize);
  c_tableRecordHash.setSize(tablerecSize);
  g_key_descriptor_pool.setSize(tablerecSize);
  c_triggerRecordPool.setSize(c_maxNoOfTriggers);
  c_triggerRecordHash.setSize(c_maxNoOfTriggers);
  c_opRecordPool.setSize(256);   // XXX need config params
  c_opCreateTable.setSize(8);
  c_opDropTable.setSize(8);
  c_opCreateIndex.setSize(8);
  c_opDropIndex.setSize(8);
  c_opAlterIndex.setSize(8);
  c_opBuildIndex.setSize(8);
  c_opCreateTrigger.setSize(8);
  c_opDropTrigger.setSize(8);
  c_opAlterTrigger.setSize(8);

  c_dictLockPool.setSize(32);
  
  // Initialize schema file copies
  c_schemaFile[0].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(0 * NDB_SF_MAX_PAGES);
  c_schemaFile[0].noOfPages = 0;
  c_schemaFile[1].schemaPage =
    (SchemaFile*)c_schemaPageRecordArray.getPtr(1 * NDB_SF_MAX_PAGES);
  c_schemaFile[1].noOfPages = 0;

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

    if (NodeBitmask::get(readNodes->allNodes, i)) {
      jam();
      nodePtr.p->nodeState = NodeRecord::NDB_NODE_ALIVE;
      if (NodeBitmask::get(readNodes->inactiveNodes, i)) {
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
    if (NodeBitmask::get(hotSpare->theHotSpareNodes, i)) {
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
    if (indexPtr.p->storedTable) {
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
  c_restartRecord.activeTable = 0;
  checkSchemaStatus(signal);
}//execSCHEMA_INFO()

void
Dbdict::restart_checkSchemaStatusComplete(Signal * signal, 
					  Uint32 callbackData,
					  Uint32 returnCode){

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
  
  startWriteSchemaFile(signal);
}

void
Dbdict::restart_writeSchemaConf(Signal * signal, 
				Uint32 callbackData,
				Uint32 returnCode){

  if(c_systemRestart){
    jam();
    signal->theData[0] = getOwnNodeId();
    sendSignal(calcDictBlockRef(c_masterNodeId), GSN_SCHEMA_INFOCONF,
	       signal, 1, JBB);
    return;
  }
  
  ndbrequire(c_nodeRestart || c_initialNodeRestart);
  c_blockState = BS_IDLE;
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
    SchemaFile::TableState schemaState = 
      (SchemaFile::TableState)newEntry->m_tableState;
    SchemaFile::TableState oldSchemaState = 
      (SchemaFile::TableState)oldEntry->m_tableState;

    if (c_restartRecord.activeTable >= c_tableRecordPool.getSize()) {
      jam();
      ndbrequire(schemaState == SchemaFile::INIT);
      ndbrequire(oldSchemaState == SchemaFile::INIT);
      continue;
    }//if

    switch(schemaState){
    case SchemaFile::INIT:{
      jam();
      bool ok = false;
      switch(oldSchemaState) {
      case SchemaFile::INIT:
	jam();
      case SchemaFile::DROP_TABLE_COMMITTED:
	jam();
	ok = true;
        jam();
	break;

      case SchemaFile::ADD_STARTED:
	jam();
      case SchemaFile::TABLE_ADD_COMMITTED:
	jam();
      case SchemaFile::DROP_TABLE_STARTED:
	jam();
      case SchemaFile::ALTER_TABLE_COMMITTED:
	jam();
	ok = true;
        jam();
	newEntry->m_tableState = SchemaFile::INIT;
	restartDropTab(signal, tableId);
	return;
      }//switch
      ndbrequire(ok);
      break;
    }
    case SchemaFile::ADD_STARTED:{
      jam();
      bool ok = false;
      switch(oldSchemaState) {
      case SchemaFile::INIT:
	jam();
      case SchemaFile::DROP_TABLE_COMMITTED:
	jam();
	ok = true;
	break;
      case SchemaFile::ADD_STARTED: 
	jam();
      case SchemaFile::DROP_TABLE_STARTED:
	jam();
      case SchemaFile::TABLE_ADD_COMMITTED:
	jam();
      case SchemaFile::ALTER_TABLE_COMMITTED:
	jam();
	ok = true;
	//------------------------------------------------------------------
	// Add Table was started but not completed. Will be dropped in all
	// nodes. Update schema information (restore table version).
	//------------------------------------------------------------------
	newEntry->m_tableState = SchemaFile::INIT;
	restartDropTab(signal, tableId);
	return;
      }
      ndbrequire(ok);
      break;
    }
    case SchemaFile::TABLE_ADD_COMMITTED:{
      jam();
      bool ok = false;
      switch(oldSchemaState) {
      case SchemaFile::INIT:
	jam();
      case SchemaFile::ADD_STARTED:
	jam();
      case SchemaFile::DROP_TABLE_STARTED:
	jam();
      case SchemaFile::DROP_TABLE_COMMITTED:
	jam();
	ok = true;
	//------------------------------------------------------------------
	// Table was added in the master node but not in our node. We can
	// retrieve the table definition from the master.
	//------------------------------------------------------------------
	restartCreateTab(signal, tableId, oldEntry, false);
        return;
        break;
      case SchemaFile::TABLE_ADD_COMMITTED:
	jam();
      case SchemaFile::ALTER_TABLE_COMMITTED:
        jam();
	ok = true;
	//------------------------------------------------------------------
	// Table was added in both our node and the master node. We can
	// retrieve the table definition from our own disk.
	//------------------------------------------------------------------
	if(* newEntry == * oldEntry){
          jam();
	  
          TableRecordPtr tablePtr;
          c_tableRecordPool.getPtr(tablePtr, tableId);
          tablePtr.p->tableVersion = oldEntry->m_tableVersion;
          tablePtr.p->tableType = (DictTabInfo::TableType)oldEntry->m_tableType;
	  
          // On NR get index from master because index state is not on file
          const bool file = c_systemRestart || tablePtr.p->isTable();
          restartCreateTab(signal, tableId, oldEntry, file);

          return;
        } else {
	  //------------------------------------------------------------------
	  // Must be a new version of the table if anything differs. Both table
	  // version and global checkpoint must be different.
	  // This should not happen for the master node. This can happen after
	  // drop table followed by add table or after change table.
	  // Not supported in this version.
	  //------------------------------------------------------------------
          ndbrequire(c_masterNodeId != getOwnNodeId());
	  ndbrequire(newEntry->m_tableVersion != oldEntry->m_tableVersion);
          jam();
	  
	  restartCreateTab(signal, tableId, oldEntry, false);
          return;
        }//if
      }
      ndbrequire(ok);
      break;
    }
    case SchemaFile::DROP_TABLE_STARTED:
      jam();
    case SchemaFile::DROP_TABLE_COMMITTED:{
      jam();
      bool ok = false;
      switch(oldSchemaState){
      case SchemaFile::INIT:
	jam();
      case SchemaFile::DROP_TABLE_COMMITTED:
	jam();
	ok = true;
	break;
      case SchemaFile::ADD_STARTED:
	jam();
      case SchemaFile::TABLE_ADD_COMMITTED:
	jam();
      case SchemaFile::DROP_TABLE_STARTED:
	jam();
      case SchemaFile::ALTER_TABLE_COMMITTED:
	jam();
	newEntry->m_tableState = SchemaFile::INIT;
	restartDropTab(signal, tableId);
	return;
      }
      ndbrequire(ok);
      break;
    }
    case SchemaFile::ALTER_TABLE_COMMITTED: {
      jam();
      bool ok = false;
      switch(oldSchemaState) {
      case SchemaFile::INIT:
	jam();
      case SchemaFile::ADD_STARTED:
	jam();
      case SchemaFile::DROP_TABLE_STARTED:
	jam();
      case SchemaFile::DROP_TABLE_COMMITTED:
	jam();
      case SchemaFile::TABLE_ADD_COMMITTED:
        jam();
	ok = true;
	//------------------------------------------------------------------
	// Table was altered in the master node but not in our node. We can
	// retrieve the altered table definition from the master.
	//------------------------------------------------------------------
	restartCreateTab(signal, tableId, oldEntry, false);
        return;
        break;
      case SchemaFile::ALTER_TABLE_COMMITTED:
        jam();
	ok = true;
	
	//------------------------------------------------------------------
	// Table was altered in both our node and the master node. We can
	// retrieve the table definition from our own disk.
	//------------------------------------------------------------------
	TableRecordPtr tablePtr;
	c_tableRecordPool.getPtr(tablePtr, tableId);
	tablePtr.p->tableVersion = oldEntry->m_tableVersion;
	tablePtr.p->tableType = (DictTabInfo::TableType)oldEntry->m_tableType;
	
	// On NR get index from master because index state is not on file
	const bool file = c_systemRestart || tablePtr.p->isTable();
	restartCreateTab(signal, tableId, oldEntry, file);

	return;
      }
      ndbrequire(ok);
      break;
    }
    }
  }
  
  execute(signal, c_schemaRecord.m_callback, 0);
}//checkSchemaStatus()

void
Dbdict::restartCreateTab(Signal* signal, Uint32 tableId, 
		      const SchemaFile::TableEntry * te, bool file){
  jam();
  
  CreateTableRecordPtr createTabPtr;  
  c_opCreateTable.seize(createTabPtr);
  ndbrequire(!createTabPtr.isNull());

  createTabPtr.p->key = ++c_opRecordSequence;
  c_opCreateTable.add(createTabPtr);
  
  createTabPtr.p->m_errorCode = 0;
  createTabPtr.p->m_tablePtrI = tableId;
  createTabPtr.p->m_coordinatorRef = reference();
  createTabPtr.p->m_senderRef = 0;
  createTabPtr.p->m_senderData = RNIL;
  createTabPtr.p->m_tabInfoPtrI = RNIL;
  createTabPtr.p->m_dihAddFragPtr = RNIL;

  if(file && !ERROR_INSERTED(6002)){
    jam();
    
    c_readTableRecord.noOfPages =
      DIV(te->m_info_words + ZPAGE_HEADER_SIZE, ZSIZE_OF_PAGES_IN_WORDS);
    c_readTableRecord.pageId = 0;
    c_readTableRecord.m_callback.m_callbackData = createTabPtr.p->key;
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
    req->senderData = createTabPtr.p->key;
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
				       Uint32 callbackData,
				       Uint32 returnCode){
  jam();
  
  PageRecordPtr pageRecPtr;
  c_pageRecordArray.getPtr(pageRecPtr, c_readTableRecord.pageId);

  ParseDictTabInfoRecord parseRecord;
  parseRecord.requestType = DictTabInfo::GetTabInfoConf;
  parseRecord.errorCode = 0;
  
  Uint32 sz = c_readTableRecord.noOfPages * ZSIZE_OF_PAGES_IN_WORDS; 
  SimplePropertiesLinearReader r(&pageRecPtr.p->word[0], sz);
  handleTabInfoInit(r, &parseRecord);
  if (parseRecord.errorCode != 0)
  {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), 
			 "Unable to restart, fail while creating table %d"
			 " error: %d. Most likely change of configuration",
			 c_readTableRecord.tableId,
			 parseRecord.errorCode);
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
  c_writeTableRecord.noOfPages = c_readTableRecord.noOfPages;
  c_writeTableRecord.pageId = c_readTableRecord.pageId;
  c_writeTableRecord.tableWriteState = WriteTableRecord::TWR_CALLBACK;
  c_writeTableRecord.m_callback.m_callbackData = callbackData;
  c_writeTableRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_writeTableConf);
  startWriteTableFile(signal, c_readTableRecord.tableId);
}

void
Dbdict::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }
  
  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();

  const Uint32 tableId = conf->tableId;
  const Uint32 senderData = conf->senderData;
  
  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, senderData));
  ndbrequire(!createTabPtr.isNull());
  ndbrequire(createTabPtr.p->m_tablePtrI == tableId);

  /**
   * Put data into table record
   */
  ParseDictTabInfoRecord parseRecord;
  parseRecord.requestType = DictTabInfo::GetTabInfoConf;
  parseRecord.errorCode = 0;
  
  SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());
  handleTabInfoInit(r, &parseRecord);
  ndbrequire(parseRecord.errorCode == 0);
  
  Callback callback;
  callback.m_callbackData = createTabPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_writeTableConf);
  
  signal->header.m_noOfSections = 0;
  writeTableFile(signal, createTabPtr.p->m_tablePtrI, tabInfoPtr, &callback);
  signal->setSection(tabInfoPtr, 0);
  releaseSections(signal);
}

void
Dbdict::restartCreateTab_writeTableConf(Signal* signal, 
					Uint32 callbackData,
					Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  Callback callback;
  callback.m_callbackData = callbackData;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_dihComplete);
  
  SegmentedSectionPtr fragDataPtr; 
  fragDataPtr.sz = 0;
  fragDataPtr.setNull();
  createTab_dih(signal, createTabPtr, fragDataPtr, &callback);
}

void
Dbdict::restartCreateTab_dihComplete(Signal* signal, 
				     Uint32 callbackData,
				     Uint32 returnCode){
  jam();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));
  
  //@todo check error
  ndbrequire(createTabPtr.p->m_errorCode == 0);

  Callback callback;
  callback.m_callbackData = callbackData;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartCreateTab_activateComplete);
  
  alterTab_activate(signal, createTabPtr, &callback);
}

void
Dbdict::restartCreateTab_activateComplete(Signal* signal, 
					  Uint32 callbackData,
					  Uint32 returnCode){
  jam();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  tabPtr.p->tabState = TableRecord::DEFINED;
  
  c_opCreateTable.release(createTabPtr);

  c_restartRecord.activeTable++;
  checkSchemaStatus(signal);
}

void
Dbdict::restartDropTab(Signal* signal, Uint32 tableId){

  const Uint32 key = ++c_opRecordSequence;

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.seize(dropTabPtr));
  
  dropTabPtr.p->key = key;
  c_opDropTable.add(dropTabPtr);
  
  dropTabPtr.p->m_errorCode = 0;
  dropTabPtr.p->m_request.tableId = tableId;
  dropTabPtr.p->m_coordinatorRef = 0;
  dropTabPtr.p->m_requestType = DropTabReq::RestartDropTab;
  dropTabPtr.p->m_participantData.m_gsn = GSN_DROP_TAB_REQ;
  

  dropTabPtr.p->m_participantData.m_block = 0;
  dropTabPtr.p->m_participantData.m_callback.m_callbackData = key;
  dropTabPtr.p->m_participantData.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::restartDropTab_complete);
  dropTab_nextStep(signal, dropTabPtr);  
}

void
Dbdict::restartDropTab_complete(Signal* signal, 
				Uint32 callbackData,
				Uint32 returnCode){
  jam();

  DropTableRecordPtr dropTabPtr;
  ndbrequire(c_opDropTable.find(dropTabPtr, callbackData));
  
  //@todo check error

  c_opDropTable.release(dropTabPtr);

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

  signal->theData[0] = failedApiNode;
  signal->theData[1] = reference();
  sendSignal(retRef, GSN_API_FAILCONF, signal, 2, JBB);
}//execAPI_FAILREQ()

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
  Uint32 theFailedNodes[NodeBitmask::Size];
  memcpy(theFailedNodes, nodeFail->theNodes, sizeof(theFailedNodes));

  c_counterMgr.execNODE_FAILREP(signal);
  
  bool ok = false;
  switch(c_blockState){
  case BS_IDLE:
    jam();
    ok = true;
    if(c_opRecordPool.getSize() != c_opRecordPool.getNoOfFree()){
      jam();
      c_blockState = BS_NODE_FAILURE;
    }
    break;
  case BS_CREATE_TAB:
    jam();
    ok = true;
    if(!masterFailed)
      break;
    // fall through
  case BS_BUSY:
  case BS_NODE_FAILURE:
    jam();
    c_blockState = BS_NODE_FAILURE;
    ok = true;
    break;
  case BS_NODE_RESTART:
    jam();
    ok = true;
    break;
  }
  ndbrequire(ok);
  
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NodeBitmask::get(theFailedNodes, i)) {
      jam();
      NodeRecordPtr nodePtr;
      c_nodes.getPtr(nodePtr, i);

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
  signal->theData[0] = reference();
  sendSignal(retRef, GSN_INCL_NODECONF, signal, 1, JBB);

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

/* ---------------------------------------------------------------- */
// This signal receives information about a table from either:
// API, Ndbcntr or from other DICT.
/* ---------------------------------------------------------------- */
void
Dbdict::execCREATE_TABLE_REQ(Signal* signal){
  jamEntry();
  if(!assembleFragments(signal)){
    return;
  }
  
  CreateTableReq* const req = (CreateTableReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  
  ParseDictTabInfoRecord parseRecord;
  do {
    if(getOwnNodeId() != c_masterNodeId){
      jam();
      parseRecord.errorCode = CreateTableRef::NotMaster;
      break;
    }
    
    if (c_blockState == BS_NODE_RESTART){
      jam();
      parseRecord.errorCode = CreateTableRef::BusyWithNR;
      break;
    }
    
    if (c_blockState != BS_IDLE){
      jam();
      parseRecord.errorCode = CreateTableRef::Busy;
      break;
    }

    CreateTableRecordPtr createTabPtr;
    c_opCreateTable.seize(createTabPtr);
    
    if(createTabPtr.isNull()){
      jam();
      parseRecord.errorCode = CreateTableRef::Busy;
      break;
    }
    
    parseRecord.requestType = DictTabInfo::CreateTableFromAPI;
    parseRecord.errorCode = 0;
    
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, CreateTableReq::DICT_TAB_INFO);
    SimplePropertiesSectionReader r(ptr, getSectionSegmentPool());
    
    handleTabInfoInit(r, &parseRecord);
    releaseSections(signal);
    
    if(parseRecord.errorCode != 0){
      jam();
      c_opCreateTable.release(createTabPtr);
      break;
    }
    
    createTabPtr.p->key = ++c_opRecordSequence;
    c_opCreateTable.add(createTabPtr);
    createTabPtr.p->m_errorCode = 0;
    createTabPtr.p->m_senderRef = senderRef;
    createTabPtr.p->m_senderData = senderData;
    createTabPtr.p->m_tablePtrI = parseRecord.tablePtr.i;
    createTabPtr.p->m_coordinatorRef = reference();
    createTabPtr.p->m_fragmentsPtrI = RNIL;
    createTabPtr.p->m_dihAddFragPtr = RNIL;

    Uint32 * theData = signal->getDataPtrSend();
    CreateFragmentationReq * const req = (CreateFragmentationReq*)theData;
    req->senderRef = reference();
    req->senderData = createTabPtr.p->key;
    req->fragmentationType = parseRecord.tablePtr.p->fragmentType;
    req->noOfFragments = 0;
    req->fragmentNode = 0;
    req->primaryTableId = RNIL;
    if (parseRecord.tablePtr.p->isOrderedIndex()) {
      // ordered index has same fragmentation as the table
      const Uint32 primaryTableId = parseRecord.tablePtr.p->primaryTableId;
      TableRecordPtr primaryTablePtr;
      c_tableRecordPool.getPtr(primaryTablePtr, primaryTableId);
      // fragmentationType must be consistent
      req->fragmentationType = primaryTablePtr.p->fragmentType;
      req->primaryTableId = primaryTableId;
    }
    sendSignal(DBDIH_REF, GSN_CREATE_FRAGMENTATION_REQ, signal,
	       CreateFragmentationReq::SignalLength, JBB);
    
    c_blockState = BS_CREATE_TAB;
    return;
  } while(0);
  
  /**
   * Something went wrong
   */
  releaseSections(signal);

  CreateTableRef * ref = (CreateTableRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->masterNodeId = c_masterNodeId;
  ref->errorCode = parseRecord.errorCode;
  ref->errorLine = parseRecord.errorLine;
  ref->errorKey = parseRecord.errorKey;
  ref->status = parseRecord.status;
  sendSignal(senderRef, GSN_CREATE_TABLE_REF, signal, 
	     CreateTableRef::SignalLength, JBB);
}

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
  // Received by master
  jamEntry();
  if(!assembleFragments(signal)){
    return;
  }
  AlterTableReq* const req = (AlterTableReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 changeMask = req->changeMask;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  ParseDictTabInfoRecord* aParseRecord;
  
  // Get table definition
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId, false);
  if(tablePtr.isNull()){
    jam();
    alterTableRef(signal, req, AlterTableRef::NoSuchTable);
    return;
  }
  
  if(getOwnNodeId() != c_masterNodeId){
    jam();
    alterTableRef(signal, req, AlterTableRef::NotMaster);
    return;
  }
  
  if(c_blockState == BS_NODE_RESTART){
    jam();
    alterTableRef(signal, req, AlterTableRef::BusyWithNR);
    return;
  }
  
  if(c_blockState != BS_IDLE){
    jam();
    alterTableRef(signal, req, AlterTableRef::Busy);
    return;
  }

  if (!check_ndb_versions())
  {
    jam();
    alterTableRef(signal, req, AlterTableRef::IncompatibleVersions);
    return;
  }
  
  const TableRecord::TabState tabState = tablePtr.p->tabState;
  bool ok = false;
  switch(tabState){
  case TableRecord::NOT_DEFINED:
  case TableRecord::REORG_TABLE_PREPARED:
  case TableRecord::DEFINING:
  case TableRecord::CHECKED:
    jam();
    alterTableRef(signal, req, AlterTableRef::NoSuchTable);
    return;
  case TableRecord::DEFINED:
    ok = true;
    jam();
    break;
  case TableRecord::BACKUP_ONGOING:
    jam();
    alterTableRef(signal, req, AlterTableRef::BackupInProgress);
    return;
  case TableRecord::PREPARE_DROPPING:
  case TableRecord::DROPPING:
    jam();
    alterTableRef(signal, req, AlterTableRef::DropInProgress);
    return;
  }
  ndbrequire(ok);

  if(tablePtr.p->tableVersion != tableVersion){
    jam();
    alterTableRef(signal, req, AlterTableRef::InvalidTableVersion);
    return;
  }
  // Parse new table defintion
  ParseDictTabInfoRecord parseRecord;
  aParseRecord = &parseRecord;
    
  CreateTableRecordPtr alterTabPtr; // Reuse create table records
  c_opCreateTable.seize(alterTabPtr);
  
  if(alterTabPtr.isNull()){
    jam();
    alterTableRef(signal, req, AlterTableRef::Busy);
    return;
  }

  alterTabPtr.p->m_changeMask = changeMask;
  parseRecord.requestType = DictTabInfo::AlterTableFromAPI;
  parseRecord.errorCode = 0;
  
  SegmentedSectionPtr ptr;
  signal->getSection(ptr, AlterTableReq::DICT_TAB_INFO);
  SimplePropertiesSectionReader r(ptr, getSectionSegmentPool());

  handleTabInfoInit(r, &parseRecord, false); // Will not save info
  
  if(parseRecord.errorCode != 0){
    jam();
    c_opCreateTable.release(alterTabPtr);
    alterTableRef(signal, req, 
		  (AlterTableRef::ErrorCode) parseRecord.errorCode, 
		  aParseRecord);
    return;
  }
  
  releaseSections(signal);
  alterTabPtr.p->key = ++c_opRecordSequence;
  c_opCreateTable.add(alterTabPtr);
  ndbrequire(c_opCreateTable.find(alterTabPtr, alterTabPtr.p->key));
  alterTabPtr.p->m_errorCode = 0;
  alterTabPtr.p->m_senderRef = senderRef;
  alterTabPtr.p->m_senderData = senderData;
  alterTabPtr.p->m_tablePtrI = parseRecord.tablePtr.i;
  alterTabPtr.p->m_alterTableFailed = false;
  alterTabPtr.p->m_coordinatorRef = reference();
  alterTabPtr.p->m_fragmentsPtrI = RNIL;
  alterTabPtr.p->m_dihAddFragPtr = RNIL;
  alterTabPtr.p->m_alterTableId = tablePtr.p->tableId;

  // Send prepare request to all alive nodes
  SimplePropertiesSectionWriter w(getSectionSegmentPool());
  packTableIntoPagesImpl(w, parseRecord.tablePtr);
  
  SegmentedSectionPtr tabInfoPtr;
  w.getPtr(tabInfoPtr);
  
  alterTabPtr.p->m_tabInfoPtrI = tabInfoPtr.i;

  // Alter table on all nodes
  c_blockState = BS_BUSY;

  Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_startLcpMutex);
  Callback c = { safe_cast(&Dbdict::alterTable_backup_mutex_locked),
		 alterTabPtr.p->key };
  
  ndbrequire(mutex.lock(c));
}

void
Dbdict::alterTable_backup_mutex_locked(Signal* signal,
				       Uint32 callbackData,
				       Uint32 retValue)
{
  jamEntry();
  
  ndbrequire(retValue == 0);
  
  CreateTableRecordPtr alterTabPtr;
  ndbrequire(c_opCreateTable.find(alterTabPtr, callbackData));

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, alterTabPtr.p->m_alterTableId, true);

  Mutex mutex(signal, c_mutexMgr, alterTabPtr.p->m_startLcpMutex);
  mutex.unlock(); // ignore response

  SegmentedSectionPtr tabInfoPtr;
  getSection(tabInfoPtr, alterTabPtr.p->m_tabInfoPtrI);
  signal->setSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);
  
  alterTabPtr.p->m_tabInfoPtrI = RNIL;
  
  if(tablePtr.p->tabState == TableRecord::BACKUP_ONGOING)
  {
    jam();
    AlterTableReq* req = (AlterTableReq*)signal->getDataPtr();
    req->senderData = alterTabPtr.p->m_senderData;
    req->senderRef = alterTabPtr.p->m_senderRef;
    alterTableRef(signal, req, AlterTableRef::BackupInProgress);

    c_tableRecordPool.getPtr(tablePtr, alterTabPtr.p->m_tablePtrI);  
    releaseTableObject(tablePtr.i, false);

    c_opCreateTable.release(alterTabPtr);
    c_blockState = BS_IDLE;
    return;
  }
  
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  alterTabPtr.p->m_coordinatorData.m_gsn = GSN_ALTER_TAB_REQ;
  SafeCounter safeCounter(c_counterMgr, 
			  alterTabPtr.p->m_coordinatorData.m_counter);
  safeCounter.init<AlterTabRef>(rg, alterTabPtr.p->key);

  AlterTabReq * const lreq = (AlterTabReq*)signal->getDataPtrSend();
  lreq->senderRef = reference();
  lreq->senderData = alterTabPtr.p->key;
  lreq->clientRef = alterTabPtr.p->m_senderRef;
  lreq->clientData = alterTabPtr.p->m_senderData;
  lreq->changeMask = alterTabPtr.p->m_changeMask;
  lreq->tableId = tablePtr.p->tableId;
  lreq->tableVersion = alter_table_inc_schema_version(tablePtr.p->tableVersion);
  lreq->gci = tablePtr.p->gciTableCreated;
  lreq->requestType = AlterTabReq::AlterTablePrepare;
  
  sendFragmentedSignal(rg, GSN_ALTER_TAB_REQ, signal, 
		       AlterTabReq::SignalLength, JBB);
}

void Dbdict::alterTableRef(Signal * signal, 
			   AlterTableReq * req, 
			   AlterTableRef::ErrorCode errCode,
			   ParseDictTabInfoRecord* parseRecord)
{
  jam();
  releaseSections(signal);
  AlterTableRef * ref = (AlterTableRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  ref->senderData = req->senderData;
  ref->senderRef = reference();
  ref->masterNodeId = c_masterNodeId;
  if (parseRecord) {
    ref->errorCode = parseRecord->errorCode;
    ref->errorLine = parseRecord->errorLine;
    ref->errorKey = parseRecord->errorKey;
    ref->status = parseRecord->status;
  }
  else {
    ref->errorCode = errCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->status = 0;
  }
  sendSignal(senderRef, GSN_ALTER_TABLE_REF, signal, 
	     AlterTableRef::SignalLength, JBB);
}

void
Dbdict::execALTER_TAB_REQ(Signal * signal) 
{
  // Received in all nodes to handle change locally
  jamEntry();

  if(!assembleFragments(signal)){
    return;
  }
  AlterTabReq* const req = (AlterTabReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 changeMask = req->changeMask;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 gci = req->gci;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) req->requestType;

  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);

  CreateTableRecordPtr alterTabPtr; // Reuse create table records

  if (senderRef != reference()) {
    jam();
    c_blockState = BS_BUSY;
  }
  if ((requestType == AlterTabReq::AlterTablePrepare)
      && (senderRef != reference())) {
    jam();
    c_opCreateTable.seize(alterTabPtr);
    if(!alterTabPtr.isNull())
      alterTabPtr.p->m_changeMask = changeMask;
  }
  else {
    jam();
    ndbrequire(c_opCreateTable.find(alterTabPtr, senderData));
  }
  if(alterTabPtr.isNull()){
    jam();
    alterTabRef(signal, req, AlterTableRef::Busy);
    return;
  }

  if (!check_ndb_versions())
  {
    jam();
    alterTabRef(signal, req, AlterTableRef::IncompatibleVersions);
    return;
  }

  alterTabPtr.p->m_alterTableId = tableId;
  alterTabPtr.p->m_coordinatorRef = senderRef;
  
  // Get table definition
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId, false);
  if(tablePtr.isNull()){
    jam();
    alterTabRef(signal, req, AlterTableRef::NoSuchTable);
    return;
  }
    
  switch(requestType) {
  case(AlterTabReq::AlterTablePrepare): {
    ParseDictTabInfoRecord* aParseRecord;
  
    const TableRecord::TabState tabState = tablePtr.p->tabState;
    bool ok = false;
    switch(tabState){
    case TableRecord::NOT_DEFINED:
    case TableRecord::REORG_TABLE_PREPARED:
    case TableRecord::DEFINING:
    case TableRecord::CHECKED:
      jam();
      alterTabRef(signal, req, AlterTableRef::NoSuchTable);
      return;
    case TableRecord::DEFINED:
      ok = true;
      jam();
      break;
    case TableRecord::PREPARE_DROPPING:
    case TableRecord::DROPPING:
      jam();
      alterTabRef(signal, req, AlterTableRef::DropInProgress);
      return;
    case TableRecord::BACKUP_ONGOING:
      jam();
      alterTabRef(signal, req, AlterTableRef::BackupInProgress);
      return;
    }
    ndbrequire(ok);

    if(alter_table_inc_schema_version(tablePtr.p->tableVersion) != tableVersion){
      jam();
      alterTabRef(signal, req, AlterTableRef::InvalidTableVersion);
      return;
    }
    TableRecordPtr newTablePtr;
    if (senderRef  != reference()) {
      jam();
      // Parse altered table defintion
      ParseDictTabInfoRecord parseRecord;
      aParseRecord = &parseRecord;
      
      parseRecord.requestType = DictTabInfo::AlterTableFromAPI;
      parseRecord.errorCode = 0;
      
      SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());
      
      handleTabInfoInit(r, &parseRecord, false); // Will not save info
      
      if(parseRecord.errorCode != 0){
	jam();
	c_opCreateTable.release(alterTabPtr);
	alterTabRef(signal, req, 
		    (AlterTableRef::ErrorCode) parseRecord.errorCode, 
		    aParseRecord);
	return;
      }
      alterTabPtr.p->key = senderData;
      c_opCreateTable.add(alterTabPtr);
      alterTabPtr.p->m_errorCode = 0;
      alterTabPtr.p->m_senderRef = senderRef;
      alterTabPtr.p->m_senderData = senderData;
      alterTabPtr.p->m_tablePtrI = parseRecord.tablePtr.i;
      alterTabPtr.p->m_fragmentsPtrI = RNIL;
      alterTabPtr.p->m_dihAddFragPtr = RNIL;
      newTablePtr = parseRecord.tablePtr;
      newTablePtr.p->tableVersion = tableVersion;
    }
    else { // (req->senderRef  == reference())
      jam();
      c_tableRecordPool.getPtr(newTablePtr, alterTabPtr.p->m_tablePtrI);
      newTablePtr.p->tableVersion = tableVersion;
    }
    if (handleAlterTab(req, alterTabPtr.p, tablePtr, newTablePtr) == -1) {
      jam();
      c_opCreateTable.release(alterTabPtr);
      alterTabRef(signal, req, AlterTableRef::UnsupportedChange);
      return;
    }
    releaseSections(signal);
    // Propagate alter table to other local blocks
    AlterTabReq * req = (AlterTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = senderData;
    req->changeMask = changeMask;
    req->tableId = tableId;
    req->tableVersion = tableVersion;
    req->gci = gci;
    req->requestType = requestType;
    sendSignal(DBLQH_REF, GSN_ALTER_TAB_REQ, signal, 
	       AlterTabReq::SignalLength, JBB);	
    return;
  }
  case(AlterTabReq::AlterTableCommit): {
    jam();
    // Write schema for altered table to disk
    SegmentedSectionPtr tabInfoPtr;
    signal->getSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);
    alterTabPtr.p->m_tabInfoPtrI = tabInfoPtr.i;
    
    signal->header.m_noOfSections = 0;

    // Update table record
    tablePtr.p->packedSize = tabInfoPtr.sz;
    tablePtr.p->tableVersion = tableVersion;
    tablePtr.p->gciTableCreated = gci;

    SchemaFile::TableEntry tabEntry;
    tabEntry.m_tableVersion = tableVersion;
    tabEntry.m_tableType    = tablePtr.p->tableType;
    tabEntry.m_tableState   = SchemaFile::ALTER_TABLE_COMMITTED;
    tabEntry.m_gcp          = gci;
    tabEntry.m_info_words   = tabInfoPtr.sz;
    memset(tabEntry.m_unused, 0, sizeof(tabEntry.m_unused));
    
    Callback callback;
    callback.m_callbackData = senderData;
    callback.m_callbackFunction = 
      safe_cast(&Dbdict::alterTab_writeSchemaConf);
    
    updateSchemaState(signal, tableId, &tabEntry, &callback);
    break;
  }
  case(AlterTabReq::AlterTableRevert): {
    jam();
    // Revert failed alter table
    revertAlterTable(signal, changeMask, tableId, alterTabPtr.p);
    // Acknowledge the reverted alter table
    AlterTabConf * conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->changeMask = changeMask;
    conf->tableId = tableId;
    conf->tableVersion = tableVersion;
    conf->gci = gci;
    conf->requestType = requestType;
    sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal, 
	       AlterTabConf::SignalLength, JBB);
    break;
  }
  default: ndbrequire(false);
  }
}

void Dbdict::alterTabRef(Signal * signal, 
			 AlterTabReq * req, 
			 AlterTableRef::ErrorCode errCode,
			 ParseDictTabInfoRecord* parseRecord)
{
  jam();
  releaseSections(signal);
  AlterTabRef * ref = (AlterTabRef*)signal->getDataPtrSend();
  Uint32 senderRef = req->senderRef;
  ref->senderData = req->senderData;
  ref->senderRef = reference();
  if (parseRecord) {
    jam();
    ref->errorCode = parseRecord->errorCode;
    ref->errorLine = parseRecord->errorLine;
    ref->errorKey = parseRecord->errorKey;
    ref->errorStatus = parseRecord->status;
  }
  else {
    jam();
    ref->errorCode = errCode;
    ref->errorLine = 0;
    ref->errorKey = 0;
    ref->errorStatus = 0;
  }
  sendSignal(senderRef, GSN_ALTER_TAB_REF, signal, 
	     AlterTabRef::SignalLength, JBB);
  
  c_blockState = BS_IDLE;
}

void Dbdict::execALTER_TAB_REF(Signal * signal){
  jamEntry();

  AlterTabRef * ref = (AlterTabRef*)signal->getDataPtr();

  Uint32 senderRef = ref->senderRef;
  Uint32 senderData = ref->senderData;
  Uint32 errorCode = ref->errorCode;
  Uint32 errorLine = ref->errorLine;
  Uint32 errorKey = ref->errorKey;
  Uint32 errorStatus = ref->errorStatus;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) ref->requestType;
  CreateTableRecordPtr alterTabPtr;  
  ndbrequire(c_opCreateTable.find(alterTabPtr, senderData));
  Uint32 changeMask = alterTabPtr.p->m_changeMask;
  SafeCounter safeCounter(c_counterMgr, alterTabPtr.p->m_coordinatorData.m_counter);
  safeCounter.clearWaitingFor(refToNode(senderRef));
  switch (requestType) {
  case(AlterTabReq::AlterTablePrepare): {
    if (safeCounter.done()) {
      jam();
      // Send revert request to all alive nodes
      TableRecordPtr tablePtr;
      c_tableRecordPool.getPtr(tablePtr, alterTabPtr.p->m_alterTableId);
      Uint32 tableId = tablePtr.p->tableId;
      Uint32 tableVersion = tablePtr.p->tableVersion;
      Uint32 gci = tablePtr.p->gciTableCreated;
      SimplePropertiesSectionWriter w(getSectionSegmentPool());
      packTableIntoPagesImpl(w, tablePtr);
      SegmentedSectionPtr spDataPtr;
      w.getPtr(spDataPtr);
      signal->setSection(spDataPtr, AlterTabReq::DICT_TAB_INFO);
      
      NodeReceiverGroup rg(DBDICT, c_aliveNodes);
      alterTabPtr.p->m_coordinatorData.m_gsn = GSN_ALTER_TAB_REQ;
      safeCounter.init<AlterTabRef>(rg, alterTabPtr.p->key);
  
      AlterTabReq * const lreq = (AlterTabReq*)signal->getDataPtrSend();
      lreq->senderRef = reference();
      lreq->senderData = alterTabPtr.p->key;
      lreq->clientRef = alterTabPtr.p->m_senderRef;
      lreq->clientData = alterTabPtr.p->m_senderData;
      lreq->changeMask = changeMask;
      lreq->tableId = tableId;
      lreq->tableVersion = tableVersion;
      lreq->gci = gci;
      lreq->requestType = AlterTabReq::AlterTableRevert;
      
      sendSignal(rg, GSN_ALTER_TAB_REQ, signal, 
		 AlterTabReq::SignalLength, JBB);
    }
    else {
      jam();
      alterTabPtr.p->m_alterTableFailed = true;
    }
    break;
  }
  case(AlterTabReq::AlterTableCommit):
    jam();
  case(AlterTabReq::AlterTableRevert): {
    AlterTableRef * apiRef = (AlterTableRef*)signal->getDataPtrSend();
    
    apiRef->senderData = senderData;
    apiRef->senderRef = reference();
    apiRef->masterNodeId = c_masterNodeId;
    apiRef->errorCode = errorCode;
    apiRef->errorLine = errorLine;
    apiRef->errorKey = errorKey;
    apiRef->status = errorStatus;
    if (safeCounter.done()) {
      jam();
      sendSignal(senderRef, GSN_ALTER_TABLE_REF, signal, 
		 AlterTableRef::SignalLength, JBB);
      c_blockState = BS_IDLE;
    }
    else {
      jam();
      alterTabPtr.p->m_alterTableFailed = true;
      alterTabPtr.p->m_alterTableRef = *apiRef;
    }
    break;
  } 
  default: ndbrequire(false);
  }
}

void
Dbdict::execALTER_TAB_CONF(Signal * signal){
  jamEntry();
  AlterTabConf * const conf = (AlterTabConf*)signal->getDataPtr();
  Uint32 senderRef = conf->senderRef;
  Uint32 senderData = conf->senderData;
  Uint32 changeMask = conf->changeMask;
  Uint32 tableId = conf->tableId;
  Uint32 tableVersion = conf->tableVersion;
  Uint32 gci = conf->gci;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) conf->requestType;
  CreateTableRecordPtr alterTabPtr;  
  ndbrequire(c_opCreateTable.find(alterTabPtr, senderData));

  switch (requestType) {
  case(AlterTabReq::AlterTablePrepare): {
    switch(refToBlock(signal->getSendersBlockRef())) {
    case DBLQH: {
      jam();
      AlterTabReq * req = (AlterTabReq*)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = senderData;
      req->changeMask = changeMask;
      req->tableId = tableId;
      req->tableVersion = tableVersion;
      req->gci = gci;
      req->requestType = requestType;
      sendSignal(DBDIH_REF, GSN_ALTER_TAB_REQ, signal, 
		 AlterTabReq::SignalLength, JBB);	
      return;
    }
    case DBDIH: {
      jam();
      AlterTabReq * req = (AlterTabReq*)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = senderData;
      req->changeMask = changeMask;
      req->tableId = tableId;
      req->tableVersion = tableVersion;
      req->gci = gci;
      req->requestType = requestType;
      sendSignal(DBTC_REF, GSN_ALTER_TAB_REQ, signal, 
		 AlterTabReq::SignalLength, JBB);	
      return;
    }
    case DBTC: {
      jam();
      // Participant is done with prepare phase, send conf to coordinator
      AlterTabConf * conf = (AlterTabConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = senderData;
      conf->changeMask = changeMask;
      conf->tableId = tableId;
      conf->tableVersion = tableVersion;
      conf->gci = gci;
      conf->requestType = requestType;
      sendSignal(alterTabPtr.p->m_coordinatorRef, GSN_ALTER_TAB_CONF, signal, 
		 AlterTabConf::SignalLength, JBB);
      return;
    }
    default :break;
    }
    // Coordinator only
    SafeCounter safeCounter(c_counterMgr, alterTabPtr.p->m_coordinatorData.m_counter);
    safeCounter.clearWaitingFor(refToNode(senderRef));
    if (safeCounter.done()) {
      jam();
      // We have received all local confirmations
      if (alterTabPtr.p->m_alterTableFailed) {
	jam();
	// Send revert request to all alive nodes
	TableRecordPtr tablePtr;
	c_tableRecordPool.getPtr(tablePtr, alterTabPtr.p->m_alterTableId);
	Uint32 tableId = tablePtr.p->tableId;
	Uint32 tableVersion = tablePtr.p->tableVersion;
	Uint32 gci = tablePtr.p->gciTableCreated;
	SimplePropertiesSectionWriter w(getSectionSegmentPool());
	packTableIntoPagesImpl(w, tablePtr);
	SegmentedSectionPtr spDataPtr;
	w.getPtr(spDataPtr);
	signal->setSection(spDataPtr, AlterTabReq::DICT_TAB_INFO);
	
	NodeReceiverGroup rg(DBDICT, c_aliveNodes);
	alterTabPtr.p->m_coordinatorData.m_gsn = GSN_ALTER_TAB_REQ;
	safeCounter.init<AlterTabRef>(rg, alterTabPtr.p->key);
	
	AlterTabReq * const lreq = (AlterTabReq*)signal->getDataPtrSend();
	lreq->senderRef = reference();
	lreq->senderData = alterTabPtr.p->key;
	lreq->clientRef = alterTabPtr.p->m_senderRef;
	lreq->clientData = alterTabPtr.p->m_senderData;
	lreq->changeMask = changeMask;
	lreq->tableId = tableId;
	lreq->tableVersion = tableVersion;
	lreq->gci = gci;
	lreq->requestType = AlterTabReq::AlterTableRevert;
	
	sendSignal(rg, GSN_ALTER_TAB_REQ, signal, 
		   AlterTabReq::SignalLength, JBB);
      }
      else {
	jam();
	// Send commit request to all alive nodes
	TableRecordPtr tablePtr;
	c_tableRecordPool.getPtr(tablePtr, tableId);
	SimplePropertiesSectionWriter w(getSectionSegmentPool());
	packTableIntoPagesImpl(w, tablePtr);
	SegmentedSectionPtr spDataPtr;
	w.getPtr(spDataPtr);
	signal->setSection(spDataPtr, AlterTabReq::DICT_TAB_INFO);
	
	NodeReceiverGroup rg(DBDICT, c_aliveNodes);
	alterTabPtr.p->m_coordinatorData.m_gsn = GSN_ALTER_TAB_REQ;
	safeCounter.init<AlterTabRef>(rg, alterTabPtr.p->key);
  	
	AlterTabReq * const lreq = (AlterTabReq*)signal->getDataPtrSend();
	lreq->senderRef = reference();
	lreq->senderData = alterTabPtr.p->key;
	lreq->clientRef = alterTabPtr.p->m_senderRef;
	lreq->clientData = alterTabPtr.p->m_senderData;
	lreq->changeMask = changeMask;
	lreq->tableId = tableId;
	lreq->tableVersion = tableVersion;
	lreq->gci = gci;
	lreq->requestType = AlterTabReq::AlterTableCommit;
	
	sendFragmentedSignal(rg, GSN_ALTER_TAB_REQ, signal, 
			     AlterTabReq::SignalLength, JBB);
      }
    }
    else {
      // (!safeCounter.done())
      jam();
    }
    break;
  }
  case(AlterTabReq::AlterTableRevert):
    jam();
  case(AlterTabReq::AlterTableCommit): {
    SafeCounter safeCounter(c_counterMgr, alterTabPtr.p->m_coordinatorData.m_counter);
    safeCounter.clearWaitingFor(refToNode(senderRef));
    if (safeCounter.done()) {
      jam();
      // We have received all local confirmations
      releaseSections(signal);
      if (alterTabPtr.p->m_alterTableFailed) {
	jam();
	AlterTableRef * apiRef = 
	  (AlterTableRef*)signal->getDataPtrSend();
	*apiRef = alterTabPtr.p->m_alterTableRef;
	sendSignal(alterTabPtr.p->m_senderRef, GSN_ALTER_TABLE_REF, signal, 
		   AlterTableRef::SignalLength, JBB);	
      }
      else {
	jam();
	// Alter table completed, inform API
	AlterTableConf * const apiConf = 
	  (AlterTableConf*)signal->getDataPtrSend();
	apiConf->senderRef = reference();
	apiConf->senderData = alterTabPtr.p->m_senderData;
	apiConf->tableId = tableId;
	apiConf->tableVersion = tableVersion;
	
	//@todo check api failed
	sendSignal(alterTabPtr.p->m_senderRef, GSN_ALTER_TABLE_CONF, signal,
		   AlterTableConf::SignalLength, JBB);
      }
      
      // Release resources
      TableRecordPtr tabPtr;
      c_tableRecordPool.getPtr(tabPtr, alterTabPtr.p->m_tablePtrI);  
      releaseTableObject(tabPtr.i, false);
      c_opCreateTable.release(alterTabPtr);
      c_blockState = BS_IDLE;
    }
    else {
      // (!safeCounter.done())
      jam();
    }
    break;
  }
  default: ndbrequire(false);
  }
}

// For debugging
inline
void Dbdict::printTables()
{
  DLHashTable<TableRecord>::Iterator iter;
  bool moreTables = c_tableRecordHash.first(iter);
  printf("TABLES IN DICT:\n");
  while (moreTables) {
    TableRecordPtr tablePtr = iter.curr;
    printf("%s ", tablePtr.p->tableName);
    moreTables = c_tableRecordHash.next(iter);
  }
  printf("\n");
}

int Dbdict::handleAlterTab(AlterTabReq * req,
			   CreateTableRecord * alterTabPtrP,
			   TableRecordPtr origTablePtr,
			   TableRecordPtr newTablePtr)
{
  Uint32 changeMask = req->changeMask;
  
  if (AlterTableReq::getNameFlag(changeMask)) {
    jam();
    // Table rename
    // Remove from hashtable
#ifdef VM_TRACE
    TableRecordPtr tmp;
    ndbrequire(c_tableRecordHash.find(tmp, *origTablePtr.p));
#endif
    c_tableRecordHash.remove(origTablePtr);
    strcpy(alterTabPtrP->previousTableName, origTablePtr.p->tableName);
    strcpy(origTablePtr.p->tableName, newTablePtr.p->tableName);
    // Set new schema version
    origTablePtr.p->tableVersion = newTablePtr.p->tableVersion;
    // Put it back
#ifdef VM_TRACE
    ndbrequire(!c_tableRecordHash.find(tmp, *origTablePtr.p));
#endif
    c_tableRecordHash.add(origTablePtr);	 
    
    return 0;
  }
  jam();
  return -1;
}

void Dbdict::revertAlterTable(Signal * signal, 
			      Uint32 changeMask, 
			      Uint32 tableId,
			      CreateTableRecord * alterTabPtrP)
{
  if (AlterTableReq::getNameFlag(changeMask)) {
    jam();
    // Table rename
    // Restore previous name
    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, tableId);
    // Remove from hashtable
#ifdef VM_TRACE
    TableRecordPtr tmp;
    ndbrequire(c_tableRecordHash.find(tmp, * tablePtr.p));
#endif
    c_tableRecordHash.remove(tablePtr);
    // Restore name
    strcpy(tablePtr.p->tableName, alterTabPtrP->previousTableName);
    // Revert schema version
    tablePtr.p->tableVersion = alter_table_dec_schema_version(tablePtr.p->tableVersion);
    // Put it back
#ifdef VM_TRACE
    ndbrequire(!c_tableRecordHash.find(tmp, * tablePtr.p));
#endif
    c_tableRecordHash.add(tablePtr);	 

    return;
  }

  ndbrequire(false);
}

void
Dbdict::alterTab_writeSchemaConf(Signal* signal, 
				 Uint32 callbackData,
				 Uint32 returnCode)
{
  jam();
  Uint32 key = callbackData;
  CreateTableRecordPtr alterTabPtr;  
  ndbrequire(c_opCreateTable.find(alterTabPtr, key));
  Uint32 tableId = alterTabPtr.p->m_alterTableId;

  Callback callback;
  callback.m_callbackData = alterTabPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::alterTab_writeTableConf);
  
  SegmentedSectionPtr tabInfoPtr;
  getSection(tabInfoPtr, alterTabPtr.p->m_tabInfoPtrI);
  
  writeTableFile(signal, tableId, tabInfoPtr, &callback);

  signal->setSection(tabInfoPtr, 0);
  releaseSections(signal);
}

void
Dbdict::alterTab_writeTableConf(Signal* signal, 
				Uint32 callbackData,
				Uint32 returnCode)
{
  jam();
  CreateTableRecordPtr alterTabPtr;  
  ndbrequire(c_opCreateTable.find(alterTabPtr, callbackData));
  Uint32 coordinatorRef = alterTabPtr.p->m_coordinatorRef;
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, alterTabPtr.p->m_alterTableId);

  // Alter table commit request handled successfully 
  AlterTabConf * conf = (AlterTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = callbackData;
  conf->tableId = tabPtr.p->tableId;
  conf->tableVersion = tabPtr.p->tableVersion;
  conf->gci = tabPtr.p->gciTableCreated;
  conf->requestType = AlterTabReq::AlterTableCommit;
  sendSignal(coordinatorRef, GSN_ALTER_TAB_CONF, signal, 
	       AlterTabConf::SignalLength, JBB);


  {
    ApiBroadcastRep* api= (ApiBroadcastRep*)signal->getDataPtrSend();
    api->gsn = GSN_ALTER_TABLE_REP;
    api->minVersion = MAKE_VERSION(4,1,15);

    AlterTableRep* rep = (AlterTableRep*)api->theData;
    rep->tableId = tabPtr.p->tableId;
    rep->tableVersion = alter_table_dec_schema_version(tabPtr.p->tableVersion);
    rep->changeType = AlterTableRep::CT_ALTERED;
    
    LinearSectionPtr ptr[3];
    ptr[0].p = (Uint32*)alterTabPtr.p->previousTableName;
    ptr[0].sz = (sizeof(alterTabPtr.p->previousTableName) + 3) >> 2;
    
    sendSignal(QMGR_REF, GSN_API_BROADCAST_REP, signal, 
	       ApiBroadcastRep::SignalLength + AlterTableRep::SignalLength,
	       JBB, ptr,1);
  }

  if(coordinatorRef != reference()) {
    jam();
    // Release resources
    c_tableRecordPool.getPtr(tabPtr, alterTabPtr.p->m_tablePtrI);  
    releaseTableObject(tabPtr.i, false);
    c_opCreateTable.release(alterTabPtr);
    c_blockState = BS_IDLE;
  }
}

void
Dbdict::execCREATE_FRAGMENTATION_REF(Signal * signal){
  jamEntry();
  const Uint32 * theData = signal->getDataPtr();
  CreateFragmentationRef * const ref = (CreateFragmentationRef*)theData;
  (void)ref;
  ndbrequire(false);
}

void
Dbdict::execCREATE_FRAGMENTATION_CONF(Signal* signal){
  jamEntry();
  const Uint32 * theData = signal->getDataPtr();
  CreateFragmentationConf * const conf = (CreateFragmentationConf*)theData;

  CreateTableRecordPtr createTabPtr;
  ndbrequire(c_opCreateTable.find(createTabPtr, conf->senderData));

  ndbrequire(signal->getNoOfSections() == 1);

  SegmentedSectionPtr fragDataPtr;
  signal->getSection(fragDataPtr, CreateFragmentationConf::FRAGMENTS);
  signal->header.m_noOfSections = 0;

  /**
   * Get table
   */
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);

  /**
   * Save fragment count
   */
  tabPtr.p->fragmentCount = conf->noOfFragments;

  /**
   * Update table version
   */
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tabEntry = getTableEntry(xsf, tabPtr.i);

  tabPtr.p->tableVersion = create_table_inc_schema_version(tabEntry->m_tableVersion);

  /**
   * Pack
   */
  SimplePropertiesSectionWriter w(getSectionSegmentPool());
  packTableIntoPagesImpl(w, tabPtr);
  
  SegmentedSectionPtr spDataPtr;
  w.getPtr(spDataPtr);
  
  signal->setSection(spDataPtr, CreateTabReq::DICT_TAB_INFO);
  signal->setSection(fragDataPtr, CreateTabReq::FRAGMENTATION);
  
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  SafeCounter tmp(c_counterMgr, createTabPtr.p->m_coordinatorData.m_counter);
  createTabPtr.p->m_coordinatorData.m_gsn = GSN_CREATE_TAB_REQ;
  createTabPtr.p->m_coordinatorData.m_requestType = CreateTabReq::CreateTablePrepare;
  tmp.init<CreateTabRef>(rg, GSN_CREATE_TAB_REF, createTabPtr.p->key);
  
  CreateTabReq * const req = (CreateTabReq*)theData;
  req->senderRef = reference();
  req->senderData = createTabPtr.p->key;
  req->clientRef = createTabPtr.p->m_senderRef;
  req->clientData = createTabPtr.p->m_senderData;
  req->requestType = CreateTabReq::CreateTablePrepare;

  req->gci = 0;
  req->tableId = tabPtr.i;
  req->tableVersion = create_table_inc_schema_version(tabEntry->m_tableVersion);
  
  sendFragmentedSignal(rg, GSN_CREATE_TAB_REQ, signal, 
		       CreateTabReq::SignalLength, JBB);

  return;
}

void
Dbdict::execCREATE_TAB_REF(Signal* signal){
  jamEntry();

  CreateTabRef * const ref = (CreateTabRef*)signal->getDataPtr();
  
  CreateTableRecordPtr createTabPtr;
  ndbrequire(c_opCreateTable.find(createTabPtr, ref->senderData));
  
  ndbrequire(createTabPtr.p->m_coordinatorRef == reference());
  ndbrequire(createTabPtr.p->m_coordinatorData.m_gsn == GSN_CREATE_TAB_REQ);

  if(ref->errorCode != CreateTabRef::NF_FakeErrorREF){
    createTabPtr.p->setErrorCode(ref->errorCode);
  }
  createTab_reply(signal, createTabPtr, refToNode(ref->senderRef));
}

void
Dbdict::execCREATE_TAB_CONF(Signal* signal){
  jamEntry();

  ndbrequire(signal->getNoOfSections() == 0);

  CreateTabConf * const conf = (CreateTabConf*)signal->getDataPtr();
  
  CreateTableRecordPtr createTabPtr;
  ndbrequire(c_opCreateTable.find(createTabPtr, conf->senderData));
  
  ndbrequire(createTabPtr.p->m_coordinatorRef == reference());
  ndbrequire(createTabPtr.p->m_coordinatorData.m_gsn == GSN_CREATE_TAB_REQ);

  createTab_reply(signal, createTabPtr, refToNode(conf->senderRef));
}

void
Dbdict::createTab_reply(Signal* signal, 
			CreateTableRecordPtr createTabPtr, 
			Uint32 nodeId)
{

  SafeCounter tmp(c_counterMgr, createTabPtr.p->m_coordinatorData.m_counter);
  if(!tmp.clearWaitingFor(nodeId)){
    jam();
    return;
  }
  
  switch(createTabPtr.p->m_coordinatorData.m_requestType){
  case CreateTabReq::CreateTablePrepare:{

    if(createTabPtr.p->m_errorCode != 0){
      jam();
      /**
       * Failed to prepare on atleast one node -> abort on all
       */
      NodeReceiverGroup rg(DBDICT, c_aliveNodes);
      createTabPtr.p->m_coordinatorData.m_gsn = GSN_CREATE_TAB_REQ;
      createTabPtr.p->m_coordinatorData.m_requestType = CreateTabReq::CreateTableDrop;
      ndbrequire(tmp.init<CreateTabRef>(rg, createTabPtr.p->key));
      
      CreateTabReq * const req = (CreateTabReq*)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = createTabPtr.p->key;
      req->requestType = CreateTabReq::CreateTableDrop;
      
      sendSignal(rg, GSN_CREATE_TAB_REQ, signal, 
		 CreateTabReq::SignalLength, JBB);
      return;
    }
    
    /**
     * Lock mutex before commiting table
     */
    Mutex mutex(signal, c_mutexMgr, createTabPtr.p->m_startLcpMutex);
    Callback c = { safe_cast(&Dbdict::createTab_startLcpMutex_locked),
		   createTabPtr.p->key};

    ndbrequire(mutex.lock(c));
    return;
  }
  case CreateTabReq::CreateTableCommit:{
    jam();
    ndbrequire(createTabPtr.p->m_errorCode == 0);
    
    /**
     * Unlock mutex before commiting table
     */
    Mutex mutex(signal, c_mutexMgr, createTabPtr.p->m_startLcpMutex);
    Callback c = { safe_cast(&Dbdict::createTab_startLcpMutex_unlocked),
		   createTabPtr.p->key};
    mutex.unlock(c);
    return;
  }
  case CreateTabReq::CreateTableDrop:{
    jam();
    CreateTableRef * const ref = (CreateTableRef*)signal->getDataPtr();
    ref->senderRef = reference();
    ref->senderData = createTabPtr.p->m_senderData;
    ref->errorCode = createTabPtr.p->m_errorCode;
    ref->masterNodeId = c_masterNodeId;
    ref->status = 0;
    ref->errorKey = 0;
    ref->errorLine = 0;
    
    //@todo check api failed
    sendSignal(createTabPtr.p->m_senderRef, GSN_CREATE_TABLE_REF, signal, 
	       CreateTableRef::SignalLength, JBB);
    c_opCreateTable.release(createTabPtr);
    c_blockState = BS_IDLE;
    return;
  }
  }
  ndbrequire(false);
}

void
Dbdict::createTab_startLcpMutex_locked(Signal* signal, 
				       Uint32 callbackData,
				       Uint32 retValue){
  jamEntry();

  ndbrequire(retValue == 0);
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));
  
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  createTabPtr.p->m_coordinatorData.m_gsn = GSN_CREATE_TAB_REQ;
  createTabPtr.p->m_coordinatorData.m_requestType = CreateTabReq::CreateTableCommit;
  SafeCounter tmp(c_counterMgr, createTabPtr.p->m_coordinatorData.m_counter);
  tmp.init<CreateTabRef>(rg, GSN_CREATE_TAB_REF, createTabPtr.p->key);
  
  CreateTabReq * const req = (CreateTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = createTabPtr.p->key;
  req->requestType = CreateTabReq::CreateTableCommit;
  
  sendSignal(rg, GSN_CREATE_TAB_REQ, signal, 
	     CreateTabReq::SignalLength, JBB);
}

void
Dbdict::createTab_startLcpMutex_unlocked(Signal* signal, 
					 Uint32 callbackData,
					 Uint32 retValue){
  jamEntry();
  
  ndbrequire(retValue == 0);
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  createTabPtr.p->m_startLcpMutex.release(c_mutexMgr);
  
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  
  CreateTableConf * const conf = (CreateTableConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = createTabPtr.p->m_senderData;
  conf->tableId = createTabPtr.p->m_tablePtrI;
  conf->tableVersion = tabPtr.p->tableVersion;
  
  //@todo check api failed
  sendSignal(createTabPtr.p->m_senderRef, GSN_CREATE_TABLE_CONF, signal, 
	     CreateTableConf::SignalLength, JBB);
  c_opCreateTable.release(createTabPtr);
  c_blockState = BS_IDLE;
  return;
}

/***********************************************************
 * CreateTable participant code
 **********************************************************/
void
Dbdict::execCREATE_TAB_REQ(Signal* signal){
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  CreateTabReq * const req = (CreateTabReq*)signal->getDataPtr();

  CreateTabReq::RequestType rt = (CreateTabReq::RequestType)req->requestType;
  switch(rt){
  case CreateTabReq::CreateTablePrepare:
    CRASH_INSERTION2(6003, getOwnNodeId() != c_masterNodeId);
    createTab_prepare(signal, req);
    return;
  case CreateTabReq::CreateTableCommit:
    CRASH_INSERTION2(6004, getOwnNodeId() != c_masterNodeId);
    createTab_commit(signal, req);
    return;
  case CreateTabReq::CreateTableDrop:
    CRASH_INSERTION2(6005, getOwnNodeId() != c_masterNodeId);
    createTab_drop(signal, req);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::createTab_prepare(Signal* signal, CreateTabReq * req){

  const Uint32 gci = req->gci;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;

  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, CreateTabReq::DICT_TAB_INFO);
  
  CreateTableRecordPtr createTabPtr;  
  if(req->senderRef == reference()){
    jam();
    ndbrequire(c_opCreateTable.find(createTabPtr, req->senderData));
  } else {
    jam();
    c_opCreateTable.seize(createTabPtr);
    
    ndbrequire(!createTabPtr.isNull());
    
    createTabPtr.p->key = req->senderData;
    c_opCreateTable.add(createTabPtr);
    createTabPtr.p->m_errorCode = 0;
    createTabPtr.p->m_tablePtrI = tableId;
    createTabPtr.p->m_coordinatorRef = req->senderRef;
    createTabPtr.p->m_senderRef = req->clientRef;
    createTabPtr.p->m_senderData = req->clientData;
    createTabPtr.p->m_dihAddFragPtr = RNIL;
    
    /**
     * Put data into table record
     */
    ParseDictTabInfoRecord parseRecord;
    parseRecord.requestType = DictTabInfo::AddTableFromDict;
    parseRecord.errorCode = 0;
    
    SimplePropertiesSectionReader r(tabInfoPtr, getSectionSegmentPool());
    
    handleTabInfoInit(r, &parseRecord);

    ndbrequire(parseRecord.errorCode == 0);
  }
  
  ndbrequire(!createTabPtr.isNull());

  SegmentedSectionPtr fragPtr;
  signal->getSection(fragPtr, CreateTabReq::FRAGMENTATION);

  createTabPtr.p->m_tabInfoPtrI = tabInfoPtr.i;
  createTabPtr.p->m_fragmentsPtrI = fragPtr.i;
  
  signal->header.m_noOfSections = 0;
  
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, tableId);
  tabPtr.p->packedSize = tabInfoPtr.sz;
  tabPtr.p->tableVersion = tableVersion;
  tabPtr.p->gciTableCreated = gci;

  SchemaFile::TableEntry tabEntry;
  tabEntry.m_tableVersion = tableVersion;
  tabEntry.m_tableType    = tabPtr.p->tableType;
  tabEntry.m_tableState   = SchemaFile::ADD_STARTED;
  tabEntry.m_gcp          = gci;
  tabEntry.m_info_words   = tabInfoPtr.sz;
  memset(tabEntry.m_unused, 0, sizeof(tabEntry.m_unused));
  
  Callback callback;
  callback.m_callbackData = createTabPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createTab_writeSchemaConf1);
  
  updateSchemaState(signal, tableId, &tabEntry, &callback);
}

void getSection(SegmentedSectionPtr & ptr, Uint32 i);

void
Dbdict::createTab_writeSchemaConf1(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  Callback callback;
  callback.m_callbackData = createTabPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createTab_writeTableConf);
  
  SegmentedSectionPtr tabInfoPtr;
  getSection(tabInfoPtr, createTabPtr.p->m_tabInfoPtrI);
  writeTableFile(signal, createTabPtr.p->m_tablePtrI, tabInfoPtr, &callback);

  createTabPtr.p->m_tabInfoPtrI = RNIL;
  signal->setSection(tabInfoPtr, 0);
  releaseSections(signal);
}

void
Dbdict::createTab_writeTableConf(Signal* signal, 
				 Uint32 callbackData,
				 Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  SegmentedSectionPtr fragDataPtr;
  getSection(fragDataPtr, createTabPtr.p->m_fragmentsPtrI);
  
  Callback callback;
  callback.m_callbackData = callbackData;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createTab_dihComplete);
  
  createTab_dih(signal, createTabPtr, fragDataPtr, &callback);
}

void
Dbdict::createTab_dih(Signal* signal, 
		      CreateTableRecordPtr createTabPtr, 
		      SegmentedSectionPtr fragDataPtr,
		      Callback * c){
  jam();
  
  createTabPtr.p->m_callback = * c;

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  
  DiAddTabReq * req = (DiAddTabReq*)signal->getDataPtrSend();
  req->connectPtr = createTabPtr.p->key;
  req->tableId = tabPtr.i;
  req->fragType = tabPtr.p->fragmentType;
  req->kValue = tabPtr.p->kValue;
  req->noOfReplicas = 0;
  req->storedTable = tabPtr.p->storedTable;
  req->tableType = tabPtr.p->tableType;
  req->schemaVersion = tabPtr.p->tableVersion;
  req->primaryTableId = tabPtr.p->primaryTableId;

  if(!fragDataPtr.isNull()){
    signal->setSection(fragDataPtr, DiAddTabReq::FRAGMENTATION);
  }

  sendSignal(DBDIH_REF, GSN_DIADDTABREQ, signal, 
	     DiAddTabReq::SignalLength, JBB);

  /**
   * Create KeyDescriptor
   */
  KeyDescriptor* desc= g_key_descriptor_pool.getPtr(tabPtr.i);
  new (desc) KeyDescriptor();

  Uint32 key = 0;
  Uint32 tAttr = tabPtr.p->firstAttribute;
  while (tAttr != RNIL) 
  {
    jam();
    AttributeRecord* aRec = c_attributeRecordPool.getPtr(tAttr);
    if (aRec->tupleKey) 
    {
      desc->noOfKeyAttr ++;
      desc->keyAttr[key].attributeDescriptor = aRec->attributeDescriptor;
      
      Uint32 csNumber = (aRec->extPrecision >> 16);
      if(csNumber)
      {
	desc->keyAttr[key].charsetInfo = all_charsets[csNumber];
	ndbrequire(all_charsets[csNumber]);
	desc->hasCharAttr = 1;
      }
      else
      {
	desc->keyAttr[key].charsetInfo = 0;	  
      }
      if(AttributeDescriptor::getDKey(aRec->attributeDescriptor))
      {
	desc->noOfDistrKeys ++;
      }
      key++;
    }
    tAttr = aRec->nextAttrInTable;
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
Dbdict::execADD_FRAGREQ(Signal* signal) {
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

  ndbrequire(node == getOwnNodeId());

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, senderData));
  
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
    req->noOfNewAttr = 0;
    req->noOfCharsets = tabPtr.p->noOfCharsets;
    req->checksumIndicator = 1;
    req->noOfAttributeGroups = 1;
    req->GCPIndicator = 0;
    req->startGci = startGci;
    req->tableType = tabPtr.p->tableType;
    req->primaryTableId = tabPtr.p->primaryTableId;
    sendSignal(DBLQH_REF, GSN_LQHFRAGREQ, signal, 
	       LqhFragReq::SignalLength, JBB);
  }
}

void
Dbdict::execLQHFRAGREF(Signal * signal){
  jamEntry();
  LqhFragRef * const ref = (LqhFragRef*)signal->getDataPtr();
 
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, ref->senderData));
  
  createTabPtr.p->setErrorCode(ref->errorCode);
  
  {
    AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();
    ref->dihPtr = createTabPtr.p->m_dihAddFragPtr;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGREF, signal, 
	       AddFragRef::SignalLength, JBB);
  }
}

void
Dbdict::execLQHFRAGCONF(Signal * signal){
  jamEntry();
  LqhFragConf * const conf = (LqhFragConf*)signal->getDataPtr();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, conf->senderData));
  
  createTabPtr.p->m_lqhFragPtr = conf->lqhFragPtr;
  
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  sendLQHADDATTRREQ(signal, createTabPtr, tabPtr.p->firstAttribute);
}

void
Dbdict::sendLQHADDATTRREQ(Signal* signal,
			  CreateTableRecordPtr createTabPtr,
			  Uint32 attributePtrI){
  jam();
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
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
      if (attrPtr.p->nextAttrInTable != RNIL) {
        getIndexAttr(tabPtr, attributePtrI, &primaryAttrId);
      } else {
        primaryAttrId = ZNIL;
        if (tabPtr.p->isOrderedIndex())
          entry.attrId = 0;     // attribute goes to TUP
      }
      entry.attrId |= (primaryAttrId << 16);
    }
    attributePtrI = attrPtr.p->nextAttrInTable;
  }
  req->lqhFragPtr = createTabPtr.p->m_lqhFragPtr;
  req->senderData = createTabPtr.p->key;
  req->senderAttrPtr = attributePtrI;
  req->noOfAttributes = i;
  
  sendSignal(DBLQH_REF, GSN_LQHADDATTREQ, signal, 
	     LqhAddAttrReq::HeaderLength + LqhAddAttrReq::EntryLength * i, JBB);
}

void
Dbdict::execLQHADDATTREF(Signal * signal){
  jamEntry();
  LqhAddAttrRef * const ref = (LqhAddAttrRef*)signal->getDataPtr();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, ref->senderData));
  
  createTabPtr.p->setErrorCode(ref->errorCode);
  
  {
    AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();
    ref->dihPtr = createTabPtr.p->m_dihAddFragPtr;
    sendSignal(DBDIH_REF, GSN_ADD_FRAGREF, signal, 
	       AddFragRef::SignalLength, JBB);
  }
  
}

void
Dbdict::execLQHADDATTCONF(Signal * signal){
  jamEntry();
  LqhAddAttrConf * const conf = (LqhAddAttrConf*)signal->getDataPtr();

  CreateTableRecordPtr createTabPtr;
  ndbrequire(c_opCreateTable.find(createTabPtr, conf->senderData));
  
  const Uint32 fragId = conf->fragId;
  const Uint32 nextAttrPtr = conf->senderAttrPtr;
  if(nextAttrPtr != RNIL){
    jam();
    sendLQHADDATTRREQ(signal, createTabPtr, nextAttrPtr);
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
Dbdict::execDIADDTABREF(Signal* signal){
  jam();
  
  DiAddTabRef * const ref = (DiAddTabRef*)signal->getDataPtr();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, ref->senderData));
  
  createTabPtr.p->setErrorCode(ref->errorCode);  
  execute(signal, createTabPtr.p->m_callback, 0);
}

void
Dbdict::execDIADDTABCONF(Signal* signal){
  jam();
  
  DiAddTabConf * const conf = (DiAddTabConf*)signal->getDataPtr();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, conf->senderData));

  signal->theData[0] = createTabPtr.p->key;
  signal->theData[1] = reference();
  signal->theData[2] = createTabPtr.p->m_tablePtrI;

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
Dbdict::execTAB_COMMITREF(Signal* signal) {
  jamEntry();
  ndbrequire(false);
}//execTAB_COMMITREF()

void
Dbdict::execTAB_COMMITCONF(Signal* signal){
  jamEntry();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, signal->theData[0]));
  
  if(refToBlock(signal->getSendersBlockRef()) == DBLQH){

    execute(signal, createTabPtr.p->m_callback, 0);
    return;
  }

  if(refToBlock(signal->getSendersBlockRef()) == DBDIH){
    TableRecordPtr tabPtr;
    c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
    
    signal->theData[0] = tabPtr.i;
    signal->theData[1] = tabPtr.p->tableVersion;
    signal->theData[2] = (Uint32)tabPtr.p->storedTable;     
    signal->theData[3] = reference();
    signal->theData[4] = (Uint32)tabPtr.p->tableType;
    signal->theData[5] = createTabPtr.p->key;
    signal->theData[6] = (Uint32)tabPtr.p->noOfPrimkey;
    
    sendSignal(DBTC_REF, GSN_TC_SCHVERREQ, signal, 7, JBB);
    return;
  }
  
  ndbrequire(false);
}

void
Dbdict::createTab_dihComplete(Signal* signal, 
			      Uint32 callbackData,
			      Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));

  //@todo check for master failed
  
  if(createTabPtr.p->m_errorCode == 0){
    jam();

    CreateTabConf * const conf = (CreateTabConf*)signal->getDataPtr();
    conf->senderRef = reference();
    conf->senderData = createTabPtr.p->key;
    sendSignal(createTabPtr.p->m_coordinatorRef, GSN_CREATE_TAB_CONF,
	       signal, CreateTabConf::SignalLength, JBB);
    return;
  }

  CreateTabRef * const ref = (CreateTabRef*)signal->getDataPtr();
  ref->senderRef = reference();
  ref->senderData = createTabPtr.p->key;
  ref->errorCode = createTabPtr.p->m_errorCode;
  ref->errorLine = 0;
  ref->errorKey = 0;
  ref->errorStatus = 0;

  sendSignal(createTabPtr.p->m_coordinatorRef, GSN_CREATE_TAB_REF,
	     signal, CreateTabRef::SignalLength, JBB);
}

void
Dbdict::createTab_commit(Signal * signal, CreateTabReq * req){
  jam();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, req->senderData));

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  
  SchemaFile::TableEntry tabEntry;
  tabEntry.m_tableVersion = tabPtr.p->tableVersion;
  tabEntry.m_tableType    = tabPtr.p->tableType;
  tabEntry.m_tableState   = SchemaFile::TABLE_ADD_COMMITTED;
  tabEntry.m_gcp          = tabPtr.p->gciTableCreated;
  tabEntry.m_info_words   = tabPtr.p->packedSize;
  memset(tabEntry.m_unused, 0, sizeof(tabEntry.m_unused));
  
  Callback callback;
  callback.m_callbackData = createTabPtr.p->key;
  callback.m_callbackFunction = 
    safe_cast(&Dbdict::createTab_writeSchemaConf2);
  
  updateSchemaState(signal, tabPtr.i, &tabEntry, &callback);
}

void
Dbdict::createTab_writeSchemaConf2(Signal* signal, 
				   Uint32 callbackData,
				   Uint32 returnCode){
  jam();
  
  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));
  
  Callback c;
  c.m_callbackData = callbackData;
  c.m_callbackFunction = safe_cast(&Dbdict::createTab_alterComplete);
  alterTab_activate(signal, createTabPtr, &c);
}

void
Dbdict::createTab_alterComplete(Signal* signal, 
				Uint32 callbackData,
				Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));
  
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  tabPtr.p->tabState = TableRecord::DEFINED;
  
  //@todo check error
  //@todo check master failed
  
  CreateTabConf * const conf = (CreateTabConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = createTabPtr.p->key;
  sendSignal(createTabPtr.p->m_coordinatorRef, GSN_CREATE_TAB_CONF,
	     signal, CreateTabConf::SignalLength, JBB);

  if(createTabPtr.p->m_coordinatorRef != reference()){
    jam();
    c_opCreateTable.release(createTabPtr);
  }
}

void
Dbdict::createTab_drop(Signal* signal, CreateTabReq * req){
  jam();

  const Uint32 key = req->senderData;

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, key));
  
  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  tabPtr.p->tabState = TableRecord::DROPPING;

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.seize(dropTabPtr));
  
  dropTabPtr.p->key = key;
  c_opDropTable.add(dropTabPtr);
  
  dropTabPtr.p->m_errorCode = 0;
  dropTabPtr.p->m_request.tableId = createTabPtr.p->m_tablePtrI;
  dropTabPtr.p->m_requestType = DropTabReq::CreateTabDrop;
  dropTabPtr.p->m_coordinatorRef = createTabPtr.p->m_coordinatorRef;
  dropTabPtr.p->m_participantData.m_gsn = GSN_DROP_TAB_REQ;
  
  dropTabPtr.p->m_participantData.m_block = 0;
  dropTabPtr.p->m_participantData.m_callback.m_callbackData = req->senderData;
  dropTabPtr.p->m_participantData.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::createTab_dropComplete);
  dropTab_nextStep(signal, dropTabPtr);  
}

void
Dbdict::createTab_dropComplete(Signal* signal, 
			       Uint32 callbackData,
			       Uint32 returnCode){
  jam();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, callbackData));
  
  DropTableRecordPtr dropTabPtr;
  ndbrequire(c_opDropTable.find(dropTabPtr, callbackData));

  TableRecordPtr tabPtr;
  c_tableRecordPool.getPtr(tabPtr, createTabPtr.p->m_tablePtrI);
  
  releaseTableObject(tabPtr.i);

  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tabPtr.i);
  tableEntry->m_tableState = SchemaFile::DROP_TABLE_COMMITTED;
  
  //@todo check error
  //@todo check master failed
  
  CreateTabConf * const conf = (CreateTabConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = createTabPtr.p->key;
  sendSignal(createTabPtr.p->m_coordinatorRef, GSN_CREATE_TAB_CONF,
	     signal, CreateTabConf::SignalLength, JBB);

  if(createTabPtr.p->m_coordinatorRef != reference()){
    jam();
    c_opCreateTable.release(createTabPtr);
  }

  c_opDropTable.release(dropTabPtr);
}

void
Dbdict::alterTab_activate(Signal* signal, CreateTableRecordPtr createTabPtr,
			  Callback * c){

  createTabPtr.p->m_callback = * c;
  
  signal->theData[0] = createTabPtr.p->key;
  signal->theData[1] = reference();
  signal->theData[2] = createTabPtr.p->m_tablePtrI;
  sendSignal(DBDIH_REF, GSN_TAB_COMMITREQ, signal, 3, JBB);
}

void
Dbdict::execTC_SCHVERCONF(Signal* signal){
  jamEntry();

  CreateTableRecordPtr createTabPtr;  
  ndbrequire(c_opCreateTable.find(createTabPtr, signal->theData[1]));

  execute(signal, createTabPtr.p->m_callback, 0);
}

#define tabRequire(cond, error) \
  if (!(cond)) { \
    jam();    \
    parseP->errorCode = error; parseP->errorLine = __LINE__; \
    parseP->errorKey = it.getKey(); \
    return;   \
  }//if

// handleAddTableFailure(signal, __LINE__, allocatedTable);

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
  DictTabInfo::Table tableDesc; tableDesc.init();
  status = SimpleProperties::unpack(it, &tableDesc, 
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
  const Uint32 tableNameLength = strlen(tableDesc.TableName) + 1;

  TableRecord keyRecord;
  tabRequire(tableNameLength <= sizeof(keyRecord.tableName),
	     CreateTableRef::TableNameTooLong);
  strcpy(keyRecord.tableName, tableDesc.TableName);
  
  TableRecordPtr tablePtr;
  c_tableRecordHash.find(tablePtr, keyRecord);
  
  if (checkExist){
    jam();
    /* ---------------------------------------------------------------- */
    // Check if table already existed.
    /* ---------------------------------------------------------------- */
    tabRequire(tablePtr.i == RNIL, CreateTableRef::TableAlreadyExist);
  }

  switch (parseP->requestType) {
  case DictTabInfo::CreateTableFromAPI: {
    jam();
  }
  case DictTabInfo::AlterTableFromAPI:{
    jam();
    tablePtr.i = getFreeTableRecord(tableDesc.PrimaryTableId);
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
    tablePtr.i = tableDesc.TableId;
    
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
#ifdef HAVE_TABLE_REORG
/* ---------------------------------------------------------------- */
// Get id of second table id and check that table doesn't already exist
// and set up links between first and second table.
/* ---------------------------------------------------------------- */
    TableRecordPtr secondTablePtr;
    secondTablePtr.i = tableDesc.SecondTableId;
    c_tableRecordPool.getPtr(secondTablePtr);
    ndbrequire(secondTablePtr.p->tabState == TableRecord::NOT_DEFINED);
    
    initialiseTableRecord(secondTablePtr);
    secondTablePtr.p->tabState = TableRecord::REORG_TABLE_PREPARED;
    secondTablePtr.p->secondTable = tablePtr.i;
    tablePtr.p->secondTable = secondTablePtr.i;
#endif
/* ---------------------------------------------------------------- */
// Set table version
/* ---------------------------------------------------------------- */
    Uint32 tableVersion = tableDesc.TableVersion;
    tablePtr.p->tableVersion = tableVersion;
    
    break;
  }
  default:
    ndbrequire(false);
    break;
  }//switch
  parseP->tablePtr = tablePtr;
  
  strcpy(tablePtr.p->tableName, keyRecord.tableName);  
  if (parseP->requestType != DictTabInfo::AlterTableFromAPI) {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbdict: name=%s,id=%u", tablePtr.p->tableName, tablePtr.i);
    TableRecordPtr tmp;
    ndbrequire(!c_tableRecordHash.find(tmp, * tablePtr.p));
#endif
    c_tableRecordHash.add(tablePtr);
  }
  
  //tablePtr.p->noOfPrimkey = tableDesc.NoOfKeyAttr;
  //tablePtr.p->noOfNullAttr = tableDesc.NoOfNullable;
  //tablePtr.p->tupKeyLength = tableDesc.KeyLength;
  tablePtr.p->noOfAttributes = tableDesc.NoOfAttributes;
  tablePtr.p->storedTable = tableDesc.TableLoggedFlag;
  tablePtr.p->minLoadFactor = tableDesc.MinLoadFactor;
  tablePtr.p->maxLoadFactor = tableDesc.MaxLoadFactor;
  tablePtr.p->fragmentType = (DictTabInfo::FragmentType)tableDesc.FragmentType;
  tablePtr.p->tableType = (DictTabInfo::TableType)tableDesc.TableType;
  tablePtr.p->kValue = tableDesc.TableKValue;
  tablePtr.p->fragmentCount = tableDesc.FragmentCount;
  tablePtr.p->maxRowsLow = tableDesc.MaxRowsLow;
  tablePtr.p->maxRowsHigh = tableDesc.MaxRowsHigh;
  tablePtr.p->minRowsLow = tableDesc.MinRowsLow;
  tablePtr.p->minRowsHigh = tableDesc.MinRowsHigh;

  Uint64 maxRows =
    (((Uint64)tablePtr.p->maxRowsHigh) << 32) + tablePtr.p->maxRowsLow;
  Uint64 minRows =
    (((Uint64)tablePtr.p->minRowsHigh) << 32) + tablePtr.p->minRowsLow;

  tablePtr.p->frmLen = tableDesc.FrmLen;
  memcpy(tablePtr.p->frmData, tableDesc.FrmData, tableDesc.FrmLen);  

  if(tableDesc.PrimaryTableId != RNIL) {
    
    tablePtr.p->primaryTableId = tableDesc.PrimaryTableId;
    tablePtr.p->indexState = (TableRecord::IndexState)tableDesc.IndexState;
    tablePtr.p->insertTriggerId = tableDesc.InsertTriggerId;
    tablePtr.p->updateTriggerId = tableDesc.UpdateTriggerId;
    tablePtr.p->deleteTriggerId = tableDesc.DeleteTriggerId;
    tablePtr.p->customTriggerId = tableDesc.CustomTriggerId;
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
  
  handleTabInfo(it, parseP);

  if(parseP->errorCode != 0)
  {
    /**
     * Release table
     */
    releaseTableObject(tablePtr.i, checkExist);
  }
}//handleTabInfoInit()

void Dbdict::handleTabInfo(SimpleProperties::Reader & it,
			   ParseDictTabInfoRecord * parseP)
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
    AttributeRecord tmpAttr;
    {
      strcpy(tmpAttr.attributeName, attrDesc.AttributeName); 
      
      AttributeRecordPtr attrPtr;
      c_attributeRecordHash.find(attrPtr, tmpAttr);
      
      if(attrPtr.i != RNIL){
	parseP->errorCode = CreateTableRef::AttributeNameTwice;
	return;
      }
    }
    
    if(!getNewAttributeRecord(tablePtr, attrPtr)){
      jam();
      parseP->errorCode = CreateTableRef::NoMoreAttributeRecords;
      return;
    }
    
    /**
     * TmpAttrib to Attribute mapping
     */
    strcpy(attrPtr.p->attributeName, attrDesc.AttributeName);
    attrPtr.p->attributeId = attrDesc.AttributeId;
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
    AttributeDescriptor::setArray(desc, attrDesc.AttributeArraySize);
    AttributeDescriptor::setNullable(desc, attrDesc.AttributeNullableFlag);
    AttributeDescriptor::setDKey(desc, attrDesc.AttributeDKey);
    AttributeDescriptor::setPrimaryKey(desc, attrDesc.AttributeKeyFlag);
    attrPtr.p->attributeDescriptor = desc;
    attrPtr.p->autoIncrement = attrDesc.AttributeAutoIncrement;
    strcpy(attrPtr.p->defaultValue, attrDesc.AttributeDefaultValue);
    
    tabRequire(attrDesc.AttributeId == i, CreateTableRef::InvalidFormat);
    
    attrCount ++;
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
    
    if (parseP->requestType != DictTabInfo::AlterTableFromAPI)
      c_attributeRecordHash.add(attrPtr);
    
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
  
}//handleTabInfo()


/* ---------------------------------------------------------------- */
// DICTTABCONF is sent when participants have received all DICTTABINFO
// and successfully handled it.
// Also sent to self (DICT master) when index table creation ready.
/* ---------------------------------------------------------------- */
void Dbdict::execCREATE_TABLE_CONF(Signal* signal) 
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);

  CreateTableConf * const conf = (CreateTableConf *)signal->getDataPtr();
  // assume part of create index operation
  OpCreateIndexPtr opPtr;
  c_opCreateIndex.find(opPtr, conf->senderData);
  ndbrequire(! opPtr.isNull());
  opPtr.p->m_request.setIndexId(conf->tableId);
  opPtr.p->m_request.setIndexVersion(conf->tableVersion);
  createIndex_fromCreateTable(signal, opPtr);
}//execCREATE_TABLE_CONF()

void Dbdict::execCREATE_TABLE_REF(Signal* signal) 
{
  jamEntry();

  CreateTableRef * const ref = (CreateTableRef *)signal->getDataPtr();
  // assume part of create index operation
  OpCreateIndexPtr opPtr;
  c_opCreateIndex.find(opPtr, ref->senderData);
  ndbrequire(! opPtr.isNull());
  opPtr.p->setError(ref);
  createIndex_fromCreateTable(signal, opPtr);
}//execCREATE_TABLE_REF()

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


/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* MODULE:          DROP TABLE                 -------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* This module contains the code used to drop a table.              */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void
Dbdict::execDROP_TABLE_REQ(Signal* signal){
  jamEntry();
  DropTableReq* req = (DropTableReq*)signal->getDataPtr();

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, req->tableId, false);
  if(tablePtr.isNull()){
    jam();
    dropTableRef(signal, req, DropTableRef::NoSuchTable);
    return;
  }

  if(getOwnNodeId() != c_masterNodeId){
    jam();
    dropTableRef(signal, req, DropTableRef::NotMaster);
    return;
  }

  if(c_blockState == BS_NODE_RESTART){
    jam();
    dropTableRef(signal, req, DropTableRef::BusyWithNR);
    return;
  }

  if(c_blockState != BS_IDLE){
    jam();
    dropTableRef(signal, req, DropTableRef::Busy);
    return;
  }
  
  const TableRecord::TabState tabState = tablePtr.p->tabState;
  bool ok = false;
  switch(tabState){
  case TableRecord::NOT_DEFINED:
  case TableRecord::REORG_TABLE_PREPARED:
  case TableRecord::DEFINING:
  case TableRecord::CHECKED:
    jam();
    dropTableRef(signal, req, DropTableRef::NoSuchTable);
    return;
  case TableRecord::DEFINED:
    ok = true;
    jam();
    break;
  case TableRecord::PREPARE_DROPPING:
  case TableRecord::DROPPING:
    jam();
    dropTableRef(signal, req, DropTableRef::DropInProgress);
    return;
  case TableRecord::BACKUP_ONGOING:
    jam();
    dropTableRef(signal, req, DropTableRef::BackupInProgress);
    return;
  }
  ndbrequire(ok);

  if(tablePtr.p->tableVersion != req->tableVersion){
    jam();
    dropTableRef(signal, req, DropTableRef::InvalidTableVersion);
    return;
  }

  /**
   * Seems ok
   */
  DropTableRecordPtr dropTabPtr;
  c_opDropTable.seize(dropTabPtr);
  
  if(dropTabPtr.isNull()){
    jam();
    dropTableRef(signal, req, DropTableRef::NoDropTableRecordAvailable);
    return;
  }

  c_blockState = BS_BUSY;
  
  dropTabPtr.p->key = ++c_opRecordSequence;
  c_opDropTable.add(dropTabPtr);

  dropTabPtr.p->m_request = * req;
  dropTabPtr.p->m_errorCode = 0;
  dropTabPtr.p->m_requestType = DropTabReq::OnlineDropTab;
  dropTabPtr.p->m_coordinatorRef = reference();
  dropTabPtr.p->m_coordinatorData.m_gsn = GSN_PREP_DROP_TAB_REQ;
  dropTabPtr.p->m_coordinatorData.m_block = 0;
  
  Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
  Callback c = { safe_cast(&Dbdict::dropTable_backup_mutex_locked),
		 dropTabPtr.p->key};
  
  ndbrequire(mutex.lock(c));

}

void
Dbdict::dropTable_backup_mutex_locked(Signal* signal, 
				      Uint32 callbackData,
				      Uint32 retValue){
  jamEntry();
  
  ndbrequire(retValue == 0);
  
  DropTableRecordPtr dropTabPtr;
  ndbrequire(c_opDropTable.find(dropTabPtr, callbackData));
  
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, dropTabPtr.p->m_request.tableId, true);
  
  Mutex mutex(signal, c_mutexMgr, dropTabPtr.p->m_define_backup_mutex);
  mutex.unlock(); // ignore response
  
  if(tablePtr.p->tabState == TableRecord::BACKUP_ONGOING)
  {
    jam();
    dropTableRef(signal, &dropTabPtr.p->m_request,
		 DropTableRef::BackupInProgress);
    
    c_blockState = BS_IDLE;
    c_opDropTable.release(dropTabPtr);
  }
  else
  {
    jam();
    tablePtr.p->tabState = TableRecord::PREPARE_DROPPING;
    prepDropTab_nextStep(signal, dropTabPtr);
  }
}

void
Dbdict::dropTableRef(Signal * signal, 
		     DropTableReq * req, DropTableRef::ErrorCode errCode){

  Uint32 tableId = req->tableId;
  Uint32 tabVersion = req->tableVersion;
  Uint32 senderData = req->senderData;
  Uint32 senderRef = req->senderRef;
  
  DropTableRef * ref = (DropTableRef*)signal->getDataPtrSend();
  ref->tableId = tableId;
  ref->tableVersion = tabVersion;
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = errCode;
  ref->masterNodeId = c_masterNodeId;
  sendSignal(senderRef, GSN_DROP_TABLE_REF, signal, 
	     DropTableRef::SignalLength, JBB);
}

void
Dbdict::prepDropTab_nextStep(Signal* signal, DropTableRecordPtr dropTabPtr){
  
  /**
   * No errors currently allowed
   */
  ndbrequire(dropTabPtr.p->m_errorCode == 0);

  Uint32 block = 0;
  switch(dropTabPtr.p->m_coordinatorData.m_block){
  case 0:
    jam();
    block = dropTabPtr.p->m_coordinatorData.m_block = DBDICT;
    break;
  case DBDICT:
    jam();
    block = dropTabPtr.p->m_coordinatorData.m_block = DBLQH;
    break;
  case DBLQH:
    jam();
    block = dropTabPtr.p->m_coordinatorData.m_block = DBTC;
    break;
  case DBTC:
    jam();
    block = dropTabPtr.p->m_coordinatorData.m_block = DBDIH;
    break;
  case DBDIH:
    jam();
    prepDropTab_complete(signal, dropTabPtr);
    return;
  default:
    ndbrequire(false);
  }

  PrepDropTabReq * prep = (PrepDropTabReq*)signal->getDataPtrSend();
  prep->senderRef = reference();
  prep->senderData = dropTabPtr.p->key;
  prep->tableId = dropTabPtr.p->m_request.tableId;
  prep->requestType = dropTabPtr.p->m_requestType;
  
  dropTabPtr.p->m_coordinatorData.m_signalCounter = c_aliveNodes;
  NodeReceiverGroup rg(block, c_aliveNodes);
  sendSignal(rg, GSN_PREP_DROP_TAB_REQ, signal, 
	     PrepDropTabReq::SignalLength, JBB);
  
#if 0  
  for (Uint32 i = 1; i < MAX_NDB_NODES; i++){
    if(c_aliveNodes.get(i)){
      jam();
      BlockReference ref = numberToRef(block, i);
      
      dropTabPtr.p->m_coordinatorData.m_signalCounter.setWaitingFor(i);
    }
  }
#endif
}

void
Dbdict::execPREP_DROP_TAB_CONF(Signal * signal){
  jamEntry();

  PrepDropTabConf * prep = (PrepDropTabConf*)signal->getDataPtr();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, prep->senderData));
  
  ndbrequire(dropTabPtr.p->m_coordinatorRef == reference());
  ndbrequire(dropTabPtr.p->m_request.tableId == prep->tableId);
  ndbrequire(dropTabPtr.p->m_coordinatorData.m_gsn == GSN_PREP_DROP_TAB_REQ);
  
  Uint32 nodeId = refToNode(prep->senderRef);
  dropTabPtr.p->m_coordinatorData.m_signalCounter.clearWaitingFor(nodeId);
  
  if(!dropTabPtr.p->m_coordinatorData.m_signalCounter.done()){
    jam();
    return;
  }
  prepDropTab_nextStep(signal, dropTabPtr);
}

void
Dbdict::execPREP_DROP_TAB_REF(Signal* signal){
  jamEntry();

  PrepDropTabRef * prep = (PrepDropTabRef*)signal->getDataPtr();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, prep->senderData));
  
  ndbrequire(dropTabPtr.p->m_coordinatorRef == reference());
  ndbrequire(dropTabPtr.p->m_request.tableId == prep->tableId);
  ndbrequire(dropTabPtr.p->m_coordinatorData.m_gsn == GSN_PREP_DROP_TAB_REQ);
  
  Uint32 nodeId = refToNode(prep->senderRef);
  dropTabPtr.p->m_coordinatorData.m_signalCounter.clearWaitingFor(nodeId);
  
  Uint32 block = refToBlock(prep->senderRef);
  if((prep->errorCode == PrepDropTabRef::NoSuchTable && block == DBLQH) ||
     (prep->errorCode == PrepDropTabRef::NF_FakeErrorREF)){
    jam();
    /**
     * Ignore errors:
     * 1) no such table and LQH, it might not exists in different LQH's
     * 2) node failure...
     */
  } else {
    dropTabPtr.p->setErrorCode((Uint32)prep->errorCode);
  }
  
  if(!dropTabPtr.p->m_coordinatorData.m_signalCounter.done()){
    jam();
    return;
  }
  prepDropTab_nextStep(signal, dropTabPtr);
}

void
Dbdict::prepDropTab_complete(Signal* signal, DropTableRecordPtr dropTabPtr){
  jam();

  dropTabPtr.p->m_coordinatorData.m_gsn = GSN_DROP_TAB_REQ;
  dropTabPtr.p->m_coordinatorData.m_block = DBDICT;
  
  DropTabReq * req = (DropTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = dropTabPtr.p->key;
  req->tableId = dropTabPtr.p->m_request.tableId;
  req->requestType = dropTabPtr.p->m_requestType;

  dropTabPtr.p->m_coordinatorData.m_signalCounter = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  sendSignal(rg, GSN_DROP_TAB_REQ, signal, 
	     DropTabReq::SignalLength, JBB);
}

void
Dbdict::execDROP_TAB_REF(Signal* signal){
  jamEntry();

  DropTabRef * const req = (DropTabRef*)signal->getDataPtr();

  Uint32 block = refToBlock(req->senderRef);
  ndbrequire(req->errorCode == DropTabRef::NF_FakeErrorREF ||
	     (req->errorCode == DropTabRef::NoSuchTable &&
	      (block == DBTUP || block == DBACC || block == DBLQH)));
  
  if(block != DBDICT){
    jam();
    ndbrequire(refToNode(req->senderRef) == getOwnNodeId());
    dropTab_localDROP_TAB_CONF(signal);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::execDROP_TAB_CONF(Signal* signal){
  jamEntry();

  DropTabConf * const req = (DropTabConf*)signal->getDataPtr();

  if(refToBlock(req->senderRef) != DBDICT){
    jam();
    ndbrequire(refToNode(req->senderRef) == getOwnNodeId());
    dropTab_localDROP_TAB_CONF(signal);
    return;
  }

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, req->senderData));
  
  ndbrequire(dropTabPtr.p->m_coordinatorRef == reference());
  ndbrequire(dropTabPtr.p->m_request.tableId == req->tableId);
  ndbrequire(dropTabPtr.p->m_coordinatorData.m_gsn == GSN_DROP_TAB_REQ);

  Uint32 nodeId = refToNode(req->senderRef);
  dropTabPtr.p->m_coordinatorData.m_signalCounter.clearWaitingFor(nodeId);
  
  if(!dropTabPtr.p->m_coordinatorData.m_signalCounter.done()){
    jam();
    return;
  }
  
  DropTableConf* conf = (DropTableConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = dropTabPtr.p->m_request.senderData;
  conf->tableId = dropTabPtr.p->m_request.tableId;
  conf->tableVersion = dropTabPtr.p->m_request.tableVersion;
  
  Uint32 ref = dropTabPtr.p->m_request.senderRef;
  sendSignal(ref, GSN_DROP_TABLE_CONF, signal, 
	     DropTableConf::SignalLength, JBB);

  c_opDropTable.release(dropTabPtr);
  c_blockState = BS_IDLE;
}

/**
 * DROP TABLE PARTICIPANT CODE
 */
void
Dbdict::execPREP_DROP_TAB_REQ(Signal* signal){
  jamEntry();
  PrepDropTabReq * prep = (PrepDropTabReq*)signal->getDataPtrSend();  

  DropTableRecordPtr dropTabPtr;  
  if(prep->senderRef == reference()){
    jam();
    ndbrequire(c_opDropTable.find(dropTabPtr, prep->senderData));
    ndbrequire(dropTabPtr.p->m_requestType == prep->requestType);
  } else {
    jam();
    c_opDropTable.seize(dropTabPtr);
    if(!dropTabPtr.isNull()){
      dropTabPtr.p->key = prep->senderData;
      c_opDropTable.add(dropTabPtr);
    }
  }
  
  ndbrequire(!dropTabPtr.isNull());

  dropTabPtr.p->m_errorCode = 0;
  dropTabPtr.p->m_request.tableId = prep->tableId;
  dropTabPtr.p->m_requestType = prep->requestType;
  dropTabPtr.p->m_coordinatorRef = prep->senderRef;
  dropTabPtr.p->m_participantData.m_gsn = GSN_PREP_DROP_TAB_REQ;

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, prep->tableId);
  tablePtr.p->tabState = TableRecord::PREPARE_DROPPING;
  
  /**
   * Modify schema
   */
  XSchemaFile * xsf = &c_schemaFile[c_schemaRecord.schemaPage != 0];
  SchemaFile::TableEntry * tableEntry = getTableEntry(xsf, tablePtr.i);
  SchemaFile::TableState tabState = 
    (SchemaFile::TableState)tableEntry->m_tableState;
  ndbrequire(tabState == SchemaFile::TABLE_ADD_COMMITTED ||
	     tabState == SchemaFile::ALTER_TABLE_COMMITTED);
  tableEntry->m_tableState   = SchemaFile::DROP_TABLE_STARTED;
  computeChecksum(xsf, tablePtr.i / NDB_SF_PAGE_ENTRIES);
  
  ndbrequire(c_writeSchemaRecord.inUse == false);
  c_writeSchemaRecord.inUse = true;
  
  c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
  c_writeSchemaRecord.newFile = false;
  c_writeSchemaRecord.firstPage = tablePtr.i / NDB_SF_PAGE_ENTRIES;
  c_writeSchemaRecord.noOfPages = 1;
  c_writeSchemaRecord.m_callback.m_callbackData = dropTabPtr.p->key;
  c_writeSchemaRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::prepDropTab_writeSchemaConf);
  startWriteSchemaFile(signal);
}

void
Dbdict::prepDropTab_writeSchemaConf(Signal* signal, 
				    Uint32 dropTabPtrI,
				    Uint32 returnCode){
  jam();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, dropTabPtrI));

  ndbrequire(dropTabPtr.p->m_participantData.m_gsn == GSN_PREP_DROP_TAB_REQ);
  
  /**
   * There probably should be node fail handlign here
   *
   * To check that coordinator hasn't died
   */
  
  PrepDropTabConf * prep = (PrepDropTabConf*)signal->getDataPtr();  
  prep->senderRef = reference();
  prep->senderData = dropTabPtrI;
  prep->tableId = dropTabPtr.p->m_request.tableId;
  
  dropTabPtr.p->m_participantData.m_gsn = GSN_PREP_DROP_TAB_CONF;
  sendSignal(dropTabPtr.p->m_coordinatorRef, GSN_PREP_DROP_TAB_CONF, signal, 
	     PrepDropTabConf::SignalLength, JBB);
}

void
Dbdict::execDROP_TAB_REQ(Signal* signal){
  jamEntry();
  DropTabReq * req = (DropTabReq*)signal->getDataPtrSend();  

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, req->senderData));

  ndbrequire(dropTabPtr.p->m_participantData.m_gsn == GSN_PREP_DROP_TAB_CONF);
  dropTabPtr.p->m_participantData.m_gsn = GSN_DROP_TAB_REQ;

  ndbrequire(dropTabPtr.p->m_requestType == req->requestType);

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, dropTabPtr.p->m_request.tableId);
  tablePtr.p->tabState = TableRecord::DROPPING;

  dropTabPtr.p->m_participantData.m_block = 0;
  dropTabPtr.p->m_participantData.m_callback.m_callbackData = dropTabPtr.p->key;
  dropTabPtr.p->m_participantData.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropTab_complete);
  dropTab_nextStep(signal, dropTabPtr);  
}

#include <DebuggerNames.hpp>

void
Dbdict::dropTab_nextStep(Signal* signal, DropTableRecordPtr dropTabPtr){

  /**
   * No errors currently allowed
   */
  ndbrequire(dropTabPtr.p->m_errorCode == 0);

  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, dropTabPtr.p->m_request.tableId);

  Uint32 block = 0;
  switch(dropTabPtr.p->m_participantData.m_block){
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
    execute(signal, dropTabPtr.p->m_participantData.m_callback, 0);
    return;
  }
  ndbrequire(block != 0);
  dropTabPtr.p->m_participantData.m_block = block;

  DropTabReq * req = (DropTabReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = dropTabPtr.p->key;
  req->tableId = dropTabPtr.p->m_request.tableId;
  req->requestType = dropTabPtr.p->m_requestType;
  
  const Uint32 nodeId = getOwnNodeId();
  dropTabPtr.p->m_participantData.m_signalCounter.clearWaitingFor();
  dropTabPtr.p->m_participantData.m_signalCounter.setWaitingFor(nodeId);
  BlockReference ref = numberToRef(block, 0);
  sendSignal(ref, GSN_DROP_TAB_REQ, signal, DropTabReq::SignalLength, JBB);
}

void
Dbdict::dropTab_localDROP_TAB_CONF(Signal* signal){
  jamEntry();
  
  DropTabConf * conf = (DropTabConf*)signal->getDataPtr();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, conf->senderData));
  
  ndbrequire(dropTabPtr.p->m_request.tableId == conf->tableId);
  ndbrequire(dropTabPtr.p->m_participantData.m_gsn == GSN_DROP_TAB_REQ);
  
  Uint32 nodeId = refToNode(conf->senderRef);
  dropTabPtr.p->m_participantData.m_signalCounter.clearWaitingFor(nodeId);
  
  if(!dropTabPtr.p->m_participantData.m_signalCounter.done()){
    jam();
    ndbrequire(false);
    return;
  }
  dropTab_nextStep(signal, dropTabPtr);
}

void
Dbdict::dropTab_complete(Signal* signal, 
			 Uint32 dropTabPtrI,
			 Uint32 returnCode){
  jam();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, dropTabPtrI));
  
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
  computeChecksum(xsf, tableId / NDB_SF_PAGE_ENTRIES);
  
  ndbrequire(c_writeSchemaRecord.inUse == false);
  c_writeSchemaRecord.inUse = true;

  c_writeSchemaRecord.pageId = c_schemaRecord.schemaPage;
  c_writeSchemaRecord.firstPage = tableId / NDB_SF_PAGE_ENTRIES;
  c_writeSchemaRecord.noOfPages = 1;
  c_writeSchemaRecord.m_callback.m_callbackData = dropTabPtr.p->key;
  c_writeSchemaRecord.m_callback.m_callbackFunction = 
    safe_cast(&Dbdict::dropTab_writeSchemaConf);
  startWriteSchemaFile(signal);
}

void
Dbdict::dropTab_writeSchemaConf(Signal* signal, 
				Uint32 dropTabPtrI,
				Uint32 returnCode){
  jam();

  DropTableRecordPtr dropTabPtr;  
  ndbrequire(c_opDropTable.find(dropTabPtr, dropTabPtrI));

  ndbrequire(dropTabPtr.p->m_participantData.m_gsn == GSN_DROP_TAB_REQ);

  dropTabPtr.p->m_participantData.m_gsn = GSN_DROP_TAB_CONF;

  releaseTableObject(dropTabPtr.p->m_request.tableId);

  DropTabConf * conf = (DropTabConf*)signal->getDataPtr();  
  conf->senderRef = reference();
  conf->senderData = dropTabPtrI;
  conf->tableId = dropTabPtr.p->m_request.tableId;
  
  dropTabPtr.p->m_participantData.m_gsn = GSN_DROP_TAB_CONF;
  sendSignal(dropTabPtr.p->m_coordinatorRef, GSN_DROP_TAB_CONF, signal, 
	     DropTabConf::SignalLength, JBB);
  
  if(dropTabPtr.p->m_coordinatorRef != reference()){
    c_opDropTable.release(dropTabPtr);
  }
}

void Dbdict::releaseTableObject(Uint32 tableId, bool removeFromHash) 
{
  TableRecordPtr tablePtr;
  AttributeRecordPtr attrPtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);
  if (removeFromHash)
  {
#ifdef VM_TRACE
    TableRecordPtr tmp;
    ndbrequire(c_tableRecordHash.find(tmp, * tablePtr.p));
#endif
    c_tableRecordHash.remove(tablePtr);
  }
  tablePtr.p->tabState = TableRecord::NOT_DEFINED;

  Uint32 nextAttrRecord = tablePtr.p->firstAttribute;
  while (nextAttrRecord != RNIL) {
    jam();
/* ---------------------------------------------------------------- */
// Release all attribute records
/* ---------------------------------------------------------------- */
    c_attributeRecordPool.getPtr(attrPtr, nextAttrRecord);
    nextAttrRecord = attrPtr.p->nextAttrInTable;
    c_attributeRecordPool.release(attrPtr);
  }//if
#ifdef HAVE_TABLE_REORG
  Uint32 secondTableId = tablePtr.p->secondTable;
  initialiseTableRecord(tablePtr);
  c_tableRecordPool.getPtr(tablePtr, secondTableId);
  initialiseTableRecord(tablePtr);
#endif
  return; 
}//releaseTableObject()

/**
 * DICT receives these on index create and drop.
 */
void Dbdict::execDROP_TABLE_CONF(Signal* signal) 
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);

  DropTableConf * const conf = (DropTableConf *)signal->getDataPtr();
  // assume part of drop index operation
  OpDropIndexPtr opPtr;
  c_opDropIndex.find(opPtr, conf->senderData);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_request.getIndexId() == conf->tableId);
  ndbrequire(opPtr.p->m_request.getIndexVersion() == conf->tableVersion);
  dropIndex_fromDropTable(signal, opPtr);
}

void Dbdict::execDROP_TABLE_REF(Signal* signal) 
{
  jamEntry();

  DropTableRef * const ref = (DropTableRef *)signal->getDataPtr();
  // assume part of drop index operation
  OpDropIndexPtr opPtr;
  c_opDropIndex.find(opPtr, ref->senderData);
  ndbrequire(! opPtr.isNull());
  opPtr.p->setError(ref);
  opPtr.p->m_errorLine = __LINE__;
  dropIndex_fromDropTable(signal, opPtr);
}

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
  TableRecord keyRecord;
  SegmentedSectionPtr ssPtr;
  signal->getSection(ssPtr,GetTableIdReq::TABLE_NAME);
  copy((Uint32*)tableName, ssPtr);
  strcpy(keyRecord.tableName, tableName);
  releaseSections(signal);

  if(len > sizeof(keyRecord.tableName)){
    jam();
    sendGET_TABLEID_REF((Signal*)signal, 
			(GetTableIdReq *)req, 
			GetTableIdRef::TableNameTooLong);
    return;
  }
  
  TableRecordPtr tablePtr;
  if(!c_tableRecordHash.find(tablePtr, keyRecord)) {
    jam();
    sendGET_TABLEID_REF((Signal*)signal, 
			(GetTableIdReq *)req, 
			GetTableIdRef::TableNotDefined);
    return;
  }
  GetTableIdConf * conf = (GetTableIdConf *)req;
  conf->tableId               = tablePtr.p->tableId;
  conf->schemaVersion         = tablePtr.p->tableVersion;
  conf->senderData            = senderData;
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
  ref->err  = errorCode;
  sendSignal(retRef, GSN_GET_TABLEID_REF, signal, 
	     GetTableIdRef::SignalLength, JBB);
}//sendGET_TABINFOREF()

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
    
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::Busy);
    return;
  }
  
  if(fromTimeQueue){
    jam();
    c_retrieveRecord.noOfWaiters--;
  } 

  const bool useLongSig = (req->requestType & GetTabInfoReq::LongSignalConf);
  const Uint32 reqType = req->requestType & (~GetTabInfoReq::LongSignalConf);
  
  TableRecordPtr tablePtr;
  if(reqType == GetTabInfoReq::RequestByName){
    jam();
    ndbrequire(signal->getNoOfSections() == 1);  
    const Uint32 len = req->tableNameLen;
    
    TableRecord keyRecord;
    if(len > sizeof(keyRecord.tableName)){
      jam();
      releaseSections(signal);
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNameTooLong);
      return;
    }

    char tableName[MAX_TAB_NAME_SIZE];
    SegmentedSectionPtr ssPtr;
    signal->getSection(ssPtr,GetTabInfoReq::TABLE_NAME);
    SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
    r0.reset(); // undo implicit first()
    if(r0.getWords((Uint32*)tableName, ((len + 3)/4)))
      memcpy(keyRecord.tableName, tableName, len);
    else {
      jam();
      releaseSections(signal);
      sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined);
      return;
    }
    releaseSections(signal);
    //    memcpy(keyRecord.tableName, req->tableName, len);
    //ntohS(&keyRecord.tableName[0], len);
   
    c_tableRecordHash.find(tablePtr, keyRecord);
  } else {
    jam();
    c_tableRecordPool.getPtr(tablePtr, req->tableId, false);
  }
  
  // The table seached for was not found
  if(tablePtr.i == RNIL){
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::InvalidTableId);
    return;
  }//if
  
  if (! (tablePtr.p->tabState == TableRecord::DEFINED ||
	 tablePtr.p->tabState == TableRecord::BACKUP_ONGOING)) {
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::TableNotDefined);
    return;
  }//if
  
  c_retrieveRecord.busyState = true;
  c_retrieveRecord.blockRef = req->senderRef;
  c_retrieveRecord.m_senderData = req->senderData;
  c_retrieveRecord.tableId = tablePtr.i;
  c_retrieveRecord.currentSent = 0;
  c_retrieveRecord.m_useLongSig = useLongSig;
  
  c_packTable.m_state = PackTable::PTS_GET_TAB;
  
  signal->theData[0] = ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tablePtr.i;
  signal->theData[2] = c_retrieveRecord.retrievePage;  
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
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
				GetTabInfoRef::ErrorCode errorCode) 
{
  jamEntry();
  GetTabInfoRef * const ref = (GetTabInfoRef *)&signal->theData[0];
  /**
   * The format of GetTabInfo Req/Ref is the same
   */
  BlockReference retRef = req->senderRef;
  ref->errorCode = errorCode;
  
  sendSignal(retRef, GSN_GET_TABINFOREF, signal, signal->length(), JBB);
}//sendGET_TABINFOREF()

Uint32 convertEndian(Uint32 in) {
#ifdef WORDS_BIGENDIAN
  Uint32 ut = 0;
  ut += ((in >> 24) & 255);
  ut += (((in >> 16) & 255) << 8);
  ut += (((in >> 8) & 255) << 16);
  ut += ((in & 255) << 24);
  return ut;
#else
  return in;
#endif
}
void
Dbdict::execLIST_TABLES_REQ(Signal* signal)
{
  jamEntry();
  Uint32 i;
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
  for (i = 0; i < c_tableRecordPool.getSize(); i++) {
    TableRecordPtr tablePtr;
    c_tableRecordPool.getPtr(tablePtr, i);
    // filter
    if (tablePtr.p->tabState == TableRecord::NOT_DEFINED ||
        tablePtr.p->tabState == TableRecord::REORG_TABLE_PREPARED)
      continue;


    if ((reqTableType != (Uint32)0) && (reqTableType != (unsigned)tablePtr.p->tableType))
      continue;
    if (reqListIndexes && reqTableId != tablePtr.p->primaryTableId)
      continue;
    conf->tableData[pos] = 0;
    // id
    conf->setTableId(pos, tablePtr.i);
    // type
    conf->setTableType(pos, tablePtr.p->tableType);
    // state
    if (tablePtr.p->isTable()) {
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
    // store
    if (! tablePtr.p->storedTable) {
      conf->setTableStore(pos, DictTabInfo::StoreTemporary);
    } else {
      conf->setTableStore(pos, DictTabInfo::StorePermanent);
    }
    pos++;
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    if (! reqListNames)
      continue;
    const Uint32 size = strlen(tablePtr.p->tableName) + 1;
    conf->tableData[pos] = size;
    pos++;
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    Uint32 k = 0;
    while (k < size) {
      char* p = (char*)&conf->tableData[pos];
      for (Uint32 j = 0; j < 4; j++) {
        if (k < size)
          *p++ = tablePtr.p->tableName[k++];
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
  // XXX merge with above somehow
  for (i = 0; i < c_triggerRecordPool.getSize(); i++) {
    if (reqListIndexes)
      break;
    TriggerRecordPtr triggerPtr;
    c_triggerRecordPool.getPtr(triggerPtr, i);
    if (triggerPtr.p->triggerState == TriggerRecord::TS_NOT_DEFINED)
      continue;
    // constant 10 hardcoded
    Uint32 type = 10 + triggerPtr.p->triggerType;
    if (reqTableType != 0 && reqTableType != type)
      continue;
    conf->tableData[pos] = 0;
    conf->setTableId(pos, triggerPtr.i);
    conf->setTableType(pos, type);
    switch (triggerPtr.p->triggerState) {
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
    conf->setTableStore(pos, DictTabInfo::StoreTemporary);
    pos++;
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
        ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    if (! reqListNames)
      continue;
    const Uint32 size = strlen(triggerPtr.p->triggerName) + 1;
    conf->tableData[pos] = size;
    pos++;
    if (pos >= ListTablesConf::DataLength) {
      sendSignal(senderRef, GSN_LIST_TABLES_CONF, signal,
		 ListTablesConf::SignalLength, JBB);
      conf->counter++;
      pos = 0;
    }
    Uint32 k = 0;
    while (k < size) {
      char* p = (char*)&conf->tableData[pos];
      for (Uint32 j = 0; j < 4; j++) {
        if (k < size)
          *p++ = triggerPtr.p->triggerName[k++];
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
      } else if (c_blockState == BS_NODE_RESTART) {
        jam();
        tmperr = CreateIndxRef::BusyWithNR;
      } else if (c_blockState != BS_IDLE) {
        jam();
        tmperr = CreateIndxRef::Busy;
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
    DictTabInfo::Table tableDesc;
    tableDesc.init();
    SimpleProperties::UnpackStatus status = SimpleProperties::unpack(
        r1, &tableDesc,
        DictTabInfo::TableMapping, DictTabInfo::TableMappingSize,
        true, true);
    if (status != SimpleProperties::Eof) {
      opPtr.p->m_errorCode = CreateIndxRef::InvalidName;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      createIndex_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    memcpy(opPtr.p->m_indexName, tableDesc.TableName, MAX_TAB_NAME_SIZE);
    opPtr.p->m_storedIndex = tableDesc.TableLoggedFlag;
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
  Uint32 attrid_map[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 k;
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
  // compute index table record
  TableRecord indexRec;
  TableRecordPtr indexPtr;
  indexPtr.i = RNIL;            // invalid
  indexPtr.p = &indexRec;
  initialiseTableRecord(indexPtr);
  if (req->getIndexType() == DictTabInfo::UniqueHashIndex) {
    indexPtr.p->storedTable = opPtr.p->m_storedIndex;
    indexPtr.p->fragmentType = tablePtr.p->fragmentType;
  } else if (req->getIndexType() == DictTabInfo::OrderedIndex) {
    // first version will not supported logging
    if (opPtr.p->m_storedIndex) {
      jam();
      opPtr.p->m_errorCode = CreateIndxRef::InvalidIndexType;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
    indexPtr.p->storedTable = false;
    // follows table fragmentation
    indexPtr.p->fragmentType = tablePtr.p->fragmentType;
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
    AttributeRecord* aRec= NULL;
    Uint32 tAttr= tablePtr.p->firstAttribute;
    for (; tAttr != RNIL; tAttr= aRec->nextAttrInTable)
    {
      aRec = c_attributeRecordPool.getPtr(tAttr);
      if (aRec->attributeId != current_id)
        continue;
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
    mask.set(current_id);

    const Uint32 a = aRec->attributeDescriptor;
    unsigned kk= k;
    if (indexPtr.p->isHashIndex()) {
      const Uint32 s1 = AttributeDescriptor::getSize(a);
      const Uint32 s2 = AttributeDescriptor::getArraySize(a);
      indexPtr.p->tupKeyLength += ((1 << s1) * s2 + 31) >> 5;
      // reorder the attributes according to the tableid order
      // for unque indexes
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
  w.add(DictTabInfo::TableLoggedFlag, indexPtr.p->storedTable);
  w.add(DictTabInfo::FragmentTypeVal, indexPtr.p->fragmentType);
  w.add(DictTabInfo::TableTypeVal, indexPtr.p->tableType);
  w.add(DictTabInfo::PrimaryTable, tablePtr.p->tableName);
  w.add(DictTabInfo::PrimaryTableId, tablePtr.i);
  w.add(DictTabInfo::NoOfAttributes, indexPtr.p->noOfAttributes);
  w.add(DictTabInfo::NoOfKeyAttr, indexPtr.p->noOfPrimkey);
  w.add(DictTabInfo::NoOfNullable, indexPtr.p->noOfNullAttr);
  w.add(DictTabInfo::KeyLength, indexPtr.p->tupKeyLength);
  // write index key attributes
  AttributeRecordPtr aRecPtr;
  c_attributeRecordPool.getPtr(aRecPtr, tablePtr.p->firstAttribute);
  for (k = 0; k < opPtr.p->m_attrList.sz; k++) {
    // insert the attributes in the order decided above in attrid_map
    // k is new order, current_id is in previous order
    // ToDo: make sure "current_id" is stored with the table and
    // passed up to NdbDictionary
    unsigned current_id= opPtr.p->m_attrList.id[attrid_map[k] & 0xffff];
    jam();
    for (Uint32 tAttr = tablePtr.p->firstAttribute; tAttr != RNIL; ) {
      AttributeRecord* aRec = c_attributeRecordPool.getPtr(tAttr);
      tAttr = aRec->nextAttrInTable;
      if (aRec->attributeId != current_id)
        continue;
      jam();
      const Uint32 a = aRec->attributeDescriptor;
      bool isNullable = AttributeDescriptor::getNullable(a);
      Uint32 attrType = AttributeDescriptor::getType(a);
      w.add(DictTabInfo::AttributeName, aRec->attributeName);
      w.add(DictTabInfo::AttributeId, k);
      if (indexPtr.p->isHashIndex()) {
        w.add(DictTabInfo::AttributeKeyFlag, (Uint32)true);
        w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
      }
      if (indexPtr.p->isOrderedIndex()) {
        w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
        w.add(DictTabInfo::AttributeNullableFlag, (Uint32)isNullable);
      }
      w.add(DictTabInfo::AttributeExtType, attrType);
      w.add(DictTabInfo::AttributeExtPrecision, aRec->extPrecision);
      w.add(DictTabInfo::AttributeExtScale, aRec->extScale);
      w.add(DictTabInfo::AttributeExtLength, aRec->extLength);
      w.add(DictTabInfo::AttributeEnd, (Uint32)true);
    }
  }
  if (indexPtr.p->isHashIndex()) {
    jam();
    // write concatenated primary table key attribute
    w.add(DictTabInfo::AttributeName, "NDB$PK");
    w.add(DictTabInfo::AttributeId, opPtr.p->m_attrList.sz);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)false);
    w.add(DictTabInfo::AttributeExtType, (Uint32)DictTabInfo::ExtUnsigned);
    w.add(DictTabInfo::AttributeExtLength, tablePtr.p->tupKeyLength);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  if (indexPtr.p->isOrderedIndex()) {
    jam();
    // write index tree node as Uint32 array attribute
    w.add(DictTabInfo::AttributeName, "NDB$TNODE");
    w.add(DictTabInfo::AttributeId, opPtr.p->m_attrList.sz);
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
      } else if (c_blockState == BS_NODE_RESTART) {
        jam();
        tmperr = DropIndxRef::BusyWithNR;
      } else if (c_blockState != BS_IDLE) {
        jam();
        tmperr = DropIndxRef::Busy;
      }
      if (tmperr != DropIndxRef::NoError) {
	err = tmperr;
	goto error;
      }
      // forward initial request plus operation key to all
      Uint32 indexId= req->getIndexId();
      Uint32 indexVersion= req->getIndexVersion();
      TableRecordPtr tmp;
      int res = getMetaTablePtr(tmp, indexId,  indexVersion);
      switch(res){
      case MetaData::InvalidArgument:
	err = DropIndxRef::IndexNotFound;
	goto error;
      case MetaData::TableNotFound:
      case MetaData::InvalidTableVersion:
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

/**
 * MODULE: Create trigger
 *
 * Create trigger in all DICT blocks.  Optionally start alter trigger
 * operation to set the trigger online.
 *
 * Request type received in REQ and returned in CONF/REF:
 *
 * RT_USER - normal user e.g. BACKUP
 * RT_ALTER_INDEX - from alter index online
 * RT_DICT_PREPARE - seize operation in each DICT
 * RT_DICT_COMMIT - commit create in each DICT
 * RT_TC - sending to TC (operation alter trigger)
 * RT_LQH - sending to LQH (operation alter trigger)
 */

void
Dbdict::execCREATE_TRIG_REQ(Signal* signal) 
{
  jamEntry();
  CreateTrigReq* const req = (CreateTrigReq*)signal->getDataPtrSend();
  OpCreateTriggerPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const CreateTrigReq::RequestType requestType = req->getRequestType();
  if (requestType == CreateTrigReq::RT_USER ||
      requestType == CreateTrigReq::RT_ALTER_INDEX ||
      requestType == CreateTrigReq::RT_BUILD_INDEX) {
    jam();
    if (! assembleFragments(signal)) {
      jam();
      return;
    }
    const bool isLocal = req->getRequestFlag() & RequestFlag::RF_LOCAL;
    NdbNodeBitmask receiverNodes = c_aliveNodes;
    if (isLocal) {
      receiverNodes.clear();
      receiverNodes.set(getOwnNodeId());
    }
    if (signal->getLength() == CreateTrigReq::SignalLength) {
      jam();
      if (! isLocal && getOwnNodeId() != c_masterNodeId) {
        jam();

	releaseSections(signal);
	OpCreateTrigger opBad;
	opPtr.p = &opBad;
	opPtr.p->save(req);
	opPtr.p->m_errorCode = CreateTrigRef::NotMaster;
	opPtr.p->m_errorLine = __LINE__;
	opPtr.p->m_errorNode = c_masterNodeId;
	createTrigger_sendReply(signal,  opPtr, true);
        return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, receiverNodes);
      sendSignal(rg, GSN_CREATE_TRIG_REQ,
          signal, CreateTrigReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == CreateTrigReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpCreateTrigger opBusy;
    if (! c_opCreateTrigger.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = CreateTrigReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = CreateTrigRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      createTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opCreateTrigger.add(opPtr);
    {
      // save name
      SegmentedSectionPtr ssPtr;
      signal->getSection(ssPtr, CreateTrigReq::TRIGGER_NAME_SECTION);
      SimplePropertiesSectionReader ssReader(ssPtr, getSectionSegmentPool());
      if (ssReader.getKey() != CreateTrigReq::TriggerNameKey ||
	  ! ssReader.getString(opPtr.p->m_triggerName)) {
	jam();
	opPtr.p->m_errorCode = CreateTrigRef::InvalidName;
	opPtr.p->m_errorLine = __LINE__;
	releaseSections(signal);
	createTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
	return;
      }
    }
    releaseSections(signal);
    {
      // check that trigger name is unique
      TriggerRecordPtr triggerPtr;
      TriggerRecord keyRecord;
      strcpy(keyRecord.triggerName, opPtr.p->m_triggerName);
      c_triggerRecordHash.find(triggerPtr, keyRecord);
      if (triggerPtr.i != RNIL) {
	jam();
	opPtr.p->m_errorCode = CreateTrigRef::TriggerExists;
	opPtr.p->m_errorLine = __LINE__;
	createTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
	return;
      }
    }

    // master expects to hear from all
    if (opPtr.p->m_isMaster)
      opPtr.p->m_signalCounter = receiverNodes;
    // check request in all participants
    createTrigger_slavePrepare(signal, opPtr);
    createTrigger_sendReply(signal, opPtr, false);
    return;
  }
  c_opCreateTrigger.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == CreateTrigReq::RT_DICT_CREATE) {
      jam();
      // master has set trigger id
      opPtr.p->m_request.setTriggerId(req->getTriggerId());
      createTrigger_slaveCreate(signal, opPtr);
      createTrigger_sendReply(signal, opPtr, false);
      return;
    }
    if (requestType == CreateTrigReq::RT_DICT_COMMIT ||
        requestType == CreateTrigReq::RT_DICT_ABORT) {
      jam();
      if (requestType == CreateTrigReq::RT_DICT_COMMIT)
        createTrigger_slaveCommit(signal, opPtr);
      else
        createTrigger_slaveAbort(signal, opPtr);
      createTrigger_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opCreateTrigger.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  releaseSections(signal);
  OpCreateTrigger opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = CreateTrigRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  createTrigger_sendReply(signal,  opPtr, true);
}

void
Dbdict::execCREATE_TRIG_CONF(Signal* signal) 
{
  jamEntry();
  ndbrequire(signal->getNoOfSections() == 0);
  CreateTrigConf* conf = (CreateTrigConf*)signal->getDataPtrSend();
  createTrigger_recvReply(signal, conf, 0);
}

void
Dbdict::execCREATE_TRIG_REF(Signal* signal) 
{
  jamEntry();
  CreateTrigRef* ref = (CreateTrigRef*)signal->getDataPtrSend();
  createTrigger_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::createTrigger_recvReply(Signal* signal, const CreateTrigConf* conf,
    const CreateTrigRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const CreateTrigReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == CreateTrigReq::RT_ALTER_INDEX) {
    jam();
    // part of alter index operation
    OpAlterIndexPtr opPtr;
    c_opAlterIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterIndex_fromCreateTrigger(signal, opPtr);
    return;
  }
  if (requestType == CreateTrigReq::RT_BUILD_INDEX) {
    jam();
    // part of build index operation
    OpBuildIndexPtr opPtr;
    c_opBuildIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    // fill in trigger id
    opPtr.p->m_constrTriggerId = conf->getTriggerId();
    buildIndex_fromCreateConstr(signal, opPtr);
    return;
  }
  if (requestType == CreateTrigReq::RT_TC ||
      requestType == CreateTrigReq::RT_LQH) {
    jam();
    // part of alter trigger operation
    OpAlterTriggerPtr opPtr;
    c_opAlterTrigger.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterTrigger_fromCreateLocal(signal, opPtr);
    return;
  }
  OpCreateTriggerPtr opPtr;
  c_opCreateTrigger.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == CreateTrigReq::RT_DICT_COMMIT ||
      requestType == CreateTrigReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    createTrigger_sendReply(signal, opPtr, true);
    c_opCreateTrigger.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = CreateTrigReq::RT_DICT_ABORT;
    createTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  if (requestType == CreateTrigReq::RT_DICT_PREPARE) {
    jam();
    // seize trigger id in master
    createTrigger_masterSeize(signal, opPtr);
    if (opPtr.p->hasError()) {
      jam();
      opPtr.p->m_requestType = CreateTrigReq::RT_DICT_ABORT;
      createTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
    opPtr.p->m_requestType = CreateTrigReq::RT_DICT_CREATE;
    createTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  if (requestType == CreateTrigReq::RT_DICT_CREATE) {
    jam();
    if (opPtr.p->m_request.getOnline()) {
      jam();
      // start alter online
      createTrigger_toAlterTrigger(signal, opPtr);
      return;
    }
    opPtr.p->m_requestType = CreateTrigReq::RT_DICT_COMMIT;
    createTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::createTrigger_slavePrepare(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
  const CreateTrigReq* const req = &opPtr.p->m_request;
  // check trigger type
  if (req->getRequestType() == CreateTrigReq::RT_USER &&
      req->getTriggerType() == TriggerType::SUBSCRIPTION ||
      req->getRequestType() == CreateTrigReq::RT_ALTER_INDEX &&
      req->getTriggerType() == TriggerType::SECONDARY_INDEX ||
      req->getRequestType() == CreateTrigReq::RT_ALTER_INDEX &&
      req->getTriggerType() == TriggerType::ORDERED_INDEX ||
      req->getRequestType() == CreateTrigReq::RT_BUILD_INDEX &&
      req->getTriggerType() == TriggerType::READ_ONLY_CONSTRAINT) {
    ;
  } else {
    jam();
    opPtr.p->m_errorCode = CreateTrigRef::UnsupportedTriggerType;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  // check the table
  const Uint32 tableId = req->getTableId();
  if (! (tableId < c_tableRecordPool.getSize())) {
    jam();
    opPtr.p->m_errorCode = CreateTrigRef::InvalidTable;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, tableId);
  if (tablePtr.p->tabState != TableRecord::DEFINED &&
      tablePtr.p->tabState != TableRecord::BACKUP_ONGOING) {
    jam();
    opPtr.p->m_errorCode = CreateTrigRef::InvalidTable;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
}

void
Dbdict::createTrigger_masterSeize(Signal* signal, OpCreateTriggerPtr opPtr)
{
  TriggerRecordPtr triggerPtr;
  if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL) {
    triggerPtr.i = opPtr.p->m_request.getTriggerId();
  } else {
    triggerPtr.i = getFreeTriggerRecord();
    if (triggerPtr.i == RNIL) {
      jam();
      opPtr.p->m_errorCode = CreateTrigRef::TooManyTriggers;
      opPtr.p->m_errorLine = __LINE__;
      return;
    }
  }
  c_triggerRecordPool.getPtr(triggerPtr);
  initialiseTriggerRecord(triggerPtr);
  triggerPtr.p->triggerState = TriggerRecord::TS_DEFINING;
  opPtr.p->m_request.setTriggerId(triggerPtr.i);
}

void
Dbdict::createTrigger_slaveCreate(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
  const CreateTrigReq* const req = &opPtr.p->m_request;
  // get the trigger record
  const Uint32 triggerId = req->getTriggerId();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  initialiseTriggerRecord(triggerPtr);
  // fill in trigger data
  strcpy(triggerPtr.p->triggerName, opPtr.p->m_triggerName);
  triggerPtr.p->triggerId = triggerId;
  triggerPtr.p->tableId = req->getTableId();
  triggerPtr.p->indexId = RNIL;
  triggerPtr.p->triggerType = req->getTriggerType();
  triggerPtr.p->triggerActionTime = req->getTriggerActionTime();
  triggerPtr.p->triggerEvent = req->getTriggerEvent();
  triggerPtr.p->monitorReplicas = req->getMonitorReplicas();
  triggerPtr.p->monitorAllAttributes = req->getMonitorAllAttributes();
  triggerPtr.p->attributeMask = req->getAttributeMask();
  triggerPtr.p->triggerState = TriggerRecord::TS_OFFLINE;
  // add to hash table
  //  ndbout_c("++++++++++++ Adding trigger id %u, %s", triggerPtr.p->triggerId, triggerPtr.p->triggerName);
  c_triggerRecordHash.add(triggerPtr);
  if (triggerPtr.p->triggerType == TriggerType::SECONDARY_INDEX ||
      triggerPtr.p->triggerType == TriggerType::ORDERED_INDEX) {
    jam();
    // connect to index record  XXX should be done in caller instead
    triggerPtr.p->indexId = req->getIndexId();
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, triggerPtr.p->indexId);
    switch (triggerPtr.p->triggerEvent) {
    case TriggerEvent::TE_INSERT:
      indexPtr.p->insertTriggerId = triggerPtr.p->triggerId;
      break;
    case TriggerEvent::TE_UPDATE:
      indexPtr.p->updateTriggerId = triggerPtr.p->triggerId;
      break;
    case TriggerEvent::TE_DELETE:
      indexPtr.p->deleteTriggerId = triggerPtr.p->triggerId;
      break;
    case TriggerEvent::TE_CUSTOM:
      indexPtr.p->customTriggerId = triggerPtr.p->triggerId;
      break;
    default:
      ndbrequire(false);
      break;
    }
  }
  if (triggerPtr.p->triggerType == TriggerType::READ_ONLY_CONSTRAINT) {
    jam();
    // connect to index record  XXX should be done in caller instead
    triggerPtr.p->indexId = req->getTableId();
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, triggerPtr.p->indexId);
    indexPtr.p->buildTriggerId = triggerPtr.p->triggerId;
  }
}

void
Dbdict::createTrigger_toAlterTrigger(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
  AlterTrigReq* req = (AlterTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(AlterTrigReq::RT_CREATE_TRIGGER);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setTriggerId(opPtr.p->m_request.getTriggerId());
  req->setTriggerInfo(0);       // not used
  req->setOnline(true);
  req->setReceiverRef(opPtr.p->m_request.getReceiverRef());
  sendSignal(reference(), GSN_ALTER_TRIG_REQ,
      signal, AlterTrigReq::SignalLength, JBB);
}

void
Dbdict::createTrigger_fromAlterTrigger(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = CreateTrigReq::RT_DICT_ABORT;
    createTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  opPtr.p->m_requestType = CreateTrigReq::RT_DICT_COMMIT;
  createTrigger_sendSlaveReq(signal, opPtr);
}

void
Dbdict::createTrigger_slaveCommit(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
  const CreateTrigReq* const req = &opPtr.p->m_request;
  // get the trigger record
  const Uint32 triggerId = req->getTriggerId();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  if (! req->getOnline()) {
    triggerPtr.p->triggerState = TriggerRecord::TS_OFFLINE;
  } else {
    ndbrequire(triggerPtr.p->triggerState == TriggerRecord::TS_ONLINE);
  }
}

void
Dbdict::createTrigger_slaveAbort(Signal* signal, OpCreateTriggerPtr opPtr)
{
  jam();
}

void
Dbdict::createTrigger_sendSlaveReq(Signal* signal, OpCreateTriggerPtr opPtr)
{
  CreateTrigReq* const req = (CreateTrigReq*)signal->getDataPtrSend();
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
  sendSignal(rg, GSN_CREATE_TRIG_REQ,
      signal, CreateTrigReq::SignalLength, JBB);
}

void
Dbdict::createTrigger_sendReply(Signal* signal, OpCreateTriggerPtr opPtr,
    bool toUser)
{
  CreateTrigRef* rep = (CreateTrigRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_CREATE_TRIG_CONF;
  Uint32 length = CreateTrigConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == CreateTrigReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = CreateTrigConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  rep->setTriggerId(opPtr.p->m_request.getTriggerId());
  rep->setTriggerInfo(opPtr.p->m_request.getTriggerInfo());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0)
      opPtr.p->m_errorNode = getOwnNodeId();
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_CREATE_TRIG_REF;
    length = CreateTrigRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/**
 * MODULE: Drop trigger.
 */

void
Dbdict::execDROP_TRIG_REQ(Signal* signal) 
{
  jamEntry();
  DropTrigReq* const req = (DropTrigReq*)signal->getDataPtrSend();
  OpDropTriggerPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const DropTrigReq::RequestType requestType = req->getRequestType();

  if (signal->getNoOfSections() > 0) {
    ndbrequire(signal->getNoOfSections() == 1);
    jam();
    TriggerRecord keyRecord;
    OpDropTrigger opTmp;
    opPtr.p=&opTmp;

    SegmentedSectionPtr ssPtr;
    signal->getSection(ssPtr, DropTrigReq::TRIGGER_NAME_SECTION);
    SimplePropertiesSectionReader ssReader(ssPtr, getSectionSegmentPool());
    if (ssReader.getKey() != DropTrigReq::TriggerNameKey ||
	! ssReader.getString(keyRecord.triggerName)) {
      jam();
      opPtr.p->m_errorCode = DropTrigRef::InvalidName;
      opPtr.p->m_errorLine = __LINE__;
      releaseSections(signal);
      dropTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    releaseSections(signal);

    TriggerRecordPtr triggerPtr;

    //    ndbout_c("++++++++++++++ Looking for trigger %s", keyRecord.triggerName);
    c_triggerRecordHash.find(triggerPtr, keyRecord);
    if (triggerPtr.i == RNIL) {
      jam();
      req->setTriggerId(RNIL);
    } else {
      jam();
      //      ndbout_c("++++++++++ Found trigger %s", triggerPtr.p->triggerName);
      req->setTriggerId(triggerPtr.p->triggerId);
      req->setTableId(triggerPtr.p->tableId);
    }
  }
  if (requestType == DropTrigReq::RT_USER ||
      requestType == DropTrigReq::RT_ALTER_INDEX ||
      requestType == DropTrigReq::RT_BUILD_INDEX) {
    jam();
    if (signal->getLength() == DropTrigReq::SignalLength) {
      if (getOwnNodeId() != c_masterNodeId) {
	jam();
	// forward to DICT master
	sendSignal(calcDictBlockRef(c_masterNodeId), GSN_DROP_TRIG_REQ,
		   signal, signal->getLength(), JBB);
	return;
      }
      if (!c_triggerRecordPool.findId(req->getTriggerId())) {
	jam();
	// return to sender
	OpDropTrigger opBad;
	opPtr.p = &opBad;
	opPtr.p->save(req);
        if (! (req->getRequestFlag() & RequestFlag::RF_FORCE)) {
          opPtr.p->m_errorCode = DropTrigRef::TriggerNotFound;
          opPtr.p->m_errorLine = __LINE__;
        }
	dropTrigger_sendReply(signal,  opPtr, true);
	return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, c_aliveNodes);
      sendSignal(rg, GSN_DROP_TRIG_REQ,
		 signal, DropTrigReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == DropTrigReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpDropTrigger opBusy;
    if (! c_opDropTrigger.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = DropTrigReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = DropTrigRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      dropTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opDropTrigger.add(opPtr);
      // master expects to hear from all
    if (opPtr.p->m_isMaster)
	opPtr.p->m_signalCounter = c_aliveNodes;
    dropTrigger_slavePrepare(signal, opPtr);
    dropTrigger_sendReply(signal, opPtr, false);
    return;
  }
  c_opDropTrigger.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == DropTrigReq::RT_DICT_COMMIT ||
	requestType == DropTrigReq::RT_DICT_ABORT) {
      jam();
      if (requestType == DropTrigReq::RT_DICT_COMMIT)
	dropTrigger_slaveCommit(signal, opPtr);
      else
	dropTrigger_slaveAbort(signal, opPtr);
      dropTrigger_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
	c_opDropTrigger.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  OpDropTrigger opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = DropTrigRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  dropTrigger_sendReply(signal,  opPtr, true);
}

void
Dbdict::execDROP_TRIG_CONF(Signal* signal) 
{
  jamEntry();
  DropTrigConf* conf = (DropTrigConf*)signal->getDataPtrSend();
  dropTrigger_recvReply(signal, conf, 0);
}

void
Dbdict::execDROP_TRIG_REF(Signal* signal) 
{
  jamEntry();
  DropTrigRef* ref = (DropTrigRef*)signal->getDataPtrSend();
  dropTrigger_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::dropTrigger_recvReply(Signal* signal, const DropTrigConf* conf,
    const DropTrigRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const DropTrigReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == DropTrigReq::RT_ALTER_INDEX) {
    jam();
    // part of alter index operation
    OpAlterIndexPtr opPtr;
    c_opAlterIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterIndex_fromDropTrigger(signal, opPtr);
    return;
  }
  if (requestType == DropTrigReq::RT_BUILD_INDEX) {
    jam();
    // part of build index operation
    OpBuildIndexPtr opPtr;
    c_opBuildIndex.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    buildIndex_fromDropConstr(signal, opPtr);
    return;
  }
  if (requestType == DropTrigReq::RT_TC ||
      requestType == DropTrigReq::RT_LQH) {
    jam();
    // part of alter trigger operation
    OpAlterTriggerPtr opPtr;
    c_opAlterTrigger.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    alterTrigger_fromDropLocal(signal, opPtr);
    return;
  }
  OpDropTriggerPtr opPtr;
  c_opDropTrigger.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == DropTrigReq::RT_DICT_COMMIT ||
      requestType == DropTrigReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    dropTrigger_sendReply(signal, opPtr, true);
    c_opDropTrigger.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = DropTrigReq::RT_DICT_ABORT;
    dropTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  if (requestType == DropTrigReq::RT_DICT_PREPARE) {
    jam();
    // start alter offline
    dropTrigger_toAlterTrigger(signal, opPtr);
    return;
  }
  ndbrequire(false);
}

void
Dbdict::dropTrigger_slavePrepare(Signal* signal, OpDropTriggerPtr opPtr)
{
  jam();
}

void
Dbdict::dropTrigger_toAlterTrigger(Signal* signal, OpDropTriggerPtr opPtr)
{
  jam();
  AlterTrigReq* req = (AlterTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(AlterTrigReq::RT_DROP_TRIGGER);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  req->setTableId(opPtr.p->m_request.getTableId());
  req->setTriggerId(opPtr.p->m_request.getTriggerId());
  req->setTriggerInfo(0);       // not used
  req->setOnline(false);
  req->setReceiverRef(0);
  sendSignal(reference(), GSN_ALTER_TRIG_REQ,
      signal, AlterTrigReq::SignalLength, JBB);
}

void
Dbdict::dropTrigger_fromAlterTrigger(Signal* signal, OpDropTriggerPtr opPtr)
{
  jam();
  // remove in all
  opPtr.p->m_requestType = DropTrigReq::RT_DICT_COMMIT;
  dropTrigger_sendSlaveReq(signal, opPtr);
}

void
Dbdict::dropTrigger_sendSlaveReq(Signal* signal, OpDropTriggerPtr opPtr)
{
  DropTrigReq* const req = (DropTrigReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  opPtr.p->m_signalCounter = c_aliveNodes;
  NodeReceiverGroup rg(DBDICT, c_aliveNodes);
  sendSignal(rg, GSN_DROP_TRIG_REQ,
      signal, DropTrigReq::SignalLength, JBB);
}

void
Dbdict::dropTrigger_slaveCommit(Signal* signal, OpDropTriggerPtr opPtr)
{
  jam();
  const DropTrigReq* const req = &opPtr.p->m_request;
  // get trigger record
  const Uint32 triggerId = req->getTriggerId();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  if (triggerPtr.p->triggerType == TriggerType::SECONDARY_INDEX ||
      triggerPtr.p->triggerType == TriggerType::ORDERED_INDEX) {
    jam();
    // disconnect from index if index trigger  XXX move to drop index
    triggerPtr.p->indexId = req->getIndexId();
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, triggerPtr.p->indexId);
    ndbrequire(! indexPtr.isNull());
    switch (triggerPtr.p->triggerEvent) {
    case TriggerEvent::TE_INSERT:
      indexPtr.p->insertTriggerId = RNIL;
      break;
    case TriggerEvent::TE_UPDATE:
      indexPtr.p->updateTriggerId = RNIL;
      break;
    case TriggerEvent::TE_DELETE:
      indexPtr.p->deleteTriggerId = RNIL;
      break;
    case TriggerEvent::TE_CUSTOM:
      indexPtr.p->customTriggerId = RNIL;
      break;
    default:
      ndbrequire(false);
      break;
    }
  }
  if (triggerPtr.p->triggerType == TriggerType::READ_ONLY_CONSTRAINT) {
    jam();
    // disconnect from index record  XXX should be done in caller instead
    triggerPtr.p->indexId = req->getTableId();
    TableRecordPtr indexPtr;
    c_tableRecordPool.getPtr(indexPtr, triggerPtr.p->indexId);
    indexPtr.p->buildTriggerId = RNIL;
  }
  // remove trigger
  //  ndbout_c("++++++++++++ Removing trigger id %u, %s", triggerPtr.p->triggerId, triggerPtr.p->triggerName);
  c_triggerRecordHash.remove(triggerPtr);
  triggerPtr.p->triggerState = TriggerRecord::TS_NOT_DEFINED;
}

void
Dbdict::dropTrigger_slaveAbort(Signal* signal, OpDropTriggerPtr opPtr)
{
  jam();
}

void
Dbdict::dropTrigger_sendReply(Signal* signal, OpDropTriggerPtr opPtr,
    bool toUser)
{
  DropTrigRef* rep = (DropTrigRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_DROP_TRIG_CONF;
  Uint32 length = DropTrigConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == DropTrigReq::RT_DICT_ABORT)
      sendRef = false;
  } else {
    sendRef = opPtr.p->hasError();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = DropTrigConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setIndexId(opPtr.p->m_request.getIndexId());
  rep->setTriggerId(opPtr.p->m_request.getTriggerId());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0)
      opPtr.p->m_errorNode = getOwnNodeId();
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_DROP_TRIG_REF;
    length = CreateTrigRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/**
 * MODULE: Alter trigger.
 *
 * Alter trigger state.  Alter online creates the trigger first in all
 * TC (if index trigger) and then in all LQH-TUP.
 *
 * Request type received in REQ and returned in CONF/REF:
 *
 * RT_USER - normal user e.g. BACKUP
 * RT_CREATE_TRIGGER - from create trigger
 * RT_DROP_TRIGGER - from drop trigger
 * RT_DICT_PREPARE - seize operations and check request
 * RT_DICT_TC - master to each DICT on way to TC
 * RT_DICT_LQH - master to each DICT on way to LQH-TUP
 * RT_DICT_COMMIT - commit state change in each DICT (no reply)
 */

void
Dbdict::execALTER_TRIG_REQ(Signal* signal) 
{
  jamEntry();
  AlterTrigReq* const req = (AlterTrigReq*)signal->getDataPtrSend();
  OpAlterTriggerPtr opPtr;
  const Uint32 senderRef = signal->senderBlockRef();
  const AlterTrigReq::RequestType requestType = req->getRequestType();
  if (requestType == AlterTrigReq::RT_USER ||
      requestType == AlterTrigReq::RT_CREATE_TRIGGER ||
      requestType == AlterTrigReq::RT_DROP_TRIGGER) {
    jam();
    const bool isLocal = req->getRequestFlag() & RequestFlag::RF_LOCAL;
    NdbNodeBitmask receiverNodes = c_aliveNodes;
    if (isLocal) {
      receiverNodes.clear();
      receiverNodes.set(getOwnNodeId());
    }
    if (signal->getLength() == AlterTrigReq::SignalLength) {
      jam();
      if (! isLocal && getOwnNodeId() != c_masterNodeId) {
        jam();
        // forward to DICT master
        sendSignal(calcDictBlockRef(c_masterNodeId), GSN_ALTER_TRIG_REQ,
            signal, AlterTrigReq::SignalLength, JBB);
        return;
      }
      // forward initial request plus operation key to all
      req->setOpKey(++c_opRecordSequence);
      NodeReceiverGroup rg(DBDICT, receiverNodes);
      sendSignal(rg, GSN_ALTER_TRIG_REQ,
          signal, AlterTrigReq::SignalLength + 1, JBB);
      return;
    }
    // seize operation record
    ndbrequire(signal->getLength() == AlterTrigReq::SignalLength + 1);
    const Uint32 opKey = req->getOpKey();
    OpAlterTrigger opBusy;
    if (! c_opAlterTrigger.seize(opPtr))
      opPtr.p = &opBusy;
    opPtr.p->save(req);
    opPtr.p->m_coordinatorRef = senderRef;
    opPtr.p->m_isMaster = (senderRef == reference());
    opPtr.p->key = opKey;
    opPtr.p->m_requestType = AlterTrigReq::RT_DICT_PREPARE;
    if (opPtr.p == &opBusy) {
      jam();
      opPtr.p->m_errorCode = AlterTrigRef::Busy;
      opPtr.p->m_errorLine = __LINE__;
      alterTrigger_sendReply(signal, opPtr, opPtr.p->m_isMaster);
      return;
    }
    c_opAlterTrigger.add(opPtr);
    // master expects to hear from all
    if (opPtr.p->m_isMaster) {
      opPtr.p->m_nodes = receiverNodes;
      opPtr.p->m_signalCounter = receiverNodes;
    }
    alterTrigger_slavePrepare(signal, opPtr);
    alterTrigger_sendReply(signal, opPtr, false);
    return;
  }
  c_opAlterTrigger.find(opPtr, req->getConnectionPtr());
  if (! opPtr.isNull()) {
    opPtr.p->m_requestType = requestType;
    if (requestType == AlterTrigReq::RT_DICT_TC ||
        requestType == AlterTrigReq::RT_DICT_LQH) {
      jam();
      if (req->getOnline())
        alterTrigger_toCreateLocal(signal, opPtr);
      else
        alterTrigger_toDropLocal(signal, opPtr);
      return;
    }
    if (requestType == AlterTrigReq::RT_DICT_COMMIT ||
        requestType == AlterTrigReq::RT_DICT_ABORT) {
      jam();
      if (requestType == AlterTrigReq::RT_DICT_COMMIT)
        alterTrigger_slaveCommit(signal, opPtr);
      else
        alterTrigger_slaveAbort(signal, opPtr);
      alterTrigger_sendReply(signal, opPtr, false);
      // done in slave
      if (! opPtr.p->m_isMaster)
        c_opAlterTrigger.release(opPtr);
      return;
    }
  }
  jam();
  // return to sender
  OpAlterTrigger opBad;
  opPtr.p = &opBad;
  opPtr.p->save(req);
  opPtr.p->m_errorCode = AlterTrigRef::BadRequestType;
  opPtr.p->m_errorLine = __LINE__;
  alterTrigger_sendReply(signal, opPtr, true);
  return;
}

void
Dbdict::execALTER_TRIG_CONF(Signal* signal) 
{
  jamEntry();
  AlterTrigConf* conf = (AlterTrigConf*)signal->getDataPtrSend();
  alterTrigger_recvReply(signal, conf, 0);
}

void
Dbdict::execALTER_TRIG_REF(Signal* signal) 
{
  jamEntry();
  AlterTrigRef* ref = (AlterTrigRef*)signal->getDataPtrSend();
  alterTrigger_recvReply(signal, ref->getConf(), ref);
}

void
Dbdict::alterTrigger_recvReply(Signal* signal, const AlterTrigConf* conf,
    const AlterTrigRef* ref)
{
  jam();
  const Uint32 senderRef = signal->senderBlockRef();
  const AlterTrigReq::RequestType requestType = conf->getRequestType();
  const Uint32 key = conf->getConnectionPtr();
  if (requestType == AlterTrigReq::RT_CREATE_TRIGGER) {
    jam();
    // part of create trigger operation
    OpCreateTriggerPtr opPtr;
    c_opCreateTrigger.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    createTrigger_fromAlterTrigger(signal, opPtr);
    return;
  }
  if (requestType == AlterTrigReq::RT_DROP_TRIGGER) {
    jam();
    // part of drop trigger operation
    OpDropTriggerPtr opPtr;
    c_opDropTrigger.find(opPtr, key);
    ndbrequire(! opPtr.isNull());
    opPtr.p->setError(ref);
    dropTrigger_fromAlterTrigger(signal, opPtr);
    return;
  }
  OpAlterTriggerPtr opPtr;
  c_opAlterTrigger.find(opPtr, key);
  ndbrequire(! opPtr.isNull());
  ndbrequire(opPtr.p->m_isMaster);
  ndbrequire(opPtr.p->m_requestType == requestType);
  /* 
   * If refuse on drop trig, because of non-existent trigger,
   * comes from anyone but the master node - ignore it and
   * remove the node from forter ALTER_TRIG communication
   * This will happen if a new node has started since the
   * trigger whas created.
   */
  if (ref &&
      refToNode(senderRef) != refToNode(reference()) &&
      opPtr.p->m_request.getRequestType() == AlterTrigReq::RT_DROP_TRIGGER &&
      ref->getErrorCode() == AlterTrigRef::TriggerNotFound) {
    jam();
    ref = 0;                                      // ignore this error
    opPtr.p->m_nodes.clear(refToNode(senderRef)); // remove this from group
  }
  opPtr.p->setError(ref);
  opPtr.p->m_signalCounter.clearWaitingFor(refToNode(senderRef));
  if (! opPtr.p->m_signalCounter.done()) {
    jam();
    return;
  }
  if (requestType == AlterTrigReq::RT_DICT_COMMIT ||
      requestType == AlterTrigReq::RT_DICT_ABORT) {
    jam();
    // send reply to user
    alterTrigger_sendReply(signal, opPtr, true);
    c_opAlterTrigger.release(opPtr);
    return;
  }
  if (opPtr.p->hasError()) {
    jam();
    opPtr.p->m_requestType = AlterTrigReq::RT_DICT_ABORT;
    alterTrigger_sendSlaveReq(signal, opPtr);
    return;
  }
  if (! (opPtr.p->m_request.getRequestFlag() & RequestFlag::RF_NOTCTRIGGER)) {
    if (requestType == AlterTrigReq::RT_DICT_PREPARE) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_TC;
      } else {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_LQH;
      }
      alterTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
    if (requestType == AlterTrigReq::RT_DICT_TC) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_LQH;
      } else {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_COMMIT;
      }
      alterTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
    if (requestType == AlterTrigReq::RT_DICT_LQH) {
      jam();
      if (opPtr.p->m_request.getOnline()) {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_COMMIT;
      } else {
        jam();
        opPtr.p->m_requestType = AlterTrigReq::RT_DICT_TC;
      }
      alterTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
  } else {
    if (requestType == AlterTrigReq::RT_DICT_PREPARE) {
      jam();
      opPtr.p->m_requestType = AlterTrigReq::RT_DICT_LQH;
      alterTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
    if (requestType == AlterTrigReq::RT_DICT_LQH) {
      jam();
      opPtr.p->m_requestType = AlterTrigReq::RT_DICT_COMMIT;
      alterTrigger_sendSlaveReq(signal, opPtr);
      return;
    }
  }
  ndbrequire(false);
}

void
Dbdict::alterTrigger_slavePrepare(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  const AlterTrigReq* const req = &opPtr.p->m_request;
  const Uint32 triggerId = req->getTriggerId();
  TriggerRecordPtr triggerPtr;
  if (! (triggerId < c_triggerRecordPool.getSize())) {
    jam();
    opPtr.p->m_errorCode = AlterTrigRef::TriggerNotFound;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  if (triggerPtr.p->triggerState == TriggerRecord::TS_NOT_DEFINED) {
    jam();
    opPtr.p->m_errorCode = AlterTrigRef::TriggerNotFound;
    opPtr.p->m_errorLine = __LINE__;
    return;
  }

  if (triggerPtr.p->triggerType == TriggerType::SUBSCRIPTION)
  {
    opPtr.p->m_request.addRequestFlag(RequestFlag::RF_NOTCTRIGGER);
  }
}

void
Dbdict::alterTrigger_toCreateLocal(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  // find trigger record
  const Uint32 triggerId = opPtr.p->m_request.getTriggerId();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, triggerId);
  CreateTrigReq* const req = (CreateTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
    jam();
    req->setRequestType(CreateTrigReq::RT_TC);
  } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
    jam();
    req->setRequestType(CreateTrigReq::RT_LQH);
  } else {
    ndbassert(false);
  }
  req->setTableId(triggerPtr.p->tableId);
  req->setIndexId(triggerPtr.p->indexId);
  req->setTriggerId(triggerPtr.i);
  req->setTriggerType(triggerPtr.p->triggerType);
  req->setTriggerActionTime(triggerPtr.p->triggerActionTime);
  req->setTriggerEvent(triggerPtr.p->triggerEvent);
  req->setMonitorReplicas(triggerPtr.p->monitorReplicas);
  req->setMonitorAllAttributes(triggerPtr.p->monitorAllAttributes);
  req->setOnline(true);
  req->setReceiverRef(opPtr.p->m_request.getReceiverRef());
  BlockReference blockRef = 0;
  if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
    jam();
    blockRef = calcTcBlockRef(getOwnNodeId());
  } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
    jam();
    blockRef = calcLqhBlockRef(getOwnNodeId());
  } else {
    ndbassert(false);
  }
  req->setAttributeMask(triggerPtr.p->attributeMask);
  sendSignal(blockRef, GSN_CREATE_TRIG_REQ,
      signal, CreateTrigReq::SignalLength, JBB);
}

void
Dbdict::alterTrigger_fromCreateLocal(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  if (! opPtr.p->hasLastError()) {
    // mark created locally
    TriggerRecordPtr triggerPtr;
    c_triggerRecordPool.getPtr(triggerPtr, opPtr.p->m_request.getTriggerId());
    if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
      jam();
      triggerPtr.p->triggerLocal |= TriggerRecord::TL_CREATED_TC;
    } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
      jam();
      triggerPtr.p->triggerLocal |= TriggerRecord::TL_CREATED_LQH;
    } else {
      ndbrequire(false);
    }
  }
  // forward CONF or REF to master
  alterTrigger_sendReply(signal, opPtr, false);
}

void
Dbdict::alterTrigger_toDropLocal(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, opPtr.p->m_request.getTriggerId());
  DropTrigReq* const req = (DropTrigReq*)signal->getDataPtrSend();
  req->setUserRef(reference());
  req->setConnectionPtr(opPtr.p->key);
  if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
    jam();
    // broken trigger allowed if force
    if (! (triggerPtr.p->triggerLocal & TriggerRecord::TL_CREATED_TC)) {
      jam();
      ndbassert(opPtr.p->m_requestFlag & RequestFlag::RF_FORCE);
      alterTrigger_sendReply(signal, opPtr, false);
      return;
    }
    req->setRequestType(DropTrigReq::RT_TC);
  } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
    jam();
    // broken trigger allowed if force
    if (! (triggerPtr.p->triggerLocal & TriggerRecord::TL_CREATED_LQH)) {
      jam();
      ndbassert(opPtr.p->m_requestFlag & RequestFlag::RF_FORCE);
      alterTrigger_sendReply(signal, opPtr, false);
      return;
    }
    req->setRequestType(DropTrigReq::RT_LQH);
  } else {
    ndbassert(false);
  }
  req->setTableId(triggerPtr.p->tableId);
  req->setIndexId(triggerPtr.p->indexId);
  req->setTriggerId(triggerPtr.i);
  req->setTriggerType(triggerPtr.p->triggerType);
  req->setTriggerActionTime(triggerPtr.p->triggerActionTime);
  req->setTriggerEvent(triggerPtr.p->triggerEvent);
  req->setMonitorReplicas(triggerPtr.p->monitorReplicas);
  req->setMonitorAllAttributes(triggerPtr.p->monitorAllAttributes);
  BlockReference blockRef = 0;
  if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
    jam();
    blockRef = calcTcBlockRef(getOwnNodeId());
  } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
    jam();
    blockRef = calcLqhBlockRef(getOwnNodeId());
  } else {
    ndbassert(false);
  }
  sendSignal(blockRef, GSN_DROP_TRIG_REQ,
      signal, DropTrigReq::SignalLength, JBB);
}

void
Dbdict::alterTrigger_fromDropLocal(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  if (! opPtr.p->hasLastError()) {
    // mark dropped locally
    TriggerRecordPtr triggerPtr;
    c_triggerRecordPool.getPtr(triggerPtr, opPtr.p->m_request.getTriggerId());
    if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_TC) {
      jam();
      triggerPtr.p->triggerLocal &= ~TriggerRecord::TL_CREATED_TC;
    } else if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_LQH) {
      jam();
      triggerPtr.p->triggerLocal &= ~TriggerRecord::TL_CREATED_LQH;
    } else {
      ndbrequire(false);
    }
  }
  // forward CONF or REF to master
  alterTrigger_sendReply(signal, opPtr, false);
}

void
Dbdict::alterTrigger_slaveCommit(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
  TriggerRecordPtr triggerPtr;
  c_triggerRecordPool.getPtr(triggerPtr, opPtr.p->m_request.getTriggerId());
  // set state
  triggerPtr.p->triggerState = TriggerRecord::TS_ONLINE;
}

void
Dbdict::alterTrigger_slaveAbort(Signal* signal, OpAlterTriggerPtr opPtr)
{
  jam();
}

void
Dbdict::alterTrigger_sendSlaveReq(Signal* signal, OpAlterTriggerPtr opPtr)
{
  AlterTrigReq* const req = (AlterTrigReq*)signal->getDataPtrSend();
  *req = opPtr.p->m_request;
  req->setUserRef(opPtr.p->m_coordinatorRef);
  req->setConnectionPtr(opPtr.p->key);
  req->setRequestType(opPtr.p->m_requestType);
  req->addRequestFlag(opPtr.p->m_requestFlag);
  NdbNodeBitmask receiverNodes = c_aliveNodes;
  if (opPtr.p->m_requestFlag & RequestFlag::RF_LOCAL) {
    receiverNodes.clear();
    receiverNodes.set(getOwnNodeId());
  } else {
    opPtr.p->m_nodes.bitAND(receiverNodes);
    receiverNodes = opPtr.p->m_nodes;
  }
  opPtr.p->m_signalCounter = receiverNodes;
  NodeReceiverGroup rg(DBDICT, receiverNodes);
  sendSignal(rg, GSN_ALTER_TRIG_REQ,
      signal, AlterTrigReq::SignalLength, JBB);
}

void
Dbdict::alterTrigger_sendReply(Signal* signal, OpAlterTriggerPtr opPtr,
    bool toUser)
{
  jam();
  AlterTrigRef* rep = (AlterTrigRef*)signal->getDataPtrSend();
  Uint32 gsn = GSN_ALTER_TRIG_CONF;
  Uint32 length = AlterTrigConf::InternalLength;
  bool sendRef;
  if (! toUser) {
    sendRef = opPtr.p->hasLastError();
    rep->setUserRef(opPtr.p->m_coordinatorRef);
    rep->setConnectionPtr(opPtr.p->key);
    rep->setRequestType(opPtr.p->m_requestType);
    if (opPtr.p->m_requestType == AlterTrigReq::RT_DICT_ABORT) {
      jam();
      sendRef = false;
    } else {
      jam();
    }
  } else {
    sendRef = opPtr.p->hasError();
    jam();
    rep->setUserRef(opPtr.p->m_request.getUserRef());
    rep->setConnectionPtr(opPtr.p->m_request.getConnectionPtr());
    rep->setRequestType(opPtr.p->m_request.getRequestType());
    length = AlterTrigConf::SignalLength;
  }
  rep->setTableId(opPtr.p->m_request.getTableId());
  rep->setTriggerId(opPtr.p->m_request.getTriggerId());
  if (sendRef) {
    if (opPtr.p->m_errorNode == 0) {
      jam();
      opPtr.p->m_errorNode = getOwnNodeId();
    } else {
      jam();
    }
    rep->setErrorCode(opPtr.p->m_errorCode);
    rep->setErrorLine(opPtr.p->m_errorLine);
    rep->setErrorNode(opPtr.p->m_errorNode);
    gsn = GSN_ALTER_TRIG_REF;
    length = AlterTrigRef::SignalLength;
  }
  sendSignal(rep->getUserRef(), gsn, signal, length, JBB);
}

/**
 * MODULE: Support routines for index and trigger.
 */

void
Dbdict::getTableKeyList(TableRecordPtr tablePtr, AttributeList& list)
{
  jam();
  list.sz = 0;
  for (Uint32 tAttr = tablePtr.p->firstAttribute; tAttr != RNIL; ) {
    AttributeRecord* aRec = c_attributeRecordPool.getPtr(tAttr);
    if (aRec->tupleKey)
      list.id[list.sz++] = aRec->attributeId;
    tAttr = aRec->nextAttrInTable;
  }
}

// XXX should store the primary attribute id
void
Dbdict::getIndexAttr(TableRecordPtr indexPtr, Uint32 itAttr, Uint32* id)
{
  jam();
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  AttributeRecord* iaRec = c_attributeRecordPool.getPtr(itAttr);
  for (Uint32 tAttr = tablePtr.p->firstAttribute; tAttr != RNIL; ) {
    AttributeRecord* aRec = c_attributeRecordPool.getPtr(tAttr);
    if (iaRec->equal(*aRec)) {
      id[0] = aRec->attributeId;
      return;
    }
    tAttr = aRec->nextAttrInTable;
  }
  ndbrequire(false);
}

void
Dbdict::getIndexAttrList(TableRecordPtr indexPtr, AttributeList& list)
{
  jam();
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  list.sz = 0;
  memset(list.id, 0, sizeof(list.id));
  ndbrequire(indexPtr.p->noOfAttributes >= 2);
  Uint32 itAttr = indexPtr.p->firstAttribute;
  for (Uint32 i = 0; i < (Uint32)indexPtr.p->noOfAttributes - 1; i++) {
    getIndexAttr(indexPtr, itAttr, &list.id[list.sz++]);
    AttributeRecord* iaRec = c_attributeRecordPool.getPtr(itAttr);
    itAttr = iaRec->nextAttrInTable;
  }
}

void
Dbdict::getIndexAttrMask(TableRecordPtr indexPtr, AttributeMask& mask)
{
  jam();
  TableRecordPtr tablePtr;
  c_tableRecordPool.getPtr(tablePtr, indexPtr.p->primaryTableId);
  mask.clear();
  ndbrequire(indexPtr.p->noOfAttributes >= 2);
  Uint32 itAttr = indexPtr.p->firstAttribute;
  for (Uint32 i = 0; i < (Uint32)indexPtr.p->noOfAttributes - 1; i++) {
    Uint32 id;
    getIndexAttr(indexPtr, itAttr, &id);
    mask.set(id);
    AttributeRecord* iaRec = c_attributeRecordPool.getPtr(itAttr);
    itAttr = iaRec->nextAttrInTable;
  }
}

// DICT lock master

const Dbdict::DictLockType*
Dbdict::getDictLockType(Uint32 lockType)
{
  static const DictLockType lt[] = {
    { DictLockReq::NodeRestartLock, BS_NODE_RESTART, "NodeRestart" }
  };
  for (int i = 0; i < sizeof(lt)/sizeof(lt[0]); i++) {
    if (lt[i].lockType == lockType)
      return &lt[i];
  }
  return NULL;
}

void
Dbdict::sendDictLockInfoEvent(Uint32 pollCount)
{
  DictLockPtr loopPtr;
  c_dictLockQueue.first(loopPtr);
  unsigned count = 0;

  char queue_buf[100];
  char *p = &queue_buf[0];
  const char *const q = &queue_buf[sizeof(queue_buf)];
  *p = 0;

  while (loopPtr.i != RNIL) {
    jam();
    my_snprintf(p, q-p, "%s%u%s",
                ++count == 1 ? "" : " ",
                (unsigned)refToNode(loopPtr.p->req.userRef),
                loopPtr.p->locked ? "L" : "");
    p += strlen(p);
    c_dictLockQueue.next(loopPtr);
  }

  infoEvent("DICT: lock bs: %d ops: %d poll: %d cnt: %d queue: %s",
      (int)c_blockState,
      c_opRecordPool.getSize() - c_opRecordPool.getNoOfFree(),
      c_dictLockPoll, (int)pollCount, queue_buf);
}

void
Dbdict::sendDictLockInfoEvent(DictLockPtr lockPtr, const char* text)
{
  infoEvent("DICT: %s %u for %s",
      text,
      (unsigned)refToNode(lockPtr.p->req.userRef), lockPtr.p->lt->text);
}

void
Dbdict::execDICT_LOCK_REQ(Signal* signal)
{
  jamEntry();
  const DictLockReq* req = (const DictLockReq*)&signal->theData[0];

  // make sure bad request crashes slave, not master (us)

  if (getOwnNodeId() != c_masterNodeId) {
    jam();
    sendDictLockRef(signal, *req, DictLockRef::NotMaster);
    return;
  }

  const DictLockType* lt = getDictLockType(req->lockType);
  if (lt == NULL) {
    jam();
    sendDictLockRef(signal, *req, DictLockRef::InvalidLockType);
    return;
  }

  if (req->userRef != signal->getSendersBlockRef() ||
      getNodeInfo(refToNode(req->userRef)).m_type != NodeInfo::DB) {
    jam();
    sendDictLockRef(signal, *req, DictLockRef::BadUserRef);
    return;
  }

  if (c_aliveNodes.get(refToNode(req->userRef))) {
    jam();
    sendDictLockRef(signal, *req, DictLockRef::TooLate);
    return;
  }

  DictLockPtr lockPtr;
  if (! c_dictLockQueue.seize(lockPtr)) {
    jam();
    sendDictLockRef(signal, *req, DictLockRef::TooManyRequests);
    return;
  }

  lockPtr.p->req = *req;
  lockPtr.p->locked = false;
  lockPtr.p->lt = lt;

  checkDictLockQueue(signal, false);

  if (! lockPtr.p->locked)
    sendDictLockInfoEvent(lockPtr, "lock request by node");
}

// only table and index ops are checked
bool
Dbdict::hasDictLockSchemaOp()
{
  return
    ! c_opCreateTable.isEmpty() ||
    ! c_opDropTable.isEmpty() ||
    ! c_opCreateIndex.isEmpty() ||
    ! c_opDropIndex.isEmpty();
}

void
Dbdict::checkDictLockQueue(Signal* signal, bool poll)
{
  Uint32 pollCount = ! poll ? 0 : signal->theData[1];

  DictLockPtr lockPtr;

  do {
    if (! c_dictLockQueue.first(lockPtr)) {
      jam();
      setDictLockPoll(signal, false, pollCount);
      return;
    }

    if (lockPtr.p->locked) {
      jam();
      ndbrequire(c_blockState == lockPtr.p->lt->blockState);
      break;
    }

    if (hasDictLockSchemaOp()) {
      jam();
      break;
    }

    ndbrequire(c_blockState == BS_IDLE);
    lockPtr.p->locked = true;
    c_blockState = lockPtr.p->lt->blockState;
    sendDictLockConf(signal, lockPtr);

    sendDictLockInfoEvent(lockPtr, "locked by node");
  } while (0);

  // poll while first request is open
  // this routine is called again when it is removed for any reason

  bool on = ! lockPtr.p->locked;
  setDictLockPoll(signal, on, pollCount);
}

void
Dbdict::execDICT_UNLOCK_ORD(Signal* signal)
{
  jamEntry();
  const DictUnlockOrd* ord = (const DictUnlockOrd*)&signal->theData[0];

  DictLockPtr lockPtr;
  c_dictLockQueue.getPtr(lockPtr, ord->lockPtr);
  ndbrequire(lockPtr.p->lt->lockType == ord->lockType);

  if (lockPtr.p->locked) {
    jam();
    ndbrequire(c_blockState == lockPtr.p->lt->blockState);
    ndbrequire(! hasDictLockSchemaOp());
    ndbrequire(! c_dictLockQueue.hasPrev(lockPtr));

    c_blockState = BS_IDLE;
    sendDictLockInfoEvent(lockPtr, "unlocked by node");
  } else {
    sendDictLockInfoEvent(lockPtr, "lock request removed by node");
  }

  c_dictLockQueue.release(lockPtr);

  checkDictLockQueue(signal, false);
}

void
Dbdict::sendDictLockConf(Signal* signal, DictLockPtr lockPtr)
{
  DictLockConf* conf = (DictLockConf*)&signal->theData[0];
  const DictLockReq& req = lockPtr.p->req;

  conf->userPtr = req.userPtr;
  conf->lockType = req.lockType;
  conf->lockPtr = lockPtr.i;

  sendSignal(req.userRef, GSN_DICT_LOCK_CONF, signal,
      DictLockConf::SignalLength, JBB);
}

void
Dbdict::sendDictLockRef(Signal* signal, DictLockReq req, Uint32 errorCode)
{
  DictLockRef* ref = (DictLockRef*)&signal->theData[0];

  ref->userPtr = req.userPtr;
  ref->lockType = req.lockType;
  ref->errorCode = errorCode;

  sendSignal(req.userRef, GSN_DICT_LOCK_REF, signal,
      DictLockRef::SignalLength, JBB);
}

// control polling

void
Dbdict::setDictLockPoll(Signal* signal, bool on, Uint32 pollCount)
{
  if (on) {
    jam();
    signal->theData[0] = ZDICT_LOCK_POLL;
    signal->theData[1] = pollCount + 1;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
  }

  bool change = (c_dictLockPoll != on);

  if (change) {
    jam();
    c_dictLockPoll = on;
  }

  // avoid too many messages if master is stuck busy (BS_NODE_FAILURE)
  bool periodic =
    pollCount < 8 ||
    pollCount < 64 && pollCount % 8 == 0 ||
    pollCount < 512 && pollCount % 64 == 0 ||
    pollCount < 4096 && pollCount % 512 == 0 ||
    pollCount % 4096 == 0; // about every 6 minutes

  if (change || periodic)
    sendDictLockInfoEvent(pollCount);
}

// NF handling

void
Dbdict::removeStaleDictLocks(Signal* signal, const Uint32* theFailedNodes)
{
  DictLockPtr loopPtr;
  c_dictLockQueue.first(loopPtr);

  if (getOwnNodeId() != c_masterNodeId) {
    ndbrequire(loopPtr.i == RNIL);
    return;
  }

  while (loopPtr.i != RNIL) {
    jam();
    DictLockPtr lockPtr = loopPtr;
    c_dictLockQueue.next(loopPtr);

    Uint32 nodeId = refToNode(lockPtr.p->req.userRef);

    if (NodeBitmask::get(theFailedNodes, nodeId)) {
      if (lockPtr.p->locked) {
        jam();
        ndbrequire(c_blockState == lockPtr.p->lt->blockState);
        ndbrequire(! hasDictLockSchemaOp());
        ndbrequire(! c_dictLockQueue.hasPrev(lockPtr));

        c_blockState = BS_IDLE;

        sendDictLockInfoEvent(lockPtr, "remove lock by failed node");
      } else {
        sendDictLockInfoEvent(lockPtr, "remove lock request by failed node");
      }

      c_dictLockQueue.release(lockPtr);
    }
  }

  checkDictLockQueue(signal, false);
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

// global metadata support

int
Dbdict::getMetaTablePtr(TableRecordPtr& tablePtr, Uint32 tableId, Uint32 tableVersion)
{
  if (tableId >= c_tableRecordPool.getSize()) {
    return MetaData::InvalidArgument;
  }
  c_tableRecordPool.getPtr(tablePtr, tableId);
  if (tablePtr.p->tabState == TableRecord::NOT_DEFINED) {
    return MetaData::TableNotFound;
  }
  if (tablePtr.p->tableVersion != tableVersion) {
    return MetaData::InvalidTableVersion;
  }
  // online flag is not maintained by DICT
  tablePtr.p->online =
    tablePtr.p->isTable() && 
    (tablePtr.p->tabState == TableRecord::DEFINED ||
     tablePtr.p->tabState == TableRecord::BACKUP_ONGOING) ||
    tablePtr.p->isIndex() && tablePtr.p->indexState == TableRecord::IS_ONLINE;
  return 0;
}

int
Dbdict::getMetaTable(MetaData::Table& table, Uint32 tableId, Uint32 tableVersion)
{
  int ret;
  TableRecordPtr tablePtr;
  if ((ret = getMetaTablePtr(tablePtr, tableId, tableVersion)) < 0) {
    return ret;
  }
  new (&table) MetaData::Table(*tablePtr.p);
  return 0;
}

int
Dbdict::getMetaTable(MetaData::Table& table, const char* tableName)
{
  int ret;
  TableRecordPtr tablePtr;
  if (strlen(tableName) + 1 > MAX_TAB_NAME_SIZE) {
    return MetaData::InvalidArgument;
  }
  TableRecord keyRecord;
  strcpy(keyRecord.tableName, tableName);
  c_tableRecordHash.find(tablePtr, keyRecord);
  if (tablePtr.i == RNIL) {
    return MetaData::TableNotFound;
  }
  if ((ret = getMetaTablePtr(tablePtr, tablePtr.i, tablePtr.p->tableVersion)) < 0) {
    return ret;
  }
  new (&table) MetaData::Table(*tablePtr.p);
  return 0;
}

int
Dbdict::getMetaAttribute(MetaData::Attribute& attr, const MetaData::Table& table, Uint32 attributeId)
{
  int ret;
  TableRecordPtr tablePtr;
  if ((ret = getMetaTablePtr(tablePtr, table.tableId, table.tableVersion)) < 0) {
    return ret;
  }
  AttributeRecordPtr attrPtr;
  attrPtr.i = tablePtr.p->firstAttribute;
  while (attrPtr.i != RNIL) {
    c_attributeRecordPool.getPtr(attrPtr);
    if (attrPtr.p->attributeId == attributeId)
      break;
    attrPtr.i = attrPtr.p->nextAttrInTable;
  }
  if (attrPtr.i == RNIL) {
    return MetaData::AttributeNotFound;
  }
  new (&attr) MetaData::Attribute(*attrPtr.p);
  return 0;
}

int
Dbdict::getMetaAttribute(MetaData::Attribute& attr, const MetaData::Table& table, const char* attributeName)
{
  int ret;
  TableRecordPtr tablePtr;
  if ((ret = getMetaTablePtr(tablePtr, table.tableId, table.tableVersion)) < 0) {
    return ret;
  }
  AttributeRecordPtr attrPtr;
  attrPtr.i = tablePtr.p->firstAttribute;
  while (attrPtr.i != RNIL) {
    c_attributeRecordPool.getPtr(attrPtr);
    if (strcmp(attrPtr.p->attributeName, attributeName) == 0)
      break;
    attrPtr.i = attrPtr.p->nextAttrInTable;
  }
  if (attrPtr.i == RNIL) {
    return MetaData::AttributeNotFound;
  }
  new (&attr) MetaData::Attribute(*attrPtr.p);
  return 0;
}

CArray<KeyDescriptor> g_key_descriptor_pool;
