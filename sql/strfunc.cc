/* Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Some useful string utility functions used by the MySQL server */

#include "strfunc.h"
#include "sql_class.h"
#include "typelib.h"                            // TYPELIB
#include "m_ctype.h"                            // my_charset_latin1
#include "mysqld.h"                             // system_charset_info

/*
  Return bitmap for strings used in a set

  SYNOPSIS
  find_set()
  lib			Strings in set
  str			Strings of set-strings separated by ','
  err_pos		If error, set to point to start of wrong set string
  err_len		If error, set to the length of wrong set string
  set_warning		Set to 1 if some string in set couldn't be used

  NOTE
    We delete all end space from str before comparison

  RETURN
    bitmap of all sets found in x.
    set_warning is set to 1 if there was any sets that couldn't be set
*/

static const char field_separator=',';

ulonglong find_set(TYPELIB *lib, const char *str, size_t length,
                   const CHARSET_INFO *cs,
                   char **err_pos, uint *err_len, bool *set_warning)
{
  const CHARSET_INFO *strip= cs ? cs : &my_charset_latin1;
  const char *end= str + strip->cset->lengthsp(strip, str, length);
  ulonglong found= 0;
  *err_pos= 0;                  // No error yet
  *err_len= 0;
  if (str != end)
  {
    const char *start= str;    
    for (;;)
    {
      const char *pos= start;
      uint var_len;
      int mblen= 1;

      if (cs && cs->mbminlen > 1)
      {
        for ( ; pos < end; pos+= mblen)
        {
          my_wc_t wc;
          if ((mblen= cs->cset->mb_wc(cs, &wc, (const uchar *) pos, 
                                               (const uchar *) end)) < 1)
            mblen= 1; // Not to hang on a wrong multibyte sequence
          if (wc == (my_wc_t) field_separator)
            break;
        }
      }
      else
        for (; pos != end && *pos != field_separator; pos++) ;
      var_len= (uint) (pos - start);
      uint find= cs ? find_type2(lib, start, var_len, cs) :
                      find_type(lib, start, var_len, (bool) 0);
      if (!find && *err_len == 0) // report the first error with length > 0
      {
        *err_pos= (char*) start;
        *err_len= var_len;
        *set_warning= 1;
      }
      else
        found|= 1ULL << (find - 1);
      if (pos >= end)
        break;
      start= pos + mblen;
    }
  }
  return found;
}

/*
  Function to find a string in a TYPELIB
  (similar to find_type() of mysys/typelib.c)

  SYNOPSIS
   find_type()
   lib			TYPELIB (struct of pointer to values + count)
   find			String to find
   length		Length of string to find
   part_match		Allow part matching of value

 RETURN
  0 error
  > 0 position in TYPELIB->type_names +1
*/

uint find_type(const TYPELIB *lib, const char *find, size_t length,
               bool part_match)
{
  uint found_count=0, found_pos=0;
  const char *end= find+length;
  const char *i;
  const char *j;
  for (uint pos=0 ; (j=lib->type_names[pos++]) ; )
  {
    for (i=find ; i != end && 
	   my_toupper(system_charset_info,*i) == 
	   my_toupper(system_charset_info,*j) ; i++, j++) ;
    if (i == end)
    {
      if (! *j)
	return(pos);
      found_count++;
      found_pos= pos;
    }
  }
  return(found_count == 1 && part_match ? found_pos : 0);
}


/*
  Find a string in a list of strings according to collation

  SYNOPSIS
   find_type2()
   lib			TYPELIB (struct of pointer to values + count)
   x			String to find
   length               String length
   cs			Character set + collation to use for comparison

  NOTES

  RETURN
    0	No matching value
    >0  Offset+1 in typelib for matched string
*/

uint find_type2(const TYPELIB *typelib, const char *x, size_t length,
                const CHARSET_INFO *cs)
{
  int pos;
  const char *j;
  DBUG_ENTER("find_type2");
  DBUG_PRINT("enter",("x: '%.*s'  lib: 0x%p",
                      static_cast<int>(length), x, typelib));

  if (!typelib->count)
  {
    DBUG_PRINT("exit",("no count"));
    DBUG_RETURN(0);
  }

  for (pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    if (!my_strnncoll(cs, (const uchar*) x, length,
                          (const uchar*) j, typelib->type_lengths[pos]))
      DBUG_RETURN(pos+1);
  }
  DBUG_PRINT("exit",("Couldn't find type"));
  DBUG_RETURN(0);
} /* find_type */


/*
  Un-hex all elements in a typelib

  SYNOPSIS
   unhex_type2()
   interval       TYPELIB (struct of pointer to values + lengths + count)

  NOTES

  RETURN
    N/A
*/

void unhex_type2(TYPELIB *interval)
{
  for (uint pos= 0; pos < interval->count; pos++)
  {
    char *from, *to;
    for (from= to= (char*) interval->type_names[pos]; *from; )
    {
      /*
        Note, hexchar_to_int(*from++) doesn't work
        one some compilers, e.g. IRIX. Looks like a compiler
        bug in inline functions in combination with arguments
        that have a side effect. So, let's use from[0] and from[1]
        and increment 'from' by two later.
      */

      *to++= (char) (hexchar_to_int(from[0]) << 4) +
                     hexchar_to_int(from[1]);
      from+= 2;
    }
    interval->type_lengths[pos] /= 2;
  }
}


/*
  Check if the first word in a string is one of the ones in TYPELIB

  SYNOPSIS
    check_word()
    lib		TYPELIB
    val		String to check
    end		End of input
    end_of_word	Store value of last used byte here if we found word

  RETURN
    0	 No matching value
    > 1  lib->type_names[#-1] matched
	 end_of_word will point to separator character/end in 'val'
*/

uint check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word)
{
  int res;
  const char *ptr;

  /* Fiend end of word */
  for (ptr= val ; ptr < end && my_isalpha(&my_charset_latin1, *ptr) ; ptr++)
    ;
  if ((res=find_type(lib, val, (uint) (ptr - val), 1)) > 0)
    *end_of_word= ptr;
  return res;
}


/*
  Converts a string between character sets

  SYNOPSIS
    strconvert()
    from_cs       source character set
    from          source, a null terminated string
    to            destination buffer
    to_length     destination buffer length

  NOTES
    'to' is always terminated with a '\0' character.
    If there is no enough space to convert whole string,
    only prefix is converted, and terminated with '\0'.

  RETURN VALUES
    result string length
*/


size_t strconvert(CHARSET_INFO *from_cs, const char *from,
                  CHARSET_INFO *to_cs, char *to, size_t to_length, uint *errors)
{
  int cnvres;
  my_wc_t wc;
  char *to_start= to;
  uchar *to_end= (uchar*) to + to_length - 1;
  my_charset_conv_mb_wc mb_wc= from_cs->cset->mb_wc;
  my_charset_conv_wc_mb wc_mb= to_cs->cset->wc_mb;
  uint error_count= 0;

  while (1)
  {
    /*
      Using 'from + 10' is safe:
      - it is enough to scan a single character in any character set.
      - if remaining string is shorter than 10, then mb_wc will return
        with error because of unexpected '\0' character.
    */
    if ((cnvres= (*mb_wc)(from_cs, &wc,
                          (uchar*) from, (uchar*) from + 10)) > 0)
    {
      if (!wc)
        break;
      from+= cnvres;
    }
    else if (cnvres == MY_CS_ILSEQ)
    {
      error_count++;
      from++;
      wc= '?';
    }
    else
      break; // Impossible char.

outp:

    if ((cnvres= (*wc_mb)(to_cs, wc, (uchar*) to, to_end)) > 0)
      to+= cnvres;
    else if (cnvres == MY_CS_ILUNI && wc != '?')
    {
      error_count++;
      wc= '?';
      goto outp;
    }
    else
      break;
  }
  *to= '\0';
  *errors= error_count;
  return static_cast<size_t>(to - to_start);

}


/*
  Searches for a LEX_STRING in an LEX_STRING array.

  SYNOPSIS
    find_string_in_array()
      heap    The array
      needle  The string to search for

  NOTE
    The last LEX_STRING in the array should have str member set to NULL

  RETURN VALUES
    -1   Not found
    >=0  Ordinal position
*/

int find_string_in_array(LEX_STRING * const haystack, LEX_STRING * const needle,
                         CHARSET_INFO * const cs)
{
  const LEX_STRING *pos;
  for (pos= haystack; pos->str; pos++)
    if (!cs->coll->strnncollsp(cs, (uchar *) pos->str, pos->length,
                               (uchar *) needle->str, needle->length, 0))
    {
      return static_cast<int>(pos - haystack);
    }
  return -1;
}


char *set_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                    const char *lib[])
{
  char buff[STRING_BUFFER_USUAL_SIZE*8];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  LEX_STRING unused;

  if (!result)
    result= &unused;

  tmp.length(0);

  for (uint i= 0; set; i++, set >>= 1)
    if (set & 1) {
      tmp.append(lib[i]);
      tmp.append(',');
    }

  if (tmp.length())
  {
    result->str=    thd->strmake(tmp.ptr(), tmp.length()-1);
    result->length= tmp.length()-1;
  }
  else
  {
    result->str= const_cast<char*>("");
    result->length= 0;
  }
  return result->str;
}

char *flagset_to_string(THD *thd, LEX_STRING *result, ulonglong set,
                        const char *lib[])
{
  char buff[STRING_BUFFER_USUAL_SIZE*8];
  String tmp(buff, sizeof(buff), &my_charset_latin1);
  LEX_STRING unused;

  if (!result) result= &unused;

  tmp.length(0);

  // note that the last element is always "default", and it's ignored below
  for (uint i= 0; lib[i+1]; i++, set >>= 1)
  {
    tmp.append(lib[i]);
    tmp.append(set & 1 ? "=on," : "=off,");
  }

  result->str=    thd->strmake(tmp.ptr(), tmp.length()-1);
  result->length= tmp.length()-1;

  return result->str;
}

