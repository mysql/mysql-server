/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__COLLECTION_IMPL_INCLUDED
#define DD__COLLECTION_IMPL_INCLUDED

#include "my_global.h"

#include "dd/iterator.h"               // dd::Iterator
#include "dd/impl/collection_item.h"   // dd::Collection_item

#include <algorithm>
#include <vector>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;
class Raw_table;
class Transaction;

///////////////////////////////////////////////////////////////////////////

class Base_collection
{
public:
  typedef std::vector<Collection_item *> Array;

public:
  Base_collection()
  { }

  ~Base_collection();

private:
  void clear_all_items();

public:
  Collection_item *add(const Collection_item_factory &item_factory);

  Collection_item *add_first(const Collection_item_factory &item_factory);

  void remove(Collection_item *item); /* purecov: deadcode */

  bool is_empty() const
  { return m_items.empty() && m_removed_items.empty(); }

  size_t size() const
  { return m_items.size(); }

public:
  bool restore_items(const Collection_item_factory &item_factory,
                     Open_dictionary_tables_ctx *otx,
                     Raw_table *table,
                     Object_key *key);

  bool store_items(Open_dictionary_tables_ctx *otx);

  bool drop_items(Open_dictionary_tables_ctx *otx, Raw_table *table, Object_key *key);

  template<typename Comparator>
  void sort_items(Comparator c)
  {
    std::sort(m_items.begin(), m_items.end(), c);
    renumerate_items();
  }

protected:
  void renumerate_items()
  {
    for (unsigned int i= 0; i < m_items.size(); ++i)
      m_items[i]->set_ordinal_position(i + 1);
  }

protected:
  Array m_items;
  Array m_removed_items;

private:
  Base_collection(const Base_collection &);
  Base_collection &operator =(const Base_collection &);
};

///////////////////////////////////////////////////////////////////////////

template <typename T>
class Collection : private Base_collection
{
public:
  typedef T value_type;
  typedef Iterator<T> iterator_type;
  typedef Iterator<const T> const_iterator_type;

  static const bool SKIP_HIDDEN_ITEMS= false;
  static const bool INCLUDE_HIDDEN_ITEMS= true;

  // Simplify implementation of clone member functions
  Array &aref()
  {
    return m_items;
  }

private:
  class Collection_iterator : public Iterator<T>
  {
  public:
    Collection_iterator(Array *array, bool include_hidden_items)
     :m_current(array->begin()),
      m_end(array->end()),
      m_include_hidden_items(include_hidden_items)
    { }

  public:
    virtual T *next()
    {
      if (m_current == m_end)
        return NULL;

      Collection_item *item= *m_current;

      // Skipping hidden items
      if (!m_include_hidden_items)
      {
        while (item->is_hidden())
        {
          ++m_current;

          if (m_current == m_end)
            return NULL;

          item= *m_current;
        }
      }

      ++m_current;

      return dynamic_cast<T *> (item);
    }

  private:
    typename Array::iterator m_current;
    typename Array::iterator m_end;
    bool m_include_hidden_items;
  };

private:
  class Collection_const_iterator : public Iterator<const T>
  {
  public:
    Collection_const_iterator(const Array *array, bool include_hidden_items)
     :m_current(array->begin()),
      m_end(array->end()),
      m_include_hidden_items(include_hidden_items)
    { }

  public:
    const T *next()
    {
      if (m_current == m_end)
        return NULL;

      const Collection_item *item= *m_current;

      // Skipping hidden items
      if (!m_include_hidden_items)
      {
        while (item->is_hidden())
        {
          ++m_current;

          if (m_current == m_end)
            return NULL;

          item= *m_current;
        }
      }

      ++m_current;

      return dynamic_cast<const T *> (item);
    }

  private:
    typename Array::const_iterator m_current;
    typename Array::const_iterator m_end;
    bool m_include_hidden_items;
  };

public:
  Collection()
  { }

  T *add(const Collection_item_factory &item_factory)
  { return dynamic_cast<T *> (Base_collection::add(item_factory)); }

  T *add_first(const Collection_item_factory &item_factory)
  { return dynamic_cast<T *> (Base_collection::add_first(item_factory)); }

  /* purecov: begin deadcode */
  void remove(Collection_item *item)
  { Base_collection::remove(item); }
  /* purecov: end */

  Iterator<T> *iterator(bool include_hidden_items)
  { return new (std::nothrow) Collection_iterator(&m_items,
                                                  include_hidden_items); }

  Iterator<const T> *const_iterator(bool include_hidden_items) const
  { return new (std::nothrow) Collection_const_iterator(&m_items,
                                                        include_hidden_items); }

  Iterator<const T> *iterator(bool include_hidden_items) const
  { return const_iterator(include_hidden_items, INCLUDE_HIDDEN_ITEMS); }

  Iterator<T> *iterator()
  { return new (std::nothrow) Collection_iterator(&m_items,
                                                  INCLUDE_HIDDEN_ITEMS); }

  Iterator<const T> *const_iterator() const
  { return new (std::nothrow) Collection_const_iterator(&m_items,
                                                        INCLUDE_HIDDEN_ITEMS); }

  Iterator<const T> *iterator() const
  { return const_iterator(); }

  bool is_empty() const
  { return Base_collection::is_empty(); }

  size_t size() const
  { return Base_collection::size(); }

  T *back() const
  {
    if (is_empty())
      return NULL;
    return dynamic_cast<T *>(m_items.back());
  }

public:
  bool restore_items(const Collection_item_factory &item_factory,
                     Open_dictionary_tables_ctx *otx,
                     Raw_table *table,
                     Object_key *key)
  { return Base_collection::restore_items(item_factory, otx, table, key); }

  bool store_items(Open_dictionary_tables_ctx *otx)
  { return Base_collection::store_items(otx); }

  bool drop_items(Open_dictionary_tables_ctx *otx, Raw_table *table, Object_key *key)
  { return Base_collection::drop_items(otx, table, key); }

  // Interface to sort items
  template <typename Item_type>
  class Collection_item_comparator
  {
  private:
    Item_type &m_cmp;

  public:
    Collection_item_comparator(Item_type &cmp) : m_cmp(cmp) {}
    bool operator() (const Collection_item* p1, const Collection_item* p2)
    {
      return m_cmp(dynamic_cast<const T*>(p1),
                   dynamic_cast<const T*>(p2));
    }
  };

  /*
    Sorts items based on comparator supplied.

    One can think of designing collection such that the items
    are always kept sorted based on some comparator.
    The problem is that the item can be changed by DD user
    after adding it to collection, which might affect the
    sort order. And we need to handle addition/updatation
    and deletion cases individually. Hence the complexity in
    always maintaining items in sorted order is more. Moreover,
    we do not see more use-cases that demand such a framework now.
    Also the number of items in collection are not too high
    that we will hit performance issues. We may consider
    re-design if some use-case demand it in future.
  */

  template <typename Comparator>
  void sort_items(Comparator c)
  { Base_collection::sort_items(Collection_item_comparator<Comparator>(c)); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLLECTION_PARENT_INCLUDED
