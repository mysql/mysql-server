/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MemoryChannel_H
#define MemoryChannel_H

//===========================================================================
//
// .DESCRIPTION
//              Pointer based communication channel for communication between
//              two thread. It does not copy any data in or out the channel so
//              the item that is put in can not be used until the other thread
//              has given it back. There is no support for detecting the return
//              of a item. The channel is half-duplex. For communication between
//              1 writer and 1 reader use the MemoryChannel class, for
//              communication between multiple writer and 1 reader use the
//              MemoryChannelMultipleWriter. There is no support for multiple
//              readers.
//
// .TYPICAL USE:
//              to communicate between threads.
//
// .EXAMPLE:
//              See AsyncFile.C
//===========================================================================
//
//
// MemoryChannel( int size= 256);
//   Constructor
// Parameters:
//      size : amount of pointer it can hold
//
// void operator ++ ();
//   increments the index with one, if size is reached it is set to zero
//
// virtual void write( T *t);
//   Puts the item in the channel if the channel is full an error is reported.
// Parameters:
//      t: pointer to item to put in the channel, after this the item
//                        is shared with the other thread.
// errors
//                      AFS_ERROR_CHANNALFULL, channel is full
//
// T* read();
//      Reads an item from the channel, if channel is empty it blocks until
//              an item can be read.
// return
//                      T : item from the channel
//
// T* tryRead();
//      Reads an item from the channel, if channel is empty it returns zero.
// return
//                      T : item from the channel or zero if channel is empty.
//

#include <NdbOut.hpp>
#include "NdbCondition.h"
#include "NdbMutex.h"

#define JAM_FILE_ID 396

template <class T>
class MemoryChannel {
 public:
  MemoryChannel();
  virtual ~MemoryChannel();

  void writeChannel(T *t);
  void writeChannelNoSignal(T *t);
  T *readChannel();
  T *tryReadChannel();

  /**
   * Should be made class using MemoryChannel
   */
  struct ListMember {
    T *m_next;
  };

 private:
  Uint32 m_occupancy;
  T *m_head;  // First element in list (e.g will be read by readChannel)
  T *m_tail;
  NdbMutex *theMutexPtr;
  NdbCondition *theConditionPtr;

  template <class U>
  friend NdbOut &operator<<(NdbOut &out, const MemoryChannel<U> &chn);
};

template <class T>
NdbOut &operator<<(NdbOut &out, const MemoryChannel<T> &chn) {
  NdbMutex_Lock(chn.theMutexPtr);
  out << "[ occupancy: " << chn.m_occupancy << " ]";
  NdbMutex_Unlock(chn.theMutexPtr);
  return out;
}

template <class T>
MemoryChannel<T>::MemoryChannel() : m_occupancy(0), m_head(0), m_tail(0) {
  theMutexPtr = NdbMutex_Create();
  theConditionPtr = NdbCondition_Create();
}

template <class T>
MemoryChannel<T>::~MemoryChannel() {
  NdbMutex_Destroy(theMutexPtr);
  NdbCondition_Destroy(theConditionPtr);
}

template <class T>
void MemoryChannel<T>::writeChannel(T *t) {
  writeChannelNoSignal(t);
  NdbCondition_Signal(theConditionPtr);
}

template <class T>
void MemoryChannel<T>::writeChannelNoSignal(T *t) {
  NdbMutex_Lock(theMutexPtr);
  if (m_head == 0) {
    assert(m_occupancy == 0);
    m_head = m_tail = t;
  } else {
    assert(m_tail != 0);
    m_tail->m_mem_channel.m_next = t;
    m_tail = t;
  }
  t->m_mem_channel.m_next = 0;
  m_occupancy++;
  NdbMutex_Unlock(theMutexPtr);
}

template <class T>
T *MemoryChannel<T>::readChannel() {
  NdbMutex_Lock(theMutexPtr);
  while (m_head == 0) {
    assert(m_occupancy == 0);
    NdbCondition_Wait(theConditionPtr, theMutexPtr);
  }
  assert(m_occupancy > 0);
  T *tmp = m_head;
  if (m_head == m_tail) {
    assert(m_occupancy == 1);
    m_head = m_tail = 0;
  } else {
    m_head = m_head->m_mem_channel.m_next;
  }
  m_occupancy--;
  NdbMutex_Unlock(theMutexPtr);
  return tmp;
}

template <class T>
T *MemoryChannel<T>::tryReadChannel() {
  NdbMutex_Lock(theMutexPtr);
  T *tmp = m_head;
  if (m_head != 0) {
    assert(m_occupancy > 0);
    if (m_head == m_tail) {
      assert(m_occupancy == 1);
      m_head = m_tail = 0;
    } else {
      m_head = m_head->m_mem_channel.m_next;
    }
    m_occupancy--;
  } else {
    assert(m_occupancy == 0);
  }
  NdbMutex_Unlock(theMutexPtr);
  return tmp;
}

#undef JAM_FILE_ID

#endif  // MemoryChannel_H
