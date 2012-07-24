/* Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file

  @brief
  Text .frm files management routines
*/

#include "sql_priv.h"
#include "parse_file.h"
#include "unireg.h"                            // CREATE_MODE
#include "sql_table.h"                        // build_table_filename
#include <errno.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <my_dir.h>

/* from sql_db.cc */
extern long mysql_rm_arc_files(THD *thd, MY_DIR *dirp, const char *org_path);


/**
  Write string with escaping.

  @param file	  IO_CACHE for record
  @param val_s	  string for writing

  @retval
    FALSE   OK
  @retval
    TRUE    error
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
      if (my_b_append(file, (const uchar *)STRING_WITH_LEN("\\\\")))
	return TRUE;
      break;
    case '\n': // parameter value delimiter
      if (my_b_append(file, (const uchar *)STRING_WITH_LEN("\\n")))
	return TRUE;
      break;
    case '\0': // problem for some string processing utilities
      if (my_b_append(file, (const uchar *)STRING_WITH_LEN("\\0")))
	return TRUE;
      break;
    case 26: // problem for windows utilities (Ctrl-Z)
      if (my_b_append(file, (const uchar *)STRING_WITH_LEN("\\z")))
	return TRUE;
      break;
    case '\'': // list of string delimiter
      if (my_b_append(file, (const uchar *)STRING_WITH_LEN("\\\'")))
	return TRUE;
      break;
    default:
      if (my_b_append(file, (const uchar *)ptr, 1))
	return TRUE;
    }
  }
  return FALSE;
}


/**
  Write parameter value to IO_CACHE.

  @param file          pointer to IO_CACHE structure for writing
  @param base          pointer to data structure
  @param parameter     pointer to parameter descriptor

  @retval
    FALSE   OK
  @retval
    TRUE    error
*/


static my_bool
write_parameter(IO_CACHE *file, uchar* base, File_option *parameter)
{
  char num_buf[20];			// buffer for numeric operations
  // string for numeric operations
  String num(num_buf, sizeof(num_buf), &my_charset_bin);
  DBUG_ENTER("write_parameter");

  switch (parameter->type) {
  case FILE_OPTIONS_STRING:
  {
    LEX_STRING *val_s= (LEX_STRING *)(base + parameter->offset);
    if (my_b_append(file, (const uchar *)val_s->str, val_s->length))
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
    if (my_b_append(file, (const uchar *)num.ptr(), num.length()))
      DBUG_RETURN(TRUE);
    break;
  }
  case FILE_OPTIONS_TIMESTAMP:
  {
    /* string have to be allocated already */
    LEX_STRING *val_s= (LEX_STRING *)(base + parameter->offset);
    time_t tm= my_time(0);

    get_date(val_s->str, GETDATE_DATE_TIME|GETDATE_GMT|GETDATE_FIXEDLENGTH,
	     tm);
    val_s->length= PARSE_FILE_TIMESTAMPLENGTH;
    if (my_b_append(file, (const uchar *)val_s->str,
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
      if ((!first && my_b_append(file, (const uchar *)STRING_WITH_LEN(" "))) ||
	  my_b_append(file, (const uchar *)STRING_WITH_LEN("\'")) ||
          write_escaped_string(file, str) ||
	  my_b_append(file, (const uchar *)STRING_WITH_LEN("\'")))
      {
	DBUG_RETURN(TRUE);
      }
      first= 0;
    }
    break;
  }
  case FILE_OPTIONS_ULLLIST:
  {
    List_iterator_fast<ulonglong> it(*((List<ulonglong>*)
                                       (base + parameter->offset)));
    bool first= 1;
    ulonglong *val;
    while ((val= it++))
    {
      num.set(*val, &my_charset_bin);
      // We need ' ' after string to detect list continuation
      if ((!first && my_b_append(file, (const uchar *)STRING_WITH_LEN(" "))) ||
          my_b_append(file, (const uchar *)num.ptr(), num.length()))
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


/**
  Write new .frm.

  @param dir           directory where put .frm
  @param file_name     .frm file name
  @param type          .frm type string (VIEW, TABLE)
  @param base          base address for parameter reading (structure like
                       TABLE)
  @param parameters    parameters description

  @retval
    FALSE   OK
  @retval
    TRUE    error
*/


my_bool
sql_create_definition_file(const LEX_STRING *dir, const LEX_STRING *file_name,
			   const LEX_STRING *type,
			   uchar* base, File_option *parameters)
{
  File handler;
  IO_CACHE file;
  char path[FN_REFLEN+1];	// +1 to put temporary file name for sure
  int path_end;
  File_option *param;
  DBUG_ENTER("sql_create_definition_file");
  DBUG_PRINT("enter", ("Dir: %s, file: %s, base 0x%lx",
		       dir ? dir->str : "(null)",
                       file_name->str, (ulong) base));

  if (dir)
  {
    fn_format(path, file_name->str, dir->str, "", MY_UNPACK_FILENAME);
    path_end= strlen(path);
  }
  else
  {
    /*
      if not dir is passed, it means file_name is a full path,
      including dir name, file name itself, and an extension,
      and with unpack_filename() executed over it.
    */    
    path_end= strxnmov(path, sizeof(path) - 1, file_name->str, NullS) - path;
  }

  // temporary file name
  path[path_end]='~';
  path[path_end+1]= '\0';
  if ((handler= mysql_file_create(key_file_fileparser,
                                  path, CREATE_MODE, O_RDWR | O_TRUNC,
                                  MYF(MY_WME))) <= 0)
  {
    DBUG_RETURN(TRUE);
  }

  if (init_io_cache(&file, handler, 0, SEQ_READ_APPEND, 0L, 0, MYF(MY_WME)))
    goto err_w_file;

  // write header (file signature)
  if (my_b_append(&file, (const uchar *)STRING_WITH_LEN("TYPE=")) ||
      my_b_append(&file, (const uchar *)type->str, type->length) ||
      my_b_append(&file, (const uchar *)STRING_WITH_LEN("\n")))
    goto err_w_file;

  // write parameters to temporary file
  for (param= parameters; param->name.str; param++)
  {
    if (my_b_append(&file, (const uchar *)param->name.str,
                    param->name.length) ||
	my_b_append(&file, (const uchar *)STRING_WITH_LEN("=")) ||
	write_parameter(&file, base, param) ||
	my_b_append(&file, (const uchar *)STRING_WITH_LEN("\n")))
      goto err_w_cache;
  }

  if (end_io_cache(&file))
    goto err_w_file;

  if (opt_sync_frm) {
    if (mysql_file_sync(handler, MYF(MY_WME)))
      goto err_w_file;
  }

  if (mysql_file_close(handler, MYF(MY_WME)))
  {
    DBUG_RETURN(TRUE);
  }

  path[path_end]='\0';

  {
    // rename temporary file
    char path_to[FN_REFLEN];
    memcpy(path_to, path, path_end+1);
    path[path_end]='~';
    if (mysql_file_rename(key_file_fileparser, path, path_to, MYF(MY_WME)))
    {
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
err_w_cache:
  end_io_cache(&file);
err_w_file:
  mysql_file_close(handler, MYF(MY_WME));
  DBUG_RETURN(TRUE);
}

/**
  Renames a frm file (including backups) in same schema.

  @thd                     thread handler
  @param schema            name of given schema
  @param old_name          original file name
  @param new_db            new schema
  @param new_name          new file name

  @retval
    0   OK
  @retval
    1   Error (only if renaming of frm failed)
*/
my_bool rename_in_schema_file(THD *thd,
                              const char *schema, const char *old_name, 
                              const char *new_db, const char *new_name)
{
  char old_path[FN_REFLEN + 1], new_path[FN_REFLEN + 1], arc_path[FN_REFLEN + 1];

  build_table_filename(old_path, sizeof(old_path) - 1,
                       schema, old_name, reg_ext, 0);
  build_table_filename(new_path, sizeof(new_path) - 1,
                       new_db, new_name, reg_ext, 0);

  if (mysql_file_rename(key_file_frm, old_path, new_path, MYF(MY_WME)))
    return 1;

  /* check if arc_dir exists: disabled unused feature (see bug #17823). */
  build_table_filename(arc_path, sizeof(arc_path) - 1, schema, "arc", "", 0);
  
  { // remove obsolete 'arc' directory and files if any
    MY_DIR *new_dirp;
    if ((new_dirp = my_dir(arc_path, MYF(MY_DONT_SORT))))
    {
      DBUG_PRINT("my",("Archive subdir found: %s", arc_path));
      (void) mysql_rm_arc_files(thd, new_dirp, arc_path);
    }
  }
  return 0;
}

/**
  Prepare frm to parse (read to memory).

  @param file_name		  path & filename to .frm file
  @param mem_root		  MEM_ROOT for buffer allocation
  @param bad_format_errors	  send errors on bad content

  @note
    returned pointer + 1 will be type of .frm

  @return
    0 - error
  @return
    parser object
*/

File_parser * 
sql_parse_prepare(const LEX_STRING *file_name, MEM_ROOT *mem_root,
		  bool bad_format_errors)
{
  MY_STAT stat_info;
  size_t len;
  char *buff, *end, *sign;
  File_parser *parser;
  File file;
  DBUG_ENTER("sql_parse_prepare");

  if (!mysql_file_stat(key_file_fileparser,
                       file_name->str, &stat_info, MYF(MY_WME)))
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

  if (!(buff= (char*) alloc_root(mem_root, stat_info.st_size+1)))
  {
    DBUG_RETURN(0);
  }

  if ((file= mysql_file_open(key_file_fileparser, file_name->str,
                             O_RDONLY | O_SHARE, MYF(MY_WME))) < 0)
  {
    DBUG_RETURN(0);
  }
  
  if ((len= mysql_file_read(file, (uchar *)buff, stat_info.st_size,
                            MYF(MY_WME))) == MY_FILE_ERROR)
  {
    mysql_file_close(file, MYF(MY_WME));
    DBUG_RETURN(0);
  }

  if (mysql_file_close(file, MYF(MY_WME)))
  {
    DBUG_RETURN(0);
  }

  end= buff + len;
  *end= '\0'; // barrier for more simple parsing

  // 7 = 5 (TYPE=) + 1 (letter at least of type name) + 1 ('\n')
  if (len < 7 ||
      buff[0] != 'T' ||
      buff[1] != 'Y' ||
      buff[2] != 'P' ||
      buff[3] != 'E' ||
      buff[4] != '=')
    goto frm_error;

  // skip signature;
  parser->file_type.str= sign= buff + 5;
  while (*sign >= 'A' && *sign <= 'Z' && sign < end)
    sign++;
  if (*sign != '\n')
    goto frm_error;
  parser->file_type.length= sign - parser->file_type.str;
  // EOS for file signature just for safety
  *sign= '\0';

  parser->end= end;
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


/**
  parse LEX_STRING.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @retval
    0	  error
  @retval
    \#	  pointer on symbol after string
*/


static const char *
parse_string(const char *ptr, const char *end, MEM_ROOT *mem_root,
             LEX_STRING *str)
{
  // get string length
  const char *eol= strchr(ptr, '\n');

  if (eol >= end)
    return 0;

  str->length= eol - ptr;

  if (!(str->str= strmake_root(mem_root, ptr, str->length)))
    return 0;
  return eol+1;
}


/**
  read escaped string from ptr to eol in already allocated str.

  @param ptr		  pointer on string beginning
  @param eol		  pointer on character after end of string
  @param str		  target string

  @retval
    FALSE   OK
  @retval
    TRUE    error
*/

my_bool
read_escaped_string(const char *ptr, const char *eol, LEX_STRING *str)
{
  char *write_pos= str->str;

  for (; ptr < eol; ptr++, write_pos++)
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


/**
  parse \\n delimited escaped string.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @retval
    0	  error
  @retval
    \#	  pointer on symbol after string
*/


const char *
parse_escaped_string(const char *ptr, const char *end, MEM_ROOT *mem_root,
                     LEX_STRING *str)
{
  const char *eol= strchr(ptr, '\n');

  if (eol == 0 || eol >= end ||
      !(str->str= (char*) alloc_root(mem_root, (eol - ptr) + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;
    
  return eol+1;
}


/**
  parse '' delimited escaped string.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @retval
    0	  error
  @retval
    \#	  pointer on symbol after string
*/

static const char *
parse_quoted_escaped_string(const char *ptr, const char *end,
			    MEM_ROOT *mem_root, LEX_STRING *str)
{
  const char *eol;
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
      !(str->str= (char*) alloc_root(mem_root, result_len + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;

  return eol+1;
}


/**
  Parser for FILE_OPTIONS_ULLLIST type value.

  @param[in,out] ptr          pointer to parameter
  @param[in] end              end of the configuration
  @param[in] line             pointer to the line begining
  @param[in] base             base address for parameter writing (structure
    like TABLE)
  @param[in] parameter        description
  @param[in] mem_root         MEM_ROOT for parameters allocation
*/

bool get_file_options_ulllist(const char *&ptr, const char *end, const char *line,
                              uchar* base, File_option *parameter,
                              MEM_ROOT *mem_root)
{
  List<ulonglong> *nlist= (List<ulonglong>*)(base + parameter->offset);
  ulonglong *num;
  nlist->empty();
  // list parsing
  while (ptr < end)
  {
    int not_used;
    char *num_end= const_cast<char *>(end);
    if (!(num= (ulonglong*)alloc_root(mem_root, sizeof(ulonglong))) ||
        nlist->push_back(num, mem_root))
      goto nlist_err;
    *num= my_strtoll10(ptr, &num_end, &not_used);
    ptr= num_end;
    switch (*ptr) {
    case '\n':
      goto end_of_nlist;
    case ' ':
      // we cant go over buffer bounds, because we have \0 at the end
      ptr++;
      break;
    default:
      goto nlist_err_w_message;
    }
  }

end_of_nlist:
  if (*(ptr++) != '\n')
    goto nlist_err;
  return FALSE;

nlist_err_w_message:
  my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0), parameter->name.str, line);
nlist_err:
  return TRUE;
}


/**
  parse parameters.

  @param base                base address for parameter writing (structure like
                             TABLE)
  @param mem_root            MEM_ROOT for parameters allocation
  @param parameters          parameters description
  @param required            number of required parameters in above list. If the file
                             contains more parameters than "required", they will
                             be ignored. If the file contains less parameters
                             then "required", non-existing parameters will
                             remain their values.
  @param hook                hook called for unknown keys
  @param hook_data           some data specific for the hook

  @retval
    FALSE   OK
  @retval
    TRUE    error
*/


my_bool
File_parser::parse(uchar* base, MEM_ROOT *mem_root,
                   struct File_option *parameters, uint required,
                   Unknown_key_hook *hook) const
{
  uint first_param= 0, found= 0;
  const char *ptr= start;
  const char *eol;
  LEX_STRING *str;
  List<LEX_STRING> *list;
  DBUG_ENTER("File_parser::parse");

  while (ptr < end && found < required)
  {
    const char *line= ptr;
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
      for (; parameter < parameters_end; parameter++)
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
	    if (!(ptr= parse_quoted_escaped_string(ptr, end, mem_root, str)))
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
        case FILE_OPTIONS_ULLLIST:
          if (get_file_options_ulllist(ptr, end, line, base,
                                       parameter, mem_root))
            DBUG_RETURN(TRUE);
          break;
	default:
	  DBUG_ASSERT(0); // never should happened
	}
      }
      else
      {
        ptr= line;
        if (hook->process_unknown_string(ptr, base, mem_root, end))
        {
          DBUG_RETURN(TRUE);
        }
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

  /*
    NOTE: if we read less than "required" parameters, it is still Ok.
    Probably, we've just read the file of the previous version, which
    contains less parameters.
  */

  DBUG_RETURN(FALSE);
}


/**
  Dummy unknown key hook.

  @param[in,out] unknown_key       reference on the line with unknown
    parameter and the parsing point
  @param[in] base                  base address for parameter writing
    (structure like TABLE)
  @param[in] mem_root              MEM_ROOT for parameters allocation
  @param[in] end                   the end of the configuration

  @note
    This hook used to catch no longer supported keys and process them for
    backward compatibility, but it will not slow down processing of modern
    format files.
    This hook does nothing except debug output.

  @retval
    FALSE OK
  @retval
    TRUE  Error
*/

bool
File_parser_dummy_hook::process_unknown_string(const char *&unknown_key,
                                               uchar* base, MEM_ROOT *mem_root,
                                               const char *end)
{
  DBUG_ENTER("file_parser_dummy_hook::process_unknown_string");
  DBUG_PRINT("info", ("Unknown key: '%60s'", unknown_key));
  DBUG_RETURN(FALSE);
}
