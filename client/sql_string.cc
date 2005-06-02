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

/* This file is originally from the mysql distribution. Coded by monty */

#include <my_global.h>

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

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
  str_length=0;
  if (Alloced_length < arg_length)
  {
    free();
    if (!(Ptr=(char*) my_malloc(arg_length,MYF(MY_WME))))
      return TRUE;
    Alloced_length=arg_length;
    alloced=1;
  }
  Ptr[0]=0;
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

bool String::set(longlong num, CHARSET_INFO *cs)
{
  uint l=20*cs->mbmaxlen+1;

  if (alloc(l))
    return TRUE;
  if (cs->cset->snprintf == my_snprintf_8bit)
  {
    str_length=(uint32) (longlong10_to_str(num,Ptr,-10)-Ptr);
  }
  else
  {
    str_length=cs->cset->snprintf(cs,Ptr,l,"%d",num);
  }
  str_charset=cs;
  return FALSE;
}

bool String::set(ulonglong num, CHARSET_INFO *cs)
{
  uint l=20*cs->mbmaxlen+1;

  if (alloc(l))
    return TRUE;
  if (cs->cset->snprintf == my_snprintf_8bit)
  {
    str_length=(uint32) (longlong10_to_str(num,Ptr,10)-Ptr);
  }
  else
  {
    str_length=cs->cset->snprintf(cs,Ptr,l,"%d",num);
  }
  str_charset=cs;
  return FALSE;
}

bool String::set(double num,uint decimals, CHARSET_INFO *cs)
{
  char buff[331];

  str_charset=cs;
  if (decimals >= NOT_FIXED_DEC)
  {
    sprintf(buff,"%.14g",num);			// Enough for a DATETIME
    return copy(buff, (uint32) strlen(buff), &my_charset_latin1, cs);
  }
#ifdef HAVE_FCONVERT
  int decpt,sign;
  char *pos,*to;

  VOID(fconvert(num,(int) decimals,&decpt,&sign,buff+1));
  if (!my_isdigit(&my_charset_latin1, buff[1]))
  {						// Nan or Inf
    pos=buff+1;
    if (sign)
    {
      buff[0]='-';
      pos=buff;
    }
    return copy(pos,(uint32) strlen(pos), &my_charset_latin1, cs);
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
  return copy(buff,(uint32) strlen(buff), &my_charset_latin1, cs);
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
  str_charset=str.str_charset;
  return FALSE;
}

bool String::copy(const char *str,uint32 arg_length, CHARSET_INFO *cs)
{
  if (alloc(arg_length))
    return TRUE;
  if ((str_length=arg_length))
    memcpy(Ptr,str,arg_length);
  Ptr[arg_length]=0;
  str_charset=cs;
  return FALSE;
}

/* Copy with charset convertion */
bool String::copy(const char *str,uint32 arg_length, CHARSET_INFO *from, CHARSET_INFO *to)
{
  uint32      new_length=to->mbmaxlen*arg_length;
  int         cnvres;
  my_wc_t     wc;
  const uchar *s=(const uchar *)str;
  const uchar *se=s+arg_length;
  uchar       *d, *de;

  if (alloc(new_length))
    return TRUE;

  d=(uchar *)Ptr;
  de=d+new_length;
  
  for (str_length=new_length ; s < se && d < de ; )
  {
    if ((cnvres=from->cset->mb_wc(from,&wc,s,se)) > 0 )
    {
      s+=cnvres;
    }
    else if (cnvres==MY_CS_ILSEQ)
    {
      s++;
      wc='?';
    }
    else
      break;

outp:
    if((cnvres=to->cset->wc_mb(to,wc,d,de)) >0 )
    {
      d+=cnvres;
    }
    else if (cnvres==MY_CS_ILUNI && wc!='?')
    {
      wc='?';
      goto outp;
    }
    else
      break;
  }
  Ptr[new_length]=0;
  length((uint32) (d-(uchar *)Ptr));
  str_charset=to;
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
   while (str_length && my_isspace(str_charset,Ptr[str_length-1]))
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
  if (use_mb(str_charset))
  {
    while (mbstr < end) {
        if ((mblen=my_ismbchar(str_charset, mbstr,end))) mbstr+=mblen;
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
  if (use_mb(str_charset))
  {
    if (i<=0) return i;
    while (i && mbstr < end) {
       if ((mblen=my_ismbchar(str_charset, mbstr,end))) mbstr+=mblen;
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
skip:
    while (str != end)
    {
      if (*str++ == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (*i++ != *j++) goto skip;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}

/*
  Search after a string without regarding to case
  This needs to be replaced when we have character sets per string
*/

int String::strstr_case(const String &s,uint32 offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return ((int) offset);	// Empty string is always found

    register const char *str = Ptr+offset;
    register const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skip:
    while (str != end)
    {
      if (str_charset->sort_order[*str++] == str_charset->sort_order[*search])
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (str_charset->sort_order[*i++] != 
              str_charset->sort_order[*j++]) 
            goto skip;
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
skip:
    while (str != end)
    {
      if (*str-- == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search-1;
	while (j != search_end)
	  if (*i-- != *j--) goto skip;
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

// added by Holyfoot for "geometry" needs
int String::reserve(uint32 space_needed, uint32 grow_by)
{
  if (Alloced_length < str_length + space_needed)
  {
    if (realloc(Alloced_length + max(space_needed, grow_by) - 1))
      return TRUE;
  }
  return FALSE;
}

void String::qs_append(const char *str)
{
  int len = (int)strlen(str);
  memcpy(Ptr + str_length, str, len + 1);
  str_length += len;
}

void String::qs_append(double d)
{
  char *buff = Ptr + str_length;
  sprintf(buff,"%.14g", d);
  str_length += (int)strlen(buff);
}

void String::qs_append(double *d)
{
  double ld;
  float8get(ld, (char*) d);
  qs_append(ld);
}

void String::qs_append(const char &c)
{
  Ptr[str_length] = c;
  str_length += sizeof(c);
}


int sortcmp(const String *x,const String *y)
{
  const char *s= x->ptr();
  const char *t= y->ptr();
  uint32 x_len=x->length(),y_len=y->length(),len=min(x_len,y_len);

  if (use_strnxfrm(x->str_charset))
  {
#ifndef CMP_ENDSPACE
    while (x_len && my_isspace(x->str_charset,s[x_len-1]))
      x_len--;
    while (y_len && my_isspace(x->str_charset,t[y_len-1]))
      y_len--;
#endif
    return my_strnncoll(x->str_charset,
                        (unsigned char *)s,x_len,(unsigned char *)t,y_len);
  }
  else
  {
    x_len-=len;					// For easy end space test
    y_len-=len;
    if (x->str_charset->sort_order)
    {
      while (len--)
      {
        if (x->str_charset->sort_order[(uchar) *s++] != 
          x->str_charset->sort_order[(uchar) *t++])
            return ((int) x->str_charset->sort_order[(uchar) s[-1]] -
                  (int) x->str_charset->sort_order[(uchar) t[-1]]);
      }
    }
    else
    {
      while (len--)
      {
        if (*s++ != *t++)
            return ((int) s[-1] - (int) t[-1]);
      }
    }
#ifndef CMP_ENDSPACE
    /* Don't compare end space in strings */
    {
      if (y_len)
      {
        const char *end=t+y_len;
        for (; t != end ; t++)
          if (!my_isspace(x->str_charset,*t))
            return -1;
      }
      else
      {
        const char *end=s+x_len;
        for (; s != end ; s++)
          if (!my_isspace(x->str_charset,*s))
            return 1;
      }
      return 0;
    }
#else
    return (int) (x_len-y_len);
#endif /* CMP_ENDSPACE */
  }
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
  to->str_charset=from->str_charset;
  return to;
}



