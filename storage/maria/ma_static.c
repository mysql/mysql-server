/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
  Static variables for MARIA library. All definied here for easy making of
  a shared library
*/

#ifndef _global_h
#include "maria_def.h"
#endif

LIST	*maria_open_list=0;
uchar	NEAR maria_file_magic[]=
{ (uchar) 254, (uchar) 254,'\007', '\001', };
uchar	NEAR maria_pack_file_magic[]=
{ (uchar) 254, (uchar) 254,'\010', '\002', };
uint	maria_quick_table_bits=9;
ulong	maria_block_size= MARIA_KEY_BLOCK_LENGTH;
my_bool maria_flush=0, maria_delay_key_write=0, maria_single_user=0;
#if defined(THREAD) && !defined(DONT_USE_RW_LOCKS)
ulong maria_concurrent_insert= 2;
#else
ulong maria_concurrent_insert= 0;
#endif
my_off_t maria_max_temp_length= MAX_FILE_SIZE;
ulong    maria_bulk_insert_tree_size=8192*1024;
ulong    maria_data_pointer_size=4;

KEY_CACHE maria_key_cache_var;
KEY_CACHE *maria_key_cache= &maria_key_cache_var;

/*
  read_vec[] is used for converting between P_READ_KEY.. and SEARCH_
  Position is , == , >= , <= , > , <
*/

uint NEAR maria_read_vec[]=
{
  SEARCH_FIND, SEARCH_FIND | SEARCH_BIGGER, SEARCH_FIND | SEARCH_SMALLER,
  SEARCH_NO_FIND | SEARCH_BIGGER, SEARCH_NO_FIND | SEARCH_SMALLER,
  SEARCH_FIND | SEARCH_PREFIX, SEARCH_LAST, SEARCH_LAST | SEARCH_SMALLER,
  MBR_CONTAIN, MBR_INTERSECT, MBR_WITHIN, MBR_DISJOINT, MBR_EQUAL
};

uint NEAR maria_readnext_vec[]=
{
  SEARCH_BIGGER, SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_BIGGER, SEARCH_SMALLER,
  SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_SMALLER
};
