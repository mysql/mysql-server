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
#include "m_string.h"

#ifdef USE_MB


void my_caseup_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str); /* BAR TODO: remove strlen() call */
  register uchar *map=cs->to_upper;
  
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    { 
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_casedn_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str);
  register uchar *map=cs->to_lower;
  
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_caseup_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  register uchar *map=cs->to_upper;
  
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else 
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

void my_casedn_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  register uchar *map=cs->to_lower;
  
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char) map[(uchar)*str];
      str++;
    }
  }
}

int my_strcasecmp_mb(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uint32 l;
  register const char *end=s+strlen(s);
  register uchar *map=cs->to_upper;
  
  while (s<end)
  {
    if ((l=my_ismbchar(cs, s,end)))
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
  return *t;
}


/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#define INC_PTR(cs,A,B) A+=((use_mb_flag && \
                          my_ismbchar(cs,A,B)) ? my_ismbchar(cs,A,B) : 1)

#define likeconv(s,A) (uchar) (s)->sort_order[(uchar) (A)]

int my_wildcmp_mb(CHARSET_INFO *cs,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */

  bool use_mb_flag=use_mb(cs);

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      int l;
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (use_mb_flag &&
          (l = my_ismbchar(cs, wildstr, wildend)))
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
      int mblen=0;
      
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
      LINT_INIT(mblen);
      if (use_mb_flag)
        mblen = my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);   
      do
      {
        if (use_mb_flag)
	{
          for (;;)
          {
            if (str >= str_end)
              return -1;
            if (mblen)
            {
              if (str+mblen <= str_end && memcmp(str, mb, mblen) == 0)
              {
                str += mblen;
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
	}
        else
        {
          while (str != str_end && likeconv(cs,*str) != cmp)
            str++;
          if (str++ == str_end) return (-1);
        }
	{
	  int tmp=my_wildcmp_mb(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}

uint my_numchars_mb(CHARSET_INFO *cs __attribute__((unused)),
		      const char *b, const char *e)
{
  register uint32 n=0,mblen;
  while (b < e) 
  {
    b+= (mblen= my_ismbchar(cs,b,e)) ? mblen : 1;
    ++n;
  }
  return n;
}

uint my_charpos_mb(CHARSET_INFO *cs __attribute__((unused)),
		     const char *b, const char *e, uint pos)
{
  uint mblen;
  const char *b0=b;
  
  while (pos && b<e)
  {
    b+= (mblen= my_ismbchar(cs,b,e)) ? mblen : 1;
    pos--;
  }
  return b-b0;
}

uint my_instr_mb(CHARSET_INFO *cs,
                 const char *b, uint b_length, 
                 const char *s, uint s_length,
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
        match->mblen= 0;
      }
      return 1;		/* Empty string is always found */
    }
    
    b0= b;
    end= b+b_length-s_length+1;
    
    while (b < end)
    {
      int mblen;
      
      if (!cs->coll->strnncoll(cs, (unsigned char*) b,   s_length, 
      				   (unsigned char*) s, s_length))
      {
        if (nmatch)
        {
          match[0].beg= 0;
          match[0].end= b-b0;
          match[0].mblen= res;
          if (nmatch > 1)
          {
            match[1].beg= match[0].end;
            match[1].end= match[0].end+s_length;
            match[1].mblen= 0;	/* Not computed */
          }
        }
        return 2;
      }
      mblen= (mblen= my_ismbchar(cs, b, end)) ? mblen : 1;
      b+= mblen;
      b_length-= mblen;
      res++;
    }
  }
  return 0;
}

/* BINARY collations handlers for MB charsets */

static int my_strnncoll_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
				const uchar *s, uint slen,
				const uchar *t, uint tlen)
{
  int cmp= memcmp(s,t,min(slen,tlen));
  return cmp ? cmp : (int) (slen - tlen);
}

static int my_strnncollsp_mb_bin(CHARSET_INFO * cs,
                               const uchar *s, uint slen,
                               const uchar *t, uint tlen)
{
  int len, cmp;

  for ( ; slen && my_isspace(cs, s[slen-1]) ; slen--);
  for ( ; tlen && my_isspace(cs, t[tlen-1]) ; tlen--);

  len  = ( slen > tlen ) ? tlen : slen;

  cmp= memcmp(s,t,len);
  return cmp ? cmp : (int) (slen - tlen);
}

static int my_strnxfrm_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
			    uchar * dest, uint len,
			    const uchar *src, 
			    uint srclen __attribute__((unused)))
{
  if (dest != src)
    memcpy(dest,src,len= min(len,srclen));
  return len;
}


static int my_strcasecmp_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
		      const char *s, const char *t)
{
  return strcmp(s,t);
}

static void my_hash_sort_mb_bin(CHARSET_INFO *cs __attribute__((unused)),
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

static int my_wildcmp_mb_bin(CHARSET_INFO *cs,
		  const char *str,const char *str_end,
		  const char *wildstr,const char *wildend,
		  int escape, int w_one, int w_many)
{
  int result= -1;				/* Not found, using wildcards */

  bool use_mb_flag=use_mb(cs);

  while (wildstr != wildend)
  {
    while (*wildstr != w_many && *wildstr != w_one)
    {
      int l;
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (use_mb_flag &&
          (l = my_ismbchar(cs, wildstr, wildend)))
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
      int mblen=0;
      
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
      LINT_INIT(mblen);
      if (use_mb_flag)
        mblen = my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      do
      {
        if (use_mb_flag)
	{
          for (;;)
          {
            if (str >= str_end)
              return -1;
            if (mblen)
            {
              if (str+mblen <= str_end && memcmp(str, mb, mblen) == 0)
              {
                str += mblen;
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
	}
        else
        {
          while (str != str_end && *str != cmp)
            str++;
          if (str++ == str_end) return (-1);
        }
	{
	  int tmp=my_wildcmp_mb(cs,str,str_end,wildstr,wildend,escape,w_one,w_many);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != w_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}


MY_COLLATION_HANDLER my_collation_mb_bin_handler =
{
    my_strnncoll_mb_bin,
    my_strnncollsp_mb_bin,
    my_strnxfrm_mb_bin,
    my_like_range_simple,
    my_wildcmp_mb_bin,
    my_strcasecmp_mb_bin,
    my_instr_mb,
    my_hash_sort_mb_bin
};


#endif
