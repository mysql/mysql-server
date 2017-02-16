/* Copyright (c) 2003, 2013, Oracle and/or its affiliates
   Copyright (c) 2009, 2016, MariaDB

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
   Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301, USA */

/* UCS2 support. Written by Alexander Barkov <bar@mysql.com> */

#include "strings_def.h"
#include <m_ctype.h>
#include <my_sys.h>
#include <stdarg.h>


#if defined(HAVE_CHARSET_utf16) || defined(HAVE_CHARSET_ucs2)
#define HAVE_CHARSET_mb2
#endif


#if defined(HAVE_CHARSET_mb2) || defined(HAVE_CHARSET_utf32)
#define HAVE_CHARSET_mb2_or_mb4
#endif


#ifndef EILSEQ
#define EILSEQ ENOENT
#endif

#undef  ULONGLONG_MAX
#define ULONGLONG_MAX                (~(ulonglong) 0)
#define MAX_NEGATIVE_NUMBER        ((ulonglong) LL(0x8000000000000000))
#define INIT_CNT  9
#define LFACTOR   ULL(1000000000)
#define LFACTOR1  ULL(10000000000)
#define LFACTOR2  ULL(100000000000)

#if defined(HAVE_CHARSET_utf32) || defined(HAVE_CHARSET_mb2)
static unsigned long lfactor[9]=
{ 1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L, 100000000L };
#endif


#ifdef HAVE_CHARSET_mb2_or_mb4
static inline int
my_bincmp(const uchar *s, const uchar *se,
          const uchar *t, const uchar *te)
{
  int slen= (int) (se - s), tlen= (int) (te - t);
  int len= min(slen, tlen);
  int cmp= memcmp(s, t, len);
  return cmp ? cmp : slen - tlen;
}


static size_t
my_caseup_str_mb2_or_mb4(CHARSET_INFO * cs  __attribute__((unused)), 
                         char * s __attribute__((unused)))
{
  DBUG_ASSERT(0);
  return 0;
}


static size_t
my_casedn_str_mb2_or_mb4(CHARSET_INFO *cs __attribute__((unused)), 
                         char * s __attribute__((unused)))
{
  DBUG_ASSERT(0);
  return 0;
}


static int
my_strcasecmp_mb2_or_mb4(CHARSET_INFO *cs __attribute__((unused)),
                         const char *s __attribute__((unused)),
                         const char *t __attribute__((unused)))
{
  DBUG_ASSERT(0);
  return 0;
}


static long
my_strntol_mb2_or_mb4(CHARSET_INFO *cs,
                      const char *nptr, size_t l, int base,
                      char **endptr, int *err)
{
  int      negative= 0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register unsigned int cutlim;
  register uint32 cutoff;
  register uint32 res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv= cs->cset->mb_wc(cs, &wc, s, e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr != NULL )
        *endptr= (char*) s;
      err[0]= (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+= cnv;
  } while (1);
  
bs:

  overflow= 0;
  res= 0;
  save= s;
  cutoff= ((uint32)~0L) / (uint32) base;
  cutlim= (uint) (((uint32)~0L) % (uint32) base);
  
  do {
    if ((cnv= cs->cset->mb_wc(cs, &wc, s, e)) > 0)
    {
      s+= cnv;
      if (wc >= '0' && wc <= '9')
        wc-= '0';
      else if (wc >= 'A' && wc <= 'Z')
        wc= wc - 'A' + 10;
      else if (wc >= 'a' && wc <= 'z')
        wc= wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow= 1;
      else
      {
        res*= (uint32) base;
        res+= wc;
      }
    }
    else if (cnv == MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*) s;
      err[0]= EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]= EDOM;
    return 0L;
  }
  
  if (negative)
  {
    if (res > (uint32) INT_MIN32)
      overflow= 1;
  }
  else if (res > INT_MAX32)
    overflow= 1;
  
  if (overflow)
  {
    err[0]= ERANGE;
    return negative ? INT_MIN32 : INT_MAX32;
  }
  
  return (negative ? -((long) res) : (long) res);
}


static ulong
my_strntoul_mb2_or_mb4(CHARSET_INFO *cs,
                       const char *nptr, size_t l, int base, 
                       char **endptr, int *err)
{
  int      negative= 0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register unsigned int cutlim;
  register uint32 cutoff;
  register uint32 res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr + l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv= cs->cset->mb_wc(cs, &wc, s, e)) > 0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr= (char*)s;
      err[0]= (cnv == MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+= cnv;
  } while (1);
  
bs:

  overflow= 0;
  res= 0;
  save= s;
  cutoff= ((uint32)~0L) / (uint32) base;
  cutlim= (uint) (((uint32)~0L) % (uint32) base);
  
  do
  {
    if ((cnv= cs->cset->mb_wc(cs, &wc, s, e)) > 0)
    {
      s+= cnv;
      if (wc >= '0' && wc <= '9')
        wc-= '0';
      else if (wc >= 'A' && wc <= 'Z')
        wc= wc - 'A' + 10;
      else if (wc >= 'a' && wc <= 'z')
        wc= wc - 'a' + 10;
      else
        break;
      if ((int) wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res*= (uint32) base;
        res+= wc;
      }
    }
    else if (cnv == MY_CS_ILSEQ)
    {
      if (endptr != NULL )
        *endptr= (char*)s;
      err[0]= EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr= (char *) s;
  
  if (s == save)
  {
    err[0]= EDOM;
    return 0L;
  }
  
  if (overflow)
  {
    err[0]= (ERANGE);
    return (~(uint32) 0);
  }
  
  return (negative ? -((long) res) : (long) res);
}


static longlong 
my_strntoll_mb2_or_mb4(CHARSET_INFO *cs,
                       const char *nptr, size_t l, int base,
                       char **endptr, int *err)
{
  int      negative=0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register ulonglong    cutoff;
  register unsigned int cutlim;
  register ulonglong    res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr+l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0] = (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:

  overflow = 0;
  res = 0;
  save = s;
  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  do {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulonglong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]=EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]=EDOM;
    return 0L;
  }
  
  if (negative)
  {
    if (res  > (ulonglong) LONGLONG_MIN)
      overflow = 1;
  }
  else if (res > (ulonglong) LONGLONG_MAX)
    overflow = 1;
  
  if (overflow)
  {
    err[0]=ERANGE;
    return negative ? LONGLONG_MIN : LONGLONG_MAX;
  }
  
  return (negative ? -((longlong)res) : (longlong)res);
}


static ulonglong
my_strntoull_mb2_or_mb4(CHARSET_INFO *cs,
                        const char *nptr, size_t l, int base,
                        char **endptr, int *err)
{
  int      negative= 0;
  int      overflow;
  int      cnv;
  my_wc_t  wc;
  register ulonglong    cutoff;
  register unsigned int cutlim;
  register ulonglong    res;
  register const uchar *s= (const uchar*) nptr;
  register const uchar *e= (const uchar*) nptr + l;
  const uchar *save;
  
  *err= 0;
  do
  {
    if ((cnv= cs->cset->mb_wc(cs,&wc,s,e)) > 0)
    {
      switch (wc)
      {
        case ' ' : break;
        case '\t': break;
        case '-' : negative= !negative; break;
        case '+' : break;
        default  : goto bs;
      }
    } 
    else /* No more characters or bad multibyte sequence */
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]= (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:

  overflow = 0;
  res = 0;
  save = s;
  cutoff = (~(ulonglong) 0) / (unsigned long int) base;
  cutlim = (uint) ((~(ulonglong) 0) % (unsigned long int) base);

  do
  {
    if ((cnv=cs->cset->mb_wc(cs,&wc,s,e))>0)
    {
      s+=cnv;
      if ( wc>='0' && wc<='9')
        wc -= '0';
      else if ( wc>='A' && wc<='Z')
        wc = wc - 'A' + 10;
      else if ( wc>='a' && wc<='z')
        wc = wc - 'a' + 10;
      else
        break;
      if ((int)wc >= base)
        break;
      if (res > cutoff || (res == cutoff && wc > cutlim))
        overflow = 1;
      else
      {
        res *= (ulonglong) base;
        res += wc;
      }
    }
    else if (cnv==MY_CS_ILSEQ)
    {
      if (endptr !=NULL )
        *endptr = (char*)s;
      err[0]= EILSEQ;
      return 0;
    } 
    else
    {
      /* No more characters */
      break;
    }
  } while(1);
  
  if (endptr != NULL)
    *endptr = (char *) s;
  
  if (s == save)
  {
    err[0]= EDOM;
    return 0L;
  }
  
  if (overflow)
  {
    err[0]= ERANGE;
    return (~(ulonglong) 0);
  }

  return (negative ? -((longlong) res) : (longlong) res);
}


static double
my_strntod_mb2_or_mb4(CHARSET_INFO *cs,
                      char *nptr, size_t length, 
                      char **endptr, int *err)
{
  char     buf[256];
  double   res;
  register char *b= buf;
  register const uchar *s= (const uchar*) nptr;
  const uchar *end;
  my_wc_t  wc;
  int     cnv;

  *err= 0;
  /* Cut too long strings */
  if (length >= sizeof(buf))
    length= sizeof(buf) - 1;
  end= s + length;

  while ((cnv= cs->cset->mb_wc(cs,&wc,s,end)) > 0)
  {
    s+= cnv;
    if (wc > (int) (uchar) 'e' || !wc)
      break;                                        /* Can't be part of double */
    *b++= (char) wc;
  }

  *endptr= b;
  res= my_strtod(buf, endptr, err);
  *endptr= nptr + cs->mbminlen * (size_t) (*endptr - buf);
  return res;
}


static ulonglong
my_strntoull10rnd_mb2_or_mb4(CHARSET_INFO *cs,
                             const char *nptr, size_t length,
                             int unsign_fl,
                             char **endptr, int *err)
{
  char  buf[256], *b= buf;
  ulonglong res;
  const uchar *end, *s= (const uchar*) nptr;
  my_wc_t  wc;
  int     cnv;

  /* Cut too long strings */
  if (length >= sizeof(buf))
    length= sizeof(buf)-1;
  end= s + length;

  while ((cnv= cs->cset->mb_wc(cs,&wc,s,end)) > 0)
  {
    s+= cnv;
    if (wc > (int) (uchar) 'e' || !wc)
      break;                            /* Can't be a number part */
    *b++= (char) wc;
  }

  res= my_strntoull10rnd_8bit(cs, buf, b - buf, unsign_fl, endptr, err);
  *endptr= (char*) nptr + cs->mbminlen * (size_t) (*endptr - buf);
  return res;
}


/*
  This is a fast version optimized for the case of radix 10 / -10
*/

static size_t
my_l10tostr_mb2_or_mb4(CHARSET_INFO *cs,
                       char *dst, size_t len, int radix, long int val)
{
  char buffer[66];
  register char *p, *db, *de;
  long int new_val;
  int  sl= 0;
  unsigned long int uval = (unsigned long int) val;
  
  p= &buffer[sizeof(buffer) - 1];
  *p= '\0';
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl= 1;
      /* Avoid integer overflow in (-val) for LONGLONG_MIN (BUG#31799). */
      uval  = (unsigned long int)0 - uval;
    }
  }
  
  new_val = (long) (uval / 10);
  *--p    = '0'+ (char) (uval - (unsigned long) new_val * 10);
  val= new_val;
  
  while (val != 0)
  {
    new_val= val / 10;
    *--p= '0' + (char) (val - new_val * 10);
    val= new_val;
  }
  
  if (sl)
  {
    *--p= '-';
  }
  
  for ( db= dst, de= dst + len ; (dst < de) && *p ; p++)
  {
    int cnvres= cs->cset->wc_mb(cs,(my_wc_t)p[0],(uchar*) dst, (uchar*) de);
    if (cnvres > 0)
      dst+= cnvres;
    else
      break;
  }
  return (int) (dst - db);
}


static size_t
my_ll10tostr_mb2_or_mb4(CHARSET_INFO *cs,
                        char *dst, size_t len, int radix, longlong val)
{
  char buffer[65];
  register char *p, *db, *de;
  long long_val;
  int sl= 0;
  ulonglong uval= (ulonglong) val;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl= 1;
      /* Avoid integer overflow in (-val) for LONGLONG_MIN (BUG#31799). */
      uval = (ulonglong)0 - uval;
    }
  }
  
  p= &buffer[sizeof(buffer)-1];
  *p='\0';
  
  if (uval == 0)
  {
    *--p= '0';
    goto cnv;
  }
  
  while (uval > (ulonglong) LONG_MAX)
  {
    ulonglong quo= uval/(uint) 10;
    uint rem= (uint) (uval- quo* (uint) 10);
    *--p= '0' + rem;
    uval= quo;
  }
  
  long_val= (long) uval;
  while (long_val != 0)
  {
    long quo= long_val/10;
    *--p= (char) ('0' + (long_val - quo*10));
    long_val= quo;
  }
  
cnv:
  if (sl)
  {
    *--p= '-';
  }
  
  for ( db= dst, de= dst + len ; (dst < de) && *p ; p++)
  {
    int cnvres= cs->cset->wc_mb(cs, (my_wc_t) p[0], (uchar*) dst, (uchar*) de);
    if (cnvres > 0)
      dst+= cnvres;
    else
      break;
  }
  return (int) (dst -db);
}

#endif /* HAVE_CHARSET_mb2_or_mb4 */


#ifdef HAVE_CHARSET_mb2
static longlong
my_strtoll10_mb2(CHARSET_INFO *cs __attribute__((unused)),
                 const char *nptr, char **endptr, int *error)
{
  const char *s, *end, *start, *n_end, *true_end;
  uchar c;
  unsigned long i, j, k;
  ulonglong li;
  int negative;
  ulong cutoff, cutoff2, cutoff3;

  s= nptr;
  /* If fixed length string */
  if (endptr)
  {
    /* Make sure string length is even */
    end= s + ((*endptr - s) / 2) * 2;
    while (s < end && !s[0] && (s[1] == ' ' || s[1] == '\t'))
      s+= 2;
    if (s == end)
      goto no_conv;
  }
  else
  {
     /* We don't support null terminated strings in UCS2 */
     goto no_conv;
  }

  /* Check for a sign. */
  negative= 0;
  if (!s[0] && s[1] == '-')
  {
    *error= -1;                                        /* Mark as negative number */
    negative= 1;
    s+= 2;
    if (s == end)
      goto no_conv;
    cutoff=  MAX_NEGATIVE_NUMBER / LFACTOR2;
    cutoff2= (MAX_NEGATIVE_NUMBER % LFACTOR2) / 100;
    cutoff3=  MAX_NEGATIVE_NUMBER % 100;
  }
  else
  {
    *error= 0;
    if (!s[0] && s[1] == '+')
    {
      s+= 2;
      if (s == end)
        goto no_conv;
    }
    cutoff=  ULONGLONG_MAX / LFACTOR2;
    cutoff2= ULONGLONG_MAX % LFACTOR2 / 100;
    cutoff3=  ULONGLONG_MAX % 100;
  }

  /* Handle case where we have a lot of pre-zero */
  if (!s[0] && s[1] == '0')
  {
    i= 0;
    do
    {
      s+= 2;
      if (s == end)
        goto end_i;                                /* Return 0 */
    }
    while (!s[0] && s[1] == '0');
    n_end= s + 2 * INIT_CNT;
  }
  else
  {
    /* Read first digit to check that it's a valid number */
    if (s[0] || (c= (s[1]-'0')) > 9)
      goto no_conv;
    i= c;
    s+= 2;
    n_end= s + 2 * (INIT_CNT-1);
  }

  /* Handle first 9 digits and store them in i */
  if (n_end > end)
    n_end= end;
  for (; s != n_end ; s+= 2)
  {
    if (s[0] || (c= (s[1]-'0')) > 9)
      goto end_i;
    i= i*10+c;
  }
  if (s == end)
    goto end_i;

  /* Handle next 9 digits and store them in j */
  j= 0;
  start= s;                                /* Used to know how much to shift i */
  n_end= true_end= s + 2 * INIT_CNT;
  if (n_end > end)
    n_end= end;
  do
  {
    if (s[0] || (c= (s[1]-'0')) > 9)
      goto end_i_and_j;
    j= j*10+c;
    s+= 2;
  } while (s != n_end);
  if (s == end)
  {
    if (s != true_end)
      goto end_i_and_j;
    goto end3;
  }
  if (s[0] || (c= (s[1]-'0')) > 9)
    goto end3;

  /* Handle the next 1 or 2 digits and store them in k */
  k=c;
  s+= 2;
  if (s == end || s[0] || (c= (s[1]-'0')) > 9)
    goto end4;
  k= k*10+c;
  s+= 2;
  *endptr= (char*) s;

  /* number string should have ended here */
  if (s != end && !s[0] && (c= (s[1]-'0')) <= 9)
    goto overflow;

  /* Check that we didn't get an overflow with the last digit */
  if (i > cutoff || (i == cutoff && ((j > cutoff2 || j == cutoff2) &&
                                     k > cutoff3)))
    goto overflow;
  li=i*LFACTOR2+ (ulonglong) j*100 + k;
  return (longlong) li;

overflow:                                        /* *endptr is set here */
  *error= MY_ERRNO_ERANGE;
  return negative ? LONGLONG_MIN : (longlong) ULONGLONG_MAX;

end_i:
  *endptr= (char*) s;
  return (negative ? ((longlong) -(long) i) : (longlong) i);

end_i_and_j:
  li= (ulonglong) i * lfactor[(size_t) (s-start) / 2] + j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end3:
  li=(ulonglong) i*LFACTOR+ (ulonglong) j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end4:
  li=(ulonglong) i*LFACTOR1+ (ulonglong) j * 10 + k;
  *endptr= (char*) s;
  if (negative)
  {
   if (li > MAX_NEGATIVE_NUMBER)
     goto overflow;
   return -((longlong) li);
  }
  return (longlong) li;

no_conv:
  /* There was no number to convert.  */
  *error= MY_ERRNO_EDOM;
  *endptr= (char *) nptr;
  return 0;
}


static size_t
my_scan_mb2(CHARSET_INFO *cs __attribute__((unused)),
            const char *str, const char *end, int sequence_type)
{
  const char *str0= str;
  end--; /* for easier loop condition, because of two bytes per character */
  
  switch (sequence_type)
  {
  case MY_SEQ_SPACES:
    for ( ; str < end; str+= 2)
    {
      if (str[0] != '\0' || str[1] != ' ')
        break;
    }
    return (size_t) (str - str0);
  default:
    return 0;
  }
}


static void
my_fill_mb2(CHARSET_INFO *cs __attribute__((unused)),
            char *s, size_t l, int fill)
{
  DBUG_ASSERT(fill <= 0xFFFF);
  for ( ; l >= 2; s[0]= (fill >> 8), s[1]= (fill & 0xFF), s+= 2, l-= 2);
}


static int
my_vsnprintf_mb2(char *dst, size_t n, const char* fmt, va_list ap)
{
  char *start=dst, *end= dst + n - 1;
  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (dst == end)                     /* End of buffer */
        break;
      
      *dst++='\0';
      *dst++= *fmt;          /* Copy ordinary char */
      continue;
    }
    
    fmt++;
    
    /* Skip if max size is used (to be compatible with printf) */
    while ( (*fmt >= '0' && *fmt <= '9') || *fmt == '.' || *fmt == '-')
      fmt++;
    
    if (*fmt == 'l')
      fmt++;
    
    if (*fmt == 's')                      /* String parameter */
    {
      char *par= va_arg(ap, char *);
      size_t plen;
      size_t left_len= (size_t)(end-dst);
      if (!par)
        par= (char*) "(null)";
      plen= strlen(par);
      if (left_len <= plen * 2)
        plen = left_len / 2 - 1;

      for ( ; plen ; plen--, dst+=2, par++)
      {
        dst[0]= '\0';
        dst[1]= par[0];
      }
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u')  /* Integer parameter */
    {
      int iarg;
      char nbuf[16];
      char *pbuf= nbuf;
      
      if ((size_t) (end - dst) < 32)
        break;
      iarg= va_arg(ap, int);
      if (*fmt == 'd')
        int10_to_str((long) iarg, nbuf, -10);
      else
        int10_to_str((long) (uint) iarg, nbuf,10);

      for (; pbuf[0]; pbuf++)
      {
        *dst++= '\0';
        *dst++= *pbuf;
      }
      continue;
    }
    
    /* We come here on '%%', unknown code or too long parameter */
    if (dst == end)
      break;
    *dst++= '\0';
    *dst++= '%';                            /* % used as % or unknown code */
  }
  
  DBUG_ASSERT(dst <= end);
  *dst='\0';                                /* End of errmessage */
  return (size_t) (dst - start);
}


static size_t
my_snprintf_mb2(CHARSET_INFO *cs __attribute__((unused)),
                char* to, size_t n, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  return my_vsnprintf_mb2(to, n, fmt, args);
}


static size_t
my_lengthsp_mb2(CHARSET_INFO *cs __attribute__((unused)),
                const char *ptr, size_t length)
{
  const char *end= ptr + length;
  while (end > ptr + 1 && end[-1] == ' ' && end[-2] == '\0')
    end-= 2;
  return (size_t) (end - ptr);
}

#endif /* HAVE_CHARSET_mb2*/




#ifdef HAVE_CHARSET_utf16

/*
  D800..DB7F - Non-provate surrogate high (896 pages)
  DB80..DBFF - Private surrogate high     (128 pages)
  DC00..DFFF - Surrogate low              (1024 codes in a page)
*/

#define MY_UTF16_HIGH_HEAD(x)  ((((uchar) (x)) & 0xFC) == 0xD8)
#define MY_UTF16_LOW_HEAD(x)   ((((uchar) (x)) & 0xFC) == 0xDC)
#define MY_UTF16_SURROGATE(x)  (((x) & 0xF800) == 0xD800)

static int
my_utf16_uni(CHARSET_INFO *cs __attribute__((unused)),
             my_wc_t *pwc, const uchar *s, const uchar *e)
{
  if (s + 2 > e)
    return MY_CS_TOOSMALL2;
  
  /*
    High bytes: 0xD[89AB] = B'110110??'
    Low bytes:  0xD[CDEF] = B'110111??'
    Surrogate mask:  0xFC = B'11111100'
  */

  if (MY_UTF16_HIGH_HEAD(*s)) /* Surrogate head */
  {
    if (s + 4 > e)
      return MY_CS_TOOSMALL4;

    if (!MY_UTF16_LOW_HEAD(s[2]))  /* Broken surrigate pair */
      return MY_CS_ILSEQ;

    /*
      s[0]= 110110??  (<< 18)
      s[1]= ????????  (<< 10)
      s[2]= 110111??  (<<  8)
      s[3]= ????????  (<<  0)
    */ 

    *pwc= ((s[0] & 3) << 18) + (s[1] << 10) +
          ((s[2] & 3) << 8) + s[3] + 0x10000;

    return 4;
  }

  if (MY_UTF16_LOW_HEAD(*s)) /* Low surrogate part without high part */
    return MY_CS_ILSEQ;
  
  *pwc= (s[0] << 8) + s[1];
  return 2;
}


static int
my_uni_utf16(CHARSET_INFO *cs __attribute__((unused)),
             my_wc_t wc, uchar *s, uchar *e)
{
  if (wc <= 0xFFFF)
  {
    if (s + 2 > e)
      return MY_CS_TOOSMALL2;
    if (MY_UTF16_SURROGATE(wc))
      return MY_CS_ILUNI;
    *s++= (uchar) (wc >> 8);
    *s= (uchar) (wc & 0xFF);
    return 2;
  }

  if (wc <= 0x10FFFF)
  {
    if (s + 4 > e)
      return MY_CS_TOOSMALL4;
    *s++= (uchar) ((wc-= 0x10000) >> 18) | 0xD8;
    *s++= (uchar) (wc >> 10) & 0xFF;
    *s++= (uchar) ((wc >> 8) & 3) | 0xDC;
    *s= (uchar) wc & 0xFF;
    return 4;
  }

  return MY_CS_ILUNI;
}


static inline void
my_tolower_utf16(MY_UNICASE_INFO * const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256 && uni_plane[page])
    *wc= uni_plane[page][*wc & 0xFF].tolower;
}


static inline void
my_toupper_utf16(MY_UNICASE_INFO * const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256 && uni_plane[page])
    *wc= uni_plane[page][*wc & 0xFF].toupper;
}


static inline void
my_tosort_utf16(MY_UNICASE_INFO * const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256)
  {
    if (uni_plane[page])
      *wc= uni_plane[page][*wc & 0xFF].sort;
  }
  else
  {
    *wc= MY_CS_REPLACEMENT_CHARACTER;
  }
}


static size_t
my_caseup_utf16(CHARSET_INFO *cs, char *src, size_t srclen,
                char *dst __attribute__((unused)),
                size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);
  
  while ((src < srcend) &&
         (res= my_utf16_uni(cs, &wc, (uchar *)src, (uchar*) srcend)) > 0)
  {
    my_toupper_utf16(uni_plane, &wc);
    if (res != my_uni_utf16(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static void
my_hash_sort_utf16(CHARSET_INFO *cs, const uchar *s, size_t slen,
                   ulong *n1, ulong *n2)
{
  my_wc_t wc;
  int res;
  const uchar *e= s+slen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  while (e > s + 1 && e[-1] == ' ' && e[-2] == '\0')
    e-= 2;

  while ((s < e) && (res= my_utf16_uni(cs, &wc, (uchar *)s, (uchar*)e)) > 0)
  {
    my_tosort_utf16(uni_plane, &wc);
    n1[0]^= (((n1[0] & 63) + n2[0]) * (wc & 0xFF)) + (n1[0] << 8);
    n2[0]+= 3;
    n1[0]^= (((n1[0] & 63) + n2[0]) * (wc >> 8)) + (n1[0] << 8);
    n2[0]+= 3;
    s+= res;
  }
}


static size_t
my_casedn_utf16(CHARSET_INFO *cs, char *src, size_t srclen,
                char *dst __attribute__((unused)),
                size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);

  while ((src < srcend) &&
         (res= my_utf16_uni(cs, &wc, (uchar*) src, (uchar*) srcend)) > 0)
  {
    my_tolower_utf16(uni_plane, &wc);
    if (res != my_uni_utf16(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static int
my_strnncoll_utf16(CHARSET_INFO *cs, 
                   const uchar *s, size_t slen, 
                   const uchar *t, size_t tlen,
                   my_bool t_is_prefix)
{
  int s_res, t_res;
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se= s + slen;
  const uchar *te= t + tlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  while (s < se && t < te)
  {
    s_res= my_utf16_uni(cs, &s_wc, s, se);
    t_res= my_utf16_uni(cs, &t_wc, t, te);

    if (s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare by char value */
      return my_bincmp(s, se, t, te);
    }

    my_tosort_utf16(uni_plane, &s_wc);
    my_tosort_utf16(uni_plane, &t_wc);

    if (s_wc != t_wc)
    {
      return  s_wc > t_wc ? 1 : -1;
    }

    s+= s_res;
    t+= t_res;
  }
  return (int) (t_is_prefix ? (t - te) : ((se - s) - (te - t)));
}


/**
  Compare strings, discarding end space

  If one string is shorter as the other, then we space extend the other
  so that the strings have equal length.

  This will ensure that the following things hold:

    "a"  == "a "
    "a\0" < "a"
    "a\0" < "a "

  @param  cs        Character set pinter.
  @param  a         First string to compare.
  @param  a_length  Length of 'a'.
  @param  b         Second string to compare.
  @param  b_length  Length of 'b'.

  IMPLEMENTATION

  @return Comparison result.
    @retval Negative number, if a less than b.
    @retval 0, if a is equal to b
    @retval Positive number, if a > b
*/

static int
my_strnncollsp_utf16(CHARSET_INFO *cs,
                     const uchar *s, size_t slen,
                     const uchar *t, size_t tlen,
                     my_bool diff_if_only_endspace_difference)
{
  int res;
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se= s + slen, *te= t + tlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  DBUG_ASSERT((slen % 2) == 0);
  DBUG_ASSERT((tlen % 2) == 0);

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
  diff_if_only_endspace_difference= FALSE;
#endif

  while (s < se && t < te)
  {
    int s_res= my_utf16_uni(cs, &s_wc, s, se);
    int t_res= my_utf16_uni(cs, &t_wc, t, te);

    if (s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare bytewise */
      return my_bincmp(s, se, t, te);
    }

    my_tosort_utf16(uni_plane, &s_wc);
    my_tosort_utf16(uni_plane, &t_wc);
    
    if (s_wc != t_wc)
    {
      return s_wc > t_wc ? 1 : -1;
    }

    s+= s_res;
    t+= t_res;
  }

  slen= (size_t) (se - s);
  tlen= (size_t) (te - t);
  res= 0;

  if (slen != tlen)
  {
    int s_res, swap= 1;
    if (diff_if_only_endspace_difference)
      res= 1;                                   /* Assume 's' is bigger */
    if (slen < tlen)
    {
      slen= tlen;
      s= t;
      se= te;
      swap= -1;
      res= -res;
    }

    for ( ; s < se; s+= s_res)
    {
      if ((s_res= my_utf16_uni(cs, &s_wc, s, se)) < 0)
      {
        DBUG_ASSERT(0);
        return 0;
      }
      if (s_wc != ' ')
        return (s_wc < ' ') ? -swap : swap;
    }
  }
  return res;
}


static uint
my_ismbchar_utf16(CHARSET_INFO *cs __attribute__((unused)),
                  const char *b __attribute__((unused)),
                  const char *e __attribute__((unused)))
{
  if (b + 2 > e)
    return 0;
  
  if (MY_UTF16_HIGH_HEAD(*b))
  {
    return (b + 4 <= e) && MY_UTF16_LOW_HEAD(b[2]) ? 4 : 0;
  }
  
  if (MY_UTF16_LOW_HEAD(*b))
    return 0;
  
  return 2;
}


static uint
my_mbcharlen_utf16(CHARSET_INFO *cs  __attribute__((unused)),
                   uint c __attribute__((unused)))
{
  return MY_UTF16_HIGH_HEAD(c) ? 4 : 2;
}


static size_t
my_numchars_utf16(CHARSET_INFO *cs,
                  const char *b, const char *e)
{
  size_t nchars= 0;
  for ( ; ; nchars++)
  {
    size_t charlen= my_ismbchar_utf16(cs, b, e);
    if (!charlen)
      break;
    b+= charlen;
  }
  return nchars;
}


static size_t
my_charpos_utf16(CHARSET_INFO *cs,
                 const char *b, const char *e, size_t pos)
{
  const char *b0= b;
  uint charlen;
  
  for ( ; pos; b+= charlen, pos--)
  {
    if (!(charlen= my_ismbchar(cs, b, e)))
      return (e + 2 - b0); /* Error, return pos outside the string */
  }
  return (size_t) (pos ? (e + 2 - b0) : (b - b0));
}


static size_t
my_well_formed_len_utf16(CHARSET_INFO *cs,
                         const char *b, const char *e,
                         size_t nchars, int *error)
{
  const char *b0= b;
  uint charlen;
  *error= 0;
  
  for ( ; nchars; b+= charlen, nchars--)
  {
    if (!(charlen= my_ismbchar(cs, b, e)))
    {
      *error= b < e ? 1 : 0;
      break;
    }
  }
  return (size_t) (b - b0);
}


static int
my_wildcmp_utf16_ci(CHARSET_INFO *cs,
                    const char *str,const char *str_end,
                    const char *wildstr,const char *wildend,
                    int escape, int w_one, int w_many)
{
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  return my_wildcmp_unicode(cs, str, str_end, wildstr, wildend,
                            escape, w_one, w_many, uni_plane); 
}


static int
my_wildcmp_utf16_bin(CHARSET_INFO *cs,
                     const char *str,const char *str_end,
                     const char *wildstr,const char *wildend,
                     int escape, int w_one, int w_many)
{
  return my_wildcmp_unicode(cs, str, str_end, wildstr, wildend,
                            escape, w_one, w_many, NULL); 
}


static int
my_strnncoll_utf16_bin(CHARSET_INFO *cs, 
                       const uchar *s, size_t slen,
                       const uchar *t, size_t tlen,
                       my_bool t_is_prefix)
{
  int s_res,t_res;
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se=s+slen;
  const uchar *te=t+tlen;

  while ( s < se && t < te )
  {
    s_res= my_utf16_uni(cs,&s_wc, s, se);
    t_res= my_utf16_uni(cs,&t_wc, t, te);

    if (s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare by char value */
      return my_bincmp(s, se, t, te);
    }
    if (s_wc != t_wc)
    {
      return s_wc > t_wc ? 1 : -1;
    }

    s+= s_res;
    t+= t_res;
  }
  return (int) (t_is_prefix ? (t - te) : ((se - s) - (te - t)));
}


static int
my_strnncollsp_utf16_bin(CHARSET_INFO *cs,
                         const uchar *s, size_t slen,
                         const uchar *t, size_t tlen,
                         my_bool diff_if_only_endspace_difference)
{
  int res;
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se= s + slen, *te= t + tlen;

  DBUG_ASSERT((slen % 2) == 0);
  DBUG_ASSERT((tlen % 2) == 0);

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
  diff_if_only_endspace_difference= FALSE;
#endif

  while (s < se && t < te)
  {
    int s_res= my_utf16_uni(cs, &s_wc, s, se);
    int t_res= my_utf16_uni(cs, &t_wc, t, te);

    if (s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare bytewise */
      return my_bincmp(s, se, t, te);
    }

    if (s_wc != t_wc)
    {
      return s_wc > t_wc ? 1 : -1;
    }

    s+= s_res;
    t+= t_res;
  }

  slen= (size_t) (se - s);
  tlen= (size_t) (te - t);
  res= 0;

  if (slen != tlen)
  {
    int s_res, swap= 1;
    if (diff_if_only_endspace_difference)
      res= 1;                                   /* Assume 's' is bigger */
    if (slen < tlen)
    {
      slen= tlen;
      s= t;
      se= te;
      swap= -1;
      res= -res;
    }

    for ( ; s < se; s+= s_res)
    {
      if ((s_res= my_utf16_uni(cs, &s_wc, s, se)) < 0)
      {
        DBUG_ASSERT(0);
        return 0;
      }
      if (s_wc != ' ')
        return (s_wc < ' ') ? -swap : swap;
    }
  }
  return res;
}


static void
my_hash_sort_utf16_bin(CHARSET_INFO *cs __attribute__((unused)),
                       const uchar *key, size_t len,ulong *nr1, ulong *nr2)
{
  const uchar *pos = key;
  
  key+= len;

  while (key > pos + 1 && key[-1] == ' ' && key[-2] == '\0')
    key-= 2;

  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^= (ulong) ((((uint) nr1[0] & 63) + nr2[0]) * 
              ((uint)*pos)) + (nr1[0] << 8);
    nr2[0]+= 3;
  }
}


static MY_COLLATION_HANDLER my_collation_utf16_general_ci_handler =
{
  NULL,                /* init */
  my_strnncoll_utf16,
  my_strnncollsp_utf16,
  my_strnxfrm_unicode,
  my_strnxfrmlen_simple,
  my_like_range_generic,
  my_wildcmp_utf16_ci,
  my_strcasecmp_mb2_or_mb4,
  my_instr_mb,
  my_hash_sort_utf16,
  my_propagate_simple
};


static MY_COLLATION_HANDLER my_collation_utf16_bin_handler =
{
  NULL,                /* init */
  my_strnncoll_utf16_bin,
  my_strnncollsp_utf16_bin,
  my_strnxfrm_unicode_full_bin,
  my_strnxfrmlen_unicode_full_bin,
  my_like_range_generic,
  my_wildcmp_utf16_bin,
  my_strcasecmp_mb2_or_mb4,
  my_instr_mb,
  my_hash_sort_utf16_bin,
  my_propagate_simple
};


MY_CHARSET_HANDLER my_charset_utf16_handler=
{
  NULL,                /* init         */
  my_ismbchar_utf16,   /* ismbchar     */
  my_mbcharlen_utf16,  /* mbcharlen    */
  my_numchars_utf16,
  my_charpos_utf16,
  my_well_formed_len_utf16,
  my_lengthsp_mb2,
  my_numcells_mb,
  my_utf16_uni,        /* mb_wc        */
  my_uni_utf16,        /* wc_mb        */
  my_mb_ctype_mb,
  my_caseup_str_mb2_or_mb4,
  my_casedn_str_mb2_or_mb4,
  my_caseup_utf16,
  my_casedn_utf16,
  my_snprintf_mb2,
  my_l10tostr_mb2_or_mb4,
  my_ll10tostr_mb2_or_mb4,
  my_fill_mb2,
  my_strntol_mb2_or_mb4,
  my_strntoul_mb2_or_mb4,
  my_strntoll_mb2_or_mb4,
  my_strntoull_mb2_or_mb4,
  my_strntod_mb2_or_mb4,
  my_strtoll10_mb2,
  my_strntoull10rnd_mb2_or_mb4,
  my_scan_mb2
};


struct charset_info_st my_charset_utf16_general_ci=
{
  54,0,0,              /* number       */
  MY_CS_COMPILED|MY_CS_PRIMARY|MY_CS_STRNXFRM|MY_CS_UNICODE|MY_CS_NONASCII,
  "utf16",             /* cs name    */
  "utf16_general_ci",  /* name         */
  "UTF-16 Unicode",    /* comment      */
  NULL,                /* tailoring    */
  NULL,                /* ctype        */
  NULL,                /* to_lower     */
  NULL,                /* to_upper     */
  NULL,                /* sort_order   */
  NULL,                /* contractions */
  NULL,                /* sort_order_big*/
  NULL,                /* tab_to_uni   */
  NULL,                /* tab_from_uni */
  my_unicase_default,  /* caseinfo     */
  NULL,                /* state_map    */
  NULL,                /* ident_map    */
  1,                   /* strxfrm_multiply */
  1,                   /* caseup_multiply  */
  1,                   /* casedn_multiply  */
  2,                   /* mbminlen     */
  4,                   /* mbmaxlen     */
  0,                   /* min_sort_char */
  0xFFFF,              /* max_sort_char */
  ' ',                 /* pad char      */
  0,                   /* escape_with_backslash_is_dangerous */
  &my_charset_utf16_handler,
  &my_collation_utf16_general_ci_handler
};


struct charset_info_st my_charset_utf16_bin=
{
  55,0,0,              /* number       */
  MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_STRNXFRM|MY_CS_UNICODE|MY_CS_NONASCII,
  "utf16",             /* cs name      */
  "utf16_bin",         /* name         */
  "UTF-16 Unicode",    /* comment      */
  NULL,                /* tailoring    */
  NULL,                /* ctype        */
  NULL,                /* to_lower     */
  NULL,                /* to_upper     */
  NULL,                /* sort_order   */
  NULL,                /* contractions */
  NULL,                /* sort_order_big*/
  NULL,                /* tab_to_uni   */
  NULL,                /* tab_from_uni */
  my_unicase_default,  /* caseinfo     */
  NULL,                /* state_map    */
  NULL,                /* ident_map    */
  1,                   /* strxfrm_multiply */
  1,                   /* caseup_multiply  */
  1,                   /* casedn_multiply  */
  2,                   /* mbminlen     */
  4,                   /* mbmaxlen     */
  0,                   /* min_sort_char */
  0xFFFF,              /* max_sort_char */
  ' ',                 /* pad char      */
  0,                   /* escape_with_backslash_is_dangerous */
  &my_charset_utf16_handler,
  &my_collation_utf16_bin_handler
};

#endif /* HAVE_CHARSET_utf16 */


#ifdef HAVE_CHARSET_utf32

static int
my_utf32_uni(CHARSET_INFO *cs __attribute__((unused)),
             my_wc_t *pwc, const uchar *s, const uchar *e)
{
  if (s + 4 > e)
    return MY_CS_TOOSMALL4;
  *pwc= (s[0] << 24) + (s[1] << 16) + (s[2] << 8) + (s[3]);
  return 4;
}


static int
my_uni_utf32(CHARSET_INFO *cs __attribute__((unused)),
             my_wc_t wc, uchar *s, uchar *e)
{
  if (s + 4 > e) 
    return MY_CS_TOOSMALL4;
  
  s[0]= (uchar) (wc >> 24);
  s[1]= (uchar) (wc >> 16) & 0xFF;
  s[2]= (uchar) (wc >> 8)  & 0xFF;
  s[3]= (uchar) wc & 0xFF;
  return 4;
}


static inline void
my_tolower_utf32(MY_UNICASE_INFO * const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256 && uni_plane[page])
    *wc= uni_plane[page][*wc & 0xFF].tolower;
}


static inline void
my_toupper_utf32(MY_UNICASE_INFO * const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256 && uni_plane[page])
    *wc= uni_plane[page][*wc & 0xFF].toupper;
}


static inline void
my_tosort_utf32(MY_UNICASE_INFO *const* uni_plane, my_wc_t *wc)
{
  uint page= *wc >> 8;
  if (page < 256)
  {
    if (uni_plane[page])
      *wc= uni_plane[page][*wc & 0xFF].sort;
  }
  else
  {
    *wc= MY_CS_REPLACEMENT_CHARACTER;
  }
}


static size_t
my_caseup_utf32(CHARSET_INFO *cs, char *src, size_t srclen,
                char *dst __attribute__((unused)),
                size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);
  
  while ((src < srcend) &&
         (res= my_utf32_uni(cs, &wc, (uchar *)src, (uchar*) srcend)) > 0)
  {
    my_toupper_utf32(uni_plane, &wc);
    if (res != my_uni_utf32(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static inline void
my_hash_add(ulong *n1, ulong *n2, uint ch)
{
  n1[0]^= (((n1[0] & 63) + n2[0]) * (ch)) + (n1[0] << 8);
  n2[0]+= 3;
}


static void
my_hash_sort_utf32(CHARSET_INFO *cs, const uchar *s, size_t slen,
                   ulong *n1, ulong *n2)
{
  my_wc_t wc;
  int res;
  const uchar *e= s + slen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  /* Skip trailing spaces */
  while (e > s + 3 && e[-1] == ' ' && !e[-2] && !e[-3] && !e[-4])
    e-= 4;

  while ((res= my_utf32_uni(cs, &wc, (uchar*) s, (uchar*) e)) > 0)
  {
    my_tosort_utf32(uni_plane, &wc);
    my_hash_add(n1, n2, (uint) (wc >> 24));
    my_hash_add(n1, n2, (uint) (wc >> 16) & 0xFF);
    my_hash_add(n1, n2, (uint) (wc >> 8)  & 0xFF);
    my_hash_add(n1, n2, (uint) (wc & 0xFF));
    s+= res;
  }
}


static size_t
my_casedn_utf32(CHARSET_INFO *cs, char *src, size_t srclen,
                char *dst __attribute__((unused)),
                size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);

  while ((res= my_utf32_uni(cs, &wc, (uchar*) src, (uchar*) srcend)) > 0)
  {
    my_tolower_utf32(uni_plane,&wc);
    if (res != my_uni_utf32(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static int
my_strnncoll_utf32(CHARSET_INFO *cs, 
                   const uchar *s, size_t slen, 
                   const uchar *t, size_t tlen,
                   my_bool t_is_prefix)
{
  my_wc_t UNINIT_VAR(s_wc),UNINIT_VAR(t_wc);
  const uchar *se= s + slen;
  const uchar *te= t + tlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  while (s < se && t < te)
  {
    int s_res= my_utf32_uni(cs, &s_wc, s, se);
    int t_res= my_utf32_uni(cs, &t_wc, t, te);
    
    if ( s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare by char value */
      return my_bincmp(s, se, t, te);
    }
    
    my_tosort_utf32(uni_plane, &s_wc);
    my_tosort_utf32(uni_plane, &t_wc);
    
    if (s_wc != t_wc)
    {
      return s_wc > t_wc ? 1 : -1;
    }
    
    s+= s_res;
    t+= t_res;
  }
  return (int) (t_is_prefix ? (t - te) : ((se - s) - (te - t)));
}


/**
  Compare strings, discarding end space

  If one string is shorter as the other, then we space extend the other
  so that the strings have equal length.

  This will ensure that the following things hold:

    "a"  == "a "
    "a\0" < "a"
    "a\0" < "a "

  @param  cs        Character set pinter.
  @param  a         First string to compare.
  @param  a_length  Length of 'a'.
  @param  b         Second string to compare.
  @param  b_length  Length of 'b'.

  IMPLEMENTATION

  @return Comparison result.
    @retval Negative number, if a less than b.
    @retval 0, if a is equal to b
    @retval Positive number, if a > b
*/


static int
my_strnncollsp_utf32(CHARSET_INFO *cs,
                     const uchar *s, size_t slen,
                     const uchar *t, size_t tlen,
                     my_bool diff_if_only_endspace_difference)
{
  int res;
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se= s + slen, *te= t + tlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  DBUG_ASSERT((slen % 4) == 0);
  DBUG_ASSERT((tlen % 4) == 0);

#ifndef VARCHAR_WITH_DIFF_ENDSPACE_ARE_DIFFERENT_FOR_UNIQUE
  diff_if_only_endspace_difference= FALSE;
#endif

  while ( s < se && t < te )
  {
    int s_res= my_utf32_uni(cs, &s_wc, s, se);
    int t_res= my_utf32_uni(cs, &t_wc, t, te);

    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare bytewise */
      return my_bincmp(s, se, t, te);
    }

    my_tosort_utf32(uni_plane, &s_wc);
    my_tosort_utf32(uni_plane, &t_wc);
    
    if ( s_wc != t_wc )
    {
      return s_wc > t_wc ? 1 : -1;
    }

    s+= s_res;
    t+= t_res;
  }

  slen= (size_t) (se - s);
  tlen= (size_t) (te - t);
  res= 0;

  if (slen != tlen)
  {
    int s_res, swap= 1;
    if (diff_if_only_endspace_difference)
      res= 1;                                   /* Assume 's' is bigger */
    if (slen < tlen)
    {
      slen= tlen;
      s= t;
      se= te;
      swap= -1;
      res= -res;
    }

    for ( ; s < se; s+= s_res)
    {
      if ((s_res= my_utf32_uni(cs, &s_wc, s, se)) < 0)
      {
        DBUG_ASSERT(0);
        return 0;
      }
      if (s_wc != ' ')
        return (s_wc < ' ') ? -swap : swap;
    }
  }
  return res;
}


static size_t
my_strnxfrmlen_utf32(CHARSET_INFO *cs __attribute__((unused)), size_t len)
{
  return len / 2;
}


static uint
my_ismbchar_utf32(CHARSET_INFO *cs __attribute__((unused)),
                  const char *b,
                  const char *e)
{
  return b + 4 > e ? 0 : 4;
}


static uint
my_mbcharlen_utf32(CHARSET_INFO *cs  __attribute__((unused)) , 
                   uint c __attribute__((unused)))
{
  return 4;
}


static int
my_vsnprintf_utf32(char *dst, size_t n, const char* fmt, va_list ap)
{
  char *start= dst, *end= dst + n;
  DBUG_ASSERT((n % 4) == 0);
  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (dst >= end)                        /* End of buffer */
        break;
      
      *dst++= '\0';
      *dst++= '\0';
      *dst++= '\0';
      *dst++= *fmt;        /* Copy ordinary char */
      continue;
    }
    
    fmt++;
    
    /* Skip if max size is used (to be compatible with printf) */
    while ( (*fmt>='0' && *fmt<='9') || *fmt == '.' || *fmt == '-')
      fmt++;
    
    if (*fmt == 'l')
      fmt++;
    
    if (*fmt == 's')                                /* String parameter */
    {
      reg2 char *par= va_arg(ap, char *);
      size_t plen;
      size_t left_len= (size_t)(end - dst);
      if (!par) par= (char*)"(null)";
      plen= strlen(par);
      if (left_len <= plen*4)
        plen= left_len / 4 - 1;

      for ( ; plen ; plen--, dst+= 4, par++)
      {
        dst[0]= '\0';
        dst[1]= '\0';
        dst[2]= '\0';
        dst[3]= par[0];
      }
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u')        /* Integer parameter */
    {
      register int iarg;
      char nbuf[16];
      char *pbuf= nbuf;
      
      if ((size_t) (end - dst) < 64)
        break;
      iarg= va_arg(ap, int);
      if (*fmt == 'd')
        int10_to_str((long) iarg, nbuf, -10);
      else
        int10_to_str((long) (uint) iarg,nbuf,10);

      for (; pbuf[0]; pbuf++)
      {
        *dst++= '\0';
        *dst++= '\0';
        *dst++= '\0';
        *dst++= *pbuf;
      }
      continue;
    }
    
    /* We come here on '%%', unknown code or too long parameter */
    if (dst == end)
      break;
    *dst++= '\0';
    *dst++= '\0';
    *dst++= '\0';
    *dst++= '%';    /* % used as % or unknown code */
  }
  
  DBUG_ASSERT(dst < end);
  *dst++= '\0';
  *dst++= '\0';
  *dst++= '\0';
  *dst++= '\0';     /* End of errmessage */
  return (size_t) (dst - start - 4);
}


static size_t
my_snprintf_utf32(CHARSET_INFO *cs __attribute__((unused)),
                  char* to, size_t n, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  return my_vsnprintf_utf32(to, n, fmt, args);
}


static longlong
my_strtoll10_utf32(CHARSET_INFO *cs __attribute__((unused)),
                   const char *nptr, char **endptr, int *error)
{
  const char *s, *end, *start, *n_end, *true_end;
  uchar c;
  unsigned long i, j, k;
  ulonglong li;
  int negative;
  ulong cutoff, cutoff2, cutoff3;

  s= nptr;
  /* If fixed length string */
  if (endptr)
  {
    /* Make sure string length is even */
    end= s + ((*endptr - s) / 4) * 4;
    while (s < end && !s[0] && !s[1] && !s[2] &&
           (s[3] == ' ' || s[3] == '\t'))
      s+= 4;
    if (s == end)
      goto no_conv;
  }
  else
  {
     /* We don't support null terminated strings in UCS2 */
     goto no_conv;
  }

  /* Check for a sign. */
  negative= 0;
  if (!s[0] && !s[1] && !s[2] && s[3] == '-')
  {
    *error= -1;                                        /* Mark as negative number */
    negative= 1;
    s+= 4;
    if (s == end)
      goto no_conv;
    cutoff=  MAX_NEGATIVE_NUMBER / LFACTOR2;
    cutoff2= (MAX_NEGATIVE_NUMBER % LFACTOR2) / 100;
    cutoff3=  MAX_NEGATIVE_NUMBER % 100;
  }
  else
  {
    *error= 0;
    if (!s[0] && !s[1] && !s[2] && s[3] == '+')
    {
      s+= 4;
      if (s == end)
        goto no_conv;
    }
    cutoff=  ULONGLONG_MAX / LFACTOR2;
    cutoff2= ULONGLONG_MAX % LFACTOR2 / 100;
    cutoff3=  ULONGLONG_MAX % 100;
  }

  /* Handle case where we have a lot of pre-zero */
  if (!s[0] && !s[1] && !s[2] && s[3] == '0')
  {
    i= 0;
    do
    {
      s+= 4;
      if (s == end)
        goto end_i;                                /* Return 0 */
    }
    while (!s[0] && !s[1] && !s[2] && s[3] == '0');
    n_end= s + 4 * INIT_CNT;
  }
  else
  {
    /* Read first digit to check that it's a valid number */
    if (s[0] || s[1] || s[2] || (c= (s[3]-'0')) > 9)
      goto no_conv;
    i= c;
    s+= 4;
    n_end= s + 4 * (INIT_CNT-1);
  }

  /* Handle first 9 digits and store them in i */
  if (n_end > end)
    n_end= end;
  for (; s != n_end ; s+= 4)
  {
    if (s[0] || s[1] || s[2] || (c= (s[3] - '0')) > 9)
      goto end_i;
    i= i * 10 + c;
  }
  if (s == end)
    goto end_i;

  /* Handle next 9 digits and store them in j */
  j= 0;
  start= s;                                /* Used to know how much to shift i */
  n_end= true_end= s + 4 * INIT_CNT;
  if (n_end > end)
    n_end= end;
  do
  {
    if (s[0] || s[1] || s[2] || (c= (s[3] - '0')) > 9)
      goto end_i_and_j;
    j= j * 10 + c;
    s+= 4;
  } while (s != n_end);
  if (s == end)
  {
    if (s != true_end)
      goto end_i_and_j;
    goto end3;
  }
  if (s[0] || s[1] || s[2] || (c= (s[3] - '0')) > 9)
    goto end3;

  /* Handle the next 1 or 2 digits and store them in k */
  k=c;
  s+= 4;
  if (s == end || s[0] || s[1] || s[2] || (c= (s[3]-'0')) > 9)
    goto end4;
  k= k * 10 + c;
  s+= 2;
  *endptr= (char*) s;

  /* number string should have ended here */
  if (s != end && !s[0] && !s[1] && !s[2] && (c= (s[3] - '0')) <= 9)
    goto overflow;

  /* Check that we didn't get an overflow with the last digit */
  if (i > cutoff || (i == cutoff && ((j > cutoff2 || j == cutoff2) &&
                                     k > cutoff3)))
    goto overflow;
  li= i * LFACTOR2+ (ulonglong) j * 100 + k;
  return (longlong) li;

overflow:                                        /* *endptr is set here */
  *error= MY_ERRNO_ERANGE;
  return negative ? LONGLONG_MIN : (longlong) ULONGLONG_MAX;

end_i:
  *endptr= (char*) s;
  return (negative ? ((longlong) -(long) i) : (longlong) i);

end_i_and_j:
  li= (ulonglong) i * lfactor[(size_t) (s-start) / 4] + j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end3:
  li= (ulonglong) i*LFACTOR+ (ulonglong) j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end4:
  li= (ulonglong) i*LFACTOR1+ (ulonglong) j * 10 + k;
  *endptr= (char*) s;
  if (negative)
  {
   if (li > MAX_NEGATIVE_NUMBER)
     goto overflow;
   return -((longlong) li);
  }
  return (longlong) li;

no_conv:
  /* There was no number to convert.  */
  *error= MY_ERRNO_EDOM;
  *endptr= (char *) nptr;
  return 0;
}


static size_t
my_numchars_utf32(CHARSET_INFO *cs __attribute__((unused)),
                  const char *b, const char *e)
{
  return (size_t) (e - b) / 4;
}


static size_t
my_charpos_utf32(CHARSET_INFO *cs __attribute__((unused)),
                 const char *b, const char *e, size_t pos)
{
  size_t string_length= (size_t) (e - b);
  return pos * 4 > string_length ? string_length + 4 : pos * 4;
}


static size_t
my_well_formed_len_utf32(CHARSET_INFO *cs __attribute__((unused)),
                         const char *b, const char *e,
                         size_t nchars, int *error)
{
  /* Ensure string length is divisible by 4 */
  const char *b0= b;
  size_t length= e - b;
  DBUG_ASSERT((length % 4) == 0);
  *error= 0;
  nchars*= 4;
  if (length > nchars)
  {
    length= nchars;
    e= b + nchars;
  }
  for (; b < e; b+= 4)
  {
    /* Don't accept characters greater than U+10FFFF */
    if (b[0] || (uchar) b[1] > 0x10)
    {
      *error= 1;
      return b - b0;
    }
  }
  return length;
}


static
void my_fill_utf32(CHARSET_INFO *cs,
                   char *s, size_t slen, int fill)
{
  char buf[10];
#ifndef DBUG_OFF
  uint buflen;
#endif
  char *e= s + slen;
  
  DBUG_ASSERT((slen % 4) == 0);

#ifndef DBUG_OFF
  buflen=
#endif
    cs->cset->wc_mb(cs, (my_wc_t) fill, (uchar*) buf,
                    (uchar*) buf + sizeof(buf));
  DBUG_ASSERT(buflen == 4);
  while (s < e)
  {
    memcpy(s, buf, 4);
    s+= 4;
  }
}


static size_t
my_lengthsp_utf32(CHARSET_INFO *cs __attribute__((unused)),
                  const char *ptr, size_t length)
{
  const char *end= ptr + length;
  DBUG_ASSERT((length % 4) == 0);
  while (end > ptr + 3 && end[-1] == ' ' && !end[-2] && !end[-3] && !end[-4])
    end-= 4;
  return (size_t) (end - ptr);
}


static int
my_wildcmp_utf32_ci(CHARSET_INFO *cs,
                    const char *str, const char *str_end,
                    const char *wildstr, const char *wildend,
                    int escape, int w_one, int w_many)
{
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  return my_wildcmp_unicode(cs, str, str_end, wildstr, wildend,
                            escape, w_one, w_many, uni_plane); 
}


static int
my_wildcmp_utf32_bin(CHARSET_INFO *cs,
                     const char *str,const char *str_end,
                     const char *wildstr,const char *wildend,
                     int escape, int w_one, int w_many)
{
  return my_wildcmp_unicode(cs, str, str_end, wildstr, wildend,
                            escape, w_one, w_many, NULL); 
}


static int
my_strnncoll_utf32_bin(CHARSET_INFO *cs, 
                       const uchar *s, size_t slen,
                       const uchar *t, size_t tlen,
                       my_bool t_is_prefix)
{
  my_wc_t UNINIT_VAR(s_wc), UNINIT_VAR(t_wc);
  const uchar *se= s + slen;
  const uchar *te= t + tlen;

  while (s < se && t < te)
  {
    int s_res= my_utf32_uni(cs, &s_wc, s, se);
    int t_res= my_utf32_uni(cs, &t_wc, t, te);
    
    if (s_res <= 0 || t_res <= 0)
    {
      /* Incorrect string, compare by char value */
      return my_bincmp(s, se, t, te);
    }
    if (s_wc != t_wc)
    {
      return  s_wc > t_wc ? 1 : -1;
    }
    
    s+= s_res;
    t+= t_res;
  }
  return (int) (t_is_prefix ? (t-te) : ((se - s) - (te - t)));
}


static inline my_wc_t
my_utf32_get(const uchar *s)
{
  return
    ((my_wc_t) s[0] << 24) +
    ((my_wc_t) s[1] << 16) +
    ((my_wc_t) s[2] << 8) +
    s[3];
}


static int
my_strnncollsp_utf32_bin(CHARSET_INFO *cs __attribute__((unused)), 
                         const uchar *s, size_t slen, 
                         const uchar *t, size_t tlen,
                         my_bool diff_if_only_endspace_difference
                         __attribute__((unused)))
{
  const uchar *se, *te;
  size_t minlen;

  DBUG_ASSERT((slen % 4) == 0);
  DBUG_ASSERT((tlen % 4) == 0);

  se= s + slen;
  te= t + tlen;

  for (minlen= min(slen, tlen); minlen; minlen-= 4)
  {
    my_wc_t s_wc= my_utf32_get(s);
    my_wc_t t_wc= my_utf32_get(t);
    if (s_wc != t_wc)
      return  s_wc > t_wc ? 1 : -1;

    s+= 4;
    t+= 4;
  }

  if (slen != tlen)
  {
    int swap= 1;
    if (slen < tlen)
    {
      s= t;
      se= te;
      swap= -1;
    }

    for ( ; s < se ; s+= 4)
    {
      my_wc_t s_wc= my_utf32_get(s);
      if (s_wc != ' ')
        return (s_wc < ' ') ? -swap : swap;
    }
  }
  return 0;
}


static size_t
my_scan_utf32(CHARSET_INFO *cs,
              const char *str, const char *end, int sequence_type)
{
  const char *str0= str;
  
  switch (sequence_type)
  {
  case MY_SEQ_SPACES:
    for ( ; str < end; )
    {
      my_wc_t wc;
      int res= my_utf32_uni(cs, &wc, (uchar*) str, (uchar*) end);
      if (res < 0 || wc != ' ')
        break;
      str+= res;
    }
    return (size_t) (str - str0);
  default:
    return 0;
  }
}


static MY_COLLATION_HANDLER my_collation_utf32_general_ci_handler =
{
  NULL, /* init */
  my_strnncoll_utf32,
  my_strnncollsp_utf32,
  my_strnxfrm_unicode,
  my_strnxfrmlen_utf32,
  my_like_range_generic,
  my_wildcmp_utf32_ci,
  my_strcasecmp_mb2_or_mb4,
  my_instr_mb,
  my_hash_sort_utf32,
  my_propagate_simple
};


static MY_COLLATION_HANDLER my_collation_utf32_bin_handler =
{
  NULL, /* init */
  my_strnncoll_utf32_bin,
  my_strnncollsp_utf32_bin,
  my_strnxfrm_unicode_full_bin,
  my_strnxfrmlen_unicode_full_bin,
  my_like_range_generic,
  my_wildcmp_utf32_bin,
  my_strcasecmp_mb2_or_mb4,
  my_instr_mb,
  my_hash_sort_utf32,
  my_propagate_simple
};


MY_CHARSET_HANDLER my_charset_utf32_handler=
{
  NULL, /* init */
  my_ismbchar_utf32,
  my_mbcharlen_utf32,
  my_numchars_utf32,
  my_charpos_utf32,
  my_well_formed_len_utf32,
  my_lengthsp_utf32,
  my_numcells_mb,
  my_utf32_uni,
  my_uni_utf32,
  my_mb_ctype_mb,
  my_caseup_str_mb2_or_mb4,
  my_casedn_str_mb2_or_mb4,
  my_caseup_utf32,
  my_casedn_utf32,
  my_snprintf_utf32,
  my_l10tostr_mb2_or_mb4,
  my_ll10tostr_mb2_or_mb4,
  my_fill_utf32,
  my_strntol_mb2_or_mb4,
  my_strntoul_mb2_or_mb4,
  my_strntoll_mb2_or_mb4,
  my_strntoull_mb2_or_mb4,
  my_strntod_mb2_or_mb4,
  my_strtoll10_utf32,
  my_strntoull10rnd_mb2_or_mb4,
  my_scan_utf32
};


struct charset_info_st my_charset_utf32_general_ci=
{
  60,0,0,              /* number       */
  MY_CS_COMPILED|MY_CS_PRIMARY|MY_CS_STRNXFRM|MY_CS_UNICODE|MY_CS_NONASCII,
  "utf32",             /* cs name    */
  "utf32_general_ci",  /* name         */
  "UTF-32 Unicode",    /* comment      */
  NULL,                /* tailoring    */
  NULL,                /* ctype        */
  NULL,                /* to_lower     */
  NULL,                /* to_upper     */
  NULL,                /* sort_order   */
  NULL,                /* contractions */
  NULL,                /* sort_order_big*/
  NULL,                /* tab_to_uni   */
  NULL,                /* tab_from_uni */
  my_unicase_default,  /* caseinfo     */
  NULL,                /* state_map    */
  NULL,                /* ident_map    */
  1,                   /* strxfrm_multiply */
  1,                   /* caseup_multiply  */
  1,                   /* casedn_multiply  */
  4,                   /* mbminlen     */
  4,                   /* mbmaxlen     */
  0,                   /* min_sort_char */
  0xFFFF,              /* max_sort_char */
  ' ',                 /* pad char      */
  0,                   /* escape_with_backslash_is_dangerous */
  &my_charset_utf32_handler,
  &my_collation_utf32_general_ci_handler
};


struct charset_info_st my_charset_utf32_bin=
{
  61,0,0,              /* number       */
  MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_UNICODE|MY_CS_NONASCII,
  "utf32",             /* cs name    */
  "utf32_bin",         /* name         */
  "UTF-32 Unicode",    /* comment      */
  NULL,                /* tailoring    */
  NULL,                /* ctype        */
  NULL,                /* to_lower     */
  NULL,                /* to_upper     */
  NULL,                /* sort_order   */
  NULL,                /* contractions */
  NULL,                /* sort_order_big*/
  NULL,                /* tab_to_uni   */
  NULL,                /* tab_from_uni */
  my_unicase_default,  /* caseinfo     */
  NULL,                /* state_map    */
  NULL,                /* ident_map    */
  1,                   /* strxfrm_multiply */
  1,                   /* caseup_multiply  */
  1,                   /* casedn_multiply  */
  4,                   /* mbminlen     */
  4,                   /* mbmaxlen     */
  0,                   /* min_sort_char */
  0xFFFF,              /* max_sort_char */
  ' ',                 /* pad char      */
  0,                   /* escape_with_backslash_is_dangerous */
  &my_charset_utf32_handler,
  &my_collation_utf32_bin_handler
};


#endif /* HAVE_CHARSET_utf32 */


#ifdef HAVE_CHARSET_ucs2

static const uchar ctype_ucs2[] = {
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
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static const uchar to_lower_ucs2[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
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

static const uchar to_upper_ucs2[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};


static int my_ucs2_uni(CHARSET_INFO *cs __attribute__((unused)),
		       my_wc_t * pwc, const uchar *s, const uchar *e)
{
  if (s+2 > e) /* Need 2 characters */
    return MY_CS_TOOSMALL2;
  
  *pwc= ((uchar)s[0]) * 256  + ((uchar)s[1]);
  return 2;
}

static int my_uni_ucs2(CHARSET_INFO *cs __attribute__((unused)) ,
		       my_wc_t wc, uchar *r, uchar *e)
{
  if ( r+2 > e ) 
    return MY_CS_TOOSMALL2;

  if (wc > 0xFFFF) /* UCS2 does not support characters outside BMP */
    return MY_CS_ILUNI;

  r[0]= (uchar) (wc >> 8);
  r[1]= (uchar) (wc & 0xFF);
  return 2;
}


static size_t my_caseup_ucs2(CHARSET_INFO *cs, char *src, size_t srclen,
                           char *dst __attribute__((unused)),
                           size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);
  
  while ((src < srcend) &&
         (res= my_ucs2_uni(cs, &wc, (uchar *)src, (uchar*) srcend)) > 0)
  {
    int plane= (wc>>8) & 0xFF;
    wc= uni_plane[plane] ? uni_plane[plane][wc & 0xFF].toupper : wc;
    if (res != my_uni_ucs2(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static void my_hash_sort_ucs2(CHARSET_INFO *cs, const uchar *s, size_t slen,
			      ulong *n1, ulong *n2)
{
  my_wc_t wc;
  int res;
  const uchar *e=s+slen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  while (e > s+1 && e[-1] == ' ' && e[-2] == '\0')
    e-= 2;

  while ((s < e) && (res=my_ucs2_uni(cs,&wc, (uchar *)s, (uchar*)e)) >0)
  {
    int plane = (wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].sort : wc;
    n1[0]^= (((n1[0] & 63)+n2[0])*(wc & 0xFF))+ (n1[0] << 8);
    n2[0]+=3;
    n1[0]^= (((n1[0] & 63)+n2[0])*(wc >> 8))+ (n1[0] << 8);
    n2[0]+=3;
    s+=res;
  }
}


static size_t my_casedn_ucs2(CHARSET_INFO *cs, char *src, size_t srclen,
                           char *dst __attribute__((unused)),
                           size_t dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  DBUG_ASSERT(src == dst && srclen == dstlen);

  while ((src < srcend) &&
         (res= my_ucs2_uni(cs, &wc, (uchar*) src, (uchar*) srcend)) > 0)
  {
    int plane= (wc>>8) & 0xFF;
    wc= uni_plane[plane] ? uni_plane[plane][wc & 0xFF].tolower : wc;
    if (res != my_uni_ucs2(cs, wc, (uchar*) src, (uchar*) srcend))
      break;
    src+= res;
  }
  return srclen;
}


static int my_strnncoll_ucs2(CHARSET_INFO *cs, 
			     const uchar *s, size_t slen, 
                             const uchar *t, size_t tlen,
                             my_bool t_is_prefix)
{
  int s_res,t_res;
  my_wc_t UNINIT_VAR(s_wc),UNINIT_VAR(t_wc);
  const uchar *se=s+slen;
  const uchar *te=t+tlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  while ( s < se && t < te )
  {
    int plane;
    s_res=my_ucs2_uni(cs,&s_wc, s, se);
    t_res=my_ucs2_uni(cs,&t_wc, t, te);
    
    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare by char value */
      return ((int)s[0]-(int)t[0]); 
    }
    
    plane=(s_wc>>8) & 0xFF;
    s_wc = uni_plane[plane] ? uni_plane[plane][s_wc & 0xFF].sort : s_wc;
    plane=(t_wc>>8) & 0xFF;
    t_wc = uni_plane[plane] ? uni_plane[plane][t_wc & 0xFF].sort : t_wc;
    if ( s_wc != t_wc )
    {
      return  s_wc > t_wc ? 1 : -1;
    }
    
    s+=s_res;
    t+=t_res;
  }
  return (int) (t_is_prefix ? t-te : ((se-s) - (te-t)));
}

/*
  Compare strings, discarding end space

  SYNOPSIS
    my_strnncollsp_ucs2()
    cs                  character set handler
    a                   First string to compare
    a_length            Length of 'a'
    b                   Second string to compare
    b_length            Length of 'b'

  IMPLEMENTATION
    If one string is shorter as the other, then we space extend the other
    so that the strings have equal length.

    This will ensure that the following things hold:

    "a"  == "a "
    "a\0" < "a"
    "a\0" < "a "

  RETURN
    < 0  a <  b
    = 0  a == b
    > 0  a > b
*/

static int my_strnncollsp_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                               const uchar *s, size_t slen,
                               const uchar *t, size_t tlen,
                               my_bool diff_if_only_endspace_difference
			       __attribute__((unused)))
{
  const uchar *se, *te;
  size_t minlen;
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;

  /* extra safety to make sure the lengths are even numbers */
  slen&= ~1;
  tlen&= ~1;

  se= s + slen;
  te= t + tlen;

  for (minlen= min(slen, tlen); minlen; minlen-= 2)
  {
    int s_wc = uni_plane[s[0]] ? (int) uni_plane[s[0]][s[1]].sort :
                                 (((int) s[0]) << 8) + (int) s[1];

    int t_wc = uni_plane[t[0]] ? (int) uni_plane[t[0]][t[1]].sort : 
                                 (((int) t[0]) << 8) + (int) t[1];
    if ( s_wc != t_wc )
      return  s_wc > t_wc ? 1 : -1;

    s+= 2;
    t+= 2;
  }

  if (slen != tlen)
  {
    int swap= 1;
    if (slen < tlen)
    {
      s= t;
      se= te;
      swap= -1;
    }

    for ( ; s < se ; s+= 2)
    {
      if (s[0] || s[1] != ' ')
        return (s[0] == 0 && s[1] < ' ') ? -swap : swap;
    }
  }
  return 0;
}


static uint my_ismbchar_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                             const char *b,
                             const char *e)
{
  return b + 2 > e ? 0 : 2;
}


static uint my_mbcharlen_ucs2(CHARSET_INFO *cs  __attribute__((unused)) , 
                              uint c __attribute__((unused)))
{
  return 2;
}


static
size_t my_numchars_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                        const char *b, const char *e)
{
  return (size_t) (e-b)/2;
}


static
size_t my_charpos_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                       const char *b  __attribute__((unused)),
                       const char *e  __attribute__((unused)),
                       size_t pos)
{
  size_t string_length= (size_t) (e - b);
  return pos > string_length ? string_length + 2 : pos * 2;
}


static
size_t my_well_formed_len_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                               const char *b, const char *e,
                               size_t nchars, int *error)
{
  /* Ensure string length is dividable with 2 */
  size_t nbytes= ((size_t) (e-b)) & ~(size_t) 1;
  *error= 0;
  nchars*= 2;
  return min(nbytes, nchars);
}


static
int my_wildcmp_ucs2_ci(CHARSET_INFO *cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  MY_UNICASE_INFO *const *uni_plane= cs->caseinfo;
  return my_wildcmp_unicode(cs,str,str_end,wildstr,wildend,
                            escape,w_one,w_many,uni_plane); 
}


static
int my_wildcmp_ucs2_bin(CHARSET_INFO *cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  return my_wildcmp_unicode(cs,str,str_end,wildstr,wildend,
                            escape,w_one,w_many,NULL); 
}


static
int my_strnncoll_ucs2_bin(CHARSET_INFO *cs, 
                          const uchar *s, size_t slen,
                          const uchar *t, size_t tlen,
                          my_bool t_is_prefix)
{
  int s_res,t_res;
  my_wc_t UNINIT_VAR(s_wc),UNINIT_VAR(t_wc);
  const uchar *se=s+slen;
  const uchar *te=t+tlen;

  while ( s < se && t < te )
  {
    s_res=my_ucs2_uni(cs,&s_wc, s, se);
    t_res=my_ucs2_uni(cs,&t_wc, t, te);
    
    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare by char value */
      return ((int)s[0]-(int)t[0]); 
    }
    if ( s_wc != t_wc )
    {
      return  s_wc > t_wc ? 1 : -1;
    }
    
    s+=s_res;
    t+=t_res;
  }
  return (int) (t_is_prefix ? t-te : ((se-s) - (te-t)));
}

static int my_strnncollsp_ucs2_bin(CHARSET_INFO *cs __attribute__((unused)), 
                                   const uchar *s, size_t slen, 
                                   const uchar *t, size_t tlen,
                                   my_bool diff_if_only_endspace_difference
                                   __attribute__((unused)))
{
  const uchar *se, *te;
  size_t minlen;

  /* extra safety to make sure the lengths are even numbers */
  slen= (slen >> 1) << 1;
  tlen= (tlen >> 1) << 1;

  se= s + slen;
  te= t + tlen;

  for (minlen= min(slen, tlen); minlen; minlen-= 2)
  {
    int s_wc= s[0] * 256 + s[1];
    int t_wc= t[0] * 256 + t[1];
    if ( s_wc != t_wc )
      return  s_wc > t_wc ? 1 : -1;

    s+= 2;
    t+= 2;
  }

  if (slen != tlen)
  {
    int swap= 1;
    if (slen < tlen)
    {
      s= t;
      se= te;
      swap= -1;
    }

    for ( ; s < se ; s+= 2)
    {
      if (s[0] || s[1] != ' ')
        return (s[0] == 0 && s[1] < ' ') ? -swap : swap;
    }
  }
  return 0;
}


static
void my_hash_sort_ucs2_bin(CHARSET_INFO *cs __attribute__((unused)),
			   const uchar *key, size_t len,ulong *nr1, ulong *nr2)
{
  const uchar *pos = key;
  
  key+= len;

  while (key > pos+1 && key[-1] == ' ' && key[-2] == '\0')
    key-= 2;

  for (; pos < (uchar*) key ; pos++)
  {
    nr1[0]^=(ulong) ((((uint) nr1[0] & 63)+nr2[0]) * 
	     ((uint)*pos)) + (nr1[0] << 8);
    nr2[0]+=3;
  }
}


static MY_COLLATION_HANDLER my_collation_ucs2_general_ci_handler =
{
    NULL,		/* init */
    my_strnncoll_ucs2,
    my_strnncollsp_ucs2,
    my_strnxfrm_unicode,
    my_strnxfrmlen_simple,
    my_like_range_generic,
    my_wildcmp_ucs2_ci,
    my_strcasecmp_mb2_or_mb4,
    my_instr_mb,
    my_hash_sort_ucs2,
    my_propagate_simple
};


static MY_COLLATION_HANDLER my_collation_ucs2_bin_handler =
{
    NULL,		/* init */
    my_strnncoll_ucs2_bin,
    my_strnncollsp_ucs2_bin,
    my_strnxfrm_unicode,
    my_strnxfrmlen_simple,
    my_like_range_generic,
    my_wildcmp_ucs2_bin,
    my_strcasecmp_mb2_or_mb4,
    my_instr_mb,
    my_hash_sort_ucs2_bin,
    my_propagate_simple
};


MY_CHARSET_HANDLER my_charset_ucs2_handler=
{
    NULL,		/* init */
    my_ismbchar_ucs2,	/* ismbchar     */
    my_mbcharlen_ucs2,	/* mbcharlen    */
    my_numchars_ucs2,
    my_charpos_ucs2,
    my_well_formed_len_ucs2,
    my_lengthsp_mb2,
    my_numcells_mb,
    my_ucs2_uni,	/* mb_wc        */
    my_uni_ucs2,	/* wc_mb        */
    my_mb_ctype_mb,
    my_caseup_str_mb2_or_mb4,
    my_casedn_str_mb2_or_mb4,
    my_caseup_ucs2,
    my_casedn_ucs2,
    my_snprintf_mb2,
    my_l10tostr_mb2_or_mb4,
    my_ll10tostr_mb2_or_mb4,
    my_fill_mb2,
    my_strntol_mb2_or_mb4,
    my_strntoul_mb2_or_mb4,
    my_strntoll_mb2_or_mb4,
    my_strntoull_mb2_or_mb4,
    my_strntod_mb2_or_mb4,
    my_strtoll10_mb2,
    my_strntoull10rnd_mb2_or_mb4,
    my_scan_mb2
};


struct charset_info_st my_charset_ucs2_general_ci=
{
    35,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_PRIMARY|MY_CS_STRNXFRM|MY_CS_UNICODE|MY_CS_NONASCII,
    "ucs2",		/* cs name    */
    "ucs2_general_ci",	/* name         */
    "",			/* comment      */
    NULL,		/* tailoring    */
    ctype_ucs2,		/* ctype        */
    to_lower_ucs2,	/* to_lower     */
    to_upper_ucs2,	/* to_upper     */
    to_upper_ucs2,	/* sort_order   */
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    1,			/* strxfrm_multiply */
    1,                  /* caseup_multiply  */
    1,                  /* casedn_multiply  */
    2,			/* mbminlen     */
    2,			/* mbmaxlen     */
    0,			/* min_sort_char */
    0xFFFF,		/* max_sort_char */
    ' ',                /* pad char      */
    0,                  /* escape_with_backslash_is_dangerous */
    &my_charset_ucs2_handler,
    &my_collation_ucs2_general_ci_handler
};


struct charset_info_st my_charset_ucs2_general_mysql500_ci=
{
  159, 0, 0,                                       /* number           */
  MY_CS_COMPILED|MY_CS_STRNXFRM|MY_CS_UNICODE|MY_CS_NONASCII, /* state */
  "ucs2",                                          /* cs name          */
  "ucs2_general_mysql500_ci",                      /* name             */
  "",                                              /* comment          */
  NULL,                                            /* tailoring        */
  ctype_ucs2,                                      /* ctype            */
  to_lower_ucs2,                                   /* to_lower         */
  to_upper_ucs2,                                   /* to_upper         */
  to_upper_ucs2,                                   /* sort_order       */
  NULL,                                            /* contractions     */
  NULL,                                            /* sort_order_big   */
  NULL,                                            /* tab_to_uni       */
  NULL,                                            /* tab_from_uni     */
  my_unicase_mysql500,                             /* caseinfo         */
  NULL,                                            /* state_map        */
  NULL,                                            /* ident_map        */
  1,                                               /* strxfrm_multiply */
  1,                                               /* caseup_multiply  */
  1,                                               /* casedn_multiply  */
  2,                                               /* mbminlen         */
  2,                                               /* mbmaxlen         */
  0,                                               /* min_sort_char    */
  0xFFFF,                                          /* max_sort_char    */
  ' ',                                             /* pad char         */
  0,                          /* escape_with_backslash_is_dangerous    */
  &my_charset_ucs2_handler,
  &my_collation_ucs2_general_ci_handler
};


struct charset_info_st my_charset_ucs2_bin=
{
    90,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_UNICODE|MY_CS_NONASCII,
    "ucs2",		/* cs name    */
    "ucs2_bin",		/* name         */
    "",			/* comment      */
    NULL,		/* tailoring    */
    ctype_ucs2,		/* ctype        */
    to_lower_ucs2,	/* to_lower     */
    to_upper_ucs2,	/* to_upper     */
    NULL,		/* sort_order   */
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    1,			/* strxfrm_multiply */
    1,                  /* caseup_multiply  */
    1,                  /* casedn_multiply  */
    2,			/* mbminlen     */
    2,			/* mbmaxlen     */
    0,			/* min_sort_char */
    0xFFFF,		/* max_sort_char */
    ' ',                /* pad char      */
    0,                  /* escape_with_backslash_is_dangerous */
    &my_charset_ucs2_handler,
    &my_collation_ucs2_bin_handler
};


#endif /* HAVE_CHARSET_ucs2 */
