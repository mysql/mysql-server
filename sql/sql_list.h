/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* mysql standard open memoryallocator */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif


class Sql_alloc
{
public:
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr, size_t size) {} /*lint -e715 */
#ifdef HAVE_purify
  bool dummy;
  inline Sql_alloc() :dummy(0) {}
  inline ~Sql_alloc() {}
#else
  inline Sql_alloc() {}
  inline ~Sql_alloc() {}
#endif

};

/*
** basic single linked list
** Used for item and item_buffs.
*/

class base_list :public Sql_alloc {
protected:
  class list_node :public Sql_alloc
  {
  public:
    list_node *next;
    void *info;
    list_node(void *info_par,list_node *next_par) : next(next_par),info(info_par) {}
    friend class base_list;
    friend class base_list_iterator;
  };
  list_node *first,**last;

public:
  uint elements;

  inline void empty() { elements=0; first=0; last=&first;}
  inline base_list() { empty(); }
  inline base_list(const base_list &tmp) :Sql_alloc()
  {
    elements=tmp.elements;
    first=tmp.first;
    last=tmp.last;
  }
  inline bool push_back(void *info)
  {
    if (((*last)=new list_node(info,0)))
    {
      last= &(*last)->next;
      elements++;
      return 0;
    }
    return 1;
  }
  inline bool push_front(void *info)
  {
    list_node *node=new list_node(info,first);
    if (node)
    {
      if (!first)
	last= &node->next;
      first=node;
      elements++;
      return 0;
    }
    return 1;
  }
  void remove(list_node **prev)
  {
    list_node *node=(*prev)->next;
    delete *prev;
    *prev=node;
    if (!--elements)
    {
      last= &first;
      first=0;
    }
  }
  inline void *pop(void)
  {
    if (!first) return 0;
    list_node *tmp=first;
    first=first->next;
    if (!--elements)
      last= &first;
    return tmp->info;
  }
  inline void *head() { return first ? first->info : 0; }
  inline void **head_ref() { return first ? &first->info : 0; }
  friend class base_list_iterator;

protected:
  void after(void *info,list_node *node)
  {
    list_node *new_node=new list_node(info,node->next);
    node->next=new_node;
    elements++;
    if (last == &(node->next))
      last= &new_node->next;
  }
};


class base_list_iterator
{
  base_list *list;
  base_list::list_node **el,**prev,*current;
public:
  base_list_iterator(base_list &list_par) :list(&list_par),el(&list_par.first),
    prev(0),current(0)
  {}
  inline void *next(void)
  {
    prev=el;
    if (!(current= *el))
      return 0;
    el= &current->next;
    return current->info;
  }
  inline void rewind(void)
  {
    el= &list->first;
  }
  void *replace(void *element)
  {						// Return old element
    void *tmp=current->info;
    current->info=element;
    return tmp;
  }
  void *replace(base_list &new_list)
  {
    void *ret_value=current->info;
    if (new_list.first)
    {
      *new_list.last=current->next;
      current->info=new_list.first->info;
      current->next=new_list.first->next;
      list->elements+=new_list.elements-1;
    }
    return ret_value;				// return old element
  }
  inline void remove(void)			// Remove current
  {
    list->remove(prev);
    el=prev;
    current=0;					// Safeguard
  }
  void after(void *element)			// Insert element after current
  {
    list->after(element,current);
    current=current->next;
    el= &current->next;
  }
  inline void **ref(void)			// Get reference pointer
  {
    return &current->info;
  }
  inline bool is_last(void)
  {
    return *el == 0;
  }
};


template <class T> class List :public base_list
{
public:
  inline List() :base_list() {}
  inline List(const List<T> &tmp) :base_list(tmp) {}
  inline bool push_back(T *a) { return base_list::push_back(a); }
  inline bool push_front(T *a) { return base_list::push_front(a); }
  inline T* head() {return (T*) base_list::head(); }
  inline T** head_ref() {return (T**) base_list::head_ref(); }
  inline T* pop()  {return (T*) base_list::pop(); }
  void delete_elements(void)
  {
    list_node *element,*next;
    for (element=first; element ; element=next)
    {
      next=element->next;
      delete (T*) element->info;
    }
    empty();
  }
};


template <class T> class List_iterator :public base_list_iterator
{
public:
  List_iterator(List<T> &a) : base_list_iterator(a) {}
  inline T* operator++(int) { return (T*) base_list_iterator::next(); }
  inline void rewind(void)  { base_list_iterator::rewind(); }
  inline T *replace(T *a)   { return (T*) base_list_iterator::replace(a); }
  inline T *replace(List<T> &a) { return (T*) base_list_iterator::replace(a); }
  inline void remove(void)  { base_list_iterator::remove(); }
  inline void after(T *a)   { base_list_iterator::after(a); }
  inline T** ref(void)	    { return (T**) base_list_iterator::ref(); }
  inline bool is_last(void) { return base_list_iterator::is_last(); }
};


/*
** An simple intrusive list with automaticly removes element from list
** on delete (for THD element)
*/

struct ilink {
  struct ilink **prev,*next;
  static void *operator new(size_t size)
  {
    return (void*)my_malloc((uint)size, MYF(MY_WME | MY_FAE));
  }
  static void operator delete(void* ptr_arg, size_t size)
  {
     my_free((gptr)ptr_arg, MYF(MY_WME|MY_ALLOW_ZERO_PTR));
  }

  inline ilink()
  {
    prev=0; next=0;
  }
  inline void unlink()
  {
    /* Extra tests because element doesn't have to be linked */
    if (prev) *prev= next;
    if (next) next->prev=prev;
    prev=0 ; next=0;
  }
  virtual ~ilink() { unlink(); }		/*lint -e1740 */
};

template <class T> class I_List_iterator;

class base_ilist {
  public:
  struct ilink *first,last;
  base_ilist() { first= &last; last.prev= &first; }
  inline bool is_empty() {  return first == &last; }
  inline void append(ilink *a)
  {
    first->prev= &a->next;
    a->next=first; a->prev= &first; first=a;
  }
  inline void push_back(ilink *a)
  {
    *last.prev= a;
    a->next= &last;
    a->prev= last.prev;
    last.prev= &a->next;
  }
  inline struct ilink *get()
  {
    struct ilink *first_link=first;
    if (first_link == &last)
      return 0;
    first_link->unlink();			// Unlink from list
    return first_link;
  }
  friend class base_list_iterator;
};


class base_ilist_iterator
{
  base_ilist *list;
  struct ilink **el,*current;
public:
  base_ilist_iterator(base_ilist &list_par) :list(&list_par),
    el(&list_par.first),current(0) {}
  void *next(void)
  {
    /* This is coded to allow push_back() while iterating */
    current= *el;
    if (current == &list->last) return 0;
    el= &current->next;
    return current;
  }
};


template <class T>
class I_List :private base_ilist {
public:
  I_List() :base_ilist()	{}
  inline bool is_empty()        { return base_ilist::is_empty(); } 
  inline void append(T* a)	{ base_ilist::append(a); }
  inline void push_back(T* a)	{ base_ilist::push_back(a); }
  inline T* get()		{ return (T*) base_ilist::get(); }
#ifndef _lint
  friend class I_List_iterator<T>;
#endif
};


template <class T> class I_List_iterator :public base_ilist_iterator
{
public:
  I_List_iterator(I_List<T> &a) : base_ilist_iterator(a) {}
  inline T* operator++(int) { return (T*) base_ilist_iterator::next(); }
};
