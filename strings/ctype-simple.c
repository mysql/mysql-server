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

#include <my_global.h>
#include "m_ctype.h"
#include "dbug.h"
#include "assert.h"

int my_strnxfrm_simple(CHARSET_INFO * cs, 
                       uchar *dest, uint len,
                       const uchar *src, uint srclen)
{
  uchar *map= cs->sort_order;
  DBUG_ASSERT(len >= srclen);
  
  for ( ; len > 0 ; len-- )
    *dest++= map[*src++];
  return srclen;
}

int my_strnncoll_simple(CHARSET_INFO * cs, const uchar *s, uint slen, 
			const uchar *t, uint tlen)
{
  int len = ( slen > tlen ) ? tlen : slen;
  uchar *map= cs->sort_order;
  while (len--)
  {
    if (map[*s++] != map[*t++])
      return ((int) map[s[-1]] - (int) map[t[-1]]);
  }
  return (int) (slen-tlen);
}

void my_caseup_str_8bit(CHARSET_INFO * cs,char *str)
{
  register uchar *map=cs->to_upper;
  while ((*str = (char) map[(uchar) *str]) != 0)
    str++;
}

void my_casedn_str_8bit(CHARSET_INFO * cs,char *str)
{
  register uchar *map=cs->to_lower;
  while ((*str = (char) map[(uchar)*str]) != 0)
    str++;
}

void my_caseup_8bit(CHARSET_INFO * cs, char *str, uint length)
{
  register uchar *map=cs->to_upper;
  for ( ; length>0 ; length--, str++)
    *str= (char) map[(uchar)*str];
}

void my_casedn_8bit(CHARSET_INFO * cs, char *str, uint length)
{
  register uchar *map=cs->to_lower;
  for ( ; length>0 ; length--, str++)
    *str= (char) map[(uchar) *str];
}

void my_tosort_8bit(CHARSET_INFO *cs, char *str, uint length)
{
  register uchar *map=cs->sort_order;
  for ( ; length>0 ; length--, str++)
    *str= (char) map[(uchar) *str];
}

int my_strcasecmp_8bit(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uchar *map=cs->to_upper;
  while (map[(uchar) *s] == map[(uchar) *t++])
    if (!*s++) return 0;
  return ((int) map[(uchar) s[0]] - (int) map[(uchar) t[-1]]);
}


int my_strncasecmp_8bit(CHARSET_INFO * cs,
				const char *s, const char *t, uint len)
{
 register uchar *map=cs->to_upper;
 while (len-- != 0 && map[(uchar)*s++] == map[(uchar)*t++]) ;
   return (int) len+1;
}

int my_mb_wc_8bit(CHARSET_INFO *cs,my_wc_t *wc,
		  const unsigned char *str,
		  const unsigned char *end __attribute__((unused)))
{
  *wc=cs->tab_to_uni[*str];
  return (!wc[0] && str[0]) ? MY_CS_ILSEQ : 1;
}

int my_wc_mb_8bit(CHARSET_INFO *cs,my_wc_t wc,
		  unsigned char *s,
		  unsigned char *e __attribute__((unused)))
{
  MY_UNI_IDX *idx;

  for(idx=cs->tab_from_uni; idx->tab ; idx++){
    if(idx->from<=wc && idx->to>=wc){
      s[0]=idx->tab[wc-idx->from];
      return (!s[0] && wc) ? MY_CS_ILUNI : 1;
    }
  }
  return MY_CS_ILUNI;
}



#ifndef NEW_HASH_FUNCTION

	/* Calc hashvalue for a key, case indepenently */

uint my_hash_caseup_simple(CHARSET_INFO *cs, const byte *key, uint length)
{
  register uint nr=1, nr2=4;
  register uchar *map=cs->to_upper;
  
  while (length--)
  {
    nr^= (((nr & 63)+nr2)*
         ((uint) (uchar) map[(uchar)*key++])) + (nr << 8);
    nr2+=3;
  }
  return((uint) nr);
}

#else

uint my_hash_caseup_simple(CHARSET_INFO *cs, const byte *key, uint len)
{
  const byte *end=key+len;
  uint hash;
  for (hash = 0; key < end; key++)
  {
    hash *= 16777619;
    hash ^= (uint) (uchar) my_toupper(cs,*key);
  }
  return (hash);
}

#endif
				  
void my_hash_sort_simple(CHARSET_INFO *cs,
				const uchar *key, uint len,
				ulong *nr1, ulong *nr2)
{
  register uchar *sort_order=cs->sort_order;
  const uchar *pos = key;
  
  key+= len;
  
  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint) sort_order[(uint) *pos])) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}
