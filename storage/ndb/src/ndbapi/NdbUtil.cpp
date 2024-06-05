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
Name:		NdbUtil.C
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

#include "NdbUtil.hpp"

NdbLabel::NdbLabel(Ndb *) {}

NdbLabel::~NdbLabel() {}

NdbSubroutine::NdbSubroutine(Ndb *) {}

NdbSubroutine::~NdbSubroutine() {}

NdbBranch::NdbBranch(Ndb *) : theSignal(nullptr) {}

NdbBranch::~NdbBranch() {}

NdbCall::NdbCall(Ndb *) : theSignal(nullptr) {}

NdbCall::~NdbCall() {}

NdbLockHandle::NdbLockHandle(Ndb *) {}

NdbLockHandle::~NdbLockHandle() {}

void NdbLockHandle::init() {
  m_state = ALLOCATED;
  m_table = nullptr;
  m_lockRef[0] = 0;
  m_openBlobCount = 0;
  thePrev = nullptr;
}

void NdbLockHandle::release(Ndb *) { m_state = FREE; }
