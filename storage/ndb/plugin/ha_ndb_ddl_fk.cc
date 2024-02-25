/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
#include "sql/key_spec.h"
#include "sql/mysqld.h"  // global_system_variables table_alias_charset ...
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "storage/ndb/plugin/ha_ndbcluster.h"
#include "storage/ndb/plugin/ndb_dbname_guard.h"
#include "storage/ndb/plugin/ndb_fk_util.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_schema_trans_guard.h"
#include "storage/ndb/plugin/ndb_table_guard.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "template_utils.h"

#define ERR_RETURN(err)              \
  {                                  \
    const NdbError &tmp = err;       \
    return ndb_to_mysql_error(&tmp); \
  }

// Typedefs for long names
typedef NdbDictionary::Dictionary NDBDICT;
typedef NdbDictionary::Table NDBTAB;
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Index NDBINDEX;
typedef NdbDictionary::ForeignKey NDBFK;

/*
  Create all the fks  for a table.

  The actual foreign keys are not passed in handler interface
  so gets them from thd->lex :-(
*/
static const NDBINDEX *find_matching_index(
    NDBDICT *dict, const NDBTAB *tab, const NDBCOL *columns[],
    /* OUT */ bool &matches_primary_key) {
  /**
   * First check if it matches primary key
   */
  {
    matches_primary_key = false;

    uint cnt_pk = 0, cnt_col = 0;
    for (unsigned i = 0; columns[i] != nullptr; i++) {
      cnt_col++;
      if (columns[i]->getPrimaryKey()) cnt_pk++;
    }

    // check if all columns was part of full primary key
    if (cnt_col == (uint)tab->getNoOfPrimaryKeys() && cnt_col == cnt_pk) {
      matches_primary_key = true;
      return nullptr;
    }
  }

  /**
   * check indexes...
   * first choice is unique index
   * second choice is ordered index...with as many columns as possible
   */
  const int noinvalidate = 0;
  uint best_matching_columns = 0;
  const NDBINDEX *best_matching_index = nullptr;

  NDBDICT::List index_list;
  dict->listIndexes(index_list, *tab);
  for (unsigned i = 0; i < index_list.count; i++) {
    const char *index_name = index_list.elements[i].name;
    const NDBINDEX *index = dict->getIndexGlobal(index_name, *tab);
    if (index->getType() == NDBINDEX::UniqueHashIndex) {
      uint cnt = 0, j;
      for (j = 0; columns[j] != nullptr; j++) {
        /*
         * Search for matching columns in any order
         * since order does not matter for unique index
         */
        bool found = false;
        for (unsigned c = 0; c < index->getNoOfColumns(); c++) {
          if (!strcmp(columns[j]->getName(), index->getColumn(c)->getName())) {
            found = true;
            break;
          }
        }
        if (found)
          cnt++;
        else
          break;
      }
      if (cnt == index->getNoOfColumns() && columns[j] == nullptr) {
        /**
         * Full match...return this index, no need to look further
         */
        if (best_matching_index) {
          // release ref to previous best candidate
          dict->removeIndexGlobal(*best_matching_index, noinvalidate);
        }
        return index;  // NOTE: also returns reference
      }

      /**
       * Not full match...i.e not usable
       */
      dict->removeIndexGlobal(*index, noinvalidate);
      continue;
    } else if (index->getType() == NDBINDEX::OrderedIndex) {
      uint cnt = 0;
      for (; columns[cnt] != nullptr; cnt++) {
        const NDBCOL *ndbcol = index->getColumn(cnt);
        if (ndbcol == nullptr) break;

        if (strcmp(columns[cnt]->getName(), ndbcol->getName()) != 0) break;
      }

      if (cnt > best_matching_columns) {
        /**
         * better match...
         */
        if (best_matching_index) {
          dict->removeIndexGlobal(*best_matching_index, noinvalidate);
        }
        best_matching_index = index;
        best_matching_columns = cnt;
      } else {
        dict->removeIndexGlobal(*index, noinvalidate);
      }
    } else {
      // what ?? unknown index type
      assert(false);
      dict->removeIndexGlobal(*index, noinvalidate);
      continue;
    }
  }

  return best_matching_index;  // NOTE: also returns reference
}

inline static int ndb_fk_casecmp(const char *name1, const char *name2) {
  return my_strcasecmp(files_charset_info, name1, name2);
}

extern bool ndb_show_foreign_key_mock_tables(THD *thd);

class Fk_util {
  THD *const m_thd;

  void info(const char *fmt, ...) const MY_ATTRIBUTE((format(printf, 2, 3)));

  void warn(const char *fmt, ...) const MY_ATTRIBUTE((format(printf, 2, 3)));

  void error(const NdbDictionary::Dictionary *dict, const char *fmt, ...) const
      MY_ATTRIBUTE((format(printf, 3, 4)));

  void remove_index_global(NdbDictionary::Dictionary *dict,
                           const NdbDictionary::Index *index) const {
    if (!index) return;

    dict->removeIndexGlobal(*index, 0);
  }

  bool copy_fk_to_new_parent(Ndb *ndb, NdbDictionary::ForeignKey &fk,
                             const char *new_parent_db,
                             const char *new_parent_name,
                             const char *column_names[]) const {
    DBUG_TRACE;
    DBUG_PRINT("info", ("new_parent_name: %s", new_parent_name));
    NdbDictionary::Dictionary *dict = ndb->getDictionary();

    // Load up the new parent table
    Ndb_table_guard new_parent_tab(ndb, new_parent_db, new_parent_name);
    if (!new_parent_tab.get_table()) {
      error(dict, "Failed to load potentially new parent '%s'",
            new_parent_name);
      return false;
    }

    // Build new parent column list from parent column names
    const NdbDictionary::Column *columns[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned num_columns = 0;
      for (unsigned i = 0; column_names[i] != nullptr; i++) {
        DBUG_PRINT("info", ("column: %s", column_names[i]));
        const NdbDictionary::Column *col =
            new_parent_tab.get_table()->getColumn(column_names[i]);
        if (!col) {
          // Parent table didn't have any column with the given name, can happen
          warn(
              "Could not resolve '%s' as fk parent for '%s' since it didn't "
              "have "
              "all the referenced columns",
              new_parent_name, fk.getChildTable());
          return false;
        }
        columns[num_columns++] = col;
      }
      columns[num_columns] = nullptr;
    }

    NdbDictionary::ForeignKey new_fk(fk);

    // Create name for the new fk by splitting the fk's name and replacing
    // the <parent id> part in format "<parent_id>/<child_id>/<name>"
    {
      char name[FN_REFLEN + 1];
      unsigned parent_id, child_id;
      if (sscanf(fk.getName(), "%u/%u/%s", &parent_id, &child_id, name) != 3) {
        warn("Skip, failed to parse name of fk: %s", fk.getName());
        return false;
      }

      char fk_name[FN_REFLEN + 1];
      snprintf(fk_name, sizeof(fk_name), "%s", name);
      DBUG_PRINT("info", ("Setting new fk name: %s", fk_name));
      new_fk.setName(fk_name);
    }

    // Find matching index
    bool parent_primary_key = false;
    const NdbDictionary::Index *parent_index = find_matching_index(
        dict, new_parent_tab.get_table(), columns, parent_primary_key);
    DBUG_PRINT("info", ("parent_primary_key: %d", parent_primary_key));

    // Check if either pk or index matched
    if (!parent_primary_key && parent_index == nullptr) {
      warn(
          "Could not resolve '%s' as fk parent for '%s' since no matching "
          "index "
          "could be found",
          new_parent_name, fk.getChildTable());
      return false;
    }

    if (parent_index != nullptr) {
      DBUG_PRINT("info",
                 ("Setting parent with index %s", parent_index->getName()));
      new_fk.setParent(*new_parent_tab.get_table(), parent_index, columns);
    } else {
      DBUG_PRINT("info", ("Setting parent without index"));
      new_fk.setParent(*new_parent_tab.get_table(), nullptr, columns);
    }

    // Old fk is dropped by cascading when the mock table is dropped

    // Create new fk referencing the new table
    DBUG_PRINT("info", ("Create new fk: %s", new_fk.getName()));
    int flags = 0;
    if (thd_test_options(m_thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }
    NdbDictionary::ObjectId objid;
    if (dict->createForeignKey(new_fk, &objid, flags) != 0) {
      error(dict, "Failed to create foreign key '%s'", new_fk.getName());
      remove_index_global(dict, parent_index);
      return false;
    }

    remove_index_global(dict, parent_index);
    return true;
  }

  /* Note! Both parent and mock are in same database */
  void resolve_mock(Ndb *ndb, const char *db_name, const char *new_parent_name,
                    const char *mock_name) const {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("mock_name '%s'", mock_name));
    assert(is_mock_name(mock_name));
    NdbDictionary::Dictionary *dict = ndb->getDictionary();

    // Load up the mock table
    Ndb_table_guard mock_tab(ndb, db_name, mock_name);
    if (!mock_tab.get_table()) {
      error(dict, "Failed to load the listed mock table '%s'", mock_name);
      assert(false);
      return;
    }

    // List dependent objects of mock table
    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *mock_tab.get_table()) != 0) {
      error(dict, "Failed to list dependent objects for mock table '%s'",
            mock_name);
      return;
    }

    for (unsigned i = 0; i < list.count; i++) {
      const NdbDictionary::Dictionary::List::Element &element =
          list.elements[i];
      if (element.type != NdbDictionary::Object::ForeignKey) continue;

      DBUG_PRINT("info", ("fk: %s", element.name));

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0) {
        error(dict, "Could not find the listed fk '%s'", element.name);
        continue;
      }

      // Build column name list for parent
      const char *col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      {
        unsigned num_columns = 0;
        for (unsigned j = 0; j < fk.getParentColumnCount(); j++) {
          const NdbDictionary::Column *col =
              mock_tab.get_table()->getColumn(fk.getParentColumnNo(j));
          if (!col) {
            error(nullptr, "Could not find column %d in mock table '%s'",
                  fk.getParentColumnNo(j), mock_name);
            continue;
          }
          col_names[num_columns++] = col->getName();
        }
        col_names[num_columns] = nullptr;

        if (num_columns != fk.getParentColumnCount()) {
          error(
              nullptr,
              "Could not find all columns referenced by fk in mock table '%s'",
              mock_name);
          continue;
        }
      }

      if (!copy_fk_to_new_parent(ndb, fk, db_name, new_parent_name, col_names))
        continue;

      // New fk has been created between child and new parent, drop the mock
      // table and it's related fk
      const int drop_flags = NDBDICT::DropTableCascadeConstraints;
      if (dict->dropTableGlobal(*mock_tab.get_table(), drop_flags) != 0) {
        error(dict, "Failed to drop mock table '%s'", mock_name);
        continue;
      }
      info("Dropped mock table '%s' - resolved by '%s'", mock_name,
           new_parent_name);
    }
    return;
  }

  bool create_mock_tables_and_drop(Ndb *ndb, const char *db_name,
                                   const NdbDictionary::Table *table) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("db_name: %s", db_name));
    DBUG_PRINT("enter", ("table: %s", table->getName()));
    NdbDictionary::Dictionary *dict = ndb->getDictionary();

    // Function creates table in NDB, thus requires dbname to be set
    assert(Ndb_dbname_guard::check_dbname(ndb, db_name));

    /*
      List all foreign keys referencing the table to be dropped
      and recreate those to point at a new mock
    */
    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0) {
      error(dict, "Failed to list dependent objects for table '%s'",
            table->getName());
      return false;
    }

    uint fk_index = 0;
    for (unsigned i = 0; i < list.count; i++) {
      const NdbDictionary::Dictionary::List::Element &element =
          list.elements[i];

      if (element.type != NdbDictionary::Object::ForeignKey) continue;

      DBUG_PRINT("fk", ("name: %s, type: %d", element.name, element.type));

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0) {
        // Could not find the listed fk
        assert(false);
        continue;
      }

      // Parent of the found fk should be the table to be dropped
      DBUG_PRINT("info", ("fk.parent: %s", fk.getParentTable()));
      char parent_db_and_name[FN_LEN + 1];
      const char *parent_name =
          fk_split_name(parent_db_and_name, fk.getParentTable());

      if (strcmp(parent_db_and_name, db_name) != 0 ||
          strcmp(parent_name, table->getName()) != 0) {
        DBUG_PRINT("info", ("fk is not parent, skip"));
        continue;
      }

      DBUG_PRINT("info", ("fk.child: %s", fk.getChildTable()));
      char child_db_and_name[FN_LEN + 1];
      const char *child_name =
          fk_split_name(child_db_and_name, fk.getChildTable());

      // Open child table and check it contains all columns referenced by fk
      Ndb_table_guard child_tab(ndb, child_db_and_name, child_name);
      if (child_tab.get_table() == nullptr) {
        error(dict, "Failed to open child table '%s'", child_name);
        return false;
      }

      /* Format mock table name */
      char mock_name[FN_REFLEN];
      if (!format_name(mock_name, sizeof(mock_name),
                       child_tab.get_table()->getObjectId(), fk_index,
                       parent_name)) {
        error(nullptr,
              "Failed to create mock parent table, too long mock name");
        return false;
      }

      // Build both column name and column type list from parent(which will be
      // dropped)
      const char *col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      const NdbDictionary::Column *col_types[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      {
        unsigned num_columns = 0;
        for (unsigned j = 0; j < fk.getParentColumnCount(); j++) {
          const NdbDictionary::Column *col =
              table->getColumn(fk.getParentColumnNo(j));
          DBUG_PRINT("col", ("[%u] %s", i, col->getName()));
          if (!col) {
            error(nullptr, "Could not find column %d in parent table '%s'",
                  fk.getParentColumnNo(j), table->getName());
            continue;
          }
          col_names[num_columns] = col->getName();
          col_types[num_columns] = col;
          num_columns++;
        }
        col_names[num_columns] = nullptr;
        col_types[num_columns] = nullptr;

        if (num_columns != fk.getParentColumnCount()) {
          error(nullptr,
                "Could not find all columns referenced by fk in parent table "
                "'%s'",
                table->getName());
          continue;
        }
      }

      // Create new mock
      if (!create(dict, mock_name, child_name, col_names, col_types)) {
        error(dict, "Failed to create mock parent table '%s", mock_name);
        assert(false);
        return false;
      }

      // Recreate fks to point at new mock
      if (!copy_fk_to_new_parent(ndb, fk, db_name, mock_name, col_names)) {
        return false;
      }

      fk_index++;
    }

    // Drop the requested table and all foreign keys referring to it
    // i.e the old fks
    const int drop_flags = NDBDICT::DropTableCascadeConstraints;
    if (dict->dropTableGlobal(*table, drop_flags) != 0) {
      error(dict, "Failed to drop the requested table");
      return false;
    }

    return true;
  }

  /**
    @brief Check if the given foreign key name was generated by the server.

    @param table_name  Table name on which the foreign key exists
    @param fk_name     The foreign key name
    @return true if the given name is a generated name. false otherwise.
   */
  bool is_generated_foreign_key_name(const std::string &table_name,
                                     const std::string &fk_name) {
    // MySQL Server versions 8.0.18 and above generate
    // FK names in the form <table_name>_fk_<generated_number>.
    // Check if the given FK name is a generated one.
    DBUG_TRACE;
    std::string generated_fk_name_prefix(table_name);
    generated_fk_name_prefix.append("_fk_");
    // If the fk_name starts with generated_fk_name_prefix and ends with a
    // number, then it is a generated name.
    return (fk_name.compare(0, generated_fk_name_prefix.length(),
                            generated_fk_name_prefix) == 0 &&
            fk_name.substr(generated_fk_name_prefix.length())
                    .find_first_not_of("0123456789") == std::string::npos);
  }

 public:
  Fk_util(THD *thd) : m_thd(thd) {}

  static inline int create_failed(const char *fk_name,
                                  const NdbError &ndb_error) {
    if (ndb_error.code == 721) {
      /* An FK constraint with same name exists */
      my_error(ER_FK_DUP_NAME, MYF(0), fk_name);
      return ER_FK_DUP_NAME;
    } else {
      return ndb_to_mysql_error(&ndb_error);
    }
  }

  static bool split_mock_name(const char *name,
                              unsigned *child_id_ptr = nullptr,
                              unsigned *child_index_ptr = nullptr,
                              const char **parent_name = nullptr) {
    const struct {
      const char *str;
      size_t len;
    } prefix = {STRING_WITH_LEN("NDB$FKM_")};

    if (strncmp(name, prefix.str, prefix.len) != 0) return false;

    char *end;
    const char *ptr = name + prefix.len + 1;

    // Parse child id
    long child_id = strtol(ptr, &end, 10);
    if (ptr == end || child_id < 0 || *end == 0 || *end != '_') return false;
    ptr = end + 1;

    // Parse child index
    long child_index = strtol(ptr, &end, 10);
    if (ptr == end || child_index < 0 || *end == 0 || *end != '_') return false;
    ptr = end + 1;

    // Assign and return OK
    if (child_id_ptr) *child_id_ptr = child_id;
    if (child_index_ptr) *child_index_ptr = child_index;
    if (parent_name) *parent_name = ptr;
    return true;
  }

  static bool is_mock_name(const char *name) { return split_mock_name(name); }

  static const char *format_name(char buf[], size_t buf_size, int child_id,
                                 uint fk_index, const char *parent_name) {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("child_id: %d, fk_index: %u, parent_name: %s",
                         child_id, fk_index, parent_name));
    const size_t len = snprintf(buf, buf_size, "NDB$FKM_%d_%u_%s", child_id,
                                fk_index, parent_name);
    if (len >= buf_size - 1) {
      DBUG_PRINT("info", ("Size of buffer too small"));
      return nullptr;
    }
    DBUG_PRINT("exit", ("buf: '%s'", buf));
    return buf;
  }

  // Adaptor function for calling create() with Mem_root_array<key_part_spec>
  bool create(NDBDICT *dict, const char *mock_name, const char *child_name,
              const Mem_root_array<Key_part_spec *> &key_part_list,
              const NDBCOL *col_types[]) {
    // Convert List<Key_part_spec> into null terminated const char* array
    const char *col_names[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned i = 0;
      for (const Key_part_spec *key : key_part_list) {
        col_names[i++] = strdup(key->get_field_name());
      }
      col_names[i] = nullptr;
    }

    const bool ret = create(dict, mock_name, child_name, col_names, col_types);

    // Free the strings in col_names array
    for (unsigned i = 0; col_names[i] != nullptr; i++) {
      const char *col_name = col_names[i];
      free(const_cast<char *>(col_name));
    }

    return ret;
  }

  bool create(NDBDICT *dict, const char *mock_name, const char *child_name,
              const char *col_names[], const NDBCOL *col_types[]) {
    NDBTAB mock_tab;

    DBUG_TRACE;
    DBUG_PRINT("enter", ("mock_name: %s", mock_name));
    assert(is_mock_name(mock_name));

    if (mock_tab.setName(mock_name)) {
      return false;
    }
    mock_tab.setLogging(false);

    unsigned i = 0;
    while (col_names[i]) {
      NDBCOL mock_col;

      const char *col_name = col_names[i];
      DBUG_PRINT("info", ("name: %s", col_name));
      if (mock_col.setName(col_name)) {
        assert(false);
        return false;
      }

      const NDBCOL *col = col_types[i];
      if (!col) {
        // Internal error, the two lists should be same size
        assert(col);
        return false;
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

      if (mock_tab.addColumn(mock_col)) {
        return false;
      }
      i++;
    }

    // Create the table in NDB
    if (dict->createTable(mock_tab) != 0) {
      // Error is available to caller in dict*
      return false;
    }
    info("Created mock table '%s' referenced by '%s'", mock_name, child_name);
    return true;
  }

  bool build_mock_list(NdbDictionary::Dictionary *dict,
                       const NdbDictionary::Table *table,
                       List<char> &mock_list) {
    DBUG_TRACE;

    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0) {
      error(dict, "Failed to list dependent objects for table '%s'",
            table->getName());
      return false;
    }

    for (unsigned i = 0; i < list.count; i++) {
      const NdbDictionary::Dictionary::List::Element &element =
          list.elements[i];
      if (element.type != NdbDictionary::Object::ForeignKey) continue;

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, element.name) != 0) {
        // Could not find the listed fk
        assert(false);
        continue;
      }

      char parent_db_and_name[FN_LEN + 1];
      const char *name = fk_split_name(parent_db_and_name, fk.getParentTable());

      if (!Fk_util::is_mock_name(name)) continue;

      mock_list.push_back(thd_strdup(m_thd, fk.getParentTable()));
    }
    return true;
  }

  void drop_mock_list(Ndb *ndb, NdbDictionary::Dictionary *dict,
                      List<char> &drop_list) {
    const char *full_name;
    List_iterator_fast<char> it(drop_list);
    while ((full_name = it++)) {
      DBUG_PRINT("info", ("drop table: '%s'", full_name));
      char db_name[FN_LEN + 1];
      const char *table_name = fk_split_name(db_name, full_name);
      Ndb_table_guard mocktab_g(ndb, db_name, table_name);
      if (!mocktab_g.get_table()) {
        // Could not open the mock table
        DBUG_PRINT("error",
                   ("Could not open the listed mock table, ignore it"));
        assert(false);
        continue;
      }

      if (dict->dropTableGlobal(*mocktab_g.get_table()) != 0) {
        DBUG_PRINT("error", ("Failed to drop the mock table '%s'",
                             mocktab_g.get_table()->getName()));
        assert(false);
        continue;
      }
      info("Dropped mock table '%s' - referencing table dropped", table_name);
    }
  }

  bool drop(Ndb *ndb, NdbDictionary::Dictionary *dict, const char *db_name,
            const NdbDictionary::Table *table) {
    DBUG_TRACE;

    // Start schema transaction to make this operation atomic
    if (dict->beginSchemaTrans() != 0) {
      error(dict, "Failed to start schema transaction");
      return false;
    }

    bool result = true;
    if (!create_mock_tables_and_drop(ndb, db_name, table)) {
      // Operation failed, set flag to abort when ending trans
      result = false;
    }

    // End schema transaction
    const Uint32 end_trans_flag =
        result ? 0 : NdbDictionary::Dictionary::SchemaTransAbort;
    if (dict->endSchemaTrans(end_trans_flag) != 0) {
      error(dict, "Failed to end schema transaction");
      result = false;
    }

    return result;
  }

  bool count_fks(NdbDictionary::Dictionary *dict,
                 const NdbDictionary::Table *table, uint &count) const {
    DBUG_TRACE;

    NdbDictionary::Dictionary::List list;
    if (dict->listDependentObjects(list, *table) != 0) {
      error(dict, "Failed to list dependent objects for table '%s'",
            table->getName());
      return false;
    }
    for (unsigned i = 0; i < list.count; i++) {
      if (list.elements[i].type == NdbDictionary::Object::ForeignKey) count++;
    }
    DBUG_PRINT("exit", ("count: %u", count));
    return true;
  }

  bool drop_fk(Ndb *ndb, NdbDictionary::Dictionary *dict, const char *fk_name) {
    DBUG_TRACE;

    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, fk_name) != 0) {
      error(dict, "Could not find fk '%s'", fk_name);
      assert(false);
      return false;
    }

    char parent_db_and_name[FN_LEN + 1];
    const char *parent_name =
        fk_split_name(parent_db_and_name, fk.getParentTable());
    if (Fk_util::is_mock_name(parent_name)) {
      // Fk is referencing a mock table, drop the table
      // and the constraint at the same time
      Ndb_table_guard mocktab_g(ndb, parent_db_and_name, parent_name);
      if (mocktab_g.get_table()) {
        const int drop_flags = NDBDICT::DropTableCascadeConstraints;
        if (dict->dropTableGlobal(*mocktab_g.get_table(), drop_flags) != 0) {
          error(dict, "Failed to drop fk mock table '%s'", parent_name);
          assert(false);
          return false;
        }
        // table and fk dropped
        return true;
      } else {
        warn("Could not open the fk mock table '%s', ignoring it...",
             parent_name);
        assert(false);
        // fallthrough and try to drop only the fk,
      }
    }

    if (dict->dropForeignKey(fk) != 0) {
      error(dict, "Failed to drop fk '%s'", fk_name);
      return false;
    }
    return true;
  }

  void resolve_mock_tables(Ndb *ndb, const char *new_parent_db,
                           const char *new_parent_name) const {
    DBUG_TRACE;
    DBUG_PRINT("enter", ("new_parent_db: %s, new_parent_name: %s",
                         new_parent_db, new_parent_name));

    /*
      List all tables in NDB and look for mock tables which could
      potentially be resolved to the new table
    */
    const NdbDictionary::Dictionary *dict = ndb->getDictionary();
    NdbDictionary::Dictionary::List table_list;
    if (dict->listObjects(table_list, NdbDictionary::Object::UserTable, true) !=
        0) {
      assert(false);
      return;
    }

    for (unsigned i = 0; i < table_list.count; i++) {
      const NdbDictionary::Dictionary::List::Element &el =
          table_list.elements[i];

      assert(el.type == NdbDictionary::Object::UserTable);

      // Check if table is in same database as the potential new parent
      if (strcmp(new_parent_db, el.database) != 0) {
        DBUG_PRINT("info", ("Skip, '%s.%s' is in different database",
                            el.database, el.name));
        continue;
      }

      const char *parent_name;
      if (!Fk_util::split_mock_name(el.name, nullptr, nullptr, &parent_name))
        continue;

      // Check if this mock table should reference the new table
      if (strcmp(parent_name, new_parent_name) != 0) {
        DBUG_PRINT("info",
                   ("Skip, parent of this mock table is not the new table"));
        continue;
      }

      resolve_mock(ndb, new_parent_db, new_parent_name, el.name);
    }

    return;
  }

  /**
    Generate FK info string from the NDBFK object.
    This can be called either by ha_ndbcluster::get_error_message
    or ha_ndbcluster:get_foreign_key_create_info.

    @param    ndb               Pointer to the Ndb Object
    @param    fk                The foreign key object whose info
                                has to be printed.
    @param    tab_id            If this is > 0, the FK is printed only if the
                                table with this table id, is the child table of
                                the passed fk. This is > 0 only if the caller is
                                ha_ndbcluster:get_foreign_key_create_info().
    @param    fk_string         String in which the fk info is to be printed.

    @retval   true              on success
              false             on failure.
  */
  bool generate_fk_constraint_string(Ndb *ndb,
                                     const NdbDictionary::ForeignKey &fk,
                                     const int tab_id, String &fk_string) {
    DBUG_TRACE;

    /* The function generates fk constraint strings for
     * showing fk info in error and in show create table.
     * child_tab_id is non zero only for generating show create info */
    bool generating_for_show_create = (tab_id != 0);

    /* Split parent name and load table */
    char parent_db_and_name[FN_LEN + 1];
    const char *parent_name =
        fk_split_name(parent_db_and_name, fk.getParentTable());
    Ndb_table_guard parent_table_guard(ndb, parent_db_and_name, parent_name);
    const NdbDictionary::Table *parenttab = parent_table_guard.get_table();
    if (parenttab == nullptr) {
      const NdbError &err = parent_table_guard.getNdbError();
      warn("Unable to load parent table : error %d, %s", err.code, err.message);
      return false;
    }

    /* Split child name and load table */
    char child_db_and_name[FN_LEN + 1];
    const char *child_name =
        fk_split_name(child_db_and_name, fk.getChildTable());
    Ndb_table_guard child_table_guard(ndb, child_db_and_name, child_name);
    const NdbDictionary::Table *childtab = child_table_guard.get_table();
    if (childtab == nullptr) {
      const NdbError &err = child_table_guard.getNdbError();
      warn("Unable to load child table : error %d, %s", err.code, err.message);
      return false;
    }

    if (!generating_for_show_create) {
      /* Print child table name if printing error */
      fk_string.append("`");
      fk_string.append(child_db_and_name);
      fk_string.append("`.`");
      fk_string.append(child_name);
      fk_string.append("`, ");
    }

    if (generating_for_show_create) {
      if (childtab->getTableId() != tab_id) {
        /**
         * This was on parent table (fk are shown on child table in SQL)
         * Skip printing this fk
         */
        assert(parenttab->getTableId() == tab_id);
        return true;
      }

      fk_string.append(",");
      fk_string.append("\n  ");
    }

    fk_string.append("CONSTRAINT `");
    {
      char db_and_name[FN_LEN + 1];
      const char *name = fk_split_name(db_and_name, fk.getName());
      fk_string.append(name);
    }
    fk_string.append("` FOREIGN KEY (");

    {
      const char *separator = "";
      for (unsigned j = 0; j < fk.getChildColumnCount(); j++) {
        const int child_col_index = fk.getChildColumnNo(j);
        fk_string.append(separator);
        fk_string.append("`");
        fk_string.append(childtab->getColumn(child_col_index)->getName());
        fk_string.append("`");
        separator = ",";
      }
    }

    fk_string.append(") REFERENCES `");
    if (strcmp(parent_db_and_name, child_db_and_name) != 0) {
      /* Print db name only if the parent and child are from different dbs */
      fk_string.append(parent_db_and_name);
      fk_string.append("`.`");
    }
    const char *real_parent_name;
    if (Fk_util::split_mock_name(parenttab->getName(), nullptr, nullptr,
                                 &real_parent_name)) {
      /* print the real table name */
      DBUG_PRINT("info", ("real_parent_name: %s", real_parent_name));
      fk_string.append(real_parent_name);
    } else {
      fk_string.append(parenttab->getName());
    }

    fk_string.append("` (");
    {
      const char *separator = "";
      for (unsigned j = 0; j < fk.getParentColumnCount(); j++) {
        const int parent_col_index = fk.getParentColumnNo(j);
        fk_string.append(separator);
        fk_string.append("`");
        fk_string.append(parenttab->getColumn(parent_col_index)->getName());
        fk_string.append("`");
        separator = ",";
      }
    }
    fk_string.append(")");

    /* print action strings */
    switch (fk.getOnDeleteAction()) {
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

    switch (fk.getOnUpdateAction()) {
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

    return true;
  }

  /**
     @brief Rename foreign keys with generated names when child is renamed

     @param dict            The NDB Dictionary object
     @param renamed_table   The newly renamed NDB table object
     @param old_table_name  Old table name
     @param new_db_name     New database name
     @param new_table_name  New table name
     @return On success 0, or error code on failure
   */
  int rename_foreign_keys(NdbDictionary::Dictionary *dict,
                          const NdbDictionary::Table *renamed_table,
                          const std::string &old_table_name,
                          const std::string &new_db_name,
                          const std::string &new_table_name) {
    DBUG_TRACE;
    // Loop all foreign keys and rename them if required
    std::vector<NdbDictionary::ForeignKey> fk_list;
    if (!retrieve_foreign_key_list_from_ndb(dict, renamed_table, &fk_list)) {
      ERR_RETURN(dict->getNdbError());
    }

    if (fk_list.empty()) {
      // Nothing to do
      return 0;
    }

    // Start a schema transaction
    Ndb_schema_trans_guard schema_trans(get_thd_ndb(m_thd), dict);
    if (!schema_trans.begin_trans()) {
      return ER_INTERNAL_ERROR;
    }

    for (const NdbDictionary::ForeignKey &fk : fk_list) {
      char child_db_and_name[FN_LEN + 1];
      const char *child_name =
          fk_split_name(child_db_and_name, fk.getChildTable());
      if (new_db_name.compare(child_db_and_name) != 0 ||
          new_table_name.compare(child_name) != 0) {
        // The table being renamed is just a parent of this FK.
        // Skip renaming FK
        continue;
      }

      std::string fk_name;
      {
        char fk_full_name[FN_LEN + 1];
        fk_name.assign(fk_split_name(fk_full_name, fk.getName()));
      }

      if (!is_generated_foreign_key_name(old_table_name, fk_name)) {
        // Not a generated FK name. No need to rename
        continue;
      }

      // Rename FK name
      fk_name.replace(0, old_table_name.length(), new_table_name);
      NdbDictionary::ForeignKey renamed_fk(fk);
      renamed_fk.setName(fk_name.c_str());

      // Create new fk referencing the new table
      DBUG_PRINT("info", ("Create new fk: %s", renamed_fk.getName()));
      int flags = 0;
      if (thd_test_options(m_thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
        flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
      }
      NdbDictionary::ObjectId objid;
      if (dict->createForeignKey(renamed_fk, &objid, flags) != 0) {
        return create_failed(fk_name.c_str(), dict->getNdbError());
      }

      // Drop old FK
      DBUG_PRINT("info", ("Dropping fk: %s", fk.getName()));
      if (dict->dropForeignKey(fk) != 0) {
        ERR_RETURN(dict->getNdbError());
      }
    }

    if (!schema_trans.commit_trans()) {
      return ER_INTERNAL_ERROR;
    }

    return 0;
  }
};

void Fk_util::info(const char *fmt, ...) const {
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // Push as warning if user has turned on ndb_show_foreign_key_mock_tables
  if (ndb_show_foreign_key_mock_tables(m_thd)) {
    push_warning(m_thd, Sql_condition::SL_WARNING, ER_YES, msg);
  }

  // Print info to log
  ndb_log_info("%s", msg);
}

void Fk_util::warn(const char *fmt, ...) const {
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  push_warning(m_thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN, msg);

  // Print warning to log
  ndb_log_warning("%s", msg);
}

void Fk_util::error(const NdbDictionary::Dictionary *dict, const char *fmt,
                    ...) const {
  va_list args;
  char msg[MYSQL_ERRMSG_SIZE];
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  push_warning(m_thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN, msg);

  char ndb_msg[MYSQL_ERRMSG_SIZE] = {0};
  if (dict) {
    // Extract message from Ndb
    const NdbError &error = dict->getNdbError();
    snprintf(ndb_msg, sizeof(ndb_msg), "%d '%s'", error.code, error.message);
    push_warning_printf(m_thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
                        "Ndb error: %s", ndb_msg);
  }
  // Print error to log
  ndb_log_error("%s, Ndb error: %s", msg, ndb_msg);
}

bool ndb_fk_util_build_list(THD *thd, NdbDictionary::Dictionary *dict,
                            const NdbDictionary::Table *table,
                            List<char> &mock_list) {
  Fk_util fk_util(thd);
  return fk_util.build_mock_list(dict, table, mock_list);
}

void ndb_fk_util_drop_list(THD *thd, Ndb *ndb, NdbDictionary::Dictionary *dict,
                           List<char> &drop_list) {
  Fk_util fk_util(thd);
  fk_util.drop_mock_list(ndb, dict, drop_list);
}

bool ndb_fk_util_drop_table(THD *thd, Ndb *ndb, const char *db_name,
                            const NdbDictionary::Table *table) {
  Fk_util fk_util(thd);
  return fk_util.drop(ndb, ndb->getDictionary(), db_name, table);
}

bool ndb_fk_util_is_mock_name(const char *table_name) {
  return Fk_util::is_mock_name(table_name);
}

void ndb_fk_util_resolve_mock_tables(THD *thd, Ndb *ndb,
                                     const char *new_parent_db,
                                     const char *new_parent_name) {
  Fk_util fk_util(thd);
  fk_util.resolve_mock_tables(ndb, new_parent_db, new_parent_name);
}

bool ndb_fk_util_generate_constraint_string(THD *thd, Ndb *ndb,
                                            const NdbDictionary::ForeignKey &fk,
                                            const int tab_id,
                                            String &fk_string) {
  Fk_util fk_util(thd);
  return fk_util.generate_fk_constraint_string(ndb, fk, tab_id, fk_string);
}

int ndb_fk_util_rename_foreign_keys(THD *thd, NdbDictionary::Dictionary *dict,
                                    const NdbDictionary::Table *renamed_table,
                                    const std::string &old_table_name,
                                    const std::string &new_db_name,
                                    const std::string &new_table_name) {
  Fk_util fk_util(thd);
  return fk_util.rename_foreign_keys(dict, renamed_table, old_table_name,
                                     new_db_name, new_table_name);
}

/*
  @brief Guard class for references to indexes in the global
  NdbApi dictionary cache which need to be released(and sometimes
  invalidated) when guard goes out of scope
*/
template <bool invalidate_index>
class Ndb_index_release_guard {
  NdbDictionary::Dictionary *const m_dict;
  std::vector<const NdbDictionary::Index *> m_indexes;

 public:
  Ndb_index_release_guard(NdbDictionary::Dictionary *dict) : m_dict(dict) {}
  Ndb_index_release_guard(const Ndb_index_release_guard &) = delete;
  ~Ndb_index_release_guard() {
    for (const NdbDictionary::Index *index : m_indexes) {
      DBUG_PRINT("info", ("Releasing index: '%s'", index->getName()));
      m_dict->removeIndexGlobal(*index, invalidate_index);
    }
  }
  // Register index to be released
  void add_index_to_release(const NdbDictionary::Index *index) {
    DBUG_PRINT("info", ("Adding index '%s' to release", index->getName()));
    m_indexes.push_back(index);
  }
};

int ha_ndbcluster::create_fks(THD *thd, Ndb *ndb, const char *dbname,
                              const char *tabname) {
  DBUG_TRACE;

  // Calls functions which require dbname
  assert(Ndb_dbname_guard::check_dbname(ndb, dbname));

  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  // Releaser for child(i.e the table being created/altered) which
  // need to be invalidated when released
  Ndb_index_release_guard<true> child_index_releaser(dict);
  // Releaser for parent(i.e the _other_ table) which is not modified
  // and thus need not be invalidated
  Ndb_index_release_guard<false> parent_index_releaser(dict);

  // return real mysql error to avoid total randomness..
  const int err_default = HA_ERR_CANNOT_ADD_FOREIGN;

  assert(thd->lex != nullptr);
  for (const Key_spec *key : thd->lex->alter_info->key_list) {
    if (key->type != KEYTYPE_FOREIGN) continue;

    const Foreign_key_spec *fk = down_cast<const Foreign_key_spec *>(key);

    // Open the table to create foreign keys for
    Ndb_table_guard child_tab(ndb, dbname, tabname);
    if (child_tab.get_table() == nullptr) {
      ERR_RETURN(child_tab.getNdbError());
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
    const NDBCOL *childcols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos = 0;
      const NDBTAB *tab = child_tab.get_table();
      for (const Key_part_spec *col : fk->columns) {
        const NDBCOL *ndbcol = tab->getColumn(col->get_field_name());
        if (ndbcol == nullptr) {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
              "Child table %s has no column %s in NDB",
              child_tab.get_table()->getName(), col->get_field_name());
          return err_default;
        }
        childcols[pos++] = ndbcol;
      }
      childcols[pos] = nullptr;  // NULL terminate
    }

    bool child_primary_key = false;
    const NDBINDEX *child_index = find_matching_index(
        dict, child_tab.get_table(), childcols, child_primary_key);
    if (child_index) {
      child_index_releaser.add_index_to_release(child_index);
    }

    if (!child_primary_key && child_index == nullptr) {
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
          "Child table %s foreign key columns match no index in NDB",
          child_tab.get_table()->getName());
      return err_default;
    }

    char parent_db[FN_REFLEN];
    char parent_name[FN_REFLEN];
    /*
     * Looking at Table_ident, testing for db.str first is safer
     * for valgrind.  Do same with table.str too.
     */
    if (fk->ref_db.str != nullptr && fk->ref_db.length != 0) {
      snprintf(parent_db, sizeof(parent_db), "%*s", (int)fk->ref_db.length,
               fk->ref_db.str);
    } else {
      /* parent db missing - so the db is same as child's */
      snprintf(parent_db, sizeof(parent_db), "%s", dbname);
    }
    if (fk->ref_table.str != nullptr && fk->ref_table.length != 0) {
      snprintf(parent_name, sizeof(parent_name), "%*s",
               (int)fk->ref_table.length, fk->ref_table.str);
    } else {
      parent_name[0] = 0;
    }

    // Switch to parent database, since a mock table might be created
    const Ndb_dbname_guard dbname_guard(ndb, parent_db);

    Ndb_table_guard parent_tab(ndb, parent_db, parent_name);
    if (parent_tab.get_table() == nullptr) {
      if (!thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
        const NdbError &error = parent_tab.getNdbError();
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANNOT_ADD_FOREIGN,
                            "Parent table %s not found in NDB: %d: %s",
                            parent_name, error.code, error.message);
        return err_default;
      }

      DBUG_PRINT("info", ("No parent and foreign_key_checks=0"));

      Fk_util fk_util(thd);

      /* Count the number of existing fks on table */
      uint existing = 0;
      if (!fk_util.count_fks(dict, child_tab.get_table(), existing)) {
        return err_default;
      }

      /* Format mock table name */
      char mock_name[FN_REFLEN];
      if (!fk_util.format_name(mock_name, sizeof(mock_name),
                               child_tab.get_table()->getObjectId(), existing,
                               parent_name)) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
            "Failed to create mock parent table, too long mock name");
        return err_default;
      }
      if (!fk_util.create(dict, mock_name, tabname, fk->ref_columns,
                          childcols)) {
        const NdbError &error = dict->getNdbError();
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_CANNOT_ADD_FOREIGN,
                            "Failed to create mock parent table in NDB: %d: %s",
                            error.code, error.message);
        return err_default;
      }

      parent_tab.init(parent_db, /* mock table is always in same db */
                      mock_name);
      parent_tab.invalidate();  // invalidate mock table when releasing
      if (parent_tab.get_table() == nullptr) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
            "INTERNAL ERROR: Could not find created mock table '%s'",
            mock_name);
        // Internal error, should be able to load the just created mock table
        assert(parent_tab.get_table());
        return err_default;
      }
    }

    const NDBCOL *parentcols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos = 0;
      const NDBTAB *tab = parent_tab.get_table();
      for (const Key_part_spec *col : fk->ref_columns) {
        const NDBCOL *ndbcol = tab->getColumn(col->get_field_name());
        if (ndbcol == nullptr) {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
              "Parent table %s has no column %s in NDB",
              parent_tab.get_table()->getName(), col->get_field_name());
          return err_default;
        }
        parentcols[pos++] = ndbcol;
      }
      parentcols[pos] = nullptr;  // NULL terminate
    }

    bool parent_primary_key = false;
    const NDBINDEX *parent_index = find_matching_index(
        dict, parent_tab.get_table(), parentcols, parent_primary_key);
    if (parent_index) {
      parent_index_releaser.add_index_to_release(parent_index);
    }

    if (!parent_primary_key && parent_index == nullptr) {
      my_error(ER_FK_NO_INDEX_PARENT, MYF(0), fk->name.str ? fk->name.str : "",
               parent_tab.get_table()->getName());
      return err_default;
    }

    {
      /**
       * Check that columns match...this happens to be same
       *   condition as the one for SPJ...
       */
      for (unsigned i = 0; parentcols[i] != nullptr; i++) {
        if (parentcols[i]->isBindable(*childcols[i]) == -1) {
          // Should never happen thanks to SQL-layer doing compatibility check.
          assert(0);
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
              "Parent column %s.%s is incompatible with child column %s.%s in "
              "NDB",
              parent_tab.get_table()->getName(), parentcols[i]->getName(),
              child_tab.get_table()->getName(), childcols[i]->getName());
          return err_default;
        }
      }
    }

    /*
      In 8.0 we rely on SQL-layer to always provide foreign key name, either
      by using the name provided by the user, or by generating a unique name.
      In either case, the name has already been prepared at this point, just
      convert the potentially unterminated string to zero terminated.
    */
    const std::string fk_name(fk->name.str, fk->name.length);

    NdbDictionary::ForeignKey ndbfk;
    ndbfk.setName(fk_name.c_str());
    ndbfk.setParent(*parent_tab.get_table(), parent_index, parentcols);
    ndbfk.setChild(*child_tab.get_table(), child_index, childcols);

    switch ((fk_option)fk->delete_opt) {
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

    switch ((fk_option)fk->update_opt) {
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
    if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }
    NdbDictionary::ObjectId objid;
    const int err = dict->createForeignKey(ndbfk, &objid, flags);
    if (err) {
      return Fk_util::create_failed(ndbfk.getName(), dict->getNdbError());
    }
  }

  ndb_fk_util_resolve_mock_tables(thd, ndb, dbname, tabname);

  return 0;
}

int ha_ndbcluster::copy_fk_for_offline_alter(THD *thd, Ndb *ndb,
                                             const char *dbname,
                                             const char *tabname) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("dbname: '%s', tabname: '%s'", dbname, tabname));

  // This function is called during DDL and should have set dbname already
  assert(Ndb_dbname_guard::check_dbname(ndb, dbname));

  const char *src_db = thd->lex->query_block->get_table_list()->db;
  const char *src_tab = thd->lex->query_block->get_table_list()->table_name;
  if (src_db == nullptr || src_tab == nullptr) {
    assert(false);
    return 0;
  }

  Ndb_table_guard srctab(ndb, src_db, src_tab);
  if (srctab.get_table() == nullptr) {
    // This is a `ALTER TABLE .. ENGINE=NDB` query.
    // srctab exists in a different engine.
    return 0;
  }

  Ndb_table_guard dsttab(ndb, dbname, tabname);
  if (dsttab.get_table() == nullptr) {
    ERR_RETURN(dsttab.getNdbError());
  }

  NDBDICT *dict = ndb->getDictionary();

  Ndb_fk_list srctab_fk_list;
  if (!retrieve_foreign_key_list_from_ndb(dict, srctab.get_table(),
                                          &srctab_fk_list)) {
    ERR_RETURN(dict->getNdbError());
  }

  for (NdbDictionary::ForeignKey &fk : srctab_fk_list) {
    // Extract FK name
    char fk_name_buffer[FN_LEN + 1];
    const char *fk_name = fk_split_name(fk_name_buffer, fk.getName());

    // Extract child name
    char child_db_name[FN_LEN + 1];
    const char *child_table_name =
        fk_split_name(child_db_name, fk.getChildTable());

    // Check if this FK needs to be copied
    bool found = false;
    for (const Alter_drop *drop_item : thd->lex->alter_info->drop_list) {
      if (drop_item->type != Alter_drop::FOREIGN_KEY) continue;
      if (ndb_fk_casecmp(drop_item->name, fk_name) != 0) continue;
      if (strcmp(child_db_name, src_db) == 0 &&
          strcmp(child_table_name, src_tab) == 0) {
        found = true;
        break;
      }
    }
    if (found) {
      // FK is on drop list. Skip copying.
      continue;
    }

    // flags for CreateForeignKey
    int create_fk_flags = 0;

    // Extract parent name
    char parent_db_name[FN_LEN + 1];
    const char *parent_table_name =
        fk_split_name(parent_db_name, fk.getParentTable());

    // Update parent table references and indexes
    // if the table being altered is the parent
    if (strcmp(parent_table_name, src_tab) == 0 &&
        strcmp(parent_db_name, src_db) == 0) {
      // The src_tab is the parent
      const NDBCOL *cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      for (unsigned j = 0; j < fk.getParentColumnCount(); j++) {
        const int parent_col_index = fk.getParentColumnNo(j);
        const NDBCOL *orgcol = srctab.get_table()->getColumn(parent_col_index);
        cols[j] = dsttab.get_table()->getColumn(orgcol->getName());
      }
      cols[fk.getParentColumnCount()] = nullptr;
      if (fk.getParentIndex() != nullptr) {
        const char *parent_index_name =
            fk_split_name(parent_db_name, fk.getParentIndex(), true);
        const NDBINDEX *idx =
            dict->getIndexGlobal(parent_index_name, *dsttab.get_table());
        if (idx == nullptr) {
          ERR_RETURN(dict->getNdbError());
        }
        fk.setParent(*dsttab.get_table(), idx, cols);
        dict->removeIndexGlobal(*idx, 0);
      } else {
        /*
          The parent column was previously the primary key.
          Make sure it still is a primary key as implicit pks
          might change during the alter. If not, get a better
          matching index.
         */
        bool parent_primary = false;
        const NDBINDEX *idx =
            find_matching_index(dict, dsttab.get_table(), cols, parent_primary);
        if (!parent_primary && idx == nullptr) {
          my_error(ER_FK_NO_INDEX_PARENT, MYF(0), fk.getName(),
                   dsttab.get_table()->getName());
          return HA_ERR_CANNOT_ADD_FOREIGN;
        }
        fk.setParent(*dsttab.get_table(), idx, cols);
      }

      /**
       * We're parent, and this is an offline alter table.
       * This foreign key being created cannot be verified
       * as the parent won't have any rows now. The new parent
       * will be populated later during copy data between tables.
       *
       * However, iff the FK is consistent when this alter starts,
       * it should remain consistent since mysql does not
       * allow the alter to modify the columns referenced
       */
      create_fk_flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }

    // Update child table references and indexes
    // if the table being altered is the child
    if (strcmp(child_table_name, src_tab) == 0 &&
        strcmp(child_db_name, src_db) == 0) {
      const NDBCOL *cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      for (unsigned j = 0; j < fk.getChildColumnCount(); j++) {
        const int child_col_index = fk.getChildColumnNo(j);
        const NDBCOL *orgcol = srctab.get_table()->getColumn(child_col_index);
        cols[j] = dsttab.get_table()->getColumn(orgcol->getName());
      }
      cols[fk.getChildColumnCount()] = nullptr;
      if (fk.getChildIndex() != nullptr) {
        bool child_primary_key = false;
        const NDBINDEX *idx = find_matching_index(dict, dsttab.get_table(),
                                                  cols, child_primary_key);
        if (!child_primary_key && idx == nullptr) {
          ERR_RETURN(dict->getNdbError());
        }
        fk.setChild(*dsttab.get_table(), idx, cols);
        if (idx) dict->removeIndexGlobal(*idx, 0);
      } else {
        fk.setChild(*dsttab.get_table(), nullptr, cols);
      }
    }

    // FK's name will have the fully qualified internal name.
    // Reset it to the actual FK name.
    fk.setName(fk_name);

    // The foreign key is on this table (i.e.) this is the child and
    // the foreign key should be consistent even during COPY ALTER.
    // So by default we verify them unless the user has explicitly
    // turned off the foreign key checks variable which might mean that
    // they were never consistent to begin with.
    if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
      create_fk_flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }
    NdbDictionary::ObjectId objid;
    if (dict->createForeignKey(fk, &objid, create_fk_flags) != 0) {
      ERR_RETURN(dict->getNdbError());
    }
  }
  return 0;
}

int ha_ndbcluster::inplace__drop_fks(THD *thd, Ndb *ndb, const char *dbname,
                                     const char *tabname) {
  DBUG_TRACE;
  if (thd->lex == nullptr) {
    assert(false);
    return 0;
  }

  Ndb_table_guard srctab(ndb, dbname, tabname);
  if (srctab.get_table() == nullptr) {
    assert(false);  // Could not find the NDB table being altered
    return 0;
  }

  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  NDBDICT::List obj_list;
  if (dict->listDependentObjects(obj_list, *srctab.get_table()) != 0) {
    ERR_RETURN(dict->getNdbError());
  }

  for (const Alter_drop *drop_item : thd->lex->alter_info->drop_list) {
    if (drop_item->type != Alter_drop::FOREIGN_KEY) continue;

    bool found = false;
    for (unsigned i = 0; i < obj_list.count; i++) {
      if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey) {
        continue;
      }

      char db_and_name[FN_LEN + 1];
      const char *name = fk_split_name(db_and_name, obj_list.elements[i].name);

      if (ndb_fk_casecmp(drop_item->name, name) != 0) continue;

      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, obj_list.elements[i].name) != 0) {
        ERR_RETURN(dict->getNdbError());
      }

      char child_db_and_name[FN_LEN + 1];
      const char *child_name =
          fk_split_name(child_db_and_name, fk.getChildTable());
      if (strcmp(child_db_and_name, dbname) == 0 &&
          strcmp(child_name, tabname) == 0) {
        found = true;
        Fk_util fk_util(thd);
        if (!fk_util.drop_fk(ndb, dict, obj_list.elements[i].name)) {
          ERR_RETURN(dict->getNdbError());
        }

        break;
      }
    }
    if (!found) {
      /*
        Since we check that foreign key to be dropped exists on SQL-layer,
        we should not come here unless there is some bug and data-dictionary
        and NDB internal structures got out of sync.
      */
      assert(false);
      my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0), drop_item->name);
      return ER_CANT_DROP_FIELD_OR_KEY;
    }
  }
  return 0;
}

/**
  Restore foreign keys into the table from fk_list
  For all foreign keys in the given fk list, if the table is a child in the
  foreign key relationship,
  - re-assign child object ids to reflect the newly created child table/indexes.
  - If the table is also the parent, i.e. the foreign key is self referencing,
    additionally re-assign the parent object ids of the foreign key.
  - Recreate the foreign key in the table.
  If the table is a parent in at least one foreign key that is not self
  referencing, resolve all mock tables based on this table to update those
  foreign keys' parent references.

  @retval
    0     ok
  @retval
   != 0   failure in restoring the foreign keys
*/

int ha_ndbcluster::recreate_fk_for_truncate(THD *thd, Ndb *ndb,
                                            const char *db_name,
                                            const char *tab_name,
                                            Ndb_fk_list *fk_list) {
  DBUG_TRACE;

  // Calls functions that requires dbname to be set
  assert(Ndb_dbname_guard::check_dbname(ndb, db_name));

  const int err_default = HA_ERR_CANNOT_ADD_FOREIGN;

  // Fetch the table from NDB
  Ndb_table_guard ndb_table_guard(ndb, db_name, tab_name);
  const NDBTAB *table = ndb_table_guard.get_table();
  if (!table) {
    push_warning_printf(
        thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
        "INTERNAL ERROR: Could not find created child table '%s'", tab_name);
    // Internal error, should be able to load the just created child table
    assert(table);
    return err_default;
  }

  NdbDictionary::Dictionary *dict = ndb->getDictionary();
  bool resolve_mock_tables = false;
  for (NdbDictionary::ForeignKey &fk : *fk_list) {
    DBUG_PRINT("info", ("Parsing foreign key : %s", fk.getName()));
    char child_db[FN_LEN + 1];
    const char *child_name = fk_split_name(child_db, fk.getChildTable());

    if (!(strcmp(db_name, child_db) == 0 &&
          strcmp(tab_name, child_name) == 0)) {
      // Table is just a parent in the foreign key reference. It will be handled
      // later in the end by resolving the mock tables based on this table.
      resolve_mock_tables = true;
      continue;
    }

    /* Get child table columns and index */
    const NDBCOL *child_cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
    {
      unsigned pos = 0;

      for (unsigned i = 0; i < fk.getChildColumnCount(); i++) {
        const NDBCOL *ndbcol = table->getColumn(fk.getChildColumnNo(i));
        if (ndbcol == nullptr) {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
              "Child table %s has no column referred by the FK %s",
              table->getName(), fk.getName());
          assert(ndbcol);
          return err_default;
        }
        child_cols[pos++] = ndbcol;
      }
      child_cols[pos] = nullptr;
    }

    bool child_primary_key = false;
    const NDBINDEX *child_index =
        find_matching_index(dict, table, child_cols, child_primary_key);

    if (!child_primary_key && child_index == nullptr) {
      assert(false);
      my_error(ER_FK_NO_INDEX_CHILD, MYF(0), fk.getName(), table->getName());
      return err_default;
    }

    /* update the fk's child references */
    fk.setChild(*table, child_index, child_cols);

    const NDBINDEX *parent_index = nullptr;
    char parent_db[FN_LEN + 1];
    const char *parent_name = fk_split_name(parent_db, fk.getParentTable());

    if (strcmp(parent_db, child_db) == 0 &&
        strcmp(parent_name, child_name) == 0) {
      const NDBCOL *parent_cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
      {
        /* Self referencing foreign key. Update the parent references*/
        unsigned pos = 0;
        for (unsigned i = 0; i < fk.getParentColumnCount(); i++) {
          const NDBCOL *ndbcol = table->getColumn(fk.getParentColumnNo(i));
          if (ndbcol == nullptr) {
            push_warning_printf(
                thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
                "parent table %s has no column referred by the FK %s",
                table->getName(), fk.getName());
            assert(ndbcol);
            return err_default;
          }
          parent_cols[pos++] = ndbcol;
        }
        parent_cols[pos] = nullptr;
      }

      bool parent_primary_key = false;
      parent_index =
          find_matching_index(dict, table, parent_cols, parent_primary_key);

      if (!parent_primary_key && parent_index == nullptr) {
        assert(false);
        my_error(ER_FK_NO_INDEX_PARENT, MYF(0), fk.getName(), table->getName());
        return err_default;
      }

      /* update the fk's parent references */
      fk.setParent(*table, parent_index, parent_cols);
    }

    /*
     the name of "fk" seems to be different when you read it up
     compared to when you create it. (Probably a historical artifact)
     So update fk's name
    */
    {
      char name[FN_REFLEN + 1];
      unsigned parent_id, child_id;
      if (sscanf(fk.getName(), "%u/%u/%s", &parent_id, &child_id, name) != 3) {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CANNOT_ADD_FOREIGN,
            "Skip, failed to parse name of fk: %s", fk.getName());
        return err_default;
      }

      char fk_name[FN_REFLEN + 1];
      snprintf(fk_name, sizeof(fk_name), "%s", name);
      DBUG_PRINT("info", ("Setting new fk name: %s", fk_name));
      fk.setName(fk_name);
    }

    int flags = 0;
    if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
      flags |= NdbDictionary::Dictionary::CreateFK_NoVerify;
    }

    NdbDictionary::ObjectId objid;
    int err = dict->createForeignKey(fk, &objid, flags);

    if (child_index) {
      dict->removeIndexGlobal(*child_index, 0);
    }

    if (parent_index) {
      dict->removeIndexGlobal(*parent_index, 0);
    }

    if (err) {
      ERR_RETURN(dict->getNdbError());
    }
  }

  if (resolve_mock_tables) {
    // Should happen only when the foreign key checks option is disabled
    assert(thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS));
    // The table was a parent in at least one foreign key relationship that was
    // not self referencing. Update all foreign key definitions referencing the
    // table by resolving all the mock tables based on it.
    ndb_fk_util_resolve_mock_tables(thd, ndb, db_name, tab_name);
  }
  return 0;
}

bool ha_ndbcluster::has_fk_dependency(
    NdbDictionary::Dictionary *dict,
    const NdbDictionary::Column *column) const {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Searching for column %s", column->getName()));
  NdbDictionary::Dictionary::List obj_list;
  if (dict->listDependentObjects(obj_list, *m_table) == 0) {
    for (unsigned i = 0; i < obj_list.count; i++) {
      const NDBDICT::List::Element &e = obj_list.elements[i];
      if (obj_list.elements[i].type != NdbDictionary::Object::ForeignKey) {
        DBUG_PRINT("info", ("skip non-FK %s type %d", e.name, e.type));
        continue;
      }
      DBUG_PRINT("info", ("found FK %s", e.name));
      NdbDictionary::ForeignKey fk;
      if (dict->getForeignKey(fk, e.name) != 0) {
        DBUG_PRINT("error", ("Could not find the listed fk '%s'", e.name));
        continue;
      }
      for (unsigned j = 0; j < fk.getParentColumnCount(); j++) {
        const NdbDictionary::Column *col =
            m_table->getColumn(fk.getParentColumnNo(j));
        DBUG_PRINT("col", ("[%u] %s", i, col->getName()));
        if (col == column) return true;
      }
      for (unsigned j = 0; j < fk.getChildColumnCount(); j++) {
        const NdbDictionary::Column *col =
            m_table->getColumn(fk.getChildColumnNo(j));
        DBUG_PRINT("col", ("[%u] %s", i, col->getName()));
        if (col == column) return true;
      }
    }
  }
  return false;
}
