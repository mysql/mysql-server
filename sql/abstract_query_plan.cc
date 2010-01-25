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

#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"
#endif

#include "abstract_query_plan.h"



namespace AQP
{

  /**************
   * Query_plan methods.
   *************/
  
  Query_plan::Query_plan(const JOIN_TAB* join_tab, int access_count)
    :m_access_count(access_count),
     m_join_tabs(join_tab)
  { 

    /* This combination is assumed not to appear. If it does, code must
       be written to handle it.*/
    DBUG_ASSERT((m_join_tabs[0].use_quick != 2)
                || (m_join_tabs[0].type == JT_ALL)
                || (m_join_tabs[0].select == NULL) 
                || (m_join_tabs[0].select->quick == NULL));
  }

  const Table_access Query_plan::get_table_access(int access_no) const      
  { 
    DBUG_ASSERT(m_join_tabs != NULL);
    DBUG_ASSERT(access_no < m_access_count);
    return Table_access(m_join_tabs, access_no);
  }

  const JOIN_TAB& Query_plan::get_join_tab(int join_tab_no) const
  {
    DBUG_ASSERT(join_tab_no < m_access_count);
    return m_join_tabs[join_tab_no];
  }

  const Table_access 
  Query_plan::get_referred_table_access(const Item_field& field_item) const
  {
    DBUG_ENTER("Query_plan::get_referred_table_access");
    DBUG_ASSERT(field_item.type() == Item::FIELD_ITEM);

    int i = 0;
    while(i < get_access_count() 
          && get_join_tab(i).table->map != field_item.field->table->map)
    {
      i++;
    }

    DBUG_ASSERT(i < get_access_count());
    DBUG_RETURN(get_table_access(i));
  }

  /**************
   * Table_access methods.
   *************/

  int Table_access::get_no_of_key_fields() const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    return get_join_tab().ref.key_parts;
  }

  const Item& Table_access::get_key_field(int field_no) const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    return *get_join_tab().ref.items[field_no];
  } 
  
  const KEY_PART_INFO& Table_access::get_key_part_info(int field_no) const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    DBUG_ASSERT(field_no < get_no_of_key_fields());
    const KEY& key = get_join_tab().table->key_info[get_join_tab().ref.key];
    return key.key_part[field_no];
  }

  const char* Table_access::get_table_name() const
  {
    return get_join_tab().table->alias;
  }
  
  handler& Table_access::get_handler() const
  {
    return *get_join_tab().table->file;
  }
  
  st_table& Table_access::get_table() const
  {
    return *get_join_tab().table;
  }

  const uchar* Table_access::get_key_buffer() const
  {
    DBUG_ASSERT(m_access_type == AT_PrimaryKeyLookup ||
                m_access_type == AT_UniqueIndexLookup);
    return get_join_tab().ref.key_buff;
  }

  inline const JOIN_TAB& Table_access::get_join_tab() const 
  { 
    return m_root_tab[m_tab_no]; 
  }

  void Table_access::dbug_print() const
  {
    const JOIN* const join = get_join_tab().join; 
    DBUG_PRINT("info", ("type:%d", get_join_tab().type));
    DBUG_PRINT("info", ("ref.key:%d", get_join_tab().ref.key));
    DBUG_PRINT("info", ("ref.key_parts:%d", get_join_tab().ref.key_parts));
    DBUG_PRINT("info", ("ref.key_length:%d", get_join_tab().ref.key_length));
    
    DBUG_PRINT("info", ("order:%p", join->order));
    DBUG_PRINT("info", ("skip_sort_order:%d", join->skip_sort_order));
    DBUG_PRINT("info", ("no_order:%d", join->no_order));
    DBUG_PRINT("info", ("simple_order:%d", join->simple_order));
    
    DBUG_PRINT("info", ("group:%d", join->group));
    DBUG_PRINT("info", ("group_list:%p", join->group_list));
    DBUG_PRINT("info", ("simple_group:%d", join->simple_group));
    DBUG_PRINT("info", ("group_optimized_away:%d", join->group_optimized_away));
    
    DBUG_PRINT("info", ("full_join:%d", join->full_join));
    DBUG_PRINT("info", ("need_tmp:%d", join->need_tmp));
    DBUG_PRINT("info", ("select_distinct:%d", join->select_distinct));
    
    DBUG_PRINT("info", ("use_quick:%d", get_join_tab().use_quick));
    DBUG_PRINT("info", ("index:%d", get_join_tab().index));
    DBUG_PRINT("info", ("quick:%p", get_join_tab().quick));
    DBUG_PRINT("info", ("select:%p", get_join_tab().select));
    if (get_join_tab().select)
    {
      DBUG_PRINT("info", ("select->quick:%p", 
                          get_join_tab().select->quick));
    }
  } // Table_access::dbug_print()

  Table_access::Table_access(const JOIN_TAB* root_tab, int tab_no)
    :m_root_tab(root_tab), 
     m_tab_no(tab_no)
  {
    DBUG_ENTER("Table_access::Table_access");
    switch(get_join_tab().type)
    {
    case JT_EQ_REF:
    case JT_CONST:
      m_index_no = get_join_tab().ref.key;

      if (m_index_no == (int)get_join_tab().table->s->primary_key)
      {
        DBUG_PRINT("info", ("Operation %d is a primary key lookup.", m_tab_no));
        m_access_type = AT_PrimaryKeyLookup;
      }
      else
      {
        DBUG_PRINT("info", ("Operation %d is a unique index lookup.", 
                            m_tab_no));
        m_access_type = AT_UniqueIndexLookup;
      }
      break;

    case JT_REF:
      DBUG_ASSERT(get_join_tab().ref.key >= 0);
      DBUG_ASSERT(get_join_tab().ref.key < MAX_KEY);
      m_index_no = get_join_tab().ref.key;
      m_access_type = AT_OrderedIndexScan;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_NEXT:
      DBUG_ASSERT(get_join_tab().index < MAX_KEY);
      m_index_no = get_join_tab().index;
      m_access_type = AT_OrderedIndexScan;
      DBUG_PRINT("info", ("Operation %d is an ordered index scan.", m_tab_no));
      break;

    case JT_ALL:
      if (get_join_tab().use_quick == 2)
      {
        DBUG_PRINT("info", 
                   ("Operation %d has 'use_quick == 2' -> not pushable",
                    m_tab_no));
        m_access_type = AT_Other;
        m_index_no = -1;
      }
      else
      {
        if (get_join_tab().select != NULL && 
            get_join_tab().select->quick != NULL)
        {
          QUICK_SELECT_I *quick = get_join_tab().select->quick;
          DBUG_EXECUTE("info", quick->dbug_dump(0, true););
          if (quick->index < MAX_KEY)
          {
            m_index_no = quick->index;
            m_access_type = AT_OrderedIndexScan;
          }
          else
          {
            // No scanable indexes; use PK, typically 'pk in (X,Y,Z)'
            
            DBUG_PRINT("info", ("Operation %d is PK-MRR", m_tab_no));
            DBUG_ASSERT(quick->index == get_join_tab().table->s->primary_key ||
                        // MRR w/ set op PK's,: 'pk in (X,Y,Z)'
                        quick->index == MAX_KEY);                           
                        // 'Index merge         
            m_index_no = get_join_tab().table->s->primary_key;
            m_access_type = AT_PrimaryKeyLookup;
          }
        }
        else
        {
          DBUG_PRINT("info", ("Operation %d is a table scan.", m_tab_no));
          m_access_type = AT_TableScan;
        }
      }
      break;

    default:
      DBUG_PRINT("info", 
                 ("Operation %d has join_type %d. -> Not pushable.",
                  m_tab_no, get_join_tab().type));
      m_access_type = AT_Other;
      m_index_no = -1;
      break;
    }
    DBUG_VOID_RETURN;
  }

  /**************
   * Equal_set_iterator methods.
   *************/
  
  Equal_set_iterator::~Equal_set_iterator()
  { 
    delete m_iterator; 
  }

  Equal_set_iterator::Equal_set_iterator(const Query_plan& plan,
                                         const Item_field& field_item)
    :m_next(NULL),
     m_iterator(NULL)
  {
    DBUG_ENTER("Equal_set_iterator::Equal_set_iterator");
    DBUG_ASSERT(field_item.type() == Item::FIELD_ITEM);

    COND_EQUAL* const cond_equal = 
      plan.get_table_access(0).get_join_tab().join->cond_equal;
    
    if (cond_equal!=NULL)
    {
      Item_equal* item_equal = NULL;
      if (field_item.item_equal == NULL)
      {
        item_equal = 
          const_cast<Item_field&>(field_item).find_item_equal(cond_equal);
      }
      else
      {
        item_equal = field_item.item_equal;
      }

      if (item_equal != NULL)
      {
        m_iterator = new Item_equal_iterator(*item_equal);
        m_next = (*m_iterator)++;
      }
    }
    DBUG_VOID_RETURN;
  }

  const Item_field&
  Equal_set_iterator::next()
  {
    DBUG_ASSERT(m_next != NULL);
    const Item_field& result = *m_next;
    m_next = (*m_iterator)++;
    return result;;
  }
}; // namespace AQP
