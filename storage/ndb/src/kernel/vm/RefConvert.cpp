/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <RefConvert.hpp>
#include <stdio.h>
#include <ndb_limits.h>

#include <NdbTap.hpp>
static int
test_numberToBlock()
{
  for (Uint32 i = FIRST_BLOCK; i < (FIRST_BLOCK + 63); i++)
  {
    for (Uint32 j = 0; j < 128; j++)
    {
      Uint32 block = i;
      Uint32 instance = j;
      BlockNumber bn = numberToBlock(block, instance);
      BlockNumber bn_old = numberToBlock_old(block, instance);
      if (bn != bn_old)
      {
        return 1;
      }
      BlockNumber main = blockToMain(bn);
      BlockInstance inst = blockToInstance(bn);
      BlockNumber main_old = blockToMain(bn_old);
      BlockNumber inst_old = blockToInstance(bn_old);
      if (main != main_old)
      {
        return 1;
      }
      if (inst != inst_old)
      {
        return 1;
      }
    }
  }
  for (Uint32 i = FIRST_BLOCK; i < (FIRST_BLOCK + 64); i++)
  {
    Uint32 block = i;
    BlockReference short_ref = numberToRef(block, 0);
    BlockReference long_ref = numberToRef(block, 0, 0);
    if (short_ref != long_ref)
    {
      return 1;
    }
  }
  for (Uint32 i = FIRST_BLOCK; i < (FIRST_BLOCK + 63); i++)
  {
    for (Uint32 j = 0; j < NDBMT_MAX_INSTANCES; j++)
    {
      Uint32 block = i;
      Uint32 instance = j;
      BlockNumber bn = numberToBlock(block, instance);
      BlockNumber main = blockToMain(bn);
      BlockInstance inst = blockToInstance(bn);
      if (main != block)
      {
        return 1;
      }
      if (inst != instance)
      {
        return 1;
      }
    }
  }
  return 0;
}

TAPTEST(RefConvert)
{
  printf("Start RefConvert test\n");
  OK(test_numberToBlock() == 0);
  return 1;
}
