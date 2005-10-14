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
#include "m_string.h"
#include "m_ctype.h"
#include <errno.h>

#include "stdarg.h"

/*
  Returns the number of bytes required for strnxfrm().
*/
uint my_strnxfrmlen_simple(CHARSET_INFO *cs, uint len)
{
  return len * (cs->strxfrm_multiply ? cs->strxfrm_multiply : 1);
}


/*
  Converts a string into its sort key.
  
  SYNOPSIS
     my_strnxfrm_xxx()
     
  IMPLEMENTATION
     
     The my_strxfrm_xxx() function transforms a string pointed to by
     'src' with length 'srclen' according to the charset+collation 
     pair 'cs' and copies the result key into 'dest'.
     
     Comparing two strings using memcmp() after my_strnxfrm_xxx()
     is equal to comparing two original strings with my_strnncollsp_xxx().
     
     Not more than 'dstlen' bytes are written into 'dst'.
     To garantee that the whole string is transformed, 'dstlen' must be
     at least srclen*cs->strnxfrm_multiply bytes long. Otherwise,
     consequent memcmp() may return a non-accurate result.
     
     If the source string is too short to fill whole 'dstlen' bytes,
     then the 'dest' string is padded up to 'dstlen', ensuring that:
     
       "a"  == "a "
       "a\0" < "a"
       "a\0" < "a "
     
     my_strnxfrm_simple() is implemented for 8bit charsets and
     simple collations with one-to-one string->key transformation.
     
     See also implementations for various charsets/collations in  
     other ctype-xxx.c files.
     
  RETURN
  
    Target len 'dstlen'.
  
*/


int my_strnxfrm_simple(CHARSET_INFO * cs, 
                       uchar *dest, uint len,
                       const uchar *src, uint srclen)
{
  uchar *map= cs->sort_order;
  uint dstlen= len;
  set_if_smaller(len, srclen);
  if (dest != src)
  {
    const uchar *end;
    for ( end=src+len; src < end ;  )
      *dest++= map[*src++];
  }
  else
  {
    const uchar *end;
    for ( end=dest+len; dest < end ; dest++)
      *dest= (char) map[(uchar) *dest];
  }
  if (dstlen > len)
    bfill(dest, dstlen - len, ' ');
  return dstlen;
}

int my_strnncoll_simple(CHARSET_INFO * cs, const uchar *s, uint slen, 
                        const uchar *t, uint tlen,
                        my_bool t_is_prefix)
{
  int len = ( slen > tlen ) ? tlen : slen;
  uchar *map= cs->sort_order;
  if (t_is_prefix && slen > tlen)
    slen=tlen;
  while (len--)
  {
    if (map[*s++] != map[*t++])
      return ((int) map[s[-1]] - (int) map[t[-1]]);
  }
  return (int) (slen - tlen);
}


/*
  Compare strings, discarding end space

  SYNOPSIS
    my_strnncollsp_simple()
    cs			character set handler
    a			First string to compare
    a_length		Length of 'a'
    b			Second string to compare
    b_length		Length of 'b'
    diff_if_only_endspace_difference
		        Set to 1 if the strings should be regarded as different
                        if they only difference in end space

  IMPLEMENTATION
    If one string is shorter as the other, then we space extend the other
    so that the strings have equal length.

    This will ensure that the following things hold:

    "a"  == "a "
    "a\0" < "a"
    "a\0" < "a "

  RETURN
    < 0	 a <  b
    = 0	 a == b
    > 0	 a > b
*/

int my_strnncollsp_simple(CHARSET_INFO * cs, const uchar *a, uint a_length, 
			  const uchar *b, uint b_length,
                          my_bool diff_if_only_endspace_difference)
{
  const uchar *map= cs->sort_order, *end;
  uint length;
  int res;

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
  diff_if_only_endspace_difference= 0;
#endif

  end= a + (length= min(a_length, b_length));
  while (a < end)
  {
    if (map[*a++] != map[*b++])
      return ((int) map[a[-1]] - (int) map[b[-1]]);
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
      swap= -1;                                 /* swap sign of result */
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

uint my_caseup_8bit(CHARSET_INFO * cs, char *src, uint srclen,
                    char *dst __attribute__((unused)),
                    uint dstlen __attribute__((unused)))
{
  uint srclen0= srclen;
  register uchar *map= cs->to_upper;
  DBUG_ASSERT(src == dst && srclen == dstlen);
  for ( ; srclen > 0 ; srclen--, src++)
    *src= (char) map[(uchar) *src];
  return srclen0;
}

uint my_casedn_8bit(CHARSET_INFO * cs, char *src, uint srclen,
                    char *dst __attribute__((unused)),
                    uint dstlen __attribute__((unused)))
{
  uint srclen0= srclen;
  register uchar *map=cs->to_lower;
  DBUG_ASSERT(src == dst && srclen == dstlen);
  for ( ; srclen > 0 ; srclen--, src++)
    *src= (char) map[(uchar) *src];
  return srclen0;
}

int my_strcasecmp_8bit(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uchar *map=cs->to_upper;
  while (map[(uchar) *s] == map[(uchar) *t++])
    if (!*s++) return 0;
  return ((int) map[(uchar) s[0]] - (int) map[(uchar) t[-1]]);
}


int my_mb_wc_8bit(CHARSET_INFO *cs,my_wc_t *wc,
		  const unsigned char *str,
		  const unsigned char *end __attribute__((unused)))
{
  if (str >= end)
    return MY_CS_TOOFEW(0);
  
  *wc=cs->tab_to_uni[*str];
  return (!wc[0] && str[0]) ? MY_CS_ILSEQ : 1;
}

int my_wc_mb_8bit(CHARSET_INFO *cs,my_wc_t wc,
		  unsigned char *str,
		  unsigned char *end __attribute__((unused)))
{
  MY_UNI_IDX *idx;

  if (str >= end)
    return MY_CS_TOOSMALL;
  
  for (idx=cs->tab_from_uni; idx->tab ; idx++)
  {
    if (idx->from <= wc && idx->to >= wc)
    {
      str[0]= idx->tab[wc - idx->from];
      return (!str[0] && wc) ? MY_CS_ILUNI : 1;
    }
  }
  return MY_CS_ILUNI;
}


/* 
   We can't use vsprintf here as it's not guaranteed to return
   the length on all operating systems.
   This function is also not called in a safe environment, so the
   end buffer must be checked.
*/

int my_snprintf_8bit(CHARSET_INFO *cs  __attribute__((unused)),
		     char* to, uint n  __attribute__((unused)),
		     const char* fmt, ...)
{
  va_list args;
  int result;
  va_start(args,fmt);
  result= my_vsnprintf(to, n, fmt, args);
  va_end(args);
  return result;
}


void my_hash_sort_simple(CHARSET_INFO *cs,
			 const uchar *key, uint len,
			 ulong *nr1, ulong *nr2)
{
  register uchar *sort_order=cs->sort_order;
  const uchar *end= key + len;
  
  /*
    Remove end space. We have to do this to be able to compare
    'A ' and 'A' as identical
  */
  while (end > key && end[-1] == ' ')
    end--;
  
  for (; key < (uchar*) end ; key++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint) sort_order[(uint) *key])) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}


long my_strntol_8bit(CHARSET_INFO *cs,
		     const char *nptr, uint l, int base,
		     char **endptr, int *err)
{
  int negative;
  register uint32 cutoff;
  register unsigned int cutlim;
  register uint32 i;
  register const char *s;
  register unsigned char c;
  const char *save, *e;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;
  
  for ( ; s<e && my_isspace(cs, *s) ; s++);
  
  if (s == e)
  {
    goto noconv;
  }
  
  /* Check for a sign.	*/
  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;
  cutoff = ((uint32)~0L) / (uint32) base;
  cutlim = (uint) (((uint32)~0L) % (uint32) base);

  overflow = 0;
  i = 0;
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint32) base;
      i += c;
    }
  }
  
  if (s == save)
    goto noconv;
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (negative)
  {
    if (i  > (uint32) INT_MIN32)
      overflow = 1;
  }
  else if (i > INT_MAX32)
    overflow = 1;
  
  if (overflow)
  {
    err[0]= ERANGE;
    return negative ? INT_MIN32 : INT_MAX32;
  }
  
  return (negative ? -((long) i) : (long) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


ulong my_strntoul_8bit(CHARSET_INFO *cs,
		       const char *nptr, uint l, int base,
		       char **endptr, int *err)
{
  int negative;
  register uint32 cutoff;
  register unsigned int cutlim;
  register uint32 i;
  register const char *s;
  register unsigned char c;
  const char *save, *e;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;
  
  for( ; s<e && my_isspace(cs, *s); s++);
  
  if (s==e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;
  cutoff = ((uint32)~0L) / (uint32) base;
  cutlim = (uint) (((uint32)~0L) % (uint32) base);
  overflow = 0;
  i = 0;
  
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (uint32) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (overflow)
  {
    err[0]= ERANGE;
    return (~(uint32) 0);
  }
  
  return (negative ? -((long) i) : (long) i);
  
noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


longlong my_strntoll_8bit(CHARSET_INFO *cs __attribute__((unused)),
			  const char *nptr, uint l, int base,
			  char **endptr,int *err)
{
  int negative;
  register ulonglong cutoff;
  register unsigned int cutlim;
  register ulonglong i;
  register const char *s, *e;
  const char *save;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for(; s<e && my_isspace(cs,*s); s++);

  if (s == e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X'|| s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;

  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for ( ; s != e; s++)
  {
    register unsigned char c= *s;
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (ulonglong) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (negative)
  {
    if (i  > (ulonglong) LONGLONG_MIN)
      overflow = 1;
  }
  else if (i > (ulonglong) LONGLONG_MAX)
    overflow = 1;

  if (overflow)
  {
    err[0]= ERANGE;
    return negative ? LONGLONG_MIN : LONGLONG_MAX;
  }

  return (negative ? -((longlong) i) : (longlong) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


ulonglong my_strntoull_8bit(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
			   char **endptr, int *err)
{
  int negative;
  register ulonglong cutoff;
  register unsigned int cutlim;
  register ulonglong i;
  register const char *s, *e;
  const char *save;
  int overflow;

  *err= 0;				/* Initialize error indicator */
#ifdef NOT_USED
  if (base < 0 || base == 1 || base > 36)
    base = 10;
#endif

  s = nptr;
  e = nptr+l;

  for(; s<e && my_isspace(cs,*s); s++);

  if (s == e)
  {
    goto noconv;
  }

  if (*s == '-')
  {
    negative = 1;
    ++s;
  }
  else if (*s == '+')
  {
    negative = 0;
    ++s;
  }
  else
    negative = 0;

#ifdef NOT_USED
  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;
#endif

#ifdef NOT_USED
  if (base == 0)
  {
    if (*s == '0')
    {
      if (s[1]=='X' || s[1]=='x')
      {
	s += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
#endif

  save = s;

  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for ( ; s != e; s++)
  {
    register unsigned char c= *s;

    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='Z')
      c = c - 'A' + 10;
    else if (c>='a' && c<='z')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (ulonglong) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (overflow)
  {
    err[0]= ERANGE;
    return (~(ulonglong) 0);
  }

  return (negative ? -((longlong) i) : (longlong) i);

noconv:
  err[0]= EDOM;
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}

/*
  Read double from string

  SYNOPSIS:
    my_strntod_8bit()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.
    end		result pointer to end of converted string
    err		Error number if failed conversion
    
  NOTES:
    If length is not INT_MAX32 or str[length] != 0 then the given str must
    be writeable
    If length == INT_MAX32 the str must be \0 terminated.

    It's implemented this way to save a buffer allocation and a memory copy.

  RETURN
    Value of number in string
*/


double my_strntod_8bit(CHARSET_INFO *cs __attribute__((unused)),
		       char *str, uint length,
		       char **end, int *err)
{
  if (length == INT_MAX32)
    length= 65535;                          /* Should be big enough */
  *end= str + length;
  return my_strtod(str, end, err);
}


/*
  This is a fast version optimized for the case of radix 10 / -10

  Assume len >= 1
*/

int my_long10_to_str_8bit(CHARSET_INFO *cs __attribute__((unused)),
		     char *dst, uint len, int radix, long int val)
{
  char buffer[66];
  register char *p, *e;
  long int new_val;
  uint sign=0;

  e = p = &buffer[sizeof(buffer)-1];
  *p= 0;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      val= -val;
      *dst++= '-';
      len--;
      sign= 1;
    }
  }
  
  new_val = (long) ((unsigned long int) val / 10);
  *--p    = '0'+ (char) ((unsigned long int) val - (unsigned long) new_val * 10);
  val     = new_val;
  
  while (val != 0)
  {
    new_val=val/10;
    *--p = '0' + (char) (val-new_val*10);
    val= new_val;
  }
  
  len= min(len, (uint) (e-p));
  memcpy(dst, p, len);
  return (int) len+sign;
}


int my_longlong10_to_str_8bit(CHARSET_INFO *cs __attribute__((unused)),
		      char *dst, uint len, int radix, longlong val)
{
  char buffer[65];
  register char *p, *e;
  long long_val;
  uint sign= 0;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      val = -val;
      *dst++= '-';
      len--;
      sign= 1;
    }
  }
  
  e = p = &buffer[sizeof(buffer)-1];
  *p= 0;
  
  if (val == 0)
  {
    *--p= '0';
    len= 1;
    goto cnv;
  }
  
  while ((ulonglong) val > (ulonglong) LONG_MAX)
  {
    ulonglong quo=(ulonglong) val/(uint) 10;
    uint rem= (uint) (val- quo* (uint) 10);
    *--p = '0' + rem;
    val= quo;
  }
  
  long_val= (long) val;
  while (long_val != 0)
  {
    long quo= long_val/10;
    *--p = '0' + (char)(long_val - quo*10);
    long_val= quo;
  }
  
  len= min(len, (uint) (e-p));
cnv:
  memcpy(dst, p, len);
  return len+sign;
}


/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#ifdef LIKE_CMP_TOUPPER
#define likeconv(s,A) (uchar) my_toupper(s,A)
#else
#define likeconv(s,A) (uchar) (s)->sort_order[(uchar) (A)]
#endif

#define INC_PTR(cs,A,B) (A)++


int my_wildcmp_8bit(CHARSET_INFO *cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  int result= -1;			/* Not found, using wildcards */

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;

      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return(1);				/* No match */
      if (wildstr == wildend)
	return(str != str_end);		/* Match if both are at end */
      result=1;					/* Found an anchor char     */
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			/* Skip one char if possible */
	  return(result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						/* Found w_many */
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
	break;					/* Not a wild character */
      }
      if (wildstr == wildend)
	return(0);				/* Ok if w_many is last */
      if (str == str_end)
	return(-1);
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;

      INC_PTR(cs,wildstr,wildend);	/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);
      do
      {
	while (str != str_end && (uchar) likeconv(cs,*str) != cmp)
	  str++;
	if (str++ == str_end) return(-1);
	{
	  int tmp=my_wildcmp_8bit(cs,str,str_end,wildstr,wildend,escape,w_one,
				  w_many);
	  if (tmp <= 0)
	    return(tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return(str != str_end ? 1 : 0);
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

my_bool my_like_range_simple(CHARSET_INFO *cs,
			     const char *ptr,uint ptr_length,
			     pbool escape, pbool w_one, pbool w_many,
			     uint res_length,
			     char *min_str,char *max_str,
			     uint *min_length,uint *max_length)
{
  const char *end= ptr + ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;
  uint charlen= res_length / cs->mbmaxlen;

  for (; ptr != end && min_str != min_end && charlen > 0 ; ptr++, charlen--)
  {
    if (*ptr == escape && ptr+1 != end)
    {
      ptr++;					/* Skip escape */
      *min_str++= *max_str++ = *ptr;
      continue;
    }
    if (*ptr == w_one)				/* '_' in SQL */
    {
      *min_str++='\0';				/* This should be min char */
      *max_str++= (char) cs->max_sort_char;
      continue;
    }
    if (*ptr == w_many)				/* '%' in SQL */
    {
      /* Calculate length of keys */
      *min_length= ((cs->state & MY_CS_BINSORT) ? (uint) (min_str - min_org) :
                    res_length);
      *max_length= res_length;
      do
      {
	*min_str++= 0;
	*max_str++= (char) cs->max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++= *max_str++ = *ptr;
  }

 *min_length= *max_length = (uint) (min_str - min_org);
  while (min_str != min_end)
    *min_str++= *max_str++ = ' ';      /* Because if key compression */
  return 0;
}


ulong my_scan_8bit(CHARSET_INFO *cs, const char *str, const char *end, int sq)
{
  const char *str0= str;
  switch (sq)
  {
  case MY_SEQ_INTTAIL:
    if (*str == '.')
    {
      for(str++ ; str != end && *str == '0' ; str++);
      return (ulong) (str - str0);
    }
    return 0;

  case MY_SEQ_SPACES:
    for ( ; str < end ; str++)
    {
      if (!my_isspace(cs,*str))
        break;
    }
    return (ulong) (str - str0);
  default:
    return 0;
  }
}


void my_fill_8bit(CHARSET_INFO *cs __attribute__((unused)),
		   char *s, uint l, int fill)
{
  bfill(s,l,fill);
}


uint my_numchars_8bit(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  return (uint) (e - b);
}


uint my_numcells_8bit(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  return (uint) (e - b);
}


uint my_charpos_8bit(CHARSET_INFO *cs __attribute__((unused)),
		     const char *b  __attribute__((unused)),
		     const char *e  __attribute__((unused)),
		     uint pos)
{
  return pos;
}


uint my_well_formed_len_8bit(CHARSET_INFO *cs __attribute__((unused)),
                             const char *start, const char *end,
                             uint nchars, int *error)
{
  uint nbytes= (uint) (end-start);
  *error= 0;
  return min(nbytes, nchars);
}


uint my_lengthsp_8bit(CHARSET_INFO *cs __attribute__((unused)),
		      const char *ptr, uint length)
{
  const char *end= ptr+length;
  while (end > ptr && end[-1] == ' ')
    end--;
  return (uint) (end-ptr);
}


uint my_instr_simple(CHARSET_INFO *cs,
                    const char *b, uint b_length, 
		    const char *s, uint s_length,
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
        match->mblen= 0;
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
      if (cs->sort_order[*str++] == cs->sort_order[*search])
      {
	register const uchar *i,*j;
	
	i= str; 
	j= search+1;
	
	while (j != search_end)
	  if (cs->sort_order[*i++] != cs->sort_order[*j++]) 
            goto skip;
        
	if (nmatch > 0)
	{
	  match[0].beg= 0;
	  match[0].end= (uint) (str- (const uchar*)b-1);
	  match[0].mblen= match[0].end;
	  
	  if (nmatch > 1)
	  {
	    match[1].beg= match[0].end;
	    match[1].end= match[0].end+s_length;
	    match[1].mblen= match[1].end-match[1].beg;
	  }
	}
	return 2;
      }
    }
  }
  return 0;
}


typedef struct
{
  int		nchars;
  MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
#define PLANE_NUMBER(x)	(((x)>>8) % PLANE_NUM)

static int pcmp(const void * f, const void * s)
{
  const uni_idx *F= (const uni_idx*) f;
  const uni_idx *S= (const uni_idx*) s;
  int res;

  if (!(res=((S->nchars)-(F->nchars))))
    res=((F->uidx.from)-(S->uidx.to));
  return res;
}

static my_bool create_fromuni(CHARSET_INFO *cs, void *(*alloc)(uint))
{
  uni_idx	idx[PLANE_NUM];
  int		i,n;
  
  /*
    Check that Unicode map is loaded.
    It can be not loaded when the collation is
    listed in Index.xml but not specified
    in the character set specific XML file.
  */
  if (!cs->tab_to_uni)
    return TRUE;
  
  /* Clear plane statistics */
  bzero(idx,sizeof(idx));
  
  /* Count number of characters in each plane */
  for (i=0; i< 0x100; i++)
  {
    uint16 wc=cs->tab_to_uni[i];
    int pl= PLANE_NUMBER(wc);
    
    if (wc || !i)
    {
      if (!idx[pl].nchars)
      {
        idx[pl].uidx.from=wc;
        idx[pl].uidx.to=wc;
      }else
      {
        idx[pl].uidx.from=wc<idx[pl].uidx.from?wc:idx[pl].uidx.from;
        idx[pl].uidx.to=wc>idx[pl].uidx.to?wc:idx[pl].uidx.to;
      }
      idx[pl].nchars++;
    }
  }
  
  /* Sort planes in descending order */
  qsort(&idx,PLANE_NUM,sizeof(uni_idx),&pcmp);
  
  for (i=0; i < PLANE_NUM; i++)
  {
    int ch,numchars;
    
    /* Skip empty plane */
    if (!idx[i].nchars)
      break;
    
    numchars=idx[i].uidx.to-idx[i].uidx.from+1;
    if (!(idx[i].uidx.tab=(uchar*) alloc(numchars * sizeof(*idx[i].uidx.tab))))
      return TRUE;
    
    bzero(idx[i].uidx.tab,numchars*sizeof(*idx[i].uidx.tab));
    
    for (ch=1; ch < PLANE_SIZE; ch++)
    {
      uint16 wc=cs->tab_to_uni[ch];
      if (wc >= idx[i].uidx.from && wc <= idx[i].uidx.to && wc)
      {
        int ofs= wc - idx[i].uidx.from;
        idx[i].uidx.tab[ofs]= ch;
      }
    }
  }
  
  /* Allocate and fill reverse table for each plane */
  n=i;
  if (!(cs->tab_from_uni= (MY_UNI_IDX*) alloc(sizeof(MY_UNI_IDX)*(n+1))))
    return TRUE;

  for (i=0; i< n; i++)
    cs->tab_from_uni[i]= idx[i].uidx;
  
  /* Set end-of-list marker */
  bzero(&cs->tab_from_uni[i],sizeof(MY_UNI_IDX));
  return FALSE;
}

static my_bool my_cset_init_8bit(CHARSET_INFO *cs, void *(*alloc)(uint))
{
  cs->caseup_multiply= 1;
  cs->casedn_multiply= 1;
  cs->pad_char= ' ';
  return create_fromuni(cs, alloc);
}

static void set_max_sort_char(CHARSET_INFO *cs)
{
  uchar max_char;
  uint  i;
  
  if (!cs->sort_order)
    return;
  
  max_char=cs->sort_order[(uchar) cs->max_sort_char];
  for (i= 0; i < 256; i++)
  {
    if ((uchar) cs->sort_order[i] > max_char)
    {
      max_char=(uchar) cs->sort_order[i];
      cs->max_sort_char= i;
    }
  }
}

static my_bool my_coll_init_simple(CHARSET_INFO *cs,
                                   void *(*alloc)(uint) __attribute__((unused)))
{
  set_max_sort_char(cs);
  return FALSE;
}


longlong my_strtoll10_8bit(CHARSET_INFO *cs __attribute__((unused)),
                           const char *nptr, char **endptr, int *error)
{
  return my_strtoll10(nptr, endptr, error);
}


/*
  Check if a constant can be propagated

  SYNOPSIS:
    my_propagate_simple()
    cs		Character set information
    str		String to convert to double
    length	Optional length for string.
    
  NOTES:
   Takes the string in the given charset and check
   if it can be safely propagated in the optimizer.
   
   create table t1 (
     s char(5) character set latin1 collate latin1_german2_ci);
   insert into t1 values (0xf6); -- o-umlaut
   select * from t1 where length(s)=1 and s='oe';

   The above query should return one row.
   We cannot convert this query into:
   select * from t1 where length('oe')=1 and s='oe';
   
   Currently we don't check the constant itself,
   and decide not to propagate a constant
   just if the collation itself allows tricky things
   like expansions and contractions. In the future
   we can write a more sophisticated functions to
   check the constants. For example, 'oa' can always
   be safety propagated in German2 because unlike 
   'oe' it does not have any special meaning.

  RETURN
    1 if constant can be safely propagated
    0 if it is not safe to propagate the constant
*/



my_bool my_propagate_simple(CHARSET_INFO *cs __attribute__((unused)),
                            const uchar *str __attribute__((unused)),
                            uint length __attribute__((unused)))
{
  return 1;
}


my_bool my_propagate_complex(CHARSET_INFO *cs __attribute__((unused)),
                             const uchar *str __attribute__((unused)),
                             uint length __attribute__((unused)))
{
  return 0;
}


MY_CHARSET_HANDLER my_charset_8bit_handler=
{
    my_cset_init_8bit,
    NULL,			/* ismbchar      */
    my_mbcharlen_8bit,		/* mbcharlen     */
    my_numchars_8bit,
    my_charpos_8bit,
    my_well_formed_len_8bit,
    my_lengthsp_8bit,
    my_numcells_8bit,
    my_mb_wc_8bit,
    my_wc_mb_8bit,
    my_caseup_str_8bit,
    my_casedn_str_8bit,
    my_caseup_8bit,
    my_casedn_8bit,
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
    my_scan_8bit
};

MY_COLLATION_HANDLER my_collation_8bit_simple_ci_handler =
{
    my_coll_init_simple,	/* init */
    my_strnncoll_simple,
    my_strnncollsp_simple,
    my_strnxfrm_simple,
    my_strnxfrmlen_simple,
    my_like_range_simple,
    my_wildcmp_8bit,
    my_strcasecmp_8bit,
    my_instr_simple,
    my_hash_sort_simple,
    my_propagate_simple
};
