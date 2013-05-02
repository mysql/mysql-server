/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

int
ha_ndbcluster::create_fks(THD *thd, Ndb *ndb)
{
  DBUG_ENTER("ha_ndbcluster::create_fks");

  // return real mysql error to avoid total randomness..
  const int err_default= HA_ERR_CANNOT_ADD_FOREIGN;
  char tmpbuf[FN_REFLEN];

  assert(thd->lex != 0);
  Key * key= 0;
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
      const NdbError &error= dict->getNdbError();
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          ER_CANNOT_ADD_FOREIGN,
                          "Parent table %s not found in NDB: %d: %s",
                          parent_name,
                          error.code, error.message);
      DBUG_RETURN(err_default);
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
  }

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
    fk_string.append(parenttab->getName());
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
