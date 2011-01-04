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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "rpl_filter.h"
#include <myisampack.h>
#include <errno.h>
#include "sql_select.h"

#include "abstract_query_plan.h"



namespace AQP
{

  /**
    @param join_tab Array of access methods constituting the nested loop join.
    @param access_count Length of array.
  */
  Join_plan::Join_plan(const JOIN* join)
   : m_join_tabs(join->join_tab),
     m_access_count(join->tables),
     m_table_accesses(NULL)
  {
    /*
      This combination is assumed not to appear. If it does, code must
      be written to handle it.
    */
    DBUG_ASSERT((m_join_tabs[0].use_quick != 2)
                || (m_join_tabs[0].type == JT_ALL)
                || (m_join_tabs[0].select == NULL)
                || (m_join_tabs[0].select->quick == NULL));

    m_table_accesses= new Table_access[m_access_count];
    for(uint i= 0; i < m_access_count; i++)
    {
      m_table_accesses[i].m_join_plan= this; 
      m_table_accesses[i].m_tab_no= i;
    }
  }

  Join_plan::~Join_plan()
  {
    delete[] m_table_accesses;
    m_table_accesses= NULL;
  }

  /** Get the JOIN_TAB of the n'th table access operation.*/
  const JOIN_TAB* Join_plan::get_join_tab(uint join_tab_no) const
  {
    DBUG_ASSERT(join_tab_no < m_access_count);
    return m_join_tabs + join_tab_no;
  }

  void
  Join_plan::find_skippabable_group_or_order() const
  {
    const
    JOIN* const join= m_join_tabs->join;

    if (join->const_tables < join->tables)
    {
      JOIN_TAB* join_head= join->join_tab+join->const_tables;
  
      m_group_by_filesort_is_skippable= join->group_optimized_away;
      m_order_by_filesort_is_skippable= join->skip_sort_order;

      /* A single row don't have to be sorted */
      if (join_head->type == JT_CONST  || 
          join_head->type == JT_SYSTEM || 
          join_head->type == JT_EQ_REF)
      {
        m_group_by_filesort_is_skippable= true;
        m_order_by_filesort_is_skippable= true;
      }
      else if (join->select_options & SELECT_BIG_RESULT)
      {
        /* Excluded from ordered index optimization */
      }
      else if (join->group_list && !m_group_by_filesort_is_skippable)
      {
        if (!join->tmp_table_param.quick_group || join->procedure)
        {
          /* Unsure how to handle - Is disabled in ::compute_type_and_index() */
        }
        if (join->simple_group)
        {
          /**
            test_if_skip_sort_order(...group_list...) already done by JOIN::optimize().
            As we still have a 'simple_group', GROUP BY has been optimized through an
            access path providing an ordered sequence as required by GROUP BY:

            Verify this assumption in ASSERT below:
          */
          DBUG_ASSERT(test_if_skip_sort_order(join_head, join->group_list,
                                              join->unit->select_limit_cnt, true, 
                                              &join_head->table->keys_in_use_for_group_by));
          m_group_by_filesort_is_skippable= true;
        }
      }
      else if (join->order && !m_order_by_filesort_is_skippable)
      {
        if (join->simple_order)
        {
          m_order_by_filesort_is_skippable= 
            test_if_skip_sort_order(join_head,
                                    join->order,
                                    join->unit->select_limit_cnt, false, 
                                    &join_head->table->keys_in_use_for_order_by);
        }
      }
    }
  }

  bool
  Join_plan::group_by_filesort_is_skippable() const
  {
    return (m_group_by_filesort_is_skippable == true);
  }

  bool
  Join_plan::order_by_filesort_is_skippable() const
  {
    return (m_order_by_filesort_is_skippable == true);
  }

  /**
    Determine join type between this table access and some other table
    access that preceeds it in the join plan..
  */
  enum_join_type 
  Table_access::get_join_type(const Table_access* predecessor) const
  {
    DBUG_ENTER("get_join_type");
    DBUG_ASSERT(get_access_no() > predecessor->get_access_no());

    if (get_join_tab()->table->pos_in_table_list->outer_join != 0)
    {
      /*
        This cover unnested outer joins such as 
        'select * from t1 left join t2 on t1.attr=t1.pk'.
       */
      DBUG_PRINT("info", ("JT_OUTER_JOIN between %s and %s",
                          predecessor->get_join_tab()->table->alias,
                          get_join_tab()->table->alias));
      DBUG_RETURN(JT_OUTER_JOIN);
    }

    const TABLE_LIST* const child_embedding= 
      get_join_tab()->table->pos_in_table_list->embedding;;

    if (child_embedding == NULL)
    {
      // 'this' is not on the inner side of any left join.
      DBUG_PRINT("info", ("JT_INNER_JOIN between %s and %s",
                          predecessor->get_join_tab()->table->alias,
                          get_join_tab()->table->alias));
      DBUG_RETURN(JT_INNER_JOIN);
    }

    DBUG_ASSERT(child_embedding->outer_join != 0);

    const TABLE_LIST *predecessor_embedding= 
      predecessor->get_join_tab()->table->pos_in_table_list->embedding;

    /*
      This covers the nested join case, i.e:
      <table reference> LEFT JOIN (<joined table>).
      
      TABLE_LIST objects form a tree where TABLE_LIST::emebedding points to
      the parent object. Now if child_embedding is non null and not an 
      ancestor of predecessor_embedding in the embedding tree, then 'this'
      must be on the inner side of some left join where 'predecessor' is on 
      the outer side.
     */
    while (true)
    {
      if (predecessor_embedding == child_embedding)
      {
        DBUG_PRINT("info", ("JT_INNER_JOIN between %s and %s",
                            predecessor->get_join_tab()->table->alias,
                            get_join_tab()->table->alias));
        DBUG_RETURN(JT_INNER_JOIN);
      }
      else if (predecessor_embedding == NULL)
      {
        /*
           We reached the root of the tree without finding child_embedding,
           so it must be in another branch and hence on the inner side of some
           left join where 'predecessor' is on the outer side.
         */
        DBUG_PRINT("info", ("JT_OUTER_JOIN between %s and %s",
                            predecessor->get_join_tab()->table->alias,
                            get_join_tab()->table->alias));
        DBUG_RETURN(JT_OUTER_JOIN);
      }
      // Iterate through ancestors of predecessor_embedding.
      predecessor_embedding = predecessor_embedding->embedding;
    }
  }

  /**
    Get the number of key values for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  uint Table_access::get_no_of_key_fields() const
  {
    DBUG_ASSERT(m_access_type == AT_PRIMARY_KEY ||
                m_access_type == AT_UNIQUE_KEY ||
                m_access_type == AT_MULTI_PRIMARY_KEY ||
                m_access_type == AT_MULTI_UNIQUE_KEY ||
                m_access_type == AT_ORDERED_INDEX_SCAN); // Used as 'range scan'
    return get_join_tab()->ref.key_parts;
  }

  /**
    Get the field_no'th key values for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const Item* Table_access::get_key_field(uint field_no) const
  {
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    return get_join_tab()->ref.items[field_no];
  }

  /**
    Get the field_no'th KEY_PART_INFO for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const KEY_PART_INFO* Table_access::get_key_part_info(uint field_no) const
  {
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    const KEY* key= &get_join_tab()->table->key_info[get_join_tab()->ref.key];
    return &key->key_part[field_no];
  }

  /**
    Get the table that this operation accesses.
  */
  st_table* Table_access::get_table() const
  {
    return get_join_tab()->table;
  }

  /** Get the JOIN_TAB object that corresponds to this operation.*/
  const JOIN_TAB* Table_access::get_join_tab() const
  {
    return m_join_plan->get_join_tab(m_tab_no);
  }

  /**
    Write an entry in the trace file about the contents of this object.
  */
  void Table_access::dbug_print() const
  {
    DBUG_PRINT("info", ("type:%d", get_join_tab()->type));
    DBUG_PRINT("info", ("ref.key:%d", get_join_tab()->ref.key));
    DBUG_PRINT("info", ("ref.key_parts:%d", get_join_tab()->ref.key_parts));
    DBUG_PRINT("info", ("ref.key_length:%d", get_join_tab()->ref.key_length));

    DBUG_PRINT("info", ("order:%p", get_join_tab()->join->order));
    DBUG_PRINT("info", ("skip_sort_order:%d",
                        get_join_tab()->join->skip_sort_order));
    DBUG_PRINT("info", ("no_order:%d", get_join_tab()->join->no_order));
    DBUG_PRINT("info", ("simple_order:%d", get_join_tab()->join->simple_order));

    DBUG_PRINT("info", ("group:%d", get_join_tab()->join->group));
    DBUG_PRINT("info", ("group_list:%p", get_join_tab()->join->group_list));
    DBUG_PRINT("info", ("simple_group:%d", get_join_tab()->join->simple_group));
    DBUG_PRINT("info", ("group_optimized_away:%d",
                        get_join_tab()->join->group_optimized_away));

    DBUG_PRINT("info", ("full_join:%d", get_join_tab()->join->full_join));
    DBUG_PRINT("info", ("need_tmp:%d", get_join_tab()->join->need_tmp));
    DBUG_PRINT("info", ("select_distinct:%d",
                        get_join_tab()->join->select_distinct));

    DBUG_PRINT("info", ("use_quick:%d", get_join_tab()->use_quick));
    DBUG_PRINT("info", ("index:%d", get_join_tab()->index));
    DBUG_PRINT("info", ("quick:%p", get_join_tab()->quick));
    DBUG_PRINT("info", ("select:%p", get_join_tab()->select));
    if (get_join_tab()->select && get_join_tab()->select->quick)
    {
      DBUG_PRINT("info", ("select->quick->get_type():%d",
                          get_join_tab()->select->quick->get_type()));
    }
  }


  /**
    Compute the access type and index (if apliccable) of this operation .
  */
  void Table_access::compute_type_and_index() const
  {
    DBUG_ENTER("Table_access::compute_type_and_index");
    const JOIN_TAB* const join_tab= get_join_tab();
    JOIN* const join= join_tab->join;

    /**
     * There are some JOIN arguments we don't fully understand or has 
     * not yet invested time into exploring pushability of:
     */
    if (join->procedure)
    {
      m_access_type= AT_OTHER;
      m_other_access_reason = 
        "'PROCEDURE'-clause post processing cannot be pushed.";
      DBUG_VOID_RETURN;
    }
    
    if (join->group_list && !join->tmp_table_param.quick_group)
    {
      m_access_type= AT_OTHER;
      m_other_access_reason = 
        "GROUP BY cannot be done using index on grouped columns.";
      DBUG_VOID_RETURN;
    }

    /* Tables below 'const_tables' has been const'ified, or entirely
     * optimized away due to 'impossible WHERE/ON'
     */
    if (join_tab < join->join_tab+join->const_tables)
    {
      DBUG_PRINT("info", ("Operation %d is const-optimized.", m_tab_no));
      m_access_type= AT_FIXED;
      DBUG_VOID_RETURN;
    }

    /* First non-const table may provide 'simple' ordering for entire join */
    if (join_tab == join->join_tab+join->const_tables)
    {
      m_join_plan->find_skippabable_group_or_order();
    }

    /*
      Identify the type of access operation and the index to use (if any).
    */
    switch (join_tab->type)
    {
    case JT_EQ_REF:
    case JT_CONST:
      m_index_no= join_tab->ref.key;

      if (m_index_no == static_cast<int>(join_tab->table->s->primary_key))
      {
        DBUG_PRINT("info", ("Operation %d is a primary key lookup.", m_tab_no));
        m_access_type= AT_PRIMARY_KEY;
      }
      else
      {
        DBUG_PRINT("info", ("Operation %d is a unique index lookup.",
                            m_tab_no));
        m_access_type= AT_UNIQUE_KEY;
      }
      break;

    case JT_REF:
    {
      DBUG_ASSERT(join_tab->ref.key >= 0);
      DBUG_ASSERT(join_tab->ref.key < MAX_KEY);
      m_index_no= join_tab->ref.key;

      /*
        All parts of a key are specified for an unique index -> access is a key lookup.
      */
      const KEY *key_info= join_tab->table->s->key_info;
      if (key_info[m_index_no].key_parts == join_tab->ref.key_parts  &&
         (key_info[m_index_no].flags & (HA_NOSAME | HA_END_SPACE_KEY)) == HA_NOSAME)
      {
        m_access_type= 
          (m_index_no == static_cast<int32>(join_tab->table->s->primary_key)) 
              ? AT_PRIMARY_KEY
              : AT_UNIQUE_KEY;
        DBUG_PRINT("info", ("Operation %d is an unique key referrence.", m_tab_no));
      }
      else
      {
        DBUG_ASSERT(join_tab->ref.key_parts > 0);
        DBUG_ASSERT(join_tab->ref.key_parts <= key_info[m_index_no].key_parts);
        m_access_type= AT_ORDERED_INDEX_SCAN;
        DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      }
      break;
    }
    case JT_NEXT:
      DBUG_ASSERT(join_tab->index < MAX_KEY);
      m_index_no=    join_tab->index;
      m_access_type= AT_ORDERED_INDEX_SCAN;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_ALL:
      if (join_tab->use_quick == 2)
      {
        /*
          use_quick == 2 means that the decision on which access method to use
          will be taken late (as rows from the preceeding operation arrive).
          This operation is therefor not pushable.
        */
        DBUG_PRINT("info",
                   ("Operation %d has 'use_quick == 2' -> not pushable",
                    m_tab_no));
        m_access_type= AT_UNDECIDED;
        m_index_no=    -1;
      }
      else
      {
        if (join_tab->select != NULL &&
            join_tab->select->quick != NULL)
        {
          QUICK_SELECT_I *quick= join_tab->select->quick;

          /** QUICK_SELECT results in execution of MRR (Multi Range Read).
           *  Depending on each range, it may require execution of
           *  either a PK-lookup or a range scan. To cover both of 
           *  these we may need to prepare both a pushed lookup join
           *  and a pushed range scan. Currently we handle it as
           *  a range scan and convert e PK lookup to a (closed-) range
           *  whenever required.
           **/

          const KEY *key_info= join_tab->table->s->key_info;
          DBUG_EXECUTE("info", quick->dbug_dump(0, TRUE););

          // Temporary assert as we are still investigation the relation between 
          // 'quick->index == MAX_KEY' and the different quick_types
          DBUG_ASSERT ((quick->index == MAX_KEY)  ==
                        ((quick->get_type() == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE) ||
                         (quick->get_type() == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT) ||
                         (quick->get_type() == QUICK_SELECT_I::QS_TYPE_ROR_UNION)));

          // JT_INDEX_MERGE: We have a set of qualifying PKs as root of pushed joins
          if (quick->index == MAX_KEY) 
          {
            m_index_no=    join_tab->table->s->primary_key;
            m_access_type= AT_MULTI_PRIMARY_KEY;    // Multiple PKs are produced by merge
          }

          // Else JT_RANGE: May be both exact PK and/or index scans when sorted index available
          else if (quick->index == join_tab->table->s->primary_key)
          {
            m_index_no= quick->index;
            if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH)
              m_access_type= AT_MULTI_PRIMARY_KEY; // MRR w/ multiple PK's
            else
              m_access_type= AT_MULTI_MIXED;       // MRR w/ both range and PKs
          }
          else
          {
            m_index_no= quick->index;
            if (key_info[m_index_no].algorithm == HA_KEY_ALG_HASH)
              m_access_type= AT_MULTI_UNIQUE_KEY; // MRR with multiple unique keys
            else
              m_access_type= AT_MULTI_MIXED;      // MRR w/ both range and unique keys
          }
        }
        else
        {
          DBUG_PRINT("info", ("Operation %d is a table scan.", m_tab_no));
          m_access_type= AT_TABLE_SCAN;
        }
      }
      break;

    default:
      /*
        Other join_types either cannot be pushed or the code analyze them is
        not yet in place.
      */
      DBUG_PRINT("info",
                 ("Operation %d has join_type %d. -> Not pushable.",
                  m_tab_no, join_tab->type));
      m_access_type= AT_OTHER;
      m_index_no=    -1;
      m_other_access_reason = "This table access method can not be pushed.";
      break;
    }
    DBUG_VOID_RETURN;
  }
  // Table_access::compute_type_and_index()


  Table_access::Table_access()
    :m_join_plan(NULL),
     m_tab_no(0),
     m_access_type(AT_VOID),
     m_other_access_reason(NULL),
     m_index_no(-1)
  {}

  /**
    @return True iff ordered index access is *required* from this operation. 
  */
  bool Table_access::is_fixed_ordered_index() const
  {
    const JOIN_TAB* const join_tab= get_join_tab();

    /* For the QUICK_SELECT_I classes we can disable ordered index usage by
     * setting 'QUICK_SELECT_I::sorted = false'.
     * However, QUICK_SELECT_I::QS_TYPE_RANGE_DESC is special as its 
     * internal implementation requires its 'multi-ranges' to be retrieved
     * in (descending) sorted order from the underlying table.
     */
    if (join_tab->select != NULL &&
        join_tab->select->quick != NULL)
    {
      QUICK_SELECT_I *quick= join_tab->select->quick;
      return (quick->get_type() == QUICK_SELECT_I::QS_TYPE_RANGE_DESC);
    }
    return false;
  }

  /**
    Check if the results from this operation will joined with results 
    from the next operation using a join buffer (instead of plain nested loop).
    @return True if using a join buffer. 
  */
  bool Table_access::uses_join_cache() const
  {
    return get_join_tab()->next_select == sub_select_cache;
  }

  /**
    @param plan Iterate over fields within this plan.
    @param field_item Iterate over Item_fields equal to this.
  */
  Equal_set_iterator::Equal_set_iterator(const Join_plan* plan,
                                         const Item_field* field_item)
    :m_next(NULL),
     m_iterator(NULL)
  {
    DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);

    COND_EQUAL* const cond_equal=
      plan->get_join_tab(0)->join->cond_equal;

    if (cond_equal!=NULL)
    {
      Item_equal* item_equal= NULL;
      if (field_item->item_equal == NULL)
        item_equal=
          const_cast<Item_field*>(field_item)->find_item_equal(cond_equal);
      else
        item_equal= field_item->item_equal;

      if (item_equal != NULL)
      {
        m_iterator= new Item_equal_iterator(*item_equal);
        m_next=     (*m_iterator)++;
      }
    }
  }


  Equal_set_iterator::~Equal_set_iterator()
  {
    delete m_iterator;
  }


  /**
    Get the next Item_field and advance the iterator.
    @return A pointer to the next Item_field, or NULL if the end has been
    reached.
  */
  const Item_field*
  Equal_set_iterator::next()
  {
    const Item_field* result= m_next;
    if (m_next != NULL)
    {
      if (m_iterator == NULL)
        m_next= NULL;
      else
      {
        if (m_iterator->is_last())
          m_next= NULL;
        else
          m_next= (*m_iterator)++;
      }
    }
    return result;
  }

};
// namespace AQP
