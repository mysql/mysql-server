/* Copyright (C) 2003 MySQL AB

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

/* Some useful string utility functions used by the MySQL server */

#include "mysql_priv.h"

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

ulonglong find_set(TYPELIB *lib, const char *str, uint length, CHARSET_INFO *cs,
                   char **err_pos, uint *err_len, bool *set_warning)
{
  CHARSET_INFO *strip= cs ? cs : &my_charset_latin1;
  const char *end= str + strip->cset->lengthsp(strip, str, length);
  ulonglong found= 0;
  *err_pos= 0;                  // No error yet
  if (str != end)
  {
    const char *start= str;    
    for (;;)
    {
      const char *pos= start;
      uint var_len;

      for (; pos != end && *pos != field_separator; pos++) ;
      var_len= (uint) (pos - start);
      uint find= cs ? find_type2(lib, start, var_len, cs) :
                      find_type(lib, start, var_len, (bool) 0);
      if (!find)
      {
        *err_pos= (char*) start;
        *err_len= var_len;
        *set_warning= 1;
      }
      else
        found|= ((longlong) 1 << (find - 1));
      if (pos == end)
        break;
      start= pos + 1;
    }
  }
  return found;
}


/*
  Function to find a string in a TYPELIB
  (Same format as mysys/typelib.c)

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

uint find_type(TYPELIB *lib, const char *find, uint length, bool part_match)
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

uint find_type2(TYPELIB *typelib, const char *x, uint length, CHARSET_INFO *cs)
{
  int find,pos,findpos;
  const char *j;
  DBUG_ENTER("find_type2");
  DBUG_PRINT("enter",("x: '%.*s'  lib: 0x%lx", length, x, typelib));

  if (!typelib->count)
  {
    DBUG_PRINT("exit",("no count"));
    DBUG_RETURN(0);
  }
  LINT_INIT(findpos);
  for (find=0, pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    if (!my_strnncoll(cs, (const uchar*) x, length,
                          (const uchar*) j, typelib->type_lengths[pos]))
      DBUG_RETURN(pos+1);
  }
  DBUG_PRINT("exit",("Couldn't find type"));
  DBUG_RETURN(0);
} /* find_type */


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
