/* -*- C++ -*- */
/* Copyright (C) 2002 MySQL AB

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

#ifndef _SP_CACHE_H_
#define _SP_CACHE_H_

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class sp_head;

class sp_cache
{
public:

  sp_cache();

  ~sp_cache();

  void
  init();

  void
  cleanup();

  inline void
  insert(sp_head *sp)
  {
    my_hash_insert(&m_hashtable, (const byte *)sp);
  }

  inline sp_head *
  lookup(char *name, uint namelen)
  {
    return (sp_head *)hash_search(&m_hashtable, (const byte *)name, namelen);
  }

  inline void
  remove(sp_head *sp)
  {
    hash_delete(&m_hashtable, (byte *)sp);
  }

private:

  HASH m_hashtable;

}; // class sp_cache

#endif /* _SP_CACHE_H_ */
