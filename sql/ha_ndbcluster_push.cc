/* Copyright (c) 2000, 2011 Oracle and/or its affiliates. All rights reserved.

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
/**
  @file

  @brief
  This file defines various classes and methods used for pushing queries
  to the ndb data node (for execution by the SPJ block).
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "ha_ndbcluster_glue.h"

#include "rpl_mi.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE

#include "ha_ndbcluster.h"
#include "ha_ndbcluster_push.h"
#include <ndbapi/NdbApi.hpp>
#include "ha_ndbcluster_cond.h"
#include <util/Bitmask.hpp>
#include <ndbapi/NdbIndexStat.hpp>
#include <ndbapi/NdbInterpretedCode.hpp>
#include <ndbapi/NdbQueryBuilder.hpp>
#include <ndbapi/NdbQueryOperation.hpp>

#include "ha_ndbcluster_binlog.h"
#include "ha_ndbcluster_tables.h"
#include "ha_ndbcluster_connection.h"

#include <mysql/plugin.h>
#include <abstract_query_plan.h>
#include <ndb_version.h>


#ifdef ndb_dynamite
#undef assert
#define assert(x) do { if(x) break; ::printf("%s %d: assert failed: %s\n", __FILE__, __LINE__, #x); ::fflush(stdout); ::signal(SIGABRT,SIG_DFL); ::abort(); ::kill(::getpid(),6); ::kill(::getpid(),9); } while (0)
#endif

ndb_pushed_join
::ndb_pushed_join(const AQP::Join_plan& plan, 
                  const ndb_table_access_map& pushed_operations,
                  uint field_refs, Field* const fields[],
                  const NdbQueryDef* query_def)
:
  m_query_def(query_def),
  m_operation_count(0),
  m_field_count(field_refs)
{
  DBUG_ASSERT(query_def != NULL);
  DBUG_ASSERT(field_refs <= MAX_REFERRED_FIELDS);
  ndb_table_access_map searched;
  for (uint i= 0; !(searched==pushed_operations); i++)
  {
    const AQP::Table_access* const join_tab= plan.get_table_access(i);
    ndb_table_access_map table_map(join_tab);
    if (pushed_operations.contain(table_map))
    {
      DBUG_ASSERT(m_operation_count < MAX_PUSHED_OPERATIONS);
      m_tables[m_operation_count++] = join_tab->get_table();
      searched.add(table_map);
    }
  }
  for (uint i= 0; i < field_refs; i++)
  {
    m_referred_fields[i] = fields[i];
  }
}

/**
 * Try to find pushable subsets of a join plan.
 * @param hton unused (maybe useful for other engines).
 * @param thd Thread.
 * @param plan The join plan to examine.
 * @return Possible error code.
 */
int ndbcluster_make_pushed_join(handlerton *hton, THD* thd,
                                AQP::Join_plan* plan)
{
  DBUG_ENTER("ndbcluster_make_pushed_join");
  (void)ha_ndb_ext; // prevents compiler warning.

  if (!ndbcluster_join_pushdown_enabled(thd))
    DBUG_RETURN(0);

  ndb_pushed_builder_ctx context(*plan);
  for (uint i= 0; i < plan->get_access_count()-1; i++)
  {
    const AQP::Table_access* const join_root=  plan->get_table_access(i);
    if (context.is_pushable_as_parent(join_root))
    {
      ha_ndbcluster* const handler=
        static_cast<ha_ndbcluster*>(join_root->get_table()->file);

      int error= handler->make_pushed_join(context,join_root);
      if (unlikely(error))
      {
        handler->print_error(error, MYF(0));
        DBUG_RETURN(error);
      }
    }
  }

  DBUG_RETURN(0);
} // ndbcluster_make_pushed_join

static inline const char* get_referred_field_name(const Item_field* field_item)
{
  DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->field_name;
}

static const char* get_referred_table_access_name(const Item_field* field_item)
{
  DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);
  return field_item->field->table->alias;
}

/**
 * Used by 'explain extended' to explain why an operation could not be pushed.
 * @param[in] msgfmt printf style format string.
 */
void ndbcluster_explain_no_push(const char* msgfmt, ...)
{
  va_list args;
  char wbuff[1024];
  va_start(args,msgfmt);
  (void) my_vsnprintf (wbuff, sizeof(wbuff), msgfmt, args);
  va_end(args);
  push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_NOTE, WARN_QUERY_NOT_PUSHED,
               wbuff);
} // ndbcluster_explain_no_push();

void ndb_pushed_builder_ctx::set_root(const AQP::Table_access* join_root)
{
  DBUG_ASSERT(join_root->get_join_plan() == &m_plan);
  m_join_root= join_root;
  m_join_scope.clear_all();
  m_const_scope.clear_all();

  m_join_scope.add(join_root);
  for (uint i= 0; i<join_root->get_access_no(); i++)
    m_const_scope.add(m_plan.get_table_access(i));
}

void
ndb_pushed_builder_ctx::add_pushed(
                  const AQP::Table_access* table,
                  const AQP::Table_access* parent,
                  const NdbQueryOperationDef* query_op)
{
  uint table_no= table->get_access_no();
  DBUG_ASSERT(table_no < MAX_TABLES);
  m_join_scope.add(table);
  m_tables[table_no].op= query_op;
  m_tables[table_no].m_maybe_pushable= 0; // Exclude from further pushing
  if (likely(parent))
  {
    uint parent_no= parent->get_access_no();

    // Aggregated set of parent and grand...grand...parents to this table
    ndb_table_access_map parent_map(parent);
    DBUG_ASSERT(m_join_scope.contain(parent_map));
    m_tables[table_no].m_parent= parent_no;
    m_tables[table_no].m_ancestors= m_tables[parent_no].m_ancestors;
    m_tables[table_no].m_ancestors.add(parent_map);

    // Maintain which scan operation is the last in a possible
    // linear list of scan operations. 
    if (!ndbcluster_is_lookup_operation(table->get_access_type()))
    {
      while (parent_no != MAX_TABLES)
      {
        m_tables[parent_no].m_last_scan_descendant= table_no;
        parent_no = m_tables[parent_no].m_parent;
      }
    }
  }
  else
  {
    m_tables[table_no].m_ancestors.clear_all();
  }
} // ndb_pushed_builder_ctx::add_pushed

/**
 *  get_referred_table_access()
 *
 * Locate the 'Table_access' object for table with the specified bitmap id
 */
const AQP::Table_access*
ndb_pushed_builder_ctx::get_referred_table_access(const ndb_table_access_map& find_table) const
{
  ndb_table_access_map searched;
  for (uint i= join_root()->get_access_no(); !(searched==m_join_scope); i++)
  {
    const AQP::Table_access* const table= m_plan.get_table_access(i);
    ndb_table_access_map table_map(table);
    if (m_join_scope.contain(table_map))
    { if (find_table==table_map)
        return table;
      searched.add(table_map);
    }
  }
  return NULL;
} // ndb_pushed_builder_ctx::get_referred_table_access

/**
 * Set up the 'm_maybe_pushable' property of each table from the 'Abstract Query Plan' associated
 * with this ndb_pushed_builder_ctx. A table may be possibly pushable as both:
 * PUSHABLE_AS_CHILD and/or PUSHABLE_AS_PARENT.
 * When a table is annotated as not PUSHABLE_AS_... it will be excluded from further
 * pushability investigation for this specific table.
 */
void
ndb_pushed_builder_ctx::init_pushability()
{
  for (uint i= 0; i< m_plan.get_access_count(); i++)
  {
    m_tables[i].m_maybe_pushable= 0;

    const AQP::Table_access* const table_access = m_plan.get_table_access(i);
    if (table_access->get_table()->file->ht != ndbcluster_hton)
    {
      EXPLAIN_NO_PUSH("Table '%s' not in ndb engine, not pushable", 
                      table_access->get_table()->alias);
      continue;
    }

    switch (table_access->get_access_type())
    {
    case AQP::AT_VOID:
      DBUG_ASSERT(false);
      break;

    case AQP::AT_FIXED:
      EXPLAIN_NO_PUSH("Table '%s' was optimized away, or const'ified by optimizer",
                      table_access->get_table()->alias);
      break;

    case AQP::AT_OTHER:
      EXPLAIN_NO_PUSH("Table '%s' is not pushable: %s",
                      table_access->get_table()->alias, 
                      table_access->get_other_access_reason());
      break;

    case AQP::AT_UNDECIDED:
      EXPLAIN_NO_PUSH("Access type for table '%s' will not be chosen before"
                      " execution time and '%s' is therefore not pushable.",
                      table_access->get_table()->alias,
                      table_access->get_table()->alias);
      break;
  
    default:
      m_tables[i].m_maybe_pushable= 
        static_cast<ha_ndbcluster*>(table_access->get_table()->file)
        ->get_pushability();
      break;
    }
  }

  m_tables[0].m_maybe_pushable &= ~PUSHABLE_AS_CHILD;
  m_tables[m_plan.get_access_count()-1].m_maybe_pushable &= ~PUSHABLE_AS_PARENT;
} // ndb_pushed_builder_ctx::init_pushability()


bool
ndb_pushed_builder_ctx::is_pushable_as_parent(const AQP::Table_access* table)
{
  DBUG_ENTER("is_pushable_as_parent");
  uint table_no = table->get_access_no();
  if ((m_tables[table_no].m_maybe_pushable & PUSHABLE_AS_PARENT) != PUSHABLE_AS_PARENT)
  {
    DBUG_PRINT("info", ("Table %d already reported 'not pushable_as_parent'", table_no));
    DBUG_RETURN(false);
  }

  const AQP::enum_access_type access_type= table->get_access_type();
  DBUG_ASSERT(access_type != AQP::AT_VOID);

  if (access_type == AQP::AT_MULTI_UNIQUE_KEY)
  {
    EXPLAIN_NO_PUSH("Table '%s' is not pushable, "
                    "access type 'MULTI_UNIQUE_KEY' not implemented",
                     table->get_table()->alias);
    m_tables[table_no].m_maybe_pushable &= ~PUSHABLE_AS_PARENT;
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
} // ndb_pushed_builder_ctx::is_pushable_as_parent()

/*********************
 * This method examines a key item (could be part of a lookup key or a scan 
 * bound) for a table access operation and calculates the set of possible
 * parents. (These are possible parent table access operations in the query 
 * tree that will be pushed to the ndb.)
 *
 * @param[in] table The table access operation to which the key item belongs.
 * @param[in] key_item The key_item to examine
 * @param[in] key_part_info Metatdata about the key item.
 * @param[out] field_parents The set of possible parents for 'key_item' 
 * ('join_root' if keys are constant).
 * @return True if at least one possible parent was found. (False means that 
 * operation cannot be pushed).
 */
bool ndb_pushed_builder_ctx
::find_field_parents(const AQP::Table_access* table,
                     const Item* key_item, 
                     const KEY_PART_INFO* key_part_info,
                     ndb_table_access_map& field_parents)
{
  DBUG_ENTER("find_field_parents()");
  const uint tab_no = table->get_access_no();

  // TODO: extend to also handle ->const_during_execution() which includes 
  // mysql parameters in addition to contant values/expressions
  if (key_item->const_item())  // ...->const_during_execution() ?
  {
    field_parents= ndb_table_access_map(join_root());
    DBUG_PRINT("info", (" Item type:%d is 'const_item'", key_item->type()));
    DBUG_RETURN(true);
  }
  else if (key_item->type() != Item::FIELD_ITEM)
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                    "column '%s' does neither 'ref' a column nor a constant",
                    table->get_table()->alias,
                    key_part_info->field->field_name);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently disable as child
    DBUG_RETURN(false);
  }

  const Item_field* const key_item_field 
    = static_cast<const Item_field*>(key_item);

  const int key_part_no = key_item - table->get_key_field(0);
  (void)key_part_no; // kill warning

  DBUG_PRINT("info", ("keyPart:%d, field:%s.%s",
              key_part_no, key_item_field->field->table->alias, 
                      key_item_field->field->field_name));

  if (!key_item_field->field
      ->eq_def(key_part_info->field))
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                    "column '%s' does not have same datatype as ref'ed "
                    "column '%s.%s'",
                    table->get_table()->alias,
                    key_part_info->field->field_name,
                    key_item_field->field->table->alias, 
                    key_item_field->field->field_name);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently disable as child
    DBUG_RETURN(false);
  }

  /**
   * Below this point 'key_item_field' is a candidate for refering a parent table
   * in a pushed join. It should either directly refer a parent common to all
   * FIELD_ITEMs, or refer a grandparent of this common parent.
   * There are different cases which should be handled:
   *
   *  1) 'key_item_field' may already refer one of the parent available within our
   *      pushed scope.
   *  2)  By using the equality set, we may find alternative parent references which
   *      may make this a pushed join.
   */

  ///////////////////////////////////////////////////////////////////
  // 0) Prepare for calculating parent candidates for this FIELD_ITEM
  //
  field_parents.clear_all();

  ////////////////////////////////////////////////////////////////////
  // 1) Add our existing parent reference to the set of parent candidates
  //
  const ndb_table_access_map parent_map(key_item_field->used_tables());

  if (m_join_scope.contain(parent_map))
  {
    field_parents.add(parent_map);
  }

  //////////////////////////////////////////////////////////////////
  // 2) Use the equality set to possibly find more parent candidates
  //    usable by substituting existing 'key_item_field'
  //
  AQP::Equal_set_iterator equal_iter(&m_plan, key_item_field);
  const Item_field* substitute_field= equal_iter.next();
  while (substitute_field != NULL)
  {
    if (substitute_field != key_item_field)
    {
      ndb_table_access_map substitute_table(substitute_field->used_tables());
      if (m_join_scope.contain(substitute_table))
      {
        DBUG_PRINT("info", 
                   (" join_items[%d] %s.%s can be replaced with %s.%s",
                    key_part_no,
                    get_referred_table_access_name(key_item_field),
                    get_referred_field_name(key_item_field),
                    get_referred_table_access_name(substitute_field),
                    get_referred_field_name(substitute_field)));

        field_parents.add(substitute_table);
      }
    }
    substitute_field= equal_iter.next();
  } // while(substitute_field != NULL)

  if (!field_parents.is_clear_all())
  {
    DBUG_RETURN(true);
  }

  if (m_const_scope.contain(parent_map))
  {
    // This key item is const. and did not cause the set of possible parents
    // to be recalculated. Reuse what we had before this key item.
    DBUG_ASSERT(field_parents.is_clear_all());
    /** 
     * Scan queries cannot be pushed if the pushed query may refer column 
     * values (paramValues) from rows stored in a join cache.  
     */
    if (!ndbcluster_is_lookup_operation(join_root()->get_access_type()))
    {
      const st_table* const referred_tab = key_item_field->field->table;
      uint access_no = tab_no;
      do
      {
        DBUG_ASSERT(access_no > 0);
        access_no--;
        if (m_plan.get_table_access(access_no)->uses_join_cache())
        {
          EXPLAIN_NO_PUSH("Cannot push table '%s' as child of '%s', since "
                          "it referes to column '%s.%s' which will be stored "
                          "in a join buffer.",
                          table->get_table()->alias, 
                          join_root()->get_table()->alias,
                          get_referred_table_access_name(key_item_field),
                          get_referred_field_name(key_item_field));
          DBUG_RETURN(false);
        }
      } while (m_plan.get_table_access(access_no)->get_table() 
               != referred_tab);

    } // if (!ndbcluster_is_lookup_operation(root_type)
    field_parents= ndb_table_access_map(join_root());
    DBUG_RETURN(true);
  }
  else
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "column '%s.%s' is outside scope of pushable join",
                     table->get_table()->alias, join_root()->get_table()->alias,
                     get_referred_table_access_name(key_item_field),
                     get_referred_field_name(key_item_field));
    DBUG_RETURN(false);
  }
} // ndb_pushed_builder_ctx::find_field_parents()

/***************************************************************
 *  is_pushable_as_child()
 *
 * Determines if the specified child ('table') can be appended to 
 * an existing chain of previously pushed join operations.
 *
 * To be considdered pushable the child operation should:
 *
 *  1) Have an REF to the previous parent operations.
 *  2) Refer only a single parent, or a grandparent reachable through 
 *     a single parent common to all key fields in the 'REF'
 *
 * In order to increase pushability we use the COND_EQUAL sets 
 * to resolve cases (2) above) where multiple parents are refered.
 * If needed too make a child pushable, we replace parent 
 * references with another from the COND_EQUAL sets which make
 * it pushable . The modified join condition is returned in 
 * join_items[] .
 ****************************************************************/
bool
ndb_pushed_builder_ctx::is_pushable_as_child(
                           const AQP::Table_access* table,
                           const Item* join_items[ndb_pushed_join::MAX_LINKED_KEYS+1],
                           const AQP::Table_access*& parent)
{
  DBUG_ENTER("is_pushable_as_child");
  const uint tab_no = table->get_access_no();
  parent= NULL;

  DBUG_ASSERT (join_root() < table);

  if ((m_tables[tab_no].m_maybe_pushable & PUSHABLE_AS_CHILD) != PUSHABLE_AS_CHILD)
  {
    DBUG_PRINT("info", ("Table %s already known 'not is_pushable_as_child'", table->get_table()->alias));
    DBUG_RETURN(false);
  }

  const AQP::enum_access_type access_type= table->get_access_type();
  const AQP::enum_access_type root_type= join_root()->get_access_type();

  if (!(ndbcluster_is_lookup_operation(access_type) || 
        access_type==AQP::AT_ORDERED_INDEX_SCAN))
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, 'type' must be a 'ref' access",
                     table->get_table()->alias);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD;
    DBUG_RETURN(false);
  }
     
  // Currently there is a limitation in not allowing LOOKUP - (index)SCAN operations
  if (access_type==AQP::AT_ORDERED_INDEX_SCAN && 
      ndbcluster_is_lookup_operation(root_type))
  {
    EXPLAIN_NO_PUSH("Push of table '%s' as scan-child "
                    "with lookup-root '%s' not implemented",
                     table->get_table()->alias, join_root()->get_table()->alias);
    // 'table' may still be PUSHABLE_AS_CHILD with another parent
    DBUG_RETURN(false);
  }

  if (access_type==AQP::AT_ORDERED_INDEX_SCAN && join_root()->is_fixed_ordered_index())  
  {
    // root must be an ordered index scan - Thus it cannot have other scan descendant.
    EXPLAIN_NO_PUSH("Push of table '%s' as scan-child "
                    "with ordered indexscan-root '%s' not implemented",
                     table->get_table()->alias, join_root()->get_table()->alias);
    DBUG_RETURN(false);
  }

  if (table->get_no_of_key_fields() > ndb_pushed_join::MAX_LINKED_KEYS)
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                    "to many ref'ed parent fields",
                     table->get_table()->alias);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently dissable
    DBUG_RETURN(false);
  }

  for (uint i = tab_no - 1; i >= join_root()->get_access_no() && i < ~uint(0); 
       i--)
  {
    if (m_plan.get_table_access(i)->uses_join_cache())
    {
      EXPLAIN_NO_PUSH("Cannot push table '%s' as child of table '%s'. Doing so "
                      "would prevent using join buffer for table '%s'.",
                      table->get_table()->alias,
                      join_root()->get_table()->alias,
                      m_plan.get_table_access(i+1)->get_table()->alias);
      DBUG_RETURN(false);
    }
  }

  DBUG_PRINT("info", ("Table:%d, Checking %d REF keys", tab_no, 
                      table->get_no_of_key_fields()));

  ndb_table_access_map current_parents;
  ndb_table_access_map parents(join_root());

  for (uint key_part_no= 0; 
       key_part_no < table->get_no_of_key_fields(); 
       key_part_no++)
  {
    const Item* const key_item= table->get_key_field(key_part_no);
    join_items[key_part_no]= key_item;
    current_parents.add(ndb_table_access_map(key_item->used_tables()));

    /* All parts of the key must be fields in some of the preceeding 
     * tables 
     */
    ndb_table_access_map field_parents;
    if (!find_field_parents(table,
                            key_item,
                            table->get_key_part_info(key_part_no), 
                            field_parents))
    {
      DBUG_RETURN(false);
    }
    /* Now we must merge the set of possible parents for this key with the set
     * of possible parents for all preceding keys.
     */
    ndb_table_access_map new_parents(parents);
    // First find the operations present in both sets.
    new_parents.intersect(field_parents);

    /* Secondly, add each operation which is only in one of the sets, but
     * which is a descendant of some operation in the other set.
     * The SPJ block can handle field references to any ancestor operation,
     * not just the (direct) parent. 
     */
    for (uint parent_no= join_root()->get_access_no(); parent_no < tab_no;
         parent_no++)
    {
      const AQP::Table_access* const parent_candidate= 
        m_plan.get_table_access(parent_no);

      if (parents.contain_table(parent_candidate) && 
          !field_parents.contain_table(parent_candidate) &&
          is_child_of(parent_candidate, field_parents))
      {
        new_parents.add(parent_candidate);
      }
      else if (!parents.contain_table(parent_candidate) && 
          field_parents.contain_table(parent_candidate) &&
          is_child_of(parent_candidate, parents))
      {
        new_parents.add(parent_candidate);
      }
    }
    parents= new_parents;

  } // for (uint key_part_no= 0 ...

  join_items[table->get_no_of_key_fields()]= NULL;

  if (m_const_scope.contain(current_parents))
  {
    // NOTE: This is a constant table wrt. this instance of the pushed join.
    //       It should be relatively simple to extend the SPJ block to 
    //       allow such tables to be included in the pushed join.
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "their dependency is 'const'",
                     table->get_table()->alias, join_root()->get_table()->alias);
    DBUG_RETURN(false);
  }
  else if (parents.is_clear_all())
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "no parents found within scope",
                     table->get_table()->alias, join_root()->get_table()->alias);
    DBUG_RETURN(false);
  }

  DBUG_ASSERT(m_join_scope.contain(parents));

  /**
   * Parent is selected among the set of 'parents'. To improve
   * fanout for lookup operations (bushy joins!) we prefer the most grandparent of the anchestors.
   * As scans can't be bushy currently, we try to serialize these by moving them 'down'.
   */
  uint parent_no;
  if (ndbcluster_is_lookup_operation(table->get_access_type()))
  {
    for (parent_no= join_root()->get_access_no();
         parent_no < tab_no && !parents.contain_table(m_plan.get_table_access(parent_no));
         parent_no++)
    {}
  }
  else // scan operation
  {
    parent_no= tab_no-1;
    while (!parents.contain_table(m_plan.get_table_access(parent_no)))
    {
      DBUG_ASSERT(parent_no > join_root()->get_access_no());
      parent_no--;
    }
    /**
     * If parent already has a scan descendant:
     *   appending 'table' will make this a 'bushy scan' which we don't yet nativily support as a pushed operation.
     *   We can solve this by appending this table after the last existing scan operation in the query.
     *   This cause an artificial grandparent dependency to be created to the actuall parent.
     *
     *  NOTE: This will also force the cross product between rows from these artificial parent to be
     *        created in the SPJ block - Which adds extra (huge) communication overhead.
     *        As a longer term solution bushy scans should be nativily supported by SPJ.
     */

    // 'm_last_scan_descendant' is the candidate to be added as an artificial grandparent.
    if (m_tables[parent_no].m_last_scan_descendant < MAX_TABLES)
    {
      uint descendant_no= m_tables[parent_no].m_last_scan_descendant;

      const AQP::Table_access* const scan_descendant = m_plan.get_table_access(descendant_no);
      parent= m_plan.get_table_access(parent_no);
      if (scan_descendant->get_join_type(parent) == AQP::JT_OUTER_JOIN)
      {
        DBUG_PRINT("info", ("  There are outer joins between parent and artificial parent -> can't append"));
        EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                        "implementation limitations for outer joins",
                        table->get_table()->alias, join_root()->get_table()->alias);
        DBUG_RETURN(false);
      }
      parent_no= descendant_no;
//    parent= scan_descendant;
      DBUG_PRINT("info", ("  Force artificial grandparent dependency through scan-child %s",
                         scan_descendant->get_table()->alias));

      if (scan_descendant && 
         table->get_join_type(scan_descendant) == AQP::JT_OUTER_JOIN)
      {
        EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                        "outer join with scan-descendant '%s' not implemented",
                         table->get_table()->alias,
                         join_root()->get_table()->alias,
                         scan_descendant->get_table()->alias);
        DBUG_RETURN(false);
      }
    }
    else
    {
      // Verify that there are no ancestors with scan descendants. (possibly through lookup operations)
      // (Which would cause an indirect bushy scan to be defined.)
      // Terminate search at first scan ancester, as the presence if this scan guarante that 
      // the tree is non scan-bush above.
      //
      const AQP::Table_access* scan_ancestor= NULL;
      uint ancestor_no= parent_no;
      while (ancestor_no != MAX_TABLES)
      {
        scan_ancestor= m_plan.get_table_access(ancestor_no);
        if (m_tables[ancestor_no].m_last_scan_descendant < MAX_TABLES)
        {
          EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                          "implementation limitations due to bushy scan "
                          "with '%s' indirect through '%s'",
                           table->get_table()->alias,
                           join_root()->get_table()->alias,
                           m_plan.get_table_access(m_tables[ancestor_no].m_last_scan_descendant)->get_table()->alias,
                           scan_ancestor->get_table()->alias);
          DBUG_RETURN(false);
        }

        if (!ndbcluster_is_lookup_operation(scan_ancestor->get_access_type()))
        {
          break; // As adding this scanop was prev. allowed, above ancestor can't be scan bushy
        }
        ancestor_no= m_tables[ancestor_no].m_parent;
      } // while

      // Outer joining scan-scan is not supported due to possible parent-NULL-row duplicates
      // being created in the NdbResultStream when incomplete child batches are received.
      // (Outer joining with scan may be indirect through lookup operations inbetween)
      if (scan_ancestor && 
         table->get_join_type(scan_ancestor) == AQP::JT_OUTER_JOIN)
      {
        EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                        "outer join with scan-ancestor '%s' not implemented",
                         table->get_table()->alias,
                         join_root()->get_table()->alias,
                         scan_ancestor->get_table()->alias);
        DBUG_RETURN(false);
      }
    }
  } // scan operation

     // get_referred_table_access ??
  parent= m_plan.get_table_access(parent_no);
  const ndb_table_access_map parent_map(parent);
//DBUG_ASSERT(parents.contain(parent_map));

  /**
   * If there are any key_fields with 'current_parents' different from
   * our selected 'parent', we have to find substitutes for
   * those key_fields within the equality set.
   **/
  if (!(parent_map==current_parents))
  {
    ndb_table_access_map grandparent_map= m_tables[parent_no].m_ancestors;
    DBUG_ASSERT (!grandparent_map.contain(parent_map));

    for (uint key_part_no= 0; 
         key_part_no < table->get_no_of_key_fields(); 
         key_part_no++)
    {
      DBUG_ASSERT(join_items[key_part_no]->const_item() || 
                  join_items[key_part_no]->type()==Item::FIELD_ITEM);

      if (join_items[key_part_no]->type() == Item::FIELD_ITEM)
      {
        const Item_field* join_item 
          = static_cast<const Item_field*>(join_items[key_part_no]);
        
        ndb_table_access_map used_table(join_item->used_tables());
        if (!(used_table == parent_map))
        {
          AQP::Equal_set_iterator iter(&m_plan, join_item);
          const Item_field* substitute_field= iter.next();
          while (substitute_field != NULL)
          {
            ///////////////////////////////////////////////////////////
            // Prefer to replace join_item with ref. to selected parent.
            //
            ndb_table_access_map substitute_table(substitute_field->used_tables());
            if (substitute_table == parent_map)
            {
              DBUG_PRINT("info", 
                         (" Replacing join_items[%d] %s.%s with %s.%s (parent)",
                          key_part_no,
                          //join_item->field->table->alias,
                          get_referred_table_access_name(join_item),
                          get_referred_field_name(join_item),
                          get_referred_table_access_name(substitute_field),
                          get_referred_field_name(substitute_field)));

              join_items[key_part_no]= join_item= substitute_field;
              break;
            }

            ////////////////////////////////////////////////////////////
            // Second best is to replace join_item with grandparent ref.
            // In this case we will continue to search for a common parent match
            //
            else if (!grandparent_map.contain(used_table) && grandparent_map.contain(substitute_table))
            {
              DBUG_PRINT("info", 
                         (" Replacing join_items[%d] %s.%s with %s.%s (grandparent)",
                          key_part_no,
                          //join_item->field->table->alias,
                          get_referred_table_access_name(join_item),
                          get_referred_field_name(join_item),
                          get_referred_table_access_name(substitute_field),
                          get_referred_field_name(substitute_field)));

              join_items[key_part_no]= join_item= substitute_field;
            }
            substitute_field= iter.next();
          }
        }
      } // Item::FIELD_ITEM
    } // for all 'key_parts'
  } // substitute

  DBUG_RETURN(true);
} // ndb_pushed_builder_ctx::is_pushable_as_child


/**
 * Fill in ix_map[] to map from KEY_PART_INFO[] order into 
 * primary key / unique key order of key fields.
 */
void
ndbcluster_build_key_map(const NDBTAB* table, const NDB_INDEX_DATA& index,
                         const KEY *key_def,
                         uint ix_map[])
{
  uint ix;

  if (index.unique_index_attrid_map) // UNIQUE_ORDERED_INDEX or UNIQUE_INDEX
  {
    for (ix = 0; ix < key_def->key_parts; ix++)
    {
      ix_map[ix]= index.unique_index_attrid_map[ix];
    }
  }
  else  // Primary key does not have a 'unique_index_attrid_map'
  {
    KEY_PART_INFO *key_part;
    uint key_pos= 0;
    int columnnr= 0;
    assert (index.type == PRIMARY_KEY_ORDERED_INDEX || index.type == PRIMARY_KEY_INDEX);

    for (ix = 0, key_part= key_def->key_part; ix < key_def->key_parts; ix++, key_part++)
    {
      // As NdbColumnImpl::m_keyInfoPos isn't available through
      // NDB API we have to calculate it ourself, else we could:
      // ix_map[ix]= table->getColumn(key_part->fieldnr-1)->m_impl.m_keyInfoPos;

      if (key_part->fieldnr < columnnr)
      {
        // PK columns are not in same order as the columns are defined in the table,
        // Restart PK search from first column: 
        key_pos=0;
        columnnr= 0;
      }

      while (columnnr < key_part->fieldnr-1)
      {
        if (table->getColumn(columnnr++)->getPrimaryKey())
           key_pos++;
      }

      assert(table->getColumn(columnnr)->getPrimaryKey());
      ix_map[ix]= key_pos;

      columnnr++;
      key_pos++;
    }
  }
} // build_key_map

#endif // WITH_NDBCLUSTER_STORAGE_ENGINE
