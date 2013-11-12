/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef WAIT_QUEUE_HPP
#define WAIT_QUEUE_HPP

#include "ArrayPool.hpp"
#include "SimulatedBlock.hpp"

#define JAM_FILE_ID 302


template <class Block,
	  class T, 
	  void (Block::* Function)(Signal*, Ptr<T>)>
class WaitQueue {
public:
  WaitQueue(Block & block, ArrayPool<T>& pool){
  }
  
  void add(Ptr<T>, void (Block::* Callback)(Signal*, Ptr<T>)) {}
  void complete(Ptr<T>) {}
};


#undef JAM_FILE_ID

#endif
