/*
   Copyright 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef ABSTRACT_QUERY_PLAN_H_INCLUDED
#define ABSTRACT_QUERY_PLAN_H_INCLUDED

struct st_join_table;
typedef st_join_table JOIN_TAB;
class Item;
class Item_field;
class Table_accessSet;
class Item_equal_iterator;

/**
  Abstract query plan (AQP) is an interface for examining certain aspects of 
  query plans without accessing mysqld internal classes (JOIN_TAB, SQL_SELECT 
  etc.) directly.

  AQP maps join execution plans, as represented by mysqld internals, to a set 
  of facade classes. Non-join operations such as sorting and aggregation is
  currently *not* modelled in the AQP.

  The AQP models an n-way join as a sequence of the n table access operations
  that the MySQL server would execute as part of its nested loop join 
  execution. (Each such table access operation is a scan of a table or index,
  or an index lookup.) For each lookup operation, it is possible to examine 
  the expression that represents each field in the key.

  A storage enging will typically use the AQP for finding sections of a join
  execution plan that may be executed in the engine rather than in mysqld. By 
  using the AQP rather than the mysqld internals directly, the coupling between
  the engine and mysqld is reduced.
*/
namespace AQP
{
  class Table_access;

  /**
    This class represents a query plan for an n-way join, in the form a 
    sequence of n table access operations that will execute as a nested loop 
    join.
  */
  class Join_plan
  {
    friend class Equal_set_iterator;
  public:

    explicit Join_plan(const JOIN_TAB* join_tab, uint access_count);

    ~Join_plan();

    const Table_access* get_table_access(uint access_no) const;

    /**
      @return The number of table access operations in the nested loop join.
    */
    uint get_access_count() const
    { 
      return m_access_count;
    }

    const Table_access* 
      get_referred_table_access(const Item_field* field_item) const;

  private:
    /** Number of table access operations.*/
    const uint m_access_count;

    /** 
      Array of the JOIN_TABs that are the internal representation of table
      access operations.
    */
    const JOIN_TAB* const m_join_tabs;

    Table_access* m_table_accesses;

    const JOIN_TAB* get_join_tab(uint join_tab_no) const;

    // No copying.
    Join_plan(const Join_plan&);
    Join_plan& operator=(const Join_plan&);
  }; 
  // class Join_plan


  /**
    This class is an iterator for iterating over sets of fields (columns) that
    should have the same value. For example, if the query is
    SELECT * FROM T1, T2, T3 WHERE T1.b = T2.a AND T2.a = T3.a
    then there would be such a set of {T1.b, T2.a, T3.a}.
  */
  class Equal_set_iterator
  {
  public:

    explicit Equal_set_iterator(const Join_plan* plan,
				const Item_field* field_item);

    ~Equal_set_iterator();

    const Item_field* next();

  private:

    /**
      The next Item_field, or NULL if end has been reached.
     */
    const Item_field* m_next;

    /**
      This class is implemented in terms of this mysqld internal class.
     */
    Item_equal_iterator* m_iterator;

    // No copying.
    Equal_set_iterator(const Equal_set_iterator&);
    Equal_set_iterator& operator=(const Equal_set_iterator&);
  }; 
  // class Equal_set_iterator

  /** The type of a table access operation. */
  enum enum_access_type
  {
    /** For default initialization.*/
    AT_VOID,
    AT_PRIMARY_KEY_LOOKUP,
    AT_UNIQUE_INDEX_LOOKUP,
    AT_ORDERED_INDEX_SCAN,
    AT_TABLE_SCAN,
    /**
      The access method has not yet been decided, or it has properties that
      otherwise prevents it from being pushed to a storage engine.
     */
    AT_OTHER
  };

  /**
    This class represents an access operation on a table, such as a table
    scan, or a scan or lookup via an index. A Table_access object is always
    owned by a Join_plan object, such that the life time of the Table_access 
    object ends when the life time of the owning Join_plan object ends.
   */
  class Table_access
  {
    friend class Join_plan;
    friend inline bool equal(const Table_access*, const Table_access*);
  public:

    /** Get the type of this operation.*/
    enum_access_type get_access_type() const
    {
      if (m_access_type == AT_VOID)
	compute_type_and_index();
      return m_access_type;
    }

    uint get_no_of_key_fields() const;

    const Item* get_key_field(uint field_no) const;

    const KEY_PART_INFO* get_key_part_info(uint field_no) const;

    uint get_access_no() const
    { 
      return m_tab_no;
    }

    int32 get_index_no() const;

    st_table* get_table() const;

    void dbug_print() const;

  private:

    /** The first access operation in the plan. */
    const JOIN_TAB* m_root_tab;

    /** This operation corresponds to m_root_tab[m_tab_no].*/
    uint m_tab_no;

    /** The type of this operation.*/
    mutable enum_access_type m_access_type;

    /** The index to use for this operation (if applicable )*/
    mutable int32 m_index_no;

    explicit Table_access(const JOIN_TAB* root_tab, uint tab_no);

    const JOIN_TAB* get_join_tab() const;

    void compute_type_and_index() const;

    explicit Table_access()
      :m_root_tab(NULL),
      m_tab_no(0),
      m_access_type(AT_VOID),
      m_index_no(-1)
    {}

    /** No copying*/
    Table_access(const Table_access&);
    Table_access& operator=(const Table_access&);
  }; 
  // class Table_access

  /**
    This class represents a subset of the access methods in a Join_plan.
   */
  class Table_access_set
  {
    friend inline bool equal(Table_access_set, Table_access_set);
    friend inline const Table_access_set intersection(Table_access_set,
						      Table_access_set);
  public:
    explicit Table_access_set() :m_map(0){};

    /** Add 'access' to the set.*/
    void add(const Table_access* access)
    {
      m_map|=  static_cast<table_map>(1) << access->get_access_no();
    }

    /** Check if the set cointains 'access'.*/
    bool contains(const Table_access* access) const
    {
      return (m_map & (static_cast<table_map>(1) << access->get_access_no()))
	!= 0;
    }

    /** Check if the set is empty.*/
    bool is_empty() const
    { 
      return m_map == 0;
    }

  private:
    /**
      A bit map of st_table::map id's. (These are unique for each join_tab
      in a plan.)
     */
    table_map m_map;
  }; 
  // class Table_access_set

  /**
    Get the n'th table access operation.
    @param access_no The index of the table access operation to fetch.
    @return The access_no'th table access operation.
  */
  inline const Table_access* Join_plan::get_table_access(uint access_no) const
  {
    DBUG_ASSERT(access_no < m_access_count);
    return m_table_accesses + access_no;
  }

  /**
    Get the number of the index to use for this access operation.
  */
  inline int32 Table_access::get_index_no() const
  {
    if (m_access_type == AT_VOID)
      compute_type_and_index();
	
    DBUG_ASSERT(m_access_type == AT_PRIMARY_KEY_LOOKUP ||
		  m_access_type == AT_UNIQUE_INDEX_LOOKUP ||
		  m_access_type == AT_ORDERED_INDEX_SCAN);
    return m_index_no;
  }

  inline bool equal(const Table_access* access_a,
		    const Table_access* access_b)
  {
    DBUG_ASSERT(access_a->m_root_tab == access_b->m_root_tab);
    return access_a->m_tab_no == access_b->m_tab_no;
  }

  /** Check if sets are identical.*/
  inline bool equal(Table_access_set set_a,
		    Table_access_set set_b)
  {
    return set_a.m_map == set_b.m_map;
  }

  /** Compute the intersection between two sets.*/
  inline const Table_access_set intersection(const Table_access_set set_a,
					     const Table_access_set set_b)
  {
    Table_access_set result;
    result.m_map= set_a.m_map & set_b.m_map;
    return result;
  }

}; 
// namespace AQP

#endif
