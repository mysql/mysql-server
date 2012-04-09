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

#include "sql_bitmap.h"

class NdbTransaction;
class NdbQueryBuilder;
class NdbQueryOperand;
class NdbQueryOperationDef;
class ndb_pushed_builder_ctx;
struct NdbError;

namespace AQP{
  class Join_plan;
  class Table_access;
};

void ndbcluster_build_key_map(const NdbDictionary::Table* table, 
			      const NDB_INDEX_DATA& index,
			      const KEY *key_def,
			      uint ix_map[]);


/** 
 * This type is used in conjunction with AQP::Join_plan and represents a set 
 * of the table access operations of the join plan. 
 * Had to subclass Bitmap as the default Bitmap<64> c'tor didn't initialize its
 * map.
 */
typedef Bitmap<(MAX_TABLES > 64 ? MAX_TABLES : 64)> table_bitmap;

class ndb_table_access_map : public table_bitmap
{
public:
  explicit ndb_table_access_map()
   : table_bitmap(0)
  {}
  explicit ndb_table_access_map(uint table_no)
   : table_bitmap(0)
  { set_bit(table_no);
  }

  void add(const ndb_table_access_map& table_map)
  { // Require const_cast as signature of class Bitmap::merge is not const correct
    merge(const_cast<ndb_table_access_map&>(table_map));
  }
  void add(uint table_no)
  {
    set_bit(table_no);
  }

  bool contain(const ndb_table_access_map& table_map) const
  {
    return table_map.is_subset(*this);
  }
  bool contain(uint table_no) const
  {
    return is_set(table_no);
  }

  uint first_table(uint start= 0) const;
  uint last_table(uint start= MAX_TABLES) const;

}; // class ndb_table_access_map


/** This class represents a prepared pushed (N-way) join operation.
 *
 *  It might be instantiated multiple times whenever the query,
 *  or this subpart of the query, is being (re-)executed by
 *  ::createQuery() or it's wrapper method 
 *  ha_ndbcluster::create_pushed_join().
 */
class ndb_pushed_join
{
public:
  explicit ndb_pushed_join(const ndb_pushed_builder_ctx& builder_ctx,
                           const NdbQueryDef* query_def);
  
  ~ndb_pushed_join(); 

  /**
   * Check that this prepared pushed query matches the type
   * of operation specified by the arguments.
   */
  bool match_definition(int type, //NdbQueryOperationDef::Type, 
                        const NDB_INDEX_DATA* idx,
                        bool needSorted) const;

  /** Create an executable instance of this defined query. */
  NdbQuery* make_query_instance(
                        NdbTransaction* trans,
                        const NdbQueryParamValue* keyFieldParams,
                        uint paramCnt) const;

  /** Get the number of pushed table access operations.*/
  uint get_operation_count() const
  { return m_operation_count; }

  /**
   * In a pushed join, fields in lookup keys and scan bounds may refer to 
   * result fields of table access operation that execute prior to the pushed
   * join. This method returns the number of such references.
   */
  uint get_field_referrences_count() const
  { return m_field_count; }

  const NdbQueryDef& get_query_def() const
  { return *m_query_def; }

  /** Get the table that is accessed by the i'th table access operation.*/
  TABLE* get_table(uint i) const
  { 
    DBUG_ASSERT(i < m_operation_count);
    return m_tables[i];
  }

  /** 
   * This is the maximal number of fields in the key of any pushed table
   * access operation.
   */
  static const uint MAX_KEY_PART= MAX_KEY;
  /**
   * In a pushed join, fields in lookup keys and scan bounds may refer to 
   * result fields of table access operation that execute prior to the pushed
   * join. This constant specifies the maximal number of such references for 
   * a query.
   */
  static const uint MAX_REFERRED_FIELDS= 16;
  /**
   * For each table access operation in a pushed join, this is the maximal 
   * number of key fields that may refer to the fields of the parent operation.
   */
  static const uint MAX_LINKED_KEYS= MAX_KEY;
  /** 
   * This is the maximal number of table access operations there can be in a 
   * single pushed join.
   */
  static const uint MAX_PUSHED_OPERATIONS= MAX_TABLES;

private:
  const NdbQueryDef* const m_query_def;  // Definition of pushed join query

  /** This is the number of table access operations in the pushed join.*/
  uint m_operation_count;

  /** This is the tables that are accessed by the pushed join.*/
  TABLE* m_tables[MAX_PUSHED_OPERATIONS];

  /**
   * This is the number of referred fields of table access operation that 
   * execute prior to the pushed join.
   */
  const uint m_field_count;

  /**
   * These are the referred fields of table access operation that execute 
   * prior to the pushed join.
   */
  Field* m_referred_fields[MAX_REFERRED_FIELDS];
}; // class ndb_pushed_join



/**
 * Contains the context and helper methods used during ::make_pushed_join().
 *
 * Interacts with the AQP which provides interface to the query prepared by
 * the mysqld optimizer.
 *
 * Execution plans built for pushed joins are stored inside this builder context.
 */
class ndb_pushed_builder_ctx
{
  friend ndb_pushed_join::ndb_pushed_join(
                           const ndb_pushed_builder_ctx& builder_ctx,
                           const NdbQueryDef* query_def);

public:
  ndb_pushed_builder_ctx(const AQP::Join_plan& plan);
  ~ndb_pushed_builder_ctx();

  /**
   * Build the pushed query identified with 'is_pushable_with_root()'.
   * Returns:
   *   = 0: A NdbQueryDef has successfully been prepared for execution.
   *   > 0: Returned value is the error code.
   *   < 0: There is a pending NdbError to be retrieved with getNdbError()
   */
  int make_pushed_join(const AQP::Table_access* join_root,
                       const ndb_pushed_join* &pushed_join);

  const NdbError& getNdbError() const;

private:
  /**
   * Collect all tables which may be pushed together with 'root'.
   * Returns 'true' if anything is pushable.
   */
  bool is_pushable_with_root(
                  const AQP::Table_access* root);

  bool is_pushable_as_child(
                  const AQP::Table_access* table);

  bool is_const_item_pushable(
                  const Item* key_item, 
                  const KEY_PART_INFO* key_part);

  bool is_field_item_pushable(
                  const AQP::Table_access* table,
                  const Item* key_item, 
                  const KEY_PART_INFO* key_part,
                  ndb_table_access_map& parents);

  int optimize_query_plan();

  int build_query();

  void collect_key_refs(
                  const AQP::Table_access* table,
                  const Item* key_refs[]) const;

  int build_key(const AQP::Table_access* table,
                const NdbQueryOperand* op_key[]);

  uint get_table_no(const Item* key_item) const;

private:
  const AQP::Join_plan& m_plan;
  const AQP::Table_access* m_join_root;

  // Scope of tables covered by this pushed join
  ndb_table_access_map m_join_scope;

  // Scope of tables evaluated prior to 'm_join_root'
  // These are effectively const or params wrt. the pushed join
  ndb_table_access_map m_const_scope;

  // Set of tables required to have strict sequential dependency
  ndb_table_access_map m_forced_sequence;

  uint m_fld_refs;
  Field* m_referred_fields[ndb_pushed_join::MAX_REFERRED_FIELDS];

  // Handle to the NdbQuery factory.
  // Possibly reused if multiple NdbQuery's are pushed.
  NdbQueryBuilder* m_builder;

  enum pushability
  {
    PUSHABLE_AS_PARENT= 0x01,
    PUSHABLE_AS_CHILD= 0x02
  } enum_pushability;

  struct pushed_tables
  {
    pushed_tables() : 
      m_maybe_pushable(0),
      m_common_parents(), 
      m_extend_parents(), 
      m_depend_parents(), 
      m_parent(MAX_TABLES), 
      m_ancestors(), 
      m_op(NULL) 
    {}

    int m_maybe_pushable;  // OR'ed bits from 'enum_pushability'

    /**
     * We maintain two sets of parent candidates for each table: 
     *  - 'common' are those parents for which ::collect_key_refs()
     *     will find key_refs[] (possibly through the EQ-sets) such that all
     *     linkedValues() refer fields from the same parent.
     *  - 'extendeded' are those parents refered from some of the 
     *     key_refs[], and having the rest of the key_refs[] available as
     *     'grandparent refs'.
     */
    ndb_table_access_map m_common_parents;
    ndb_table_access_map m_extend_parents;

    /**
     * (sub)Set of a parents which *must* be available as ancestors
     * due to dependencies on these parents tables.
     */
    ndb_table_access_map m_depend_parents;

    /**
     * The actual parent is choosen among (m_common_parents | m_extend_parents)
     * by ::optimize_query_plan()
     */
    uint m_parent;

    /**
     * All ancestors available throught the 'm_parent' chain
     */
    ndb_table_access_map m_ancestors;

    const NdbQueryOperationDef* m_op;
  } m_tables[MAX_TABLES];

  /**
   * There are two different table enumerations used:
   */
  struct table_remap
  {
    Uint16 to_external;  // m_remap[] is indexed with internal table_no
    Uint16 to_internal;  // m_remap[] is indexed with external tablenr
  } m_remap[MAX_TABLES];

}; // class ndb_pushed_builder_ctx


