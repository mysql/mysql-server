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
      mblen= my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      cmp=likeconv(cs,cmp);   
      do
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


uint my_numchars_mb(CHARSET_INFO *cs __attribute__((unused)),
		      const char *pos, const char *end)
{
  register uint32 count=0;
  while (pos < end) 
  {
    uint mblen;
    pos+= (mblen= my_ismbchar(cs,pos,end)) ? mblen : 1;
    count++;
  }
  return count;
}


uint my_charpos_mb(CHARSET_INFO *cs __attribute__((unused)),
		     const char *pos, const char *end, uint length)
{
  const char *start= pos;
  
  while (length && pos < end)
  {
    uint mblen;
    pos+= (mblen= my_ismbchar(cs, pos, end)) ? mblen : 1;
    length--;
  }
  return length ? end+2-start : pos-start;
}


uint my_well_formed_len_mb(CHARSET_INFO *cs,
			   const char *b, const char *e, uint pos)
{
  const char *b_start= b;
  
  while (pos)
  {
    my_wc_t wc;
    int mblen;

    if ((mblen= cs->cset->mb_wc(cs, &wc, (uchar*) b, (uchar*) e)) <0)
      break;
    b+= mblen;
    pos--;
  }
  return b - b_start;
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
      				   (unsigned char*) s, s_length, 0))
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
				const uchar *t, uint tlen,
                                my_bool t_is_prefix)
{
  uint len=min(slen,tlen);
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

  NOTE
   This function is used for character strings with binary collations.
   The shorter string is extended with end space to be as long as the longer
   one.

  RETURN
    A negative number if s < t
    A positive number if s > t
    0 if strings are equal
*/

static int my_strnncollsp_mb_bin(CHARSET_INFO * cs __attribute__((unused)),
                                 const uchar *a, uint a_length, 
                                 const uchar *b, uint b_length)
{
  const uchar *end;
  uint length;
  
  end= a + (length= min(a_length, b_length));
  while (a < end)
  {
    if (*a++ != *b++)
      return ((int) a[-1] - (int) b[-1]);
  }
  if (a_length != b_length)
  {
    int swap= 0;
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
    }
    for (end= a + a_length-length; a < end ; a++)
    {
      if (*a != ' ')
	return ((int) *a - (int) ' ') ^ swap;
    }
  }
  return 0;
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
			 const char *ptr,uint ptr_length,
			 pbool escape, pbool w_one, pbool w_many,
			 uint res_length,
			 char *min_str,char *max_str,
			 uint *min_length,uint *max_length)
{
  const char *end=ptr+ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;
  char *max_end=max_str+res_length;

  for (; ptr != end && min_str != min_end ; ptr++)
  {
    if (*ptr == escape && ptr+1 != end)
    {
      ptr++;					/* Skip escape */
      *min_str++= *max_str++ = *ptr;
      continue;
    }
    if (*ptr == w_one || *ptr == w_many)	/* '_' and '%' in SQL */
    {
      char buf[10];
      uint buflen;
      uint charlen= my_charpos(cs, min_org, min_str, res_length/cs->mbmaxlen);
      
      if (charlen < (uint) (min_str - min_org))
        min_str= min_org + charlen;
      
      /* Write min key  */
      *min_length= (uint) (min_str - min_org);
      *max_length=res_length;
      do
      {
	*min_str++= (char) cs->min_sort_char;
      } while (min_str != min_end);
      
      /* 
        Write max key: create a buffer with multibyte
        representation of the max_sort_char character,
        and copy it into max_str in a loop. 
      */
      buflen= cs->cset->wc_mb(cs, cs->max_sort_char, (uchar*) buf,
                              (uchar*) buf + sizeof(buf));
      DBUG_ASSERT(buflen > 0);
      do
      {
        if ((max_str + buflen) <= max_end)
        {
          /* Enough space for max characer */
          memcpy(max_str, buf, buflen);
          max_str+= buflen;
        }
        else
        {
          /* 
            There is no space for whole multibyte
            character, then add trailing spaces.
          */
          
	  *max_str++= ' ';
	}
      } while (max_str != max_end);
      return 0;
    }
    *min_str++= *max_str++ = *ptr;
  }
  *min_length= *max_length = (uint) (min_str - min_org);

  while (min_str != min_end)
    *min_str++ = *max_str++ = ' ';	/* Because if key compression */
  return 0;
}

static int my_wildcmp_mb_bin(CHARSET_INFO *cs,
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
      mblen= my_ismbchar(cs, wildstr, wildend);
      INC_PTR(cs,wildstr,wildend);		/* This is compared trough cmp */
      do
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

uint my_numcells_mb(CHARSET_INFO *cs, const char *b, const char *e)
{
  my_wc_t wc;
  int clen= 0;
  
  while (b < e)
  {
    int mblen;
    uint pg;
    if ((mblen= cs->cset->mb_wc(cs, &wc, (uchar*) b, (uchar*) e)) <= 0)
    {
      mblen= 1; /* Let's think a wrong sequence takes 1 dysplay cell */
      b++;
      continue;
    }
    b+= mblen;
    pg= (wc >> 8) & 0xFF;
    clen+= utr11_data[pg].p ? utr11_data[pg].p[wc & 0xFF] : utr11_data[pg].page;
    clen++;
  }
  return clen;
}


MY_COLLATION_HANDLER my_collation_mb_bin_handler =
{
    NULL,		/* init */
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
