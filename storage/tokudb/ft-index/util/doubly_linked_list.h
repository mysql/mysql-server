/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

//******************************************************************************
//
// Overview: A doubly linked list with elements of type T.
//   Each element that wants to be put into the list provides a
//   LinkedListElement<T> as well as a pointer to the the object of type T.
//   Typically, the user embeds the linked list element into the object itself,
//   for example as
//     struct foo {
//       toku::LinkedListElement<struct foo *> linked_list_elt;
//       ... other elements of foo
//     };
//   then when inserting foo into a list defined as 
//      toku::DoublyLinkedList<struct foo *> list_of_foos;
//   you write
//      struct foo f;
//      list_of_foos->insert(&f->linked_list_elt, &f);
//
// Operations:  Constructor and deconstructors are provided (they don't 
//   need to anything but fill in a field) for the DoublyLinkedList.
//   Operations to insert an element and remove it, as well as to pop
//   an element out of the list.
//   Also a LinkedListElement class is provided with a method to get a
//   pointer to the object of type T.
//******************************************************************************

#include <stdbool.h>
#include <portability/toku_assert.h>

namespace toku {

template<typename T> class DoublyLinkedList;

template<typename T> class LinkedListElement {
    friend class DoublyLinkedList<T>;
 private:
    T container;
    LinkedListElement<T> *prev, *next;
 public:
    T get_container(void) {
	return container;
    }
};

template<typename T> class DoublyLinkedList {
 public:
    void init (void);
    // Effect: Initialize a doubly linked list (to be empty).

    void insert(LinkedListElement<T> *ll_elt, T container); 
    // Effect: Add an item to a linked list.
    // Implementation note: Push the item to the head of the list.

    void remove(LinkedListElement<T> *ll_elt);
    // Effect: Remove an item from a linked list.
    // Requires: The item is in the list identified by head.

    bool pop(LinkedListElement<T> **ll_eltp);
    // Effect: if the list is empty, return false.
    //   Otherwise return true and set *ll_eltp to the first item, and remove that item from the list.

    template<typename extra_t> int iterate(int (*fun)(T container, extra_t extra), extra_t extra);
    // Effect: Call fun(e, extra) on every element of the linked list.  If ever fun returns nonzero, then quit early and return that value.
    //  If fun always return zero, then this function returns zero.

 private:
    LinkedListElement<T> *m_first;
};

//******************************************************************************
// DoublyLinkedList implementation starts here.
//******************************************************************************

#include <stddef.h>



template<typename T> void DoublyLinkedList<T>::init(void) {
    m_first     = NULL;
}

template<typename T> void DoublyLinkedList<T>::insert(LinkedListElement<T> *ll_elt, T container) {
    LinkedListElement<T> *old_first = m_first;
    ll_elt->container = container;
    ll_elt->next      = old_first;
    ll_elt->prev      = NULL;
    if (old_first!=NULL) {
	old_first->prev = ll_elt;
    }
    m_first = ll_elt;
}

template<typename T> void DoublyLinkedList<T>::remove(LinkedListElement<T> *ll_elt) {
    LinkedListElement<T> *old_prev = ll_elt->prev;
    LinkedListElement<T> *old_next = ll_elt->next;

    if (old_prev==NULL) {
	m_first = old_next;
    } else {
	old_prev->next = old_next;
    }
    if (old_next==NULL) {
	/* nothing */
    } else {
	old_next->prev = old_prev;
    }
}

template<typename T> bool DoublyLinkedList<T>::pop(LinkedListElement<T> **ll_eltp) {
    LinkedListElement<T> *first = m_first;
    if (first) {
	invariant(first->prev==NULL);
	m_first = first->next;
	if (first->next) {
	    first->next->prev = NULL;
	}
	first->next=NULL;
	*ll_eltp = first;
	return true;
    } else {
	return false;
    }
}

template<typename T>
template<typename extra_t>
int DoublyLinkedList<T>::iterate(int (*fun)(T container, extra_t extra), extra_t extra) {
    for (LinkedListElement<T> *le = m_first; le; le=le->next) {
	int r = fun(le->container, extra);
	if (r!=0) return r;
    }
    return 0;
}

}
