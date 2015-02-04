/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "abstract_query_plan.h"

#include "sql_optimizer.h"    // JOIN

namespace AQP
{

  /**
    @param join_tab Array of access methods constituting the nested loop join.
    @param access_count Length of array.
  */
  Join_plan::Join_plan(const JOIN* join)
   : m_qep_tabs(join->qep_tab),
     m_access_count(join->primary_tables),
     m_table_accesses(NULL)
  {
    /*
      This combination is assumed not to appear. If it does, code must
      be written to handle it.
    */
    DBUG_ASSERT(!m_qep_tabs[0].dynamic_range()
                || (m_qep_tabs[0].type() == JT_ALL)
                || (m_qep_tabs[0].quick() == NULL));

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

  /** Get the QEP_TAB of the n'th table access operation.*/
  const QEP_TAB* Join_plan::get_qep_tab(uint qep_tab_no) const
  {
    DBUG_ASSERT(qep_tab_no < m_access_count);
    return m_qep_tabs + qep_tab_no;
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

    const QEP_TAB* const me= get_qep_tab();
    const plan_idx first_inner= me->first_inner();
    if (first_inner == NO_PLAN_IDX)
    {
      // 'this' is not outer joined with any table.
      DBUG_PRINT("info", ("JT_INNER_JOIN'ed table %s",
                          me->table()->alias));
      DBUG_RETURN(JT_INNER_JOIN);
    }

    /**
     * Fall Through: 'this' is a member in an outer join,
     * but 'predecessor' may still be embedded in the same
     * inner join as 'this'.
     */
    const plan_idx last_inner= me->join()->qep_tab[first_inner].last_inner();
    if (predecessor->get_access_no() >= static_cast<uint>(first_inner) &&
        predecessor->get_access_no() <= static_cast<uint>(last_inner))
    {
      DBUG_PRINT("info", ("JT_INNER_JOIN between %s and %s",
                          predecessor->get_qep_tab()->table()->alias,
                          me->table()->alias));
      DBUG_RETURN(JT_INNER_JOIN);
    }
    else
    {
      DBUG_PRINT("info", ("JT_OUTER_JOIN between %s and %s",
                          predecessor->get_qep_tab()->table()->alias,
                          me->table()->alias));
      DBUG_RETURN(JT_OUTER_JOIN);
    }
  } //Table_access::get_join_type

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
    return get_qep_tab()->ref().key_parts;
  }

  /**
    Get the field_no'th key values for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const Item* Table_access::get_key_field(uint field_no) const
  {
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    return get_qep_tab()->ref().items[field_no];
  }

  /**
    Get the field_no'th KEY_PART_INFO for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const KEY_PART_INFO* Table_access::get_key_part_info(uint field_no) const
  {
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    const KEY* key= &get_qep_tab()->table()->key_info[get_qep_tab()->ref().key];
    return &key->key_part[field_no];
  }

  /**
    Get the table that this operation accesses.
  */
  TABLE* Table_access::get_table() const
  {
    return get_qep_tab()->table();
  }

  double Table_access::get_fanout() const
  {
    switch (get_access_type())
    {
      case AT_PRIMARY_KEY:
      case AT_UNIQUE_KEY:
        return 1.0;

      case AT_ORDERED_INDEX_SCAN:
        DBUG_ASSERT(get_qep_tab()->position());
        DBUG_ASSERT(get_qep_tab()->position()->rows_fetched > 0.0);
        return get_qep_tab()->position()->rows_fetched;

      case AT_MULTI_PRIMARY_KEY:
      case AT_MULTI_UNIQUE_KEY:
      case AT_MULTI_MIXED:
        DBUG_ASSERT(get_qep_tab()->position());
        DBUG_ASSERT(get_qep_tab()->position()->rows_fetched > 0.0);
        return get_qep_tab()->position()->rows_fetched;

      case AT_TABLE_SCAN:
        DBUG_ASSERT(get_qep_tab()->table()->file->stats.records > 0.0);
        return static_cast<double>(get_qep_tab()->table()->file->stats.records);

      default:
        return 99999999.0;
    }
  }

  /** Get the QEP_TAB object that corresponds to this operation.*/
  const QEP_TAB* Table_access::get_qep_tab() const
  {
    return m_join_plan->get_qep_tab(m_tab_no);
  }

  /** Get the Item_equal's set relevant for the specified 'Item_field' */
  Item_equal*
  Table_access::get_item_equal(const Item_field* field_item) const
  {
    DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);

    COND_EQUAL* const cond_equal = get_qep_tab()->join()->cond_equal;
    if (cond_equal!=NULL)
    {
      return (field_item->item_equal != NULL)
               ? field_item->item_equal
               : const_cast<Item_field*>(field_item)->find_item_equal(cond_equal);
    }
    return NULL;
  }

  /**
    Write an entry in the trace file about the contents of this object.
  */
  void Table_access::dbug_print() const
  {
    DBUG_PRINT("info", ("type:%d", get_qep_tab()->type()));
    DBUG_PRINT("info", ("ref().key:%d", get_qep_tab()->ref().key));
    DBUG_PRINT("info", ("ref().key_parts:%d", get_qep_tab()->ref().key_parts));
    DBUG_PRINT("info", ("ref().key_length:%d", get_qep_tab()->ref().key_length));

    DBUG_PRINT("info", ("order:%p", get_qep_tab()->join()->order.order));
    DBUG_PRINT("info", ("skip_sort_order:%d",
                        get_qep_tab()->join()->skip_sort_order));
    DBUG_PRINT("info", ("no_order:%d", get_qep_tab()->join()->no_order));
    DBUG_PRINT("info", ("simple_order:%d", get_qep_tab()->join()->simple_order));

    DBUG_PRINT("info", ("group:%d", get_qep_tab()->join()->grouped));
    DBUG_PRINT("info", ("group_list:%p", get_qep_tab()->join()->group_list.order));
    DBUG_PRINT("info", ("simple_group:%d", get_qep_tab()->join()->simple_group));
    DBUG_PRINT("info", ("group_optimized_away:%d",
                        get_qep_tab()->join()->group_optimized_away));

    DBUG_PRINT("info", ("need_tmp:%d", get_qep_tab()->join()->need_tmp));
    DBUG_PRINT("info", ("select_distinct:%d",
                        get_qep_tab()->join()->select_distinct));

    DBUG_PRINT("info", ("dynamic_range:%d", (int)get_qep_tab()->dynamic_range()));
    DBUG_PRINT("info", ("index:%d", get_qep_tab()->index()));
    DBUG_PRINT("info", ("quick:%p", get_qep_tab()->quick()));
    if (get_qep_tab()->quick())
    {
      DBUG_PRINT("info", ("quick->get_type():%d",
                          get_qep_tab()->quick()->get_type()));
    }
  }


  /**
    Compute the access type and index (if apliccable) of this operation .
  */
  void Table_access::compute_type_and_index() const
  {
    DBUG_ENTER("Table_access::compute_type_and_index");
    const QEP_TAB* const qep_tab= get_qep_tab();
    JOIN* const join= qep_tab->join();

    /**
     * OLEJA: I think this restriction can be removed
     * now as WL5558 and other changes has cleaned up the 
     * ORDER/GROUP BY optimize + execute path.
     */
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
    if (qep_tab < join->qep_tab + join->const_tables)
    {
      DBUG_PRINT("info", ("Operation %d is const-optimized.", m_tab_no));
      m_access_type= AT_FIXED;
      DBUG_VOID_RETURN;
    }

    /*
      Identify the type of access operation and the index to use (if any).
    */
    switch (qep_tab->type())
    {
    case JT_EQ_REF:
      m_index_no= qep_tab->ref().key;

      if (m_index_no == static_cast<int>(qep_tab->table()->s->primary_key))
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
      DBUG_ASSERT(qep_tab->ref().key >= 0);
      DBUG_ASSERT((uint)qep_tab->ref().key < MAX_KEY);
      m_index_no= qep_tab->ref().key;

      /*
        All parts of a key are specified for an unique index -> access is a key lookup.
      */
      const KEY *key_info= qep_tab->table()->s->key_info;
      if (key_info[m_index_no].user_defined_key_parts ==
          qep_tab->ref().key_parts &&
          key_info[m_index_no].flags & HA_NOSAME)
      {
        m_access_type= 
          (m_index_no == static_cast<int32>(qep_tab->table()->s->primary_key)) 
              ? AT_PRIMARY_KEY
              : AT_UNIQUE_KEY;
        DBUG_PRINT("info", ("Operation %d is an unique key referrence.", m_tab_no));
      }
      else
      {
        DBUG_ASSERT(qep_tab->ref().key_parts > 0);
        DBUG_ASSERT(qep_tab->ref().key_parts <=
                    key_info[m_index_no].user_defined_key_parts);
        m_access_type= AT_ORDERED_INDEX_SCAN;
        DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      }
      break;
    }
    case JT_INDEX_SCAN:
      DBUG_ASSERT(qep_tab->index() < MAX_KEY);
      m_index_no=    qep_tab->index();
      m_access_type= AT_ORDERED_INDEX_SCAN;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_ALL:
    case JT_RANGE:
    case JT_INDEX_MERGE:
      if (qep_tab->dynamic_range())
      {
        /*
          It means that the decision on which access method to use
          will be taken late (as rows from the preceding operation arrive).
          This operation is therefor not pushable.
        */
        DBUG_PRINT("info",
                   ("Operation %d has 'dynamic range' -> not pushable",
                    m_tab_no));
        m_access_type= AT_UNDECIDED;
        m_index_no=    -1;
      }
      else
      {
        if (qep_tab->quick() != NULL)
        {
          QUICK_SELECT_I *quick= qep_tab->quick();

          /** QUICK_SELECT results in execution of MRR (Multi Range Read).
           *  Depending on each range, it may require execution of
           *  either a PK-lookup or a range scan. To cover both of 
           *  these we may need to prepare both a pushed lookup join
           *  and a pushed range scan. Currently we handle it as
           *  a range scan and convert e PK lookup to a (closed-) range
           *  whenever required.
           **/

          const KEY *key_info= qep_tab->table()->s->key_info;
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
            m_index_no=    qep_tab->table()->s->primary_key;
            m_access_type= AT_MULTI_PRIMARY_KEY;    // Multiple PKs are produced by merge
          }

          // Else JT_RANGE: May be both exact PK and/or index scans when sorted index available
          else if (quick->index == qep_tab->table()->s->primary_key)
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

    case JT_CONST:
    case JT_SYSTEM:
    default:
      /*
        Other join_types either cannot be pushed or the code analyze them is
        not yet in place.
      */
      DBUG_PRINT("info",
                 ("Operation %d has join_type %d. -> Not pushable.",
                  m_tab_no, qep_tab->type()));
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
    Check if the results from this operation will joined with results 
    from the next operation using a join buffer (instead of plain nested loop).
    @return True if using a join buffer. 
  */
  bool Table_access::uses_join_cache() const
  {
    return get_qep_tab()->op &&
      get_qep_tab()->op->type() == QEP_operation::OT_CACHE;
  }

  /**
    Check if 'FirstMatch' strategy is used for this table and return
    the last table 'firstmatch' will skip over.
    The tables ['last_skipped'..'this'] will form a range of tables
    which we skipped when a 'firstmatch' is found
  */
  const Table_access* Table_access::get_firstmatch_last_skipped() const
  {
    const QEP_TAB* const qep_tab= get_qep_tab();
    if (qep_tab->do_firstmatch())
    {
      DBUG_ASSERT(qep_tab->firstmatch_return < qep_tab->idx());
      const uint firstmatch_last_skipped= 
        qep_tab->firstmatch_return + 1;

      return m_join_plan->get_table_access(firstmatch_last_skipped);
    }
    return NULL;
  }

  /**
   Check if this table will be presorted to an intermediate record storage
   before it is joined with its siblings.
  */
  bool Table_access::filesort_before_join() const
  {
    if (m_access_type == AT_PRIMARY_KEY ||
        m_access_type == AT_UNIQUE_KEY)
    {
      return false;
    }

    const QEP_TAB* const qep_tab= get_qep_tab();
    JOIN* const join= qep_tab->join();

    /**
     Table will be presorted before joining with child tables, if:
      1) This is the first non-const table
      2) There are more tables to be joined
      3) It is not already decide to write entire join result to temp.
      4a) The GROUP BY is 'simple' and does not match an orderd index
      4b) The ORDER BY is 'simple' and does not match an orderd index

     A 'simple' order/group by contain only column references to
     the first non-const table
    */
    if (qep_tab == join->qep_tab + join->const_tables &&    // First non-const table
        !join->plan_is_const())                         // There are more tables
    {
      if (join->need_tmp)
        return false;
      else if (join->group_list && join->simple_group)
        return (join->ordered_index_usage!=JOIN::ordered_index_group_by);
      else if (join->order && join->simple_order)
        return (join->ordered_index_usage!=JOIN::ordered_index_order_by);
      else
        return false;
    }
    return false;
  }

  void Table_access::set_pushed_table_access_method() const
  {
    // Remove the QEP_TABs constness allowing the QEP_TAB
    // instance for this part ot the join to be modified
    QEP_TAB* const qep_tab= const_cast<QEP_TAB*>(get_qep_tab());
    qep_tab->set_pushed_table_access_method();
  }

};
// namespace AQP
