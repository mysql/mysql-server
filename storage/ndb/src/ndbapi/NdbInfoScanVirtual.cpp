/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

// Implements
#include "NdbInfoScanVirtual.hpp"

#include <memory>

#include "ndbapi/NdbApi.hpp"
#include "NdbInfo.hpp"
#include "util/OutputStream.hpp"

using DictionaryList = NdbDictionary::Dictionary::List;

int NdbInfoScanVirtual::readTuples()
{
  DBUG_ENTER("NdbInfoScanVirtual::readTuples");
  if (m_state != Initial)
    DBUG_RETURN(NdbInfo::ERR_WrongState);

  m_state = Prepared;
  DBUG_RETURN(0);
}


const NdbInfoRecAttr* NdbInfoScanVirtual::getValue(const char * anAttrName)
{
  DBUG_ENTER("NdbInfoScanVirtual::getValue(const char*)");
  DBUG_PRINT("enter", ("anAttrName: %s", anAttrName));
  if (m_state != Prepared)
    DBUG_RETURN(NULL);

  const NdbInfo::Column* column = m_table->getColumn(anAttrName);
  if (!column)
    DBUG_RETURN(NULL);
  DBUG_RETURN(getValue(column->m_column_id));
}


const NdbInfoRecAttr* NdbInfoScanVirtual::getValue(Uint32 anAttrId)
{
  DBUG_ENTER("NdbInfoScanVirtual::getValue(Uint32)");
  DBUG_PRINT("enter", ("anAttrId: %u", anAttrId));
  if (m_state != Prepared)
    DBUG_RETURN(NULL);

  if (anAttrId >= m_table->columns())
    DBUG_RETURN(NULL);

  DBUG_RETURN(m_recAttrs.get_value(anAttrId));
}

/*
  Interface class for virtual table implementations.
*/
class VirtualTable
{
public:
  /*
    Utility class used to populate rows for virtual tables.

    The Row utility provides a limited number of functions
    for the Virtual table implementations to fill rows
    in a standardized way.
  */
  class Row
  {
  public:
    void write_string(const char* str);
    void write_number(Uint32 val);
    void write_number64(Uint64 val);
  private:
    friend class NdbInfoScanVirtual;
    Row(class NdbInfoScanVirtual* owner,
        const NdbInfo::Table* table,
        char* buffer, size_t buf_size);

    Row(const Row&); // Prevent
    Row& operator=(const Row&); // Prevent

    const class NdbInfoScanVirtual* const m_owner;
    const NdbInfo::Table* const m_table;
    char* const m_end;        // End of row buffer
    char* m_curr;             // Current position in row buffer
    unsigned m_col_counter;    // Current column counter

    bool check_data_type(NdbInfo::Column::Type type) const;
    bool check_buffer_space(size_t len) const;
  };

  /*
    Initialize the table
  */
  virtual bool init() { return true; }

  /*
    Return the NdbInfo::Table corresponding to this virtual table
  */
  virtual NdbInfo::Table* get_instance() const = 0;

  /*
     Start reading of row(s) from the virtual table
  */
  virtual bool start_scan(VirtualScanContext* ctx) const = 0;

  /*
    Read one row from the virtual table

    Data for the row specified by row_number should be filled in
    by the functions provided by Virtual::Row

    0  No more data
    1  More rows available
    >1 Error occurred
  */
  virtual int read_row(VirtualScanContext* ctx, Row& row, Uint32 row_number) const = 0;

  int seek(std::map<int, int>::const_iterator &,
           const int &key, NdbInfoScanOperation::Seek) const;

 virtual ~VirtualTable() = 0;

protected:
  std::map<int, int> m_index;
  static constexpr int column_buff_size = 512;
};

/*
  key: int32 primary key value
  returns row number in table
*/
int index_seek(const std::map<int, int> &index,
               std::map<int, int>::const_iterator &iter,
               const int &key,
               NdbInfoScanOperation::Seek seek) {
  switch(seek.mode) {
    case NdbInfoScanOperation::Seek::Mode::first:
      iter = index.cbegin();
      return iter->second;
    case NdbInfoScanOperation::Seek::Mode::last:
      iter = std::prev(index.cend(), 1);
      return iter->second;
    case NdbInfoScanOperation::Seek::Mode::next:
      if(iter == index.cend()) return -1;
      if(++iter == index.cend()) return -1;
      return iter->second;
    case NdbInfoScanOperation::Seek::Mode::previous:
      if(iter == index.cbegin()) return -1;
      iter--;
      return iter->second;
    case NdbInfoScanOperation::Seek::Mode::value:
      iter = index.lower_bound(key);
      break;
  }
  if(iter == index.cend()) return -1;

  /* Check for exact match */
  if(! (seek.inclusive() && (iter->first == key)))
  {
    /* Exact match failed. Check for bounded ranges */
    if(seek.high() || seek.low())
    {
      if (seek.high() && iter->first == key) {
        iter++;
      } else if (seek.low()) {
        if(iter == index.cbegin()) return -1; // nothing lower than first rec
        iter--;
      }
    }
    else return -1; // exact match failed and ranges are not wanted
  }

  return (iter == index.cend()) ? -1 : iter->second;
}

int VirtualTable::seek(std::map<int, int>::const_iterator &iter,
                       const int &key,
                       NdbInfoScanOperation::Seek seek) const {
  return index_seek(m_index, iter, key, seek);
}

VirtualTable::~VirtualTable() {}


bool VirtualTable::Row::check_data_type(NdbInfo::Column::Type type) const
{
  return (m_table->getColumn(m_col_counter)->m_type == type);
}


bool
VirtualTable::Row::check_buffer_space(size_t len) const
{
  if(m_curr+len > m_end)
  {
    // Not enough room in buffer
    assert(false);
    return false;
  }

  return true;
}


void VirtualTable::Row::write_string(const char* str)
{
  DBUG_ENTER("write_string");
  DBUG_PRINT("enter", ("str: %s", str));

  assert(check_data_type(NdbInfo::Column::String));

  const unsigned col_idx = m_col_counter++;
  if (!m_owner->m_recAttrs.is_requested(col_idx))
    DBUG_VOID_RETURN;

  const size_t clen = strlen(str) + 1; // length including terminator
  if (!check_buffer_space(clen))
    return;

  // setup RecAttr
  m_owner->m_recAttrs.set_recattr(col_idx, m_curr, clen);

  // copy string to buffer
  memcpy(m_curr, str, clen);
  m_curr += clen;
  if(clen % 4) m_curr += (4 - (clen % 4)); // pad for alignment

  DBUG_VOID_RETURN;
}


void VirtualTable::Row::write_number(Uint32 val) {
  DBUG_ENTER("write_number");
  DBUG_PRINT("enter", ("val: %u", val));

  assert(check_data_type(NdbInfo::Column::Number));

  const unsigned col_idx = m_col_counter++;
  if (!m_owner->m_recAttrs.is_requested(col_idx))
    DBUG_VOID_RETURN;

  const size_t clen = sizeof(Uint32);
  if (!check_buffer_space(clen))
    return;

  // setup RecAttr
  m_owner->m_recAttrs.set_recattr(col_idx, m_curr, clen);

  // copy value to buffer
  memcpy(m_curr, &val, clen);
  m_curr+=clen;

  DBUG_VOID_RETURN;
}


void VirtualTable::Row::write_number64(Uint64 val) {
  DBUG_ENTER("write_number64");
  DBUG_PRINT("enter", ("val: %llu", val));

  assert(check_data_type(NdbInfo::Column::Number64));

  const unsigned col_idx = m_col_counter++;
  if (!m_owner->m_recAttrs.is_requested(col_idx))
    DBUG_VOID_RETURN;

  const size_t clen = sizeof(Uint64);
  if (!check_buffer_space(clen))
    return;

  // setup recAttr
  m_owner->m_recAttrs.set_recattr(col_idx, m_curr, clen);

  // copy value to buffer
  memcpy(m_curr, &val, clen);
  m_curr+=clen;

  DBUG_VOID_RETURN;
}


VirtualTable::Row::Row(NdbInfoScanVirtual* owner,
                       const NdbInfo::Table* table,
                       char* buffer, size_t buf_size) :
  m_owner(owner),
  m_table(table),
  m_end(buffer + buf_size),
  m_curr(buffer),
  m_col_counter(0)
{
  // Reset all recattr values before reading the new row
  m_owner->m_recAttrs.reset_recattrs();
}


int NdbInfoScanVirtual::nextResult()
{
  DBUG_ENTER("NdbInfoScanVirtual::nextResult");
  if (m_state != MoreData)
    DBUG_RETURN(-1);

  assert(m_buffer);

  VirtualTable::Row row(this, m_table,
                        m_buffer, m_buffer_size);
  const int result = m_virt->read_row(m_ctx, row, m_row_counter);
  if (result == 0)
  {
    // No more rows
    m_state = End;
    DBUG_RETURN(0);
  }

  if (result == 1)
  {
    // More rows

    // Check that all columns where processed(i.e by the virtual table class)
    assert(row.m_col_counter == m_table->columns());

    m_row_counter++;
    DBUG_RETURN(1);  // More rows
  }

  // Error occurred
  m_state = End;
  DBUG_RETURN(result);
}


class BlobSource {
 public:
  BlobSource() : table_id(0), col_id(0) {}
  const NdbDictionary::Table *resolve(const char *blobTableName,
                                      const DictionaryList &, Ndb *);
  Uint32 table_id, col_id;
};

const NdbDictionary::Table *BlobSource::resolve(const char *blobTable,
                                                const DictionaryList &tables,
                                                Ndb *ndb) {
  if (sscanf(blobTable, "NDB$BLOB_%u_%u", &table_id, &col_id) == 2) {
    for (Uint32 i = 0 ; i < tables.count ; i++) {
      const DictionaryList::Element &elem = tables.elements[i];
      if (elem.id == table_id) {
        ndb->setDatabaseName(elem.database);
        return ndb->getDictionary()->getTableGlobal(elem.name);
      }
    }
  }
  return nullptr;
}

class VirtualScanContext {
 public:
  VirtualScanContext(Ndb_cluster_connection *connection)
      : m_connection(connection) {}

  bool create_ndb(const char *dbname = "sys") {
    m_ndb = new Ndb(m_connection, dbname);
    if (m_ndb->init() != 0) return false;
    return true;
  }

  bool openTable(const char *tabname) {
    m_ndbtab = m_ndb->getDictionary()->getTableGlobal(tabname);
    if (!m_ndbtab) return false;
    return true;
  }

  NdbDictionary::Dictionary *getDictionary() const {
    return m_ndb->getDictionary();
  }

  void releaseTable() {
    if(m_ndbtab) {
      getDictionary()->removeTableGlobal(*m_ndbtab, 0);
      m_ndbtab = nullptr;
    }
  }

  void releaseIndex() {
    if(m_ndbindex) {
      getDictionary()->removeIndexGlobal(*m_ndbindex, 0);
      m_ndbindex = nullptr;
    }
  }

  const char * getDatabaseName() const {
    return m_ndb->getDatabaseName();
  }

  const NdbDictionary::Table *getSourceTable(const char * table,
                                             BlobSource &blobSource) {
    return blobSource.resolve(table, m_list, m_ndb);
  }

  /* Fetch table for list element.
     Release the previously held table.
  */
  const NdbDictionary::Table *getTable(const DictionaryList::Element *elem) {
    releaseTable();
    m_ndb->setDatabaseName(elem->database);
    m_ndbtab = getDictionary()->getTableGlobal(elem->name);
    return m_ndbtab;
  }

  /* If table is a blob table, Dictionary::getTableGlobal() has returned null.
     Try to follow path from table name back through source to blob table.
     For proper cache management, leave m_ndbtab pointing to source table.
  */
  const NdbDictionary::Table *getBlobTable(const DictionaryList::Element *el) {
    BlobSource blob;
    assert(m_ndbtab == nullptr);
    m_ndbtab = getSourceTable(el->name, blob);
    if(m_ndbtab) {
      const NdbDictionary::Column * col = m_ndbtab->getColumn(blob.col_id);
      if(col) return col->getBlobTable();
    }
    return nullptr;
  }

  const NdbDictionary::Table *getTable() const { return m_ndbtab; }

  /* Fetch index for list element.
     Release the previously held index.
  */
  const NdbDictionary::Index * getIndex(const DictionaryList::Element *elem) {
    releaseIndex();
    m_ndbindex = getDictionary()->getIndexGlobal(elem->name, *m_ndbtab);
    return m_ndbindex;
  }

  bool startTrans() {
    m_trans = m_ndb->startTransaction();
    if (!m_trans) return false;
    return true;
  }
  NdbTransaction *getTrans() { return m_trans; }

  bool scanTable(const NdbRecord *result_record,
                 NdbOperation::LockMode lock_mode = NdbOperation::LM_Read,
                 const unsigned char *result_mask = nullptr,
                 const NdbScanOperation::ScanOptions *options = nullptr,
                 Uint32 sizeOfOptions = 0) {
    assert(m_trans != nullptr);
    m_scan_op = m_trans->scanTable(result_record, lock_mode, result_mask,
                                   options, sizeOfOptions);
    if (!m_scan_op) return false;
    if (m_trans->execute(NdbTransaction::NoCommit) != 0) return false;
    return true;
  }
  NdbScanOperation *getScanOp() { return m_scan_op; }

  bool createRecord(NdbDictionary::RecordSpecification *recordSpec,
                    Uint32 length, Uint32 elemSize) {
    assert(m_record == nullptr);  // Only one record supported for now
    m_record = m_ndb->getDictionary()->createRecord(m_ndbtab, recordSpec,
                                                    length, elemSize);
    return (m_record != nullptr);
  }

  const NdbRecord *getRecord() { return m_record; }

  void listObjects(NdbDictionary::Object::Type type) {
    assert(m_list_cursor == 0);
    m_ndb->getDictionary()->listObjects(m_list, type);
  }

  const DictionaryList::Element * nextInList(Uint32 row=0xFFFFFFFF) {
    if(m_using_index) {
      assert(row < m_list.count);
      m_list_cursor = row;
    }
    if(m_list_cursor == m_list.count) return nullptr;
    return & m_list.elements[m_list_cursor++];
  }

  const NdbDictionary::Index * nextIndex() {
    assert(! m_using_index);
    while(m_inner_cursor == m_inner_list.count) {
      const NdbDictionary::Table * table;
      do {
        const DictionaryList::Element * elem = nextInList();
        if(elem == nullptr) return nullptr;  // end of list
        table = getTable(elem);  // will be null if table is a blob table
      } while(table == nullptr);
      m_inner_cursor = 0;
      m_inner_list.clear();
      if(m_ndb->getDictionary()->listIndexes(m_inner_list, *table) != 0)
        return nullptr;  // error in listIndexes()
    }
    return getIndex(& m_inner_list.elements[m_inner_cursor++]);
  }

  const NdbDictionary::Column * nextColumn() {
    assert(! m_using_index);  // columns table has no indexes
    if((m_ndbtab == nullptr) /* first table */ ||
       (int)m_inner_cursor == m_ndbtab->getNoOfColumns()) {
      m_inner_cursor=0;
      do {
        const DictionaryList::Element * elem = nextInList();
        if(elem == nullptr) return nullptr; // end of list
        getTable(elem);
        /* Fixme: also get columns from blob tables */
      } while(m_ndbtab == nullptr);
    }
    return m_ndbtab->getColumn(m_inner_cursor++);
  }

  void buildIndex() {
    if(m_list.count > 0) {
      for(unsigned int i = 0; i < m_list.count ; i++)
        m_list_index[m_list.elements[i].id] = i;
      m_using_index = true;
    }
  }

  bool usingIndex() { return m_using_index; }

  int seek(std::map<int, int>::const_iterator &iter,
           const int &key, NdbInfoScanOperation::Seek seek) const {
    return index_seek(m_list_index, iter, key, seek);
  }

  ~VirtualScanContext() {
    if (m_record) m_ndb->getDictionary()->releaseRecord(m_record);
    if (m_scan_op) m_scan_op->close();
    if (m_trans) m_ndb->closeTransaction(m_trans);
    releaseTable();
    releaseIndex();
    delete m_ndb;
  }

 private:
  std::map<int, int> m_list_index;
  DictionaryList m_list, m_inner_list;
  Uint32 m_list_cursor{0}, m_inner_cursor{0};
  bool m_using_index{false};
  Ndb_cluster_connection *const m_connection;
  Ndb *m_ndb{nullptr};
  const NdbDictionary::Table *m_ndbtab{nullptr};
  const NdbDictionary::Index *m_ndbindex{nullptr};
  NdbTransaction *m_trans{nullptr};
  NdbScanOperation *m_scan_op{nullptr};
  NdbRecord* m_record{nullptr};
};

int NdbInfoScanVirtual::execute()
{
  DBUG_ENTER("NdbInfoScanVirtual::execute");
  if (m_state != Prepared)
    DBUG_RETURN(NdbInfo::ERR_WrongState);

  {
    // Allocate the row buffer big enough to hold all the requested columns
    size_t buffer_size = 0;
    for (unsigned i = 0; i < m_table->columns(); i++)
    {

      if (!m_recAttrs.is_requested(i))
        continue;

      const NdbInfo::Column* col = m_table->getColumn(i);
      switch (col->m_type)
      {
      case NdbInfo::Column::Number:
        buffer_size += sizeof(Uint32);
        break;
      case NdbInfo::Column::Number64:
        buffer_size += sizeof(Uint64);
        break;
      case NdbInfo::Column::String:
        buffer_size += 516; // Varchar(512) plus terminator plus padding
        break;
      }
    }

    m_buffer = new char[buffer_size];
    if (!m_buffer)
      DBUG_RETURN(NdbInfo::ERR_OutOfMemory);
    m_buffer_size = buffer_size;
  }

  if (!m_virt->start_scan(m_ctx))
    DBUG_RETURN(NdbInfo::ERR_VirtScanStart);

  m_state = MoreData;

  assert(m_row_counter == 0);
  DBUG_RETURN(0);
}


NdbInfoScanVirtual::NdbInfoScanVirtual(Ndb_cluster_connection *connection,
                                       const NdbInfo::Table *table,
                                       const VirtualTable *virt)
    : m_state(Undefined),
      m_table(table),
      m_virt(virt),
      m_recAttrs(table->columns()),
      m_buffer(nullptr),
      m_buffer_size(0),
      m_row_counter(0),
      m_ctx(new VirtualScanContext(connection))
{}

int NdbInfoScanVirtual::init()
{
  if (m_state != Undefined)
    return NdbInfo::ERR_WrongState;

  m_state = Initial;
  return NdbInfo::ERR_NoError;
}

void NdbInfoScanVirtual::initIndex(Uint32) {
  m_ctx->buildIndex();
}

bool NdbInfoScanVirtual::seek(NdbInfoScanOperation::Seek mode, int key) {
  int r;
  if(m_ctx->usingIndex())
    r = m_ctx->seek(m_index_pos, key, mode);
  else
    r = m_virt->seek(m_index_pos, key, mode);
  if(r == -1) return false;
  m_row_counter = r;
  return true;
}

NdbInfoScanVirtual::~NdbInfoScanVirtual()
{
  delete m_ctx;
  delete[] m_buffer;
}

// Tables begin here

#include "kernel/BlockNames.hpp"
class BlocksTable : public VirtualTable
{
public:
  BlocksTable() {
    for(int row = 0; row < NO_OF_BLOCKS ; row++)
    {
      const BlockName & bn = BlockNames[row];
      m_index[bn.number] = row;
    }
  }

  bool start_scan(VirtualScanContext*) const override { return true; }

  int read_row(VirtualScanContext*, VirtualTable::Row& w,
                Uint32 row_number) const override
  {
    if (row_number >= NO_OF_BLOCK_NAMES)
    {
      // No more rows
      return 0;
    }

    const BlockName& bn = BlockNames[row_number];
    w.write_number(bn.number);
    w.write_string(bn.name);
    return 1;
  }

  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table("blocks", this,
                                             NO_OF_BLOCK_NAMES);
    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("block_number", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("block_name", 1,
                                        NdbInfo::Column::String)))
      return nullptr;
    return tab;
  }
};


#include "kernel/signaldata/DictTabInfo.hpp"
class DictObjTypesTable : public VirtualTable
{
private:
  // The compiler will catch if the array size is too small
  static constexpr int OBJ_TYPES_TABLE_SIZE = 20;
  struct Entry {
     const DictTabInfo::TableType type;
     const char* name;
    };
    struct Entry entries[OBJ_TYPES_TABLE_SIZE] =
     {
       {DictTabInfo::SystemTable, "System table"},
       {DictTabInfo::UserTable, "User table"},
       {DictTabInfo::UniqueHashIndex, "Unique hash index"},
       {DictTabInfo::HashIndex, "Hash index"},
       {DictTabInfo::UniqueOrderedIndex, "Unique ordered index"},
       {DictTabInfo::OrderedIndex, "Ordered index"},
       {DictTabInfo::HashIndexTrigger, "Hash index trigger"},
       {DictTabInfo::SubscriptionTrigger, "Subscription trigger"},
       {DictTabInfo::ReadOnlyConstraint, "Read only constraint"},
       {DictTabInfo::IndexTrigger, "Index trigger"},
       {DictTabInfo::ReorgTrigger, "Reorganize trigger"},
       {DictTabInfo::Tablespace, "Tablespace"},
       {DictTabInfo::LogfileGroup,  "Log file group"},
       {DictTabInfo::Datafile, "Data file"},
       {DictTabInfo::Undofile, "Undo file"},
       {DictTabInfo::HashMap, "Hash map"},
       {DictTabInfo::ForeignKey, "Foreign key definition"},
       {DictTabInfo::FKParentTrigger, "Foreign key parent trigger"},
       {DictTabInfo::FKChildTrigger, "Foreign key child trigger"},
       {DictTabInfo::SchemaTransaction, "Schema transaction"}
     };

public:
  DictObjTypesTable() {
    for(int row_count = 0 ; row_count < OBJ_TYPES_TABLE_SIZE ; row_count++)
    {
      m_index[entries[row_count].type] = row_count;
    }
  }

  bool start_scan(VirtualScanContext*) const override { return true; }

  int read_row(VirtualScanContext*, VirtualTable::Row& w,
                Uint32 row_number) const override
  {
    if (row_number >= OBJ_TYPES_TABLE_SIZE)
    {
      // No more rows
      return 0;
    }

    const Entry& e = entries[row_number];
    w.write_number(e.type);
    w.write_string(e.name);
    return 1;
  }

  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table("dict_obj_types", this,
                                             OBJ_TYPES_TABLE_SIZE);
    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("type_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("type_name", 1,
                                        NdbInfo::Column::String)))
      return nullptr;
    return tab;
  }
};

/*
   This class implements a table returning all the NDB specific
   error codes as rows in table error_messages.

   There are three different type of error codes

   1) MgmApi error codes
   2) NDB error codes
   3) NDB exit codes(from ndbmtd or ndbd)

   the table is filled in the above order.
*/

#include "../storage/ndb/include/ndbapi/ndberror.h"
#include "../storage/ndb/include/mgmapi/ndbd_exit_codes.h"
#include "../storage/ndb/include/mgmapi/mgmapi_error.h"
class ErrorCodesTable : public VirtualTable
{
  struct ErrorMessage {
    int err_no;
    const char * status_msg;
    const char * class_msg;
    const char * error_msg;
  };

  Vector<ErrorMessage> m_error_messages;
public:
  bool init() override
  {
    // Build an index into the three error message arrays
    // to allow further lookup of the error messages by "row_number"

    // Iterate Mgmapi errors
    for (int i = 0;i < ndb_mgm_noOfErrorMsgs; i++)
    {
      ErrorMessage err;
      err.err_no = ndb_mgm_error_msgs[i].code;
      err.status_msg = ndb_mgm_error_msgs[i].msg;
      err.class_msg = ""; // No status for MgmApi
      err.error_msg= ""; // No classification for MgmApi

      if (m_error_messages.push_back(err) != 0)
        return false;
    }

    // Iterate NDB errors
    for (int i = 0;; i++)
    {
      int err_no;
      const char * status_msg;
      const char * class_msg;
      const char * error_msg;
      if (ndb_error_get_next(i, &err_no, &status_msg,
                             &class_msg, &error_msg) == -1)
      {
        // No more NDB errors
        break;
      }

      ErrorMessage err;
      err.err_no = err_no;
      err.status_msg = status_msg;
      err.class_msg = class_msg;
      err.error_msg= error_msg;

      if (m_error_messages.push_back(err) != 0)
        return false;
    }

    // Iterate NDB exit codes
    for (int i = 0;; i++)
    {
      int exit_code;
      const char * status_msg;
      const char * class_msg;
      const char * error_msg;
      if (ndbd_exit_code_get_next(i, &exit_code, &status_msg,
                                  &class_msg, &error_msg) == -1)
      {
        // No more ndbd exit codes
        break;
      }

      ErrorMessage err;
      err.err_no = exit_code;
      err.status_msg = status_msg;
      err.class_msg = class_msg;
      err.error_msg= error_msg;

      if (m_error_messages.push_back(err) != 0)
        return false;
    }

    return true;
  }

  bool start_scan(VirtualScanContext*) const override { return true; }

  int read_row(VirtualScanContext*, VirtualTable::Row& w,
               Uint32 row_number) const override
  {
    if (row_number >= m_error_messages.size())
    {
      // No more rows
      return 0;
    }

    const ErrorMessage& err = m_error_messages[row_number];

    w.write_number(err.err_no); // error_code
    w.write_string(err.error_msg); // error_description
    w.write_string(err.status_msg); // error_status
    w.write_string(err.class_msg); // error_classification

    return 1;
  }

  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table("error_messages", this,
                                             m_error_messages.size());
    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("error_code", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("error_description", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("error_status", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("error_classification", 3,
                                        NdbInfo::Column::String)))
      return nullptr;
    return tab;
  }

};

#include "mgmapi/mgmapi_config_parameters.h"
#include "mgmcommon/ConfigInfo.hpp"
class ConfigParamsTable : public VirtualTable
{
  ConfigInfo m_config_info;
  // Index by "row_number" into ConfigInfo
  Vector<const ConfigInfo::ParamInfo*> m_config_params;
public:
  bool init() override
  {
    // Build an index to allow further lookup
    // of the values by "row_number"
    const ConfigInfo::ParamInfo* pinfo= nullptr;
    ConfigInfo::ParamInfoIter param_iter(m_config_info,
                                         CFG_SECTION_NODE,
                                         NODE_TYPE_DB);
    int row_count = 0;

    while((pinfo= param_iter.next())) {
      if (pinfo->_paramId == 0 || // KEY_INTERNAL
          pinfo->_status != ConfigInfo::CI_USED)
        continue;

      if (m_config_params.push_back(pinfo) != 0)
        return false;

      m_index[pinfo->_paramId] = row_count++;
    }
    return true;
  }

  bool start_scan(VirtualScanContext*) const override { return true; }

  int read_row(VirtualScanContext*, VirtualTable::Row& w,
               Uint32 row_number) const override
  {
    if (row_number >= m_config_params.size())
    {
      // No more rows
      return 0;
    }

    char tmp_buf[256];
    const ConfigInfo::ParamInfo* const param = m_config_params[row_number];
    const char* const param_name = param->_fname;

    w.write_number(param->_paramId); // param_number
    w.write_string(param_name); // param_name
    w.write_string(param->_description); // param_description

     // param_type
    const char* param_type;
    switch (param->_type)
    {
      case ConfigInfo::CI_BOOL:
        param_type = "bool";
        break;
      case ConfigInfo::CI_INT:
      case ConfigInfo::CI_INT64:
        param_type = "unsigned";
        break;
      case ConfigInfo::CI_ENUM:
        param_type = "enum";
        break;
      case ConfigInfo::CI_BITMASK:
        param_type = "bitmask";
        break;
      case ConfigInfo::CI_STRING:
        param_type = "string";
        break;
      default:
        assert(false);
        param_type = "unknown";
        break;
    }
    w.write_string(param_type);

    const ConfigInfo& info = m_config_info;
    const Properties* const section = info.getInfo(param->_section);
    switch (param->_type)
    {
    case ConfigInfo::CI_BOOL:
    {
      // param_default
      BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%s", "");
      if (info.hasDefault(section, param_name))
        BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%llu", info.getDefault(section, param_name));
      w.write_string(tmp_buf);

       // param_min
      w.write_string("");

      // param_max
      w.write_string("");
      break;
    }

    case ConfigInfo::CI_INT:
    case ConfigInfo::CI_INT64:
    {
       // param_default
      BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%s", "");
      if (info.hasDefault(section, param_name))
        BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%llu", info.getDefault(section, param_name));
      w.write_string(tmp_buf);

       // param_min
      BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%llu", info.getMin(section, param_name));
      w.write_string(tmp_buf);

       // param_max
      BaseString::snprintf(tmp_buf, sizeof(tmp_buf), "%llu", info.getMax(section, param_name));
      w.write_string(tmp_buf);
      break;
    }

    case ConfigInfo::CI_BITMASK:
    case ConfigInfo::CI_ENUM:
    case ConfigInfo::CI_STRING:
    {
       // param_default
      const char* default_value = "";
      if (info.hasDefault(section, param_name))
        default_value = info.getDefaultString(section, param_name);
      w.write_string(default_value);

       // param_min
      w.write_string("");

       // param_max
      w.write_string("");
      break;
    }

    case ConfigInfo::CI_SECTION:
      abort(); // No sections should appear here
      break;
    }

     // param_mandatory
    Uint32 mandatory = 0;
    if (info.getMandatory(section, param_name))
      mandatory = 1;
    w.write_number(mandatory);

     // param_status
    const char* status_str = "";
    Uint32 status = info.getStatus(section, param_name);
    if (status & ConfigInfo::CI_EXPERIMENTAL)
      status_str = "experimental";
    if (status & ConfigInfo::CI_DEPRECATED)
      status_str = "deprecated";
    w.write_string(status_str);

    return 1;
  }


  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table("config_params", this,
                                             m_config_params.size());
    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("param_number", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("param_name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_description", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_type", 3,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_default", 4,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_min", 5,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_max", 6,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("param_mandatory", 7,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("param_status", 8,
                                        NdbInfo::Column::String)))

      return nullptr;
    return tab;
  }
};


#include "kernel/statedesc.hpp"

class NdbkernelStateDescTable : public VirtualTable
{
  // Pointer to the null terminated array
  const ndbkernel_state_desc* m_array;
  // Number of elements in the the array
  size_t m_array_count;
  const char* m_table_name;
public:

  NdbkernelStateDescTable(const char* table_name,
                          const ndbkernel_state_desc* null_terminated_array) :
    m_array(null_terminated_array),
    m_array_count(0),
    m_table_name(table_name)
  {
    while(m_array[m_array_count].name != nullptr)
    {
       m_index[m_array[m_array_count].value] = m_array_count;
       m_array_count++;
    }
  }

  bool start_scan(VirtualScanContext*) const override { return true; }

  int read_row(VirtualScanContext*, VirtualTable::Row& w,
               Uint32 row_number) const override
  {
    if (row_number >= m_array_count)
    {
      // No more rows
      return 0;
    }

    const ndbkernel_state_desc* desc = &m_array[row_number];
    w.write_number(desc->value);
    w.write_string(desc->name);
    w.write_string(desc->friendly_name);
    w.write_string(desc->description);

    return 1;
  }

  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table(m_table_name, this, m_array_count);

    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("state_int_value", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("state_name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("state_friendly_name", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("state_description", 3,
                                        NdbInfo::Column::String)))
      return nullptr;
    return tab;
  }
};


// Virtual table which reads the backup id from SYSTAB_0, the table have
// one column and one row
class BackupIdTable : public VirtualTable
{
  bool pk_read_backupid(const NdbDictionary::Table *ndbtab,
                        NdbTransaction *trans, Uint64 *backup_id,
                        Uint32 *fragment, Uint64 *rowid) const {
    DBUG_TRACE;
    // Get NDB operation
    NdbOperation *op = trans->getNdbOperation(ndbtab);
    if (op == nullptr) {
      return false;
    }

    // Primary key committed read of the "backup id" row
    if (op->readTuple(NdbOperation::LM_CommittedRead) != 0 ||
        op->equal("SYSKEY_0", NDB_BACKUP_SEQUENCE) != 0) { // Primary key
      assert(false);
      return false;
    }

    NdbRecAttr *nextid = op->getValue("NEXTID");
    NdbRecAttr *frag = op->getValue(NdbDictionary::Column::FRAGMENT);
    NdbRecAttr *row = op->getValue(NdbDictionary::Column::ROWID);

    // Specify columns to reads, the backup id and two pseudo columns
    if ((nextid == nullptr) || (frag == nullptr) || (row == nullptr))
    {
      assert(false);
      return false;
    }

    // Execute transaction
    if (trans->execute(NdbTransaction::NoCommit) != 0) {
      return false;
    }

    // Successful read, assign return value
    *backup_id = nextid->u_64_value();
    *fragment = frag->u_32_value();
    *rowid = row->u_64_value();

    return true;
  }

public:

  bool start_scan(VirtualScanContext* ctx) const override {
    DBUG_TRACE;
    if (!ctx->create_ndb()) return false;
    if (!ctx->openTable("SYSTAB_0")) return false;
    if (!ctx->startTrans()) return false;
    return true;
  }

  int read_row(VirtualScanContext* ctx, VirtualTable::Row& w,
               Uint32 row_number) const override
  {
    DBUG_TRACE;
    if (row_number >= 1)
    {
      // No more rows
      return 0;
    }

     // Read backup id from NDB
    Uint64 backup_id;
    Uint32 fragment;
    Uint64 row_id;
    if (!pk_read_backupid(ctx->getTable(), ctx->getTrans(), &backup_id,
                          &fragment, &row_id))
      return NdbInfo::ERR_ClusterFailure;

    w.write_number64(backup_id);  // id
    w.write_number(fragment);     // fragment
    w.write_number64(row_id);     // row_id
    return 1;
  }


  NdbInfo::Table* get_instance() const override
  {
    NdbInfo::Table* tab = new NdbInfo::Table("backup_id", this, 1);

    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("id", 0,
                                        NdbInfo::Column::Number64)))
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("fragment", 1,
                                        NdbInfo::Column::Number)))
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("row_id", 2,
                                        NdbInfo::Column::Number64)))
      return nullptr;
    return tab;
  }
};

class IndexStatsTable : public VirtualTable {
  struct IndexStatRow {
    Uint32 index_id;
    Uint32 index_version;
    Uint32 sample_version;
  };

public:
  bool start_scan(VirtualScanContext *ctx) const override {
    DBUG_TRACE;
    if (!ctx->create_ndb("mysql")) return false;
    if (!ctx->openTable("ndb_index_stat_head")) return false;
    if (!ctx->startTrans()) return false;
    const NdbDictionary::Table *const ndbtab = ctx->getTable();
    const NdbDictionary::Column *const index_id_col =
        ndbtab->getColumn("index_id");
    const NdbDictionary::Column *const index_version_col =
        ndbtab->getColumn("index_version");
    const NdbDictionary::Column *const sample_version_col =
        ndbtab->getColumn("sample_version");

    // Set up record specification for the 3 columns
    NdbDictionary::RecordSpecification record_spec[3];
    record_spec[0].column = index_id_col;
    record_spec[0].offset = offsetof(IndexStatRow, index_id);
    record_spec[0].nullbit_byte_offset = 0;  // Not nullable
    record_spec[0].nullbit_bit_in_byte = 0;

    record_spec[1].column = index_version_col;
    record_spec[1].offset = offsetof(IndexStatRow, index_version);
    record_spec[1].nullbit_byte_offset = 0;  // Not nullable
    record_spec[1].nullbit_bit_in_byte = 0;

    record_spec[2].column = sample_version_col;
    record_spec[2].offset = offsetof(IndexStatRow, sample_version);
    record_spec[2].nullbit_byte_offset = 0;  // Not nullable
    record_spec[2].nullbit_bit_in_byte = 0;
    if (!ctx->createRecord(record_spec, 3, sizeof(record_spec[0]))) {
      return false;
    }

    // Set up attribute mask to scan only the 3 columns of interest
    const unsigned char attr_mask = ((1 << index_id_col->getColumnNo()) |
                                     (1 << index_version_col->getColumnNo()) |
                                     (1 << sample_version_col->getColumnNo()));
    if (!ctx->scanTable(ctx->getRecord(), NdbOperation::LM_Read, &attr_mask)) {
      return false;
    }
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w,
               Uint32) const override {
    const IndexStatRow *row_data;
    const int scan_next_result =
        ctx->getScanOp()->nextResult((const char **)&row_data, true, false);
    if (scan_next_result == 0) {
      // Row found
      w.write_number(row_data->index_id);
      w.write_number(row_data->index_version);
      w.write_number(row_data->sample_version);
      return 1;
    }
    if (scan_next_result == 1) {
      // No more rows
      return 0;
    }
    // Error
    return NdbInfo::ERR_ClusterFailure;
  }

  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table *tab = new NdbInfo::Table("index_stats", this,
                                             64, // Hard-coded estimate
                                             false);
    if (!tab)
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("index_id", 0,
                                        NdbInfo::Column::Number)))
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("index_version", 1,
                                        NdbInfo::Column::Number)))
      return nullptr;
    if (!tab->addColumn(NdbInfo::Column("sample_version", 2,
                                        NdbInfo::Column::Number)))
      return nullptr;
    return tab;
  }
};

class DictionaryTablesTable : public VirtualTable {

public:
  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("dictionary_tables", this,
                                             40, false,
                                             NdbInfo::TableName::NoPrefix);
    if (!tab) return nullptr;
    if (!tab->addColumn(NdbInfo::Column("database_name", 0,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("table_name", 1,
                                         NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("table_id", 2,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("status", 3,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("attributes", 4,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("primary_key_cols", 5,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("primary_key", 6,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("storage", 7,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("logging", 8,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("dynamic", 9,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("read_backup", 10,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("fully_replicated", 11,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("checksum", 12,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("row_size", 13,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("min_rows", 14,
                                        NdbInfo::Column::Number64)) ||
        !tab->addColumn(NdbInfo::Column("max_rows", 15,
                                        NdbInfo::Column::Number64)) ||
        !tab->addColumn(NdbInfo::Column("tablespace", 16,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("fragment_type", 17,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("hash_map", 18,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("fragments", 19,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("partitions", 20,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("partition_balance", 21,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("contains_GCI", 22,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("single_user_mode", 23,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("force_var_part", 24,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("GCI_bits", 25,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("author_bits", 26,
                                        NdbInfo::Column::Number)))
      return nullptr;
    return tab;
  }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::UserTable);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32 row)
      const override {

    const DictionaryList::Element * elem;
    const NdbDictionary::Table * t;

    do {
      elem = ctx->nextInList(static_cast<int>(row));
      if(elem == nullptr) return 0;  // end of list
      t = ctx->getTable(elem);
      if(t == nullptr) t = ctx->getBlobTable(elem);
      if(ctx->usingIndex() && (t == nullptr)) return 0;
    } while (t == nullptr);

    {
      NdbDictionary::HashMap hashMap;
      char pk_cols[column_buff_size];
      size_t s = 0;

      // Get value for column "dynamic"
      int is_dynamic = [t]() {
        if(t->getForceVarPart()) return 1;
        for(int i = 0 ; i < t->getNoOfColumns() ; i++)
          if(t->getColumn(i)->getDynamic()) return 1;
        return 0;
      }();

      // Get value for column "hash_map"
      ctx->getDictionary()->getHashMap(hashMap, t);

      // Get value for column "primary_key"
      for(int i = 0 ; i < t->getNoOfPrimaryKeys() ; i++) {
        if(i) s += snprintf(pk_cols+s, column_buff_size-s, ",");
        s += snprintf(pk_cols+s, column_buff_size-s, "%s", t->getPrimaryKey(i));
      }

      // Get tablespace ID
      Uint32 tablespace_id;
      if (!t->getTablespace(&tablespace_id)) tablespace_id = 0;

      w.write_string(elem->database);                       // database
      w.write_string(t->getName());                         // name
      w.write_number(t->getTableId());                      // table_id
      w.write_number(t->getObjectStatus() + 1);             // status
      w.write_number(t->getNoOfColumns());                  // attributes
      w.write_number(t->getNoOfPrimaryKeys());              // primary_key_cols
      w.write_string(pk_cols),                              // primary_key
      w.write_number((int)(t->getStorageType()) + 1);       // storage
      w.write_number(t->getLogging());                      // logging
      w.write_number(is_dynamic);                           // dynamic
      w.write_number(t->getReadBackupFlag());               // read_backup
      w.write_number(t->getFullyReplicated());              // fully_replicated
      w.write_number(t->getRowChecksumIndicator());         // checksum
      w.write_number(t->getRowSizeInBytes());               // row_size
      w.write_number64(t->getMinRows());                    // min_rows
      w.write_number64(t->getMaxRows());                    // max_rows
      w.write_number(tablespace_id);                        // tablespace
      w.write_number((int) t->getFragmentType());           // fragment_type
      w.write_string(hashMap.getName());                    // hash_map
      w.write_number(t->getFragmentCount());                // fragments
      w.write_number(t->getPartitionCount());               // partitions
      w.write_string(t->getPartitionBalanceString());       // partition_balance
      w.write_number(t->getRowGCIIndicator());              // contains_GCI
      w.write_number(t->getSingleUserMode() + 1);           // single_user_mode
      w.write_number(t->getForceVarPart());                 // force_var_part
      w.write_number(t->getExtraRowGciBits());              // GCI_bits
      w.write_number(t->getExtraRowAuthorBits());           // author_bits
    }
    return 1;
  }
};

class BlobsTable : public VirtualTable {
  public:
  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("blobs", this, 10, false,
                                             NdbInfo::TableName::NoPrefix);
    if (!tab) return nullptr;
    if (!tab->addColumn(NdbInfo::Column("table_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("database_name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("table_name", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("column_id", 3,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("column_name", 4,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("inline_size", 5,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("part_size", 6,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("stripe_size", 7,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("blob_table_name", 8,
                                        NdbInfo::Column::String)))
      return nullptr;
    return tab;
 }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::UserTable);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32)
      const override {

    const DictionaryList::Element * elem;
    const NdbDictionary::Table * t;
    BlobSource blob;

    do {
      elem = ctx->nextInList();
      if(elem == nullptr) return 0;  // end of list
      t = ctx->getTable(elem) ? nullptr : ctx->getSourceTable(elem->name, blob);
    } while (t == nullptr);

    const NdbDictionary::Column * col = t->getColumn(blob.col_id);
    assert(col);
    assert(col->getBlobTable());

    w.write_number(t->getTableId());                      // table_id
    w.write_string(elem->database);                       // database_name
    w.write_string(t->getName());                         // table_name
    w.write_number(blob.col_id);                          // column_id
    w.write_string(col->getName());                       // column_name
    w.write_number(col->getInlineSize());                 // inline_size
    w.write_number(col->getPartSize());                   // part_size
    w.write_number(col->getStripeSize());                 // stripe_size
    w.write_string(col->getBlobTable()->getName());       // blob_table_name

    return 1;
  }
};

class EventsTable : public VirtualTable {
  public:
  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("events", this, 40, false,
                                             NdbInfo::TableName::NoPrefix);
    if (!tab) return nullptr;
    if (!tab->addColumn(NdbInfo::Column("event_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("table_id", 2,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("reporting", 3,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("columns", 4,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("table_event", 5,
                                        NdbInfo::Column::Number)))
       return nullptr;
     return tab;
  }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::TableEvent);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32 row)
      const override {

    const DictionaryList::Element * elem;
    NdbDictionary::Event_ptr event;

    do {
      elem = ctx->nextInList(row);
      if (elem == nullptr) return 0;   // end of list
      // Open Event by name from NDB
      event.reset(ctx->getDictionary()->getEvent(elem->name));
      if( ctx->usingIndex() && (event == nullptr)) return 0;
    } while(event == nullptr);

    {
      char event_columns[column_buff_size];
      size_t s = 0;
      Uint32 reporting = 0;
      Uint32 table_event = 0;

      /* columns */
      for(int i = 0 ; i < event->getNoOfEventColumns() ; i++) {
        if(i) s += snprintf(event_columns+s, column_buff_size-s, ",");
        s += snprintf(event_columns+s, column_buff_size-s, "%s",
                      event->getEventColumn(i)->getName());
      }

      /* reporting
         The options describing how the Event is configured to report are
         translated into a SET
      */
      const Uint32 report_options = event->getReportOptions();
      if ((report_options & NdbDictionary::Event::ER_ALL) == 0)
        reporting |= 1;  // ER_UPDATED
      else
        reporting |= 2;
      if (report_options & NdbDictionary::Event::ER_SUBSCRIBE) reporting |= 4;
      if (report_options & NdbDictionary::Event::ER_DDL) reporting |= 8;

      /* table_event
         The first 13 bits are contiguous in the bitfield.
         The three exceptional event types are *not* reported here:
           TE_EMPTY, TE_INCONSISTENT, TE_OUT_OF_MEMORY.
         If all 13 bits are set, rewrite the value to "ALL".
      */
      for(Uint32 i = 0; i < 13 ; i++)
        if(event->getTableEvent((NdbDictionary::Event::TableEvent)(1<<i)))
          table_event |= 1<<i;

      if(table_event == 0x1FFF) table_event++; // "ALL"

      w.write_number(elem->id);                             // id
      w.write_string(elem->name);                           // name
      w.write_number(event->getTable()->getObjectId());     // table_id
      w.write_number(reporting);                            // reporting
      w.write_string(event_columns);                        // columns
      w.write_number(table_event);                          // table_event
    }
    return 1;
  }
};

class IndexColumnsTable : public VirtualTable {
  public:
  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("index_columns", this, 20, false,
                                               NdbInfo::TableName::NoPrefix);
      if (!tab) return nullptr;
      if (!tab->addColumn(NdbInfo::Column("table_id", 0,
                                          NdbInfo::Column::Number)) ||
          !tab->addColumn(NdbInfo::Column("database_name", 1,
                                          NdbInfo::Column::String)) ||
          !tab->addColumn(NdbInfo::Column("table_name", 2,
                                          NdbInfo::Column::String)) ||
          !tab->addColumn(NdbInfo::Column("index_object_id", 3,
                                          NdbInfo::Column::Number)) ||
          !tab->addColumn(NdbInfo::Column("index_name", 4,
                                          NdbInfo::Column::String)) ||
          !tab->addColumn(NdbInfo::Column("index_type", 5,
                                          NdbInfo::Column::Number)) ||
          !tab->addColumn(NdbInfo::Column("status", 6,
                                          NdbInfo::Column::Number)) ||
          !tab->addColumn(NdbInfo::Column("columns", 7,
                                          NdbInfo::Column::String)))
         return nullptr;
       return tab;
  }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::UserTable);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32)
      const override {

    char index_columns[column_buff_size];
    size_t s = 0;

    const NdbDictionary::Index * index = ctx->nextIndex();
    if(index == nullptr) return 0;   // end of list

    /* columns */
    for(unsigned int i = 0 ; i < index->getNoOfColumns() ; i++) {
      if(i) s += snprintf(index_columns+s, column_buff_size-s, ",");
      s += snprintf(index_columns+s, column_buff_size-s, "%s",
                    index->getColumn(i)->getName());
    }

    w.write_number(ctx->getTable()->getTableId());          // table_id
    w.write_string(ctx->getDatabaseName());                 // database_name
    w.write_string(index->getTable());                      // table_name
    w.write_number(index->getObjectId());                   // index_object_id
    w.write_string(index->getName());                       // index_name
    w.write_number(index->getType());                       // index_type
    w.write_number(index->getObjectStatus() + 1);           // status
    w.write_string(index_columns);                          // columns

    return 1;
  }
};

class ForeignKeysTable : public VirtualTable {
public:

  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("foreign_keys", this, 10, false,
                                               NdbInfo::TableName::NoPrefix);

    if (!tab->addColumn(NdbInfo::Column("object_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("parent_table", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("parent_columns", 3,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("child_table", 4,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("child_columns", 5,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("parent_index", 6,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("child_index", 7,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("on_update_action", 8,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("on_delete_action", 9,
                                        NdbInfo::Column::Number)))
      return nullptr;
    return tab;
  }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::ForeignKey);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32 row)
      const override {

    char parent_columns[column_buff_size];
    char child_columns[column_buff_size];
    static constexpr const char * pk = "PK";
    size_t s = 0;

    NdbDictionary::ForeignKey fk;
    const DictionaryList::Element * elem;

    elem = ctx->nextInList(row);
    if (elem == nullptr) return 0;   // end of list
    int r = ctx->getDictionary()->getForeignKey(fk, elem->name);
    if(r != 0) return 0; // error from getForeignKey()

    /* parent_columns */
    for(unsigned int i = 0 ; i < fk.getParentColumnCount() ; i++) {
      s += snprintf(parent_columns+s, column_buff_size-s, "%s%d",
                    i ? "," : "", fk.getParentColumnNo(i));
    }

    /* child_columns */
    s = 0;
    for(unsigned int i = 0 ; i < fk.getChildColumnCount() ; i++) {
      s += snprintf(child_columns+s, column_buff_size-s, "%s%d",
                    i ? "," : "", fk.getChildColumnNo(i));
    }

    const char * childIdx = fk.getChildIndex() ? fk.getChildIndex() : pk;
    const char * parentIdx = fk.getParentIndex() ? fk. getParentIndex() : pk;

    w.write_number(fk.getObjectId());                   // object_id
    w.write_string(fk.getName());                       // name
    w.write_string(fk.getParentTable());                // parent_table
    w.write_string(parent_columns);                     // parent_columns
    w.write_string(fk.getChildTable());                 // child_table
    w.write_string(child_columns);                      // child_columns
    w.write_string(childIdx);                           // parent_index
    w.write_string(parentIdx);                          // child_index
    w.write_number(fk.getOnUpdateAction() + 1);         // on_update
    w.write_number(fk.getOnDeleteAction() + 1);         // on_delete

    return 1;
  }
};

class ColumnsTable : public VirtualTable {
public:

  NdbInfo::Table* get_instance() const override {
    NdbInfo::Table* tab = new NdbInfo::Table("dictionary_columns",
                                             this, 200, false, /* estimate */
                                             NdbInfo::TableName::NoPrefix);

    if (!tab->addColumn(NdbInfo::Column("table_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("column_id", 1,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("name", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("column_type", 3,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("default_value", 4,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("nullable", 5,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("array_type", 6,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("storage_type", 7,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("primary_key", 8,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("partition_key", 9,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("dynamic", 10,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("auto_inc", 11,
                                        NdbInfo::Column::Number)))
      return nullptr;
    return tab;
  }

  bool start_scan(VirtualScanContext *ctx) const override {
    if (!ctx->create_ndb()) return false;
    ctx->listObjects(NdbDictionary::Object::UserTable);
    return true;
  }

  int read_row(VirtualScanContext *ctx, VirtualTable::Row &w, Uint32)
     const override {

    NdbDictionary::NdbDataPrintFormat defaultFormat;
    defaultFormat.hex_format = 1;  /* Display binary field defaults as hex */
    char col_type[column_buff_size];
    char default_value[column_buff_size];
    StaticBuffOutputStream type_buf(col_type, column_buff_size);
    StaticBuffOutputStream default_buf(default_value, column_buff_size);
    NdbOut type_buf_out(type_buf);
    NdbOut default_buf_out(default_buf);

    const NdbDictionary::Column *col = ctx->nextColumn();
    if (col == nullptr) return 0;   // end of list

    const void * col_default = col->getDefaultValue();
    NdbDictionary::printColumnTypeDescription(type_buf_out, *col);
    if(col_default != nullptr)
      NdbDictionary::printFormattedValue(default_buf_out, defaultFormat, col,
                                         col_default);

    w.write_number(ctx->getTable()->getTableId());     // table_id
    w.write_number(col->getColumnNo());                // column_id
    w.write_string(col->getName());                    // name
    w.write_string(col_type);                          // column_type
    w.write_string(col_default ? default_value : "");  // default_value
    w.write_number(col->getNullable() + 1);            // nullable
    w.write_number(col->getArrayType() + 1);           // array_type
    w.write_number(col->getStorageType() + 1);         // storage_type
    w.write_number(col->getPrimaryKey());              // primary_key
    w.write_number(col->getPartitionKey());            // partition_key
    w.write_number(col->getDynamic());                 // dynamic
    w.write_number(col->getAutoIncrement());           // auto_inc
    return 1;
  }
};

/*
  Create the virtual tables and stash them in the provided list.

  There is only one instance of each Virtual table. Its pointer
  is copied between instances of Table so that all Table instances
  use the same Virtual.

  The Virtual tables are created in NdbInfo::init() and destroyed
  by ~NdbInfo(). NdbInfo keeps them in a list which is passed to both
  the create and destroy functions.
*/
bool
NdbInfoScanVirtual::create_virtual_tables(Vector<NdbInfo::Table*>& list)
{
  // polymorphic lambda:
  auto add_table = [&list](auto t) {
    if (!t) return false;
    if (!t->init()) return false;
    if (list.push_back(t->get_instance()) != 0) return false;
    return true;
  };

  return (
    add_table(new BlocksTable) &&
    add_table(new DictObjTypesTable) &&
    add_table(new ErrorCodesTable) &&
    add_table(new ConfigParamsTable) &&
    add_table(new NdbkernelStateDescTable("dbtc_apiconnect_state",
                                          g_dbtc_apiconnect_state_desc)) &&
    add_table(new NdbkernelStateDescTable("dblqh_tcconnect_state",
                                          g_dblqh_tcconnect_state_desc)) &&
    add_table(new BackupIdTable) &&
    add_table(new IndexStatsTable) &&
    add_table(new DictionaryTablesTable) &&
    add_table(new BlobsTable) &&
    add_table(new EventsTable) &&
    add_table(new IndexColumnsTable) &&
    add_table(new ForeignKeysTable) &&
    add_table(new ColumnsTable)
  );
}

/*
  Delete the virtual part of the tables in list
*/
void NdbInfoScanVirtual::delete_virtual_tables(Vector<NdbInfo::Table*>& list)
{
  for (unsigned i = 0; i < list.size(); i++)
  {
    NdbInfo::Table* tab = list[i];
    const VirtualTable* virt = tab->getVirtualTable();
    delete virt;
    delete tab;
  }
  list.clear();
}
