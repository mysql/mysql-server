/* Copyright (C) 2003 MySQL AB

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

class NdbApiSignal;
class NdbOperation;

class NdbLabel
{
friend class NdbOperation;
friend class Ndb;

private:
  NdbLabel();
  ~NdbLabel();

  NdbLabel* theNext;
  Uint32   theSubroutine[16];
  Uint32   theLabelAddress[16];
  Uint32   theLabelNo[16];
};

class NdbSubroutine
{
friend class NdbOperation;
friend class Ndb;

private:
  NdbSubroutine();
  ~NdbSubroutine();

  NdbSubroutine* theNext;
  Uint32   theSubroutineAddress[16];
};

class NdbBranch
{
friend class NdbOperation;
friend class Ndb;

private:
  NdbBranch();
  ~NdbBranch();

  NdbApiSignal* theSignal;
  Uint32       theSignalAddress;
  Uint32       theBranchAddress;
  Uint32	theBranchLabel;
  Uint32	theSubroutine;
  NdbBranch*	theNext;
};

class NdbCall
{
friend class NdbOperation;
friend class Ndb;

private:
  NdbCall();
  ~NdbCall();

  NdbApiSignal* theSignal;
  Uint32       theSignalAddress;
  Uint32	theSubroutine;
  NdbCall*	theNext;
};

#endif
