/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include "m_ctype.h"
#include "m_string.h"

#ifdef USE_MB


size_t my_caseup_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register uchar *map= cs->to_upper;
  char *str_orig= str;
  
  while (*str)
  {
    /* Pointing after the '\0' is safe here. */
    if ((l= my_ismbchar(cs, str, str + cs->mbmaxlen)))
      str+= l;
    else
    { 
      *str= (char) map[(uchar)*str];
      str++;
    }
  }
  return (size_t) (str - str_orig);
}


size_t my_casedn_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register uchar *map= cs->to_lower;
  char *str_orig= str;
  
  while (*str)
  {
    /* Pointing after the '\0' is safe here. */
    if ((l= my_ismbchar(cs, str, str + cs->mbmaxlen)))
      str+= l;
    else
    {
      *str= (char) map[(uchar)*str];
      str++;
    }
  }
  return (size_t) (str - str_orig);
}


static inline MY_UNICASE_INFO*
get_case_info_for_ch(CHARSET_INFO *cs, uint page, uint offs)
{
  MY_UNICASE_INFO *p;
  return cs->caseinfo ? ((p= cs->caseinfo[page]) ? &p[offs] : NULL) :  NULL;
}


/*
  For character sets which don't change octet length in case conversion.
*/
size_t my_caseup_mb(CHARSET_INFO * cs, char *src, size_t srclen,
                    char *dst __attribute__((unused)),
                    size_t dstlen __attribute__((unused)))
{
  register uint32 l;
  register char *srcend= src + srclen;
  register uchar *map= cs->to_upper;

  DBUG_ASSERT(cs->caseup_multiply == 1);
  DBUG_ASSERT(src == dst && srclen == dstlen);
  DBUG_ASSERT(cs->mbmaxlen == 2);
  
  while (src < srcend)
  {
    if ((l=my_ismbchar(cs, src, srcend)))
    {
      MY_UNICASE_INFO *ch;
      if ((ch= get_case_info_for_ch(cs, (uchar) src[0], (uchar) src[1])))
      {
        *src++= ch->toupper >> 8;
        *src++= ch->toupper & 0xFF;
      }
      else
        src+= l;
    }
    else 
    {
      *src=(char) map[(uchar) *src];
      src++;
    }
  }
  return srclen;
}


size_t my_casedn_mb(CHARSET_INFO * cs, char *src, size_t srclen,
                    char *dst __attribute__((unused)),
                    size_t dstlen __attribute__((unused)))
{
  register uint32 l;
  register char *srcend= src + srclen;
  register uchar *map=cs->to_lower;

  DBUG_ASSERT(cs->casedn_multiply == 1);
  DBUG_ASSERT(src == dst && srclen == dstlen);  
  DBUG_ASSERT(cs->mbmaxlen == 2);
  
  while (src < srcend)
  {
    if ((l= my_ismbchar(cs, src, srcend)))
    {
      MY_UNICASE_INFO *ch;
      if ((ch= get_case_info_for_ch(cs, (uchar) src[0], (uchar) src[1])))
      {
        *src++= ch->tolower >> 8;
        *src++= ch->tolower & 0xFF;
      }
      else
        src+= l;
    }
    else
    {
      *src= (char) map[(uchar)*src];
      src++;
    }
  }
  return srclen;
}


/*
  Case folding functions for character set
  where case conversion can change string octet length.
  For example, in EUCKR,
    _euckr 0xA9A5 == "LATIN LETTER DOTLESS I" (Turkish letter)
  is upper-cased to to
    _euckr 0x49 "LATIN CAPITAL LETTER I"  ('usual' letter I)
  Length is reduced in this example from two bytes to one byte.
*/
static size_t
my_casefold_mb_varlen(CHARSET_INFO *cs,
                      char *src, size_t srclen,
                      char *dst, size_t dstlen __attribute__((unused)),
                      uchar *map,
                      size_t is_upper)
{
  char *srcend= src + srclen, *dst0= dst;

  DBUG_ASSERT(cs->mbmaxlen == 2);

  while (src < srcend)
  {
    size_t mblen= my_ismbchar(cs, src, srcend);
    if (mblen)
    {
      MY_UNICASE_INFO *ch;
      if ((ch= get_case_info_for_ch(cs, (uchar) src[0], (uchar) src[1])))
      {
        int code= is_upper ? ch->toupper : ch->tolower;
        src+= 2;
        if (code > 0xFF)
          *dst++= code >> 8;
        *dst++= code & 0xFF;
      }
      else
      {
        *dst++= *src++;
        *dst++= *src++;
      }
    }
    else
    {
      *dst++= (char) map[(uchar) *src++];
    }
  }
  return (size_t) (dst - dst0);
}


size_t
my_casedn_mb_varlen(CHARSET_INFO * cs, char *src, size_t srclen,
                    char *dst, size_t dstlen)
{
  DBUG_ASSERT(dstlen >= srclen * cs->casedn_multiply); 
  DBUG_ASSERT(src != dst || cs->casedn_multiply == 1);
  return my_casefold_mb_varlen(cs, src, srclen, dst, dstlen, cs->to_lower, 0);
}


size_t
my_caseup_mb_varlen(CHARSET_INFO * cs, char *src, size_t srclen,
                    char *dst, size_t dstlen)
{
  DBUG_ASSERT(dstlen >= srclen * cs->caseup_multiply);
  DBUG_ASSERT(src != dst || cs->caseup_multiply == 1);
  return my_casefold_mb_varlen(cs, src, srclen, dst, dstlen, cs->to_upper, 1);
}


/*
  my_strcasecmp_mb() returns 0 if strings are equal, non-zero otherwise.
 */

int my_strcasecmp_mb(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uint32 l;
  register uchar *map=cs->to_upper;
  
  while (*s && *t)
  {
    /* Pointing after the '\0' is safe here. */
    if ((l=my_ismbchar(cs, s, s + cs->mbmaxlen)))
    {
      while (l--)
        if (*s++ != *t++) 
          return 1;
    }
    else if (my_mbcharlen(cs, *t) > 1)
      return 1;
    else if (map[(uchar) *s++] != map[(uchar) *t++])
      return 1;
  }
  /* At least one of '*s' and '*t' is zero here. */
  return (*t != *s);
}


/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#define INC_PTR(cs,A,B) A+=(my_ismbchar(cs,A,B) ? my_ismbchar(cs,A,B) : 1)

#define likeconv(s,A) (uchar) (s)->sort_order[(uchar) (A)]

int my_wildcmp_mb(CHARSET_INFO *cs,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      int l;
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if ((l = my_ismbchar(cs, wildstr, wildend)))
      {
	  if (str+l > str_end || memcmp(str, wildstr, l) != 0)
	      return 1;
	  str += l;
	  wildstr += l;
      }
      else
      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return(1);				/* No match */
      if (wildstr == wildend)
	return (str != str_end);		/* Match if both are at end */
      result=1;					/* Found an anchor char */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return (result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
      uchar cmp;
      const char* mb = wildstr;
      int mb_len=0;
      
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	    return (-1);
	  INC_PTR(cs,str,str_end);
	  continue;
	}
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
	return(0);				/* Ok if w_many is last */
      if (str == str_end)
	return -1;
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
	
      mb=wildstr;
      mb_len= my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);   
      do
      {
        for (;;)
        {
          if (str >= str_end)
            return -1;
          if (mb_len)
          {
            if (str+mb_len <= str_end && memcmp(str, mb, mb_len) == 0)
            {
              str += mb_len;
              break;
            }
          }
          else if (!my_ismbchar(cs, str, str_end) &&
                   likeconv(cs,*str) == cmp)
          {
            str++;
            break;
          }
          INC_PTR(cs,str, str_end);
        }
	{
	  int tmp=my_wildcmp_mb(cs,str,str_end,wildstr,wildend,escape,w_one,
                                w_many);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}


size_t my_numchars_mb(CHARSET_INFO *cs __attribute__((unused)),
		      const char *pos, const char *end)
{
  register size_t count= 0;
  while (pos < end) 
  {
    uint mb_len;
    pos+= (mb_len= my_ismbchar(cs,pos,end)) ? mb_len : 1;
    count++;
  }
  return count;
}


size_t my_charpos_mb(CHARSET_INFO *cs __attribute__((unused)),
		     const char *pos, const char *end, size_t length)
{
  const char *start= pos;
  
  while (length && pos < end)
  {
    uint mb_len;
    pos+= (mb_len= my_ismbchar(cs, pos, end)) ? mb_len : 1;
    length--;
  }
  return (size_t) (length ? end+2-start : pos-start);
}


size_t my_well_formed_len_mb(CHARSET_INFO *cs, const char *b, const char *e,
                             size_t pos, int *error)
{
  const char *b_start= b;
  *error= 0;
  while (pos)
  {
    my_wc_t wc;
    int mb_len;

    if ((mb_len= cs->cset->mb_wc(cs, &wc, (uchar*) b, (uchar*) e)) <= 0)
    {
      *error= b < e ? 1 : 0;
      break;
    }
    b+= mb_len;
    pos--;
  }
  return (size_t) (b - b_start);
}


uint my_instr_mb(CHARSET_INFO *cs,
                 const char *b, size_t b_length, 
                 const char *s, size_t s_length,
                 my_match_t *match, uint nmatch)
{
  register const char *end, *b0;
  int res= 0;
  
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
    
    b0= b;
    end= b+b_length-s_length+1;
    
    while (b < end)
    {
      int mb_len;
      
      if (!cs->coll->strnncoll(cs, (uchar*) b,   s_length, 
      				   (uchar*) s, s_length, 0))
      {
        if (nmatch)
        {
          match[0].beg= 0;
          match[0].end= (size_t) (b-b0);
          match[0].mb_len= res;
          if (nmatch > 1)
          {
            match[1].beg= match[0].end;
            match[1].end= match[0].end+s_length;
            match[1].mb_len= 0;	/* Not computed */
          }
        }
        return 2;
      }
      mb_len= (mb_len= my_ismbchar(cs, b, end)) ? mb_len : 1;
      b+= mb_len;
      b_length-= mb_len;
      res++;
    }
  }
  return 0;
}


/* BINARY collations handlers for MB charsets */

int
my_strnncoll_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
                    const uchar *s, size_t slen,
                    const uchar *t, size_t tlen,
                    my_bool t_is_prefix)
{
  size_t len=min(slen,tlen);
  int cmp= memcmp(s,t,len);
  return cmp ? cmp : (int) ((t_is_prefix ? len : slen) - tlen);
}


/*
  Compare two strings. 
  
  SYNOPSIS
    my_strnncollsp_mb_bin()
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
    A negative number if s < t
    A positive number if s > t
    0 if strings are equal
*/

int
my_strnncollsp_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
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
    if (diff_if_only_endspace_difference)
      res= 1;                                   /* Assume 'a' is bigger */
    /*
      Check the next not space character of the longer key. If it's < ' ',
      then it's smaller than the other key.
    */
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


static size_t my_strnxfrm_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
                                 uchar *dest, size_t dstlen,
                                 const uchar *src, size_t srclen)
{
  if (dest != src)
    memcpy(dest, src, min(dstlen, srclen));
  if (dstlen > srclen)
    bfill(dest + srclen, dstlen - srclen, ' ');
  return dstlen;
}


int
my_strcasecmp_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
                     const char *s, const char *t)
{
  return strcmp(s,t);
}


void
my_hash_sort_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
                    const uchar *key, size_t len,ulong *nr1, ulong *nr2)
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


/* 
  Fill the given buffer with 'maximum character' for given charset
  SYNOPSIS
      pad_max_char()
      cs   Character set
      str  Start of buffer to fill
      end  End of buffer to fill

  DESCRIPTION
      Write max key:
      - for non-Unicode character sets:
        just set to 255.
      - for Unicode character set (utf-8):
        create a buffer with multibyte representation of the max_sort_char
        character, and copy it into max_str in a loop. 
*/
static void pad_max_char(CHARSET_INFO *cs, char *str, char *end)
{
  char buf[10];
  char buflen;
  
  if (!(cs->state & MY_CS_UNICODE))
  {
    bfill(str, end - str, 255);
    return;
  }
  
  buflen= cs->cset->wc_mb(cs, cs->max_sort_char, (uchar*) buf,
                          (uchar*) buf + sizeof(buf));
  
  DBUG_ASSERT(buflen > 0);
  do
  {
    if ((str + buflen) < end)
    {
      /* Enough space for the characer */
      memcpy(str, buf, buflen);
      str+= buflen;
    }
    else
    {
      /* 
        There is no space for whole multibyte
        character, then add trailing spaces.
      */  
      *str++= ' ';
    }
  } while (str < end);
}

/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from min_str and max_str
** res_length	Length of min_str and max_str.
** min_str	Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

my_bool my_like_range_mb(CHARSET_INFO *cs,
			 const char *ptr,size_t ptr_length,
			 pbool escape, pbool w_one, pbool w_many,
			 size_t res_length,
			 char *min_str,char *max_str,
			 size_t *min_length,size_t *max_length)
{
  uint mb_len;
  const char *end= ptr + ptr_length;
  char *min_org= min_str;
  char *min_end= min_str + res_length;
  char *max_end= max_str + res_length;
  size_t maxcharlen= res_length / cs->mbmaxlen;
  const char *contraction_flags= cs->contractions ? 
              ((const char*) cs->contractions) + 0x40*0x40 : NULL;

  for (; ptr != end && min_str != min_end && maxcharlen ; maxcharlen--)
  {
    /* We assume here that escape, w_any, w_namy are one-byte characters */
    if (*ptr == escape && ptr+1 != end)
      ptr++;                                    /* Skip escape */
    else if (*ptr == w_one || *ptr == w_many)   /* '_' and '%' in SQL */
    {      
fill_max_and_min:
      /*
        Calculate length of keys:
        'a\0\0... is the smallest possible string when we have space expand
        a\ff\ff... is the biggest possible string
      */
      *min_length= ((cs->state & MY_CS_BINSORT) ? (size_t) (min_str - min_org) :
                    res_length);
      *max_length= res_length;
      /* Create min key  */
      do
      {
	*min_str++= (char) cs->min_sort_char;
      } while (min_str != min_end);
      
      /* 
        Write max key: create a buffer with multibyte
        representation of the max_sort_char character,
        and copy it into max_str in a loop. 
      */
      *max_length= res_length;
      pad_max_char(cs, max_str, max_end);
      return 0;
    }
    if ((mb_len= my_ismbchar(cs, ptr, end)) > 1)
    {
      if (ptr+mb_len > end || min_str+mb_len > min_end)
        break;
      while (mb_len--)
       *min_str++= *max_str++= *ptr++;
    }
    else
    {
      /*
        Special case for collations with contractions.
        For example, in Chezh, 'ch' is a separate letter
        which is sorted between 'h' and 'i'.
        If the pattern 'abc%', 'c' at the end can mean:
        - letter 'c' itself,
        - beginning of the contraction 'ch'.

        If we simply return this LIKE range:

         'abc\min\min\min' and 'abc\max\max\max'

        then this query: SELECT * FROM t1 WHERE a LIKE 'abc%'
        will only find values starting from 'abc[^h]',
        but won't find values starting from 'abch'.

        We must ignore contraction heads followed by w_one or w_many.
        ('Contraction head' means any letter which can be the first
        letter in a contraction)

        For example, for Czech 'abc%', we will return LIKE range,
        which is equal to LIKE range for 'ab%':

        'ab\min\min\min\min' and 'ab\max\max\max\max'.

      */
      if (contraction_flags && ptr + 1 < end &&
          contraction_flags[(uchar) *ptr])
      {
        /* Ptr[0] is a contraction head. */
        
        if (ptr[1] == w_one || ptr[1] == w_many)
        {
          /* Contraction head followed by a wildcard, quit. */
          goto fill_max_and_min;
        }
        
        /*
          Some letters can be both contraction heads and contraction tails.
          For example, in Danish 'aa' is a separate single letter which
          is sorted after 'z'. So 'a' can be both head and tail.
          
          If ptr[0]+ptr[1] is a contraction,
          then put both letters together.
          
          If ptr[1] can be a contraction part, but ptr[0]+ptr[1]
          is not a contraction, then we put only ptr[0],
          and continue with ptr[1] on the next loop.
        */
        if (contraction_flags[(uchar) ptr[1]] &&
            cs->contractions[(*ptr-0x40)*0x40 + ptr[1] - 0x40])
        {
          /* Contraction found */
          if (maxcharlen == 1 || min_str + 1 >= min_end)
          {
            /* Both contraction parts don't fit, quit */
            goto fill_max_and_min;
          }
          
          /* Put contraction head */
          *min_str++= *max_str++= *ptr++;
          maxcharlen--;
        }
      }
      /* Put contraction tail, or a single character */
      *min_str++= *max_str++= *ptr++;    
    }
  }

  *min_length= *max_length = (size_t) (min_str - min_org);
  while (min_str != min_end)
    *min_str++= *max_str++= ' ';           /* Because if key compression */
  return 0;
}


int
my_wildcmp_mb_bin(CHARSET_INFO *cs,
                  const char *str,const char *str_end,
                  const char *wildstr,const char *wildend,
                  int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      int l;
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if ((l = my_ismbchar(cs, wildstr, wildend)))
      {
	  if (str+l > str_end || memcmp(str, wildstr, l) != 0)
	      return 1;
	  str += l;
	  wildstr += l;
      }
      else
      if (str == str_end || *wildstr++ != *str++)
	return(1);				/* No match */
      if (wildstr == wildend)
	return (str != str_end);		/* Match if both are at end */
      result=1;					/* Found an anchor char */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return (result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
      uchar cmp;
      const char* mb = wildstr;
      int mb_len=0;
      
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for (; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == w_many)
	  continue;
	if (*wildstr == w_one)
	{
	  if (str == str_end)
	    return (-1);
	  INC_PTR(cs,str,str_end);
	  continue;
	}
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
	return(0);				/* Ok if w_many is last */
      if (str == str_end)
	return -1;
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
	
      mb=wildstr;
      mb_len= my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      do
      {
        for (;;)
        {
          if (str >= str_end)
            return -1;
          if (mb_len)
          {
            if (str+mb_len <= str_end && memcmp(str, mb, mb_len) == 0)
            {
              str += mb_len;
              break;
            }
          }
          else if (!my_ismbchar(cs, str, str_end) && *str == cmp)
          {
            str++;
            break;
          }
          INC_PTR(cs,str, str_end);
        }
	{
	  int tmp=my_wildcmp_mb_bin(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}


/*
  Data was produced from EastAsianWidth.txt 
  using utt11-dump utility.
*/
static char pg11[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pg23[256]=
{
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pg2E[256]=
{
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pg2F[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0
};

static char pg30[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

static char pg31[256]=
{
0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

static char pg32[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0
};

static char pg4D[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pg9F[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pgA4[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pgD7[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pgFA[256]=
{
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pgFE[256]=
{
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char pgFF[256]=
{
0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static struct {int page; char *p;} utr11_data[256]=
{
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,pg11},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,pg23},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,pg2E},{0,pg2F},
{0,pg30},{0,pg31},{0,pg32},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{0,pg4D},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{0,pg9F},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{0,pgA4},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},
{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{1,NULL},{0,pgD7},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},{0,NULL},
{0,NULL},{1,NULL},{0,pgFA},{0,NULL},{0,NULL},{0,NULL},{0,pgFE},{0,pgFF}
};


size_t my_numcells_mb(CHARSET_INFO *cs, const char *b, const char *e)
{
  my_wc_t wc;
  size_t clen= 0;
  
  while (b < e)
  {
    int mb_len;
    uint pg;
    if ((mb_len= cs->cset->mb_wc(cs, &wc, (uchar*) b, (uchar*) e)) <= 0)
    {
      mb_len= 1; /* Let's think a wrong sequence takes 1 dysplay cell */
      b++;
      continue;
    }
    b+= mb_len;
    if (wc > 0xFFFF)
    {
      if (wc >= 0x20000 && wc <= 0x3FFFD) /* CJK Ideograph Extension B, C */
        clen+= 1;
    }
    else
    {
      pg= (wc >> 8) & 0xFF;
      clen+= utr11_data[pg].p ? utr11_data[pg].p[wc & 0xFF] : utr11_data[pg].page;
    }
    clen++;
  }
  return clen;
}


int my_mb_ctype_mb(CHARSET_INFO *cs, int *ctype,
                   const uchar *s, const uchar *e)
{
  my_wc_t wc;
  int res= cs->cset->mb_wc(cs, &wc, s, e);
  if (res <= 0 || wc > 0xFFFF)
    *ctype= 0;
  else
    *ctype= my_uni_ctype[wc>>8].ctype ?
            my_uni_ctype[wc>>8].ctype[wc&0xFF] :
            my_uni_ctype[wc>>8].pctype;    
  return res;
}


MY_COLLATION_HANDLER my_collation_mb_bin_handler =
{
    NULL,		/* init */
    my_strnncoll_mb_bin,
    my_strnncollsp_mb_bin,
    my_strnxfrm_mb_bin,
    my_strnxfrmlen_simple,
    my_like_range_mb,
    my_wildcmp_mb_bin,
    my_strcasecmp_mb_bin,
    my_instr_mb,
    my_hash_sort_mb_bin,
    my_propagate_simple
};


#endif
