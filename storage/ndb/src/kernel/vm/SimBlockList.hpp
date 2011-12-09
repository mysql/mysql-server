/*
   Copyright (C) 2003, 2005, 2006 MySQL AB, 2008 Sun Microsystems, Inc.
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

#ifndef SimBlockList_H
#define SimBlockList_H

#include <SimulatedBlock.hpp>

struct EmulatorData;

class SimBlockList 
{
public:
  SimBlockList();
  ~SimBlockList();
  
  void load(EmulatorData&);
  void unload();
private:
  int noOfBlocks;
  SimulatedBlock** theList;
};

inline
SimBlockList::SimBlockList(){
  noOfBlocks = 0;
  theList    = 0;
}

inline
SimBlockList::~SimBlockList(){
  unload();
}

#endif
