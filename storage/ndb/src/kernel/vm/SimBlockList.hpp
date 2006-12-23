/* Copyright (C) 2003 MySQL AB

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

#ifndef SimBlockList_H
#define SimBlockList_H

#include <SimulatedBlock.hpp>

class EmulatorData;

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
