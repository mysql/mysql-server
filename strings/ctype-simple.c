/* Copyright (C) 2000 MySQL AB

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
                       char *dest, uint len,
                       const char *src, uint srclen)
{
  DBUG_ASSERT(len >= srclen);
  
  for ( ; len > 0 ; len-- )
    *dest++= (char) cs->sort_order[(uchar) *src++];
  return srclen;
}

int my_strnncoll_simple(CHARSET_INFO * cs,const char *s, uint slen, 
				const char *t, uint tlen)
{
  int len = ( slen > tlen ) ? tlen : slen;
  while (len--)
  {
    if (cs->sort_order[(uchar) *s++] != cs->sort_order[(uchar) *t++])
      return ((int) cs->sort_order[(uchar) s[-1]] -
              (int) cs->sort_order[(uchar) t[-1]]);
  }
  return (int) (slen-tlen);
}


void my_caseup_str_8bit(CHARSET_INFO * cs,char *str)
{
  while ((*str = (char) my_toupper(cs,(uchar) *str)) != 0)
    str++;
}


void my_casedn_str_8bit(CHARSET_INFO * cs,char *str)
{
  while ((*str = (char) my_tolower(cs,(uchar)*str)) != 0)
    str++;
}


void my_caseup_8bit(CHARSET_INFO * cs, char *str, uint length)
{
  for ( ; length>0 ; length--, str++)
    *str= (char) my_toupper(cs,(uchar)*str);
}

void my_casedn_8bit(CHARSET_INFO * cs, char *str, uint length)
{
  for ( ; length>0 ; length--, str++)
    *str= (char)my_tolower(cs,(uchar) *str);
}


int my_strcasecmp_8bit(CHARSET_INFO * cs,const char *s, const char *t)
{
  while (my_toupper(cs,(uchar) *s) == my_toupper(cs,(uchar) *t++))
    if (!*s++) return 0;
  return ((int) my_toupper(cs,(uchar) s[0]) - (int) my_toupper(cs,(uchar) t[-1]));
}


int my_strncasecmp_8bit(CHARSET_INFO * cs,
				const char *s, const char *t, uint len)
{
 while (len-- != 0 && my_toupper(cs,(uchar)*s++) == my_toupper(cs,(uchar)*t++)) ;
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
