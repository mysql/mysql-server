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

#include <NDBT_ReturnCodes.h>
#include "consumer_restore.hpp"
#include <my_sys.h>
#include <NdbSleep.h>

extern my_bool opt_core;

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static void callback(int, NdbTransaction*, void*);
static Uint32 get_part_id(const NdbDictionary::Table *table,
                          Uint32 hash_value);

extern const char * g_connect_string;
extern BaseString g_options;

bool
BackupRestore::init()
{
  release();

  if (!m_restore && !m_restore_meta && !m_restore_epoch)
    return true;

  m_cluster_connection = new Ndb_cluster_connection(g_connect_string);
  m_cluster_connection->set_name(g_options.c_str());
  if(m_cluster_connection->connect(12, 5, 1) != 0)
  {
    return false;
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
  if (!table.have_auto_inc())
    return ret;

  Uint64 max_val= table.get_max_auto_val();
  do
  {
    Uint64 auto_val = ~(Uint64)0;
    int r= m_ndb->readAutoIncrementValue(get_table(table.m_dictTable), auto_val);
    if (r == -1 && m_ndb->getNdbError().status == NdbError::TemporaryError)
    {
      NdbSleep_MilliSleep(50);
      continue; // retry
    }
    else if (r == -1 && m_ndb->getNdbError().code != 626)
    {
      ret= false;
    }
    else if ((r == -1 && m_ndb->getNdbError().code == 626) ||
             max_val+1 > auto_val || auto_val == ~(Uint64)0)
    {
      r= m_ndb->setAutoIncrementValue(get_table(table.m_dictTable),
                                      max_val+1, false);
      if (r == -1 &&
            m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(50);
        continue; // retry
      }
      ret = (r == 0);
    }
    return (ret);
  } while (1);
}


#ifdef NOT_USED
static bool default_nodegroups(NdbDictionary::Table *table)
{
  Uint16 *node_groups = (Uint16*)table->getFragmentData();
  Uint32 no_parts = table->getFragmentDataLen() >> 1;
  Uint32 i;

  if (node_groups[0] != 0)
    return false; 
  for (i = 1; i < no_parts; i++) 
  {
    if (node_groups[i] != UNDEF_NODEGROUP)
      return false;
  }
  return true;
}
#endif


static Uint32 get_no_fragments(Uint64 max_rows, Uint32 no_nodes)
{
  Uint32 i = 0;
  Uint32 acc_row_size = 27;
  Uint32 acc_fragment_size = 512*1024*1024;
  Uint32 no_parts= (max_rows*acc_row_size)/acc_fragment_size + 1;
  Uint32 reported_parts = no_nodes; 
  while (reported_parts < no_parts && ++i < 4 &&
         (reported_parts + no_parts) < MAX_NDB_PARTITIONS)
    reported_parts+= no_nodes;
  if (reported_parts < no_parts)
  {
    err << "Table will be restored but will not be able to handle the maximum";
    err << " amount of rows as requested" << endl;
  }
  return reported_parts;
}


static void set_default_nodegroups(NdbDictionary::Table *table)
{
  Uint32 no_parts = table->getFragmentCount();
  Uint16 node_group[MAX_NDB_PARTITIONS];
  Uint32 i;

  node_group[0] = 0;
  for (i = 1; i < no_parts; i++)
  {
    node_group[i] = UNDEF_NODEGROUP;
  }
  table->setFragmentData((const void*)node_group, 2 * no_parts);
}

Uint32 BackupRestore::map_ng(Uint32 ng)
{
  NODE_GROUP_MAP *ng_map = m_nodegroup_map;

  if (ng == UNDEF_NODEGROUP ||
      ng_map[ng].map_array[0] == UNDEF_NODEGROUP)
  {
    return ng;
  }
  else
  {
    Uint32 new_ng;
    Uint32 curr_inx = ng_map[ng].curr_index;
    Uint32 new_curr_inx = curr_inx + 1;

    assert(ng < MAX_NDB_PARTITIONS);
    assert(curr_inx < MAX_MAPS_PER_NODE_GROUP);
    assert(new_curr_inx < MAX_MAPS_PER_NODE_GROUP);

    if (new_curr_inx >= MAX_MAPS_PER_NODE_GROUP)
      new_curr_inx = 0;
    else if (ng_map[ng].map_array[new_curr_inx] == UNDEF_NODEGROUP)
      new_curr_inx = 0;
    new_ng = ng_map[ng].map_array[curr_inx];
    ng_map[ng].curr_index = new_curr_inx;
    return new_ng;
  }
}


bool BackupRestore::map_nodegroups(Uint16 *ng_array, Uint32 no_parts)
{
  Uint32 i;
  bool mapped = FALSE;
  DBUG_ENTER("map_nodegroups");

  assert(no_parts < MAX_NDB_PARTITIONS);
  for (i = 0; i < no_parts; i++)
  {
    Uint32 ng;
    ng = map_ng((Uint32)ng_array[i]);
    if (ng != ng_array[i])
      mapped = TRUE;
    ng_array[i] = ng;
  }
  DBUG_RETURN(mapped);
}


static void copy_byte(const char **data, char **new_data, uint *len)
{
  **new_data = **data;
  (*data)++;
  (*new_data)++;
  (*len)++;
}


bool BackupRestore::search_replace(char *search_str, char **new_data,
                                   const char **data, const char *end_data,
                                   uint *new_data_len)
{
  uint search_str_len = strlen(search_str);
  uint inx = 0;
  bool in_delimiters = FALSE;
  bool escape_char = FALSE;
  char start_delimiter = 0;
  DBUG_ENTER("search_replace");

  do
  {
    char c = **data;
    copy_byte(data, new_data, new_data_len);
    if (escape_char)
    {
      escape_char = FALSE;
    }
    else if (in_delimiters)
    {
      if (c == start_delimiter)
        in_delimiters = FALSE;
    }
    else if (c == '\'' || c == '\"')
    {
      in_delimiters = TRUE;
      start_delimiter = c;
    }
    else if (c == '\\')
    {
      escape_char = TRUE;
    }
    else if (c == search_str[inx])
    {
      inx++;
      if (inx == search_str_len)
      {
        bool found = FALSE;
        uint number = 0;
        while (*data != end_data)
        {
          if (isdigit(**data))
          {
            found = TRUE;
            number = (10 * number) + (**data);
            if (number > MAX_NDB_NODES)
              break;
          }
          else if (found)
          {
            /*
               After long and tedious preparations we have actually found
               a node group identifier to convert. We'll use the mapping
               table created for node groups and then insert the new number
               instead of the old number.
            */
            uint temp = map_ng(number);
            int no_digits = 0;
            char digits[10];
            while (temp != 0)
            {
              digits[no_digits] = temp % 10;
              no_digits++;
              temp/=10;
            }
            for (no_digits--; no_digits >= 0; no_digits--)
            {
              **new_data = digits[no_digits];
              *new_data_len+=1;
            }
            DBUG_RETURN(FALSE); 
          }
          else
            break;
          (*data)++;
        }
        DBUG_RETURN(TRUE);
      }
    }
    else
      inx = 0;
  } while (*data < end_data);
  DBUG_RETURN(FALSE);
}

bool BackupRestore::map_in_frm(char *new_data, const char *data,
                                       uint data_len, uint *new_data_len)
{
  const char *end_data= data + data_len;
  const char *end_part_data;
  const char *part_data;
  char *extra_ptr;
  uint start_key_definition_len = uint2korr(data + 6);
  uint key_definition_len = uint4korr(data + 47);
  uint part_info_len;
  DBUG_ENTER("map_in_frm");

  if (data_len < 4096) goto error;
  extra_ptr = (char*)data + start_key_definition_len + key_definition_len;
  if ((int)data_len < ((extra_ptr - data) + 2)) goto error;
  extra_ptr = extra_ptr + 2 + uint2korr(extra_ptr);
  if ((int)data_len < ((extra_ptr - data) + 2)) goto error;
  extra_ptr = extra_ptr + 2 + uint2korr(extra_ptr);
  if ((int)data_len < ((extra_ptr - data) + 4)) goto error;
  part_info_len = uint4korr(extra_ptr);
  part_data = extra_ptr + 4;
  if ((int)data_len < ((part_data + part_info_len) - data)) goto error;
 
  do
  {
    copy_byte(&data, &new_data, new_data_len);
  } while (data < part_data);
  end_part_data = part_data + part_info_len;
  do
  {
    if (search_replace((char*)" NODEGROUP = ", &new_data, &data,
                       end_part_data, new_data_len))
      goto error;
  } while (data != end_part_data);
  do
  {
    copy_byte(&data, &new_data, new_data_len);
  } while (data < end_data);
  DBUG_RETURN(FALSE);
error:
  DBUG_RETURN(TRUE);
}


bool BackupRestore::translate_frm(NdbDictionary::Table *table)
{
  uchar *pack_data, *data, *new_pack_data;
  char *new_data;
  uint new_data_len;
  size_t data_len, new_pack_len;
  uint no_parts, extra_growth;
  DBUG_ENTER("translate_frm");

  pack_data = (uchar*) table->getFrmData();
  no_parts = table->getFragmentCount();
  /*
    Add max 4 characters per partition to handle worst case
    of mapping from single digit to 5-digit number.
    Fairly future-proof, ok up to 99999 node groups.
  */
  extra_growth = no_parts * 4;
  if (unpackfrm(&data, &data_len, pack_data))
  {
    DBUG_RETURN(TRUE);
  }
  if ((new_data = (char*) my_malloc(data_len + extra_growth, MYF(0))))
  {
    DBUG_RETURN(TRUE);
  }
  if (map_in_frm(new_data, (const char*)data, data_len, &new_data_len))
  {
    my_free(new_data);
    DBUG_RETURN(TRUE);
  }
  if (packfrm((uchar*) new_data, new_data_len,
              &new_pack_data, &new_pack_len))
  {
    my_free(new_data);
    DBUG_RETURN(TRUE);
  }
  table->setFrm(new_pack_data, new_pack_len);
  DBUG_RETURN(FALSE);
}

#include <signaldata/DictTabInfo.hpp>

bool
BackupRestore::object(Uint32 type, const void * ptr)
{
  if (!m_restore_meta)
    return true;
  
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  switch(type){
  case DictTabInfo::Tablespace:
  {
    NdbDictionary::Tablespace old(*(NdbDictionary::Tablespace*)ptr);

    Uint32 id = old.getObjectId();

    if (!m_no_restore_disk)
    {
      NdbDictionary::LogfileGroup * lg = m_logfilegroups[old.getDefaultLogfileGroupId()];
      old.setDefaultLogfileGroup(* lg);
      info << "Creating tablespace: " << old.getName() << "..." << flush;
      int ret = dict->createTablespace(old);
      if (ret)
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create tablespace failed: " << old.getName() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    
    NdbDictionary::Tablespace curr = dict->getTablespace(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::Tablespace* currptr = new NdbDictionary::Tablespace(curr);
      NdbDictionary::Tablespace * null = 0;
      m_tablespaces.set(currptr, id, null);
      debug << "Retreived tablespace: " << currptr->getName() 
	    << " oldid: " << id << " newid: " << currptr->getObjectId() 
	    << " " << (void*)currptr << endl;
      return true;
    }
    
    err << "Failed to retrieve tablespace \"" << old.getName() << "\": "
	<< errobj << endl;
    
    return false;
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    NdbDictionary::LogfileGroup old(*(NdbDictionary::LogfileGroup*)ptr);
    
    Uint32 id = old.getObjectId();
    
    if (!m_no_restore_disk)
    {
      info << "Creating logfile group: " << old.getName() << "..." << flush;
      int ret = dict->createLogfileGroup(old);
      if (ret)
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create logfile group failed: " << old.getName() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    
    NdbDictionary::LogfileGroup curr = dict->getLogfileGroup(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::LogfileGroup* currptr = 
	new NdbDictionary::LogfileGroup(curr);
      NdbDictionary::LogfileGroup * null = 0;
      m_logfilegroups.set(currptr, id, null);
      debug << "Retreived logfile group: " << currptr->getName() 
	    << " oldid: " << id << " newid: " << currptr->getObjectId() 
	    << " " << (void*)currptr << endl;
      return true;
    }
    
    err << "Failed to retrieve logfile group \"" << old.getName() << "\": "
	<< errobj << endl;
    
    return false;
    break;
  }
  case DictTabInfo::Datafile:
  {
    if (!m_no_restore_disk)
    {
      NdbDictionary::Datafile old(*(NdbDictionary::Datafile*)ptr);
      NdbDictionary::ObjectId objid;
      old.getTablespaceId(&objid);
      NdbDictionary::Tablespace * ts = m_tablespaces[objid.getObjectId()];
      debug << "Connecting datafile " << old.getPath() 
	    << " to tablespace: oldid: " << objid.getObjectId()
	    << " newid: " << ts->getObjectId() << endl;
      old.setTablespace(* ts);
      info << "Creating datafile \"" << old.getPath() << "\"..." << flush;
      if (dict->createDatafile(old))
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create datafile failed: " << old.getPath() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    return true;
    break;
  }
  case DictTabInfo::Undofile:
  {
    if (!m_no_restore_disk)
    {
      NdbDictionary::Undofile old(*(NdbDictionary::Undofile*)ptr);
      NdbDictionary::ObjectId objid;
      old.getLogfileGroupId(&objid);
      NdbDictionary::LogfileGroup * lg = m_logfilegroups[objid.getObjectId()];
      debug << "Connecting undofile " << old.getPath() 
	    << " to logfile group: oldid: " << objid.getObjectId()
	    << " newid: " << lg->getObjectId() 
	    << " " << (void*)lg << endl;
      old.setLogfileGroup(* lg);
      info << "Creating undofile \"" << old.getPath() << "\"..." << flush;
      if (dict->createUndofile(old))
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create undofile failed: " << old.getPath() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    return true;
    break;
  }
  }
  return true;
}

bool
BackupRestore::has_temp_error(){
  return m_temp_error;
}

bool
BackupRestore::update_apply_status(const RestoreMetaData &metaData)
{
  if (!m_restore_epoch)
    return true;

  bool result= false;
  unsigned apply_table_format= 0;

  m_ndb->setDatabaseName(NDB_REP_DB);
  m_ndb->setSchemaName("def");

  NdbDictionary::Dictionary *dict= m_ndb->getDictionary();
  const NdbDictionary::Table *ndbtab= dict->getTable(Ndb_apply_table);
  if (!ndbtab)
  {
    err << Ndb_apply_table << ": "
	<< dict->getNdbError() << endl;
    return false;
  }
  if
    (ndbtab->getColumn(0)->getType() == NdbDictionary::Column::Unsigned &&
     ndbtab->getColumn(1)->getType() == NdbDictionary::Column::Bigunsigned)
  {
    if (ndbtab->getNoOfColumns() == 2)
    {
      apply_table_format= 1;
    }
    else if
      (ndbtab->getColumn(2)->getType() == NdbDictionary::Column::Varchar &&
       ndbtab->getColumn(3)->getType() == NdbDictionary::Column::Bigunsigned &&
       ndbtab->getColumn(4)->getType() == NdbDictionary::Column::Bigunsigned)
    {
      apply_table_format= 2;
    }
  }
  if (apply_table_format == 0)
  {
    err << Ndb_apply_table << " has wrong format\n";
    return false;
  }

  Uint32 server_id= 0;
  Uint64 epoch= metaData.getStopGCP();
  Uint64 zero= 0;
  char empty_string[1];
  empty_string[0]= 0;
  NdbTransaction * trans= m_ndb->startTransaction();
  if (!trans)
  {
    err << Ndb_apply_table << ": "
	<< m_ndb->getNdbError() << endl;
    return false;
  }
  NdbOperation * op= trans->getNdbOperation(ndbtab);
  if (!op)
  {
    err << Ndb_apply_table << ": "
	<< trans->getNdbError() << endl;
    goto err;
  }
  if (op->writeTuple() ||
      op->equal(0u, (const char *)&server_id, sizeof(server_id)) ||
      op->setValue(1u, (const char *)&epoch, sizeof(epoch)))
  {
    err << Ndb_apply_table << ": "
	<< op->getNdbError() << endl;
    goto err;
  }
  if ((apply_table_format == 2) &&
      (op->setValue(2u, (const char *)&empty_string, 1) ||
       op->setValue(3u, (const char *)&zero, sizeof(zero)) ||
       op->setValue(4u, (const char *)&zero, sizeof(zero))))
  {
    err << Ndb_apply_table << ": "
	<< op->getNdbError() << endl;
    goto err;
  }
  if (trans->execute(NdbTransaction::Commit))
  {
    err << Ndb_apply_table << ": "
	<< trans->getNdbError() << endl;
    goto err;
  }
  result= true;
err:
  m_ndb->closeTransaction(trans);
  return result;
}

bool
BackupRestore::table_equal(const TableS &tableS)
{
  if (!m_restore)
    return true;

  const char *tablename = tableS.getTableName();

  if(tableS.m_dictTable == NULL){
    ndbout<<"Table %s has no m_dictTable " << tablename << endl;
    return false;
  }
  /**
   * Ignore blob tables
   */
  if(match_blob(tablename) >= 0)
    return true;

  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* tableS.m_dictTable);
  if ((int) tmptab.m_indexType != (int) NdbDictionary::Index::Undefined){
    return true;
  }

  BaseString tmp(tablename);
  Vector<BaseString> split;
  if(tmp.split(split, "/") != 3){
    err << "Invalid table name format " << tablename << endl;
    return false;
  }

  m_ndb->setDatabaseName(split[0].c_str());
  m_ndb->setSchemaName(split[1].c_str());

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();  
  const NdbDictionary::Table* tab = dict->getTable(split[2].c_str());
  if(tab == 0){
    err << "Unable to find table: " << split[2].c_str() << endl;
    return false;
  }

  if(tab->getNoOfColumns() != tableS.m_dictTable->getNoOfColumns())
  {
    ndbout_c("m_columns.size %d != %d",tab->getNoOfColumns(),
                       tableS.m_dictTable->getNoOfColumns());
    return false;
  }

 for(int i = 0; i<tab->getNoOfColumns(); i++)
  {
    if(!tab->getColumn(i)->equal(*(tableS.m_dictTable->getColumn(i))))
    {
      ndbout_c("m_columns %s != %s",tab->getColumn(i)->getName(),
                tableS.m_dictTable->getColumn(i)->getName());
      return false;
    }
  }

  return true;
}

bool
BackupRestore::createSystable(const TableS & tables){
  if (!m_restore && !m_restore_meta && !m_restore_epoch)
    return true;
  const char *tablename = tables.getTableName();

  if( strcmp(tablename, NDB_REP_DB "/def/" NDB_APPLY_TABLE) != 0 &&
      strcmp(tablename, NDB_REP_DB "/def/" NDB_SCHEMA_TABLE) != 0 )
  {
    return true;
  }

  BaseString tmp(tablename);
  Vector<BaseString> split;
  if(tmp.split(split, "/") != 3){
    err << "Invalid table name format " << tablename << endl;
    return false;
  }

  m_ndb->setDatabaseName(split[0].c_str());
  m_ndb->setSchemaName(split[1].c_str());

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if( dict->getTable(split[2].c_str()) != NULL ){
    return true;
  }
  return table(tables);
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
  if ((int) tmptab.m_indexType != (int) NdbDictionary::Index::Undefined){
    m_indexes.push_back(table.m_dictTable);
    return true;
  }
  
  BaseString tmp(name);
  Vector<BaseString> split;
  if(tmp.split(split, "/") != 3){
    err << "Invalid table name format `" << name << "`" << endl;
    return false;
  }

  m_ndb->setDatabaseName(split[0].c_str());
  m_ndb->setSchemaName(split[1].c_str());
  
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if(m_restore_meta)
  {
    NdbDictionary::Table copy(*table.m_dictTable);

    copy.setName(split[2].c_str());
    Uint32 id;
    if (copy.getTablespace(&id))
    {
      debug << "Connecting " << name << " to tablespace oldid: " << id << flush;
      NdbDictionary::Tablespace* ts = m_tablespaces[id];
      debug << " newid: " << ts->getObjectId() << endl;
      copy.setTablespace(* ts);
    }
    
    if (copy.getDefaultNoPartitionsFlag())
    {
      /*
        Table was defined with default number of partitions. We can restore
        it with whatever is the default in this cluster.
        We use the max_rows parameter in calculating the default number.
      */
      Uint32 no_nodes = m_cluster_connection->no_db_nodes();
      copy.setFragmentCount(get_no_fragments(copy.getMaxRows(),
                            no_nodes));
      set_default_nodegroups(&copy);
    }
    else
    {
      /*
        Table was defined with specific number of partitions. It should be
        restored with the same number of partitions. It will either be
        restored in the same node groups as when backup was taken or by
        using a node group map supplied to the ndb_restore program.
      */
      Uint16 *ng_array = (Uint16*)copy.getFragmentData();
      Uint16 no_parts = copy.getFragmentCount();
      if (map_nodegroups(ng_array, no_parts))
      {
        if (translate_frm(&copy))
        {
          err << "Create table " << table.getTableName() << " failed: ";
          err << "Translate frm error" << endl;
          return false;
        }
      }
      copy.setFragmentData((const void *)ng_array, no_parts << 1);
    }

    /**
     * Force of varpart was introduced in 5.1.18, telco 6.1.7 and 6.2.1
     * Since default from mysqld is to add force of varpart (disable with
     * ROW_FORMAT=FIXED) we force varpart onto tables when they are restored
     * from backups taken with older versions. This will be wrong if
     * ROW_FORMAT=FIXED was used on original table, however the likelyhood of
     * this is low, since ROW_FORMAT= was a NOOP in older versions.
     */

    if (table.getBackupVersion() < MAKE_VERSION(5,1,18))
      copy.setForceVarPart(true);
    else if (getMajor(table.getBackupVersion()) == 6 &&
             (table.getBackupVersion() < MAKE_VERSION(6,1,7) ||
              table.getBackupVersion() == MAKE_VERSION(6,2,0)))
      copy.setForceVarPart(true);

    /*
      update min and max rows to reflect the table, this to
      ensure that memory is allocated properly in the ndb kernel
    */
    copy.setMinRows(table.getNoOfRecords());
    if (table.getNoOfRecords() > copy.getMaxRows())
    {
      copy.setMaxRows(table.getNoOfRecords());
    }
    
    NdbTableImpl &tableImpl = NdbTableImpl::getImpl(copy);
    if (table.getBackupVersion() < MAKE_VERSION(5,1,0) && !m_no_upgrade){
      for(int i= 0; i < copy.getNoOfColumns(); i++)
      {
        NdbDictionary::Column::Type t = copy.getColumn(i)->getType();

        if (t == NdbDictionary::Column::Varchar ||
          t == NdbDictionary::Column::Varbinary)
          tableImpl.getColumn(i)->setArrayType(NdbDictionary::Column::ArrayTypeShortVar);
        if (t == NdbDictionary::Column::Longvarchar ||
          t == NdbDictionary::Column::Longvarbinary)
          tableImpl.getColumn(i)->setArrayType(NdbDictionary::Column::ArrayTypeMediumVar);
      }
    }

    if (dict->createTable(copy) == -1) 
    {
      err << "Create table `" << table.getTableName() << "` failed: "
          << dict->getNdbError() << endl;
      if (dict->getNdbError().code == 771)
      {
        /*
          The user on the cluster where the backup was created had specified
          specific node groups for partitions. Some of these node groups
          didn't exist on this cluster. We will warn the user of this and
          inform him of his option.
        */
        err << "The node groups defined in the table didn't exist in this";
        err << " cluster." << endl << "There is an option to use the";
        err << " the parameter ndb-nodegroup-map to define a mapping from";
        err << endl << "the old nodegroups to new nodegroups" << endl; 
      }
      return false;
    }
    info << "Successfully restored table `"
         << table.getTableName() << "`" << endl;
  }  
  
  const NdbDictionary::Table* tab = dict->getTable(split[2].c_str());
  if(tab == 0){
    err << "Unable to find table: `" << split[2].c_str() << "`" << endl;
    return false;
  }
  if(m_restore_meta)
  {
    if (tab->getFrmData())
    {
      // a MySQL Server table is restored, thus an event should be created
      BaseString event_name("REPL$");
      event_name.append(split[0].c_str());
      event_name.append("/");
      event_name.append(split[2].c_str());

      NdbDictionary::Event my_event(event_name.c_str());
      my_event.setTable(*tab);
      my_event.addTableEvent(NdbDictionary::Event::TE_ALL);

      // add all columns to the event
      bool has_blobs = false;
      for(int a= 0; a < tab->getNoOfColumns(); a++)
      {
	my_event.addEventColumn(a);
        NdbDictionary::Column::Type t = tab->getColumn(a)->getType();
        if (t == NdbDictionary::Column::Blob ||
            t == NdbDictionary::Column::Text)
          has_blobs = true;
      }
      if (has_blobs)
        my_event.mergeEvents(true);

      while ( dict->createEvent(my_event) ) // Add event to database
      {
	if (dict->getNdbError().classification == NdbError::SchemaObjectExists)
	{
	  info << "Event for table " << table.getTableName()
	       << " already exists, removing.\n";
	  if (!dict->dropEvent(my_event.getName()))
	    continue;
	}
	err << "Create table event for " << table.getTableName() << " failed: "
	    << dict->getNdbError() << endl;
	dict->dropTable(split[2].c_str());
	return false;
      }
      info << "Successfully restored table event " << event_name << endl ;
    }
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

    Vector<BaseString> split;
    {
      BaseString tmp(indtab.m_primaryTable.c_str());
      if (tmp.split(split, "/") != 3)
      {
        err << "Invalid table name format `" << indtab.m_primaryTable.c_str()
            << "`" << endl;
        return false;
      }
    }
    
    m_ndb->setDatabaseName(split[0].c_str());
    m_ndb->setSchemaName(split[1].c_str());
    
    const NdbDictionary::Table * prim = dict->getTable(split[2].c_str());
    if(prim == 0){
      err << "Unable to find base table `" << split[2].c_str() 
	  << "` for index `"
	  << indtab.getName() << "`" << endl;
      return false;
    }
    NdbTableImpl& base = NdbTableImpl::getImpl(*prim);
    NdbIndexImpl* idx;
    Vector<BaseString> split_idx;
    {
      BaseString tmp(indtab.getName());
      if (tmp.split(split_idx, "/") != 4)
      {
        err << "Invalid index name format `" << indtab.getName() << "`" << endl;
        return false;
      }
    }
    if(NdbDictInterface::create_index_obj_from_table(&idx, &indtab, &base))
    {
      err << "Failed to create index `" << split_idx[3]
	  << "` on " << split[2].c_str() << endl;
	return false;
    }
    idx->setName(split_idx[3].c_str());
    if(dict->createIndex(* idx) != 0)
    {
      delete idx;
      err << "Failed to create index `" << split_idx[3].c_str()
	  << "` on `" << split[2].c_str() << "`" << endl
	  << dict->getNdbError() << endl;

      return false;
    }
    delete idx;
    info << "Successfully created index `" << split_idx[3].c_str()
	 << "` on `" << split[2].c_str() << "`" << endl;
  }
  return true;
}

void BackupRestore::tuple(const TupleS & tup, Uint32 fragmentId)
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
  cb->fragId = fragmentId;
  cb->tup = tup; // must do copy!
  tuple_a(cb);

}

void BackupRestore::tuple_a(restore_callback_t *cb)
{
  Uint32 partition_id = cb->fragId;
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
      err << "Cannot start transaction" << endl;
      exitHandler();
    } // if
    
    const TupleS &tup = cb->tup;
    const NdbDictionary::Table * table = get_table(tup.getTable()->m_dictTable);

    NdbOperation * op = cb->connection->getNdbOperation(table);
    
    if (op == NULL) 
    {
      if (errorHandler(cb)) 
	continue;
      err << "Cannot get operation: " << cb->connection->getNdbError() << endl;
      exitHandler();
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (errorHandler(cb))
	continue;
      err << "Error defining op: " << cb->connection->getNdbError() << endl;
      exitHandler();
    } // if

    if (table->getFragmentType() == NdbDictionary::Object::UserDefined)
    {
      if (table->getDefaultNoPartitionsFlag())
      {
        /*
          This can only happen for HASH partitioning with
          user defined hash function where user hasn't
          specified the number of partitions and we
          have to calculate it. We use the hash value
          stored in the record to calculate the partition
          to use.
        */
        int i = tup.getNoOfAttributes() - 1;
	const AttributeData  *attr_data = tup.getData(i);
        Uint32 hash_value =  *attr_data->u_int32_value;
        op->setPartitionId(get_part_id(table, hash_value));
      }
      else
      {
        /*
          Either RANGE or LIST (with or without subparts)
          OR HASH partitioning with user defined hash
          function but with fixed set of partitions.
        */
        op->setPartitionId(partition_id);
      }
    }
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
	Uint32 length = 0;
       
        if (!attr_data->null)
        {
          const unsigned char * src = (const unsigned char *)dataPtr;
          switch(attr_desc->m_column->getType()){
          case NdbDictionary::Column::Varchar:
          case NdbDictionary::Column::Varbinary:
            length = src[0] + 1;
            break;
          case NdbDictionary::Column::Longvarchar:
          case NdbDictionary::Column::Longvarbinary:
            length = src[0] + (src[1] << 8) + 2;
            break;
          default:
            length = attr_data->size;
            break;
          }
        }
	if (j == 0 && tup.getTable()->have_auto_inc(i))
	  tup.getTable()->update_max_auto_val(dataPtr,size*arraySize);
	
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
		   size, arraySize, length);
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
      err << "Error defining op: " << cb->connection->getNdbError() << endl;
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
    err << "Success error: " << error << endl;
    return false;
    // ERROR!
    
  case NdbError::TemporaryError:
    err << "Temporary error: " << error << endl;
    m_temp_error = true;
    NdbSleep_MilliSleep(sleepTime);
    return true;
    // RETRY
    
  case NdbError::UnknownResult:
    err << "Unknown: " << error << endl;
    return false;
    // ERROR!
    
  default:
  case NdbError::PermanentError:
    //ERROR
    err << "Permanent: " << error << endl;
    return false;
  }
  err << "No error status" << endl;
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

#ifdef NOT_USED
static bool use_part_id(const NdbDictionary::Table *table)
{
  if (table->getDefaultNoPartitionsFlag() &&
      (table->getFragmentType() == NdbDictionary::Object::UserDefined))
    return false;
  else
    return true;
}
#endif

static Uint32 get_part_id(const NdbDictionary::Table *table,
                          Uint32 hash_value)
{
  Uint32 no_frags = table->getFragmentCount();
  
  if (table->getLinearFlag())
  {
    Uint32 part_id;
    Uint32 mask = 1;
    while (no_frags > mask) mask <<= 1;
    mask--;
    part_id = hash_value & mask;
    if (part_id >= no_frags)
      part_id = hash_value & (mask >> 1);
    return part_id;
  }
  else
    return (hash_value % no_frags);
}

void
BackupRestore::logEntry(const LogEntry & tup)
{
  if (!m_restore)
    return;

  NdbTransaction * trans = m_ndb->startTransaction();
  if (trans == NULL) 
  {
    // TODO: handle the error
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

  if (table->getFragmentType() == NdbDictionary::Object::UserDefined)
  {
    if (table->getDefaultNoPartitionsFlag())
    {
      const AttributeS * attr = tup[tup.size()-1];
      Uint32 hash_value = *(Uint32*)attr->Data.string_value;
      op->setPartitionId(get_part_id(table, hash_value));
    }
    else
      op->setPartitionId(tup.m_frag_id);
  }

  Bitmask<4096> keys;
  for (Uint32 i= 0; i < tup.size(); i++) 
  {
    const AttributeS * attr = tup[i];
    int size = attr->Desc->size;
    int arraySize = attr->Desc->arraySize;
    const char * dataPtr = attr->Data.string_value;
    
    if (tup.m_table->have_auto_inc(attr->Desc->attrId))
      tup.m_table->update_max_auto_val(dataPtr,size*arraySize);

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
      // TODO: handle the error
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
template class Vector<NdbDictionary::Tablespace*>;
template class Vector<NdbDictionary::LogfileGroup*>;
