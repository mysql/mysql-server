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

#include "records.hpp"

void printOut(const char *string, Uint32 value) {
  ndbout_c("%-30s%-12u%-12x", string, value, value);
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool AbortTransactionRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 AbortTransactionRecord::getLogRecordSize() {
  return ABORTTRANSACTIONRECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const AbortTransactionRecord& atr) {
  no << "----------ABORT TRANSACTION RECORD-------------" << endl << endl;
  printOut("Record type:", atr.m_recordType);
  printOut("TransactionId1:", atr.m_transactionId1);
  printOut("TransactionId2:", atr.m_transactionId2);
  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool NextMbyteRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 NextMbyteRecord::getLogRecordSize() {
  return NEXTMBYTERECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const NextMbyteRecord& nmr) {
  no << "----------NEXT MBYTE RECORD--------------------" << endl << endl;
  printOut("Record type:", nmr.m_recordType);
  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool CommitTransactionRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 CommitTransactionRecord::getLogRecordSize() {
  return COMMITTRANSACTIONRECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const CommitTransactionRecord& ctr) {
  no << "----------COMMIT TRANSACTION RECORD------------" << endl << endl;
  printOut("Record type:", ctr.m_recordType);
  printOut("TableId", ctr.m_tableId);
  printOut("SchemaVersion:", ctr.m_schemaVersion);
  printOut("FfragmentId", ctr.m_fragmentId);
  printOut("File no. of Prep. Op.", ctr.m_fileNumberOfPrepareOperation);
  printOut("Start page no. of Prep. Op.", ctr.m_startPageNumberOfPrepareOperation);
  printOut("Start page index of Prep. Op.", ctr.m_startPageIndexOfPrepareOperation);
  printOut("Stop page no. of Prep. Op.", ctr.m_stopPageNumberOfPrepareOperation);
  printOut("GlobalCheckpoint", ctr.m_globalCheckpoint);

  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool InvalidCommitTransactionRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 InvalidCommitTransactionRecord::getLogRecordSize() {
  return COMMITTRANSACTIONRECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const InvalidCommitTransactionRecord& ictr) {
  no << "------INVALID COMMIT TRANSACTION RECORD--------" << endl << endl;
  printOut("Record type:", ictr.m_recordType);
  printOut("TableId", ictr.m_tableId);
  printOut("FfragmentId", ictr.m_fragmentId);
  printOut("File no. of Prep. Op.", ictr.m_fileNumberOfPrepareOperation);
  printOut("Start page no. of Prep. Op.", ictr.m_startPageNumberOfPrepareOperation);
  printOut("Start page index of Prep. Op.", ictr.m_startPageIndexOfPrepareOperation);
  printOut("Stop page no. of Prep. Op.", ictr.m_stopPageNumberOfPrepareOperation);
  printOut("GlobalCheckpoint", ictr.m_globalCheckpoint);

  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool PrepareOperationRecord::check() {
  // Not fully implemented.
  if (m_operationType == 3 && m_attributeLength != 0)
    return false;

  if (m_logRecordSize != (m_attributeLength + m_keyLength + 6))
    return false;

  return true;
}

Uint32 PrepareOperationRecord::getLogRecordSize(Uint32 wordsRead) {
  if (wordsRead < 2)
    return 2; // make sure we read more
  return m_logRecordSize;
}

NdbOut& operator<<(NdbOut& no, const PrepareOperationRecord& por) {
  no << "-----------PREPARE OPERATION RECORD------------" << endl << endl;
  printOut("Record type:", por.m_recordType);
  printOut("logRecordSize:", por.m_logRecordSize);
  printOut("hashValue:", por.m_hashValue);
  switch (por.m_operationType) {
  case 0:
    ndbout_c("%-30s%-12u%-6s", "operationType:", 
	     por.m_operationType, "read");
    break;
  case 1:
    ndbout_c("%-30s%-12u%-6s", "operationType:", 
	     por.m_operationType, "update");
    break;
  case 2:
    ndbout_c("%-30s%-12u%-6s", "operationType:", 
	     por.m_operationType, "insert");
    break;
  case 3:
    ndbout_c("%-30s%-12u%-6s", "operationType:", 
	     por.m_operationType, "delete");
    break;
  default:
    printOut("operationType:", por.m_operationType);
  }
  printOut("attributeLength:", por.m_attributeLength);
  printOut("keyLength:", por.m_keyLength);

#if 1
  // Print keydata
  Uint32* p = (Uint32*)&por.m_keyInfo;
  for(Uint32 i=0; i < por.m_keyLength; i++){    
    printOut("keydata:", *p);
    p++;
  }

  // Print attrdata
  for(Uint32 i=0; i < por.m_attributeLength; i++){    
    printOut("attrdata:", *p);
    p++;
  }
#endif

  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool CompletedGCIRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 CompletedGCIRecord::getLogRecordSize() {
  return COMPLETEDGCIRECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const CompletedGCIRecord& cGCIr) {
  no << "-----------COMPLETED GCI RECORD----------------" << endl << endl;
  printOut("Record type:", cGCIr.m_recordType);
  printOut("Completed GCI:", cGCIr.m_theCompletedGCI);
  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

bool NextLogRecord::check() {
  // Not implemented yet.
  return true;
}

Uint32 NextLogRecord::getLogRecordSize(Uint32 pageIndex) {
  return PAGESIZE - pageIndex;
}

NdbOut& operator<<(NdbOut& no, const NextLogRecord& nl) {
  no << "-----------NEXT LOG RECORD --------------------" << endl << endl;
  printOut("Record type:", nl.m_recordType);
  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

Uint32 PageHeader::getLogRecordSize() {
  return PAGEHEADERSIZE;
}

bool PageHeader::check() {
  // Not implemented yet.
  return true;
}

bool PageHeader::lastPage()
{
  return m_next_page == 0xffffff00;
}

Uint32 PageHeader::lastWord()
{
  return m_current_page_index;
}


NdbOut& operator<<(NdbOut& no, const PageHeader& ph) {
  no << "------------PAGE HEADER------------------------" << endl << endl;
  ndbout_c("%-30s%-12s%-12s\n", "", "Decimal", "Hex");
  printOut("Checksum:", ph.m_checksum);
  printOut("Laps since initial start:",  ph.m_lap);
  printOut("Max gci completed:",  ph.m_max_gci_completed);
  printOut("Max gci started:",  ph.m_max_gci_started);
  printOut("Ptr to next page:", ph.m_next_page);	 
  printOut("Ptr to previous page:",  ph.m_previous_page);	 
  printOut("Ndb version:",  ph.m_ndb_version);
  printOut("Number of log files:", ph.m_number_of_logfiles);
  printOut("Current page index:",  ph.m_current_page_index);
  printOut("Oldest prepare op. file No.:", ph.m_old_prepare_file_number);	 
  printOut("Oldest prepare op. page ref.:",  ph.m_old_prepare_page_reference);	 
  printOut("Dirty flag:", ph.m_dirty_flag);	 
  no << endl;
  return no;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

Uint32 FileDescriptor::getLogRecordSize() {
  return  FILEDESCRIPTORHEADERSIZE
    + m_fdHeader.m_noOfDescriptors * FILEDESCRIPTORRECORDSIZE;
}

NdbOut& operator<<(NdbOut& no, const FileDescriptor& fd) {
  no << "-------FILE DESCRIPTOR HEADER------------------" << endl << endl;
  printOut("Record type:", fd.m_fdHeader.m_recordType);
  printOut("Number of file descriptors:", fd.m_fdHeader.m_noOfDescriptors);
  printOut("File number:", fd.m_fdHeader.m_fileNo);
  ndbout << endl;
  for(Uint32 i = 0; i < fd.m_fdHeader.m_noOfDescriptors; i++) {
    fd.printARecord(i);
  }
  return no;
}

void FileDescriptor::printARecord( Uint32 recordIndex ) const {
  ndbout << "------------------FILE DESCRIPTOR " << recordIndex 
	 <<" ---------------------" << endl << endl;
  ndbout_c("%-30s%-12s%-12s\n", "", "Decimal", "Hex"); 

  for(int i = 1; i <= NO_MBYTE_IN_FILE; i++) {
    ndbout_c("%s%2d%s%-12u%-12x", "Max GCI completed, mbyte ", i, ":  ", 
	     m_fdRecord[recordIndex].m_maxGciCompleted[i-1],
	     m_fdRecord[recordIndex].m_maxGciCompleted[i-1]);
  }
  for(int i = 1; i <= NO_MBYTE_IN_FILE; i++) {
    ndbout_c("%s%2d%s%-12u%-12x", "Max GCI started,  mbyte ", i, ":   ", 
	     m_fdRecord[recordIndex].m_maxGciStarted[i-1],
	     m_fdRecord[recordIndex].m_maxGciStarted[i-1]);
  }
  for(int i = 1; i <= NO_MBYTE_IN_FILE; i++) {
    ndbout_c("%s%2d%s%-12u%-12x", "Last prepared ref, mbyte ", i, ":  ", 
	     m_fdRecord[recordIndex].m_lastPreparedReference[i-1],
	     m_fdRecord[recordIndex].m_lastPreparedReference[i-1]);
  }
  ndbout << endl;
}

bool FileDescriptor::check() {
  // Not implemented yet.
  return true;
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------
