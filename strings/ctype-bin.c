/* Copyright (C) 2002 MySQL AB & tommy@valley.ne.jp.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* This file is for binary pseudo charset, created by bar@mysql.com */


#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"


static int my_strnncoll_binary(CHARSET_INFO * cs __attribute__((unused)),
				const uchar *s, uint slen,
				const uchar *t, uint tlen)
{
  int len = ( slen > tlen ) ? tlen : slen;
  return memcmp(s,t,len);
}

static void my_caseup_str_bin(CHARSET_INFO *cs __attribute__((unused)),
		       char *str __attribute__((unused)))
{
}

static void my_casedn_str_bin(CHARSET_INFO * cs __attribute__((unused)),
		       char *str __attribute__((unused)))
{
}

static void my_caseup_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length __attribute__((unused)))
{
}

static void my_casedn_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length  __attribute__((unused)))
{
}

static void my_tosort_bin(CHARSET_INFO * cs __attribute__((unused)),
		   char *str __attribute__((unused)),
		   uint length  __attribute__((unused)))
{
}

static int my_strcasecmp_bin(CHARSET_INFO * cs __attribute__((unused)),
		      const char *s, const char *t)
{
  return strcmp(s,t);
}

static int my_strncasecmp_bin(CHARSET_INFO * cs __attribute__((unused)),
				const char *s, const char *t, uint len)
{
  return memcmp(s,t,len);
}

static int my_mb_wc_bin(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t *wc,
		  const unsigned char *str,
		  const unsigned char *end __attribute__((unused)))
{
  *wc=str[0];
  return 1;
}

static int my_wc_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t wc,
		  unsigned char *s,
		  unsigned char *e __attribute__((unused)))
{
  if ( wc<256 )
  {
    s[0]=wc;
    return 1;
  }
  return MY_CS_ILUNI;
}


#ifndef NEW_HASH_FUNCTION

	/* Calc hashvalue for a key, case indepenently */

static uint my_hash_caseup_bin(CHARSET_INFO *cs __attribute__((unused)),
				const byte *key, uint length)
{
  register uint nr=1, nr2=4;
  
  while (length--)
  {
    nr^= (((nr & 63)+nr2)*
         ((uint) (uchar) *key++)) + (nr << 8);
    nr2+=3;
  }
  return((uint) nr);
}

#else

static uint my_hash_caseup_bin(CHARSET_INFO *cs __attribute__((unused)),
			       const byte *key, uint len)
{
  const byte *end=key+len;
  uint hash;
  for (hash = 0; key < end; key++)
  {
    hash *= 16777619;
    hash ^= (uint) (uchar) *key;
  }
  return (hash);
}

#endif
				  
void my_hash_sort_bin(CHARSET_INFO *cs __attribute__((unused)),
		      const uchar *key, uint len,ulong *nr1, ulong *nr2)
{
  const uchar *pos = key;
  
  key+= len;
  
  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint)*pos)) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}



static CHARSET_INFO my_charset_bin_st =
{
    63,				/* number       */
    MY_CS_COMPILED|MY_CS_BINSORT,/* state         */
    "binary",			/* name          */
    "",				/* comment       */
    NULL,			/* ctype         */
    NULL,			/* to_lower      */
    NULL,			/* to_upper      */
    NULL,			/* sort_order    */
    NULL,			/* tab_to_uni    */
    NULL,			/* tab_from_uni  */
    0,				/* strxfrm_multiply */
    my_strnncoll_binary,	/* strnncoll     */
    NULL,			/* strxnfrm      */
    NULL,			/* like_rabge    */
    0,				/* mbmaxlen      */
    NULL,			/* ismbchar      */
    NULL,			/* ismbhead      */
    NULL,			/* mbcharlen     */
    my_mb_wc_bin,		/* mb_wc         */
    my_wc_mb_bin,		/* wc_mb         */
    my_caseup_str_bin,		/* caseup_str    */
    my_casedn_str_bin,		/* casedn_str    */
    my_caseup_bin,		/* caseup        */
    my_casedn_bin,		/* casedn        */
    my_tosort_bin,		/* tosort        */
    my_strcasecmp_bin,		/* strcasecmp    */
    my_strncasecmp_bin,		/* strncasecmp   */
    my_hash_caseup_bin,		/* hash_caseup   */
    my_hash_sort_bin,		/* hash_sort     */
    255,			/* max_sort_char */
    my_snprintf_8bit		/* snprintf      */
};


CHARSET_INFO *my_charset_bin = &my_charset_bin_st;
