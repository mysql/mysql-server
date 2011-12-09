/*
   Copyright (C) 2003, 2005, 2006 MySQL AB, 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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

NdbLabel::NdbLabel(Ndb*)
{
}

NdbLabel::~NdbLabel()
{
}

NdbSubroutine::NdbSubroutine(Ndb*)
{
}

NdbSubroutine::~NdbSubroutine()
{
}

NdbBranch::NdbBranch(Ndb*) :
  theSignal(NULL)
{
}

NdbBranch::~NdbBranch()
{
}

NdbCall::NdbCall(Ndb*) :
  theSignal(NULL)
{
}

NdbCall::~NdbCall()
{
}


NdbLockHandle::NdbLockHandle(Ndb*)
{
}

NdbLockHandle::~NdbLockHandle()
{
}

void
NdbLockHandle::init()
{
  m_state = ALLOCATED;
  m_table = NULL;
  m_lockRef[0] = 0;
  m_openBlobCount = 0;
  thePrev = NULL;
}

void
NdbLockHandle::release(Ndb* ndb)
{
  m_state = FREE;
}
