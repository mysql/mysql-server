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

#include "mysql_priv.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif
#include "sp_cache.h"
#include "sp_head.h"

static pthread_mutex_t Cversion_lock;
static ulong volatile Cversion= 0;


/*
  Cache of stored routines. 
*/

class sp_cache
{
public:
  ulong version;

  sp_cache();
  ~sp_cache();

  inline void insert(sp_head *sp)
  {
    /* TODO: why don't we check return value? */
    my_hash_insert(&m_hashtable, (const byte *)sp);
  }

  inline sp_head *lookup(char *name, uint namelen)
  {
    return (sp_head *)hash_search(&m_hashtable, (const byte *)name, namelen);
  }

#ifdef NOT_USED
  inline bool remove(char *name, uint namelen)
  {
    sp_head *sp= lookup(name, namelen);
    if (sp)
    {
      hash_delete(&m_hashtable, (byte *)sp);
      return TRUE;
    }
    return FALSE;
  }
#endif 

  inline void remove_all()
  {
    cleanup();
    init();
  }

private:
  void init();
  void cleanup();

  /* All routines in this cache */
  HASH m_hashtable;
}; // class sp_cache


/* Initialize the SP caching once at startup */

void sp_cache_init()
{
  pthread_mutex_init(&Cversion_lock, MY_MUTEX_INIT_FAST);
}


/*
  Clear the cache *cp and set *cp to NULL.

  SYNOPSIS
    sp_cache_clear()
    cp  Pointer to cache to clear

  NOTE
    This function doesn't invalidate other caches.
*/

void sp_cache_clear(sp_cache **cp)
{
  sp_cache *c= *cp;

  if (c)
  {
    delete c;
    *cp= NULL;
  }
}


/*
  Insert a routine into the cache.

  SYNOPSIS
    sp_cache_insert()
     cp  The cache to put routine into
     sp  Routine to insert.
      
  TODO: Perhaps it will be more straightforward if in case we returned an 
        error from this function when we couldn't allocate sp_cache. (right
        now failure to put routine into cache will cause a 'SP not found'
        error to be reported at some later time)
*/

void sp_cache_insert(sp_cache **cp, sp_head *sp)
{
  sp_cache *c;
  ulong v;

  if (!(c= *cp))
  {
    if (!(c= new sp_cache()))
      return;                                   // End of memory error
    c->version= Cversion;      // No need to lock when reading long variable
  }
  DBUG_PRINT("info",("sp_cache: inserting: %.*s", sp->m_qname.length,
                     sp->m_qname.str));
  c->insert(sp);
  *cp= c;                                       // Update *cp if it was NULL
}


/* 
  Look up a routine in the cache.
  SYNOPSIS
    sp_cache_lookup()
      cp    Cache to look into
      name  Name of rutine to find
      
  NOTE
    An obsolete (but not more obsolete then since last
    sp_cache_flush_obsolete call) routine may be returned.

  RETURN 
    The routine or
    NULL if the routine not found.
*/

sp_head *sp_cache_lookup(sp_cache **cp, sp_name *name)
{
  sp_cache *c= *cp;
  if (! c)
    return NULL;
  return c->lookup(name->m_qname.str, name->m_qname.length);
}


/*
  Invalidate all routines in all caches.

  SYNOPSIS
    sp_cache_invalidate()
      
  NOTE
    This is called when a VIEW definition is modifed. We can't destroy sp_head
    objects here as one may modify VIEW definitions from prelocking-free SPs.
*/

void sp_cache_invalidate()
{
  DBUG_PRINT("info",("sp_cache: invalidating"));
  thread_safe_increment(Cversion, &Cversion_lock);
}


/*
  Remove out-of-date SPs from the cache. 
  
  SYNOPSIS
    sp_cache_flush_obsolete()
      cp  Cache to flush

  NOTE
    This invalidates pointers to sp_head objects this thread uses.
    In practice that means 'dont call this function when inside SP'.
*/

void sp_cache_flush_obsolete(sp_cache **cp)
{
  sp_cache *c= *cp;
  if (c)
  {
    ulong v;
    v= Cversion;                 // No need to lock when reading long variable
    if (c->version < v)
    {
      DBUG_PRINT("info",("sp_cache: deleting all functions"));
      /* We need to delete all elements. */
      c->remove_all();
      c->version= v;
    }
  }
}


/*************************************************************************
  Internal functions 
 *************************************************************************/

static byte *hash_get_key_for_sp_head(const byte *ptr, uint *plen,
			       my_bool first)
{
  sp_head *sp= (sp_head *)ptr;
  *plen= sp->m_qname.length;
  return (byte*) sp->m_qname.str;
}


static void
hash_free_sp_head(void *p)
{
  sp_head *sp= (sp_head *)p;
  delete sp;
}


sp_cache::sp_cache()
{
  init();
}


sp_cache::~sp_cache()
{
  hash_free(&m_hashtable);
}


void
sp_cache::init()
{
  hash_init(&m_hashtable, system_charset_info, 0, 0, 0,
	    hash_get_key_for_sp_head, hash_free_sp_head, 0);
  version= 0;
}


void
sp_cache::cleanup()
{
  hash_free(&m_hashtable);
}
