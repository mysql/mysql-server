/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "ha_ndbcluster_glue.h"
#include "ha_ndbcluster.h"
#include "ndb_table_guard.h"

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

struct Ndb_fk_item : Sql_alloc
{
  FOREIGN_KEY_INFO f_key_info;
  int update_action;    // NDBFK::FkAction
  int delete_action;
  bool is_child;
  bool is_parent;
};

struct Ndb_fk_data : Sql_alloc
{
  List<Ndb_fk_item> list;
  uint cnt_child;
  uint cnt_parent;
};

// Forward decl
static
const char *
fk_split_name(char dst[], const char * src, bool index= false);

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
    matches_primary_key= FALSE;

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
      matches_primary_key= TRUE;
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
  const NDBINDEX* return_index= 0;

  NDBDICT::List index_list;
  dict->listIndexes(index_list, *tab);
  for (unsigned i = 0; i < index_list.count; i++)
  {
    const char * index_name= index_list.elements[i].name;
    const NDBINDEX* index= dict->getIndexGlobal(index_name, *tab);
    if (index->getType() == NDBINDEX::UniqueHashIndex)
    {
      uint cnt= 0;
      for (unsigned j = 0; columns[j] != 0; j++)
      {
        bool found= FALSE;
        for (unsigned c = 0; c < index->getNoOfColumns(); c++)
        {
          if (!strcmp(columns[j]->getName(), index->getColumn(c)->getName()))
          {
            found= TRUE;
            break;
          }
        }
        if (found)
          cnt++;
        else
          break;
      }
      if (cnt == index->getNoOfColumns())
      {
        /**
         * Full match...
         */
        return_index= index;
        goto found;
      }
      else
      {
        /**
         * Not full match...i.e not usable
         */
        dict->removeIndexGlobal(* index, noinvalidate);
        continue;
      }
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
found:
  if (return_index)
  {
    // release ref to previous best candidate
    if (best_matching_index)
    {
      dict->removeIndexGlobal(* best_matching_index, noinvalidate);
    }
    return return_index; // NOTE: also returns reference
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

struct Ndb_db_guard
{
  Ndb_db_guard(Ndb* ndb) {
    this->ndb = ndb;
    strcpy(save_db, ndb->getDatabaseName());
  }

  void restore() {
    ndb->setDatabaseName(save_db);
  }

  ~Ndb_db_guard() {
    ndb->setDatabaseName(save_db);
  }
private:
  Ndb* ndb;
  char save_db[FN_REFLEN + 1];
};

/**
 * ndbapi want's c-strings (null terminated)
 * mysql frequently uses LEX-string...(ptr + len)
 *
 * also...they have changed between 5.1 and 5.5...
 * add a small compability-kit
 */
static inline
const char *
lex2str(const LEX_STRING& str, char buf[], size_t len)
{
  my_snprintf(buf, len, "%.*s", (int)str.length, str.str);
  return buf;
}

static inline
const char *
lex2str(const char * str, char buf[], size_t len)
{
  return str;
}

static inline
bool
isnull(const LEX_STRING& str)
{
  return str.str == 0 || str.length == 0;
}

static inline
bool
isnull(const char * str)
{
  return str == 0;
}

extern bool ndb_show_foreign_key_mock_tables(THD* thd);

class Fk_util
{
  THD* m_thd;

  void
  info(const char* fmt, ...) const
  {
    va_list args;
    char msg[MYSQL_ERRMSG_SIZE];
    va_start(args,fmt);
    my_vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Push as warning if user has turned on ndb_show_foreign_key_mock_tables
    if (ndb_show_foreign_key_mock_tables(m_thd))
    {
      push_warning(m_thd, Sql_condition::WARN_LEVEL_WARN, ER_YES, msg);
    }

    // Print info to log
    sql_print_information("NDB FK: %s", msg);
  }


  void
  warn(const char* fmt, ...) const
  {
    va_list args;
    char msg[MYSQL_ERRMSG_SIZE];
    va_start(args,fmt);
    my_vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    push_warning(m_thd, Sql_condition::WARN_LEVEL_WARN, ER_CANNOT_ADD_FOREIGN, msg);

    // Print warning to log
    sql_print_warning("NDB FK: %s", msg);
  }


  void
  error(const NdbDictionary::Dictionary* dict, const char* fmt, ...) const
  {
    va_list args;
    char msg[MYSQL_ERRMSG_SIZE];
    va_start(args,fmt);
    my_vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    push_warning(m_thd, Sql_condition::WARN_LEVEL_WARN,
                 ER_CANNOT_ADD_FOREIGN, msg);

    char ndb_msg[MYSQL_ERRMSG_SIZE] = {0};
    if (dict)
    {
      // Extract message from Ndb
      const NdbError& error = dict->getNdbError();
      my_snprintf(ndb_msg, sizeof(ndb_msg),
                  "%d '%s'", error.code, error.message);
      push_warning_printf(m_thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_CANNOT_ADD_FOREIGN, "Ndb error: %s", ndb_msg);
    }
    // Print error to log
    sql_print_error("NDB FK: %s, Ndb error: %s", msg, ndb_msg);
  }


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
      my_snprintf(fk_name, sizeof(fk_name), "%u/%u/%s",
                  new_parent_tab.get_table()->getObjectId(),
                  child_id,
                  name);
      DBUG_PRINT("info", ("Setting new fk name: %s", fk_name));
      new_fk.setName(fk_name);
    }

    // Find matching index
    bool parent_primary_key= FALSE;
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
    if (dict->createForeignKey(new_fk) != 0)
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
            error(NULL, "Could not find column '%s' in mock table '%s'",
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


  void
  resolve_mock_tables(NdbDictionary::Dictionary* dict,
                      const char* new_parent_name) const
  {
    DBUG_ENTER("resolve_mock_tables");
    DBUG_PRINT("enter", ("new_parent_name: %s", new_parent_name));

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


  bool
  create_mock_tables_and_drop(NdbDictionary::Dictionary* dict,
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

      if (strcmp(parent_name, table->getName()) != 0)
      {
        DBUG_PRINT("info", ("fk is not parent, skip"));
        continue;
      }

      DBUG_PRINT("info", ("fk.child: %s", fk.getChildTable()));
      char child_db_and_name[FN_LEN + 1];
      const char * child_name = fk_split_name(child_db_and_name, fk.getChildTable());

      // Open child table
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
            error(NULL, "Could not find column '%s' in parent table '%s'",
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

      // Create new mock
      if (!create(dict, mock_name, child_name,
                  col_names, col_types))
      {
        error(dict, "Failed to create mock parent table");
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
    const size_t len = my_snprintf(buf, buf_size, "NDB$FKM_%d_%u_%s",
                                   child_id, fk_index, parent_name);
    DBUG_PRINT("info", ("len: %lu, buf_size: %lu", len, buf_size));
    if (len >= buf_size - 1)
    {
      DBUG_PRINT("info", ("Size of buffer too small"));
      DBUG_RETURN(NULL);
    }
    DBUG_PRINT("exit", ("buf: '%s', len: %lu", buf, len));
    DBUG_RETURN(buf);
  }


  // Adaptor function for calling create() with List<key_part_spec>
  bool create(NDBDICT *dict, const char* mock_name, const char* child_name,
              List<Key_part_spec> key_part_list, const NDBCOL * col_types[])
  {
    // Convert List<Key_part_spec> into null terminated const char* array
    const char* col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned i = 0;
      Key_part_spec* key = 0;
      List_iterator<Key_part_spec> it1(key_part_list);
      while ((key= it1++))
      {
        char col_name_buf[FN_REFLEN];
        const char* col_name = lex2str(key->field_name, col_name_buf, sizeof(col_name_buf));
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
    mock_tab.setLogging(FALSE);

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

  static
  void resolve_mock_tables(THD* thd, NdbDictionary::Dictionary* dict,
                           const char* new_parent_name)
  {
    Fk_util fk_util(thd);
    fk_util.resolve_mock_tables(dict, new_parent_name);
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

      mock_list.push_back(thd_strdup(m_thd, name));
    }
    DBUG_RETURN(true);
  }


  void
  drop_mock_list(NdbDictionary::Dictionary* dict, List<char> &drop_list)
  {
    const char* tabname;
    List_iterator_fast<char> it(drop_list);
    while ((tabname=it++))
    {
      DBUG_PRINT("info", ("drop table: %s", tabname));
      Ndb_table_guard mocktab_g(dict, tabname);
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
      info("Dropped mock table '%s' - referencing table dropped", tabname);
    }
  }


  bool
  drop(NdbDictionary::Dictionary* dict,
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
    if (!create_mock_tables_and_drop(dict, table))
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
};

bool ndb_fk_util_build_list(THD* thd, NdbDictionary::Dictionary* dict,
                            const NdbDictionary::Table* table, List<char> &mock_list)
{
  Fk_util fk_util(thd);
  return fk_util.build_mock_list(dict, table, mock_list);
}


void ndb_fk_util_drop_list(THD* thd, NdbDictionary::Dictionary* dict, List<char> &drop_list)
{
  Fk_util fk_util(thd);
  fk_util.drop_mock_list(dict, drop_list);
}


bool ndb_fk_util_drop_table(THD* thd, NdbDictionary::Dictionary* dict,
                            const NdbDictionary::Table* table)
{
  Fk_util fk_util(thd);
  return fk_util.drop(dict, table);
}


int
ha_ndbcluster::create_fks(THD *thd, Ndb *ndb)
{
  DBUG_ENTER("ha_ndbcluster::create_fks");

  // return real mysql error to avoid total randomness..
  const int err_default= HA_ERR_CANNOT_ADD_FOREIGN;
  char tmpbuf[FN_REFLEN];

  assert(thd->lex != 0);
  Key * key= 0;
  uint fk_index = 0;
  List_iterator<Key> key_iterator(thd->lex->alter_info.key_list);
  while ((key=key_iterator++))
  {
    if (key->type != Key::FOREIGN_KEY)
      continue;

    NDBDICT *dict= ndb->getDictionary();
    Foreign_key * fk= reinterpret_cast<Foreign_key*>(key);

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
     * Get table columns columns...
     */
    const NDBCOL * childcols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos= 0;
      const NDBTAB * tab= child_tab.get_table();
      Key_part_spec* col= 0;
      List_iterator<Key_part_spec> it1(fk->columns);
      while ((col= it1++))
      {
        const NDBCOL * ndbcol= tab->getColumn(lex2str(col->field_name,
                                                      tmpbuf, sizeof(tmpbuf)));
        if (ndbcol == 0)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_CANNOT_ADD_FOREIGN,
                              "Child table %s has no column %s in NDB",
                              child_tab.get_table()->getName(), tmpbuf);
          DBUG_RETURN(err_default);
        }
        childcols[pos++]= ndbcol;
      }
      childcols[pos]= 0; // NULL terminate
    }

    bool child_primary_key= FALSE;
    const NDBINDEX* child_index= find_matching_index(dict,
                                                     child_tab.get_table(),
                                                     childcols,
                                                     child_primary_key);

    if (!child_primary_key && child_index == 0)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
      my_snprintf(parent_db, sizeof(parent_db), "%*s",
                  (int)fk->ref_db.length,
                  fk->ref_db.str);
    }
    else
    {
      parent_db[0]= 0;
    }
    if (fk->ref_table.str != 0 && fk->ref_table.length != 0)
    {
      my_snprintf(parent_name, sizeof(parent_name), "%*s",
                  (int)fk->ref_table.length,
                  fk->ref_table.str);
    }
    else
    {
      parent_name[0]= 0;
    }
    setDbName(ndb, parent_db);
    Ndb_table_guard parent_tab(dict, parent_name);
    if (parent_tab.get_table() == 0)
    {
       if (!thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS))
       {
         const NdbError &error= dict->getNdbError();
         push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                             ER_CANNOT_ADD_FOREIGN,
                             "Parent table %s not found in NDB: %d: %s",
                             parent_name,
                             error.code, error.message);
         DBUG_RETURN(err_default);
       }

       DBUG_PRINT("info", ("No parent and foreign_key_checks=0"));

       /* Format mock table name */
       char mock_name[FN_REFLEN];
       Fk_util fk_util(thd);
       if (!fk_util.format_name(mock_name, sizeof(mock_name),
                                child_tab.get_table()->getObjectId(),
                                fk_index, parent_name))
       {
         push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                             ER_CANNOT_ADD_FOREIGN,
                             "Failed to create mock parent table, too long mock name");
         DBUG_RETURN(err_default);
       }
       if (!fk_util.create(dict, mock_name, m_tabname,
                           fk->ref_columns, childcols))
       {
         const NdbError &error= dict->getNdbError();
         push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                             ER_CANNOT_ADD_FOREIGN,
                             "Failed to create mock parent table in NDB: %d: %s",
                             error.code, error.message);
         DBUG_RETURN(err_default);
       }

       parent_tab.init(mock_name);
       parent_tab.invalidate(); // invalidate mock table when releasing
       if (parent_tab.get_table() == 0)
       {
         push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
      Key_part_spec* col= 0;
      List_iterator<Key_part_spec> it1(fk->ref_columns);
      while ((col= it1++))
      {
        const NDBCOL * ndbcol= tab->getColumn(lex2str(col->field_name,
                                                      tmpbuf, sizeof(tmpbuf)));
        if (ndbcol == 0)
        {
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_CANNOT_ADD_FOREIGN,
                              "Parent table %s has no column %s in NDB",
                              parent_tab.get_table()->getName(), tmpbuf);
          DBUG_RETURN(err_default);
        }
        parentcols[pos++]= ndbcol;
      }
      parentcols[pos]= 0; // NULL terminate
    }

    bool parent_primary_key= FALSE;
    const NDBINDEX* parent_index= find_matching_index(dict,
                                                      parent_tab.get_table(),
                                                      parentcols,
                                                      parent_primary_key);

    db_guard.restore(); // restore db

    if (!parent_primary_key && parent_index == 0)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_CANNOT_ADD_FOREIGN,
                          "Parent table %s foreign key columns match no index in NDB",
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
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
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
    if (!isnull(fk->name))
    {
      char fk_name[FN_REFLEN];
      my_snprintf(fk_name, sizeof(fk_name), "%u/%u/%s",
                  parent_tab.get_table()->getObjectId(),
                  child_tab.get_table()->getObjectId(),
                  lex2str(fk->name, tmpbuf, sizeof(tmpbuf)));
      ndbfk.setName(fk_name);
    }
    else
    {
      char fk_name[FN_REFLEN];
      my_snprintf(fk_name, sizeof(fk_name), "%u/%u/FK_%u_%u",
                  parent_tab.get_table()->getObjectId(),
                  child_tab.get_table()->getObjectId(),
                  parent_index ?
                  parent_index->getObjectId() :
                  parent_tab.get_table()->getObjectId(),
                  child_index ?
                  child_index->getObjectId() :
                  child_tab.get_table()->getObjectId());
      ndbfk.setName(fk_name);
    }
    ndbfk.setParent(* parent_tab.get_table(), parent_index, parentcols);
    ndbfk.setChild(* child_tab.get_table(), child_index, childcols);

    switch((Foreign_key::fk_option)fk->delete_opt){
    case Foreign_key::FK_OPTION_UNDEF:
    case Foreign_key::FK_OPTION_NO_ACTION:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::NoAction);
      break;
    case Foreign_key::FK_OPTION_RESTRICT:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Restrict);
      break;
    case Foreign_key::FK_OPTION_CASCADE:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Cascade);
      break;
    case Foreign_key::FK_OPTION_SET_NULL:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetNull);
      break;
    case Foreign_key::FK_OPTION_DEFAULT:
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetDefault);
      break;
    default:
      assert(false);
      ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::NoAction);
    }

    switch((Foreign_key::fk_option)fk->update_opt){
    case Foreign_key::FK_OPTION_UNDEF:
    case Foreign_key::FK_OPTION_NO_ACTION:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::NoAction);
      break;
    case Foreign_key::FK_OPTION_RESTRICT:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Restrict);
      break;
    case Foreign_key::FK_OPTION_CASCADE:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Cascade);
      break;
    case Foreign_key::FK_OPTION_SET_NULL:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetNull);
      break;
    case Foreign_key::FK_OPTION_DEFAULT:
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetDefault);
      break;
    default:
      assert(false);
      ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::NoAction);
    }

    int err= dict->createForeignKey(ndbfk);

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
      ERR_RETURN(dict->getNdbError());
    }

    fk_index++;
  }

  Fk_util::resolve_mock_tables(thd, ndb->getDictionary(), m_tabname);

  DBUG_RETURN(0);
}

bool
ha_ndbcluster::is_fk_defined_on_table_or_index(uint index)
{
  /**
   * This doesnt seem implemented in Innodb either...
   */
  return FALSE;
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

  if (m_table == 0)
  {
    DBUG_RETURN(0);
  }

  THD* thd= table->in_use;
  if (thd == 0)
  {
    thd= current_thd;
  }

  if (thd == 0)
  {
    DBUG_RETURN(0);
  }

  // first shot

  LEX *lex= thd->lex;
  DBUG_ASSERT(lex != 0);
  if (lex->sql_command != SQLCOM_ALTER_TABLE)
    DBUG_RETURN(1);

  Alter_info &alter_info= lex->alter_info;
  uint alter_flags= alter_info.flags;

  if (!(alter_flags & Alter_info::ALTER_OPTIONS))
    DBUG_RETURN(1);

  HA_CREATE_INFO &create_info= lex->create_info;
  if (create_info.db_type->db_type == DB_TYPE_NDBCLUSTER)
    DBUG_RETURN(1);

  if (is_child_or_parent_of_fk())
    DBUG_RETURN(0);

  DBUG_RETURN(1);
}

static
const char *
fk_split_name(char dst[], const char * src, bool index)
{
  DBUG_PRINT("info", ("fk_split_name: %s index=%d", src, index));

  /**
   * Split a fully qualified (ndb) name into db and name
   *
   * Store result in dst
   */
  char * dstptr = dst;
  const char * save = src;
  while (src[0] != 0 && src[0] != '/')
  {
    * dstptr = * src;
    dstptr++;
    src++;
  }

  if (src[0] == 0)
  {
    /**
     * No '/' found
     *  set db to ''
     *  and return pointer to name
     *
     * This is for compability with create_fk/drop_fk tools...
     */
    dst[0] = 0;
    strcpy(dst + 1, save);
    DBUG_PRINT("info", ("fk_split_name: %s,%s", dst, dst + 1));
    return dst + 1;
  }

  assert(src[0] == '/');
  src++;
  * dstptr = 0;
  dstptr++;

  // Skip over catalog (not implemented)
  while (src[0] != '/')
  {
    src++;
  }

  assert(src[0] == '/');
  src++;

  /**
   * Indexes contains an extra /
   */
  if (index)
  {
    while (src[0] != '/')
    {
      src++;
    }
    assert(src[0] == '/');
    src++;
  }
  strcpy(dstptr, src);
  DBUG_PRINT("info", ("fk_split_name: %s,%s", dst, dstptr));
  return dstptr;
}

struct Ndb_mem_root_guard {
  Ndb_mem_root_guard(MEM_ROOT *new_root) {
    root_ptr= my_pthread_getspecific(MEM_ROOT**, THR_MALLOC);
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
  init_alloc_root(mem_root, fk_root_block_size, 0);

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
                      data->list.elements, data->cnt_child, data->cnt_parent));

  m_fk_data= data;
  DBUG_RETURN(0);
}

void
ha_ndbcluster::release_fk_data(THD *thd)
{
  DBUG_ENTER("ha_ndbcluster::release_fk_data");

  Ndb_fk_data *data= m_fk_data;
  if (data != 0)
  {
    DBUG_PRINT("info", ("count FKs total %u child %u parent %u",
                        data->list.elements, data->cnt_child, data->cnt_parent));
  }

  MEM_ROOT *mem_root= &m_fk_mem_root;
  free_root(mem_root, 0);
  m_fk_data= 0;

  DBUG_VOID_RETURN;
}

int
ha_ndbcluster::get_child_or_parent_fk_list(THD *thd,
                                           List<FOREIGN_KEY_INFO> * f_key_list,
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
ha_ndbcluster::get_foreign_key_list(THD *thd,
                                    List<FOREIGN_KEY_INFO> * f_key_list)
{
  DBUG_ENTER("ha_ndbcluster::get_foreign_key_list");
  int res= get_child_or_parent_fk_list(thd, f_key_list, true, false);
  DBUG_PRINT("info", ("count FKs child %u", f_key_list->elements));
  DBUG_RETURN(res);
}

int
ha_ndbcluster::get_parent_foreign_key_list(THD *thd,
                                           List<FOREIGN_KEY_INFO> * f_key_list)
{
  DBUG_ENTER("ha_ndbcluster::get_parent_foreign_key_list");
  int res= get_child_or_parent_fk_list(thd, f_key_list, false, true);
  DBUG_PRINT("info", ("count FKs parent %u", f_key_list->elements));
  DBUG_RETURN(res);
}

static
int
cmp_fk_name(const void * _e0, const void * _e1)
{
  const NDBDICT::List::Element * e0 = (NDBDICT::List::Element*)_e0;
  const NDBDICT::List::Element * e1 = (NDBDICT::List::Element*)_e1;
  int res;
  if ((res= strcmp(e0->name, e1->name)) != 0)
    return res;

  if ((res= strcmp(e0->database, e1->database)) != 0)
    return res;

  if ((res= strcmp(e0->schema, e1->schema)) != 0)
    return res;

  return e0->id - e1->id;
}

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
  my_qsort(obj_list.elements, obj_list.count, sizeof(obj_list.elements[0]),
           cmp_fk_name);
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

    NdbError err;
    const int noinvalidate= 0;
    const NDBTAB * childtab= 0;
    const NDBTAB * parenttab= 0;

    char parent_db_and_name[FN_LEN + 1];
    {
      const char * name = fk_split_name(parent_db_and_name,fk.getParentTable());
      setDbName(ndb, parent_db_and_name);
      parenttab= dict->getTableGlobal(name);
      if (parenttab == 0)
      {
        err= dict->getNdbError();
        goto errout;
      }
    }

    char child_db_and_name[FN_LEN + 1];
    {
      const char * name = fk_split_name(child_db_and_name, fk.getChildTable());
      setDbName(ndb, child_db_and_name);
      childtab= dict->getTableGlobal(name);
      if (childtab == 0)
      {
        err= dict->getNdbError();
        goto errout;
      }
    }

    if (! (strcmp(child_db_and_name, m_dbname) == 0 &&
           strcmp(childtab->getName(), m_tabname) == 0))
    {
      /**
       * this was on parent table (fk are shown on child table in SQL)
       */
      assert(strcmp(parent_db_and_name, m_dbname) == 0);
      assert(strcmp(parenttab->getName(), m_tabname) == 0);
      continue;
    }

    fk_string.append(",");
    fk_string.append("\n ");
    fk_string.append("CONSTRAINT `");
    {
      char db_and_name[FN_LEN+1];
      const char * name = fk_split_name(db_and_name, fk.getName());
      fk_string.append(name);
    }
    fk_string.append("` FOREIGN KEY(");

    {
      bool first= true;
      for (unsigned j = 0; j < fk.getChildColumnCount(); j++)
      {
        unsigned no = fk.getChildColumnNo(j);
        if (!first)
        {
          fk_string.append(",");
        }
        fk_string.append("`");
        fk_string.append(childtab->getColumn(no)->getName());
        fk_string.append("`");
        first= false;
      }
    }

    fk_string.append(") REFERENCES `");
    if (strcmp(parent_db_and_name, child_db_and_name) != 0)
    {
      fk_string.append(parent_db_and_name);
      fk_string.append("`.`");
    }


    const char* real_parent_name;
    if (ndb_show_foreign_key_mock_tables(thd) == false &&
        Fk_util::split_mock_name(parenttab->getName(),
                                 NULL, NULL, &real_parent_name))
    {
      DBUG_PRINT("info", ("real_parent_name: %s", real_parent_name));
      fk_string.append(real_parent_name);
    }
    else
    {
      fk_string.append(parenttab->getName());
    }

    fk_string.append("` (");

    {
      bool first= true;
      for (unsigned j = 0; j < fk.getParentColumnCount(); j++)
      {
        unsigned no = fk.getParentColumnNo(j);
        if (!first)
        {
          fk_string.append(",");
        }
        fk_string.append("`");
        fk_string.append(parenttab->getColumn(no)->getName());
        fk_string.append("`");
        first= false;
      }
    }
    fk_string.append(")");

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
errout:
    if (childtab)
    {
      dict->removeTableGlobal(* childtab, noinvalidate);
    }

    if (parenttab)
    {
      dict->removeTableGlobal(* parenttab, noinvalidate);
    }

    if (err.code != 0)
    {
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "Failed to retreive FK information: %d:%s",
                          err.code,
                          err.message);

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
  DBUG_ENTER("copy_fk_for_offline_alter");
  if (thd->lex == 0)
  {
    assert(false);
    DBUG_RETURN(0);
  }

  Ndb_db_guard db_guard(ndb);
  const char * src_db = thd->lex->select_lex.table_list.first->db;
  const char * src_tab = thd->lex->select_lex.table_list.first->table_name;

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
  dict->listDependentObjects(obj_list, *srctab.get_table());
  for (unsigned i = 0; i < obj_list.count; i++)
  {
    if (obj_list.elements[i].type == NdbDictionary::Object::ForeignKey)
    {
      {
        /**
         * Check if it should be copied
         */
        char db_and_name[FN_LEN + 1];
        const char * name= fk_split_name(db_and_name,obj_list.elements[i].name);

        bool found= false;
        Alter_drop * drop_item= 0;
        List_iterator<Alter_drop> drop_iterator(thd->lex->alter_info.drop_list);
        while ((drop_item=drop_iterator++))
        {
          if (drop_item->type != Alter_drop::FOREIGN_KEY)
            continue;
          if (strcmp(drop_item->name, name) == 0)
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

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
      {
        ERR_RETURN(dict->getNdbError());
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
          cols[j]= dsttab.get_table()->getColumn(no);
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
          fk.setParent(* dsttab.get_table(), 0, cols);
        }
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
          cols[j]= dsttab.get_table()->getColumn(no);
        }
        cols[fk.getChildColumnCount()]= 0;
        childObjectId= dsttab.get_table()->getObjectId();
        if (fk.getChildIndex() != 0)
        {
          name = fk_split_name(db_and_name, fk.getChildIndex(), true);
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
          fk.setChild(* dsttab.get_table(), idx, cols);
          dict->removeIndexGlobal(* idx, 0);
        }
        else
        {
          fk.setChild(* dsttab.get_table(), 0, cols);
        }
      }

      char new_name[FN_LEN + 1];
      name= fk_split_name(db_and_name, fk.getName());
      my_snprintf(new_name, sizeof(new_name), "%u/%u/%s",
                  parentObjectId,
                  childObjectId,
                  name);
      fk.setName(new_name);
      setDbName(ndb, db_and_name);
      if (dict->createForeignKey(fk) != 0)
      {
        ERR_RETURN(dict->getNdbError());
      }
    }
  }
  DBUG_RETURN(0);
}

int
ha_ndbcluster::drop_fk_for_online_alter(THD * thd, NDBDICT * dict,
                                        const NDBTAB* tab)
{
  DBUG_ENTER("drop_fk_for_online");
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
  dict->listDependentObjects(obj_list, *srctab.get_table());

  Alter_drop * drop_item= 0;
  List_iterator<Alter_drop> drop_iterator(thd->lex->alter_info.drop_list);
  while ((drop_item=drop_iterator++))
  {
    if (drop_item->type != Alter_drop::FOREIGN_KEY)
      continue;

    bool found= false;
    for (unsigned i = 0; i < obj_list.count && !found; i++)
    {
      if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey)
      {
        continue;
      }

      char db_and_name[FN_LEN + 1];
      const char * name= fk_split_name(db_and_name,obj_list.elements[i].name);

      /*
       * sql_yacc allows "..drop foreign key;" with no FK name
       * (or column list) and passes NULL name here from opt_ident
       *
       * Edit: mysql-5.6 parser now catches this with ER_PARSE_ERROR
       * but we leave the old check here.
       */
      if (drop_item->name == 0)
      {
        my_printf_error(ER_SYNTAX_ERROR,
                        "Drop foreign key must specify key name",
                        MYF(0));
        DBUG_RETURN(1);
      }

      if (strcmp(drop_item->name, name) == 0)
      {
        found= true;
        NdbDictionary::ForeignKey fk;
        if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0)
        {
          ERR_RETURN(dict->getNdbError());
        }
        if (dict->dropForeignKey(fk) != 0)
        {
          ERR_RETURN(dict->getNdbError());
        }
        break;
      }
    }
  }
  DBUG_RETURN(0);
}
