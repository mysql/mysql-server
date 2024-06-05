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

/************************************************************************************************
Name:		NdbUtil.H
Include:
Link:
Author:		UABRONM Mikael Ronström UAB/B/SD
Date:		991029
Version:	0.4
Description:	Utility classes for NDB API
Documentation:
Adjust:		991029  UABRONM   First version.
Comment:
************************************************************************************************/
#ifndef NdbUtil_H
#define NdbUtil_H

#include <ndb_global.h>

class Ndb;
class NdbApiSignal;
class NdbOperation;
class NdbTableImpl;

template <class T>
struct Free_list_element {
  Free_list_element() { theNext = nullptr; }
  void next(T *obj) { theNext = obj; }
  T *next() { return theNext; }

  T *theNext;
};

class NdbLabel : public Free_list_element<NdbLabel> {
  friend class NdbOperation;
  friend class Ndb;

 public:
  NdbLabel(Ndb *);
  ~NdbLabel();

 private:
  Uint32 theSubroutine[16];
  Uint32 theLabelAddress[16];
  Uint32 theLabelNo[16];
};

class NdbSubroutine : public Free_list_element<NdbSubroutine> {
  friend class NdbOperation;
  friend class Ndb;

 public:
  NdbSubroutine(Ndb *);
  ~NdbSubroutine();

  Uint32 theSubroutineAddress[16];
};

class NdbBranch : public Free_list_element<NdbBranch> {
  friend class NdbOperation;
  friend class Ndb;

 public:
  NdbBranch(Ndb *);
  ~NdbBranch();

  NdbApiSignal *theSignal;
  Uint32 theSignalAddress;
  Uint32 theBranchAddress;
  Uint32 theBranchLabel;
  Uint32 theSubroutine;
};

class NdbCall : public Free_list_element<NdbCall> {
  friend class NdbOperation;
  friend class Ndb;

 public:
  NdbCall(Ndb *);
  ~NdbCall();

  NdbApiSignal *theSignal;
  Uint32 theSignalAddress;
  Uint32 theSubroutine;
};

class NdbLockHandle : public Free_list_element<NdbLockHandle> {
 public:
  enum State {
    FREE,      /* In freelist */
    ALLOCATED, /* Allocated, but not prepared */
    PREPARED   /* Prepared, and possibly executed
                * if isLockRefValid() returns true
                */
  };

  State m_state;
  const NdbTableImpl *m_table;

  /* Components of lock reference */
  Uint32 m_lockRef[3];

  Uint32 m_openBlobCount;

  /* Used for per-transaction list of lockhandles */
  NdbLockHandle *thePrev;

  NdbLockHandle(Ndb *);
  ~NdbLockHandle();

  void init();
  void release(Ndb *ndb);
  inline bool isLockRefValid() const {
    /* LockRef[ 0 ] contains the NodeId and FragId
     * A valid lockref would have a non-zero nodeid.
     */
    return (m_lockRef[0] != 0);
  }
  inline Uint32 getDistKey() const {
    /* First word of LockRef is distkey to send */
    return m_lockRef[0];
  }
  inline const Uint32 *getKeyInfoWords(Uint32 &sz) const {
    /* Second and third words of LockRef are KeyInfo to send */
    sz = 2;
    return &m_lockRef[1];
  }
};
#endif
