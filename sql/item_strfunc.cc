/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/* This file defines all string functions
** Warning: Some string functions doesn't always put and end-null on a String
** (This shouldn't be needed)
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include "md5.h"

String empty_string("");

uint nr_of_decimals(const char *str)
{
  if ((str=strchr(str,'.')))
  {
    const char *start= ++str;
    for ( ; isdigit(*str) ; str++) ;
    return (uint) (str-start);
  }
  return 0;
}

double Item_str_func::val()
{
  String *res;
  res=val_str(&str_value);
  return res ? atof(res->c_ptr()) : 0.0;
}

longlong Item_str_func::val_int()
{
  String *res;
  res=val_str(&str_value);
  return res ? strtoll(res->c_ptr(),NULL,10) : (longlong) 0;
}


String *Item_func_md5::val_str(String *str)
{
  String * sptr= args[0]->val_str(str);
  if (sptr)
  {
    my_MD5_CTX context;
    unsigned char digest[16];

    null_value=0;
    my_MD5Init (&context);
    my_MD5Update (&context,(unsigned char *) sptr->ptr(), sptr->length());
    my_MD5Final (digest, &context);
    str->alloc(32);				// Ensure that memory is free
    sprintf((char *) str->ptr(),
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2], digest[3],
	    digest[4], digest[5], digest[6], digest[7],
	    digest[8], digest[9], digest[10], digest[11],
	    digest[12], digest[13], digest[14], digest[15]);
    str->length((uint) 32);
    return str;
  }
  null_value=1;
  return 0;
}


void Item_func_md5::fix_length_and_dec()
{
   max_length=32;
}

/*
** Concatinate args with the following premissess
** If only one arg which is ok, return value of arg
** Don't reallocate val_str() if not absolute necessary.
*/

String *Item_func_concat::val_str(String *str)
{
  String *res,*res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(res=args[0]->val_str(str)))
    goto null;
  use_as_buff= &tmp_value;
  for (i=1 ; i < arg_count ; i++)
  {
    if (res->length() == 0)
    {
      if (!(res=args[i]->val_str(str)))
	goto null;
    }
    else
    {
      if (!(res2=args[i]->val_str(use_as_buff)))
	goto null;
      if (res2->length() == 0)
	continue;
      if (res->length()+res2->length() > max_allowed_packet)
	goto null;				// Error check
      if (res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
	res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
	if (str == res2)
	  str->replace(0,0,*res);
	else
	{
	  str->copy(*res);
	  str->append(*res2);
	}
	res=str;
      }
      else if (res == &tmp_value)
      {
	if (res->append(*res2))			// Must be a blob
	  goto null;
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
	if (tmp_value.replace(0,0,*res))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	       res2->ptr() <= tmp_value.ptr() + tmp_value.alloced_length())
      {
	/*
	  This happens really seldom:
	  In this case res2 is sub string of tmp_value.  We will
	  now work in place in tmp_value to set it to res | res2
	*/
	/* Chop the last characters in tmp_value that isn't in res2 */
	tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
			 res2->length());
	/* Place res2 at start of tmp_value, remove chars before res2 */
	if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			      *res))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
	if (tmp_value.alloc(max_length) ||
	    tmp_value.copy(*res) ||
	    tmp_value.append(*res2))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;
      }
    }
  }
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  max_length=0;
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}



/* 
** concat with separator. First arg is the separator
** concat_ws takes at least two arguments.
*/

String *Item_func_concat_ws::val_str(String *str)
{
  char tmp_str_buff[10];
  String tmp_sep_str(tmp_str_buff, sizeof(tmp_str_buff)),
         *sep_str, *res, *res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(sep_str= separator->val_str(&tmp_sep_str)))
    goto null;

  use_as_buff= &tmp_value;
  str->length(0);				// QQ; Should be removed
  res=str;

  // Skip until non-null and non-empty argument is found.
  // If not, return the empty string
  for (i=0; i < arg_count; i++)
    if ((res= args[i]->val_str(str)) && res->length())
      break;
  if (i ==  arg_count)
    return &empty_string;

  for (i++; i < arg_count ; i++)
  {
    if (!(res2= args[i]->val_str(use_as_buff)) || !res2->length())
      continue;					// Skipp NULL and empty string

    if (res->length() + sep_str->length() + res2->length() >
	max_allowed_packet)
      goto null;				// Error check
    if (res->alloced_length() >=
	res->length() + sep_str->length() + res2->length())
    {						// Use old buffer
      res->append(*sep_str);			// res->length() > 0 always
      res->append(*res2);
    }
    else if (str->alloced_length() >=
	     res->length() + sep_str->length() + res2->length())
    {
      /* We have room in str;  We can't get any errors here */
      if (str == res2)
      {						// This is quote uncommon!
	str->replace(0,0,*sep_str);
	str->replace(0,0,*res);
      }
      else
      {
	str->copy(*res);
	str->append(*sep_str);
	str->append(*res2);
      }
      res=str;
      use_as_buff= &tmp_value;
    }
    else if (res == &tmp_value)
    {
      if (res->append(*sep_str) || res->append(*res2))
	goto null; // Must be a blob
    }
    else if (res2 == &tmp_value)
    {						// This can happend only 1 time
      if (tmp_value.replace(0,0,*sep_str) || tmp_value.replace(0,0,*res))
	goto null;
      res= &tmp_value;
      use_as_buff=str;				// Put next arg here
    }
    else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	     res2->ptr() < tmp_value.ptr() + tmp_value.alloced_length())
    {
      /*
	This happens really seldom:
	In this case res2 is sub string of tmp_value.  We will
	now work in place in tmp_value to set it to res | sep_str | res2
      */
      /* Chop the last characters in tmp_value that isn't in res2 */
      tmp_value.length((uint32) (res2->ptr() - tmp_value.ptr()) +
		       res2->length());
      /* Place res2 at start of tmp_value, remove chars before res2 */
      if (tmp_value.replace(0,(uint32) (res2->ptr() - tmp_value.ptr()),
			    *res) ||
	  tmp_value.replace(res->length(),0, *sep_str))
	goto null;
      res= &tmp_value;
      use_as_buff=str;			// Put next arg here
    }
    else
    {						// Two big const strings
      if (tmp_value.alloc(max_length) ||
	  tmp_value.copy(*res) ||
	  tmp_value.append(*sep_str) ||
	  tmp_value.append(*res2))
	goto null;
      res= &tmp_value;
      use_as_buff=str;
    }
  }
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat_ws::fix_length_and_dec()
{
  max_length=0;
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
  used_tables_cache|=separator->used_tables();
  const_item_cache&=separator->const_item();
}

void Item_func_concat_ws::update_used_tables()
{
  Item_func::update_used_tables();
  separator->update_used_tables();
  used_tables_cache|=separator->used_tables();
  const_item_cache&=separator->const_item();
}


String *Item_func_reverse::val_str(String *str)
{
  String *res = args[0]->val_str(str);
  char *ptr,*end;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &empty_string;
  res=copy_if_not_alloced(str,res,res->length());
  ptr = (char *) res->ptr();
  end=ptr+res->length();
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    String tmpstr;
    tmpstr.copy(*res);
    char *tmp = (char *) tmpstr.ptr() + tmpstr.length();
    register uint32 l;
    while (ptr < end)
    {
      if ((l=my_ismbchar(default_charset_info, ptr,end)))
        tmp-=l, memcpy(tmp,ptr,l), ptr+=l;
      else
        *--tmp=*ptr++;
    }
    memcpy((char *) res->ptr(),(char *) tmpstr.ptr(), res->length());
  }
  else
#endif /* USE_MB */
  {
    char tmp;
    while (ptr < end)
    {
      tmp=*ptr;
      *ptr++=*--end;
      *end=tmp;
    }
  }
  return res;
}


void Item_func_reverse::fix_length_and_dec()
{
  max_length = args[0]->max_length;
}

/*
** Replace all occurences of string2 in string1 with string3.
** Don't reallocate val_str() if not needed
*/

/* TODO: Fix that this works with binary strings when using USE_MB */

String *Item_func_replace::val_str(String *str)
{
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;
#ifdef USE_MB
  const char *ptr,*end,*strend,*search,*search_end;
  register uint32 l;
  bool binary_str = (args[0]->binary || args[1]->binary ||
		     !use_mb(default_charset_info));
#endif

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  if (res2->length() == 0)
    return res;
#ifndef USE_MB
  if ((offset=res->strstr(*res2)) < 0)
    return res;
#else
  offset=0;
  if (binary_str && (offset=res->strstr(*res2)) < 0)
    return res;
#endif
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

#ifdef USE_MB
  if (!binary_str)
  {
    search=res2->ptr();
    search_end=search+from_length;
redo:
    ptr=res->ptr()+offset;
    strend=res->ptr()+res->length();
    end=strend-from_length+1;
    while (ptr < end)
    {
        if (*ptr == *search)
        {
          register char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skipp;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length > max_allowed_packet)
            goto null;
          if (!alloced)
          {
            alloced=1;
            res=copy_if_not_alloced(str,res,res->length()+to_length);
          }
          res->replace((uint) offset,from_length,*res3);
	  offset+=(int) to_length;
          goto redo;
        }
skipp:
        if ((l=my_ismbchar(default_charset_info, ptr,strend))) ptr+=l;
        else ++ptr;
    }
  }
  else
#endif /* USE_MB */
    do
    {
      if (res->length()-from_length + to_length > max_allowed_packet)
        goto null;
      if (!alloced)
      {
        alloced=1;
        res=copy_if_not_alloced(str,res,res->length()+to_length);
      }
      res->replace((uint) offset,from_length,*res3);
      offset+=(int) to_length;
    }
    while ((offset=res->strstr(*res2,(uint) offset)) >= 0);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_replace::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  int diff=(int) (args[2]->max_length - args[1]->max_length);
  if (diff > 0 && args[1]->max_length)
  {						// Calculate of maxreplaces
    max_length= max_length/args[1]->max_length;
    max_length= (max_length+1)*(uint) diff;
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_insert::val_str(String *str)
{
  String *res,*res2;
  uint start,length;

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start=(uint) args[1]->val_int()-1;
  length=(uint) args[2]->val_int();
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */
#ifdef USE_MB
  if (use_mb(default_charset_info) && !args[0]->binary)
  {
    start=res->charpos(start);
    length=res->charpos(length,start);
  }
#endif
  if (start > res->length()+1)
    return res;					// Wrong param; skipp insert
  if (length > res->length()-start)
    length=res->length()-start;
  if (res->length() - length + res2->length() > max_allowed_packet)
    goto null;					// OOM check
  res=copy_if_not_alloced(str,res,res->length());
  res->replace(start,length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  max_length=args[0]->max_length+args[3]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lcase::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->casedn();
  return res;
}


String *Item_func_ucase::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->caseup();
  return res;
}


String *Item_func_left::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;
  if (length <= 0)
    return &empty_string;
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
    length = res->charpos(length);
#endif
  if (res->length() > (ulong) length)
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      res= &str_value;
    }
    res->length((uint) length);
  }
  return res;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int();
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  if (length <= 0)
    return &empty_string; /* purecov: inspected */
  if (res->length() <= (uint) length)
    return res; /* purecov: inspected */
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    uint start=res->numchars()-(uint) length;
    if (start<=0) return res;
    start=res->charpos(start);
    tmp_value.set(*res,start,res->length()-start);
  }
  else
#endif
  {
    tmp_value.set(*res,(res->length()- (uint) length),(uint) length);
  }
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_substr::val_str(String *str)
{
  String *res  = args[0]->val_str(str);
  int32 start	= (int32) args[1]->val_int()-1;
  int32 length	= arg_count == 3 ? (int32) args[2]->val_int() : INT_MAX32;
  int32 tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    start=res->charpos(start);
    length=res->charpos(length,start);
  }
#endif
  if (start < 0 || (uint) start+1 > res->length() || length <= 0)
    return &empty_string;

  tmp_length=(int32) res->length()-start;
  length=min(length,tmp_length);

  if (!start && res->length() == (uint) length)
    return res;
  tmp_value.set(*res,(uint) start,(uint) length);
  return &tmp_value;
}


void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  if (args[1]->const_item())
  {
    int32 start=(int32) args[1]->val_int()-1;
    if (start < 0 || start >= (int32) max_length)
      max_length=0; /* purecov: inspected */
    else
      max_length-= (uint) start;
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int();
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
}


String *Item_func_substr_index::val_str(String *str)
{
  String *res =args[0]->val_str(str);
  String *delimeter =args[1]->val_str(&tmp_value);
  int32 count = (int32) args[2]->val_int();
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimeter_length=delimeter->length();
  if (!res->length() || !delimeter_length || !count)
    return &empty_string;		// Wrong parameters

#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    const char *ptr=res->ptr();
    const char *strend = ptr+res->length();
    const char *end=strend-delimeter_length+1;
    const char *search=delimeter->ptr();
    const char *search_end=search+delimeter_length;
    int32 n=0,c=count,pass;
    register uint32 l;
    for (pass=(count>0);pass<2;++pass)
    {
      while (ptr < end)
      {
        if (*ptr == *search)
        {
	  register char *i,*j;
	  i=(char*) ptr+1; j=(char*) search+1;
	  while (j != search_end)
	    if (*i++ != *j++) goto skipp;
	  if (pass==0) ++n;
	  else if (!--c) break;
	  ptr+=delimeter_length;
	  continue;
	}
    skipp:
        if ((l=my_ismbchar(default_charset_info, ptr,strend))) ptr+=l;
        else ++ptr;
      } /* either not found or got total number when count<0 */
      if (pass == 0) /* count<0 */
      {
        c+=n+1;
        if (c<=0) return res; /* not found, return original string */
        ptr=res->ptr();
      }
      else
      {
        if (c) return res; /* Not found, return original string */
        if (count>0) /* return left part */
        {
	  tmp_value.set(*res,0,(ulong) (ptr-res->ptr()));
        }
        else /* return right part */
        {
	  ptr+=delimeter_length;
	  tmp_value.set(*res,(ulong) (ptr-res->ptr()), (ulong) (strend-ptr));
        }
      }
    }
  }
  else
#endif /* USE_MB */
  {
    if (count > 0)
    {					// start counting from the beginning
      for (offset=0 ;; offset+=delimeter_length)
      {
	if ((int) (offset=res->strstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!--count)
	{
	  tmp_value.set(*res,0,offset);
	  break;
	}
      }
    }
    else
    {					// Start counting at end
      for (offset=res->length() ; ; offset-=delimeter_length-1)
      {
	if ((int) (offset=res->strrstr(*delimeter,offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!++count)
	{
	  offset+=delimeter_length;
	  tmp_value.set(*res,offset,res->length()- offset);
	  break;
	}
      }
    }
  }
  return (&tmp_value);
}

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/


String *Item_func_ltrim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
    while (ptr != end && *ptr == chr)
      ptr++;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
    end-=remove_length;
    while (ptr < end && !memcmp(ptr,r_ptr,remove_length))
      ptr+=remove_length;
    end+=remove_length;
  }
  if (ptr == res->ptr())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_rtrim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
#ifdef USE_MB
  char *p=ptr;
  register uint32 l;
#endif
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
#ifdef USE_MB
    if (use_mb(default_charset_info) && !binary)
    {
      while (ptr < end)
      {
	if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l,p=ptr;
	else ++ptr;
      }
      ptr=p;
    }
#endif
    while (ptr != end  && end[-1] == chr)
      end--;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
#ifdef USE_MB
    if (use_mb(default_charset_info) && !binary)
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l;
	else ++ptr;
      }
      if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
      {
	end-=remove_length;
	ptr=p;
	goto loop;
      }
    }
    else
#endif /* USE_MB */
    {
      while (ptr + remove_length < end &&
	     !memcmp(end-remove_length,r_ptr,remove_length))
	end-=remove_length;
    }
  }
  if (end == res->ptr()+res->length())
    return res;
  tmp_value.set(*res,0,(uint) (end-res->ptr()));
  return &tmp_value;
}


String *Item_func_trim::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove_str=args[1]->val_str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove_str || (remove_length=remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  const char *r_ptr=remove_str->ptr();
  while (ptr+remove_length <= end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary)
  {
    char *p=ptr;
    register uint32 l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l=my_ismbchar(default_charset_info, ptr,end))) ptr+=l;
      else ++ptr;
    }
    if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
    {
      end-=remove_length;
      ptr=p;
      goto loop;
    }
    ptr=p;
  }
  else
#endif /* USE_MB */
  {
    while (ptr + remove_length <= end &&
	   !memcmp(end-remove_length,r_ptr,remove_length))
      end-=remove_length;
  }
  if (ptr == res->ptr() && end == ptr+res->length())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_password::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;
  make_scrambled_password(tmp_value,res->c_ptr());
  str->set(tmp_value,16);
  return str;
}

#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  String *res  =args[0]->val_str(str);

#ifdef HAVE_CRYPT
  char salt[3],*salt_ptr;
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;

  if (arg_count == 1)
  {					// generate random salt
    time_t timestamp=current_thd->query_start();
    salt[0] = bin_to_ascii( (ulong) timestamp & 0x3f);
    salt[1] = bin_to_ascii(( (ulong) timestamp >> 5) & 0x3f);
    salt[2] = 0;
    salt_ptr=salt;
  }
  else
  {					// obtain salt from the first two bytes
    String *salt_str=args[1]->val_str(&tmp_value);
    if ((null_value= (args[1]->null_value || salt_str->length() < 2)))
      return 0;
    salt_ptr= salt_str->c_ptr();
  }
  pthread_mutex_lock(&LOCK_crypt);
  char *tmp=crypt(res->c_ptr(),salt_ptr);
  str->set(tmp,(uint) strlen(tmp));
  str->copy();
  pthread_mutex_unlock(&LOCK_crypt);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null;
}

String *Item_func_encode::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.encode((char*) res->ptr(),res->length());
  return res;
}

String *Item_func_decode::val_str(String *str)
{
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.decode((char*) res->ptr(),res->length());
  return res;
}


String *Item_func_database::val_str(String *str)
{
  if (!current_thd->db)
    str->length(0);
  else
    str->set((const char*) current_thd->db,(uint) strlen(current_thd->db));
  return str;
}

String *Item_func_user::val_str(String *str)
{
  THD *thd=current_thd;
  if (str->copy((const char*) thd->user,(uint) strlen(thd->user)) ||
      str->append('@') ||
      str->append(thd->host ? thd->host : thd->ip ? thd->ip : ""))
    return &empty_string;
  return str;
}

void Item_func_soundex::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  set_if_bigger(max_length,4);
}


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skipp to next char
    else return 0
    */

extern "C" {
extern const char *soundex_map;		// In mysys/static.c
}

static char get_scode(char *ptr)
{
  uchar ch=toupper(*ptr);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


String *Item_func_soundex::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  char last_ch,ch;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  if (str_value.alloc(max(res->length(),4)))
    return str; /* purecov: inspected */
  char *to= (char *) str_value.ptr();
  char *from= (char *) res->ptr(), *end=from+res->length();

  while (from != end && isspace(*from)) // Skipp pre-space
    from++; /* purecov: inspected */
  if (from == end)
    return &empty_string;		// No alpha characters.
  *to++ = toupper(*from);		// Copy first letter
  last_ch = get_scode(from);		// code of the first letter
					// for the first 'double-letter check.
					// Loop on input letters until
					// end of input (null) or output
					// letter code count = 3
  for (from++ ; from < end ; from++)
  {
    if (!isalpha(*from))
      continue;
    ch=get_scode(from);
    if ((ch != '0') && (ch != last_ch)) // if not skipped or double
    {
       *to++ = ch;			// letter, copy to output
       last_ch = ch;			// save code of last input letter
    }					// for next double-letter check
  }
  for (end=(char*) str_value.ptr()+4 ; to < end ; to++)
    *to = '0';
  *to=0;				// end string
  str_value.length((uint) (to-str_value.ptr()));
  return &str_value;
}


/*
** Change a number to format '3,333,333,333.000'
** This should be 'internationalized' sometimes.
*/

Item_func_format::Item_func_format(Item *org,int dec) :Item_str_func(org)
{
  decimals=(uint) set_zone(dec,0,30);
}


String *Item_func_format::val_str(String *str)
{
  double nr	=args[0]->val();
  uint32 diff,length,str_length;
  uint dec;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  dec= decimals ? decimals+1 : 0;
  str->set(nr,decimals);
  str_length=str->length();
  if (nr < 0)
    str_length--;				// Don't count sign
  length=str->length()+(diff=(str_length- dec-1)/3);
  if (diff)
  {
    char *tmp,*pos;
    str=copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp=(char*) str->ptr()+length - dec-1;
    for (pos=(char*) str->ptr()+length ; pos != tmp; pos--)
      pos[0]=pos[- (int) diff];
    while (diff)
    {
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
  with_sum_func= with_sum_func || item->with_sum_func;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_elt::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


double Item_func_elt::val()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  return args[tmp-1]->val();
}

longlong Item_func_elt::val_int()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return args[tmp-1]->val_int();
}

String *Item_func_elt::val_str(String *str)
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return NULL;
  }
  null_value=0;
  return args[tmp-1]->val_str(str);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;
  for (uint i=1 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


String *Item_func_make_set::val_str(String *str)
{
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&empty_string;

  bits=item->val_int();
  if ((null_value=item->null_value))
    return NULL;

  if (arg_count < 64)
    bits &= ((ulonglong) 1 << arg_count)-1;

  for (; bits; bits >>= 1, ptr++)
  {
    if (bits & 1)
    {
      String *res= (*ptr)->val_str(str);
      if (res)					// Skipp nulls
      {
	if (!first_found)
	{					// First argument
	  first_found=1;
	  if (res != str)
	    result=res;				// Use original string
	  else
	  {
	    if (tmp_str.copy(*res))		// Don't use 'str'
	      return &empty_string;
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
	      return &empty_string;
	    result= &tmp_str;
	  }
	  if (tmp_str.append(',') || tmp_str.append(*res))
	    return &empty_string;
	}
      }
    }
  }
  return result;
}


String *Item_func_char::val_str(String *str)
{
  str->length(0);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32 num=(int32) args[i]->val_int();
    if (!args[i]->null_value)
#ifdef USE_MB
      if (use_mb(default_charset_info))
      {
        if (num&0xFF000000L) {
           str->append((char)(num>>24));
           goto b2;
        } else if (num&0xFF0000L) {
b2:        str->append((char)(num>>16));
           goto b1;
        } else if (num&0xFF00L) {   
b1:        str->append((char)(num>>8));
        }
      }
#endif
      str->append((char)num);
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return str;
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
			    ulong length)
{
  if (res->alloced_length() < length)
  {
    if (str->alloced_length() >= length)
    {
      (void) str->copy(*res);
      str->length(length);
      return str;
    }
    else
    {
      if (tmp_value->alloc(length))
	return 0;
      (void) tmp_value->copy(*res);
      tmp_value->length(length);
      return tmp_value;
    }
  }
  res->length(length);
  return res;
}


void Item_func_repeat::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    max_length=(long) (args[0]->max_length * args[1]->val_int());
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
** Item_func_repeat::str is carefully written to avoid reallocs
** as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  uint length,tot_length;
  char *to;
  long count= (long) args[1]->val_int();
  String *res =args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value=0;
  if (count <= 0)			// For nicer SQL code
    return &empty_string;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  if (length > max_allowed_packet/count)// Safe length check
    goto err;				// Probably an error
  tot_length= length*(uint) count;
  if (!(res= alloc_buffer(res,str,&tmp_value,tot_length)))
    goto err;

  to=(char*) res->ptr()+length;
  while (--count)
  {
    memcpy(to,res->ptr(),length);
    to+=length;
  }
  return (res);

err:
  null_value=1;
  return 0;
}


void Item_func_rpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_rpad::val_str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  int32 count= (int32) args[1]->val_int();
  String *res =args[0]->val_str(str);
  String *rpad = args[2]->val_str(str);

  if (!res || args[1]->null_value || !rpad)
    goto err;
  null_value=0;
  if (count <= (int32) (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= rpad->length();
  if ((ulong) count > max_allowed_packet || args[2]->null_value || !length_pad)
    goto err;
  if (!(res= alloc_buffer(res,str,&tmp_value,count)))
    goto err;

  to= (char*) res->ptr()+res_length;
  ptr_pad=rpad->ptr();
  for (count-= res_length; (uint32) count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lpad::val_str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  ulong count= (long) args[1]->val_int();
  String *res= args[0]->val_str(str);
  String *lpad= args[2]->val_str(str);

  if (!res || args[1]->null_value || !lpad)
    goto err;
  null_value=0;
  if (count <= (res_length=res->length()))
  {						// String to pad is big enough
    res->length(count);				// Shorten result if longer
    return (res);
  }
  length_pad= lpad->length();
  if (count > max_allowed_packet || args[2]->null_value || !length_pad)
    goto err;

  if (res->alloced_length() < count)
  {
    if (str->alloced_length() >= count)
    {
      memcpy((char*) str->ptr()+(count-res_length),res->ptr(),res_length);
      res=str;
    }
    else
    {
      if (tmp_value.alloc(count))
	goto err;
      memcpy((char*) tmp_value.ptr()+(count-res_length),res->ptr(),res_length);
      res=&tmp_value;
    }
  }
  else
    bmove_upp((char*) res->ptr()+count,res->ptr()+res_length,res_length);
  res->length(count);

  to= (char*) res->ptr();
  ptr_pad= lpad->ptr();
  for (count-= res_length; count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


String *Item_func_conv::val_str(String *str)
{
  String *res= args[0]->val_str(str);
  char *endptr,ans[65],*ptr;
  longlong dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (from_base < 0)
    dec= strtoll(res->c_ptr(),&endptr,-from_base);
  else
    dec= (longlong) strtoull(res->c_ptr(),&endptr,from_base);
  ptr= longlong2str(dec,ans,to_base);
  if (str->copy(ans,(uint32) (ptr-ans)))
    return &empty_string;
  return str;
}

#include <my_dir.h>				// For my_stat

String *Item_load_file::val_str(String *str)
{
  String *file_name;
  File file;
  MY_STAT stat_info;
  DBUG_ENTER("load_file");

  if (!(file_name= args[0]->val_str(str)) ||
      !(current_thd->master_access & FILE_ACL) ||
      !my_stat(file_name->c_ptr(), &stat_info, MYF(MY_WME)))
    goto err;
  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) max_allowed_packet)
  {
    /* my_error(ER_TOO_LONG_STRING, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (byte*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
  {
    my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length(stat_info.st_size);
  my_close(file, MYF(0));
  null_value = 0;
  return &tmp_value;

err:
  null_value = 1;
  DBUG_RETURN(0);
}


String* Item_func_export_set::val_str(String* str)
{
  ulonglong the_set = (ulonglong) args[0]->val_int();
  String yes_buf, *yes; 
  yes = args[1]->val_str(&yes_buf);
  String no_buf, *no; 
  no = args[2]->val_str(&no_buf);
  String *sep = NULL, sep_buf ; 

  uint num_set_values = 64;
  ulonglong mask = 0x1;
  str->length(0);

  /* Check if some argument is a NULL value */
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value=1;
    return 0;
  }
  switch(arg_count) {
  case 5:
    num_set_values = (uint) args[4]->val_int();
    if (num_set_values > 64)
      num_set_values=64;
    if (args[4]->null_value)
    {
      null_value=1;
      return 0;
    }
    /* Fall through */
  case 4:
    if (!(sep = args[3]->val_str(&sep_buf)))	// Only true if NULL
    {
      null_value=1;
      return 0;
    }
    break;
  case 3:
    sep_buf.set(",", 1);
    sep = &sep_buf;
  }
  null_value=0;

  for (uint i = 0; i < num_set_values; i++, mask = (mask << 1))
  {
    if (the_set & mask)
      str->append(*yes);
    else
      str->append(*no);
    if(i != num_set_values - 1)
      str->append(*sep);
  }
  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint length=max(args[1]->max_length,args[2]->max_length);
  uint sep_length=(arg_count > 3 ? args[3]->max_length : 1);
  max_length=length*64+sep_length*63;
}

String* Item_func_inet_ntoa::val_str(String* str)
{
  uchar buf[8], *p;
  ulonglong n = (ulonglong) args[0]->val_int();
  char num[4];
  // we do not know if args[0] is NULL until we have called
  // some val function on it if args[0] is not a constant!
  if ((null_value=args[0]->null_value))
    return 0;					// Null value
  str->length(0);
  int8store(buf,n);

  // now we can assume little endian
  // we handle the possibility of an 8-byte IP address
  // however, we do not want to confuse those who are just using
  // 4 byte ones
  
  for (p= buf + 8; p > buf+4 && p[-1] == 0 ; p-- ) ;
  num[3]='.';
  while (p-- > buf)
  {
    uint c = *p;
    uint n1,n2;					// Try to avoid divisions
    n1= c / 100;				// 100 digits
    c-= n1*100;
    n2= c / 10;					// 10 digits
    c-=n2*10;					// last digit
    num[0]=(char) n1+'0';
    num[1]=(char) n2+'0';
    num[2]=(char) c+'0';
    uint length=(n1 ? 4 : n2 ? 3 : 2);		// Remove pre-zero

    (void) str->append(num+4-length,length);
  }
  str->length(str->length()-1);			// Remove last '.';
  return str;
}
