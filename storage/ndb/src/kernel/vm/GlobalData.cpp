/* Copyright (C) 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <ndb_global.h>
#include <kernel_types.h>
#include <SimulatedBlock.hpp>
#include "GlobalData.hpp"

SimulatedBlock*
GlobalData::getBlock(BlockNumber blockNo, Uint32 instanceNo)
{
  SimulatedBlock* b = getBlock(blockNo);
  if (b != 0 && instanceNo != 0)
    b = b->getInstance(instanceNo);
  return b;
}
