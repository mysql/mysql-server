/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

  if (anAttrId >= m_recAttrs.size())
    DBUG_RETURN(NULL);

  NdbInfoRecAttr *recAttr = new NdbInfoRecAttr;
  m_recAttrs[anAttrId] = recAttr;
  DBUG_RETURN(recAttr);
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

      NdbInfoRecAttr* attr = m_recAttrs[i];
      if (!attr)
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
  protected:
    friend class NdbInfoScanVirtual;
    Row(class NdbInfoScanVirtual* owner,
        const NdbInfo::Table* table,
        Vector<class NdbInfoRecAttr*> attrs,
        char* buffer, size_t buf_size);

    Row(); // Prevent
    Row(const Row&); // Prevent
    ~Row();
  private:
    const class NdbInfoScanVirtual* const m_owner;
    const NdbInfo::Table* const m_table;
    char* const m_start;      // Start of row buffer
    char* const m_end;        // End of row buffer
    char* m_curr;             // Current position in row buffer
    unsigned m_col_counter;    // Current column counter

    void check_data_type(NdbInfo::Column::Type type) const;
    bool check_buffer_space(size_t len) const;
  };

  /*
    Read one row from the virtual table

    Data for the row specified by row_number should be filled in
    by the functions provided by Virtual::Row

    false No more data
    true More rows available
  */
  virtual bool read_row(Row& row, Uint32 row_number) const = 0;

  virtual ~VirtualTable() {}
};

void
VirtualTable::Row::check_data_type(NdbInfo::Column::Type type) const
{
 assert(m_table->getColumn(m_col_counter)->m_type == type);
}


bool
VirtualTable::Row::check_buffer_space(size_t len) const
{
  const Uint32 avail = (Uint32)(m_end - m_curr);

  if(len > avail)
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

  check_data_type(NdbInfo::Column::String);

  NdbInfoRecAttr* attr = m_owner->get_recattr(m_col_counter++);
  if (!attr)
    DBUG_VOID_RETURN;

  const size_t clen = strlen(str) + 1; // length including terminator
  if (!check_buffer_space(clen))
    return;

  // setup RecAttr
  m_owner->set_recattr(attr, m_curr, clen);

  // copy string to buffer
  memcpy(m_curr, str, clen);
  m_curr+=clen;

  DBUG_VOID_RETURN;
}


void VirtualTable::Row::write_number(Uint32 val) {
  DBUG_ENTER("write_number");
  DBUG_PRINT("enter", ("val: %u", val));

  check_data_type(NdbInfo::Column::Number);

  NdbInfoRecAttr* attr = m_owner->get_recattr(m_col_counter++);
  if (!attr)
    DBUG_VOID_RETURN;

  const size_t clen = sizeof(Uint32);
  if (!check_buffer_space(clen))
    return;

  // setup RecAttr
  m_owner->set_recattr(attr, m_curr, clen);

  // copy value to buffer
  memcpy(m_curr, &val, clen);
  m_curr+=clen;

  DBUG_VOID_RETURN;
}


void VirtualTable::Row::write_number64(Uint64 val) {
  DBUG_ENTER("write_number64");
  DBUG_PRINT("enter", ("val: %llu", val));

  check_data_type(NdbInfo::Column::Number64);

  NdbInfoRecAttr* attr = m_owner->get_recattr(m_col_counter++);
  if (!attr)
    DBUG_VOID_RETURN;

  const size_t clen = sizeof(Uint64);
  if (!check_buffer_space(clen))
    return;

  // setup recAttr
  m_owner->set_recattr(attr, m_curr, clen);

  // copy value to buffer
  memcpy(m_curr, &val, clen);
  m_curr+=clen;

  DBUG_VOID_RETURN;
}


VirtualTable::Row::Row(NdbInfoScanVirtual* owner,
                                  const NdbInfo::Table* table,
                                  Vector<class NdbInfoRecAttr*> attrs,
                                  char* buffer, size_t buf_size) :
  m_owner(owner),
  m_table(table),
  m_start(buffer),
  m_end(buffer + buf_size),
  m_curr(buffer),
  m_col_counter(0)
{
  // Reset all recattr values before reading the new row
  m_owner->reset_recattrs();
}


VirtualTable::Row::~Row()
{
}


int NdbInfoScanVirtual::nextResult()
{
  DBUG_ENTER("NdbInfoScanVirtual::nextResult");
  if (m_state != MoreData)
    DBUG_RETURN(-1);

  assert(m_buffer);

  VirtualTable::Row row(this, m_table, m_recAttrs,
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
  m_buffer(NULL),
  m_buffer_size(0),
  m_row_counter(0)
{
  for (unsigned i = 0; i < m_table->columns(); i++)
    m_recAttrs.push_back(NULL);
}


int NdbInfoScanVirtual::init()
{
  if (m_state != Undefined)
    return NdbInfo::ERR_WrongState;

  m_state = Initial;
  return NdbInfo::ERR_NoError;
}


void NdbInfoScanVirtual::set_recattr(NdbInfoRecAttr* attr,
                                     const char* data,
                                     Uint32 len) const
{
#ifndef DBUG_OFF
  // Make sure the given "attr" is in the list
  // of rec attrs this class owns.
  bool found = false;
  for (unsigned i = 0; i < m_recAttrs.size(); i++)
  {
    const NdbInfoRecAttr* a = m_recAttrs[i];
    if (a && attr == a)
    {
      found = true;
      break;
    }
  }
  assert(found);
#endif

  attr->m_data = data;
  attr->m_len = len;
  attr->m_defined = true;
}


NdbInfoRecAttr* NdbInfoScanVirtual::get_recattr(Uint32 col_number) const
{
  assert(col_number < m_recAttrs.size());
  return m_recAttrs[col_number];
}


void NdbInfoScanVirtual::reset_recattrs(void) const
{
  for (unsigned i = 0; i < m_recAttrs.size(); i++)
  {
    if (m_recAttrs[i])
      m_recAttrs[i]->m_defined = false;
  }
}


NdbInfoScanVirtual::~NdbInfoScanVirtual()
{
  for (unsigned i = 0; i < m_recAttrs.size(); i++)
  {
    if (m_recAttrs[i])
    {
      delete m_recAttrs[i];
      m_recAttrs[i] = NULL;
    }
  }

  delete[] m_buffer;
}


bool NdbInfo::load_virtual_tables(void)
{
  // The virtual tables should already have been created
  assert(m_virtual_tables.size() > 0);

  // Append the virtual tables to the list of tables
  for (size_t i = 0; i < m_virtual_tables.size(); i++)
  {
    Table* tab = m_virtual_tables[i];
    assert(tab->m_virt);
    assert(tab->m_table_id == Table::InvalidTableId);

    BaseString hash_key = mysql_table_name(tab->getName());
    tab->m_table_id = m_tables.entries(); // Set increasing table id
    if (!m_tables.insert(hash_key.c_str(), *tab))
      return false;
  }

  return true;
}


#include "../src/common/debugger/BlockNames.cpp"
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

    Entry entries[] =
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

    const ConfigInfo::ParamInfo* param = m_config_params[row_number];
    w.write_number(param->_paramId);
    w.write_string(param->_fname);
    return true;
  }
};


#include "kernel/statedesc.hpp"

class NdbkernelStateDescTable : public VirtualTable
{
  // Pointer to the null terminated array
  const ndbkernel_state_desc* m_array;
  // Number of elements in the the array
  size_t m_array_count;
public:

  NdbkernelStateDescTable(const ndbkernel_state_desc* null_terminated_array) :
    m_array(null_terminated_array),
    m_array_count(0)
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
};


/*
  Create the virtual tables and stash them in the provided list.

  There is only one instance of each Virtual table. Its pointer
  is copied between instances of Table so that all Table instances
  use the same Virtual.

  The Virtual tables are created during in NdbInfo::init() and destroyed
  by ~NdbInfo(). NdbInfo keeps them in a list which is passed to both
  the create and destroy functions.
*/
bool NdbInfo::create_virtual_tables(Vector<Table*>& list)
{
  {
    BlocksTable* blocksTable = new BlocksTable;
    if (!blocksTable)
      return false;

    Table* tab = new Table("blocks", Table::InvalidTableId, blocksTable);
    if (!tab)
      return false;
    if (!tab->addColumn(Column("block_number", 0, Column::Number)) ||
        !tab->addColumn(Column("block_name", 1, Column::String)))
      return false;

    if (list.push_back(tab) != 0)
      return false;
  }

  {
    DictObjTypesTable* dictObjTypesTable = new DictObjTypesTable;
    if (!dictObjTypesTable)
      return false;

    Table* tab = new Table("dict_obj_types", Table::InvalidTableId,
                           dictObjTypesTable);
    if (!tab)
      return false;
    if (!tab->addColumn(Column("type_id", 0, Column::Number)) ||
        !tab->addColumn(Column("type_name", 1, Column::String)))
      return false;

    if (list.push_back(tab) != 0)
      return false;
  }

  {
    ConfigParamsTable* configParamsTable = new ConfigParamsTable;
    if (!configParamsTable)
      return false;
    if (!configParamsTable->init())
      return false;

    Table* tab = new Table("config_params", Table::InvalidTableId,
                           configParamsTable);
    if (!tab)
      return false;
    if (!tab->addColumn(Column("param_number", 0, Column::Number)) ||
        !tab->addColumn(Column("param_name", 1, Column::String)))
      return false;

    if (list.push_back(tab) != 0)
      return false;
  }

  {
    NdbkernelStateDescTable* dbtcApiConnectStateTable =
        new NdbkernelStateDescTable(g_dbtc_apiconnect_state_desc);
    if (!dbtcApiConnectStateTable)
      return false;

    Table* tab = new Table("dbtc_apiconnect_state", Table::InvalidTableId,
                           dbtcApiConnectStateTable);
    if (!tab)
      return false;
    if (!tab->addColumn(Column("state_int_value", 0, Column::Number)) ||
        !tab->addColumn(Column("state_name", 1, Column::String)) ||
        !tab->addColumn(Column("state_friendly_name", 2, Column::String)) ||
        !tab->addColumn(Column("state_description", 3, Column::String)))
      return false;

    if (list.push_back(tab) != 0)
      return false;
  }

  {
    NdbkernelStateDescTable* dblqhTcConnectStateTable =
        new NdbkernelStateDescTable(g_dblqh_tcconnect_state_desc);
    if (!dblqhTcConnectStateTable)
      return false;

    Table* tab = new Table("dblqh_tcconnect_state", Table::InvalidTableId,
                           dblqhTcConnectStateTable);
    if (!tab)
      return false;
    if (!tab->addColumn(Column("state_int_value", 0, Column::Number)) ||
        !tab->addColumn(Column("state_name", 1, Column::String)) ||
        !tab->addColumn(Column("state_friendly_name", 2, Column::String)) ||
        !tab->addColumn(Column("state_description", 3, Column::String)))
      return false;

    if (list.push_back(tab) != 0)
      return false;
  }
  return true;
}

/*
  Delete the virtual part of the tables in list
*/
void NdbInfo::delete_virtual_tables(Vector<Table*>& list)
{
  for (unsigned i = 0; i < list.size(); i++)
  {
    Table* tab = list[i];
    delete tab->m_virt;
    delete tab;
  }
  list.clear();
}
