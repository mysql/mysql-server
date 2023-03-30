/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <string.h>
#include <assert.h>

// #include "node.h"
#include "uv.h"


/* Simple LIFO shareable list.
   Uses uv_mutex_t for synchronization.
   signalinfo can be used for in-band metadata. 
   The note can serve to document a list item, and also as cache-line padding.
*/
#define VPSZ sizeof(void *)
#define ISZ  sizeof(int)
#define LIST_ITEM_NOTE_SIZE 64 - (ISZ + VPSZ + VPSZ)

template<typename T> class ListNode {
public:
  ListNode<T> * next;
  T * item;
  int signalinfo;
private:
  char note[LIST_ITEM_NOTE_SIZE];

public:
  /* Constructor */
  ListNode<T>(T *t) : next(0), item(t), signalinfo(0)
  {
    note[0] = '\0';
  }
  
  /* Methods */
  void setNote(const char *txt) {
    strncpy(note, txt, LIST_ITEM_NOTE_SIZE);
    /* If txt is too long, strncpy() leaves it unterminated */
    note[LIST_ITEM_NOTE_SIZE] = '\0';
  }

  const char * getNote() const {
    return note;
  }
};


template<typename T> class SharedList {
private:
  uv_mutex_t lock;
  ListNode<T> * head;
  
public:
  SharedList<T>() : head(0)
  {
    int i = uv_mutex_init(& lock);
    assert(i == 0);
  }
  
  
  ~SharedList<T>()
  {
    uv_mutex_destroy(& lock);
  }
  
  
  void produce(ListNode<T> * node) {
    /* Find the tail */
    ListNode<T> * tail;
    for(tail = node; tail->next; tail = tail->next) {};
    
    uv_mutex_lock(& lock);
    tail->next = head;
    head = node;
    uv_mutex_unlock(& lock);
  }
  
  
  ListNode<T> * consumeAll() {
    uv_mutex_lock(& lock);
    ListNode<T> * result = head;
    head = 0;
    uv_mutex_unlock(& lock);
    return result;
  }
};



