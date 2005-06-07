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
static ulong Cversion = 0;

void
sp_cache_init()
{
  pthread_mutex_init(&Cversion_lock, MY_MUTEX_INIT_FAST);
}

void
sp_cache_clear(sp_cache **cp)
{
  sp_cache *c= *cp;

  if (c)
  {
    delete c;
    *cp= NULL;
  }
}

void
sp_cache_insert(sp_cache **cp, sp_head *sp)
{
  sp_cache *c= *cp;

  if (! c)
    c= new sp_cache();
  if (c)
  {
    ulong v;

    pthread_mutex_lock(&Cversion_lock); // LOCK
    v= Cversion;
    pthread_mutex_unlock(&Cversion_lock); // UNLOCK

    if (c->version < v)
    {
      if (*cp)
	c->remove_all();
      c->version= v;
    }
    c->insert(sp);
    if (*cp == NULL)
      *cp= c;
  }
}

sp_head *
sp_cache_lookup(sp_cache **cp, sp_name *name)
{
  ulong v;
  sp_cache *c= *cp;

  if (! c)
    return NULL;

  pthread_mutex_lock(&Cversion_lock); // LOCK
  v= Cversion;
  pthread_mutex_unlock(&Cversion_lock); // UNLOCK

  if (c->version < v)
  {
    c->remove_all();
    c->version= v;
    return NULL;
  }
  return c->lookup(name->m_qname.str, name->m_qname.length);
}

bool
sp_cache_remove(sp_cache **cp, sp_name *name)
{
  sp_cache *c= *cp;
  bool found= FALSE;

  if (c)
  {
    ulong v;

    pthread_mutex_lock(&Cversion_lock); // LOCK
    v= Cversion++;
    pthread_mutex_unlock(&Cversion_lock); // UNLOCK

    if (c->version < v)
      c->remove_all();
    else
      found= c->remove(name->m_qname.str, name->m_qname.length);
    c->version= v+1;
  }
  return found;
}

void
sp_cache_invalidate()
{
  pthread_mutex_lock(&Cversion_lock); // LOCK
  Cversion++;
  pthread_mutex_unlock(&Cversion_lock); // UNLOCK
}

static byte *
hash_get_key_for_sp_head(const byte *ptr, uint *plen,
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
