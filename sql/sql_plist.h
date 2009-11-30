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

template <typename T, typename B> class I_P_List_iterator;


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
*/

template <typename T, typename B>
class I_P_List
{
  T *first;

  /*
    Do not prohibit copying of I_P_List object to simplify their usage in
    backup/restore scenarios. Note that performing any operations on such
    is a bad idea.
  */
public:
  I_P_List() : first(NULL) { };
  inline void empty()      { first= NULL; }
  inline bool is_empty() const { return (first == NULL); }
  inline void push_front(T* a)
  {
    *B::next_ptr(a)= first;
    if (first)
      *B::prev_ptr(first)= B::next_ptr(a);
    first= a;
    *B::prev_ptr(a)= &first;
  }
  inline void remove(T *a)
  {
    T *next= *B::next_ptr(a);
    if (next)
      *B::prev_ptr(next)= *B::prev_ptr(a);
    **B::prev_ptr(a)= next;
  }
  inline T* head() { return first; }
  void swap(I_P_List<T,B> &rhs)
  {
    swap_variables(T *, first, rhs.first);
    if (first)
      *B::prev_ptr(first)= &first;
    if (rhs.first)
      *B::prev_ptr(rhs.first)= &rhs.first;
  }
#ifndef _lint
  friend class I_P_List_iterator<T, B>;
#endif
};


/**
   Iterator for I_P_List.
*/

template <typename T, typename B>
class I_P_List_iterator
{
  I_P_List<T, B> *list;
  T *current;
public:
  I_P_List_iterator(I_P_List<T, B> &a) : list(&a), current(a.first) {}
  inline void init(I_P_List<T, B> &a)
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
  inline void rewind()
  {
    current= list->first;
  }
};

#endif
