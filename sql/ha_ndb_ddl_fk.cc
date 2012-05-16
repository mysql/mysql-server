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

    uint cnt= 0;
    for (unsigned i = 0; columns[i] != 0; i++)
    {
      if (columns[i]->getPrimaryKey())
        cnt++;
    }

    // check if all columns was part of full primary key
    if (cnt == (uint)tab->getNoOfPrimaryKeys())
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

/**
 * ndbapi want's c-strings (null terminated)
 * mysql frequently uses LEX-string...(ptr + len)
 *
 * also...they have changed between 5.1 and 5.5...
 * add a small compability-kit
 */
inline
const char *
lex2str(const LEX_STRING& str, char buf[], size_t len)
{
  my_snprintf(buf, len, "%.*s", str.length, str.str);
  return buf;
}

inline
const char *
lex2str(const char * str, char buf[], size_t len)
{
  return str;
}

inline
bool
isnull(const LEX_STRING& str)
{
  return str.str == 0 || str.length == 0;
}

inline
bool
isnull(const char * str)
{
  return str == 0;
}

int
ha_ndbcluster::create_fks(THD *thd, Ndb *ndb, TABLE *tab)
{
  DBUG_ENTER("ha_ndbcluster::create_fks");

  char tmpbuf[FN_REFLEN];

  /**
   * TODO handle databases too...
   */
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
          DBUG_RETURN(1); // TODO suitable error code
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
      DBUG_RETURN(1); // TODO suitable error code
    }

    char parent_name[FN_REFLEN];
    my_snprintf(parent_name, sizeof(parent_name), "%*s",
                (int)fk->ref_table->table.length,
                fk->ref_table->table.str);
    Ndb_table_guard parent_tab(dict, parent_name);
    if (parent_tab.get_table() == 0)
    {
      ERR_RETURN(dict->getNdbError());
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
          DBUG_RETURN(1); // TODO suitable error code
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

    if (!parent_primary_key && parent_index == 0)
    {
      DBUG_RETURN(1); // TODO suitable error code
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

int
ha_ndbcluster::get_foreign_key_list(THD *thd,
                                    List<FOREIGN_KEY_INFO> * f_key_list)
{
  DBUG_ENTER("ha_ndbcluster::get_foreign_key_list");
  Ndb *ndb= get_ndb(thd);
  if (ndb == 0)
  {
    DBUG_RETURN(0);
  }

  /**
   * TODO
   */

  DBUG_RETURN(0);
}

uint
ha_ndbcluster::referenced_by_foreign_key()
{
  DBUG_ENTER("ha_ndbcluster::referenced_by_foreign_key");

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

  Ndb *ndb= get_ndb(thd);
  if (ndb == 0)
  {
    DBUG_RETURN(0);
  }

  NDBDICT *dict= ndb->getDictionary();
  NDBDICT::List obj_list;
  dict->listDependentObjects(obj_list, *m_table);
  for (unsigned i = 0; i < obj_list.count; i++)
  {
    if (obj_list.elements[i].type == NdbDictionary::Object::ForeignKey)
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

static
const char *
fk_split_name(char dst[], const char * src, bool index = false)
{
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
  return dstptr;
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

    {
      char db_and_name[FN_LEN + 1];
      const char * name = fk_split_name(db_and_name, fk.getParentTable());
      ndb->setDatabaseName(db_and_name);
      parenttab= dict->getTableGlobal(name);
      if (parenttab == 0)
      {
        err= dict->getNdbError();
        goto errout;
      }
    }

    {
      char db_and_name[FN_LEN + 1];
      const char * name = fk_split_name(db_and_name, fk.getChildTable());
      ndb->setDatabaseName(db_and_name);
      childtab= dict->getTableGlobal(name);
      if (childtab == 0)
      {
        err= dict->getNdbError();
        goto errout;
      }
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
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
