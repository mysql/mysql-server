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

#ifdef __GNUC__
#pragma implementation		       /* gcc: Class implementation */
#endif

#include <global.h>
#include <my_sys.h>
#include "cache_manager.h"

/*
  cache_manager.cc
  -----------------------------------------------------------------------
  The cache_manager class manages a number of blocks (which are allocatable
  units of memory).
  -----------------------------------------------------------------------
*/

#define HEADER_LENGTH ALIGN_SIZE(8)
#define SUFFIX_LENGTH 4

#define ALLOC_MASK 0x3FFFFFFFL
#define FREE_BIT   (1L << 31)
#define LOCK_BIT   (1L << 30)
#define SMALLEST_BLOCK 32



/*
** Internal Methods
** --------------------
*/


/* list manipulation methods */
void *cache_manager::init_list(void)
{

return;
}


void *cache_manager::link_into_abs(void *ptr)
{
  for (int i(0); (*abs_list)[i] != NULL; i++);

  (*abs_list)[i] = ptr;

  return (abs_list)[i]; // ???
}



bool *cache_manager::unlink_from_abs(void *ptr)
{
  (*ptr) = NULL;

return;
}



/* memory allocation methods */
void *cache_manager::find_in_llist(uint)
{

return;
}


void cache_manager::defrag(void)
{
  printf("Defragging: ..........");

  return;
}



/*
** Public Methods
** ------------------
*/

cache_manager::cache_manager(uint size)
{
  base_ptr = my_malloc(size, MY_WME);     /* MY_WME = write mesg on err */

  return;
}


cache_manager::~cache_manager(void)
{
  free(base_ptr);
  delete base_ptr;

  return;
}


void *cache_manager::alloc(uint size)
{
  void *llist;
  void *abs_ptr;
  
  size=ALIGN_SIZE(size+HEADER_LENGTH+SUFFIX_LENGTH);
  if (!(llist = find_in_llist(size)))
    {
      //defrag();
      if (!(llist = find_in_llist(size)))
	return 0;     /* return null pointer, buffer exhausted! */
    }
  size_of_found_block=int4korr((char*) llist) & ALLOC_MASK;
  //  if (size_of_found_block < SMALLEST_BLOCK)
  
  abs_ptr = link_into_abs(llist);
  return abs_ptr;
}


void cache_manager::dealloc(void)
{
  printf("Deallocating: ..........\n");

  return;
}



void cache_manager::clear(void)
{
  // reset the internal linked list, forgetting all pointers to mem blks

  return;
}
