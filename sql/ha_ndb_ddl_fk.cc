/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <algorithm>

#include "my_dbug.h"
#include "mysql/service_thd_alloc.h"
#include "sql/ha_ndbcluster.h"
#include "sql/key_spec.h"
#include "sql/mysqld.h"     // global_system_variables table_alias_charset ...
#include "sql/ndb_fk_util.h"
#include "sql/ndb_log.h"
#include "sql/ndb_table_guard.h"
#include "sql/ndb_tdc.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_table.h"
#include "template_utils.h"

#define ERR_RETURN(err)                  \
{                                        \
  const NdbError& tmp= err;              \
  DBUG_RETURN(ndb_to_mysql_error(&tmp)); \
}

// Typedefs for long names 
typedef NdbDictionary::Dictionary NDBDICT;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Index NDBINDEX;
typedef NdbDictionary::ForeignKey NDBFK;

/*
  Foreign key data where this table is child or parent or both.
  Like indexes, these are cached under each handler instance.
  Unlike indexes, no references to global dictionary are kept.
*/

struct Ndb_fk_item
{
  FOREIGN_KEY_INFO f_key_info;
  int update_action;    // NDBFK::FkAction
  int delete_action;
  bool is_child;
  bool is_parent;
};

struct Ndb_fk_data
{
  List<Ndb_fk_item> list;
  uint cnt_child;
  uint cnt_parent;
};

/*
  Create all the fks  for a table.

  The actual foreign keys are not passed in handler interface
  so gets them from thd->lex :-(
*/
static
const NDBINDEX*
find_matching_index(NDBDICT* dict,
                    const NDBTAB * tab,
                    const NDBCOL * columns[],
                    /* OUT */ bool & matches_primary_key)
{
  /**
   * First check if it matches primary key
   */
  {
    matches_primary_key= false;

    uint cnt_pk= 0, cnt_col= 0;
    for (unsigned i = 0; columns[i] != 0; i++)
    {
      cnt_col++;
      if (columns[i]->getPrimaryKey())
        cnt_pk++;
    }

    // check if all columns was part of full primary key
    if (cnt_col == (uint)tab->getNoOfPrimaryKeys() &&
        cnt_col == cnt_pk)
    {
      matches_primary_key= true;
      return 0;
    }
  }

  /**
   * check indexes...
   * first choice is unique index
   * second choice is ordered index...with as many columns as possible
   */
  const int noinvalidate= 0;
  uint best_matching_columns= 0;
  const NDBINDEX* best_matching_index= 0;

  NDBDICT::List index_list;
  dict->listIndexes(index_list, *tab);
  for (unsigned i = 0; i < index_list.count; i++)
  {
    const char * index_name= index_list.elements[i].name;
    const NDBINDEX* index= dict->getIndexGlobal(index_name, *tab);
    if (index->getType() == NDBINDEX::UniqueHashIndex)
    {
      uint cnt= 0, j;
      for (j = 0; columns[j] != 0; j++)
      {
        /*
         * Search for matching columns in any order
         * since order does not matter for unique index
         */
        bool found= false;
        for (unsigned c = 0; c < index->getNoOfColumns(); c++)
        {
          if (!strcmp(columns[j]->getName(), index->getColumn(c)->getName()))
          {
            found= true;
            break;
          }
        }
        if (found)
          cnt++;
        else
          break;
      }
      if (cnt == index->getNoOfColumns() && columns[j] == 0)
      {
        /**
         * Full match...return this index, no need to look further
         */
        if (best_matching_index)
        {
          // release ref to previous best candidate
          dict->removeIndexGlobal(* best_matching_index, noinvalidate);
        }
        return index; // NOTE: also returns reference
      }

      /**
       * Not full match...i.e not usable
       */
      dict->removeIndexGlobal(* index, noinvalidate);
      continue;
    }
    else if (index->getType() == NDBINDEX::OrderedIndex)
    {
      uint cnt= 0;
      for (; columns[cnt] != 0; cnt++)
      {
        const NDBCOL * ndbcol= index->getColumn(cnt);
        if (ndbcol == 0)
          break;

        if (strcmp(columns[cnt]->getName(), ndbcol->getName()) != 0)
          break;
      }

      if (cnt > best_matching_columns)
      {
        /**
         * better match...
         */
        if (best_matching_index)
        {
          dict->removeIndexGlobal(* best_matching_index, noinvalidate);
        }
        best_matching_index= index;
        best_matching_columns= cnt;
      }
      else
      {
        dict->removeIndexGlobal(* index, noinvalidate);
      }
    }
    else
    {
      // what ?? unknown index type
      assert(false);
      dict->removeIndexGlobal(* index, noinvalidate);
      continue;
    }
  }

  return best_matching_index; // NOTE: also returns reference
}
 
static
void
setDbName(Ndb* ndb, const char * name)
{
  if (name && strlen(name) != 0)
  {
    ndb->setDatabaseName(name);
  }
}


template <size_t buf_size>
const char *
lex2str(const LEX_CSTRING& str, char (&buf)[buf_size])
{
  snprintf(buf, buf_size, "%.*s", (int)str.length, str.str);
  return buf;
}


static void
ndb_fk_casedn(char *name)
{
  DBUG_ASSERT(name != 0);
  uint length = (uint)strlen(name);
  DBUG_ASSERT(files_charset_info != 0 &&
              files_charset_info->casedn_multiply == 1);
  files_charset_info->cset->casedn(files_charset_info,
                                   name, length, name, length);
}

static int
ndb_fk_casecmp(const char* name1, const char* name2)
{
  if (!lower_case_table_names)
  {
    return strcmp(name1, name2);
  }
  char tmp1[FN_LEN + 1];
  char tmp2[FN_LEN + 1];
  strcpy(tmp1, name1);
  strcpy(tmp2, name2);
  ndb_fk_casedn(tmp1);
  ndb_fk_casedn(tmp2);
  return strcmp(tmp1, tmp2);
}


extern bool ndb_show_foreign_key_mock_tables(THD* thd);

class Fk_util
{
  THD* m_thd;

  void
  info(const char* fmt, ...) const
    MY_ATTRIBUTE((format(printf, 2, 3)));

  void
  warn(const char* fmt, ...) const
    MY_ATTRIBUTE((format(printf, 2, 3)));

  void
  error(const NdbDictionary::Dictionary* dict, const char* fmt, ...) const
    MY_ATTRIBUTE((format(printf, 3, 4)));

  void
  remove_index_global(NdbDictionary::Dictionary* dict, const NdbDictionary::Index* index) const
  {
    if (!index)
      return;

    dict->removeIndexGlobal(*index, 0);
  }


  bool
  copy_fk_to_new_parent(NdbDictionary::Dictionary* dict, NdbDictionary::ForeignKey& fk,
                   const char* new_parent_name, const char* column_names[]) const
  {
    DBUG_ENTER("copy_fk_to_new_parent");
    DBUG_PRINT("info", ("new_parent_name: %s", new_parent_name));

    // Load up the new parent table
    Ndb_table_guard new_parent_tab(dict, new_parent_name);
    if (!new_parent_tab.get_table())
    {
      error(dict, "Failed to load potentially new parent '%s'", new_parent_name);
      DBUG_RETURN(false);
    }

    // Build new parent column list from parent column names
    const NdbDictionary::Column* columns[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned num_columns = 0;
      for (unsigned i = 0; column_names[i] != 0; i++)
      {
        DBUG_PRINT("info", ("column: %s", column_names[i]));
        const NdbDictionary::Column* col =
            new_parent_tab.get_table()->getColumn(column_names[i]);
        if (!col)
        {
          // Parent table didn't have any column with the given name, can happen
          warn("Could not resolve '%s' as fk parent for '%s' since it didn't have "
               "all the referenced columns", new_parent_name, fk.getChildTable());
          DBUG_RETURN(false);
        }
        columns[num_columns++]= col;
      }
      columns[num_columns]= 0;
    }

    NdbDictionary::ForeignKey new_fk(fk);

    // Create name for the new fk by splitting the fk's name and replacing
    // the <parent id> part in format "<parent_id>/<child_id>/<name>"
    {
      char name[FN_REFLEN+1];
      unsigned parent_id, child_id;
      if (sscanf(fk.getName(), "%u/%u/%s",
             &parent_id, &child_id, name) != 3)
      {
        warn("Skip, failed to parse name of fk: %s", fk.getName());
        DBUG_RETURN(false);
      }

      char fk_name[FN_REFLEN+1];
      snprintf(fk_name, sizeof(fk_name), "%s",
                  name);
      DBUG_PRINT("info", ("Setting new fk name: %s", fk_name));
      new_fk.setName(fk_name);
    }

    // Find matching index
    bool parent_primary_key= false;
    const NdbDictionary::Index* parent_index= find_matching_index(dict,
                                                                  new_parent_tab.get_table(),
                                                                  columns,
                                                                  parent_primary_key);
    DBUG_PRINT("info", ("parent_primary_key: %d", parent_primary_key));

    // Check if either pk or index matched
    if (!parent_primary_key && parent_index == 0)
    {
      warn("Could not resolve '%s' as fk parent for '%s' since no matching index "
           "could be found", new_parent_name, fk.getChildTable());
      DBUG_RETURN(false);
    }

    if (parent_index != 0)
    {
      DBUG_PRINT("info", ("Setting parent with index %s", parent_index->getName()));
      new_fk.setParent(*new_parent_tab.get_table(), parent_index, columns);
    }
    else
    {
      DBUG_PRINT("info", ("Setting parent without index"));
      new_fk.setParent(*new_parent_tab.get_table(), 0, columns);
    }

    // Old fk is dropped by cascading when the mock table is dropped

    // Create new fk referencing the new table
    DBUG_PRINT("info", ("Create new fk: %s", new_fk.getName()));
    int flags = 0;
    if (thd_test_options(m_thd, OPTION_NO_FOREIGN_KEY_CHECKS))
    {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }
    NdbDictionary::ObjectId objid;
    if (dict->createForeignKey(new_fk, &objid, flags) != 0)
    {
      error(dict, "Failed to create foreign key '%s'", new_fk.getName());
      remove_index_global(dict, parent_index);
      DBUG_RETURN(false);
    }

    remove_index_global(dict, parent_index);
    DBUG_RETURN(true);
  }


  void
  resolve_mock(NdbDictionary::Dictionary* dict,
               const char* new_parent_name, const char* mock_name) const
  {
    DBUG_ENTER("resolve_mock");
    DBUG_PRINT("enter", ("mock_name '%s'", mock_name));
    DBUG_ASSERT(is_mock_name(mock_name));

    // Load up the mock table
    Ndb_table_guard mock_tab(dict, mock_name);
    if (!mock_tab.get_table())
    {
      error(dict, "Failed to load the listed mock table '%s'", mock_name);
      DBUG_ASSERT(false);
      DBUG_VOID_RETURN;
    }

    // List dependent objects of mock table
    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *mock_tab.get_table()) != 0)
    {
      error(dict, "Failed to list dependent objects for mock table '%s'", mock_name);
      DBUG_VOID_RETURN;
    }

    for (unsigned i = 0; i < list.count; i++)
    {
      const NdbDictionary::Dictionary::List::Element& element = list.elements[i];
      if (element.type != NdbDictionary::Object::ForeignKey)
        continue;

      DBUG_PRINT("info", ("fk: %s", element.name));

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0)
      {
        error(dict, "Could not find the listed fk '%s'", element.name);
        continue;
      }

      // Build column name list for parent
      const char* col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      {
        unsigned num_columns = 0;
        for (unsigned j = 0; j < fk.getParentColumnCount(); j++)
        {
          const NdbDictionary::Column* col =
              mock_tab.get_table()->getColumn(fk.getParentColumnNo(j));
          if (!col)
          {
            error(NULL, "Could not find column %d in mock table '%s'",
                  fk.getParentColumnNo(j), mock_name);
            continue;
          }
          col_names[num_columns++]= col->getName();
        }
        col_names[num_columns]= 0;

        if (num_columns != fk.getParentColumnCount())
        {
          error(NULL, "Could not find all columns referenced by fk in mock table '%s'",
                mock_name);
          continue;
        }
      }

      if (!copy_fk_to_new_parent(dict, fk, new_parent_name, col_names))
        continue;

      // New fk has been created between child and new parent, drop the mock
      // table and it's related fk
      const int drop_flags= NDBDICT::DropTableCascadeConstraints;
      if (dict->dropTableGlobal(*mock_tab.get_table(), drop_flags) != 0)
      {
        error(dict, "Failed to drop mock table '%s'", mock_name);
        continue;
      }
      info("Dropped mock table '%s' - resolved by '%s'", mock_name, new_parent_name);
    }
    DBUG_VOID_RETURN;
  }


  bool
  create_mock_tables_and_drop(Ndb* ndb, NdbDictionary::Dictionary* dict,
                              const NdbDictionary::Table* table)
  {
    DBUG_ENTER("create_mock_tables_and_drop");
    DBUG_PRINT("enter", ("table: %s", table->getName()));

    /*
      List all foreign keys referencing the table to be dropped
      and recreate those to point at a new mock
    */
    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0)
    {
      error(dict, "Failed to list dependent objects for table '%s'", table->getName());
      DBUG_RETURN(false);
    }

    uint fk_index = 0;
    for (unsigned i = 0; i < list.count; i++)
    {
      const NdbDictionary::Dictionary::List::Element& element = list.elements[i];

      if (element.type != NdbDictionary::Object::ForeignKey)
        continue;

      DBUG_PRINT("fk", ("name: %s, type: %d", element.name, element.type));

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0)
      {
        // Could not find the listed fk
        DBUG_ASSERT(false);
        continue;
      }

      // Parent of the found fk should be the table to be dropped
      DBUG_PRINT("info", ("fk.parent: %s", fk.getParentTable()));
      char parent_db_and_name[FN_LEN + 1];
      const char * parent_name = fk_split_name(parent_db_and_name, fk.getParentTable());

      if (strcmp(parent_db_and_name, ndb->getDatabaseName()) != 0 ||
          strcmp(parent_name, table->getName()) != 0)
      {
        DBUG_PRINT("info", ("fk is not parent, skip"));
        continue;
      }

      DBUG_PRINT("info", ("fk.child: %s", fk.getChildTable()));
      char child_db_and_name[FN_LEN + 1];
      const char * child_name = fk_split_name(child_db_and_name, fk.getChildTable());

      // Open child table
      Ndb_db_guard db_guard(ndb);
      setDbName(ndb, child_db_and_name);
      Ndb_table_guard child_tab(dict, child_name);
      if (child_tab.get_table() == 0)
      {
        error(dict, "Failed to open child table '%s'", child_name);
        DBUG_RETURN(false);
      }

      /* Format mock table name */
      char mock_name[FN_REFLEN];
      if (!format_name(mock_name, sizeof(mock_name),
                       child_tab.get_table()->getObjectId(),
                       fk_index, parent_name))
      {
        error(NULL, "Failed to create mock parent table, too long mock name");
        DBUG_RETURN(false);
      }

      // Build both column name and column type list from parent(which will be dropped)
      const char* col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      const NdbDictionary::Column* col_types[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      {
        unsigned num_columns = 0;
        for (unsigned j = 0; j < fk.getParentColumnCount(); j++)
        {
          const NdbDictionary::Column* col =
              table->getColumn(fk.getParentColumnNo(j));
          DBUG_PRINT("col", ("[%u] %s", i, col->getName()));
          if (!col)
          {
            error(NULL, "Could not find column %d in parent table '%s'",
                  fk.getParentColumnNo(j), table->getName());
            continue;
          }
          col_names[num_columns] = col->getName();
          col_types[num_columns] = col;
          num_columns++;
        }
        col_names[num_columns]= 0;
        col_types[num_columns] = 0;

        if (num_columns != fk.getParentColumnCount())
        {
          error(NULL, "Could not find all columns referenced by fk in parent table '%s'",
                table->getName());
          continue;
        }
      }
      db_guard.restore(); // restore db

      // Create new mock
      if (!create(dict, mock_name, child_name,
                  col_names, col_types))
      {
        error(dict, "Failed to create mock parent table '%s", mock_name);
        DBUG_ASSERT(false);
        DBUG_RETURN(false);
      }

      // Recreate fks to point at new mock
      if (!copy_fk_to_new_parent(dict, fk, mock_name, col_names))
      {
        DBUG_RETURN(false);
      }

      fk_index++;
    }

    // Drop the requested table and all foreign keys refering to it
    // i.e the old fks
    const int drop_flags= NDBDICT::DropTableCascadeConstraints;
    if (dict->dropTableGlobal(*table, drop_flags) != 0)
    {
      error(dict, "Failed to drop the requested table");
      DBUG_RETURN(false);
    }

    DBUG_RETURN(true);
  }

public:
  Fk_util(THD* thd) : m_thd(thd) {}

  static
  bool split_mock_name(const char* name,
                       unsigned* child_id_ptr = NULL,
                       unsigned* child_index_ptr = NULL,
                       const char** parent_name = NULL)
  {
    const struct {
      const char* str;
      size_t len;
    } prefix = { STRING_WITH_LEN("NDB$FKM_") };

    if (strncmp(name, prefix.str, prefix.len) != 0)
      return false;

    char* end;
    const char* ptr= name + prefix.len + 1;

    // Parse child id
    long child_id = strtol(ptr, &end, 10);
    if (ptr == end || child_id < 0 || *end == 0 || *end != '_')
      return false;
    ptr = end+1;

    // Parse child index
    long child_index = strtol(ptr, &end, 10);
    if (ptr == end || child_id < 0 || *end == 0 || *end != '_')
      return false;
    ptr = end+1;

    // Assign and return OK
    if (child_id_ptr)
      *child_id_ptr = child_id;
    if (child_index_ptr)
      *child_index_ptr = child_index;
    if (parent_name)
      *parent_name = ptr;
    return true;
  }

  static
  bool is_mock_name(const char* name)
  {
    return split_mock_name(name);
  }

  static
  const char* format_name(char buf[], size_t buf_size, int child_id,
                          uint fk_index, const char* parent_name)
  {
    DBUG_ENTER("format_name");
    DBUG_PRINT("enter", ("child_id: %d, fk_index: %u, parent_name: %s",
                         child_id, fk_index, parent_name));
    const size_t len = snprintf(buf, buf_size, "NDB$FKM_%d_%u_%s",
                                   child_id, fk_index, parent_name);
    if (len >= buf_size - 1)
    {
      DBUG_PRINT("info", ("Size of buffer too small"));
      DBUG_RETURN(NULL);
    }
    DBUG_PRINT("exit", ("buf: '%s'", buf));
    DBUG_RETURN(buf);
  }


  // Adaptor function for calling create() with Mem_root_array<key_part_spec>
  bool create(NDBDICT *dict, const char* mock_name, const char* child_name,
              const Mem_root_array<const Key_part_spec*> &key_part_list,
              const NDBCOL * col_types[])
  {
    // Convert List<Key_part_spec> into null terminated const char* array
    const char* col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned i = 0;
      for (const Key_part_spec *key : key_part_list)
      {
        char col_name_buf[FN_REFLEN];
        const char* col_name = lex2str(key->field_name, col_name_buf);
        col_names[i++] = strdup(col_name);
      }
      col_names[i] = 0;
    }

    const bool ret = create(dict, mock_name, child_name, col_names, col_types);

    // Free the strings in col_names array
    for (unsigned i = 0; col_names[i] != 0; i++)
    {
      const char* col_name = col_names[i];
      free(const_cast<char*>(col_name));
    }

    return ret;
  }


  bool create(NDBDICT *dict, const char* mock_name, const char* child_name,
              const char* col_names[], const NDBCOL * col_types[])
  {
    NDBTAB mock_tab;

    DBUG_ENTER("mock_table::create");
    DBUG_PRINT("enter", ("mock_name: %s", mock_name));
    DBUG_ASSERT(is_mock_name(mock_name));

    if (mock_tab.setName(mock_name))
    {
      DBUG_RETURN(false);
    }
    mock_tab.setLogging(false);

    unsigned i = 0;
    while (col_names[i])
    {
      NDBCOL mock_col;

      const char* col_name = col_names[i];
      DBUG_PRINT("info", ("name: %s", col_name));
      if (mock_col.setName(col_name))
      {
        DBUG_ASSERT(false);
        DBUG_RETURN(false);
      }

      const NDBCOL * col= col_types[i];
      if (!col)
      {
        // Internal error, the two lists should be same size
        DBUG_ASSERT(col);
        DBUG_RETURN(false);
      }

      // Use column spec as requested(normally built from child table)
      mock_col.setType(col->getType());
      mock_col.setPrecision(col->getPrecision());
      mock_col.setScale(col->getScale());
      mock_col.setLength(col->getLength());
      mock_col.setCharset(col->getCharset());

      // Make column part of primary key and thus not nullable
      mock_col.setPrimaryKey(true);
      mock_col.setNullable(false);

      if (mock_tab.addColumn(mock_col))
      {
        DBUG_RETURN(false);
      }
      i++;
    }

    // Create the table in NDB
    if (dict->createTable(mock_tab) != 0)
    {
      // Error is available to caller in dict*
      DBUG_RETURN(false);
    }
    info("Created mock table '%s' referenced by '%s'", mock_name, child_name);
    DBUG_RETURN(true);
  }

  bool
  build_mock_list(NdbDictionary::Dictionary* dict,
                  const NdbDictionary::Table* table, List<char> &mock_list)
  {
    DBUG_ENTER("build_mock_list");

    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0)
    {
      error(dict, "Failed to list dependent objects for table '%s'", table->getName());
      DBUG_RETURN(false);
    }

    for (unsigned i = 0; i < list.count; i++)
    {
      const NdbDictionary::Dictionary::List::Element& element = list.elements[i];
      if (element.type != NdbDictionary::Object::ForeignKey)
        continue;

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0)
      {
        // Could not find the listed fk
        DBUG_ASSERT(false);
        continue;
      }

      char parent_db_and_name[FN_LEN + 1];
      const char * name = fk_split_name(parent_db_and_name,fk.getParentTable());

      if (!Fk_util::is_mock_name(name))
        continue;

      mock_list.push_back(thd_strdup(m_thd, fk.getParentTable()));
    }
    DBUG_RETURN(true);
  }


  void
  drop_mock_list(Ndb* ndb, NdbDictionary::Dictionary* dict, List<char> &drop_list)
  {
    const char* full_name;
    List_iterator_fast<char> it(drop_list);
    while ((full_name=it++))
    {
      DBUG_PRINT("info", ("drop table: '%s'", full_name));
      char db_name[FN_LEN + 1];
      const char * table_name = fk_split_name(db_name, full_name);
      Ndb_db_guard db_guard(ndb);
      setDbName(ndb, db_name);
      Ndb_table_guard mocktab_g(dict, table_name);
      if (!mocktab_g.get_table())
      {
       // Could not open the mock table
       DBUG_PRINT("error", ("Could not open the listed mock table, ignore it"));
       DBUG_ASSERT(false);
       continue;
      }

      if (dict->dropTableGlobal(*mocktab_g.get_table()) != 0)
      {
        DBUG_PRINT("error", ("Failed to drop the mock table '%s'",
                              mocktab_g.get_table()->getName()));
        DBUG_ASSERT(false);
        continue;
      }
      info("Dropped mock table '%s' - referencing table dropped", table_name);
    }
  }


  bool
  drop(Ndb* ndb, NdbDictionary::Dictionary* dict,
       const NdbDictionary::Table* table)
  {
    DBUG_ENTER("drop");

    // Start schema transaction to make this operation atomic
    if (dict->beginSchemaTrans() != 0)
    {
      error(dict, "Failed to start schema transaction");
      DBUG_RETURN(false);
    }

    bool result = true;
    if (!create_mock_tables_and_drop(ndb, dict, table))
    {
      // Operation failed, set flag to abort when ending trans
      result = false;
    }

    // End schema transaction
    const Uint32 end_trans_flag = result ?  0 : NdbDictionary::Dictionary::SchemaTransAbort;
    if (dict->endSchemaTrans(end_trans_flag) != 0)
    {
      error(dict, "Failed to end schema transaction");
      result = false;
    }

    DBUG_RETURN(result);
  }

  bool count_fks(NdbDictionary::Dictionary* dict,
                 const NdbDictionary::Table* table, uint& count) const
  {
    DBUG_ENTER("count_fks");

    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0)
    {
      error(dict, "Failed to list dependent objects for table '%s'", table->getName());
      DBUG_RETURN(false);
    }
    for (unsigned i = 0; i < list.count; i++)
    {
      if (list.elements[i].type == NdbDictionary::Object::ForeignKey)
        count++;
    }
    DBUG_PRINT("exit", ("count: %u", count));
    DBUG_RETURN(true);
  }


  bool drop_fk(Ndb* ndb, NdbDictionary::Dictionary* dict, const char* fk_name)
  {
    DBUG_ENTER("drop_fk");

    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, fk_name) != 0)
    {
      error(dict, "Could not find fk '%s'", fk_name);
      DBUG_ASSERT(false);
      DBUG_RETURN(false);
    }

    char parent_db_and_name[FN_LEN + 1];
    const char * parent_name = fk_split_name(parent_db_and_name,fk.getParentTable());
    if (Fk_util::is_mock_name(parent_name))
    {
      // Fk is referencing a mock table, drop the table
      // and the constraint at the same time
      Ndb_db_guard db_guard(ndb);
      setDbName(ndb, parent_db_and_name);
      Ndb_table_guard mocktab_g(dict, parent_name);
      if (mocktab_g.get_table())
      {
        const int drop_flags= NDBDICT::DropTableCascadeConstraints;
        if (dict->dropTableGlobal(*mocktab_g.get_table(), drop_flags) != 0)
        {
          error(dict, "Failed to drop fk mock table '%s'", parent_name);
          DBUG_ASSERT(false);
          DBUG_RETURN(false);
        }
        // table and fk dropped
        DBUG_RETURN(true);
      }
      else
      {
        warn("Could not open the fk mock table '%s', ignoring it...",
             parent_name);
        DBUG_ASSERT(false);
        // fallthrough and try to drop only the fk,
      }
    }

    if (dict->dropForeignKey(fk) != 0)
    {
      error(dict, "Failed to drop fk '%s'", fk_name);
      DBUG_RETURN(false);
    }
    DBUG_RETURN(true);
  }


  void
  resolve_mock_tables(NdbDictionary::Dictionary* dict,
                      const char* new_parent_db,
                      const char* new_parent_name) const
  {
    DBUG_ENTER("resolve_mock_tables");
    DBUG_PRINT("enter", ("new_parent_db: %s, new_parent_name: %s",
                         new_parent_db, new_parent_name));

    /*
      List all tables in NDB and look for mock tables which could
      potentially be resolved to the new table
    */
    NdbDictionary::Dictionary::List table_list;
    if (dict->listObjects(table_list, NdbDictionary::Object::UserTable, true) != 0)
    {
      DBUG_ASSERT(false);
      DBUG_VOID_RETURN;
    }

    for (unsigned i = 0; i < table_list.count; i++)
    {
      const NdbDictionary::Dictionary::List::Element& el = table_list.elements[i];

      DBUG_ASSERT(el.type == NdbDictionary::Object::UserTable);

      // Check if table is in same database as the potential new parent
      if (strcmp(new_parent_db, el.database) != 0)
      {
        DBUG_PRINT("info", ("Skip, '%s.%s' is in different database",
                            el.database, el.name));
        continue;
      }

      const char* parent_name;
      if (!Fk_util::split_mock_name(el.name, NULL, NULL, &parent_name))
        continue;

      // Check if this mock table should reference the new table
      if (strcmp(parent_name, new_parent_name) != 0)
      {
        DBUG_PRINT("info", ("Skip, parent of this mock table is not the new table"));
        continue;
      }

      resolve_mock(dict, new_parent_name, el.name);
    }

    DBUG_VOID_RETURN;
  }


  bool truncate_allowed(NdbDictionary::Dictionary* dict, const char* db,
                        const NdbDictionary::Table* table, bool& allow) const
  {
    DBUG_ENTER("truncate_allowed");

    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0)
    {
      error(dict, "Failed to list dependent objects for table '%s'", table->getName());
      DBUG_RETURN(false);
    }
    allow = true;
    for (unsigned i = 0; i < list.count; i++)
    {
      const NdbDictionary::Dictionary::List::Element& element = list.elements[i];
      if (element.type != NdbDictionary::Object::ForeignKey)
        continue;

      DBUG_PRINT("info", ("fk: %s", element.name));

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0)
      {
        error(dict, "Could not find the listed fk '%s'", element.name);
        DBUG_ASSERT(false);
        continue;
      }

      // Refuse if table is parent of fk
      char parent_db_and_name[FN_LEN + 1];
      const char * parent_name = fk_split_name(parent_db_and_name,
                                               fk.getParentTable());
      if (strcmp(db, parent_db_and_name) != 0 ||
          strcmp(parent_name, table->getName()) != 0)
      {
        // Not parent of the fk, skip
        continue;
      }

      allow = false;
      break;
    }
    DBUG_PRINT("exit", ("allow: %u", allow));
    DBUG_RETURN(true);
  }

  /**
    Generate FK info string from the NDBFK object.
    This can be called either by ha_ndbcluster::get_error_message
    or ha_ndbcluster:get_foreign_key_create_info.

    @param    thd               Current thread.
    @param    ndb               Pointer to the Ndb Object
    @param    fk                The foreign key object whose info
                                has to be printed.
    @param    tab_id            If this is > 0, the FK is printed only if the
                                table with this table id, is the child table of
                                the passed fk. This is > 0 only if the caller is
                                ha_ndbcluster:get_foreign_key_create_info().
    @param    print_mock_table_names
                                If true, mock tables names are printed rather
                                than the real parent names.
    @param    fk_string         String in which the fk info is to be printed.

    @retval   true              on success
              false             on failure.
  */
  bool
  generate_fk_constraint_string(Ndb *ndb,
                                const NdbDictionary::ForeignKey &fk,
                                const int tab_id,
                                const bool print_mock_table_names,
                                String &fk_string)
  {
    DBUG_ENTER("generate_fk_constraint_string");

    const NDBTAB *parenttab= 0;
    const NDBTAB *childtab= 0;
    NDBDICT *dict = ndb->getDictionary();
    Ndb_db_guard db_guard(ndb);

    /* The function generates fk constraint strings for
     * showing fk info in error and in show create table.
     * child_tab_id is non zero only for generating show create info */
    bool generating_for_show_create = (tab_id != 0);

    /* Fetch parent db and name and load it */
    Ndb_table_guard parent_table_guard(dict);
    char parent_db_and_name[FN_LEN + 1];
    {
      const char *name = fk_split_name(parent_db_and_name,
                                       fk.getParentTable());
      setDbName(ndb, parent_db_and_name);
      parent_table_guard.init(name);
      parenttab= parent_table_guard.get_table();
      if (parenttab == 0)
      {
        NdbError err= dict->getNdbError();
        warn("Unable to load parent table : error %d, %s",
             err.code, err.message);
        DBUG_RETURN(false);
      }
    }

    /* Fetch child db and name and load it */
    Ndb_table_guard child_table_guard(dict);
    char child_db_and_name[FN_LEN + 1];
    {
      const char * name = fk_split_name(child_db_and_name,
                                        fk.getChildTable());
      setDbName(ndb, child_db_and_name);
      child_table_guard.init(name);
      childtab= child_table_guard.get_table();
      if (childtab == 0)
      {
        NdbError err= dict->getNdbError();
        err= dict->getNdbError();
        warn("Unable to load child table : error %d, %s",
             err.code, err.message);
        DBUG_RETURN(false);
      }

      if(!generating_for_show_create)
      {
        /* Print child table name if printing error */
        fk_string.append("`");
        fk_string.append(child_db_and_name);
        fk_string.append("`.`");
        fk_string.append(name);
        fk_string.append("`, ");
      }
    }

    if (generating_for_show_create)
    {
      if(childtab->getTableId() != tab_id)
      {
        /**
         * This was on parent table (fk are shown on child table in SQL)
         * Skip printing this fk
         */
        assert(parenttab->getTableId() == tab_id);
        DBUG_RETURN(true);
      }

      fk_string.append(",");
      fk_string.append("\n  ");
    }

    fk_string.append("CONSTRAINT `");
    {
      char db_and_name[FN_LEN+1];
      const char * name = fk_split_name(db_and_name, fk.getName());
      fk_string.append(name);
    }
    fk_string.append("` FOREIGN KEY (");

    {
      const char* separator = "";
      for (unsigned j = 0; j < fk.getChildColumnCount(); j++)
      {
        unsigned no = fk.getChildColumnNo(j);
        fk_string.append(separator);
        fk_string.append("`");
        fk_string.append(childtab->getColumn(no)->getName());
        fk_string.append("`");
        separator = ",";
      }
    }

    fk_string.append(") REFERENCES `");
    if (strcmp(parent_db_and_name, child_db_and_name) != 0)
    {
      /* Print db name only if the parent and child are from different dbs */
      fk_string.append(parent_db_and_name);
      fk_string.append("`.`");
    }
    const char* real_parent_name;
    if (!print_mock_table_names &&
        Fk_util::split_mock_name(parenttab->getName(),
                                 NULL, NULL, &real_parent_name))
    {
      /* print the real table name */
      DBUG_PRINT("info", ("real_parent_name: %s", real_parent_name));
      fk_string.append(real_parent_name);
    }
    else
    {
      fk_string.append(parenttab->getName());
    }

    fk_string.append("` (");
    {
      const char* separator = "";
      for (unsigned j = 0; j < fk.getParentColumnCount(); j++)
      {
        unsigned no = fk.getParentColumnNo(j);
        fk_string.append(separator);
        fk_string.append("`");
        fk_string.append(parenttab->getColumn(no)->getName());
        fk_string.append("`");
        separator = ",";
      }
    }
    fk_string.append(")");

    /* print action strings */
    switch(fk.getOnDeleteAction()){
    case NdbDictionary::ForeignKey::NoAction:
      fk_string.append(" ON DELETE NO ACTION");
      break;
    case NdbDictionary::ForeignKey::Restrict:
      fk_string.append(" ON DELETE RESTRICT");
      break;
    case NdbDictionary::ForeignKey::Cascade:
      fk_string.append(" ON DELETE CASCADE");
      break;
    case NdbDictionary::ForeignKey::SetNull:
      fk_string.append(" ON DELETE SET NULL");
      break;
    case NdbDictionary::ForeignKey::SetDefault:
      fk_string.append(" ON DELETE SET DEFAULT");
      break;
    }

    switch(fk.getOnUpdateAction()){
    case NdbDictionary::ForeignKey::NoAction:
      fk_string.append(" ON UPDATE NO ACTION");
      break;
    case NdbDictionary::ForeignKey::Restrict:
      fk_string.append(" ON UPDATE RESTRICT");
      break;
    case NdbDictionary::ForeignKey::Cascade:
      fk_string.append(" ON UPDATE CASCADE");
      break;
    case NdbDictionary::ForeignKey::SetNull:
      fk_string.append(" ON UPDATE SET NULL");
      break;
    case NdbDictionary::ForeignKey::SetDefault:
      fk_string.append(" ON UPDATE SET DEFAULT");
      break;
    }

    DBUG_RETURN(true);
  }
};

void Fk_util::info(const char* fmt, ...) const
{
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args,fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Push as warning if user has turned on ndb_show_foreign_key_mock_tables
  if (ndb_show_foreign_key_mock_tables(m_thd))
  {
    push_warning(m_thd, Sql_condition::SL_WARNING, ER_YES, msg);
  }

  // Print info to log
  ndb_log_info("%s", msg);
}

void Fk_util::warn(const char* fmt, ...) const
{
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args,fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  push_warning(m_thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN, msg);

  // Print warning to log
  ndb_log_warning("%s", msg);
}

void Fk_util::error(const NdbDictionary::Dictionary* dict, const char* fmt, ...) const
{
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args,fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  push_warning(m_thd, Sql_condition::SL_WARNING,
               ER_CANNOT_ADD_FOREIGN, msg);

  char ndb_msg[MYSQL_ERRMSG_SIZE] = {0};
  if (dict)
  {
    // Extract message from Ndb
    const NdbError& error = dict->getNdbError();
    snprintf(ndb_msg, sizeof(ndb_msg),
                "%d '%s'", error.code, error.message);
    push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                        ER_CANNOT_ADD_FOREIGN, "Ndb error: %s", ndb_msg);
  }
  // Print error to log
  ndb_log_error("%s, Ndb error: %s", msg, ndb_msg);
}



bool ndb_fk_util_build_list(THD* thd, NdbDictionary::Dictionary* dict,
                            const NdbDictionary::Table* table, List<char> &mock_list)
{
  Fk_util fk_util(thd);
  return fk_util.build_mock_list(dict, table, mock_list);
}


void ndb_fk_util_drop_list(THD* thd, Ndb* ndb, NdbDictionary::Dictionary* dict, List<char> &drop_list)
{
  Fk_util fk_util(thd);
  fk_util.drop_mock_list(ndb, dict, drop_list);
}


bool ndb_fk_util_drop_table(THD* thd, Ndb* ndb, NdbDictionary::Dictionary* dict,
                            const NdbDictionary::Table* table)
{
  Fk_util fk_util(thd);
  return fk_util.drop(ndb, dict, table);
}


bool ndb_fk_util_is_mock_name(const char* table_name)
{
  return Fk_util::is_mock_name(table_name);
}


void
ndb_fk_util_resolve_mock_tables(THD* thd, NdbDictionary::Dictionary* dict,
                                const char* new_parent_db,
                                const char* new_parent_name)
{
  Fk_util fk_util(thd);
  fk_util.resolve_mock_tables(dict, new_parent_db, new_parent_name);
}


bool ndb_fk_util_truncate_allowed(THD* thd, NdbDictionary::Dictionary* dict,
                                  const char* db,
                                  const NdbDictionary::Table* table,
                                  bool& allowed)
{
  Fk_util fk_util(thd);
  if (!fk_util.truncate_allowed(dict, db, table, allowed))
    return false;
  return true;
}


bool ndb_fk_util_generate_constraint_string(THD* thd, Ndb *ndb,
                                            const NdbDictionary::ForeignKey &fk,
                                            const int tab_id,
                                            const bool print_mock_table_names,
                                            String &fk_string)
{
  Fk_util fk_util(thd);
  return fk_util.generate_fk_constraint_string(ndb, fk, tab_id,
                                               print_mock_table_names,
                                               fk_string);
}


/**
  @brief Flush the parent table after a successful addition/deletion
         to the Foreign Key. This is done to force reload the Parent
         table's metadata.

  @param thd            thread handle
  @param parent_db      Parent table's database name
  @param parent_name    Parent table's name
  @return Void
*/
static void
flush_parent_table_for_fk(THD* thd,
                          const char* parent_db, const char* parent_name)
{
  DBUG_ENTER("ha_ndbcluster::flush_parent_table_for_fk");

  if(Fk_util::is_mock_name(parent_name))
  {
    /* Parent table is mock - no need to flush */
    DBUG_PRINT("debug", ("Parent table is a mock - skipped flushing"));
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("debug", ("Flushing table : `%s`.`%s` ",
                       parent_db, parent_name));
  ndb_tdc_close_cached_table(thd, parent_db, parent_name);
  DBUG_VOID_RETURN;
}


int
ha_ndbcluster::create_fks(THD *thd, Ndb *ndb)
{
  DBUG_ENTER("ha_ndbcluster::create_fks");

  // return real mysql error to avoid total randomness..
  const int err_default= HA_ERR_CANNOT_ADD_FOREIGN;
  char tmpbuf[FN_REFLEN];

  assert(thd->lex != 0);
  for (const Key_spec *key : thd->lex->alter_info->key_list)
  {
    if (key->type != KEYTYPE_FOREIGN)
      continue;

    NDBDICT *dict= ndb->getDictionary();
    const Foreign_key_spec * fk= down_cast<const Foreign_key_spec*>(key);

    /**
     * NOTE: we need to fetch also child table...
     *   cause the one we just created (in m_table) is not properly
     *   initialize
     */
    Ndb_table_guard child_tab(dict, m_tabname);
    if (child_tab.get_table() == 0)
    {
      ERR_RETURN(dict->getNdbError());
    }

    /**
     * NOTE 2: we mark the table as invalid
     *         so that it gets removed from GlobalDictCache if
     *         the schema transaction later fails...
     *
     * TODO: This code currently fetches table definition from data-nodes
     *       once per FK...which could be improved to once if a FK
     */
    child_tab.invalidate();

    /**
     * Get table columns columns...
     */
    const NDBCOL * childcols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos= 0;
      const NDBTAB * tab= child_tab.get_table();
      for (const Key_part_spec *col : fk->columns)
      {
        const NDBCOL * ndbcol= tab->getColumn(lex2str(col->field_name,
                                                      tmpbuf));
        if (ndbcol == 0)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANNOT_ADD_FOREIGN,
                              "Child table %s has no column %s in NDB",
                              child_tab.get_table()->getName(), tmpbuf);
          DBUG_RETURN(err_default);
        }
        childcols[pos++]= ndbcol;
      }
      childcols[pos]= 0; // NULL terminate
    }

    bool child_primary_key= false;
    const NDBINDEX* child_index= find_matching_index(dict,
                                                     child_tab.get_table(),
                                                     childcols,
                                                     child_primary_key);

    if (!child_primary_key && child_index == 0)
    {
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_CANNOT_ADD_FOREIGN,
                          "Child table %s foreign key columns match no index in NDB",
                          child_tab.get_table()->getName());
      DBUG_RETURN(err_default);
    }

    Ndb_db_guard db_guard(ndb); // save db

    char parent_db[FN_REFLEN];
    char parent_name[FN_REFLEN];
    /*
     * Looking at Table_ident, testing for db.str first is safer
     * for valgrind.  Do same with table.str too.
     */
    if (fk->ref_db.str != 0 && fk->ref_db.length != 0)
    {
      snprintf(parent_db, sizeof(parent_db), "%*s",
                  (int)fk->ref_db.length,
                  fk->ref_db.str);
    }
    else
    {
      /* parent db missing - so the db is same as child's */
      snprintf(parent_db, sizeof(parent_db), "%*s",
                  (int)sizeof(m_dbname), m_dbname);
    }
    if (fk->ref_table.str != 0 && fk->ref_table.length != 0)
    {
      snprintf(parent_name, sizeof(parent_name), "%*s",
                  (int)fk->ref_table.length,
                  fk->ref_table.str);
    }
    else
    {
      parent_name[0]= 0;
    }
    if (lower_case_table_names)
    {
      ndb_fk_casedn(parent_db);
      ndb_fk_casedn(parent_name);
    }
    setDbName(ndb, parent_db);
    Ndb_table_guard parent_tab(dict, parent_name);
    if (parent_tab.get_table() == 0)
    {
       if (!thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
       {
         const NdbError &error= dict->getNdbError();
         push_warning_printf(thd, Sql_condition::SL_WARNING,
                             ER_CANNOT_ADD_FOREIGN,
                             "Parent table %s not found in NDB: %d: %s",
                             parent_name,
                             error.code, error.message);
         DBUG_RETURN(err_default);
       }

       DBUG_PRINT("info", ("No parent and foreign_key_checks=0"));

       Fk_util fk_util(thd);

       /* Count the number of existing fks on table */
       uint existing = 0;
       if(!fk_util.count_fks(dict, child_tab.get_table(), existing))
       {
         DBUG_RETURN(err_default);
       }

       /* Format mock table name */
       char mock_name[FN_REFLEN];
       if (!fk_util.format_name(mock_name, sizeof(mock_name),
                                child_tab.get_table()->getObjectId(),
                                existing, parent_name))
       {
         push_warning_printf(thd, Sql_condition::SL_WARNING,
                             ER_CANNOT_ADD_FOREIGN,
                             "Failed to create mock parent table, too long mock name");
         DBUG_RETURN(err_default);
       }
       if (!fk_util.create(dict, mock_name, m_tabname,
                           fk->ref_columns, childcols))
       {
         const NdbError &error= dict->getNdbError();
         push_warning_printf(thd, Sql_condition::SL_WARNING,
                             ER_CANNOT_ADD_FOREIGN,
                             "Failed to create mock parent table in NDB: %d: %s",
                             error.code, error.message);
         DBUG_RETURN(err_default);
       }

       parent_tab.init(mock_name);
       parent_tab.invalidate(); // invalidate mock table when releasing
       if (parent_tab.get_table() == 0)
       {
         push_warning_printf(thd, Sql_condition::SL_WARNING,
                             ER_CANNOT_ADD_FOREIGN,
                             "INTERNAL ERROR: Could not find created mock table '%s'",
                             mock_name);
         // Internal error, should be able to load the just created mock table
         DBUG_ASSERT(parent_tab.get_table());
         DBUG_RETURN(err_default);
       }       
    }

    const NDBCOL * parentcols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos= 0;
      const NDBTAB * tab= parent_tab.get_table();
      for (const Key_part_spec *col : fk->ref_columns)
      {
        const NDBCOL * ndbcol= tab->getColumn(lex2str(col->field_name,
                                                      tmpbuf));
        if (ndbcol == 0)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANNOT_ADD_FOREIGN,
                              "Parent table %s has no column %s in NDB",
                              parent_tab.get_table()->getName(), tmpbuf);
          DBUG_RETURN(err_default);
        }
        parentcols[pos++]= ndbcol;
      }
      parentcols[pos]= 0; // NULL terminate
    }

    bool parent_primary_key= false;
    const NDBINDEX* parent_index= find_matching_index(dict,
                                                      parent_tab.get_table(),
                                                      parentcols,
                                                      parent_primary_key);

    db_guard.restore(); // restore db

    if (!parent_primary_key && parent_index == 0)
    {
      my_error(ER_FK_NO_INDEX_PARENT, MYF(0),
               fk->name.str ? fk->name.str : "",
               parent_tab.get_table()->getName());
      DBUG_RETURN(err_default);
    }

    {
      /**
       * Check that columns match...this happens to be same
       *   condition as the one for SPJ...
       */
      for (unsigned i = 0; parentcols[i] != 0; i++)
      {
        if (parentcols[i]->isBindable(* childcols[i]) == -1)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANNOT_ADD_FOREIGN,
                              "Parent column %s.%s is incompatible with child column %s.%s in NDB",
                              parent_tab.get_table()->getName(),
                              parentcols[i]->getName(),
                              child_tab.get_table()->getName(),
                              childcols[i]->getName());
          DBUG_RETURN(err_default);
        }
      }
    }

    NdbDictionary::ForeignKey ndbfk;
    char fk_name[FN_REFLEN];
    if (fk->name.str && fk->name.length)
    {
      // The fk has a name, use it
      lex2str(fk->name, fk_name);
    }
    else
    {
      // The fk has no name, generate a name
      snprintf(fk_name, sizeof(fk_name), "FK_%u_%u",
                  parent_index ?
                  parent_index->getObjectId() :
                  parent_tab.get_table()->getObjectId(),
                  child_index ?
                  child_index->getObjectId() :
                  child_tab.get_table()->getObjectId());
    }
    if (lower_case_table_names)
      ndb_fk_casedn(fk_name);
    ndbfk.setName(fk_name);
    ndbfk.setParent(* parent_tab.get_table(), parent_index, parentcols);
    ndbfk.setChild(* child_tab.get_table(), child_index, childcols);

    switch((fk_option)fk->delete_opt){
    case FK_OPTION_UNDEF:
    case FK_OPTION_NO_ACTION:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::NoAction);
      break;
    case FK_OPTION_RESTRICT:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Restrict);
      break;
    case FK_OPTION_CASCADE:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Cascade);
      break;
    case FK_OPTION_SET_NULL:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetNull);
      break;
    case FK_OPTION_DEFAULT:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetDefault);
      break;
    default:
      assert(false);
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::NoAction);
    }

    switch((fk_option)fk->update_opt){
    case FK_OPTION_UNDEF:
    case FK_OPTION_NO_ACTION:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::NoAction);
      break;
    case FK_OPTION_RESTRICT:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Restrict);
      break;
    case FK_OPTION_CASCADE:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Cascade);
      break;
    case FK_OPTION_SET_NULL:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetNull);
      break;
    case FK_OPTION_DEFAULT:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetDefault);
      break;
    default:
      assert(false);
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::NoAction);
    }

    int flags = 0;
    if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
    {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }
    NdbDictionary::ObjectId objid;
    int err= dict->createForeignKey(ndbfk, &objid, flags);

    if (child_index)
    {
      dict->removeIndexGlobal(* child_index, 0);
    }

    if (parent_index)
    {
      dict->removeIndexGlobal(* parent_index, 0);
    }

    if (err)
    {
      const NdbError err = dict->getNdbError();
      if (err.code == 721)
      {
        /* An FK constraint with same name exists */
        my_error(ER_FK_DUP_NAME, MYF(0), ndbfk.getName());
        DBUG_RETURN(err_default);
      }
      else
      {
        /* Return the error returned by dict */
        ERR_RETURN(err);
      }
    }

    /* Flush the parent table out if parent is different from child */
    if (parent_tab.get_table()->getObjectId() !=
          child_tab.get_table()->getObjectId())
    {
      /* flush parent table */
      flush_parent_table_for_fk(thd, parent_db, parent_name);
    }
  }

  ndb_fk_util_resolve_mock_tables(thd, ndb->getDictionary(),
                                  m_dbname, m_tabname);

  DBUG_RETURN(0);
}


uint
ha_ndbcluster::referenced_by_foreign_key()
{
  DBUG_ENTER("ha_ndbcluster::referenced_by_foreign_key");

  Ndb_fk_data *data= m_fk_data;
  if (data == 0)
  {
    DBUG_ASSERT(false);
    DBUG_RETURN(0);
  }

  DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                      data->list.elements, data->cnt_child, data->cnt_parent));
  DBUG_RETURN(data->cnt_parent != 0);
}

uint
ha_ndbcluster::is_child_or_parent_of_fk()
{
  DBUG_ENTER("ha_ndbcluster::is_child_or_parent_of_fk");

  Ndb_fk_data *data= m_fk_data;
  if (data == 0)
  {
    DBUG_ASSERT(false);
    DBUG_RETURN(0);
  }
  
  DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                      data->list.elements, data->cnt_child, data->cnt_parent));
  DBUG_RETURN(data->list.elements != 0);
}

bool
ha_ndbcluster::can_switch_engines()
{
  DBUG_ENTER("ha_ndbcluster::can_switch_engines");

  if (is_child_or_parent_of_fk())
    DBUG_RETURN(0);

  DBUG_RETURN(1);
}


struct Ndb_mem_root_guard {
  Ndb_mem_root_guard(MEM_ROOT *new_root) {
    root_ptr= THR_MALLOC;
    DBUG_ASSERT(root_ptr != 0);
    old_root= *root_ptr;
    *root_ptr= new_root;
  }
  ~Ndb_mem_root_guard() {
    *root_ptr= old_root;
  }
private:
  MEM_ROOT **root_ptr;
  MEM_ROOT *old_root;
};

int
ha_ndbcluster::get_fk_data(THD *thd, Ndb *ndb)
{
  DBUG_ENTER("ha_ndbcluster::get_fk_data");

  MEM_ROOT *mem_root= &m_fk_mem_root;
  Ndb_mem_root_guard mem_root_guard(mem_root);

  free_root(mem_root, 0);
  m_fk_data= 0;
  init_alloc_root(PSI_INSTRUMENT_ME, mem_root, fk_root_block_size, 0);

  NdbError err_OOM;
  err_OOM.code= 4000; // should we check OOM errors at all?
  NdbError err_API;
  err_API.code= 4011; // API internal should not happen

  Ndb_fk_data *data= new (mem_root) Ndb_fk_data;
  if (data == 0)
    ERR_RETURN(err_OOM);
  data->cnt_child= 0;
  data->cnt_parent= 0;

  DBUG_PRINT("info", ("%s.%s: list dependent objects",
                      m_dbname, m_tabname));
  int res;
  NDBDICT *dict= ndb->getDictionary();
  NDBDICT::List obj_list;
  res= dict->listDependentObjects(obj_list, *m_table);
  if (res != 0)
    ERR_RETURN(dict->getNdbError());
  DBUG_PRINT("info", ("found %u dependent objects", obj_list.count));

  for (unsigned i = 0; i < obj_list.count; i++)
  {
    const NDBDICT::List::Element &e= obj_list.elements[i];
    if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
    {
      DBUG_PRINT("info", ("skip non-FK %s type %d", e.name, e.type));
      continue;
    }
    DBUG_PRINT("info", ("found FK %s", e.name));

    NdbDictionary::ForeignKey fk;
    res= dict->getForeignKey(fk, e.name);
    if (res != 0)
      ERR_RETURN(dict->getNdbError());

    Ndb_fk_item *item= new (mem_root) Ndb_fk_item;
    if (item == 0)
      ERR_RETURN(err_OOM);
    FOREIGN_KEY_INFO &f_key_info= item->f_key_info;

    {
      char fk_full_name[FN_LEN + 1];
      const char * name = fk_split_name(fk_full_name, fk.getName());
      f_key_info.foreign_id = thd_make_lex_string(thd, 0, name,
                                                  (uint)strlen(name), 1);
    }

    {
      char child_db_and_name[FN_LEN + 1];
      const char * child_name = fk_split_name(child_db_and_name,
                                              fk.getChildTable());

      /* Dependent (child) database name */
      f_key_info.foreign_db =
        thd_make_lex_string(thd, 0, child_db_and_name,
                            (uint)strlen(child_db_and_name),
                            1);
      /* Dependent (child) table name */
      f_key_info.foreign_table =
        thd_make_lex_string(thd, 0, child_name,
                            (uint)strlen(child_name),
                            1);

      Ndb_db_guard db_guard(ndb);
      setDbName(ndb, child_db_and_name);
      Ndb_table_guard child_tab(dict, child_name);
      if (child_tab.get_table() == 0)
      {
        DBUG_ASSERT(false);
        ERR_RETURN(dict->getNdbError());
      }

      for (unsigned i = 0; i < fk.getChildColumnCount(); i++)
      {
        const NdbDictionary::Column * col =
          child_tab.get_table()->getColumn(fk.getChildColumnNo(i));
        if (col == 0)
          ERR_RETURN(err_API);
        LEX_STRING * name =
          thd_make_lex_string(thd, 0, col->getName(),
                              (uint)strlen(col->getName()), 1);
        f_key_info.foreign_fields.push_back(name);
      }
    }

    {
      char parent_db_and_name[FN_LEN + 1];
      const char * parent_name = fk_split_name(parent_db_and_name,
                                               fk.getParentTable());

      /* Referenced (parent) database name */
      f_key_info.referenced_db =
        thd_make_lex_string(thd, 0, parent_db_and_name,
                            (uint)strlen(parent_db_and_name),
                            1);
      /* Referenced (parent) table name */
      f_key_info.referenced_table =
        thd_make_lex_string(thd, 0, parent_name,
                            (uint)strlen(parent_name),
                            1);

      Ndb_db_guard db_guard(ndb);
      setDbName(ndb, parent_db_and_name);
      Ndb_table_guard parent_tab(dict, parent_name);
      if (parent_tab.get_table() == 0)
      {
        DBUG_ASSERT(false);
        ERR_RETURN(dict->getNdbError());
      }

      for (unsigned i = 0; i < fk.getParentColumnCount(); i++)
      {
        const NdbDictionary::Column * col =
          parent_tab.get_table()->getColumn(fk.getParentColumnNo(i));
        if (col == 0)
          ERR_RETURN(err_API);
        LEX_STRING * name =
          thd_make_lex_string(thd, 0, col->getName(),
                              (uint)strlen(col->getName()), 1);
        f_key_info.referenced_fields.push_back(name);
      }

    }

    {
      const char *update_method = "";
      switch (item->update_action= fk.getOnUpdateAction()){
      case NdbDictionary::ForeignKey::NoAction:
        update_method = "NO ACTION";
        break;
      case NdbDictionary::ForeignKey::Restrict:
        update_method = "RESTRICT";
        break;
      case NdbDictionary::ForeignKey::Cascade:
        update_method = "CASCADE";
        break;
      case NdbDictionary::ForeignKey::SetNull:
        update_method = "SET NULL";
        break;
      case NdbDictionary::ForeignKey::SetDefault:
        update_method = "SET DEFAULT";
        break;
      }
      f_key_info.update_method =
        thd_make_lex_string(thd, 0, update_method,
                            (uint)strlen(update_method),
                            1);
    }

    {
      const char *delete_method = "";
      switch (item->delete_action= fk.getOnDeleteAction()){
      case NdbDictionary::ForeignKey::NoAction:
        delete_method = "NO ACTION";
        break;
      case NdbDictionary::ForeignKey::Restrict:
        delete_method = "RESTRICT";
        break;
      case NdbDictionary::ForeignKey::Cascade:
        delete_method = "CASCADE";
        break;
      case NdbDictionary::ForeignKey::SetNull:
        delete_method = "SET NULL";
        break;
      case NdbDictionary::ForeignKey::SetDefault:
        delete_method = "SET DEFAULT";
        break;
      }
      f_key_info.delete_method =
        thd_make_lex_string(thd, 0, delete_method,
                            (uint)strlen(delete_method),
                            1);
    }

    if (fk.getParentIndex() != 0)
    {
      // sys/def/10/xb1$unique
      char db_and_name[FN_LEN + 1];
      const char * name=fk_split_name(db_and_name, fk.getParentIndex(), true);
      f_key_info.referenced_key_name =
        thd_make_lex_string(thd, 0, name,
                            (uint)strlen(name),
                            1);
    }
    else
    {
      const char* name= "PRIMARY";
      f_key_info.referenced_key_name =
        thd_make_lex_string(thd, 0, name,
                            (uint)strlen(name),
                            1);
    }

    item->is_child=
      strcmp(m_dbname, f_key_info.foreign_db->str) == 0 &&
      strcmp(m_tabname, f_key_info.foreign_table->str) == 0;

    item->is_parent=
      strcmp(m_dbname, f_key_info.referenced_db->str) == 0 &&
      strcmp(m_tabname, f_key_info.referenced_table->str) == 0;

    data->cnt_child+= item->is_child;
    data->cnt_parent+= item->is_parent;

    res= data->list.push_back(item);
    if (res != 0)
      ERR_RETURN(err_OOM);
  }

  DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                      data->list.elements, data->cnt_child,
                      data->cnt_parent));

  m_fk_data= data;
  DBUG_RETURN(0);
}

void
ha_ndbcluster::release_fk_data()
{
  DBUG_ENTER("ha_ndbcluster::release_fk_data");

  Ndb_fk_data *data= m_fk_data;
  if (data != 0)
  {
    DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                        data->list.elements, data->cnt_child,
                        data->cnt_parent));
  }

  MEM_ROOT *mem_root= &m_fk_mem_root;
  free_root(mem_root, 0);
  m_fk_data= 0;

  DBUG_VOID_RETURN;
}

int
ha_ndbcluster::get_child_or_parent_fk_list(List<FOREIGN_KEY_INFO> * f_key_list,
                                           bool is_child, bool is_parent)
{
  DBUG_ENTER("ha_ndbcluster::get_child_or_parent_fk_list");
  DBUG_PRINT("info", ("table %s.%s", m_dbname, m_tabname));

  Ndb_fk_data *data= m_fk_data;
  if (data == 0)
  {
    DBUG_ASSERT(false);
    DBUG_RETURN(0);
  }

  DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                      data->list.elements, data->cnt_child, data->cnt_parent));

  Ndb_fk_item *item= 0;
  List_iterator<Ndb_fk_item> iter(data->list);
  while ((item= iter++))
  {
    FOREIGN_KEY_INFO &f_key_info= item->f_key_info;
    DBUG_PRINT("info", ("FK %s ref %s -> %s is_child %d is_parent %d",
                        f_key_info.foreign_id->str,
                        f_key_info.foreign_table->str,
                        f_key_info.referenced_table->str,
                        item->is_child, item->is_parent));
    if (is_child && !item->is_child)
      continue;
    if (is_parent && !item->is_parent)
      continue;

    DBUG_PRINT("info", ("add %s to list", f_key_info.foreign_id->str));
    f_key_list->push_back(&f_key_info);
  }

  DBUG_RETURN(0);
}

int
ha_ndbcluster::get_foreign_key_list(THD*,
                                    List<FOREIGN_KEY_INFO> * f_key_list)
{
  DBUG_ENTER("ha_ndbcluster::get_foreign_key_list");
  int res= get_child_or_parent_fk_list(f_key_list, true, false);
  DBUG_PRINT("info", ("count FKs child %u", f_key_list->elements));
  DBUG_RETURN(res);
}

int
ha_ndbcluster::get_parent_foreign_key_list(THD*,
                                           List<FOREIGN_KEY_INFO> * f_key_list)
{
  DBUG_ENTER("ha_ndbcluster::get_parent_foreign_key_list");
  int res= get_child_or_parent_fk_list(f_key_list, false, true);
  DBUG_PRINT("info", ("count FKs parent %u", f_key_list->elements));
  DBUG_RETURN(res);
}

namespace {

struct cmp_fk_name {
  bool operator() (const NDBDICT::List::Element &e0,
                   const NDBDICT::List::Element &e1) const
  {
    int res;
    if ((res= strcmp(e0.name, e1.name)) != 0)
      return res < 0;

    if ((res= strcmp(e0.database, e1.database)) != 0)
      return res < 0;

    if ((res= strcmp(e0.schema, e1.schema)) != 0)
      return res < 0;

    return e0.id < e1.id;
  }
};

}  // namespace

char*
ha_ndbcluster::get_foreign_key_create_info()
{
  DBUG_ENTER("ha_ndbcluster::get_foreign_key_create_info");

  /**
   * List foreigns for this table
   */
  if (m_table == 0)
  {
    DBUG_RETURN(0);
  }

  if (table == 0)
  {
    DBUG_RETURN(0);
  }

  THD* thd = table->in_use;
  if (thd == 0)
  {
    DBUG_RETURN(0);
  }

  Ndb *ndb= get_ndb(thd);
  if (ndb == 0)
  {
    DBUG_RETURN(0);
  }

  NDBDICT *dict= ndb->getDictionary();
  NDBDICT::List obj_list;

  dict->listDependentObjects(obj_list, *m_table);
  /**
   * listDependentObjects will return FK's in order that they
   *   are stored in hash-table in Dbdict (i.e random)
   *
   * sort them to make MTR and similar happy
   */
  std::sort(obj_list.elements, obj_list.elements + obj_list.count,
            cmp_fk_name());
  String fk_string;
  for (unsigned i = 0; i < obj_list.count; i++)
  {
    if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
      continue;

    NdbDictionary::ForeignKey fk;
    int res= dict->getForeignKey(fk, obj_list.elements[i].name);
    if (res != 0)
    {
      // Push warning??
      DBUG_RETURN(0);
    }

    if (!ndb_fk_util_generate_constraint_string(thd, ndb, fk,
                                                m_table->getTableId(),
                                                ndb_show_foreign_key_mock_tables(thd),
                                                fk_string))
    {
      DBUG_RETURN(0); // How to report error ??
    }
  }

  DBUG_RETURN(strdup(fk_string.c_ptr()));
}

void
ha_ndbcluster::free_foreign_key_create_info(char* str)
{
  if (str != 0)
  {
    free(str);
  }
}

int
ha_ndbcluster::copy_fk_for_offline_alter(THD * thd, Ndb* ndb, NDBTAB* _dsttab)
{
  DBUG_ENTER("ha_ndbcluster::copy_fk_for_offline_alter");
  if (thd->lex == 0)
  {
    assert(false);
    DBUG_RETURN(0);
  }

  Ndb_db_guard db_guard(ndb);
  const char * src_db = thd->lex->select_lex->table_list.first->db;
  const char * src_tab = thd->lex->select_lex->table_list.first->table_name;

  if (src_db == 0 || src_tab == 0)
  {
    assert(false);
    DBUG_RETURN(0);
  }

  assert(thd->lex != 0);
  NDBDICT* dict = ndb->getDictionary();
  setDbName(ndb, src_db);
  Ndb_table_guard srctab(dict, src_tab);
  if (srctab.get_table() == 0)
  {
    /**
     * when doign alter table engine=ndb this can happen
     */
    DBUG_RETURN(0);
  }

  db_guard.restore();
  Ndb_table_guard dsttab(dict, _dsttab->getName());
  if (dsttab.get_table() == 0)
  {
    ERR_RETURN(dict->getNdbError());
  }

  setDbName(ndb, src_db);
  NDBDICT::List obj_list;
  if (dict->listDependentObjects(obj_list, *srctab.get_table()) != 0)
  {
    ERR_RETURN(dict->getNdbError());
  }

  // check if fk to drop exists
  {
    for (const Alter_drop *drop_item : thd->lex->alter_info->drop_list)
    {
      if (drop_item->type != Alter_drop::FOREIGN_KEY)
        continue;
      bool found= false;
      for (unsigned i = 0; i < obj_list.count; i++)
      {
        // Skip if the element is not a foreign key
        if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
          continue;

        // Check if this is the fk being dropped
        char db_and_name[FN_LEN + 1];
        const char * name= fk_split_name(db_and_name,obj_list.elements[i].name);
        if (ndb_fk_casecmp(drop_item->name, name) != 0)
          continue;

        NdbDictionary::ForeignKey fk;
        if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
        {
          // should never happen
          DBUG_ASSERT(false);
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANT_DROP_FIELD_OR_KEY,
                              "INTERNAL ERROR: Could not find foreign key '%s'",
                              obj_list.elements[i].name);
          ERR_RETURN(dict->getNdbError());
        }

        // The FK we are looking for is on src_tab.
        char child_db_and_name[FN_LEN + 1];
        const char* child_name = fk_split_name(child_db_and_name,
                                               fk.getChildTable());
        if (strcmp(child_db_and_name, src_db) == 0 &&
            strcmp(child_name, src_tab) == 0)
        {
          found= true;
          break;
        }
      }
      if (!found)
      {
        // FK not found
        my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0), drop_item->name);
        DBUG_RETURN(ER_CANT_DROP_FIELD_OR_KEY);
      }
    }
  }

  for (unsigned i = 0; i < obj_list.count; i++)
  {
    if (obj_list.elements[i].type == NdbDictionary::Object::ForeignKey)
    {
      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
      {
        // should never happen
        DBUG_ASSERT(false);
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_ALTER_INFO,
                            "INTERNAL ERROR: Could not find foreign key '%s'",
                            obj_list.elements[i].name);
        ERR_RETURN(dict->getNdbError());
      }

      {
        /**
         * Check if it should be copied
         */
        char db_and_name[FN_LEN + 1];
        const char * name= fk_split_name(db_and_name,obj_list.elements[i].name);

        bool found= false;
        for (const Alter_drop *drop_item : thd->lex->alter_info->drop_list)
        {
          if (drop_item->type != Alter_drop::FOREIGN_KEY)
            continue;
          if (ndb_fk_casecmp(drop_item->name, name) != 0)
            continue;

          char child_db_and_name[FN_LEN + 1];
          const char* child_name = fk_split_name(child_db_and_name,
                                                 fk.getChildTable());
          if (strcmp(child_db_and_name, src_db) == 0 &&
              strcmp(child_name, src_tab) == 0)
          {
            found= true;
            break;
          }
        }
        if (found)
        {
          /**
           * Item is on drop list...
           *   don't copy it
           */
          continue;
        }
      }

      unsigned parentObjectId= 0;
      unsigned childObjectId= 0;

      {
        char db_and_name[FN_LEN + 1];
        const char * name= fk_split_name(db_and_name, fk.getParentTable());
        setDbName(ndb, db_and_name);
        Ndb_table_guard org_parent(dict, name);
        if (org_parent.get_table() == 0)
        {
          ERR_RETURN(dict->getNdbError());
        }
        parentObjectId= org_parent.get_table()->getObjectId();
      }

      {
        char db_and_name[FN_LEN + 1];
        const char * name= fk_split_name(db_and_name, fk.getChildTable());
        setDbName(ndb, db_and_name);
        Ndb_table_guard org_child(dict, name);
        if (org_child.get_table() == 0)
        {
          ERR_RETURN(dict->getNdbError());
        }
        childObjectId= org_child.get_table()->getObjectId();
      }

      /**
       * flags for CreateForeignKey
       */
      int flags = 0;

      char db_and_name[FN_LEN + 1];
      const char * name= fk_split_name(db_and_name, fk.getParentTable());
      if (strcmp(name, src_tab) == 0 &&
          strcmp(db_and_name, src_db) == 0)
      {
        /**
         * We used to be parent...
         */
        const NDBCOL * cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
        for (unsigned j= 0; j < fk.getParentColumnCount(); j++)
        {
          unsigned no= fk.getParentColumnNo(j);
          const NDBCOL * orgcol = srctab.get_table()->getColumn(no);
          cols[j]= dsttab.get_table()->getColumn(orgcol->getName());
        }
        cols[fk.getParentColumnCount()]= 0;
        parentObjectId= dsttab.get_table()->getObjectId();
        if (fk.getParentIndex() != 0)
        {
          name = fk_split_name(db_and_name, fk.getParentIndex(), true);
          setDbName(ndb, db_and_name);
          const NDBINDEX * idx = dict->getIndexGlobal(name,*dsttab.get_table());
          if (idx == 0)
          {
            printf("%u %s - %u/%u get_index(%s)\n",
                   __LINE__, fk.getName(),
                   parentObjectId,
                   childObjectId,
                   name); fflush(stdout);
            ERR_RETURN(dict->getNdbError());
          }
          fk.setParent(* dsttab.get_table(), idx, cols);
          dict->removeIndexGlobal(* idx, 0);
        }
        else
        {
          /*
            The parent column was previously the primary key.
            Make sure it still is a primary key as implicit pks
            might change during the alter. If not, get a better
            matching index.
           */
          bool parent_primary = false;
          const NDBINDEX * idx = find_matching_index(dict,
                                                     dsttab.get_table(),
                                                     cols,
                                                     parent_primary);
          if (!parent_primary && idx == 0)
          {
            my_error(ER_FK_NO_INDEX_PARENT, MYF(0), fk.getName(),
                     dsttab.get_table()->getName());
            DBUG_RETURN(HA_ERR_CANNOT_ADD_FOREIGN);
          }
          fk.setParent(*dsttab.get_table(), idx, cols);
        }


        /**
         * We're parent, and this is offline alter table
         *   then we can't verify that FK cause the new parent will
         *   be populated later during copy data between tables
         *
         * However, iff FK is consistent when this alter starts,
         *   it should remain consistent since mysql does not
         *   allow the alter to modify the columns referenced
         */
        flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
      }
      else
      {
        name = fk_split_name(db_and_name, fk.getChildTable());
        assert(strcmp(name, src_tab) == 0 &&
               strcmp(db_and_name, src_db) == 0);
        const NDBCOL * cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
        for (unsigned j= 0; j < fk.getChildColumnCount(); j++)
        {
          unsigned no= fk.getChildColumnNo(j);
          const NDBCOL * orgcol = srctab.get_table()->getColumn(no);
          cols[j]= dsttab.get_table()->getColumn(orgcol->getName());
        }
        cols[fk.getChildColumnCount()]= 0;
        childObjectId= dsttab.get_table()->getObjectId();
        if (fk.getChildIndex() != 0)
        {
          name = fk_split_name(db_and_name, fk.getChildIndex(), true);
          setDbName(ndb, db_and_name);
          bool child_primary_key = false;
          const NDBINDEX * idx = find_matching_index(dict,
                                                     dsttab.get_table(),
                                                     cols,
                                                     child_primary_key);
          if (!child_primary_key && idx == 0)
          {
            printf("%u %s - %u/%u get_index(%s)\n",
                   __LINE__, fk.getName(),
                   parentObjectId,
                   childObjectId,
                   name); fflush(stdout);
            ERR_RETURN(dict->getNdbError());
          }
          fk.setChild(* dsttab.get_table(), idx, cols);
          if(idx)
            dict->removeIndexGlobal(*idx, 0);
        }
        else
        {
          fk.setChild(* dsttab.get_table(), 0, cols);
        }
      }

      char new_name[FN_LEN + 1];
      name= fk_split_name(db_and_name, fk.getName());
      snprintf(new_name, sizeof(new_name), "%s",
                  name);
      fk.setName(new_name);
      setDbName(ndb, db_and_name);

      if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
      {
        flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
      }
      NdbDictionary::ObjectId objid;
      if (dict->createForeignKey(fk, &objid, flags) != 0)
      {
        ERR_RETURN(dict->getNdbError());
      }
    }
  }
  DBUG_RETURN(0);
}

int
ha_ndbcluster::inplace__drop_fks(THD * thd, Ndb* ndb, NDBDICT * dict,
                                const NDBTAB* tab)
{
  DBUG_ENTER("ha_ndbcluster::inplace__drop_fks");
  if (thd->lex == 0)
  {
    assert(false);
    DBUG_RETURN(0);
  }

  Ndb_table_guard srctab(dict, tab->getName());
  if (srctab.get_table() == 0)
  {
    DBUG_ASSERT(false); // Why ??
    DBUG_RETURN(0);
  }

  NDBDICT::List obj_list;
  if (dict->listDependentObjects(obj_list, *srctab.get_table()) != 0)
  {
    ERR_RETURN(dict->getNdbError());
  }

  for (const Alter_drop *drop_item : thd->lex->alter_info->drop_list)
  {
    if (drop_item->type != Alter_drop::FOREIGN_KEY)
      continue;

    bool found= false;
    for (unsigned i = 0; i < obj_list.count; i++)
    {
      if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
      {
        continue;
      }

      char db_and_name[FN_LEN + 1];
      const char * name= fk_split_name(db_and_name,obj_list.elements[i].name);

      if (ndb_fk_casecmp(drop_item->name, name) != 0)
        continue;

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
      {
        ERR_RETURN(dict->getNdbError());
      }

      char child_db_and_name[FN_LEN + 1];
      const char* child_name = fk_split_name(child_db_and_name,
                                             fk.getChildTable());
      if (strcmp(child_db_and_name, ndb->getDatabaseName()) == 0 &&
          strcmp(child_name, tab->getName()) == 0)
      {
        found= true;
        Fk_util fk_util(thd);
        if (!fk_util.drop_fk(ndb, dict, obj_list.elements[i].name))
        {
          ERR_RETURN(dict->getNdbError());
        }

        /* Flush the parent table out if parent is different from child */
        if(ndb_fk_casecmp(fk.getParentTable(), fk.getChildTable()) != 0)
        {
          char parent_db[FN_LEN + 1];
          const char* parent_name = fk_split_name(parent_db,
                                                  fk.getParentTable());
          flush_parent_table_for_fk(thd, parent_db, parent_name);
        }
        break;
      }
    }
    if (!found)
    {
      // FK not found
      my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0), drop_item->name);
      DBUG_RETURN(ER_CANT_DROP_FIELD_OR_KEY);
    }
  }
  DBUG_RETURN(0);
}


/**
  Save all fk data into a fk_list
  - Build list of foreign keys for which the given table is child

  @retval
    0     ok
  @retval
   != 0   failure in saving the fk data
*/

int
ha_ndbcluster::get_fk_data_for_truncate(NDBDICT* dict, const NDBTAB* table,
                                        Ndb_fk_list& fk_list)
{
  DBUG_ENTER("ha_ndbcluster::get_fk_data_for_truncate");

  NDBDICT::List obj_list;
  if (dict->listDependentObjects(obj_list, *table) != 0)
  {
    ERR_RETURN(dict->getNdbError());
  }
  for (unsigned i = 0; i < obj_list.count; i++)
  {
    DBUG_PRINT("debug", ("DependentObject %d : %s, Type : %d", i,
                          obj_list.elements[i].name,
                          obj_list.elements[i].type));
    if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
      continue;

    /* obj is an fk. Fetch it */
    NDBFK fk;
    if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
    {
      ERR_RETURN(dict->getNdbError());
    }
    DBUG_PRINT("debug", ("Retrieving FK : %s", fk.getName()));

    fk_list.push_back(new NdbDictionary::ForeignKey(fk));
    DBUG_PRINT("info", ("Foreign Key added to list : %s", fk.getName()));
  }

  DBUG_RETURN(0);
}


/**
  Restore foreign keys into the child table from fk_list
  - for all foreign keys in the given fk list, re-assign child object ids
    to reflect the newly created child table/indexes
  - create the fk in the child table

  @retval
    0     ok
  @retval
   != 0   failure in recreating the fk data
*/

int
ha_ndbcluster::recreate_fk_for_truncate(THD* thd, Ndb* ndb, const char* tab_name,
                                        Ndb_fk_list& fk_list)
{
  DBUG_ENTER("ha_ndbcluster::create_fk_for_truncate");

  int flags = 0;
  const int err_default= HA_ERR_CANNOT_ADD_FOREIGN;

  NDBDICT* dict = ndb->getDictionary();

  /* fetch child table */
  Ndb_table_guard child_tab(dict, tab_name);
  if (child_tab.get_table() == 0)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_CANNOT_ADD_FOREIGN,
                        "INTERNAL ERROR: Could not find created child table '%s'",
                        tab_name);
    // Internal error, should be able to load the just created child table
    DBUG_ASSERT(child_tab.get_table());
    DBUG_RETURN(err_default);
  }

  NDBFK* fk;
  List_iterator<NDBFK> fk_iterator(fk_list);
  while ((fk= fk_iterator++))
  {
    DBUG_PRINT("info",("Parsing foreign key : %s", fk->getName()));

    /* Get child table columns and index */
    const NDBCOL * child_cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos= 0;
      const NDBTAB* tab= child_tab.get_table();
      for(unsigned i= 0; i < fk->getChildColumnCount(); i++)
      {
        const NDBCOL * ndbcol= tab->getColumn(fk->getChildColumnNo(i));
        if (ndbcol == 0)
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_CANNOT_ADD_FOREIGN,
                              "Child table %s has no column referred by the FK %s",
                              tab->getName(), fk->getName());
          DBUG_ASSERT(ndbcol);
          DBUG_RETURN(err_default);
        }
        child_cols[pos++]= ndbcol;
      }
      child_cols[pos]= 0;
    }

    bool child_primary_key= false;
    const NDBINDEX* child_index= find_matching_index(dict,
                                                     child_tab.get_table(),
                                                     child_cols,
                                                     child_primary_key);

    if (!child_primary_key && child_index == 0)
    {
      my_error(ER_FK_NO_INDEX_CHILD, MYF(0), fk->getName(),
               child_tab.get_table()->getName());
      DBUG_RETURN(err_default);
    }

    /* update the fk's child references */
    fk->setChild(* child_tab.get_table(), child_index, child_cols);

    /*
     the name of "fk" seems to be different when you read it up
     compared to when you create it. (Probably a historical artifact)
     So update fk's name
    */
    {
      char name[FN_REFLEN+1];
      unsigned parent_id, child_id;
      if (sscanf(fk->getName(), "%u/%u/%s",
                 &parent_id, &child_id, name) != 3)
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANNOT_ADD_FOREIGN,
                            "Skip, failed to parse name of fk: %s",
                            fk->getName());
        DBUG_RETURN(err_default);
      }

      char fk_name[FN_REFLEN+1];
      snprintf(fk_name, sizeof(fk_name), "%s",
                  name);
      DBUG_PRINT("info", ("Setting new fk name: %s", fk_name));
      fk->setName(fk_name);
    }

    if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
    {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }

    NdbDictionary::ObjectId objid;
    int err= dict->createForeignKey(*fk, &objid, flags);

    if (child_index)
    {
      dict->removeIndexGlobal(* child_index, 0);
    }

    if (err)
    {
      ERR_RETURN(dict->getNdbError());
    }

    /* Flush the parent table out if parent is different from child */
    char parent_db[FN_LEN + 1];
    const char* parent_name = fk_split_name(parent_db,
                                            fk->getParentTable());
    if(ndb_fk_casecmp(parent_name, tab_name) != 0 ||
       ndb_fk_casecmp(parent_db, ndb->getDatabaseName()) != 0)
    {
      flush_parent_table_for_fk(thd, parent_db, parent_name);
    }
  }
  DBUG_RETURN(0);
}
