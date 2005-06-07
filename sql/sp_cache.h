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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

class sp_head;
class sp_cache;

/* Initialize the SP caching once at startup */
void sp_cache_init();

/* Clear the cache *cp and set *cp to NULL */
void sp_cache_clear(sp_cache **cp);

/* Insert an SP to cache. If 'cp' points to NULL, it's set to a new cache */
void sp_cache_insert(sp_cache **cp, sp_head *sp);

/* Lookup an SP in cache */
sp_head *sp_cache_lookup(sp_cache **cp, sp_name *name);

/* Remove an SP from cache. Returns true if something was removed */
bool sp_cache_remove(sp_cache **cp, sp_name *name);

/* Invalidate a cache */
void sp_cache_invalidate();


/*
 *
 * The cache class. Don't use this directly, use the C API above
 *
 */

class sp_cache
{
public:

  ulong version;

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

  inline bool
  remove(char *name, uint namelen)
  {
    sp_head *sp= lookup(name, namelen);

    if (sp)
    {
      hash_delete(&m_hashtable, (byte *)sp);
      return TRUE;
    }
    return FALSE;
  }

  inline void
  remove_all()
  {
    cleanup();
    init();
  }

private:

  HASH m_hashtable;

}; // class sp_cache

#endif /* _SP_CACHE_H_ */
