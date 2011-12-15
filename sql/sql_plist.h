#ifndef SQL_PLIST_H
#define SQL_PLIST_H
/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include <my_global.h>

template <typename T, typename B, typename C, typename I>
class I_P_List_iterator;
class I_P_List_null_counter;
template <typename T> class I_P_List_no_push_back;


/**
   Intrusive parameterized list.

   Unlike I_List does not require its elements to be descendant of ilink
   class and therefore allows them to participate in several such lists
   simultaneously.

   Unlike List is doubly-linked list and thus supports efficient deletion
   of element without iterator.

   @param T  Type of elements which will belong to list.
   @param B  Class which via its methods specifies which members
             of T should be used for participating in this list.
             Here is typical layout of such class:

             struct B
             {
               static inline T **next_ptr(T *el)
               {
                 return &el->next;
               }
               static inline T ***prev_ptr(T *el)
               {
                 return &el->prev;
               }
             };
   @param C  Policy class specifying how counting of elements in the list
             should be done. Instance of this class is also used as a place
             where information about number of list elements is stored.
             @sa I_P_List_null_counter, I_P_List_counter
   @param I  Policy class specifying whether I_P_List should support
             efficient push_back() operation. Instance of this class
             is used as place where we store information to support
             this operation.
             @sa I_P_List_no_push_back, I_P_List_fast_push_back.
*/

template <typename T, typename B,
          typename C = I_P_List_null_counter,
          typename I = I_P_List_no_push_back<T> >
class I_P_List : public C, public I
{
  T *m_first;

  /*
    Do not prohibit copying of I_P_List object to simplify their usage in
    backup/restore scenarios. Note that performing any operations on such
    is a bad idea.
  */
public:
  I_P_List() : I(&m_first), m_first(NULL) {};
  inline void empty()      { m_first= NULL; C::reset(); I::set_last(&m_first); }
  inline bool is_empty() const { return (m_first == NULL); }
  inline void push_front(T* a)
  {
    *B::next_ptr(a)= m_first;
    if (m_first)
      *B::prev_ptr(m_first)= B::next_ptr(a);
    else
      I::set_last(B::next_ptr(a));
    m_first= a;
    *B::prev_ptr(a)= &m_first;
    C::inc();
  }
  inline void push_back(T *a)
  {
    T **last= I::get_last();
    *B::next_ptr(a)= *last;
    *last= a;
    *B::prev_ptr(a)= last;
    I::set_last(B::next_ptr(a));
    C::inc();
  }
  inline void insert_after(T *pos, T *a)
  {
    if (pos == NULL)
      push_front(a);
    else
    {
      *B::next_ptr(a)= *B::next_ptr(pos);
      *B::prev_ptr(a)= B::next_ptr(pos);
      *B::next_ptr(pos)= a;
      if (*B::next_ptr(a))
      {
        T *old_next= *B::next_ptr(a);
        *B::prev_ptr(old_next)= B::next_ptr(a);
      }
      else
        I::set_last(B::next_ptr(a));
      C::inc();
    }
  }
  inline void remove(T *a)
  {
    T *next= *B::next_ptr(a);
    if (next)
      *B::prev_ptr(next)= *B::prev_ptr(a);
    else
      I::set_last(*B::prev_ptr(a));
    **B::prev_ptr(a)= next;
    C::dec();
  }
  inline T* front() { return m_first; }
  inline const T *front() const { return m_first; }
  inline T* pop_front()
  {
    T *result= front();

    if (result)
      remove(result);

    return result;
  }
  void swap(I_P_List<T, B, C> &rhs)
  {
    swap_variables(T *, m_first, rhs.m_first);
    I::swap(rhs);
    if (m_first)
      *B::prev_ptr(m_first)= &m_first;
    else
      I::set_last(&m_first);
    if (rhs.m_first)
      *B::prev_ptr(rhs.m_first)= &rhs.m_first;
    else
      I::set_last(&rhs.m_first);
    C::swap(rhs);
  }
#ifndef _lint
  friend class I_P_List_iterator<T, B, C, I>;
#endif
  typedef I_P_List_iterator<T, B, C, I> Iterator;
};


/**
   Iterator for I_P_List.
*/

template <typename T, typename B,
          typename C = I_P_List_null_counter,
          typename I = I_P_List_no_push_back<T> >
class I_P_List_iterator
{
  const I_P_List<T, B, C, I> *list;
  T *current;
public:
  I_P_List_iterator(const I_P_List<T, B, C, I> &a)
    : list(&a), current(a.m_first) {}
  I_P_List_iterator(const I_P_List<T, B, C, I> &a, T* current_arg)
    : list(&a), current(current_arg) {}
  inline void init(const I_P_List<T, B, C, I> &a)
  {
    list= &a;
    current= a.m_first;
  }
  inline T* operator++(int)
  {
    T *result= current;
    if (result)
      current= *B::next_ptr(current);
    return result;
  }
  inline T* operator++()
  {
    current= *B::next_ptr(current);
    return current;
  }
  inline void rewind()
  {
    current= list->m_first;
  }
};


/**
  Hook class which via its methods specifies which members
  of T should be used for participating in a intrusive list.
*/

template <typename T, T* T::*next, T** T::*prev>
struct I_P_List_adapter
{
  static inline T **next_ptr(T *el) { return &(el->*next); }

  static inline T ***prev_ptr(T *el) { return &(el->*prev); }
};


/**
  Element counting policy class for I_P_List to be used in
  cases when no element counting should be done.
*/

class I_P_List_null_counter
{
protected:
  void reset() {}
  void inc() {}
  void dec() {}
  void swap(I_P_List_null_counter &rhs) {}
};


/**
  Element counting policy class for I_P_List which provides
  basic element counting.
*/

class I_P_List_counter
{
  uint m_counter;
protected:
  I_P_List_counter() : m_counter (0) {}
  void reset() {m_counter= 0;}
  void inc() {m_counter++;}
  void dec() {m_counter--;}
  void swap(I_P_List_counter &rhs)
  { swap_variables(uint, m_counter, rhs.m_counter); }
public:
  uint elements() const { return m_counter; }
};


/**
  A null insertion policy class for I_P_List to be used
  in cases when push_back() operation is not necessary.
*/

template <typename T> class I_P_List_no_push_back
{
protected:
  I_P_List_no_push_back(T **a) {};
  void set_last(T **a) {}
  /*
    T** get_last() const method is intentionally left unimplemented
    in order to prohibit usage of push_back() method in lists which
    use this policy.
  */
  void swap(I_P_List_no_push_back<T> &rhs) {}
};


/**
  An insertion policy class for I_P_List which can
  be used when fast push_back() operation is required.
*/

template <typename T> class I_P_List_fast_push_back
{
  T **m_last;
protected:
  I_P_List_fast_push_back(T **a) : m_last(a) { };
  void set_last(T **a) { m_last= a; }
  T** get_last() const { return m_last; }
  void swap(I_P_List_fast_push_back<T> &rhs)
  { swap_variables(T**, m_last, rhs.m_last); }
};

#endif
