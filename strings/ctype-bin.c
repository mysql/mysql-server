/* Copyright (c) 2002-2007 MySQL AB & tommy@valley.ne.jp
   Copyright (c) 2002, 2014, Oracle and/or its affiliates.
   Copyright (c) 2009, 2014, SkySQL Ab.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA */

/* This file is for binary pseudo charset, created by bar@mysql.com */


#include "strings_def.h"
#include <m_ctype.h>

static const uchar ctype_bin[]=
{
  0,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
  72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
  16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
  16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};


/* Dummy array for toupper / tolower / sortorder */

static const uchar bin_char_array[] =
{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};


static my_bool 
my_coll_init_8bit_bin(struct charset_info_st *cs,
                      void *(*alloc)(size_t) __attribute__((unused)))
{
  cs->max_sort_char=255; 
  return FALSE;
}

static int my_strnncoll_binary(CHARSET_INFO * cs __attribute__((unused)),
                               const uchar *s, size_t slen,
                               const uchar *t, size_t tlen,
                               my_bool t_is_prefix)
{
  size_t len=min(slen,tlen);
  int cmp= memcmp(s,t,len);
  return cmp ? cmp : (int)((t_is_prefix ? len : slen) - tlen);
}


size_t my_lengthsp_binary(CHARSET_INFO *cs __attribute__((unused)),
                          const char *ptr __attribute__((unused)),
                          size_t length)
{
  return length;
}


/*
  Compare two strings. Result is sign(first_argument - second_argument)

  SYNOPSIS
    my_strnncollsp_binary()
    cs			Chararacter set
    s			String to compare
    slen		Length of 's'
    t			String to compare
    tlen		Length of 't'

  NOTE
   This function is used for real binary strings, i.e. for
   BLOB, BINARY(N) and VARBINARY(N).
   It compares trailing spaces as spaces.

  RETURN
  < 0	s < t
  0	s == t
  > 0	s > t
*/

static int my_strnncollsp_binary(CHARSET_INFO * cs __attribute__((unused)),
                                 const uchar *s, size_t slen,
                                 const uchar *t, size_t tlen,
                                 my_bool diff_if_only_endspace_difference
                                 __attribute__((unused)))
{
  return my_strnncoll_binary(cs,s,slen,t,tlen,0);
}


static int my_strnncoll_8bit_bin(CHARSET_INFO * cs __attribute__((unused)),
                                 const uchar *s, size_t slen,
                                 const uchar *t, size_t tlen,
                                 my_bool t_is_prefix)
{
  size_t len=min(slen,tlen);
  int cmp= memcmp(s,t,len);
  return cmp ? cmp : (int)((t_is_prefix ? len : slen) - tlen);
}


/*
  Compare two strings. Result is sign(first_argument - second_argument)

  SYNOPSIS
    my_strnncollsp_8bit_bin()
    cs			Chararacter set
    s			String to compare
    slen		Length of 's'
    t			String to compare
    tlen		Length of 't'
    diff_if_only_endspace_difference
		        Set to 1 if the strings should be regarded as different
                        if they only difference in end space

  NOTE
   This function is used for character strings with binary collations.
   The shorter string is extended with end space to be as long as the longer
   one.

  RETURN
  < 0	s < t
  0	s == t
  > 0	s > t
*/

static int my_strnncollsp_8bit_bin(CHARSET_INFO * cs __attribute__((unused)),
                                   const uchar *a, size_t a_length, 
                                   const uchar *b, size_t b_length,
                                   my_bool diff_if_only_endspace_difference)
{
  const uchar *end;
  size_t length;
  int res;

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
  diff_if_only_endspace_difference= 0;
#endif

  end= a + (length= min(a_length, b_length));
  while (a < end)
  {
    if (*a++ != *b++)
      return ((int) a[-1] - (int) b[-1]);
  }
  res= 0;
  if (a_length != b_length)
  {
    int swap= 1;
    /*
      Check the next not space character of the longer key. If it's < ' ',
      then it's smaller than the other key.
    */
    if (diff_if_only_endspace_difference)
      res= 1;                                   /* Assume 'a' is bigger */
    if (a_length < b_length)
    {
      /* put shorter key in s */
      a_length= b_length;
      a= b;
      swap= -1;					/* swap sign of result */
      res= -res;
    }
    for (end= a + a_length-length; a < end ; a++)
    {
      if (*a != ' ')
	return (*a < ' ') ? -swap : swap;
    }
  }
  return res;
}


/* This function is used for all conversion functions */

static size_t my_case_str_bin(CHARSET_INFO *cs __attribute__((unused)),
                              char *str __attribute__((unused)))
{
  return 0;
}


static size_t my_case_bin(CHARSET_INFO *cs __attribute__((unused)),
                          char *src __attribute__((unused)),
                          size_t srclen,
                          char *dst __attribute__((unused)),
                          size_t dstlen __attribute__((unused)))
{
  return srclen;
}


static int my_strcasecmp_bin(CHARSET_INFO * cs __attribute__((unused)),
			     const char *s, const char *t)
{
  return strcmp(s,t);
}


uint my_mbcharlen_8bit(CHARSET_INFO *cs __attribute__((unused)),
                      uint c __attribute__((unused)))
{
  return 1;
}


static int my_mb_wc_bin(CHARSET_INFO *cs __attribute__((unused)),
			my_wc_t *wc,
			const uchar *str,
			const uchar *end __attribute__((unused)))
{
  if (str >= end)
    return MY_CS_TOOSMALL;
  
  *wc=str[0];
  return 1;
}


static int my_wc_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
			my_wc_t wc,
			uchar *s,
			uchar *e __attribute__((unused)))
{
  if (s >= e)
    return MY_CS_TOOSMALL;

  if (wc < 256)
  {
    s[0]= (char) wc;
    return 1;
  }
  return MY_CS_ILUNI;
}


void my_hash_sort_8bit_bin(CHARSET_INFO *cs __attribute__((unused)),
                           const uchar *key, size_t len,
                           ulong *nr1, ulong *nr2)
{
  const uchar *pos = key;
  
  /*
     Remove trailing spaces. We have to do this to be able to compare
    'A ' and 'A' as identical
  */
  key= skip_trailing_space(key, len);

  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint)*pos)) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}


void my_hash_sort_bin(CHARSET_INFO *cs __attribute__((unused)),
		      const uchar *key, size_t len,ulong *nr1, ulong *nr2)
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


/*
  The following defines is here to keep the following code identical to
  the one in ctype-simple.c
*/

#define likeconv(s,A) (A)
#define INC_PTR(cs,A,B) (A)++


static
int my_wildcmp_bin_impl(CHARSET_INFO *cs,
                        const char *str,const char *str_end,
                        const char *wildstr,const char *wildend,
                        int escape, int w_one, int w_many, int recurse_level)
{
  int result= -1;			/* Not found, using wildcards */

  if (my_string_stack_guard && my_string_stack_guard(recurse_level))
    return 1;
  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return(1);			/* No match */
      if (wildstr == wildend)
	return(str != str_end);		/* Match if both are at end */
      result=1;				/* Found an anchor char */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)		/* Skip one char if possible */
	  return(result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {					/* Found w_many */
      uchar cmp;
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	    return(-1);
	  INC_PTR(cs,str,str_end);
	  continue;
	}
	break;				/* Not a wild character */
      }
      if (wildstr == wildend)
	return(0);			/* match if w_many is last */
      if (str == str_end)
	return(-1);
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;

      INC_PTR(cs,wildstr,wildend);	/* This is compared through cmp */
      cmp=likeconv(cs,cmp);
      do
      {
	while (str != str_end && (uchar) likeconv(cs,*str) != cmp)
	  str++;
	if (str++ == str_end)
	  return(-1);
	{
	  int tmp=my_wildcmp_bin_impl(cs,str,str_end,wildstr,wildend,escape,w_one,
                                      w_many, recurse_level + 1);
	  if (tmp <= 0)
	    return(tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return(str != str_end ? 1 : 0);
}

int my_wildcmp_bin(CHARSET_INFO *cs,
                   const char *str,const char *str_end,
                   const char *wildstr,const char *wildend,
                   int escape, int w_one, int w_many)
{
  return my_wildcmp_bin_impl(cs, str, str_end,
                             wildstr, wildend,
                             escape, w_one, w_many, 1);
}


static size_t my_strnxfrm_bin(CHARSET_INFO *cs __attribute__((unused)),
                              uchar *dest, size_t dstlen,
                              const uchar *src, size_t srclen)
{
  if (dest != src)
    memcpy(dest, src, min(dstlen,srclen));
  if (dstlen > srclen)
    bfill(dest + srclen, dstlen - srclen, 0);
  return dstlen;
}


static
size_t my_strnxfrm_8bit_bin(CHARSET_INFO *cs __attribute__((unused)),
                            uchar *dest, size_t dstlen,
                            const uchar *src, size_t srclen)
{
  if (dest != src)
    memcpy(dest, src, min(dstlen,srclen));
  if (dstlen > srclen)
    bfill(dest + srclen, dstlen - srclen, ' ');
  return dstlen;
}


static
uint my_instr_bin(CHARSET_INFO *cs __attribute__((unused)),
		  const char *b, size_t b_length,
		  const char *s, size_t s_length,
		  my_match_t *match, uint nmatch)
{
  register const uchar *str, *search, *end, *search_end;

  if (s_length <= b_length)
  {
    if (!s_length)
    {
      if (nmatch)
      {
        match->beg= 0;
        match->end= 0;
        match->mb_len= 0;
      }
      return 1;		/* Empty string is always found */
    }

    str= (const uchar*) b;
    search= (const uchar*) s;
    end= (const uchar*) b+b_length-s_length+1;
    search_end= (const uchar*) s + s_length;

skip:
    while (str != end)
    {
      if ( (*str++) == (*search))
      {
	register const uchar *i,*j;

	i= str;
	j= search+1;

	while (j != search_end)
	  if ((*i++) != (*j++))
            goto skip;

        if (nmatch > 0)
	{
	  match[0].beg= 0;
	  match[0].end= (size_t) (str- (const uchar*)b-1);
	  match[0].mb_len= match[0].end;

	  if (nmatch > 1)
	  {
	    match[1].beg= match[0].end;
	    match[1].end= match[0].end+s_length;
	    match[1].mb_len= match[1].end-match[1].beg;
	  }
	}
	return 2;
      }
    }
  }
  return 0;
}


MY_COLLATION_HANDLER my_collation_8bit_bin_handler =
{
  my_coll_init_8bit_bin,
  my_strnncoll_8bit_bin,
  my_strnncollsp_8bit_bin,
  my_strnxfrm_8bit_bin,
  my_strnxfrmlen_simple,
  my_like_range_simple,
  my_wildcmp_bin,
  my_strcasecmp_bin,
  my_instr_bin,
  my_hash_sort_8bit_bin,
  my_propagate_simple
};


static MY_COLLATION_HANDLER my_collation_binary_handler =
{
  NULL,			/* init */
  my_strnncoll_binary,
  my_strnncollsp_binary,
  my_strnxfrm_bin,
  my_strnxfrmlen_simple,
  my_like_range_simple,
  my_wildcmp_bin,
  my_strcasecmp_bin,
  my_instr_bin,
  my_hash_sort_bin,
  my_propagate_simple
};


static MY_CHARSET_HANDLER my_charset_handler=
{
  NULL,			/* init */
  NULL,			/* ismbchar      */
  my_mbcharlen_8bit,	/* mbcharlen     */
  my_numchars_8bit,
  my_charpos_8bit,
  my_well_formed_len_8bit,
  my_lengthsp_binary,
  my_numcells_8bit,
  my_mb_wc_bin,
  my_wc_mb_bin,
  my_mb_ctype_8bit,
  my_case_str_bin,
  my_case_str_bin,
  my_case_bin,
  my_case_bin,
  my_snprintf_8bit,
  my_long10_to_str_8bit,
  my_longlong10_to_str_8bit,
  my_fill_8bit,
  my_strntol_8bit,
  my_strntoul_8bit,
  my_strntoll_8bit,
  my_strntoull_8bit,
  my_strntod_8bit,
  my_strtoll10_8bit,
  my_strntoull10rnd_8bit,
  my_scan_8bit
};


struct charset_info_st my_charset_bin =
{
    63,0,0,			/* number        */
    MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_PRIMARY,/* state */
    "binary",			/* cs name    */
    "binary",			/* name          */
    "",				/* comment       */
    NULL,			/* tailoring     */
    ctype_bin,			/* ctype         */
    bin_char_array,		/* to_lower      */
    bin_char_array,		/* to_upper      */
    NULL,			/* sort_order    */
    NULL,			/* contractions */
    NULL,			/* sort_order_big*/
    NULL,			/* tab_to_uni    */
    NULL,			/* tab_from_uni  */
    my_unicase_default,         /* caseinfo     */
    NULL,			/* state_map    */
    NULL,			/* ident_map    */
    1,				/* strxfrm_multiply */
    1,                          /* caseup_multiply  */
    1,                          /* casedn_multiply  */
    1,				/* mbminlen      */
    1,				/* mbmaxlen      */
    0,				/* min_sort_char */
    255,			/* max_sort_char */
    0,                          /* pad char      */
    0,                          /* escape_with_backslash_is_dangerous */
    &my_charset_handler,
    &my_collation_binary_handler
};
