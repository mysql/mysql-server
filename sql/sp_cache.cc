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

#ifdef __GNUC__
#pragma implementation
#endif

#include "mysql_priv.h"
#include "sp_cache.h"
#include "sp_head.h"

static byte *
hash_get_key_for_sp_head(const byte *ptr, uint *plen,
			       my_bool first)
{
  return ((sp_head*)ptr)->name(plen);
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
	    hash_get_key_for_sp_head, 0, 0);
}

void
sp_cache::cleanup()
{
  hash_free(&m_hashtable);
}
