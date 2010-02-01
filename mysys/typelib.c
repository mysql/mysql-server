/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Functions to handle typelib */

#include "mysys_priv.h"
#include <m_string.h>
#include <m_ctype.h>


static const char field_separator=',';

int find_type_or_exit(const char *x, TYPELIB *typelib, const char *option)
{
  int res;
  const char **ptr;

  if ((res= find_type((char *) x, typelib, 2)) <= 0)
  {
    ptr= typelib->type_names;
    if (!*x)
      fprintf(stderr, "No option given to %s\n", option);
    else
      fprintf(stderr, "Unknown option to %s: %s\n", option, x);
    fprintf(stderr, "Alternatives are: '%s'", *ptr);
    while (*++ptr)
      fprintf(stderr, ",'%s'", *ptr);
    fprintf(stderr, "\n");
    exit(1);
  }
  return res;
}


/*
  Search after a string in a list of strings. Endspace in x is not compared.

  SYNOPSIS
   find_type()
   x			String to find
   lib			TYPELIB (struct of pointer to values + count)
   full_name		bitmap of what to do
			If & 1 accept only whole names
			If & 2 don't expand if half field
			If & 4 allow #number# as type
			If & 8 use ',' as string terminator

  NOTES
    If part, uniq field is found and full_name == 0 then x is expanded
    to full field.

  RETURN
    -1	Too many matching values
    0	No matching value
    >0  Offset+1 in typelib for matched string
*/


int find_type(char *x, const TYPELIB *typelib, uint full_name)
{
  int find,pos;
  int UNINIT_VAR(findpos);                       /* guarded by find */
  reg1 char * i;
  reg2 const char *j;
  DBUG_ENTER("find_type");
  DBUG_PRINT("enter",("x: '%s'  lib: 0x%lx", x, (long) typelib));

  if (!typelib->count)
  {
    DBUG_PRINT("exit",("no count"));
    DBUG_RETURN(0);
  }
  find=0;
  for (pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    for (i=x ; 
    	*i && (!(full_name & 8) || *i != field_separator) &&
        my_toupper(&my_charset_latin1,*i) == 
    		my_toupper(&my_charset_latin1,*j) ; i++, j++) ;
    if (! *j)
    {
      while (*i == ' ')
	i++;					/* skip_end_space */
      if (! *i || ((full_name & 8) && *i == field_separator))
	DBUG_RETURN(pos+1);
    }
    if ((!*i && (!(full_name & 8) || *i != field_separator)) && 
        (!*j || !(full_name & 1)))
    {
      find++;
      findpos=pos;
    }
  }
  if (find == 0 && (full_name & 4) && x[0] == '#' && strend(x)[-1] == '#' &&
      (findpos=atoi(x+1)-1) >= 0 && (uint) findpos < typelib->count)
    find=1;
  else if (find == 0 || ! x[0])
  {
    DBUG_PRINT("exit",("Couldn't find type"));
    DBUG_RETURN(0);
  }
  else if (find != 1 || (full_name & 1))
  {
    DBUG_PRINT("exit",("Too many possybilities"));
    DBUG_RETURN(-1);
  }
  if (!(full_name & 2))
    (void) strmov(x,typelib->type_names[findpos]);
  DBUG_RETURN(findpos+1);
} /* find_type */


	/* Get name of type nr 'nr' */
	/* Warning first type is 1, 0 = empty field */

void make_type(register char * to, register uint nr,
	       register TYPELIB *typelib)
{
  DBUG_ENTER("make_type");
  if (!nr)
    to[0]=0;
  else
    (void) strmov(to,get_type(typelib,nr-1));
  DBUG_VOID_RETURN;
} /* make_type */


	/* Get type */
	/* Warning first type is 0 */

const char *get_type(TYPELIB *typelib, uint nr)
{
  if (nr < (uint) typelib->count && typelib->type_names)
    return(typelib->type_names[nr]);
  return "?";
}


/*
  Create an integer value to represent the supplied comma-seperated
  string where each string in the TYPELIB denotes a bit position.

  SYNOPSIS
    find_typeset()
    x		string to decompose
    lib		TYPELIB (struct of pointer to values + count)
    err		index (not char position) of string element which was not 
                found or 0 if there was no error

  RETURN
    a integer representation of the supplied string
*/

my_ulonglong find_typeset(char *x, TYPELIB *lib, int *err)
{
  my_ulonglong result;
  int find;
  char *i;
  DBUG_ENTER("find_set");
  DBUG_PRINT("enter",("x: '%s'  lib: 0x%lx", x, (long) lib));

  if (!lib->count)
  {
    DBUG_PRINT("exit",("no count"));
    DBUG_RETURN(0);
  }
  result= 0;
  *err= 0;
  while (*x)
  {
    (*err)++;
    i= x;
    while (*x && *x != field_separator)
      x++;
    if (x[0] && x[1])                            /* skip separator if found */
      x++;
    if ((find= find_type(i, lib, 2 | 8) - 1) < 0)
      DBUG_RETURN(0);
    result|= (ULL(1) << find);
  }
  *err= 0;
  DBUG_RETURN(result);
} /* find_set */


/*
  Create a copy of a specified TYPELIB structure.

  SYNOPSIS
    copy_typelib()
    root	pointer to a MEM_ROOT object for allocations
    from	pointer to a source TYPELIB structure

  RETURN
    pointer to the new TYPELIB structure on successful copy, or
    NULL otherwise
*/

TYPELIB *copy_typelib(MEM_ROOT *root, TYPELIB *from)
{
  TYPELIB *to;
  uint i;

  if (!from)
    return NULL;

  if (!(to= (TYPELIB*) alloc_root(root, sizeof(TYPELIB))))
    return NULL;

  if (!(to->type_names= (const char **)
        alloc_root(root, (sizeof(char *) + sizeof(int)) * (from->count + 1))))
    return NULL;
  to->type_lengths= (unsigned int *)(to->type_names + from->count + 1);
  to->count= from->count;
  if (from->name)
  {
    if (!(to->name= strdup_root(root, from->name)))
      return NULL;
  }
  else
    to->name= NULL;

  for (i= 0; i < from->count; i++)
  {
    if (!(to->type_names[i]= strmake_root(root, from->type_names[i],
                                          from->type_lengths[i])))
      return NULL;
    to->type_lengths[i]= from->type_lengths[i];
  }
  to->type_names[to->count]= NULL;
  to->type_lengths[to->count]= 0;

  return to;
}
