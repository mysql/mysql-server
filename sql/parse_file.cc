/* Copyright (C) 2004 MySQL AB

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

// Text .frm files management routines

#include "mysql_priv.h"
#include <errno.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <my_dir.h>


/*
  write string with escaping

  SYNOPSIS
    write_escaped_string()
    file	- IO_CACHE for record
    val_s	- string for writing

  RETURN
    FALSE - OK
    TRUE  - error
*/

static my_bool
write_escaped_string(IO_CACHE *file, LEX_STRING *val_s)
{
  char *eos= val_s->str + val_s->length;
  char *ptr= val_s->str;

  for (; ptr < eos; ptr++)
  {
    /*
      Should be in sync with read_escaped_string() and
      parse_quoted_escaped_string()
    */
    switch(*ptr) {
    case '\\': // escape character
      if (my_b_append(file, (const byte *)"\\\\", 2))
	return TRUE;
      break;
    case '\n': // parameter value delimiter
      if (my_b_append(file, (const byte *)"\\n", 2))
	return TRUE;
      break;
    case '\0': // problem for some string processing utilities
      if (my_b_append(file, (const byte *)"\\0", 2))
	return TRUE;
      break;
    case 26: // problem for windows utilities (Ctrl-Z)
      if (my_b_append(file, (const byte *)"\\z", 2))
	return TRUE;
      break;
    case '\'': // list of string delimiter
      if (my_b_append(file, (const byte *)"\\\'", 2))
	return TRUE;
      break;
    default:
      if (my_b_append(file, (const byte *)ptr, 1))
	return TRUE;
    }
  }
  return FALSE;
}


/*
  write parameter value to IO_CACHE

  SYNOPSIS
    write_parameter()
    file	pointer to IO_CACHE structure for writing
    base	pointer to data structure
    parameter	pointer to parameter descriptor
    old_version	for returning back old version number value

  RETURN
    FALSE - OK
    TRUE  - error
*/

static my_bool
write_parameter(IO_CACHE *file, gptr base, File_option *parameter,
		ulonglong *old_version)
{
  char num_buf[20];			// buffer for numeric operations
  // string for numeric operations
  String num(num_buf, sizeof(num_buf), &my_charset_bin);
  DBUG_ENTER("write_parameter");

  switch (parameter->type) {
  case FILE_OPTIONS_STRING:
  {
    LEX_STRING *val_s= (LEX_STRING *)(base + parameter->offset);
    if (my_b_append(file, (const byte *)val_s->str, val_s->length))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_ESTRING:
  {
    if (write_escaped_string(file, (LEX_STRING *)(base + parameter->offset)))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_ULONGLONG:
  {
    num.set(*((ulonglong *)(base + parameter->offset)), &my_charset_bin);
    if (my_b_append(file, (const byte *)num.ptr(), num.length()))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_REV:
  {
    ulonglong *val_i= (ulonglong *)(base + parameter->offset);
    *old_version= (*val_i)++;
    num.set(*val_i, &my_charset_bin);
    if (my_b_append(file, (const byte *)num.ptr(), num.length()))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_TIMESTAMP:
  {
    /* string have to be allocated already */
    LEX_STRING *val_s= (LEX_STRING *)(base + parameter->offset);
    time_t tm= time(NULL);

    get_date(val_s->str, GETDATE_DATE_TIME|GETDATE_GMT|GETDATE_FIXEDLENGTH,
	     tm);
    val_s->length= PARSE_FILE_TIMESTAMPLENGTH;
    if (my_b_append(file, (const byte *)val_s->str,
                    PARSE_FILE_TIMESTAMPLENGTH))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_STRLIST:
  {
    List_iterator_fast<LEX_STRING> it(*((List<LEX_STRING>*)
					(base + parameter->offset)));
    bool first= 1;
    LEX_STRING *str;
    while ((str= it++))
    {
      // We need ' ' after string to detect list continuation
      if ((!first && my_b_append(file, (const byte *)" ", 1)) ||
	  my_b_append(file, (const byte *)"\'", 1) ||
          write_escaped_string(file, str) ||
	  my_b_append(file, (const byte *)"\'", 1))
      {
	DBUG_RETURN(TRUE);
      }
      first= 0;
    }
    break;
  }
  default:
    DBUG_ASSERT(0); // never should happened
  }
  DBUG_RETURN(FALSE);
}


/*
  write new .frm

  SYNOPSIS
    sql_create_definition_file()
    dir			directory where put .frm
    file		.frm file name
    type		.frm type string (VIEW, TABLE)
    base		base address for parameter reading (structure like
			TABLE)
    parameters		parameters description
    max_versions	number of versions to save

  RETURN
    FALSE - OK
    TRUE - error
*/

my_bool
sql_create_definition_file(const LEX_STRING *dir, const LEX_STRING *file_name,
			   const LEX_STRING *type,
			   gptr base, File_option *parameters,
			   uint max_versions)
{
  File handler;
  IO_CACHE file;
  char path[FN_REFLEN+1];	// +1 to put temporary file name for sure
  ulonglong old_version= ULONGLONG_MAX;
  int path_end;
  File_option *param;
  DBUG_ENTER("sql_create_definition_file");
  DBUG_PRINT("enter", ("Dir: %s, file: %s, base 0x%lx",
		       dir->str, file_name->str, (ulong) base));

  fn_format(path, file_name->str, dir->str, 0, MY_UNPACK_FILENAME);
  path_end= strlen(path);

  // temporary file name
  path[path_end]='~';
  path[path_end+1]= '\0';
  if ((handler= my_create(path, CREATE_MODE, O_RDWR | O_TRUNC,
			  MYF(MY_WME))) <= 0)
  {
    DBUG_RETURN(TRUE);
  }

  if (init_io_cache(&file, handler, 0, SEQ_READ_APPEND, 0L, 0, MYF(MY_WME)))
    goto err_w_file;

  // write header (file signature)
  if (my_b_append(&file, (const byte *)"TYPE=", 5) ||
      my_b_append(&file, (const byte *)type->str, type->length) ||
      my_b_append(&file, (const byte *)"\n", 1))
    goto err_w_file;

  // write parameters to temporary file
  for (param= parameters; param->name.str; param++)
  {
    if (my_b_append(&file, (const byte *)param->name.str,
                    param->name.length) ||
	my_b_append(&file, (const byte *)"=", 1) ||
	write_parameter(&file, base, param, &old_version) ||
	my_b_append(&file, (const byte *)"\n", 1))
      goto err_w_cache;
  }

  if (end_io_cache(&file))
    goto err_w_file;

  if (my_close(handler, MYF(MY_WME)))
  {
    DBUG_RETURN(TRUE);
  }

  // archive copies management
  path[path_end]='\0';
  if (!access(path, F_OK))
  {
    if (old_version != ULONGLONG_MAX && max_versions != 0)
    {
      // save backup
      char path_arc[FN_REFLEN];
      // backup old version
      char path_to[FN_REFLEN];

      // check archive directory existence
      fn_format(path_arc, "arc", dir->str, "", MY_UNPACK_FILENAME);
      if (access(path_arc, F_OK))
      {
	if (my_mkdir(path_arc, 0777, MYF(MY_WME)))
	{
	  DBUG_RETURN(TRUE);
	}
      }

      my_snprintf(path_to, FN_REFLEN, "%s/%s-%04lu",
		  path_arc, file_name->str, (ulong) old_version);
      if (my_rename(path, path_to, MYF(MY_WME)))
      {
	DBUG_RETURN(TRUE);
      }

      // remove very old version
      if (old_version > max_versions)
      {
	my_snprintf(path_to, FN_REFLEN, "%s/%s-%04lu",
		    path_arc, file_name->str,
		    (ulong)(old_version - max_versions));
	if (!access(path_arc, F_OK) && my_delete(path_to, MYF(MY_WME)))
	{
	  DBUG_RETURN(TRUE);
	}
      }
    }
    else
    {
      if (my_delete(path, MYF(MY_WME)))	// no backups
      {
	DBUG_RETURN(TRUE);
      }
    }
  }

  {
    // rename temporary file
    char path_to[FN_REFLEN];
    memcpy(path_to, path, path_end+1);
    path[path_end]='~';
    if (my_rename(path, path_to, MYF(MY_WME)))
    {
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
err_w_cache:
  end_io_cache(&file);
err_w_file:
  my_close(handler, MYF(MY_WME));
  DBUG_RETURN(TRUE);
}


/*
  Prepare frm to parse (read to memory)

  SYNOPSIS
    sql_parse_prepare()
    file_name		- path & filename to .frm file
    mem_root		- MEM_ROOT for buffer allocation
    bad_format_errors	- send errors on bad content

  RETURN
    0 - error
    parser object

  NOTE
    returned pointer + 1 will be type of .frm
*/

File_parser * 
sql_parse_prepare(const LEX_STRING *file_name, MEM_ROOT *mem_root,
		  bool bad_format_errors)
{
  MY_STAT stat_info;
  uint len;
  char *end, *sign;
  File_parser *parser;
  File file;
  DBUG_ENTER("sql__parse_prepare");

  if (!my_stat(file_name->str, &stat_info, MYF(MY_WME)))
  {
    DBUG_RETURN(0);
  }

  if (stat_info.st_size > INT_MAX-1)
  {
    my_error(ER_FPARSER_TOO_BIG_FILE, MYF(0), file_name->str);
    DBUG_RETURN(0);
  }

  if (!(parser= new(mem_root) File_parser))
  {
    DBUG_RETURN(0);
  }

  if (!(parser->buff= alloc_root(mem_root, stat_info.st_size+1)))
  {
    DBUG_RETURN(0);
  }

  if ((file= my_open(file_name->str, O_RDONLY | O_SHARE, MYF(MY_WME))) < 0)
  {
    DBUG_RETURN(0);
  }
  
  if ((len= my_read(file, (byte *)parser->buff,
                    stat_info.st_size, MYF(MY_WME))) ==
      MY_FILE_ERROR)
  {
    my_close(file, MYF(MY_WME));
    DBUG_RETURN(0);
  }

  if (my_close(file, MYF(MY_WME)))
  {
    DBUG_RETURN(0);
  }

  end= parser->end= parser->buff + len;
  *end= '\0'; // barrier for more simple parsing

  // 7 = 5 (TYPE=) + 1 (letter at least of type name) + 1 ('\n')
  if (len < 7 ||
      parser->buff[0] != 'T' ||
      parser->buff[1] != 'Y' ||
      parser->buff[2] != 'P' ||
      parser->buff[3] != 'E' ||
      parser->buff[4] != '=')
    goto frm_error;

  // skip signature;
  parser->file_type.str= sign= parser->buff + 5;
  while (*sign >= 'A' && *sign <= 'Z' && sign < end)
    sign++;
  if (*sign != '\n')
    goto frm_error;
  parser->file_type.length= sign - parser->file_type.str;
  // EOS for file signature just for safety
  *sign= '\0';

  parser->start= sign + 1;
  parser->content_ok= 1;

  DBUG_RETURN(parser);

frm_error:
  if (bad_format_errors)
  {
    my_error(ER_FPARSER_BAD_HEADER, MYF(0), file_name->str);
    DBUG_RETURN(0);
  }
  else
    DBUG_RETURN(parser); // upper level have to check parser->ok()
}


/*
  parse LEX_STRING

  SYNOPSIS
    parse_string()
    ptr		- pointer on string beginning
    end		- pointer on symbol after parsed string end (still owned
		  by buffer and can be accessed
    mem_root	- MEM_ROOT for parameter allocation
    str		- pointer on string, where results should be stored

  RETURN
    0	- error
    #	- pointer on symbol after string
*/

static char *
parse_string(char *ptr, char *end, MEM_ROOT *mem_root, LEX_STRING *str)
{
  // get string length
  char *eol= strchr(ptr, '\n');

  if (eol >= end)
    return 0;

  str->length= eol - ptr;

  if (!(str->str= alloc_root(mem_root, str->length+1)))
    return 0;

  memcpy(str->str, ptr, str->length);
  str->str[str->length]= '\0'; // just for safety
  return eol+1;
}


/*
  read escaped string from ptr to eol in already allocated str

  SYNOPSIS
    parse_escaped_string()
    ptr		- pointer on string beginning
    eol		- pointer on character after end of string
    str		- target string

  RETURN
    FALSE - OK
    TRUE  - error
*/

my_bool
read_escaped_string(char *ptr, char *eol, LEX_STRING *str)
{
  char *write_pos= str->str;

  for(; ptr < eol; ptr++, write_pos++)
  {
    char c= *ptr;
    if (c == '\\')
    {
      ptr++;
      if (ptr >= eol)
	return TRUE;
      /*
	Should be in sync with write_escaped_string() and
	parse_quoted_escaped_string()
      */
      switch(*ptr) {
      case '\\':
	*write_pos= '\\';
	break;
      case 'n':
	*write_pos= '\n';
	break;
      case '0':
	*write_pos= '\0';
	break;
      case 'z':
	*write_pos= 26;
	break;
      case '\'':
	*write_pos= '\'';
        break;
      default:
	return TRUE;
      }
    }
    else
      *write_pos= c;
  }
  str->str[str->length= write_pos-str->str]= '\0'; // just for safety
  return FALSE;
}


/*
  parse \n delimited escaped string

  SYNOPSIS
    parse_escaped_string()
    ptr		- pointer on string beginning
    end		- pointer on symbol after parsed string end (still owned
		  by buffer and can be accessed
    mem_root	- MEM_ROOT for parameter allocation
    str		- pointer on string, where results should be stored

  RETURN
    0	- error
    #	- pointer on symbol after string
*/

static char *
parse_escaped_string(char *ptr, char *end, MEM_ROOT *mem_root, LEX_STRING *str)
{
  char *eol= strchr(ptr, '\n');

  if (eol == 0 || eol >= end ||
      !(str->str= alloc_root(mem_root, (eol - ptr) + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;
    
  return eol+1;
}


/*
  parse '' delimited escaped string

  SYNOPSIS
    parse_escaped_string()
    ptr		- pointer on string beginning
    end		- pointer on symbol after parsed string end (still owned
		  by buffer and can be accessed
    mem_root	- MEM_ROOT for parameter allocation
    str		- pointer on string, where results should be stored

  RETURN
    0	- error
    #	- pointer on symbol after string
*/

static char *
parse_quoted_escaped_string(char *ptr, char *end,
			    MEM_ROOT *mem_root, LEX_STRING *str)
{
  char *eol;
  uint result_len= 0;
  bool escaped= 0;

  // starting '
  if (*(ptr++) != '\'')
    return 0;

  // find ending '
  for (eol= ptr; (*eol != '\'' || escaped) && eol < end; eol++)
  {
    if (!(escaped= (*eol == '\\' && !escaped)))
      result_len++;
  }

  // process string
  if (eol >= end ||
      !(str->str= alloc_root(mem_root, result_len + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;

  return eol+1;
}


/*
  parse parameters
 
  SYNOPSIS
    File_parser::parse()
    base                base address for parameter writing (structure like
                        TABLE)
    mem_root            MEM_ROOT for parameters allocation
    parameters          parameters description
    required            number of required parameters in above list

  RETURN
    FALSE - OK
    TRUE - error
*/

my_bool
File_parser::parse(gptr base, MEM_ROOT *mem_root,
                   struct File_option *parameters, uint required)
{
  uint first_param= 0, found= 0;
  register char *ptr= start;
  char *eol;
  LEX_STRING *str;
  List<LEX_STRING> *list;
  DBUG_ENTER("File_parser::parse");

  while (ptr < end && found < required)
  {
    char *line= ptr;
    if (*ptr == '#')
    {
      // it is comment
      if (!(ptr= strchr(ptr, '\n')))
      {
	my_error(ER_FPARSER_EOF_IN_COMMENT, MYF(0), line);
	DBUG_RETURN(TRUE);
      }
      ptr++;
    }
    else
    {
      File_option *parameter= parameters+first_param,
	*parameters_end= parameters+required;
      int len= 0;
      for(; parameter < parameters_end; parameter++)
      {
	len= parameter->name.length;
	// check length
	if (len < (end-ptr) && ptr[len] != '=')
	  continue;
	// check keyword
	if (memcmp(parameter->name.str, ptr, len) == 0)
	  break;
      }

      if (parameter < parameters_end)
      {
	found++;
	/*
	  if we found first parameter, start search from next parameter
	  next time.
	  (this small optimisation should work, because they should be
	  written in same order)
	*/
	if (parameter == parameters+first_param)
	  first_param++;

	// get value
	ptr+= (len+1);
	switch (parameter->type) {
	case FILE_OPTIONS_STRING:
	{
	  if (!(ptr= parse_string(ptr, end, mem_root,
				  (LEX_STRING *)(base +
						 parameter->offset))))
	  {
	    my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                     parameter->name.str, line);
	    DBUG_RETURN(TRUE);
	  }
	  break;
	}
	case FILE_OPTIONS_ESTRING:
	{
	  if (!(ptr= parse_escaped_string(ptr, end, mem_root,
					  (LEX_STRING *)
					  (base + parameter->offset))))
	  {
	    my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                     parameter->name.str, line);
	    DBUG_RETURN(TRUE);
	  }
	  break;
	}
	case FILE_OPTIONS_ULONGLONG:
	case FILE_OPTIONS_REV:
	  if (!(eol= strchr(ptr, '\n')))
	  {
	    my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                     parameter->name.str, line);
	    DBUG_RETURN(TRUE);
	  }
          {
            int not_used;
	    *((ulonglong*)(base + parameter->offset))=
              my_strtoll10(ptr, 0, &not_used);
          }
	  ptr= eol+1;
	  break;
	case FILE_OPTIONS_TIMESTAMP:
	{
	  /* string have to be allocated already */
	  LEX_STRING *val= (LEX_STRING *)(base + parameter->offset);
	  /* yyyy-mm-dd HH:MM:SS = 19(PARSE_FILE_TIMESTAMPLENGTH) characters */
	  if (ptr[PARSE_FILE_TIMESTAMPLENGTH] != '\n')
	  {
	    my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                     parameter->name.str, line);
	    DBUG_RETURN(TRUE);
	  }
	  memcpy(val->str, ptr, PARSE_FILE_TIMESTAMPLENGTH);
	  val->str[val->length= PARSE_FILE_TIMESTAMPLENGTH]= '\0';
	  ptr+= (PARSE_FILE_TIMESTAMPLENGTH+1);
	  break;
	}
	case FILE_OPTIONS_STRLIST:
	{
          list= (List<LEX_STRING>*)(base + parameter->offset);
	    
	  list->empty();
	  // list parsing
	  while (ptr < end)
	  {
	    if (!(str= (LEX_STRING*)alloc_root(mem_root,
					       sizeof(LEX_STRING))) ||
		list->push_back(str, mem_root))
	      goto list_err;
	    if(!(ptr= parse_quoted_escaped_string(ptr, end, mem_root, str)))
	      goto list_err_w_message;
	    switch (*ptr) {
	    case '\n':
	      goto end_of_list;
	    case ' ':
	      // we cant go over buffer bounds, because we have \0 at the end
	      ptr++;
	      break;
	    default:
	      goto list_err_w_message;
	    }
	  }
      end_of_list:
	  if (*(ptr++) != '\n')
	    goto list_err;
	  break;

      list_err_w_message:
	  my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                   parameter->name.str, line);
      list_err:
	  DBUG_RETURN(TRUE);
	}
	default:
	  DBUG_ASSERT(0); // never should happened
	}
      }
      else
      {
	// skip unknown parameter
	if (!(ptr= strchr(ptr, '\n')))
	{
	  my_error(ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER, MYF(0), line);
	  DBUG_RETURN(TRUE);
	}
	ptr++;
      }
    }
  }
  DBUG_RETURN(FALSE);
}
