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
  Join_plan::Join_plan(const JOIN_TAB* join_tab, uint access_count)
    :m_access_count(access_count),
     m_join_tabs(join_tab),
     m_table_accesses(new Table_access[access_count])
  {
    /*
      This combination is assumed not to appear. If it does, code must
      be written to handle it.
    */
    DBUG_ASSERT((m_join_tabs[0].use_quick != 2)
                || (m_join_tabs[0].type == JT_ALL)
                || (m_join_tabs[0].select == NULL)
                || (m_join_tabs[0].select->quick == NULL));

    for(uint i= 0; i < access_count; i++)
    {
      m_table_accesses[i].m_root_tab= join_tab; 
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

  /**
    Find the table that a given Item_field refers to. 
    Returns a negative value if field_item does not refer a
    table within this Join_plan.
  */
  const Table_access*
  Join_plan::get_referred_table_access(const Item_field* field_item) const
  {
    DBUG_ASSERT(field_item->type() == Item::FIELD_ITEM);

    for (uint i= 0; i < get_access_count(); i++)
    {
      if (get_join_tab(i)->table->map == field_item->field->table->map)
        return (m_table_accesses + i);
    }
    return NULL;
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
                m_access_type == AT_MULTI_UNIQUE_KEY);
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
    Compute the access type and index (if apliccable) of this operation .
  */
  void Table_access::compute_type_and_index() const
  {
    DBUG_ENTER("Table_access::compute_type_and_index");
    const JOIN_TAB* const join_tab= get_join_tab();

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
    case JT_REF_OR_NULL:
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
        m_access_type= AT_OTHER;
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
            m_access_type= AT_OTHER;
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
      break;
    }
    DBUG_VOID_RETURN;
  }
  // Table_access::compute_type_and_index()


  Table_access::Table_access()
    :m_root_tab(NULL),
     m_tab_no(0),
     m_access_type(AT_VOID),
     m_index_no(-1)
  {}

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
