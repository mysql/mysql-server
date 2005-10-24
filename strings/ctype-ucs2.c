/* Copyright (C) 2000 MySQL AB
   
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

/* UCS2 support. Written by Alexander Barkov <bar@mysql.com> */

#include <my_global.h>
#include <my_sys.h>
#include "m_string.h"
#include "m_ctype.h"
#include <errno.h>


#ifdef HAVE_CHARSET_ucs2

#ifndef EILSEQ
#define EILSEQ ENOENT
#endif


static uchar ctype_ucs2[] = {
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

static uchar to_lower_ucs2[] = {
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

static uchar to_upper_ucs2[] = {
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
    return MY_CS_TOOFEW(0);
  
  *pwc= ((unsigned char)s[0]) * 256  + ((unsigned char)s[1]);
  return 2;
}

static int my_uni_ucs2(CHARSET_INFO *cs __attribute__((unused)) ,
		       my_wc_t wc, uchar *r, uchar *e)
{
  if ( r+2 > e ) 
    return MY_CS_TOOSMALL;
  
  r[0]= (uchar) (wc >> 8);
  r[1]= (uchar) (wc & 0xFF);
  return 2;
}


static uint my_caseup_ucs2(CHARSET_INFO *cs, char *src, uint srclen,
                           char *dst __attribute__((unused)),
                           uint dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;
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


static void my_hash_sort_ucs2(CHARSET_INFO *cs, const uchar *s, uint slen,
			      ulong *n1, ulong *n2)
{
  my_wc_t wc;
  int res;
  const uchar *e=s+slen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;

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


static void my_caseup_str_ucs2(CHARSET_INFO * cs  __attribute__((unused)), 
			       char * s __attribute__((unused)))
{
}



static uint my_casedn_ucs2(CHARSET_INFO *cs, char *src, uint srclen,
                           char *dst __attribute__((unused)),
                           uint dstlen __attribute__((unused)))
{
  my_wc_t wc;
  int res;
  char *srcend= src + srclen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;
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

static void my_casedn_str_ucs2(CHARSET_INFO *cs __attribute__((unused)), 
			       char * s __attribute__((unused)))
{
}


static int my_strnncoll_ucs2(CHARSET_INFO *cs, 
			     const uchar *s, uint slen, 
                             const uchar *t, uint tlen,
                             my_bool t_is_prefix)
{
  int s_res,t_res;
  my_wc_t s_wc,t_wc;
  const uchar *se=s+slen;
  const uchar *te=t+tlen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;

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
                               const uchar *s, uint slen,
                               const uchar *t, uint tlen,
                               my_bool diff_if_only_endspace_difference
			       __attribute__((unused)))
{
  const uchar *se, *te;
  uint minlen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;

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


static int my_strncasecmp_ucs2(CHARSET_INFO *cs,
			       const char *s, const char *t,  uint len)
{
  int s_res,t_res;
  my_wc_t s_wc,t_wc;
  const char *se=s+len;
  const char *te=t+len;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;
  
  while ( s < se && t < te )
  {
    int plane;
    
    s_res=my_ucs2_uni(cs,&s_wc, (const uchar*)s, (const uchar*)se);
    t_res=my_ucs2_uni(cs,&t_wc, (const uchar*)t, (const uchar*)te);
    
    if ( s_res <= 0 || t_res <= 0 )
    {
      /* Incorrect string, compare by char value */
      return ((int)s[0]-(int)t[0]); 
    }
    
    plane=(s_wc>>8) & 0xFF;
    s_wc = uni_plane[plane] ? uni_plane[plane][s_wc & 0xFF].tolower : s_wc;

    plane=(t_wc>>8) & 0xFF;
    t_wc = uni_plane[plane] ? uni_plane[plane][t_wc & 0xFF].tolower : t_wc;
    
    if ( s_wc != t_wc )
      return  ((int) s_wc) - ((int) t_wc);
    
    s+=s_res;
    t+=t_res;
  }
  return (int) ( (se-s) - (te-t) );
}


static int my_strcasecmp_ucs2(CHARSET_INFO *cs, const char *s, const char *t)
{
  uint s_len= (uint) strlen(s);
  uint t_len= (uint) strlen(t);
  uint len = (s_len > t_len) ? s_len : t_len;
  return  my_strncasecmp_ucs2(cs, s, t, len);
}


static int my_strnxfrm_ucs2(CHARSET_INFO *cs, 
	uchar *dst, uint dstlen, const uchar *src, uint srclen)
{
  my_wc_t wc;
  int res;
  int plane;
  uchar *de = dst + dstlen;
  const uchar *se = src + srclen;
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;

  while( src < se && dst < de )
  {
    if ((res=my_ucs2_uni(cs,&wc, src, se))<0)
    {
      break;
    }
    src+=res;
    srclen-=res;
    
    plane=(wc>>8) & 0xFF;
    wc = uni_plane[plane] ? uni_plane[plane][wc & 0xFF].sort : wc;
    
    if ((res=my_uni_ucs2(cs,wc,dst,de)) <0)
    {
      break;
    }
    dst+=res;
  }
  if (dst < de)
    cs->cset->fill(cs, (char*) dst, (uint) (de - dst), ' ');
  return dstlen;
}


static int my_ismbchar_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                     const char *b __attribute__((unused)),
                     const char *e __attribute__((unused)))
{
  return 2;
}


static int my_mbcharlen_ucs2(CHARSET_INFO *cs  __attribute__((unused)) , 
                      uint c __attribute__((unused)))
{
  return 2;
}


#include <m_string.h>
#include <stdarg.h>

static int my_vsnprintf_ucs2(char *dst, uint n, const char* fmt, va_list ap)
{
  char *start=dst, *end=dst+n-1;
  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (dst == end)			/* End of buffer */
	break;
      
      *dst++='\0'; *dst++= *fmt;	/* Copy ordinary char */
      continue;
    }
    
    fmt++;
    
    /* Skip if max size is used (to be compatible with printf) */
    while ( (*fmt>='0' && *fmt<='9') || *fmt == '.' || *fmt == '-')
      fmt++;
    
    if (*fmt == 'l')
      fmt++;
    
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      uint plen;
      uint left_len = (uint)(end-dst);
      if (!par) par = (char*)"(null)";
      plen = (uint) strlen(par);
      if (left_len <= plen*2)
	plen = left_len/2 - 1;

      for ( ; plen ; plen--, dst+=2, par++)
      {
        dst[0]='\0';
        dst[1]=par[0];
      }
      continue;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      char nbuf[16];
      char *pbuf=nbuf;
      
      if ((uint) (end-dst) < 32)
	break;
      iarg = va_arg(ap, int);
      if (*fmt == 'd')
	int10_to_str((long) iarg, nbuf, -10);
      else
	int10_to_str((long) (uint) iarg,nbuf,10);

      for (; pbuf[0]; pbuf++)
      {
        *dst++='\0';
        *dst++=*pbuf;
      }
      continue;
    }
    
    /* We come here on '%%', unknown code or too long parameter */
    if (dst == end)
      break;
    *dst++='\0';
    *dst++='%';				/* % used as % or unknown code */
  }
  
  DBUG_ASSERT(dst <= end);
  *dst='\0';				/* End of errmessage */
  return (uint) (dst - start);
}

static int my_snprintf_ucs2(CHARSET_INFO *cs __attribute__((unused)),
			    char* to, uint n, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  return my_vsnprintf_ucs2(to, n, fmt, args);
}


long my_strntol_ucs2(CHARSET_INFO *cs,
		     const char *nptr, uint l, int base,
		     char **endptr, int *err)
{
  int      negative=0;
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

#ifdef NOT_USED  
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif
  
  overflow = 0;
  res = 0;
  save = s;
  cutoff = ((uint32)~0L) / (uint32) base;
  cutlim = (uint) (((uint32)~0L) % (uint32) base);
  
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
        res *= (uint32) base;
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
    if (res > (uint32) INT_MIN32)
      overflow = 1;
  }
  else if (res > INT_MAX32)
    overflow = 1;
  
  if (overflow)
  {
    err[0]=ERANGE;
    return negative ? INT_MIN32 : INT_MAX32;
  }
  
  return (negative ? -((long) res) : (long) res);
}


ulong my_strntoul_ucs2(CHARSET_INFO *cs,
		       const char *nptr, uint l, int base, 
		       char **endptr, int *err)
{
  int      negative=0;
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

#ifdef NOT_USED
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

  overflow = 0;
  res = 0;
  save = s;
  cutoff = ((uint32)~0L) / (uint32) base;
  cutlim = (uint) (((uint32)~0L) % (uint32) base);
  
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
        res *= (uint32) base;
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
  
  if (overflow)
  {
    err[0]=(ERANGE);
    return (~(uint32) 0);
  }
  
  return (negative ? -((long) res) : (long) res);
}



longlong  my_strntoll_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
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

#ifdef NOT_USED  
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

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




ulonglong  my_strntoull_ucs2(CHARSET_INFO *cs,
			   const char *nptr, uint l, int base,
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
      err[0]= (cnv==MY_CS_ILSEQ) ? EILSEQ : EDOM;
      return 0;
    } 
    s+=cnv;
  } while (1);
  
bs:
  
#ifdef NOT_USED
  if (base <= 0 || base == 1 || base > 36)
    base = 10;
#endif

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


double my_strntod_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                       char *nptr, uint length, 
                       char **endptr, int *err)
{
  char     buf[256];
  double   res;
  register char *b=buf;
  register const uchar *s= (const uchar*) nptr;
  const uchar *end;
  my_wc_t  wc;
  int      cnv;

  *err= 0;
  /* Cut too long strings */
  if (length >= sizeof(buf))
    length= sizeof(buf)-1;
  end= s+length;

  while ((cnv=cs->cset->mb_wc(cs,&wc,s,end)) > 0)
  {
    s+=cnv;
    if (wc > (int) (uchar) 'e' || !wc)
      break;					/* Can't be part of double */
    *b++= (char) wc;
  }

  *endptr= b;
  res= my_strtod(buf, endptr, err);
  *endptr= nptr + (uint) (*endptr- buf);
  return res;
}


/*
  This is a fast version optimized for the case of radix 10 / -10
*/

int my_l10tostr_ucs2(CHARSET_INFO *cs,
		     char *dst, uint len, int radix, long int val)
{
  char buffer[66];
  register char *p, *db, *de;
  long int new_val;
  int  sl=0;
  
  p = &buffer[sizeof(buffer)-1];
  *p='\0';
  
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
  
  for ( db=dst, de=dst+len ; (dst<de) && *p ; p++)
  {
    int cnvres=cs->cset->wc_mb(cs,(my_wc_t)p[0],(uchar*) dst, (uchar*) de);
    if (cnvres>0)
      dst+=cnvres;
    else
      break;
  }
  return (int) (dst-db);
}

int my_ll10tostr_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		      char *dst, uint len, int radix, longlong val)
{
  char buffer[65];
  register char *p, *db, *de;
  long long_val;
  int  sl=0;
  
  if (radix < 0)
  {
    if (val < 0)
    {
      sl=1;
      val = -val;
    }
  }
  
  p = &buffer[sizeof(buffer)-1];
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
    *--p = (char) ('0' + (long_val - quo*10));
    long_val= quo;
  }
  
cnv:
  if (sl)
  {
    *--p='-';
  }
  
  for ( db=dst, de=dst+len ; (dst<de) && *p ; p++)
  {
    int cnvres=cs->cset->wc_mb(cs, (my_wc_t) p[0], (uchar*) dst, (uchar*) de);
    if (cnvres>0)
      dst+=cnvres;
    else
      break;
  }
  return (int) (dst-db);
}


#undef  ULONGLONG_MAX
#define ULONGLONG_MAX		(~(ulonglong) 0)
#define MAX_NEGATIVE_NUMBER	((ulonglong) LL(0x8000000000000000))
#define INIT_CNT  9
#define LFACTOR   ULL(1000000000)
#define LFACTOR1  ULL(10000000000)
#define LFACTOR2  ULL(100000000000)

static unsigned long lfactor[9]=
{
  1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L, 100000000L
};


longlong my_strtoll10_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                           const char *nptr, char **endptr, int *error)
{
  const char *s, *end, *start, *n_end, *true_end;
  unsigned char c;
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

  /* Check for a sign.	*/
  negative= 0;
  if (!s[0] && s[1] == '-')
  {
    *error= -1;					/* Mark as negative number */
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
	goto end_i;				/* Return 0 */
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
  start= s;				/* Used to know how much to shift i */
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

overflow:					/* *endptr is set here */
  *error= MY_ERRNO_ERANGE;
  return negative ? LONGLONG_MIN : (longlong) ULONGLONG_MAX;

end_i:
  *endptr= (char*) s;
  return (negative ? ((longlong) -(long) i) : (longlong) i);

end_i_and_j:
  li= (ulonglong) i * lfactor[(uint) (s-start) / 2] + j;
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


static
uint my_numchars_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  return (uint) (e-b)/2;
}


static
uint my_charpos_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		     const char *b  __attribute__((unused)),
		     const char *e  __attribute__((unused)),
		     uint pos)
{
  uint string_length= (uint) (e - b);
  return pos > string_length ? string_length + 2 : pos * 2;
}


static
uint my_well_formed_len_ucs2(CHARSET_INFO *cs __attribute__((unused)),
                             const char *b, const char *e,
                             uint nchars, int *error)
{
  /* Ensure string length is dividable with 2 */
  uint nbytes= ((uint) (e-b)) & ~(uint) 1;
  *error= 0;
  nchars*= 2;
  return min(nbytes, nchars);
}


static
void my_fill_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		   char *s, uint l, int fill)
{
  for ( ; l >= 2; s[0]= 0, s[1]= fill, s+=2, l-=2);
}


static
uint my_lengthsp_ucs2(CHARSET_INFO *cs __attribute__((unused)),
		      const char *ptr, uint length)
{
  const char *end= ptr+length;
  while (end > ptr+1 && end[-1] == ' ' && end[-2] == '\0')
    end-=2;
  return (uint) (end-ptr);
}


static
int my_wildcmp_ucs2_ci(CHARSET_INFO *cs,
		    const char *str,const char *str_end,
		    const char *wildstr,const char *wildend,
		    int escape, int w_one, int w_many)
{
  MY_UNICASE_INFO **uni_plane= cs->caseinfo;
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
                          const uchar *s, uint slen,
                          const uchar *t, uint tlen,
                          my_bool t_is_prefix)
{
  int s_res,t_res;
  my_wc_t s_wc,t_wc;
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

static int my_strnncollsp_ucs2_bin(CHARSET_INFO *cs, 
                                   const uchar *s, uint slen, 
                                   const uchar *t, uint tlen,
                                   my_bool diff_if_only_endspace_difference
                                   __attribute__((unused)))
{
  /* TODO: Needs to be fixed to handle end space! */
  return my_strnncoll_ucs2_bin(cs,s,slen,t,tlen,0);
}


static
int my_strcasecmp_ucs2_bin(CHARSET_INFO *cs, const char *s, const char *t)
{
  uint s_len= (uint) strlen(s);
  uint t_len= (uint) strlen(t);
  uint len = (s_len > t_len) ? s_len : t_len;
  return  my_strncasecmp_ucs2(cs, s, t, len);
}


static
int my_strnxfrm_ucs2_bin(CHARSET_INFO *cs __attribute__((unused)),
			 uchar *dst, uint dstlen,
			 const uchar *src, uint srclen)
{
  if (dst != src)
    memcpy(dst,src,srclen= min(dstlen,srclen));
  if (dstlen > srclen)
    cs->cset->fill(cs, (char*) dst + srclen, dstlen - srclen, ' ');
  return dstlen;
}


static
void my_hash_sort_ucs2_bin(CHARSET_INFO *cs __attribute__((unused)),
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

my_bool my_like_range_ucs2(CHARSET_INFO *cs,
			   const char *ptr,uint ptr_length,
			   pbool escape, pbool w_one, pbool w_many,
			   uint res_length,
			   char *min_str,char *max_str,
			   uint *min_length,uint *max_length)
{
  const char *end=ptr+ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;
  uint charlen= res_length / cs->mbmaxlen;
  
  for ( ; ptr + 1 < end && min_str + 1 < min_end && charlen > 0
        ; ptr+=2, charlen--)
  {
    if (ptr[0] == '\0' && ptr[1] == escape && ptr + 1 < end)
    {
      ptr+=2;					/* Skip escape */
      *min_str++= *max_str++ = ptr[0];
      *min_str++= *max_str++ = ptr[1];
      continue;
    }
    if (ptr[0] == '\0' && ptr[1] == w_one)	/* '_' in SQL */
    {
      *min_str++= (char) (cs->min_sort_char >> 8);
      *min_str++= (char) (cs->min_sort_char & 255);
      *max_str++= (char) (cs->max_sort_char >> 8);
      *max_str++= (char) (cs->max_sort_char & 255);
      continue;
    }
    if (ptr[0] == '\0' && ptr[1] == w_many)	/* '%' in SQL */
    {
      /*
        Calculate length of keys:
        'a\0\0... is the smallest possible string when we have space expand
        a\ff\ff... is the biggest possible string
      */
      *min_length= ((cs->state & MY_CS_BINSORT) ? (uint) (min_str - min_org) :
                    res_length);
      *max_length= res_length;
      do {
        *min_str++ = 0;
	*min_str++ = 0;
	*max_str++ = (char) (cs->max_sort_char >> 8);
	*max_str++ = (char) (cs->max_sort_char & 255);
      } while (min_str + 1 < min_end);
      return 0;
    }
    *min_str++= *max_str++ = ptr[0];
    *min_str++= *max_str++ = ptr[1];
  }

  /* Temporary fix for handling w_one at end of string (key compression) */
  {
    char *tmp;
    for (tmp= min_str ; tmp-1 > min_org && tmp[-1] == '\0' && tmp[-2]=='\0';)
    {
      *--tmp=' ';
      *--tmp='\0';
    }
  }
  
  *min_length= *max_length = (uint) (min_str - min_org);
  while (min_str + 1 < min_end)
  {
    *min_str++ = *max_str++ = '\0';
    *min_str++ = *max_str++ = ' ';      /* Because if key compression */
  }
  return 0;
}



ulong my_scan_ucs2(CHARSET_INFO *cs __attribute__((unused)),
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
    return (ulong) (str - str0);
  default:
    return 0;
  }
}



static MY_COLLATION_HANDLER my_collation_ucs2_general_ci_handler =
{
    NULL,		/* init */
    my_strnncoll_ucs2,
    my_strnncollsp_ucs2,
    my_strnxfrm_ucs2,
    my_strnxfrmlen_simple,
    my_like_range_ucs2,
    my_wildcmp_ucs2_ci,
    my_strcasecmp_ucs2,
    my_instr_mb,
    my_hash_sort_ucs2,
    my_propagate_simple
};


static MY_COLLATION_HANDLER my_collation_ucs2_bin_handler =
{
    NULL,		/* init */
    my_strnncoll_ucs2_bin,
    my_strnncollsp_ucs2_bin,
    my_strnxfrm_ucs2_bin,
    my_strnxfrmlen_simple,
    my_like_range_simple,
    my_wildcmp_ucs2_bin,
    my_strcasecmp_ucs2_bin,
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
    my_lengthsp_ucs2,
    my_numcells_mb,
    my_ucs2_uni,	/* mb_wc        */
    my_uni_ucs2,	/* wc_mb        */
    my_caseup_str_ucs2,
    my_casedn_str_ucs2,
    my_caseup_ucs2,
    my_casedn_ucs2,
    my_snprintf_ucs2,
    my_l10tostr_ucs2,
    my_ll10tostr_ucs2,
    my_fill_ucs2,
    my_strntol_ucs2,
    my_strntoul_ucs2,
    my_strntoll_ucs2,
    my_strntoull_ucs2,
    my_strntod_ucs2,
    my_strtoll10_ucs2,
    my_scan_ucs2
};


CHARSET_INFO my_charset_ucs2_general_ci=
{
    35,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_PRIMARY|MY_CS_STRNXFRM|MY_CS_UNICODE,
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

CHARSET_INFO my_charset_ucs2_bin=
{
    90,0,0,		/* number       */
    MY_CS_COMPILED|MY_CS_BINSORT|MY_CS_UNICODE,
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


#endif
