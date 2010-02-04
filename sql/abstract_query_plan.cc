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
  Query_plan::Query_plan(const JOIN_TAB* join_tab, int32 access_count)
    :m_access_count(access_count),
    m_join_tabs(join_tab)
  {
    /*
      This combination is assumed not to appear. If it does, code must
      be written to handle it.
    */
    DBUG_ASSERT((m_join_tabs[0].use_quick != 2)
                || (m_join_tabs[0].type == JT_ALL)
                || (m_join_tabs[0].select == NULL)
                || (m_join_tabs[0].select->quick == NULL));
  }

  /** Get the JOIN_TAB of the n'th table access operation.*/
  const JOIN_TAB* Query_plan::get_join_tab(int32 join_tab_no) const
  {
    DBUG_ASSERT(join_tab_no < m_access_count);
    return m_join_tabs + join_tab_no;
  }

  /**
    Find the table that a given Item_field refers to. 
    Returns a negative value if field_item does not refer a
    table within this Query_plan.
  */
  int32
  Query_plan::get_referred_table(const Item_field* field_item) const
  {
    DBUG_ENTER("Query_plan::get_referred_table");
    DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);

    for (int32 i= 0; i < get_access_count(); i++)
    {
      if (get_join_tab(i)->table->map == field_item->field->table->map)
        DBUG_RETURN(i);
    }
    DBUG_RETURN(-1);
  }

  /**
    Get the number of key values for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  int32 Table_access::get_no_of_key_fields() const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    return get_join_tab()->ref.key_parts;
  }

  /**
    Get the field_no'th key values for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const Item* Table_access::get_key_field(int32 field_no) const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    return get_join_tab()->ref.items[field_no];
  }

  /**
    Get the field_no'th KEY_PART_INFO for this operation. It is an error
    to call this method on an operation that is not an index lookup
    operation.
  */
  const KEY_PART_INFO* Table_access::get_key_part_info(int32 field_no) const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    const KEY* key= &get_join_tab()->table->key_info[get_join_tab()->ref.key];
    return &key->key_part[field_no];
  }

  /**
    Get the name of the table that this operation accesses.
  */
  const char* Table_access::get_table_name() const
  {
    return get_join_tab()->table->alias;
  }

  /**
    Get the handler object of this table access operation.
  */
  handler* Table_access::get_handler() const
  {
    return get_join_tab()->table->file;
  }

  /**
    Get the table that this operation accesses.
  */
  st_table* Table_access::get_table() const
  {
    return get_join_tab()->table;
  }

  /** Get the JOIN_TAB object that corresponds to this operation.*/
  inline const JOIN_TAB* Table_access::get_join_tab() const
  {
    return m_root_tab + m_tab_no;
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
    if (get_join_tab()->select)
      DBUG_PRINT("info", ("select->quick:%p",
                          get_join_tab()->select->quick));
  }


  /**
    @param root_tab The first access operation in the plan.
    @param tab_no This operation corresponds to root_tab[tab_no].
  */
  Table_access::Table_access(const JOIN_TAB* root_tab, int32 tab_no)
    :m_root_tab(root_tab),
     m_tab_no(tab_no)
  {
    DBUG_ENTER("Table_access::Table_access");
    /*
      Identify the type of access operation and the index to use (if any).
    */
    switch(get_join_tab()->type)
    {
    case JT_EQ_REF:
    case JT_CONST:
      m_index_no= get_join_tab()->ref.key;

      if (m_index_no == (int32)get_join_tab()->table->s->primary_key)
      {
        DBUG_PRINT("info", ("Operation %d is a primary key lookup.", m_tab_no));
        m_access_type= AT_PrimaryKeyLookup;
      }
      else
      {
        DBUG_PRINT("info", ("Operation %d is a unique index lookup.",
                            m_tab_no));
        m_access_type= AT_UniqueIndexLookup;
      }
      break;

    case JT_REF:
    case JT_REF_OR_NULL:
      DBUG_ASSERT(get_join_tab()->ref.key >= 0);
      DBUG_ASSERT(get_join_tab()->ref.key < MAX_KEY);
      m_index_no= get_join_tab()->ref.key;
      m_access_type= AT_OrderedIndexScan;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_NEXT:
      DBUG_ASSERT(get_join_tab()->index < MAX_KEY);
      m_index_no=    get_join_tab()->index;
      m_access_type= AT_OrderedIndexScan;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_ALL:
      if (get_join_tab()->use_quick == 2)
      {
        /*
          use_quick == 2 means that the decision on which access method to use
          will be taken late (as rows from the preceeding operation arrive).
          This operation is therefor not pushable.
        */
        DBUG_PRINT("info",
                   ("Operation %d has 'use_quick == 2' -> not pushable",
                    m_tab_no));
        m_access_type= AT_Other;
        m_index_no=    -1;
      }
      else
      {
        if (get_join_tab()->select != NULL &&
            get_join_tab()->select->quick != NULL)
        {
          QUICK_SELECT_I *quick= get_join_tab()->select->quick;
          DBUG_EXECUTE("info", quick->dbug_dump(0, TRUE););
          if (quick->index < MAX_KEY)
          {
            m_index_no=    quick->index;
            m_access_type= AT_OrderedIndexScan;
          }
          else
          {
            // No scanable indexes; use PK, typically 'pk in (X,Y,Z)'
            DBUG_PRINT("info", ("Operation %d is PK-MRR", m_tab_no));
            /*
              Check that this is either:
              - Multi-range read like <primary key> IN (X,Y,Z...).
              - Index merge.
             */
            DBUG_ASSERT(quick->index == get_join_tab()->table->s->primary_key ||
                        quick->index == MAX_KEY);
            m_index_no=    get_join_tab()->table->s->primary_key;
            m_access_type= AT_PrimaryKeyLookup;
          }
        }
        else
        {
          DBUG_PRINT("info", ("Operation %d is a table scan.", m_tab_no));
          m_access_type= AT_TableScan;
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
                  m_tab_no, get_join_tab()->type));
      m_access_type= AT_Other;
      m_index_no=    -1;
      break;
    }
    DBUG_VOID_RETURN;
  }
  // Table_access::Table_access()


  /**
    @param plan Iterate over fields within this plan.
    @param field_item Iterate over Item_fields equal to this.
  */
  Equal_set_iterator::Equal_set_iterator(const Query_plan* plan,
                                         const Item_field* field_item)
    :m_next(NULL),
     m_iterator(NULL)
  {
    DBUG_ENTER("Equal_set_iterator::Equal_set_iterator");
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
    DBUG_VOID_RETURN;
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
