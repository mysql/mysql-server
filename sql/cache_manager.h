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
#pragma interface			/* gcc class implementation */
#endif

#ifndef _CACHE_MANAGER_H_
#define _CACHE_MANAGER_H_
#endif

/*
  cache_manager.h
  -----------------------------------------------------------------------
  The cache_manager class manages a number of blocks (which are allocatable
  units of memory).
  -----------------------------------------------------------------------
*/



class cache_manager {
  void **abs_list;                /* List holding block abstraction ptrs */

  typedef struct free_blks {
    struct free_blks *next, **prev;
    uint Size;
  } FREE_BLKS;
  FREE_BLKS *base_ptr;            /* Pointer to newly allocated sys mem  */


  /* list manipulation methods */
  void *link_into_abs(void *);    /* Return an abstract pointer to blk   */
  bool *unlink_from_abs(void *);  /* Used to dealloc a blk               */
  void *find_in_fblist(uint);     /*   */

  /* memory allocation methods */
  void defrag(void);              /* Defragment the cache                */
  bool *init_blk(void *);         /* Return a pointer to new list        */

 public:
  cache_manager(uint);            /* Get allocation of size from system  */
  ~cache_manager(void);           /* Destructor; return the cache        */

  void *alloc(uint);              /* Alloc size bytes from the cache     */
  bool *dealloc(void *);          /* Deallocate blocks (with *ptr_arg)   */
  void clear(void);               /* Clear the cache                     */
};




