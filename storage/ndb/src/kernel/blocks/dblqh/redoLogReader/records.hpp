/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RECORDS_HPP
#define RECORDS_HPP

#include <NdbOut.hpp>
#include <ndb_types.h>

#define ZNEW_PREP_OP_TYPE 0
#define ZPREP_OP_TYPE 1
#define ZCOMMIT_TYPE 2
#define ZABORT_TYPE 3
#define ZFD_TYPE 4
#define ZFRAG_SPLIT_TYPE 5
#define ZNEXT_LOG_RECORD_TYPE 6
#define ZNEXT_MBYTE_TYPE 7
#define ZCOMPLETED_GCI_TYPE 8
#define ZINVALID_COMMIT_TYPE 9

#define REDOLOG_PAGESIZE 8192
#define REDOLOG_PAGES_IN_MBYTE 32

#define COMMITTRANSACTIONRECORDSIZE 9
#define COMPLETEDGCIRECORDSIZE 2
#define PAGEHEADERSIZE 32
#define FILEDESCRIPTORHEADERSIZE 3
#define FILEDESCRIPTORENTRYSIZE 3
#define NEXTMBYTERECORDSIZE 1
#define ABORTTRANSACTIONRECORDSIZE 3

extern unsigned NO_MBYTE_IN_FILE;

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class AbortTransactionRecord {
  friend NdbOut& operator<<(NdbOut&, const AbortTransactionRecord&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  Uint32 m_recordType;
  Uint32 m_transactionId1;
  Uint32 m_transactionId2;
};


//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class NextMbyteRecord {
  friend NdbOut& operator<<(NdbOut&, const NextMbyteRecord&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  Uint32 m_recordType;
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------


class PrepareOperationRecord {
  friend NdbOut& operator<<(NdbOut&, const PrepareOperationRecord&);
public:
  bool check();
  Uint32 getLogRecordSize(Uint32 wordsRead);

protected:
  Uint32 m_recordType;
  Uint32 m_logRecordSize;
  Uint32 m_hashValue;
  Uint32 m_operationType; // 0 READ, 1 UPDATE, 2 INSERT, 3 DELETE
  Uint32 m_attributeLength;
  Uint32 m_keyLength;
  Uint32 m_page_no;
  Uint32 m_page_idx;
  Uint32 *m_keyInfo; // In this order
  Uint32 *m_attrInfo;// In this order
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class CompletedGCIRecord {
  friend NdbOut& operator<<(NdbOut&, const CompletedGCIRecord&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  Uint32 m_recordType;
  Uint32 m_theCompletedGCI;
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class NextLogRecord {
  friend NdbOut& operator<<(NdbOut&, const NextLogRecord&);
public:
  bool check();
  Uint32 getLogRecordSize(Uint32);
protected:
  Uint32 m_recordType;
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class PageHeader {
  friend NdbOut& operator<<(NdbOut&, const PageHeader&);
public:
  bool check();
  Uint32 getLogRecordSize();
  bool lastPage();
  Uint32 lastWord();
//protected:
  Uint32 m_checksum;
  Uint32 m_lap;
  Uint32 m_max_gci_completed;
  Uint32 m_max_gci_started;
  Uint32 m_next_page;
  Uint32 m_previous_page;
  Uint32 m_ndb_version;
  Uint32 m_number_of_logfiles;
  Uint32 m_current_page_index;
  Uint32 m_old_prepare_file_number;
  Uint32 m_old_prepare_page_reference;
  Uint32 m_dirty_flag;
/* Debug info Start */
  Uint32 m_log_timer;
  Uint32 m_page_i_value;
  Uint32 m_place_written_from;
  Uint32 m_page_no;
  Uint32 m_file_no;
  Uint32 m_word_written;
  Uint32 m_in_writing_flag;
  Uint32 m_prev_page_no;
  Uint32 m_in_free_list;
/* Debug info End */
};

//----------------------------------------------------------------
// File descriptor.
//----------------------------------------------------------------

class FileDescriptorHeader {
public:
 Uint32 m_recordType;
  Uint32 m_noOfDescriptors;
  Uint32 m_fileNo;
};

class FileDescriptor 
{
  friend NdbOut& operator<<(NdbOut&, const FileDescriptor&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  void printARecord( Uint32 ) const;
  FileDescriptorHeader m_fdHeader;
  Uint32 m_fdRecord[1];
};


//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class CommitTransactionRecord {
  friend NdbOut& operator<<(NdbOut&, const CommitTransactionRecord&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  Uint32 m_recordType;
  Uint32 m_tableId;
  Uint32 m_schemaVersion;
  Uint32 m_fragmentId;
  Uint32 m_fileNumberOfPrepareOperation;
  Uint32 m_startPageNumberOfPrepareOperation;
  Uint32 m_startPageIndexOfPrepareOperation;
  Uint32 m_stopPageNumberOfPrepareOperation;
  Uint32 m_globalCheckpoint;
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

class InvalidCommitTransactionRecord {
  friend NdbOut& operator<<(NdbOut&, const InvalidCommitTransactionRecord&);
public:
  bool check();
  Uint32 getLogRecordSize();
protected:
  Uint32 m_recordType;
  Uint32 m_tableId;
  Uint32 m_fragmentId;
  Uint32 m_fileNumberOfPrepareOperation;
  Uint32 m_startPageNumberOfPrepareOperation;
  Uint32 m_startPageIndexOfPrepareOperation;
  Uint32 m_stopPageNumberOfPrepareOperation;
  Uint32 m_globalCheckpoint;
};

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

struct NextLogRec {

};

struct NewPrepareOperation {

};

struct FragmentSplit {

};

#endif
