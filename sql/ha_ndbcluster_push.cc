/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
/**
  @file

  @brief
  This file defines various classes and methods used for pushing queries
  to the ndb data node (for execution by the SPJ block).
*/


#include "sql/ha_ndbcluster_push.h"

#include "my_dbug.h"
#include "sql/abstract_query_plan.h"
#include "sql/current_thd.h"
#include "sql/ha_ndbcluster.h"
#include "sql/ha_ndbcluster_cond.h"
#include "sql/ndb_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "storage/ndb/include/ndb_version.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/ndbapi/NdbInterpretedCode.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryBuilder.hpp"
#include "storage/ndb/src/ndbapi/NdbQueryOperation.hpp"

typedef NdbDictionary::Table NDBTAB;

/*
  Explain why an operation could not be pushed
  @param[in] msgfmt printf style format string.
*/
#define EXPLAIN_NO_PUSH(msgfmt, ...)                              \
do                                                                \
{                                                                 \
  if (unlikely(current_thd->lex->is_explain()))                   \
  {                                                               \
    push_warning_printf(current_thd,                              \
                        Sql_condition::SL_NOTE, ER_YES,           \
                        (msgfmt), __VA_ARGS__);                   \
  }                                                               \
}                                                                 \
while(0)


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

static bool ndbcluster_is_lookup_operation(AQP::enum_access_type accessType)
{
  return accessType == AQP::AT_PRIMARY_KEY ||
    accessType == AQP::AT_MULTI_PRIMARY_KEY ||
    accessType == AQP::AT_UNIQUE_KEY;
}

uint
ndb_table_access_map::first_table(uint start) const
{
  for (uint table_no= start; table_no<length(); table_no++)
  {
    if (contain(table_no))
      return table_no;
  }
  return length();
}

uint
ndb_table_access_map::last_table(uint start) const
{
  uint table_no= start;
  while(true)
  {
    if (contain(table_no))
      return table_no;
    else if (table_no == 0)
      return length();
    table_no--;
  }
}

ndb_pushed_join::ndb_pushed_join(
                  const ndb_pushed_builder_ctx& builder,
                  const NdbQueryDef* query_def)
:
  m_query_def(query_def),
  m_operation_count(0),
  m_field_count(builder.m_fld_refs)
{
  DBUG_ASSERT(query_def != NULL);
  DBUG_ASSERT(builder.m_fld_refs <= MAX_REFERRED_FIELDS);
  ndb_table_access_map searched;
  for (uint tab_no= 0; !(searched==builder.m_join_scope); tab_no++)
  {
    const AQP::Table_access* const join_tab= builder.m_plan.get_table_access(tab_no);
    if (builder.m_join_scope.contain(tab_no))
    {
      DBUG_ASSERT(m_operation_count < MAX_PUSHED_OPERATIONS);
      m_tables[m_operation_count++] = join_tab->get_table();
      searched.add(tab_no);
    }
  }
  for (uint i= 0; i < builder.m_fld_refs; i++)
  {
    m_referred_fields[i] = builder.m_referred_fields[i];
  }
}

ndb_pushed_join::~ndb_pushed_join()
{
  if (m_query_def)
    m_query_def->destroy();
}

bool ndb_pushed_join::match_definition(
                      int type, //NdbQueryOperationDef::Type, 
                      const NDB_INDEX_DATA* idx) const
{
  const NdbQueryOperationDef* const root_operation= 
    m_query_def->getQueryOperation((uint)0);
  const NdbQueryOperationDef::Type def_type=  
    root_operation->getType();

  if (def_type != type)
  {
    DBUG_PRINT("info", 
               ("Cannot execute push join. Root operation prepared as %s "
                "not executable as %s",
                NdbQueryOperationDef::getTypeName(def_type),
                NdbQueryOperationDef::getTypeName((NdbQueryOperationDef::Type)type)));
    return false;
  }
  const NdbDictionary::Index* const expected_index= root_operation->getIndex();

  // Check that we still use the same index as when the query was prepared.
  switch (def_type)
  {
  case NdbQueryOperationDef::PrimaryKeyAccess:
    DBUG_ASSERT(idx!=NULL);
    DBUG_ASSERT(idx->unique_index == expected_index);
    break;

  case NdbQueryOperationDef::UniqueIndexAccess:
    DBUG_ASSERT(idx!=NULL);
    // DBUG_ASSERT(idx->unique_index == expected_index);
    if (idx->unique_index != expected_index)
    {
      DBUG_PRINT("info", ("Actual index %s differs from expected index %s."
                          "Therefore, join cannot be pushed.", 
                          idx->unique_index->getName(),
                          expected_index->getName()));
      return false;
    }
    break;

  case NdbQueryOperationDef::TableScan:
    DBUG_ASSERT (idx==NULL && expected_index==NULL);
    break;

  case NdbQueryOperationDef::OrderedIndexScan:
    DBUG_ASSERT(idx!=NULL);
    // DBUG_ASSERT(idx->index == expected_index);
    if (idx->index != expected_index)
    {
      DBUG_PRINT("info", ("Actual index %s differs from expected index %s. "
                          "Therefore, join cannot be pushed.", 
                          idx->index->getName(),
                          expected_index->getName()));
      return false;
    }
    break;

  default:
    DBUG_ASSERT(false);
    break;
  }

  /**
   * There may be referrences to Field values from tables outside the scope of
   * our pushed join which are supplied as paramValues().
   * If any of these are NULL values, join can't be pushed.
   */
  for (uint i= 0; i < get_field_referrences_count(); i++)
  {
    Field* field= m_referred_fields[i];
    if (field->is_real_null())
    {
      DBUG_PRINT("info", 
                 ("paramValue is NULL, can not execute as pushed join"));
      return false;
    }
  }

  return true;
}

NdbQuery* ndb_pushed_join::make_query_instance(
                       NdbTransaction* trans,
                       const NdbQueryParamValue* keyFieldParams,
                       uint paramCnt) const
{
  DBUG_ENTER("make_query_instance");
  DBUG_PRINT("info", 
             ("executing chain of %d pushed joins."
              " First table is %s, accessed as %s.", 
              get_operation_count(),
              get_table(0)->alias,
              NdbQueryOperationDef::getTypeName(
                m_query_def->getQueryOperation((uint)0)->getType())
             ));

  const NdbQueryParamValue* paramValues = keyFieldParams;

  /**
   * There may be referrences to Field values from tables outside the scope of
   * our pushed join: These are expected to be supplied as paramValues()
   * after the keyFieldParams[]. 
   */
  uint outer_fields= get_field_referrences_count();
  NdbQueryParamValue* extendedParams = NULL;
  if (unlikely(outer_fields > 0))
  {
    uint size= sizeof(NdbQueryParamValue) * (paramCnt+outer_fields);
    extendedParams = reinterpret_cast<NdbQueryParamValue*>(my_alloca(size));
    // Copy specified keyFieldParams[] first
    for (uint i= 0; i < paramCnt; i++)
    {
      new (extendedParams + i) NdbQueryParamValue(keyFieldParams[i]);
    }

    // There may be referrences to Field values from tables outside the scope of
    // our pushed join: These are expected to be supplied as paramValues()
    for (uint i= 0; i < outer_fields; i++)
    {
      Field* field= m_referred_fields[i];
      DBUG_ASSERT(!field->is_real_null());  // Checked by ::check_if_pushable()
      new (extendedParams + paramCnt + i) NdbQueryParamValue(field->ptr, false);
    }
    paramValues= extendedParams;
  }

  NdbQuery* query= trans->createQuery(&get_query_def(), paramValues);
  if (unlikely(extendedParams != NULL))
  {
    for (uint i = 0; i < paramCnt + outer_fields; i++)
    {
      extendedParams[i].~NdbQueryParamValue();
    }
  }
  DBUG_RETURN(query);
}

/////////////////////////////////////////

ndb_pushed_builder_ctx::ndb_pushed_builder_ctx(const AQP::Join_plan& plan)
:
  m_plan(plan),
  m_join_root(),
  m_join_scope(),
  m_const_scope(),
  m_internal_op_count(0),
  m_fld_refs(0),
  m_builder(NULL)
{ 
  const uint count= m_plan.get_access_count();

  DBUG_ASSERT(count <= MAX_TABLES);
  if (count > 1)
  {
    for (uint i= 0; i < count; i++)
    {
      m_tables[i].m_maybe_pushable= 0;

      const AQP::Table_access* const table = m_plan.get_table_access(i);
      if (table->get_table() == NULL)
      {
        // There could be unused tables allocated in the 'plan', skip these
        continue;
      }
      
      if (table->get_table()->s->db_type()->db_type != DB_TYPE_NDBCLUSTER)
      {
        DBUG_PRINT("info", ("Table '%s' not in ndb engine, not pushable", 
                            table->get_table()->alias));
        continue;
      }

      switch (table->get_access_type())
      {
      case AQP::AT_VOID:
        DBUG_ASSERT(false);
        break;

      case AQP::AT_FIXED:
        EXPLAIN_NO_PUSH("Table '%s' was optimized away, or const'ified by optimizer",
                        table->get_table()->alias);
        break;

      case AQP::AT_OTHER:
        EXPLAIN_NO_PUSH("Table '%s' is not pushable: %s",
                        table->get_table()->alias, 
                        table->get_other_access_reason());
        break;

      case AQP::AT_UNDECIDED:
        EXPLAIN_NO_PUSH("Table '%s' is not pushable: "
                        "Access type was not chosen at 'prepare' time",
                        table->get_table()->alias);
        break;
  
      default:
        const char* reason= NULL;
        const ha_ndbcluster* handler=
          static_cast<ha_ndbcluster*>(table->get_table()->file);

        if (handler->maybe_pushable_join(reason))
        {
          m_tables[i].m_maybe_pushable= PUSHABLE_AS_CHILD | PUSHABLE_AS_PARENT;
        }
        else if (reason != NULL)
        {
          EXPLAIN_NO_PUSH("Table '%s' is not pushable: %s",
                          table->get_table()->alias, reason);
        }
        break;
      } //switch
    } //for 'all tables'

    m_tables[0].m_maybe_pushable &= ~PUSHABLE_AS_CHILD;
    m_tables[count-1].m_maybe_pushable &= ~PUSHABLE_AS_PARENT;

#if !defined(NDEBUG)
    // Fill in garbage table enums.
    for (uint i= 0; i < MAX_TABLES; i++)
    {
      m_remap[i].to_external= 0x1111;
      m_remap[i].to_internal= 0x2222;
    }
#endif

    for (uint i= 0; i < count; i++)
    {
      m_remap[i].to_external= MAX_TABLES;
      m_remap[i].to_internal= MAX_TABLES;
    }

    // Fill in table for maping internal <-> external table enumeration
    for (uint i= 0; i < count; i++)
    {
      if (m_tables[i].m_maybe_pushable)
      {
        const AQP::Table_access* const table = m_plan.get_table_access(i);	
        const uint external= table->get_table()->pos_in_table_list->tableno();
        DBUG_ASSERT(external <  MAX_TABLES);

        m_remap[i].to_external= external;
        m_remap[external].to_internal= i;
      }
    }
  }
} // ndb_pushed_builder_ctx::ndb_pushed_builder_ctx()

ndb_pushed_builder_ctx::~ndb_pushed_builder_ctx()
{
  if (m_builder)
  {
    m_builder->destroy();
  }
}

const NdbError& ndb_pushed_builder_ctx::getNdbError() const
{
  DBUG_ASSERT(m_builder);
  return m_builder->getNdbError();
}

/**
 * Get *internal* table_no of table referred by 'key_item'
 */
uint
ndb_pushed_builder_ctx::get_table_no(const Item* key_item) const
{
  DBUG_ASSERT(key_item->type() == Item::FIELD_ITEM);
  const uint count= m_plan.get_access_count();
  table_map bitmap= key_item->used_tables();

  for (uint i= 0; i<count && bitmap!=0; i++, bitmap>>=1)
  {
    if (bitmap & 1)
    {
      DBUG_ASSERT(bitmap == 0x01);  // Only a single table in 'bitmap'
      return m_remap[i].to_internal;
    }
  }
  return MAX_TABLES;
}

/**
 * Main entry point to build a pushed join having 'join_root'
 * as it first operation.
 *
 * If the root operation is pushable, we append as many 'child' 
 * operations as possible to the pushed join.
 *
 * This currently is implemented as a 3 pass algorithm:
 *
 *  1) Analyze each child and add it to 'm_join_scope' as 
 *    'pushable' if it qualifies as such. Part of this phase
 *     is also calculations of possible parents for each table.
 *
 *  2) Determine the parent to be used among the set of possible
 *     parents. This is decided based on simple heuristic where
 *     the goal is to employ filters as soon as possible,  
 *     reduce the fanout of intermediate results, and utilize
 *     the parallelism of the SPJ block whenever considdered optimal.
 *
 *  3) Build the pushed query.
 *
 */
int 
ndb_pushed_builder_ctx::make_pushed_join(
                            const AQP::Table_access* join_root,
                            const ndb_pushed_join* &pushed_join)
{
  DBUG_ENTER("make_pushed_join");
  pushed_join= NULL;

  if (is_pushable_with_root(join_root))
  {
    int error;
    error= optimize_query_plan();
    if (unlikely(error))
      DBUG_RETURN(error);
    
    error= build_query();
    if (unlikely(error))
      DBUG_RETURN(error);

    const NdbQueryDef* const query_def= m_builder->prepare(get_thd_ndb(current_thd)->ndb);
    if (unlikely(query_def == NULL))
      DBUG_RETURN(-1);  // Get error with ::getNdbError()

    pushed_join= new ndb_pushed_join(*this, query_def);
    if (unlikely (pushed_join == NULL))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);

    DBUG_PRINT("info", ("Created pushed join with %d child operations", 
                        pushed_join->get_operation_count()-1));
  }
  DBUG_RETURN(0);
} // ndb_pushed_builder_ctx::make_pushed_join()


/**
 * Find the number SPJ operations needed to execute a given access type.
 * (Unique index lookups are translated to two single table lookups internally.)
 */
uint internal_operation_count(AQP::enum_access_type accessType)
{
  switch (accessType)
  {
  case AQP::AT_PRIMARY_KEY:
  case AQP::AT_ORDERED_INDEX_SCAN:
  case AQP::AT_MULTI_PRIMARY_KEY:
  case AQP::AT_MULTI_MIXED:
  case AQP::AT_TABLE_SCAN:
    return 1;
 
    // Unique key lookups is mapped to two primary key lookups internally.
  case AQP::AT_UNIQUE_KEY:
  case AQP::AT_MULTI_UNIQUE_KEY:
    return 2;

  default:
    // Other access types are not pushable, so seeing them here is an error.
    DBUG_ASSERT(false);
    return 2;
  }
}
 
/**
 * If there is a pushable query starting with 'root'; add as many
 * child operations as possible to this 'ndb_pushed_builder_ctx' starting
 * with that join_root.
 */
bool
ndb_pushed_builder_ctx::is_pushable_with_root(const AQP::Table_access* root)
{
  DBUG_ENTER("is_pushable_with_root");

  const uint root_no= root->get_access_no();
  if ((m_tables[root_no].m_maybe_pushable & PUSHABLE_AS_PARENT) != PUSHABLE_AS_PARENT)
  {
    DBUG_PRINT("info", ("Table %d already reported 'not pushable_as_parent'", root_no));
    DBUG_RETURN(false);
  }

  const AQP::enum_access_type access_type= root->get_access_type();
  DBUG_ASSERT(access_type != AQP::AT_VOID);

  if (access_type == AQP::AT_MULTI_UNIQUE_KEY)
  {
    EXPLAIN_NO_PUSH("Table '%s' is not pushable, "
                    "access type 'MULTI_UNIQUE_KEY' not implemented",
                     root->get_table()->alias);
    m_tables[root_no].m_maybe_pushable &= ~PUSHABLE_AS_PARENT;
    DBUG_RETURN(false);
  }

  if (root->filesort_before_join())
  {
    EXPLAIN_NO_PUSH("Table '%s' is not pushable, "
                    "need filesort before joining child tables",
                     root->get_table()->alias);
    DBUG_RETURN(false);
  }

  /**
   * Past this point we know at least root to be pushable as parent
   * operation. Search remaining tables appendable if '::is_pushable_as_child()'
   */
  DBUG_PRINT("info", ("Table %d is pushable as root", root->get_access_no()));
  DBUG_EXECUTE("info", root->dbug_print(););
  m_fld_refs= 0;
  m_join_root= root;
  m_const_scope.set_prefix(root_no);
  m_join_scope= ndb_table_access_map(root_no);
  m_internal_op_count = internal_operation_count(access_type);

  uint push_cnt= 0;
  for (uint tab_no= root->get_access_no()+1; tab_no<m_plan.get_access_count(); tab_no++)
  {
    const AQP::Table_access* const table= m_plan.get_table_access(tab_no);
    if (is_pushable_as_child(table))
    {
      push_cnt++;
    }
  }
  DBUG_RETURN(push_cnt>0);

} // ndb_pushed_builder_ctx::is_pushable_with_root()


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
 * it pushable .
 ****************************************************************/
bool
ndb_pushed_builder_ctx::is_pushable_as_child(
                           const AQP::Table_access* table)
{
  DBUG_ENTER("is_pushable_as_child");
  const uint root_no= m_join_root->get_access_no();
  const uint tab_no= table->get_access_no();

  DBUG_ASSERT(tab_no > root_no);
  
  if ((m_tables[tab_no].m_maybe_pushable & PUSHABLE_AS_CHILD) != PUSHABLE_AS_CHILD)
  {
    if (table->get_table()) //Possible not a real table at all
    {
      DBUG_PRINT("info", ("Table %s already known 'not is_pushable_as_child'", table->get_table()->alias));
    }
    DBUG_RETURN(false);
  }

  const AQP::enum_access_type root_type= m_join_root->get_access_type();
  const AQP::enum_access_type access_type= table->get_access_type();

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
                     table->get_table()->alias,
                     m_join_root->get_table()->alias);
    // 'table' may still be PUSHABLE_AS_CHILD with another parent
    DBUG_RETURN(false);
  }

  if (table->get_no_of_key_fields() > ndb_pushed_join::MAX_LINKED_KEYS)
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                    "too many ref'ed parent fields",
                     table->get_table()->alias);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently dissable
    DBUG_RETURN(false);
  }

  for (uint i = tab_no; i > root_no; i--)
  {
    if (m_plan.get_table_access(i)->uses_join_cache())
    {
      EXPLAIN_NO_PUSH("Cannot push table '%s' as child of table '%s'. Doing so "
                      "would prevent using join buffer for table '%s'.",
                      table->get_table()->alias,
                      m_join_root->get_table()->alias,
                      m_plan.get_table_access(i)->get_table()->alias);
      DBUG_RETURN(false);
    }
  }

  // Check that we do not exceed the max number of pushable operations.
  const uint internal_ops_needed = internal_operation_count(access_type);
  if (unlikely(m_internal_op_count + internal_ops_needed
               > NDB_SPJ_MAX_TREE_NODES))
  {
    EXPLAIN_NO_PUSH("Cannot push table '%s' as child of '%s'. Max number"
                    " of pushable tables exceeded.",
                    table->get_table()->alias,
                    m_join_root->get_table()->alias);
    DBUG_RETURN(false);
  }
  m_internal_op_count += internal_ops_needed;

  DBUG_PRINT("info", ("Table:%d, Checking %d REF keys", tab_no, 
                      table->get_no_of_key_fields()));

  /*****
   * Calculate the set of possible parents for table, where:
   *  - 'current' are those currently being referred by the 
   *     FIELD_ITEMs as set up by the MySQL optimizer.
   *  - 'common' are those we may refer (possibly through the EQ-sets)
   *     such that all FIELD_ITEMs are from the same parent.
   *  - 'extended' are those parents refered from some of the 
   *     FIELD_ITEMs, and having the rest of the referred FIELD_ITEM 
   *     tables available as 'grandparent refs'
   *     (The SPJ block can handle field references to any ancestor
   *      operation, not just the (direct) parent).
   *
   * In addition there are firm dependencies between some parents
   * such that all 'depend_parents' must be referred as an ancestors
   * of the table. By default 'depend_parents' will at least contain
   * the most 'grandparent' of the extended parents.
   *
   ****/
  ndb_table_access_map current_parents;
  ndb_table_access_map common_parents(m_join_scope);
  ndb_table_access_map extend_parents;
  ndb_table_access_map depend_parents;

  for (uint key_part_no= 0; 
       key_part_no < table->get_no_of_key_fields(); 
       key_part_no++)
  {
    const Item* const key_item= table->get_key_field(key_part_no);
    const KEY_PART_INFO* key_part= table->get_key_part_info(key_part_no);

    if (key_item->const_item()) // REF is a litteral or field from const-table
    {
      DBUG_PRINT("info", (" Item type:%d is 'const_item'", key_item->type()));
      if (!is_const_item_pushable(key_item, key_part))
      {
        DBUG_RETURN(false);
      }
    }
    else if (key_item->type() == Item::FIELD_ITEM)
    {
      /**
       * Calculate all parents FIELD_ITEM may refer - Including those 
       * available through usage of equality sets.
       */
      ndb_table_access_map field_parents;
      if (!is_field_item_pushable(table, key_item, key_part, field_parents))
      {
        DBUG_RETURN(false);
      }

      /**
       * Calculate 'current_parents' as the set of tables
       * currently being referred by some 'key_item'.
       */
      DBUG_ASSERT(key_item == table->get_key_field(key_part_no));
      DBUG_ASSERT(key_item->type() == Item::FIELD_ITEM);
      current_parents.add(get_table_no(key_item));

      /**
       * Calculate 'common_parents' as the set of possible 'field_parents'
       * available from all 'key_part'.
       */
      common_parents.intersect(field_parents);

      /**
       * 'Extended' parents are refered from some 'FIELD_ITEM', and contain
       * all parents directly referred, or available as 'depend_parents'. 
       * The later excludes those before the first (grand-)parent
       * available from all 'field_parents' (first_grandparent).
       * However, it also introduce a dependency of those
       * tables to really be available as grand parents.
       */
      extend_parents.add(field_parents);

      const uint first= field_parents.first_table(root_no);
      depend_parents.add(first);
    }
    else
    {
      EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                      "column '%s' does neither 'ref' a column nor a constant",
                      table->get_table()->alias,
                      key_part->field->field_name);
      m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently disable as child
      DBUG_RETURN(false);
    }
  } // for (uint key_part_no= 0 ...

  if (m_const_scope.contain(current_parents))
  {
    // NOTE: This is a constant table wrt. this instance of the pushed join.
    //       It should be relatively simple to extend the SPJ block to 
    //       allow such tables to be included in the pushed join.
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "their dependency is 'const'",
                     table->get_table()->alias,
                     m_join_root->get_table()->alias);
    DBUG_RETURN(false);
  }
  else if (extend_parents.is_clear_all())
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "no parents found within scope",
                     table->get_table()->alias,
                     m_join_root->get_table()->alias);
    DBUG_RETURN(false);
  }

  if (!ndbcluster_is_lookup_operation(table->get_access_type()))
  {
    /**
     * Outer joining scan-scan is not supported, due to the following problem:
     * Consider the query:
     *
     * select * from t1 left join t2 
     *   on t1.attr=t2.ordered_index
     *   where predicate(t1.row, t2. row);
     *
     * Where 'predicate' cannot be pushed to the ndb. The ndb api may then
     * return:
     * +---------+---------+
     * | t1.row1 | t2.row1 | (First batch)
     * | t1.row2 | t2.row1 | 
     * ..... (NextReq).....
     * | t1.row1 | t2.row2 | (Next batch)
     * +---------+---------+
     * Now assume that all rows but [t1.row1, t2.row1] satisfies 'predicate'.
     * mysqld would be confused since the rows are not grouped on t1 values.
     * It would therefor generate a NULL row such that it returns:
     * +---------+---------+
     * | t1.row1 | NULL    | -> Error! 
     * | t1.row2 | t2.row1 | 
     * | t1.row1 | t2.row2 |
     * +---------+---------+
     * 
     * (Outer joining with scan may be indirect through lookup operations 
     * inbetween)
     */
    const AQP::enum_join_type join_type = table->get_join_type(m_join_root);
    if (join_type == AQP::JT_OUTER_JOIN)
    {
      EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                      "outer join of scan-child not implemented",
                       table->get_table()->alias,
                       m_join_root->get_table()->alias);
      DBUG_RETURN(false);
    }

    /**
     * As for outer joins, there are similar scan-scan restrictions
     * for semi joins:
     *
     * Scan-scan result may return the same ancestor-scan rowset
     * multiple times when rowset from child scan has to be fetched
     * in multiple batches (as above). This is fine for nested loop
     * evaluations of pure loops as it should just produce the total
     * set of join combinations - in any order.
     *
     * However, the different semi join strategies (FirstMatch,
     * Loosescan, Duplicate Weedout) requires that skipping
     * a row (and its nested loop ancestors) is 'permanent' such
     * that it will never reappear later in later batches.
     */
    if (join_type == AQP::JT_SEMI_JOIN)
    {
      EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                      "semi join of scan-child not implemented",
                       table->get_table()->alias,
                       m_join_root->get_table()->alias);
      DBUG_RETURN(false);
    }

    /**
     * 'JT_NEST_JOIN' is returned if 'table' is (inner-)joined
     * with a root in a different 'nest' (nest: A group of nested-loop
     * inner joined tables).
     * This has the same scan-scan restriction as described above.
     */
    if (join_type == AQP::JT_NEST_JOIN)
    {
      EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                      "not members of same join 'nest'",
                       table->get_table()->alias,
                       m_join_root->get_table()->alias);
      DBUG_RETURN(false);
    }

    /**
     * Note, for both 'outer join', and 'semi joins restriction above:
     *
     * The restriction could have been lifted if we could
     * somehow ensure that all rows from a child scan are fetched
     * before we move to the next ancestor row.
     *
     * Which is why we do not force the same restrictions on lookup.
     */
    
  } // scan operation

  /**
   * Check outer join restrictions if multiple 'depend_parents':
   *
   * If this table has multiple dependencies, it can only be added to 
   * the set of pushed tables if the dependent tables themself
   * depends, or could be make dependent, on each other.
   *
   * Such new dependencies can only be added iff all 'depend_parents'
   * are in the same 'inner join nest', i.e. we can not add *new*
   * dependencies on outer joined tables. 
   * Any existing explicit specified outer joins are allowed though.
   * Another way to view this is that the explained query plan
   * should have no outer joins inbetween the table and the tables
   * it joins with.
   *
   * Algorithm:
   * 1. Calculate a single 'common ancestor' for all dependent tables
   *    which is the closest ancestor which they all depends on.
   *    (directly or indirectly through other dependencies.)
   *
   * 2. For all ancestors in the 'depend_parents' set:
   *    If none of the children of this  ancestor are already 'joined'
   *    with this ancestor, (eiter directly or indirectly) it need
   *    to be in an inner join relation with our common ancestor.
   */

  DBUG_ASSERT(!depend_parents.is_clear_all());
  DBUG_ASSERT(!depend_parents.contain(tab_no)); // Circular dependency!

  ndb_table_access_map dependencies(depend_parents);

  /**
   * Calculate the single 'common ancestor' of all 'depend_parents'.
   * Iterating all directly, and indirectly, 'depend_parents'
   * until a single dependent ancestor remains.
   */
  uint common_ancestor_no= tab_no;
  while (true)
  {
    common_ancestor_no= dependencies.last_table(common_ancestor_no-1);
    dependencies.clear_bit(common_ancestor_no);
    if (dependencies.is_clear_all())
      break;

    const ndb_table_access_map &ancestor_dependencies= 
      m_tables[common_ancestor_no].m_depend_parents;
    const uint first_ancestor=
       ancestor_dependencies.last_table(common_ancestor_no-1);
    dependencies.add(first_ancestor);
  } //while

  const AQP::Table_access* const common_ancestor= 
    m_plan.get_table_access(common_ancestor_no);

  /**
   * Check that no dependencies on outer joined 'common ancestor'
   * need to be added in order to allow this new child to be joined.
   */
  ndb_table_access_map child_dependencies;
  dependencies= depend_parents;

  for (uint ancestor_no= dependencies.last_table(tab_no-1);
       ancestor_no!= common_ancestor_no;
       ancestor_no= dependencies.last_table(ancestor_no-1))
  {
    const AQP::Table_access* const ancestor=
      m_plan.get_table_access(ancestor_no);

    /**
     * If there are children of this ancestor which depends on it,
     * (i.e 'joins with it') then this ancestor can only be added to our
     * 'join nest' if it is inner joined with our common_ancestor.
     */
    if (depend_parents.contain(ancestor_no) &&
        ancestor->get_join_type(common_ancestor) == AQP::JT_OUTER_JOIN)
    {
      /**
       * Found an outer joined ancestor which none of my parents
       * can depend / join with:
       */
      if (child_dependencies.is_clear_all())
      {
        /**
         * As this was the last (outer joined) 'depend_parents',
         * with no other mandatory dependencies, this table may still
         * be added to the pushed join.
         * However, it may contain 'common' & 'extend' parent candidates
         * which may now be joined with this outer joined ancestor.
         * These are removed below.
         */
        DBUG_ASSERT(extend_parents.contain(common_parents));
        for (uint parent_no= extend_parents.last_table(tab_no-1);
             parent_no > ancestor_no;
             parent_no= extend_parents.last_table(parent_no-1))
        {
          if (!m_tables[parent_no].m_depend_parents.contain(ancestor_no))
	  {
            common_parents.clear_bit(parent_no);
            extend_parents.clear_bit(parent_no);
          }
        }
        DBUG_ASSERT(!extend_parents.is_clear_all());
      }
      else if (!child_dependencies.contain(ancestor_no))
      {
        /**
         * No child of this ancestor depends (joins) with this ancestor,
         * and adding it as a 'depend_parent' would introduce new
         * dependencies on outer joined grandparents.
         * We will not be allowed to add this table to the pushed join.
         */
        EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                        "as it would introduce a dependency on "
                        "outer joined grandparent '%s'", 
                         table->get_table()->alias,
                         m_join_root->get_table()->alias,
                         ancestor->get_table()->alias);
        DBUG_RETURN(false);
      }
    }

    // Aggregate dependency sets
    child_dependencies.add(m_tables[ancestor_no].m_depend_parents);
    dependencies.add(m_tables[ancestor_no].m_depend_parents);
  } //for

  DBUG_ASSERT(m_join_scope.contain(common_parents));
  DBUG_ASSERT(m_join_scope.contain(extend_parents));
  DBUG_ASSERT(extend_parents.is_clear_all() ||
              extend_parents.contain(common_parents));
  /**
   * Register calculated parent sets - ::optimize() choose from these.
   */
  m_tables[tab_no].m_common_parents= common_parents;
  m_tables[tab_no].m_extend_parents= extend_parents;
  m_tables[tab_no].m_depend_parents= depend_parents;
  m_tables[tab_no].m_parent= MAX_TABLES;

  m_tables[tab_no].m_maybe_pushable= 0; // Exclude from further pushing
  m_join_scope.add(tab_no);

  DBUG_RETURN(true);
} // ndb_pushed_builder_ctx::is_pushable_as_child


/*********************
 * This method examines a key item (could be part of a lookup key or a scan 
 * bound) for a table access operation and calculates the set of possible
 * parents. (These are possible parent table access operations in the query 
 * tree that will be pushed to the ndb.)
 *
 * @param[in] table The table access operation to which the key item belongs.
 * @param[in] key_item The key_item to examine
 * @param[in] key_part Metatdata about the key item.
 * @param[out] field_parents The set of possible parents for 'key_item' 
 * ('join_root' if keys are constant).
 * @return True if at least one possible parent was found. (False means that 
 * operation cannot be pushed).
 */
bool ndb_pushed_builder_ctx::is_field_item_pushable(
                     const AQP::Table_access* table,
                     const Item* key_item, 
                     const KEY_PART_INFO* key_part,
                     ndb_table_access_map& field_parents)
{
  DBUG_ENTER("is_field_item_pushable()");
  const uint tab_no = table->get_access_no();
  DBUG_ASSERT(key_item->type() == Item::FIELD_ITEM);

  const Item_field* const key_item_field 
    = static_cast<const Item_field*>(key_item);

  DBUG_PRINT("info", ("keyPart:%d, field:%s.%s",
              (int)(key_item - table->get_key_field(0)),
              key_item_field->field->table->alias, 
              key_item_field->field->field_name));

  if (!key_item_field->field->eq_def(key_part->field))
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child, "
                    "column '%s' does not have same datatype as ref'ed "
                    "column '%s.%s'",
                    table->get_table()->alias,
                    key_part->field->field_name,
                    key_item_field->field->table->alias, 
                    key_item_field->field->field_name);
    m_tables[tab_no].m_maybe_pushable &= ~PUSHABLE_AS_CHILD; // Permanently disable as child
    DBUG_RETURN(false);
  }

  if (key_item_field->field->is_virtual_gcol())
  {
    EXPLAIN_NO_PUSH("Can't push condition on virtual generated column '%s.%s'",
                    key_item_field->field->table->alias,
                    key_item_field->field->field_name);
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
  uint referred_table_no= get_table_no(key_item_field);
  if (m_join_scope.contain(referred_table_no))
  {
    field_parents.add(referred_table_no);
  }

  //////////////////////////////////////////////////////////////////
  // 2) Use the equality set to possibly find more parent candidates
  //    usable by substituting existing 'key_item_field'
  //
  Item_equal* item_equal= table->get_item_equal(key_item_field);
  if (item_equal)
  {
    AQP::Equal_set_iterator equal_iter(*item_equal);
    const Item_field* substitute_field;
    while ((substitute_field= equal_iter.next()) != NULL)
    {
      if (substitute_field != key_item_field)
      {
        const uint substitute_table_no= get_table_no(substitute_field);
        if (m_join_scope.contain(substitute_table_no))
        {
          DBUG_PRINT("info", 
                     (" join_items[%d] %s.%s can be replaced with %s.%s",
                      (int)(key_item - table->get_key_field(0)),
                      get_referred_table_access_name(key_item_field),
                      get_referred_field_name(key_item_field),
                      get_referred_table_access_name(substitute_field),
                      get_referred_field_name(substitute_field)));

          field_parents.add(substitute_table_no);
        }
      }
    } // while(substitute_field != NULL)
  }
  if (!field_parents.is_clear_all())
  {
    DBUG_RETURN(true);
  }

  if (m_const_scope.contain(referred_table_no))
  {
    // This key item is const. and did not cause the set of possible parents
    // to be recalculated. Reuse what we had before this key item.
    DBUG_ASSERT(field_parents.is_clear_all());

    /**
     * Field referrence is a 'paramValue' to a column value evaluated
     * prior to the root of this pushed join candidate. Some restrictions
     * applies to when a field reference is allowed in a pushed join:
     */
    if (ndbcluster_is_lookup_operation(m_join_root->get_access_type()))
    {
      /**
       * The 'eq_ref' access function join_read_key(), may optimize away
       * key reads if the key for a requested row is the same as the
       * previous. Thus, iff this is the root of a pushed lookup join
       * we do not want it to contain childs with references to columns 
       * 'outside' the the pushed joins, as these may still change
       * between calls to join_read_key() independent of the root key
       * itself being the same.
       */
      EXPLAIN_NO_PUSH("Cannot push table '%s' as child of '%s', since "
                      "it referes to column '%s.%s' prior to a "
                      "potential 'const' root.",
                      table->get_table()->alias, 
                      m_join_root->get_table()->alias,
                      get_referred_table_access_name(key_item_field),
                      get_referred_field_name(key_item_field));
      DBUG_RETURN(false);
    }
    else  
    {
      /** 
       * Scan queries cannot be pushed if the pushed query may refer column 
       * values (paramValues) from rows stored in a join cache.  
       */
      const TABLE* const referred_tab = key_item_field->field->table;
      uint access_no = tab_no;
      do
      {
        if (m_plan.get_table_access(access_no)->uses_join_cache())
        {
          EXPLAIN_NO_PUSH("Cannot push table '%s' as child of '%s', since "
                          "it referes to column '%s.%s' which will be stored "
                          "in a join buffer.",
                          table->get_table()->alias, 
                          m_join_root->get_table()->alias,
                          get_referred_table_access_name(key_item_field),
                          get_referred_field_name(key_item_field));
          DBUG_RETURN(false);
        }
        DBUG_ASSERT(access_no > 0);
        access_no--;
      } while (m_plan.get_table_access(access_no)->get_table() 
               != referred_tab);

    } // if (!ndbcluster_is_lookup_operation(root_type)
    field_parents= ndb_table_access_map(m_join_root->get_access_no());
    DBUG_RETURN(true);
  }
  else
  {
    EXPLAIN_NO_PUSH("Can't push table '%s' as child of '%s', "
                    "column '%s.%s' is outside scope of pushable join",
                     table->get_table()->alias, m_join_root->get_table()->alias,
                     get_referred_table_access_name(key_item_field),
                     get_referred_field_name(key_item_field));
    DBUG_RETURN(false);
  }
} // ndb_pushed_builder_ctx::is_field_item_pushable()


bool ndb_pushed_builder_ctx::is_const_item_pushable(
                     const Item* key_item, 
                     const KEY_PART_INFO* key_part)
{
  DBUG_ENTER("is_const_item_pushable()");
  DBUG_ASSERT(key_item->const_item());

  /** 
   * Propagate Items constant value to Field containing the value of this 
   * key_part:
   */
  Field* const field= key_part->field;
  const int error= 
    const_cast<Item*>(key_item)->save_in_field_no_warnings(field, true);
  if (unlikely(error))
  {
    DBUG_PRINT("info", ("Failed to store constant Item into Field -> not"
                        " pushable"));
    DBUG_RETURN(false);
  }
  if (field->is_real_null())
  {
    DBUG_PRINT("info", ("NULL constValues in key -> not pushable"));
    DBUG_RETURN(false);   // TODO, handle gracefull -> continue?
  }
  DBUG_RETURN(true);
} // ndb_pushed_builder_ctx::is_const_item_pushable()


int
ndb_pushed_builder_ctx::optimize_query_plan()
{
  DBUG_ENTER("optimize_query_plan");
  const uint root_no= m_join_root->get_access_no();

  for (uint tab_no= root_no; tab_no<m_plan.get_access_count(); tab_no++)
  {
    if (m_join_scope.contain(tab_no))
    {
      m_tables[tab_no].m_fanout = m_plan.get_table_access(tab_no)->get_fanout();
      m_tables[tab_no].m_child_fanout = 1.0;
    }
  }

  // Find an optimal order for joining the tables
  for (uint tab_no= m_plan.get_access_count()-1;
       tab_no > root_no;
       tab_no--)
  {
    if (!m_join_scope.contain(tab_no))
      continue;

    /**
     * Enforce the parent dependencies on the available
     * 'common' and 'extended' parents set such that we
     * don't skip any dependent parents from our ancestors
     * when selecting the actuall 'm_parent' to be used.
     */
    pushed_tables &table= m_tables[tab_no];
    if (!table.m_depend_parents.is_clear_all())
    {
      ndb_table_access_map const &dependency= table.m_depend_parents;
      DBUG_ASSERT(!dependency.contain(tab_no)); // Circular dependency!

      uint depends_on_parent= dependency.last_table(tab_no-1);
      ndb_table_access_map dependency_mask;
      dependency_mask.set_prefix(depends_on_parent);

      /**
       * Remove any parent candidates prior to last 'depends_on_parent':
       * All 'depend_parents' must be made available as grandparents
       * prior to joining with any 'extend_parents' / 'common_parents'
       */
      table.m_common_parents.subtract(dependency_mask);
      table.m_extend_parents.subtract(dependency_mask);

      /**
       * Need some parent hints if all where cleared.
       * Can always use closest depend_parent.
       */
      if (table.m_extend_parents.is_clear_all())
      {
        table.m_extend_parents.add(depends_on_parent);
      }
    }

    /**
     * Select set to choose parent from, prefer a 'common'
     * parent if available.
     */
    uint parent_no;
    ndb_table_access_map const &parents=
       table.m_common_parents.is_clear_all()
       ? table.m_extend_parents
       : table.m_common_parents;

    DBUG_ASSERT(!parents.is_clear_all());
    DBUG_ASSERT(!parents.contain(tab_no)); // No circular dependency!

    /**
     * In order to take advantage of the parallelism in the SPJ block;
     * Initial parent candidate is the first possible among 'parents'.
     * Will result in the most 'bushy' query plan (aka: star-join)
     */
    parent_no= parents.first_table(root_no);

    /**
     * Push optimization for execution of child operations:
     *
     * To take advantage of the selectivity of parent operations we 
     * execute any parent operations with fanout <= 1 before this
     * child operation. By making them depending on parent 
     * operations with high selectivity, child will be eliminated when
     * the parent returns no matching rows.
     *
     * -> Execute child operation after any such parents
     */
    for (uint candidate= parent_no+1; candidate<tab_no; candidate++)
    {
      if (parents.contain(candidate))
      {
        if (m_tables[candidate].m_fanout > 1.0)
          break;

        parent_no= candidate;     // Parent candidate is selective, eval after
      }
    }

    DBUG_ASSERT(parent_no < tab_no);
    table.m_parent= parent_no;
    m_tables[parent_no].m_child_fanout*= table.m_fanout*table.m_child_fanout;

    /**
     * Any remaining parent dependencies for this table has to be
     * added to the selected parent in order to be taken into account 
     * for parent calculation for its ancestors.
     */
    ndb_table_access_map dependency(table.m_depend_parents);
    dependency.clear_bit(parent_no);
    m_tables[parent_no].m_depend_parents.add(dependency);
  }

  /* Build the set of ancestors available through the selected 'm_parent' */
  for (uint tab_no= root_no+1;
       tab_no < m_plan.get_access_count();
       tab_no++)
  {
    if (m_join_scope.contain(tab_no))
    {
      pushed_tables &table= m_tables[tab_no];
      const uint parent_no= table.m_parent;
      table.m_ancestors= m_tables[parent_no].m_ancestors;
      table.m_ancestors.add(parent_no);
      DBUG_ASSERT(table.m_ancestors.contain(table.m_depend_parents));
    }
  }
  DBUG_RETURN(0);
} // ndb_pushed_builder_ctx::optimize_query_plan


void
ndb_pushed_builder_ctx::collect_key_refs(
                                  const AQP::Table_access* table,
                                  const Item* key_refs[]) const
{
  DBUG_ENTER("collect_key_refs");

  const uint tab_no= table->get_access_no();
  const uint parent_no= m_tables[tab_no].m_parent;
  const ndb_table_access_map& ancestors= m_tables[tab_no].m_ancestors;

  DBUG_ASSERT(m_join_scope.contain(ancestors));
  DBUG_ASSERT(ancestors.contain(parent_no));

  /**
   * If there are any key_fields with 'current_parents' different from
   * our selected 'parent', we have to find substitutes for
   * those key_fields within the equality set.
   **/
  for (uint key_part_no= 0; 
       key_part_no < table->get_no_of_key_fields(); 
       key_part_no++)
  {
    const Item* const key_item= table->get_key_field(key_part_no);
    key_refs[key_part_no]= key_item;

    DBUG_ASSERT(key_item->const_item() || key_item->type()==Item::FIELD_ITEM);

    if (key_item->type() == Item::FIELD_ITEM)
    {
      const Item_field* join_item= static_cast<const Item_field*>(key_item);
      uint referred_table_no= get_table_no(join_item);
      Item_equal* item_equal;

      if (referred_table_no != parent_no && 
          (item_equal= table->get_item_equal(join_item)) != NULL)
      {
        AQP::Equal_set_iterator iter(*item_equal);
        const Item_field* substitute_field;
        while ((substitute_field= iter.next()) != NULL)
        {
          ///////////////////////////////////////////////////////////
          // Prefer to replace join_item with ref. to selected parent.
          //
          const uint substitute_table_no= get_table_no(substitute_field);
          if (substitute_table_no == parent_no)
          {
            DBUG_PRINT("info", 
                       (" Replacing key_refs[%d] %s.%s with %s.%s (parent)",
                        key_part_no,
                        get_referred_table_access_name(join_item),
                        get_referred_field_name(join_item),
                        get_referred_table_access_name(substitute_field),
                        get_referred_field_name(substitute_field)));

            referred_table_no= substitute_table_no;
            key_refs[key_part_no]= join_item= substitute_field;
            break;
          }
          else if (ancestors.contain(substitute_table_no))
          {
            DBUG_ASSERT(substitute_table_no <= parent_no);

            //////////////////////////////////////////////////////////////////////
            // Second best is to replace join_item with closest grandparent ref.
            // In this case we will continue to search for the common parent match:
            // Updates key_refs[] if:
            //   1): Replace incorrect refs of tables not being an 'ancestor'. 
            //   2): Found a better substitute closer to selected parent 
            //
            if (!ancestors.contain(referred_table_no) ||   // 1
                referred_table_no < substitute_table_no)   // 2)
            {
              DBUG_PRINT("info", 
                         (" Replacing key_refs[%d] %s.%s with %s.%s (grandparent)",
                          key_part_no,
                          get_referred_table_access_name(join_item),
                          get_referred_field_name(join_item),
                          get_referred_table_access_name(substitute_field),
                          get_referred_field_name(substitute_field)));

              referred_table_no= substitute_table_no;
              key_refs[key_part_no]= join_item= substitute_field;
            }
          }
        } // while (substitute...

        DBUG_ASSERT (referred_table_no == parent_no ||
                     !m_join_scope.contain(referred_table_no)  || // Is a 'const' paramValue
                     !m_tables[tab_no].m_common_parents.contain(parent_no));
      }
    } // Item::FIELD_ITEM
  }

  key_refs[table->get_no_of_key_fields()]= NULL;
  DBUG_VOID_RETURN;
} // ndb_pushed_builder_ctx::collect_key_refs()


int
ndb_pushed_builder_ctx::build_key(const AQP::Table_access* table,
                                  const NdbQueryOperand *op_key[])
{
  DBUG_ENTER("build_key");
  DBUG_ASSERT(m_join_scope.contain(table->get_access_no()));

  const KEY* const key= &table->get_table()->key_info[table->get_index_no()];
  op_key[0]= NULL;

  if (table == m_join_root)
  {
    if (ndbcluster_is_lookup_operation(table->get_access_type()))
    {
      for (uint i= 0; i < key->user_defined_key_parts; i++)
      {
        op_key[i]= m_builder->paramValue();
        if (unlikely(op_key[i] == NULL))
        {
          DBUG_RETURN(-1);
        }
      }
      op_key[key->user_defined_key_parts]= NULL;
    }
  }
  else
  {
    const uint key_fields= table->get_no_of_key_fields();
    DBUG_ASSERT(key_fields > 0 && key_fields <= key->user_defined_key_parts);
    uint map[ndb_pushed_join::MAX_LINKED_KEYS+1];

    if (ndbcluster_is_lookup_operation(table->get_access_type()))
    {
      const ha_ndbcluster* handler=
        static_cast<ha_ndbcluster*>(table->get_table()->file);
      ndbcluster_build_key_map(handler->m_table, 
                               handler->m_index[table->get_index_no()],
                               key, map);
    }
    else
    {
      for (uint ix = 0; ix < key_fields; ix++)
      {
        map[ix]= ix;
      }
    }

    const Item* join_items[ndb_pushed_join::MAX_LINKED_KEYS+1];
    collect_key_refs(table,join_items);

    const KEY_PART_INFO *key_part= key->key_part;
    for (uint i= 0; i < key_fields; i++, key_part++)
    {
      const Item* const item= join_items[i];
      op_key[map[i]]= NULL;

      DBUG_ASSERT(item->const_item() == item->const_for_execution());
      if (item->const_item())
      {
        /** 
         * Propagate Items constant value to Field containing the value of this 
         * key_part:
         */
        Field* const field= key_part->field;
        DBUG_ASSERT(!field->is_real_null());
        const uchar* const ptr= (field->real_type() == MYSQL_TYPE_VARCHAR)
                ? field->ptr + ((Field_varstring*)field)->length_bytes
                : field->ptr;

        op_key[map[i]]= m_builder->constValue(ptr, field->data_length());
      }
      else
      {
        DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
        const Item_field* const field_item= static_cast<const Item_field*>(item);
        const uint referred_table_no= get_table_no(field_item);

        if (m_join_scope.contain(referred_table_no))
        {
          // Locate the parent operation for this 'join_items[]'.
          // May refer any of the preceding parent tables
          const NdbQueryOperationDef* const parent_op= m_tables[referred_table_no].m_op;
          DBUG_ASSERT(parent_op != NULL);

          // TODO use field_index ??
          op_key[map[i]]= m_builder->linkedValue(parent_op, 
                                               field_item->field_name);
        }
        else
        {
          DBUG_ASSERT(m_const_scope.contain(referred_table_no));
          // Outside scope of join plan, Handle as parameter as its value
          // will be known when we are ready to execute this query.
          if (unlikely(m_fld_refs >= ndb_pushed_join::MAX_REFERRED_FIELDS))
          {
            DBUG_PRINT("info", ("Too many Field refs ( >= MAX_REFERRED_FIELDS) "
                                "encountered"));
            DBUG_RETURN(-1);  // TODO, handle gracefull -> continue?
          }
          m_referred_fields[m_fld_refs++]= field_item->field;
          op_key[map[i]]= m_builder->paramValue();
        }
      }

      if (unlikely(op_key[map[i]] == NULL))
      {
        DBUG_RETURN(-1);
      }
    }
    op_key[key_fields]= NULL;
  }
  DBUG_RETURN(0);
} // ndb_pushed_builder_ctx::build_key()


int
ndb_pushed_builder_ctx::build_query()
{
  DBUG_ENTER("build_query");

  DBUG_PRINT("enter", ("Table %d as root is pushable", m_join_root->get_access_no()));
  DBUG_EXECUTE("info", m_join_root->dbug_print(););

  uint root_no= m_join_root->get_access_no();
  DBUG_ASSERT(m_join_scope.contain(root_no));

  if (m_builder == NULL)
  {
    m_builder= NdbQueryBuilder::create();
    if (unlikely (m_builder==NULL))
    {
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    }
  }

  for (uint tab_no= root_no; tab_no<m_plan.get_access_count(); tab_no++)
  {
    if (!m_join_scope.contain(tab_no))
      continue;

    const AQP::Table_access* const table= m_plan.get_table_access(tab_no);
    const AQP::enum_access_type access_type= table->get_access_type();
    const ha_ndbcluster* handler=
      static_cast<ha_ndbcluster*>(table->get_table()->file);

    const NdbQueryOperand* op_key[ndb_pushed_join::MAX_KEY_PART+1];
    if (table->get_index_no() >= 0)
    {
      const int error= build_key(table, op_key);
      if (unlikely(error))
        DBUG_RETURN(error);
    }

    NdbQueryOptions options;
    if (handler->m_cond)
    {
      NdbInterpretedCode code(handler->m_table);
      if (handler->m_cond->generate_scan_filter(&code, NULL) != 0)
      {
//      ERR_RETURN(code.getNdbError());  // FIXME
      }
      options.setInterpretedCode(code);
    } 
    if (table != m_join_root)
    {
      DBUG_ASSERT(m_tables[tab_no].m_parent!=MAX_TABLES);
      const uint parent_no= m_tables[tab_no].m_parent;
      const AQP::Table_access* parent= m_plan.get_table_access(parent_no);

      if (!m_tables[tab_no].m_common_parents.contain(parent_no))
      {
        DBUG_ASSERT(m_tables[parent_no].m_op != NULL);
        options.setParent(m_tables[parent_no].m_op);
      }
      if (table->get_join_type(parent) == AQP::JT_INNER_JOIN)
      {
        options.setMatchType(NdbQueryOptions::MatchNonNull);
      }
    }

    const NdbQueryOperationDef* query_op= NULL;
    if (ndbcluster_is_lookup_operation(access_type))
    {
      // Primary key access assumed
      if (access_type == AQP::AT_PRIMARY_KEY || 
          access_type == AQP::AT_MULTI_PRIMARY_KEY)
      {
        DBUG_PRINT("info", ("Operation is 'primary-key-lookup'"));
        query_op= m_builder->readTuple(handler->m_table, op_key, &options);
      }
      else
      {
        DBUG_ASSERT(access_type == AQP::AT_UNIQUE_KEY);
        DBUG_PRINT("info", ("Operation is 'unique-index-lookup'"));
        const NdbDictionary::Index* const index 
          = handler->m_index[table->get_index_no()].unique_index;
        DBUG_ASSERT(index);
        query_op= m_builder->readTuple(index, handler->m_table, op_key, &options);
      }
    } // ndbcluster_is_lookup_operation()

    /**
     *  AT_MULTI_MIXED may have 'ranges' which are pure single key lookups also.
     *  In our current implementation these are converted into range access in the
     *  pushed MRR implementation. However, the future plan is to build both 
     *  RANGE and KEY pushable joins for these.
     */
    else if (access_type == AQP::AT_ORDERED_INDEX_SCAN  ||
             access_type == AQP::AT_MULTI_MIXED)
    {
      DBUG_ASSERT(table->get_index_no() >= 0);
      DBUG_ASSERT(handler->m_index[table->get_index_no()].index != NULL);

      DBUG_PRINT("info", ("Operation is 'equal-range-lookup'"));
      DBUG_PRINT("info", ("Creating scanIndex on index id:%d, name:%s",
                          table->get_index_no(), 
                          handler->m_index[table->get_index_no()]
                           .index->getName()));

      const NdbQueryIndexBound bounds(op_key);
      query_op= m_builder->scanIndex(handler->m_index[table->get_index_no()].index,
                                   handler->m_table, &bounds, &options);
    }
    else if (access_type == AQP::AT_TABLE_SCAN) 
    {
      DBUG_PRINT("info", ("Operation is 'table scan'"));
      query_op= m_builder->scanTable(handler->m_table, &options);
    }
    else
    {
      DBUG_ASSERT(false);
    }

    if (unlikely(!query_op))
      DBUG_RETURN(-1);

    m_tables[tab_no].m_op= query_op;
  } // for (join_cnt= m_join_root->get_access_no(); join_cnt<plan.get_access_count(); join_cnt++)

  DBUG_RETURN(0);
} // ndb_pushed_builder_ctx::build_query()


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
    for (ix = 0; ix < key_def->user_defined_key_parts; ix++)
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

    for (ix = 0, key_part= key_def->key_part; ix < key_def->user_defined_key_parts; ix++, key_part++)
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
} // ndbcluster_build_key_map
