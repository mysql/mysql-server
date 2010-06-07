#ifndef SQL_PLIST_H
#define SQL_PLIST_H
/* Copyright (C) 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


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
  T *first;

  /*
    Do not prohibit copying of I_P_List object to simplify their usage in
    backup/restore scenarios. Note that performing any operations on such
    is a bad idea.
  */
public:
  I_P_List() : I(&first), first(NULL) {};
  inline void empty()      { first= NULL; C::reset(); I::set_last(&first); }
  inline bool is_empty() const { return (first == NULL); }
  inline void push_front(T* a)
  {
    *B::next_ptr(a)= first;
    if (first)
      *B::prev_ptr(first)= B::next_ptr(a);
    else
      I::set_last(B::next_ptr(a));
    first= a;
    *B::prev_ptr(a)= &first;
    C::inc();
  }
  inline void push_back(T *a)
  {
    T **last= I::get_last();
    *B::next_ptr(a)= *last;
    *last= a;
    *B::prev_ptr(a)= last;
    I::set_last(B::next_ptr(a));
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
  inline T* front() { return first; }
  inline const T *front() const { return first; }
  void swap(I_P_List<T, B, C> &rhs)
  {
    swap_variables(T *, first, rhs.first);
    I::swap(rhs);
    if (first)
      *B::prev_ptr(first)= &first;
    else
      I::set_last(&first);
    if (rhs.first)
      *B::prev_ptr(rhs.first)= &rhs.first;
    else
      I::set_last(&rhs.first);
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
    : list(&a), current(a.first) {}
  I_P_List_iterator(const I_P_List<T, B, C, I> &a, T* current_arg)
    : list(&a), current(current_arg) {}
  inline void init(const I_P_List<T, B, C, I> &a)
  {
    list= &a;
    current= a.first;
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
    current= list->first;
  }
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
  T **last;
protected:
  I_P_List_fast_push_back(T **a) : last(a) { };
  void set_last(T **a) { last= a; }
  T** get_last() const { return last; }
  void swap(I_P_List_fast_push_back<T> &rhs)
  { swap_variables(T**, last, rhs.last); }
};

#endif
