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
#include "my_sys.h"			/* defines errno */
#include <errno.h>

#include "stdarg.h"
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


#ifdef NOT_USED
static int my_vsnprintf_8bit(char *to, size_t n, const char* fmt, va_list ap)
{
  char *start=to, *end=to+n-1;
  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (to == end)			/* End of buffer */
	break;
      *to++= *fmt;			/* Copy ordinary char */
      continue;
    }
    /* Skip if max size is used (to be compatible with printf) */
    fmt++;
    while (my_isdigit(system_charset_info,*fmt) || *fmt == '.' || *fmt == '-')
      fmt++;
    if (*fmt == 'l')
      fmt++;
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      uint plen,left_len = (uint)(end-to);
      if (!par) par = (char*)"(null)";
      plen = (uint) strlen(par);
      if (left_len <= plen)
	plen = left_len - 1;
      to=strnmov(to,par,plen);
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      if ((uint) (end-to) < 16)
	break;
      iarg = va_arg(ap, int);
      if (*fmt == 'd')
	to=int10_to_str((long) iarg,to, -10);
      else
	to=int10_to_str((long) (uint) iarg,to,10);
      continue;
    }
    /* We come here on '%%', unknown code or too long parameter */
    if (to == end)
      break;
    *to++='%';				/* % used as % or unknown code */
  }
  DBUG_ASSERT(to <= end);
  *to='\0';				/* End of errmessage */
  return (uint) (to - start);
}
#endif

int my_snprintf_8bit(CHARSET_INFO *cs  __attribute__((unused)),
		     char* to, uint n  __attribute__((unused)),
		     const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
#ifdef NOT_USED
  return my_vsnprintf_8bit(to, n, fmt, args);
#endif
  /* 
     FIXME: generally not safe, but it is OK for now
     FIXME: as far as it's not called unsafely in the current code
  */
  return vsprintf(to,fmt,args); /* FIXME */
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


long        my_strntol_8bit(CHARSET_INFO *cs,
			   const char *nptr, uint l, char **endptr, int base)
{
  int negative;
  register ulong cutoff;
  register unsigned int cutlim;
  register ulong i;
  register const char *s;
  register unsigned char c;
  const char *save, *e;
  int overflow;
  
  if (base < 0 || base == 1 || base > 36)
    base = 10;
  
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

  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;

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

  save = s;
  cutoff = ((ulong)~0L) / (unsigned long int) base;
  cutlim = (uint) (((ulong)~0L) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='F')
      c = c - 'A' + 10;
    else if (c>='a' && c<='f')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (ulong) base;
      i += c;
    }
  }
  
  if (s == save)
    goto noconv;
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (negative)
  {
    if (i  > (ulong) LONG_MIN)
      overflow = 1;
  }
  else if (i > (ulong) LONG_MAX)
    overflow = 1;
  
  if (overflow)
  {
    my_errno=(ERANGE);
    return negative ? LONG_MIN : LONG_MAX;
  }
  
  return (negative ? -((long) i) : (long) i);

noconv:
  my_errno=(EDOM);
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


ulong      my_strntoul_8bit(CHARSET_INFO *cs,
			   const char *nptr, uint l, char **endptr, int base)
{
  int negative;
  register ulong cutoff;
  register unsigned int cutlim;
  register ulong i;
  register const char *s;
  register unsigned char c;
  const char *save, *e;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    base = 10;
  
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

  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;

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

  save = s;
  cutoff = ((ulong)~0L) / (unsigned long int) base;
  cutlim = (uint) (((ulong)~0L) % (unsigned long int) base);
  overflow = 0;
  i = 0;
  
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='F')
      c = c - 'A' + 10;
    else if (c>='a' && c<='f')
      c = c - 'a' + 10;
    else
      break;
    if (c >= base)
      break;
    if (i > cutoff || (i == cutoff && c > cutlim))
      overflow = 1;
    else
    {
      i *= (ulong) base;
      i += c;
    }
  }

  if (s == save)
    goto noconv;

  if (endptr != NULL)
    *endptr = (char *) s;

  if (overflow)
  {
    my_errno=(ERANGE);
    return ((ulong)~0L);
  }
  
  return (negative ? -((long) i) : (long) i);
  
noconv:
  my_errno=(EDOM);
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


longlong   my_strntoll_8bit(CHARSET_INFO *cs __attribute__((unused)),
			   const char *nptr, uint l, char **endptr, int base)
{
  int negative;
  register ulonglong cutoff;
  register unsigned int cutlim;
  register ulonglong i;
  register const char *s, *e;
  register unsigned char c;
  const char *save;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    base = 10;

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

  if (base == 16 && s[0] == '0' && (s[1]=='X'|| s[1]=='x'))
    s += 2;

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

  save = s;

  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='F')
      c = c - 'A' + 10;
    else if (c>='a' && c<='f')
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
    my_errno=(ERANGE);
    return negative ? LONGLONG_MIN : LONGLONG_MAX;
  }

  return (negative ? -((longlong) i) : (longlong) i);

noconv:
  my_errno=(EDOM);
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}


ulonglong my_strntoull_8bit(CHARSET_INFO *cs,
			   const char *nptr, uint l, char **endptr, int base)
{
  int negative;
  register ulonglong cutoff;
  register unsigned int cutlim;
  register ulonglong i;
  register const char *s, *e;
  register unsigned char c;
  const char *save;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    base = 10;

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

  if (base == 16 && s[0] == '0' && (s[1]=='X' || s[1]=='x'))
    s += 2;

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

  save = s;

  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  overflow = 0;
  i = 0;
  for (c = *s; s != e; c = *++s)
  {
    if (c>='0' && c<='9')
      c -= '0';
    else if (c>='A' && c<='F')
      c = c - 'A' + 10;
    else if (c>='a' && c<='f')
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
    my_errno=(ERANGE);
    return (~(ulonglong) 0);
  }

  return (negative ? -((longlong) i) : (longlong) i);

noconv:
  my_errno=(EDOM);
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0L;
}

double      my_strntod_8bit(CHARSET_INFO *cs __attribute__((unused)),
			   const char *s, uint l, char **e)
{
  char   buf[256];
  double res;
  if((l+1)>sizeof(buf))
  {
    if (e)
      memcpy(*e,s,sizeof(s));
    return 0;
  }
  strncpy(buf,s,l);
  buf[l]='\0';
  res=strtod(buf,e);
  if (e)
    memcpy(*e,*e-buf+s,sizeof(s));
  return res;
}


/*
  This is a fast version optimized for the case of radix 10 / -10
*/

int my_l10tostr_8bit(CHARSET_INFO *cs __attribute__((unused)),
		     char *dst, uint len, int radix, long int val)
{
  char buffer[66];
  register char *p, *e;
  long int new_val;
  int  sl=0;
  uint l;
  
  e = p = &buffer[sizeof(buffer)-1];
  *e='\0';
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl   = 1;
      val  = -val;
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
  
  if (sl)
  {
    *--p='-';
  }

  l=e-p;
  l=(l>len)?len:l;
  memcpy(dst,p,l);
  return (int)l;
}

int my_ll10tostr_8bit(CHARSET_INFO *cs __attribute__((unused)),
		      char *dst, uint len, int radix, longlong val)
{
  char buffer[65];
  register char *p, *e;
  long long_val;
  int  sl=0;
  uint l;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl=1;
      val = -val;
    }
  }
  
  e = p = &buffer[sizeof(buffer)-1];
  *p='\0';
  
  if (val == 0)
  {
    *--p='0';
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
    *--p = '0' + (long_val - quo*10);
    long_val= quo;
  }
  
cnv:
  if (sl)
  {
    *--p='-';
  }
  
  l=e-p;
  l=(l>len)?len:l;
  memcpy(dst,p,l);
  return (int)(e-p);
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

#define INC_PTR(cs,A,B) A++


int my_wildcmp_8bit(CHARSET_INFO *cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  int result= -1;				// Not found, using wildcards

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;

      if (str == str_end || likeconv(cs,*wildstr++) != likeconv(cs,*str++))
	return(1);				// No match
      if (wildstr == wildend)
	return (str != str_end);		// Match if both are at end
      result=1;					// Found an anchor char
    }
    if (*wildstr == w_one)
    {
      do
      {
	if (str == str_end)			// Skip one char if possible
	  return (result);
	INC_PTR(cs,str,str_end);
      } while (++wildstr < wildend && *wildstr == w_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == w_many)
    {						// Found w_many
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
	    return (-1);
	  INC_PTR(cs,str,str_end);
	  continue;
	}
	break;					// Not a wild character
      }
      if (wildstr == wildend)
	return(0);				// Ok if w_many is last
      if (str == str_end)
	return -1;
      
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;

      INC_PTR(cs,wildstr,wildend);		// This is compared trough cmp
      cmp=likeconv(cs,cmp);   
      do
      {
          while (str != str_end && likeconv(cs,*str) != cmp)
            str++;
          if (str++ == str_end) return (-1);
	{
	  int tmp=my_wildcmp_8bit(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
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
				int escape, int w_one, int w_many,
				uint res_length,
				char *min_str,char *max_str,
				uint *min_length,uint *max_length)
{
  const char *end=ptr+ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;

  for (; ptr != end && min_str != min_end ; ptr++)
  {
    if (*ptr == escape && ptr+1 != end)
    {
      ptr++;					// Skip escape
      *min_str++= *max_str++ = *ptr;
      continue;
    }
    if (*ptr == w_one)				// '_' in SQL
    {
      *min_str++='\0';				// This should be min char
      *max_str++=cs->max_sort_char;
      continue;
    }
    if (*ptr == w_many)				// '%' in SQL
    {
      *min_length= (uint) (min_str - min_org);
      *max_length=res_length;
      do {
	*min_str++ = ' ';			// Because if key compression
	*max_str++ = cs->max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++= *max_str++ = *ptr;
  }
  *min_length= *max_length = (uint) (min_str - min_org);

  /* Temporary fix for handling w_one at end of string (key compression) */
  {
    char *tmp;
    for (tmp= min_str ; tmp > min_org && tmp[-1] == '\0';)
      *--tmp=' ';
  }

  while (min_str != min_end)
    *min_str++ = *max_str++ = ' ';		// Because if key compression
  return 0;
}
