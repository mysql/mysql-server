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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                       /* gcc class implementation */
#endif

#include <abstract_query_plan.h>

#define EXPLAIN_NO_PUSH(msgfmt, ...)                              \
do                                                                \
{                                                                 \
  if (unlikely(current_thd->lex->describe & DESCRIBE_EXTENDED))   \
  {                                                               \
    ndbcluster_explain_no_push ((msgfmt), __VA_ARGS__);           \
  }                                                               \
}                                                                 \
while(0)

int ndbcluster_make_pushed_join(handlerton *hton, THD* thd,
				AQP::Join_plan* plan);

inline bool ndbcluster_is_lookup_operation(AQP::enum_access_type accessType)
{
  return accessType == AQP::AT_PRIMARY_KEY ||
    accessType == AQP::AT_MULTI_PRIMARY_KEY ||
    accessType == AQP::AT_UNIQUE_KEY;
}

void ndbcluster_explain_no_push(const char* msgfmt, ...);

void ndbcluster_build_key_map(const NdbDictionary::Table* table, 
			      const NDB_INDEX_DATA& index,
			      const KEY *key_def,
			      uint ix_map[]);

/**
 * This is a list of NdbQueryDef objects that have been created within a 
 * transaction. This list is kept to make sure that they are all released 
 * when the transaction ends.
 * An NdbQueryDef object is required to live longer than any NdbQuery object
 * instantiated from it. Since NdbQueryObjects may be kept until the 
 * transaction ends, this list is necessary.
 */
class ndb_query_def_list
{
public:
  ndb_query_def_list(const NdbQueryDef* def, const ndb_query_def_list* next):
    m_def(def), m_next(next){}

  const NdbQueryDef* get_def() const
  { return m_def; }

  const ndb_query_def_list* get_next() const
  { return m_next; }

private:
  const NdbQueryDef* const m_def;
  const ndb_query_def_list* const m_next;
};

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

  explicit ndb_table_access_map(table_map bitmap)
   : table_bitmap(0)
//{ set_map(table); } .. Bitmap<>::set_map() is expected to be available in the near future
  {
    for (uint i= 0; bitmap!=0; i++, bitmap>>=1)
    {
      if (bitmap & 1)
        set_bit(i);
    }
  }

  explicit ndb_table_access_map(const AQP::Table_access* const table)
   : table_bitmap(0)
  { set_bit(table->get_table()->tablenr); }

  void add(const ndb_table_access_map& table_map)
  { // Require const_cast as signature of class Bitmap::merge is not const correct
    merge(const_cast<ndb_table_access_map&>(table_map));
  }
  void add(const AQP::Table_access* const table)
  {
    ndb_table_access_map table_map(table);
    add(table_map);
  }

  bool contain(const ndb_table_access_map& table_map) const
  {
    return table_map.is_subset(*this);
  }
  bool contain_table(const AQP::Table_access* const table) const
  {
    ndb_table_access_map table_map(table);
    return contain(table_map);
  }
}; // class ndb_table_access_map


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
public:

  ndb_pushed_builder_ctx(AQP::Join_plan& plan)
   : m_plan(plan), m_join_root(), m_join_scope(), m_const_scope()
  { 
    if (plan.get_access_count() > 1)
      init_pushability();
  }

  /** Define root of pushable tree.*/
  void set_root(const AQP::Table_access* join_root);

  const AQP::Join_plan& plan() const
  { return m_plan; }

  const AQP::Table_access* join_root() const
  { return m_join_root; }

  const ndb_table_access_map& join_scope() const
  { return m_join_scope; }

  const ndb_table_access_map& const_scope() const
  { return m_const_scope; }

  const class NdbQueryOperationDef* 
    get_query_operation(const AQP::Table_access* table) const
  {
    DBUG_ASSERT(m_join_scope.contain_table(table));
    return m_tables[table->get_access_no()].op;
  }

  const AQP::Table_access* get_referred_table_access(
                  const ndb_table_access_map& used_tables) const;

  bool is_pushable_as_parent(
                  const AQP::Table_access* table);

  bool is_pushable_as_child(
                  const AQP::Table_access* table,
                  const Item* join_items[],
                  const AQP::Table_access*& parent);

  void add_pushed(const AQP::Table_access* table,
                  const AQP::Table_access* parent,
                  const NdbQueryOperationDef* query_op);

  // Check if 'table' is child of some of the specified 'parents'
  bool is_child_of(const AQP::Table_access* table,
                   const ndb_table_access_map& parents) const
  {
    DBUG_ASSERT(m_join_scope.contain_table(table));
    DBUG_ASSERT(parents.is_subset(m_join_scope));
    return parents.is_overlapping(m_tables[table->get_access_no()].m_ancestors);
  }

  enum pushability
  {
    PUSHABLE_AS_PARENT= 0x01,
    PUSHABLE_AS_CHILD= 0x02
  } enum_pushability;

private:
  void init_pushability();

  bool find_field_parents(const AQP::Table_access* table,
                          const Item* key_item, 
                          const KEY_PART_INFO* key_part_info,
                          ndb_table_access_map& parents);
private:
  const AQP::Join_plan& m_plan;
  const AQP::Table_access* m_join_root;

  // Scope of tables covered by this pushed join
  ndb_table_access_map m_join_scope;

  // Scope of tables evaluated prior to 'm_join_root'
  // These are effectively const or params wrt. the pushed join
  ndb_table_access_map m_const_scope;

  struct pushed_tables
  {
    pushed_tables() : 
      m_maybe_pushable(0),
      m_parent(MAX_TABLES), 
      m_ancestors(), 
      m_last_scan_descendant(MAX_TABLES), 
      op(NULL) 
    {}

    int  m_maybe_pushable;
    uint m_parent;
    ndb_table_access_map m_ancestors;
    uint m_last_scan_descendant;

    const NdbQueryOperationDef* op;
  } m_tables[MAX_TABLES];

}; // class ndb_pushed_builder_ctx


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
  explicit ndb_pushed_join(const AQP::Join_plan& plan, 
			  const ndb_table_access_map& pushed_operations,
			  uint field_refs, Field* const fields[],
			  const NdbQueryDef* query_def);
  
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

  /** Get the no'th referred field of table access operations that executes
   * prior to the pushed join.*/
  Field* get_field_ref(uint no) const
  { 
    DBUG_ASSERT(no < m_field_count);
    return m_referred_fields[no]; 
  }

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
  st_table* m_tables[MAX_PUSHED_OPERATIONS];

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

