/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/*
  remove all records from database
  Identical as hp_create() and hp_open() but used HP_SHARE* instead of name and
  database remains open.
*/

#include "heapdef.h"

void heap_clear(HP_INFO *info)
{
  hp_clear(info->s);
}

void hp_clear(HP_SHARE *info)
{
  uint key;
  DBUG_ENTER("hp_clear");

  if (info->block.levels)
    VOID(hp_free_level(&info->block,info->block.levels,info->block.root,
			(byte*) 0));
  info->block.levels=0;
  for (key=0 ; key < info->keys ; key++)
  {
    HP_KEYDEF *keyinfo = info->keydef + key;
    if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
    {
      delete_tree(&keyinfo->rb_tree);
    }
    else
    {
      HP_BLOCK *block= &keyinfo->block;
      if (block->levels)
        VOID(hp_free_level(block,block->levels,block->root,(byte*) 0));
      block->levels=0;
      block->last_allocated=0;
    }
  }
  info->records=info->deleted=info->data_length=info->index_length=0;
  info->blength=1;
  info->changed=0;
  info->del_link=0;
  DBUG_VOID_RETURN;
}
