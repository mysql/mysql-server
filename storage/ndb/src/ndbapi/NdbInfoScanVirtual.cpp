/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include "NdbInfo.hpp"
#include "NdbInfoScanVirtual.hpp"


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


int NdbInfoScanVirtual::execute()
{
  DBUG_ENTER("NdbInfoScanVirtual::execute");
  if (m_state != Prepared)
    DBUG_RETURN(NdbInfo::ERR_WrongState);

  {
    // Allocate the row buffer big enough to hold all
    // the reuqested columns
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
        buffer_size += 512+1; // Varchar(512) including null terminator
        break;
      }
    }

    m_buffer = new char[buffer_size];
    if (!m_buffer)
      DBUG_RETURN(NdbInfo::ERR_OutOfMemory);
    m_buffer_size = buffer_size;
  }

  m_state = MoreData;

  assert(m_row_counter == 0);
  DBUG_RETURN(0);
}


/*
  Interface class for virtual(aka. hardcoded) tables.
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
    Return the NdbInfo::Table corresponding to this virtual table
  */
  virtual NdbInfo::Table* get_instance() const = 0;

  /*
    Read one row from the virtual table

    Data for the row specified by row_number should be filled in
    by the functions provided by Virtual::Row

    false No more data
    true More rows available
  */
  virtual bool read_row(Row& row, Uint32 row_number) const = 0;

  virtual ~VirtualTable() = 0;
};


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
  m_curr+=clen;

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
  if (!m_virt->read_row(row, m_row_counter))
  {
    // No more rows
    m_state = End;
    DBUG_RETURN(0);
  }
  // Check that all columns where processed(i.e by the virtual table class)
  assert(row.m_col_counter == m_table->columns());

  m_row_counter++;
  DBUG_RETURN(1); // More rows
}


NdbInfoScanVirtual::NdbInfoScanVirtual(const NdbInfo::Table* table,
                                       const VirtualTable* virt) :
  m_state(Undefined),
  m_table(table),
  m_virt(virt),
  m_recAttrs(table->columns()),
  m_buffer(NULL),
  m_buffer_size(0),
  m_row_counter(0)
{
}


int NdbInfoScanVirtual::init()
{
  if (m_state != Undefined)
    return NdbInfo::ERR_WrongState;

  m_state = Initial;
  return NdbInfo::ERR_NoError;
}



NdbInfoScanVirtual::~NdbInfoScanVirtual()
{
  delete[] m_buffer;
}


#include "kernel/BlockNames.hpp"
class BlocksTable : public VirtualTable
{
public:
  virtual bool read_row(VirtualTable::Row& w,
                        Uint32 row_number) const
  {
    if (row_number >= NO_OF_BLOCK_NAMES)
    {
      // No more rows
      return false;
    }

    const BlockName& bn = BlockNames[row_number];
    w.write_number(bn.number);
    w.write_string(bn.name);
    return true;
  }

  NdbInfo::Table* get_instance() const
  {
    NdbInfo::Table* tab = new NdbInfo::Table("blocks",
                                             NdbInfo::Table::InvalidTableId,
                                             this);
    if (!tab)
      return NULL;
    if (!tab->addColumn(NdbInfo::Column("block_number", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("block_name", 1,
                                        NdbInfo::Column::String)))
      return NULL;
    return tab;
  }
};


#include "kernel/signaldata/DictTabInfo.hpp"
class DictObjTypesTable : public VirtualTable
{
public:
  virtual bool read_row(VirtualTable::Row& w,
                        Uint32 row_number) const
  {
    struct Entry {
     const DictTabInfo::TableType type;
     const char* name;
    };

    static const Entry entries[] =
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

    if (row_number >= sizeof(entries) / sizeof(entries[0]))
    {
      // No more rows
      return false;
    }

    const Entry& e = entries[row_number];
    w.write_number(e.type);
    w.write_string(e.name);
    return true;
  }

  NdbInfo::Table* get_instance() const
  {
    NdbInfo::Table* tab = new NdbInfo::Table("dict_obj_types",
                                             NdbInfo::Table::InvalidTableId,
                                             this);
    if (!tab)
      return NULL;
    if (!tab->addColumn(NdbInfo::Column("type_id", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("type_name", 1,
                                        NdbInfo::Column::String)))
      return NULL;
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
  bool init()
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

  virtual bool read_row(VirtualTable::Row& w,
                        Uint32 row_number) const
  {
    if (row_number >= m_error_messages.size())
    {
      // No more rows
      return false;
    }

    const ErrorMessage& err = m_error_messages[row_number];

    w.write_number(err.err_no); // error_code
    w.write_string(err.error_msg); // error_description
    w.write_string(err.status_msg); // error_status
    w.write_string(err.class_msg); // error_classification

    return true;
  }

  NdbInfo::Table* get_instance() const
  {
    NdbInfo::Table* tab = new NdbInfo::Table("error_messages",
                                             NdbInfo::Table::InvalidTableId,
                                             this);
    if (!tab)
      return NULL;
    if (!tab->addColumn(NdbInfo::Column("error_code", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("error_description", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("error_status", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("error_classification", 3,
                                        NdbInfo::Column::String)))
      return NULL;
    return tab;
  }

};

#include "mgmapi/mgmapi_config_parameters.h"
#include "../mgmsrv/ConfigInfo.hpp"
class ConfigParamsTable : public VirtualTable
{
  ConfigInfo m_config_info;
  // Index by "row_number" into ConfigInfo
  Vector<const ConfigInfo::ParamInfo*> m_config_params;
public:
  bool init()
  {
    // Build an index to allow further lookup
    // of the values by "row_number"
    const ConfigInfo::ParamInfo* pinfo= NULL;
    ConfigInfo::ParamInfoIter param_iter(m_config_info,
                                         CFG_SECTION_NODE,
                                         NODE_TYPE_DB);

    while((pinfo= param_iter.next())) {
      if (pinfo->_paramId == 0 || // KEY_INTERNAL
          pinfo->_status != ConfigInfo::CI_USED)
        continue;

      if (m_config_params.push_back(pinfo) != 0)
        return false;
    }
    return true;
  }

  virtual bool read_row(VirtualTable::Row& w,
                        Uint32 row_number) const
  {
    if (row_number >= m_config_params.size())
    {
      // No more rows
      return false;
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
        DBUG_ASSERT(false);
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

    return true;
  }


  NdbInfo::Table* get_instance() const
  {
    NdbInfo::Table* tab = new NdbInfo::Table("config_params",
                                             NdbInfo::Table::InvalidTableId,
                                             this);
    if (!tab)
      return NULL;
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

      return NULL;
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
    while(m_array[m_array_count].name != 0)
      m_array_count++;
  }

  virtual bool read_row(VirtualTable::Row& w,
                        Uint32 row_number) const
  {
    if (row_number >= m_array_count)
    {
      // No more rows
      return false;
    }

    const ndbkernel_state_desc* desc = &m_array[row_number];
    w.write_number(desc->value);
    w.write_string(desc->name);
    w.write_string(desc->friendly_name);
    w.write_string(desc->description);

    return true;
  }

  NdbInfo::Table* get_instance() const
  {
    NdbInfo::Table* tab = new NdbInfo::Table(m_table_name,
                                             NdbInfo::Table::InvalidTableId,
                                             this);
    if (!tab)
      return NULL;
    if (!tab->addColumn(NdbInfo::Column("state_int_value", 0,
                                        NdbInfo::Column::Number)) ||
        !tab->addColumn(NdbInfo::Column("state_name", 1,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("state_friendly_name", 2,
                                        NdbInfo::Column::String)) ||
        !tab->addColumn(NdbInfo::Column("state_description", 3,
                                        NdbInfo::Column::String)))
      return NULL;
    return tab;
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
  {
    BlocksTable* blocksTable = new BlocksTable;
    if (!blocksTable)
      return false;

    if (list.push_back(blocksTable->get_instance()) != 0)
      return false;
  }

  {
    DictObjTypesTable* dictObjTypesTable = new DictObjTypesTable;
    if (!dictObjTypesTable)
      return false;

    if (list.push_back(dictObjTypesTable->get_instance()) != 0)
      return false;
  }

  {
    ErrorCodesTable* errorCodesTable = new ErrorCodesTable;
    if (!errorCodesTable)
      return false;

    if (!errorCodesTable->init())
      return false;

    if (list.push_back(errorCodesTable->get_instance()) != 0)
      return false;
  }

  {
    ConfigParamsTable* configParamsTable = new ConfigParamsTable;
    if (!configParamsTable)
      return false;
    if (!configParamsTable->init())
      return false;

    if (list.push_back(configParamsTable->get_instance()) != 0)
      return false;
  }

  {
    NdbkernelStateDescTable* dbtcApiConnectStateTable =
        new NdbkernelStateDescTable("dbtc_apiconnect_state",
                                    g_dbtc_apiconnect_state_desc);
    if (!dbtcApiConnectStateTable)
      return false;

    if (list.push_back(dbtcApiConnectStateTable->get_instance()) != 0)
      return false;
  }

  {
    NdbkernelStateDescTable* dblqhTcConnectStateTable =
        new NdbkernelStateDescTable("dblqh_tcconnect_state",
                                    g_dblqh_tcconnect_state_desc);
    if (!dblqhTcConnectStateTable)
      return false;

    if (list.push_back(dblqhTcConnectStateTable->get_instance()) != 0)
      return false;
  }
  return true;
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
