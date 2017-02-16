/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/* Functions to handle typelib */

#include "mysys_priv.h"
#include <m_string.h>
#include <m_ctype.h>


#define is_field_separator(F, X) \
  ((F & FIND_TYPE_COMMA_TERM) && ((X) == ',' || (X) == '='))

int find_type_with_warning(const char *x, TYPELIB *typelib, const char *option)
{
  int res;
  const char **ptr;

  if ((res= find_type((char *) x, typelib, FIND_TYPE_BASIC)) <= 0)
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
  }
  return res;
}


int find_type_or_exit(const char *x, TYPELIB *typelib, const char *option)
{
  int res;
  if ((res= find_type_with_warning(x, typelib, option)) <= 0)
  {
    sf_leaking_memory= 1; /* no memory leak reports here */
    exit(1);
  }
  return res;
}


/**
  Search after a string in a list of strings. Endspace in x is not compared.

  @param x              pointer to string to find
                        (not necessarily zero-terminated).
                        by return it'll be advanced to point to the terminator.
  @param typelib        TYPELIB (struct of pointer to values + count)
  @param flags          flags to tune behaviour: a combination of
                        FIND_TYPE_NO_PREFIX
                        FIND_TYPE_COMMA_TERM.
  @param eol            a pointer to the end of the string.

  @retval
    -1  Too many matching values
  @retval
    0   No matching value
  @retval
    >0  Offset+1 in typelib for matched string
*/


static int find_type_eol(const char **x, const TYPELIB *typelib, uint flags,
                         const char *eol)
{
  int find,pos;
  int UNINIT_VAR(findpos);                       /* guarded by find */
  const char *UNINIT_VAR(termptr);
  const char *i;
  const char *j;
  CHARSET_INFO *cs= &my_charset_latin1;
  DBUG_ENTER("find_type_eol");
  DBUG_PRINT("enter",("x: '%s'  lib: 0x%lx", *x, (long) typelib));

  DBUG_ASSERT(!(flags & ~(FIND_TYPE_NO_PREFIX | FIND_TYPE_COMMA_TERM)));

  if (!typelib->count)
  {
    DBUG_PRINT("exit",("no count"));
    DBUG_RETURN(0);
  }
  find=0;
  for (pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    for (i=*x ; 
         i < eol && !is_field_separator(flags, *i) &&
         my_toupper(cs, *i) == my_toupper(cs, *j) ; i++, j++) ;
    if (! *j)
    {
      while (i < eol && *i == ' ')
	i++;					/* skip_end_space */
      if (i >= eol || is_field_separator(flags, *i))
      {
        *x= i;
	DBUG_RETURN(pos+1);
      }
    }
    if ((i >= eol && !is_field_separator(flags, *i)) &&
        (!*j || !(flags & FIND_TYPE_NO_PREFIX)))
    {
      find++;
      findpos=pos;
      termptr=i;
    }
  }
  if (find == 0 || *x == eol)
  {
    DBUG_PRINT("exit",("Couldn't find type"));
    DBUG_RETURN(0);
  }
  else if (find != 1 || (flags & FIND_TYPE_NO_PREFIX))
  {
    DBUG_PRINT("exit",("Too many possibilities"));
    DBUG_RETURN(-1);
  }
  *x= termptr;
  DBUG_RETURN(findpos+1);
} /* find_type_eol */


/**
  Search after a string in a list of strings. Endspace in x is not compared.

  Same as find_type_eol, but for zero-terminated strings,
  and without advancing the pointer.
*/
int find_type(const char *x, const TYPELIB *typelib, uint flags)
{
  return find_type_eol(&x, typelib, flags, x + strlen(x));
}

/**
  Get name of type nr
 
  @note
  first type is 1, 0 = empty field
*/

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


/**
  Get type

  @note
  first type is 0
*/

const char *get_type(TYPELIB *typelib, uint nr)
{
  if (nr < (uint) typelib->count && typelib->type_names)
    return(typelib->type_names[nr]);
  return "?";
}


/**
  Create an integer value to represent the supplied comma-seperated
  string where each string in the TYPELIB denotes a bit position.

  @param x      string to decompose
  @param lib    TYPELIB (struct of pointer to values + count)
  @param err    index (not char position) of string element which was not 
                found or 0 if there was no error

  @retval
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
    while (*x && *x != ',')
      x++;
    if (x[0] && x[1])      /* skip separator if found */
      x++;
    if ((find= find_type(i, lib, FIND_TYPE_COMMA_TERM) - 1) < 0)
      DBUG_RETURN(0);
    result|= (ULL(1) << find);
  }
  *err= 0;
  DBUG_RETURN(result);
} /* find_set */


/**
  Create a copy of a specified TYPELIB structure.

  @param root   pointer to a MEM_ROOT object for allocations
  @param from   pointer to a source TYPELIB structure

  @retval
    pointer to the new TYPELIB structure on successful copy
  @retval
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


static const char *on_off_default_names[]= { "off","on","default", 0};
static TYPELIB on_off_default_typelib= {array_elements(on_off_default_names)-1,
                                        "", on_off_default_names, 0};

/**
  Parse a TYPELIB name from the buffer

  @param lib          Set of names to scan for.
  @param strpos INOUT Start of the buffer (updated to point to the next
                      character after the name)
  @param end          End of the buffer

  @note
  The buffer is assumed to contain one of the names specified in the TYPELIB,
  followed by comma, '=', or end of the buffer.

  @retval
    0   No matching name
  @retval
    >0  Offset+1 in typelib for matched name
*/

static uint parse_name(const TYPELIB *lib, const char **pos, const char *end)
{
  uint find= find_type_eol(pos, lib,
                           FIND_TYPE_COMMA_TERM | FIND_TYPE_NO_PREFIX, end);
  return find;
}

/**
  Parse and apply a set of flag assingments

  @param lib               Flag names
  @param default_name      Number of "default" in the typelib
  @param cur_set           Current set of flags (start from this state)
  @param default_set       Default set of flags (use this for assign-default
                           keyword and flag=default assignments)
  @param str               String to be parsed
  @param length            Length of the string
  @param err_pos      OUT  If error, set to point to start of wrong set string
                           NULL on success
  @param err_len      OUT  If error, set to the length of wrong set string

  @details
  Parse a set of flag assignments, that is, parse a string in form:

    param_name1=value1,param_name2=value2,... 
  
  where the names are specified in the TYPELIB, and each value can be
  either 'on','off', or 'default'. Setting the same name twice is not 
  allowed.
  
  Besides param=val assignments, we support the "default" keyword (keyword 
  #default_name in the typelib). It can be used one time, if specified it 
  causes us to build the new set over the default_set rather than cur_set
  value.

  @note
  it's not charset aware

  @retval
    Parsed set value if (*errpos == NULL), otherwise undefined
*/

my_ulonglong find_set_from_flags(const TYPELIB *lib, uint default_name,
                              my_ulonglong cur_set, my_ulonglong default_set,
                              const char *str, uint length,
                              char **err_pos, uint *err_len)
{
  const char *end= str + length;
  my_ulonglong flags_to_set= 0, flags_to_clear= 0, res;
  my_bool set_defaults= 0;

  *err_pos= 0;                  /* No error yet */
  if (str != end)
  {
    const char *start= str;    
    for (;;)
    {
      const char *pos= start;
      uint flag_no, value;

      if (!(flag_no= parse_name(lib, &pos, end)))
        goto err;

      if (flag_no == default_name)
      {
        /* Using 'default' twice isn't allowed. */
        if (set_defaults)
          goto err;
        set_defaults= TRUE;
      }
      else
      {
        my_ulonglong bit=  (1ULL << (flag_no - 1));
        /* parse the '=on|off|default' */
        if ((flags_to_clear | flags_to_set) & bit ||
            pos >= end || *pos++ != '=' ||
            !(value= parse_name(&on_off_default_typelib, &pos, end)))
          goto err;
        
        if (value == 1) /* this is '=off' */
          flags_to_clear|= bit;
        else if (value == 2) /* this is '=on' */
          flags_to_set|= bit;
        else /* this is '=default'  */
        {
          if (default_set & bit)
            flags_to_set|= bit;
          else
            flags_to_clear|= bit;
        }
      }
      if (pos >= end)
        break;

      if (*pos++ != ',')
        goto err;

      start=pos;
      continue;
   err:
      *err_pos= (char*)start;
      *err_len= end - start;
      break;
    }
  }
  res= set_defaults? default_set : cur_set;
  res|= flags_to_set;
  res&= ~flags_to_clear;
  return res;
}

