/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program file is free software; you can redistribute it and/or
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

/* This file is originally from the mysql distribution. Coded by monty */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#ifdef HAVE_FCONVERT
#include <floatingpoint.h>
#endif

extern gptr sql_alloc(unsigned size);
extern void sql_element_free(void *ptr);

#include "sql_string.h"

/*****************************************************************************
** String functions
*****************************************************************************/

bool String::real_alloc(uint32 arg_length)
{
  arg_length=ALIGN_SIZE(arg_length+1);
  if (Alloced_length < arg_length)
  {
    free();
    if (!(Ptr=(char*) my_malloc(arg_length,MYF(MY_WME))))
    {
      str_length=0;
      return TRUE;
    }
    Alloced_length=arg_length;
    alloced=1;
  }
  Ptr[0]=0;
  str_length=0;
  return FALSE;
}


/*
** Check that string is big enough. Set string[alloc_length] to 0
** (for C functions)
*/

bool String::realloc(uint32 alloc_length)
{
  uint32 len=ALIGN_SIZE(alloc_length+1);
  if (Alloced_length < len)
  {
    char *new_ptr;
    if (alloced)
    {
      if ((new_ptr= (char*) my_realloc(Ptr,len,MYF(MY_WME))))
      {
	Ptr=new_ptr;
	Alloced_length=len;
      }
      else
	return TRUE;				// Signal error
    }
    else if ((new_ptr= (char*) my_malloc(len,MYF(MY_WME))))
    {
      if (str_length)				// Avoid bugs in memcpy on AIX
	memcpy(new_ptr,Ptr,str_length);
      new_ptr[str_length]=0;
      Ptr=new_ptr;
      Alloced_length=len;
      alloced=1;
    }
    else
      return TRUE;			// Signal error
  }
  Ptr[alloc_length]=0;			// This make other funcs shorter
  return FALSE;
}

bool String::set(longlong num)
{
  if (alloc(21))
    return TRUE;
  str_length=(uint32) (longlong10_to_str(num,Ptr,-10)-Ptr);
  return FALSE;
}

bool String::set(ulonglong num)
{
  if (alloc(21))
    return TRUE;
  str_length=(uint32) (longlong10_to_str(num,Ptr,10)-Ptr);
  return FALSE;
}

bool String::set(double num,uint decimals)
{
  char buff[331];
  if (decimals >= NOT_FIXED_DEC)
  {
    sprintf(buff,"%.14g",num);			// Enough for a DATETIME
    return copy(buff, (uint32) strlen(buff));
  }
#ifdef HAVE_FCONVERT
  int decpt,sign;
  char *pos,*to;

  VOID(fconvert(num,(int) decimals,&decpt,&sign,buff+1));
  if (!isdigit(buff[1]))
  {						// Nan or Inf
    pos=buff+1;
    if (sign)
    {
      buff[0]='-';
      pos=buff;
    }
    return copy(pos,(uint32) strlen(pos));
  }
  if (alloc((uint32) ((uint32) decpt+3+decimals)))
    return TRUE;
  to=Ptr;
  if (sign)
    *to++='-';

  pos=buff+1;
  if (decpt < 0)
  {					/* value is < 0 */
    *to++='0';
    if (!decimals)
      goto end;
    *to++='.';
    if ((uint32) -decpt > decimals)
      decpt= - (int) decimals;
    decimals=(uint32) ((int) decimals+decpt);
    while (decpt++ < 0)
      *to++='0';
  }
  else if (decpt == 0)
  {
    *to++= '0';
    if (!decimals)
      goto end;
    *to++='.';
  }
  else
  {
    while (decpt-- > 0)
      *to++= *pos++;
    if (!decimals)
      goto end;
    *to++='.';
  }
  while (decimals--)
    *to++= *pos++;

end:
  *to=0;
  str_length=(uint32) (to-Ptr);
  return FALSE;
#else
#ifdef HAVE_SNPRINTF
  buff[sizeof(buff)-1]=0;			// Safety
  snprintf(buff,sizeof(buff)-1, "%.*f",(int) decimals,num);
#else
  sprintf(buff,"%.*f",(int) decimals,num);
#endif
  return copy(buff,(uint32) strlen(buff));
#endif
}


bool String::copy()
{
  if (!alloced)
  {
    Alloced_length=0;				// Force realloc
    return realloc(str_length);
  }
  return FALSE;
}

bool String::copy(const String &str)
{
  if (alloc(str.str_length))
    return TRUE;
  str_length=str.str_length;
  bmove(Ptr,str.Ptr,str_length);		// May be overlapping
  Ptr[str_length]=0;
  return FALSE;
}

bool String::copy(const char *str,uint32 arg_length)
{
  if (alloc(arg_length))
    return TRUE;
  if ((str_length=arg_length))
    memcpy(Ptr,str,arg_length);
  Ptr[arg_length]=0;
  return FALSE;
}

/* This is used by mysql.cc */

bool String::fill(uint32 max_length,char fill_char)
{
  if (str_length > max_length)
    Ptr[str_length=max_length]=0;
  else
  {
    if (realloc(max_length))
      return TRUE;
    bfill(Ptr+str_length,max_length-str_length,fill_char);
    str_length=max_length;
  }
  return FALSE;
}

void String::strip_sp()
{
   while (str_length && isspace(Ptr[str_length-1]))
    str_length--;
}

bool String::append(const String &s)
{
  if (s.length())
  {
    if (realloc(str_length+s.length()))
      return TRUE;
    memcpy(Ptr+str_length,s.ptr(),s.length());
    str_length+=s.length();
  }
  return FALSE;
}

bool String::append(const char *s,uint32 arg_length)
{
  if (!arg_length)				// Default argument
    if (!(arg_length= (uint32) strlen(s)))
      return FALSE;
  if (realloc(str_length+arg_length))
    return TRUE;
  memcpy(Ptr+str_length,s,arg_length);
  str_length+=arg_length;
  return FALSE;
}

#ifdef TO_BE_REMOVED
bool String::append(FILE* file, uint32 arg_length, myf my_flags)
{
  if (realloc(str_length+arg_length))
    return TRUE;
  if (my_fread(file, (byte*) Ptr + str_length, arg_length, my_flags))
  {
    shrink(str_length);
    return TRUE;
  }
  str_length+=arg_length;
  return FALSE;
}
#endif

bool String::append(IO_CACHE* file, uint32 arg_length)
{
  if (realloc(str_length+arg_length))
    return TRUE;
  if (my_b_read(file, (byte*) Ptr + str_length, arg_length))
  {
    shrink(str_length);
    return TRUE;
  }
  str_length+=arg_length;
  return FALSE;
}

uint32 String::numchars()
{
#ifdef USE_MB
  register uint32 n=0,mblen;
  register const char *mbstr=Ptr;
  register const char *end=mbstr+str_length;
  if (use_mb(default_charset_info))
  {
    while (mbstr < end) {
        if ((mblen=my_ismbchar(default_charset_info, mbstr,end))) mbstr+=mblen;
        else ++mbstr;
        ++n;
    }
    return n;
  }
  else
#endif
    return str_length;
}

int String::charpos(int i,uint32 offset)
{
#ifdef USE_MB
  register uint32 mblen;
  register const char *mbstr=Ptr+offset;
  register const char *end=Ptr+str_length;
  if (use_mb(default_charset_info))
  {
    if (i<=0) return i;
    while (i && mbstr < end) {
       if ((mblen=my_ismbchar(default_charset_info, mbstr,end))) mbstr+=mblen;
       else ++mbstr;
       --i;
    }
    if ( INT_MAX32-i <= (int) (mbstr-Ptr-offset)) 
      return INT_MAX32;
    else 
      return (int) ((mbstr-Ptr-offset)+i);
  }
  else
#endif
    return i;
}

int String::strstr(const String &s,uint32 offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return ((int) offset);	// Empty string is always found

    register const char *str = Ptr+offset;
    register const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skipp:
    while (str != end)
    {
      if (*str++ == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (*i++ != *j++) goto skipp;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}


/*
** Search string from end. Offset is offset to the end of string
*/

int String::strrstr(const String &s,uint32 offset)
{
  if (s.length() <= offset && offset <= str_length)
  {
    if (!s.length())
      return offset;				// Empty string is always found
    register const char *str = Ptr+offset-1;
    register const char *search=s.ptr()+s.length()-1;

    const char *end=Ptr+s.length()-2;
    const char *search_end=s.ptr()-1;
skipp:
    while (str != end)
    {
      if (*str-- == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search-1;
	while (j != search_end)
	  if (*i-- != *j--) goto skipp;
	return (int) (i-Ptr) +1;
      }
    }
  }
  return -1;
}

/*
** replace substring with string
** If wrong parameter or not enough memory, do nothing
*/


bool String::replace(uint32 offset,uint32 arg_length,const String &to)
{
  long diff = (long) to.length()-(long) arg_length;
  if (offset+arg_length <= str_length)
  {
    if (diff < 0)
    {
      if (to.length())
	memcpy(Ptr+offset,to.ptr(),to.length());
      bmove(Ptr+offset+to.length(),Ptr+offset+arg_length,
	    str_length-offset-arg_length);
    }
    else
    {
      if (diff)
      {
	if (realloc(str_length+(uint32) diff))
	  return TRUE;
	bmove_upp(Ptr+str_length+diff,Ptr+str_length,
		  str_length-offset-arg_length);
      }
      if (to.length())
	memcpy(Ptr+offset,to.ptr(),to.length());
    }
    str_length+=(uint32) diff;
  }
  return FALSE;
}


int sortcmp(const String *x,const String *y)
{
  const char *s= x->ptr();
  const char *t= y->ptr();
  uint32 x_len=x->length(),y_len=y->length(),len=min(x_len,y_len);

#ifdef USE_STRCOLL
  if (use_strcoll(default_charset_info))
  {
#ifndef CMP_ENDSPACE
    while (x_len && isspace(s[x_len-1]))
      x_len--;
    while (y_len && isspace(t[y_len-1]))
      y_len--;
#endif
    return my_strnncoll(default_charset_info,
                        (unsigned char *)s,x_len,(unsigned char *)t,y_len);
  }
  else
  {
#endif /* USE_STRCOLL */
    x_len-=len;					// For easy end space test
    y_len-=len;
    while (len--)
    {
      if (my_sort_order[(uchar) *s++] != my_sort_order[(uchar) *t++])
        return ((int) my_sort_order[(uchar) s[-1]] -
                (int) my_sort_order[(uchar) t[-1]]);
    }
#ifndef CMP_ENDSPACE
    /* Don't compare end space in strings */
    {
      if (y_len)
      {
        const char *end=t+y_len;
        for (; t != end ; t++)
          if (!isspace(*t))
            return -1;
      }
      else
      {
        const char *end=s+x_len;
        for (; s != end ; s++)
          if (!isspace(*s))
            return 1;
      }
      return 0;
    }
#else
    return (int) (x_len-y_len);
#endif /* CMP_ENDSPACE */
#ifdef USE_STRCOLL
  }
#endif
}


int stringcmp(const String *x,const String *y)
{
  const char *s= x->ptr();
  const char *t= y->ptr();
  uint32 x_len=x->length(),y_len=y->length(),len=min(x_len,y_len);

  while (len--)
  {
    if (*s++ != *t++)
      return ((int) (uchar) s[-1] - (int) (uchar) t[-1]);
  }
  return (int) (x_len-y_len);
}


String *copy_if_not_alloced(String *to,String *from,uint32 from_length)
{
  if (from->Alloced_length >= from_length)
    return from;
  if (from->alloced || !to || from == to)
  {
    (void) from->realloc(from_length);
    return from;
  }
  if (to->realloc(from_length))
    return from;				// Actually an error
  if ((to->str_length=min(from->str_length,from_length)))
    memcpy(to->Ptr,from->Ptr,to->str_length);
  return to;
}

/* Make it easier to handle different charactersets */

#ifdef USE_MB
#define INC_PTR(A,B) A+=((use_mb_flag && \
                          my_ismbchar(default_charset_info,A,B)) ? \
                          my_ismbchar(default_charset_info,A,B) : 1)
#else
#define INC_PTR(A,B) A++
#endif

/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

#ifdef LIKE_CMP_TOUPPER
#define likeconv(A) (uchar) toupper(A)
#else
#define likeconv(A) (uchar) my_sort_order[(uchar) (A)]
#endif

int wild_case_compare(const char *str,const char *str_end,
		      const char *wildstr,const char *wildend,
		      char escape)
{
  int result= -1;				// Not found, using wildcards
#ifdef USE_MB
  bool use_mb_flag=use_mb(default_charset_info);
#endif
  while (wildstr != wildend)
  {
    while (*wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
#ifdef USE_MB
      int l;
      if (use_mb_flag &&
          (l = my_ismbchar(default_charset_info, wildstr, wildend)))
      {
	  if (str+l > str_end || memcmp(str, wildstr, l) != 0)
	      return 1;
	  str += l;
	  wildstr += l;
      }
      else
#endif
      if (str == str_end || likeconv(*wildstr++) != likeconv(*str++))
	return(1);				// No match
      if (wildstr == wildend)
	return (str != str_end);		// Match if both are at end
      result=1;					// Found an anchor char
    }
    if (*wildstr == wild_one)
    {
      do
      {
	if (str == str_end)			// Skipp one char if possible
	  return (result);
	INC_PTR(str,str_end);
      } while (++wildstr < wildend && *wildstr == wild_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == wild_many)
    {						// Found wild_many
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for ( ; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == wild_many)
	  continue;
	if (*wildstr == wild_one)
	{
	  if (str == str_end)
	    return (-1);
	  INC_PTR(str,str_end);
	  continue;
	}
	break;					// Not a wild character
      }
      if (wildstr == wildend)
	return(0);				// Ok if wild_many is last
      if (str == str_end)
	return -1;

      uchar cmp;
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
#ifdef USE_MB
      const char* mb = wildstr;
      int mblen;
      LINT_INIT(mblen);
      if (use_mb_flag)
        mblen = my_ismbchar(default_charset_info, wildstr, wildend);
#endif
      INC_PTR(wildstr,wildend);			// This is compared trough cmp
      cmp=likeconv(cmp);   
      do
      {
#ifdef USE_MB
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
            else if (!my_ismbchar(default_charset_info, str, str_end) &&
                     likeconv(*str) == cmp)
            {
              str++;
              break;
            }
            INC_PTR(str, str_end);
          }
	}
        else
        {
#endif /* USE_MB */
          while (str != str_end && likeconv(*str) != cmp)
            str++;
          if (str++ == str_end) return (-1);
#ifdef USE_MB
        }
#endif
	{
	  int tmp=wild_case_compare(str,str_end,wildstr,wildend,escape);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != wild_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}


int wild_case_compare(String &match,String &wild, char escape)
{
  return wild_case_compare(match.ptr(),match.ptr()+match.length(),
			   wild.ptr(), wild.ptr()+wild.length(),escape);
}

/*
** The following is used when using LIKE on binary strings
*/

int wild_compare(const char *str,const char *str_end,
		 const char *wildstr,const char *wildend,char escape)
{
  int result= -1;				// Not found, using wildcards
  while (wildstr != wildend)
  {
    while (*wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (str == str_end || *wildstr++ != *str++)
	return(1);
      if (wildstr == wildend)
	return (str != str_end);		// Match if both are at end
      result=1;					// Found an anchor char
    }
    if (*wildstr == wild_one)
    {
      do
      {
	if (str == str_end)			// Skipp one char if possible
	  return (result);
	str++;
      } while (*++wildstr == wild_one && wildstr != wildend);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == wild_many)
    {						// Found wild_many
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for ( ; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == wild_many)
	  continue;
	if (*wildstr == wild_one)
	{
	  if (str == str_end)
	    return (-1);
	  str++;
	  continue;
	}
	break;					// Not a wild character
      }
      if (wildstr == wildend)
	return(0);				// Ok if wild_many is last
      if (str == str_end)
	return -1;

      char cmp;
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
      wildstr++;				// This is compared trough cmp
      do
      {
	while (str != str_end && *str != cmp)
	  str++;
	if (str++ == str_end) return (-1);
	{
	  int tmp=wild_compare(str,str_end,wildstr,wildend,escape);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != str_end && wildstr[0] != wild_many);
      return(-1);
    }
  }
  return (str != str_end ? 1 : 0);
}


int wild_compare(String &match,String &wild, char escape)
{
  return wild_compare(match.ptr(),match.ptr()+match.length(),
		      wild.ptr(), wild.ptr()+wild.length(),escape);
}
