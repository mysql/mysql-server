/* Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/parse_file.h"

#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"  // ER_*
#include "sql/mysqld.h"    // key_file_fileparser
#include "sql/sql_list.h"  // List_iterator_fast

// Dummy unknown key hook.
File_parser_dummy_hook file_parser_dummy_hook;

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

File_parser *sql_parse_prepare(const LEX_STRING *file_name, MEM_ROOT *mem_root,
                               bool bad_format_errors) {
  MY_STAT stat_info;
  size_t len;
  char *buff, *end, *sign;
  File_parser *parser;
  File file;
  DBUG_ENTER("sql_parse_prepare");

  if (!mysql_file_stat(key_file_fileparser, file_name->str, &stat_info,
                       MYF(MY_WME))) {
    DBUG_RETURN(0);
  }

  if (stat_info.st_size > INT_MAX - 1) {
    my_error(ER_FPARSER_TOO_BIG_FILE, MYF(0), file_name->str);
    DBUG_RETURN(0);
  }

  if (!(parser = new (mem_root) File_parser)) {
    DBUG_RETURN(0);
  }

  if (!(buff = (char *)alloc_root(
            mem_root, static_cast<size_t>(stat_info.st_size) + 1))) {
    DBUG_RETURN(0);
  }

  if ((file = mysql_file_open(key_file_fileparser, file_name->str, O_RDONLY,
                              MYF(MY_WME))) < 0) {
    DBUG_RETURN(0);
  }

  if ((len = mysql_file_read(file, (uchar *)buff,
                             static_cast<size_t>(stat_info.st_size),
                             MYF(MY_WME))) == MY_FILE_ERROR) {
    mysql_file_close(file, MYF(MY_WME));
    DBUG_RETURN(0);
  }

  if (mysql_file_close(file, MYF(MY_WME))) {
    DBUG_RETURN(0);
  }

  end = buff + len;
  *end = '\0';  // barrier for more simple parsing

  // 7 = 5 (TYPE=) + 1 (letter at least of type name) + 1 ('\n')
  if (len < 7 || buff[0] != 'T' || buff[1] != 'Y' || buff[2] != 'P' ||
      buff[3] != 'E' || buff[4] != '=')
    goto frm_error;

  // skip signature;
  parser->file_type.str = sign = buff + 5;
  while (*sign >= 'A' && *sign <= 'Z' && sign < end) sign++;
  if (*sign != '\n') goto frm_error;
  parser->file_type.length = sign - parser->file_type.str;
  // EOS for file signature just for safety
  *sign = '\0';

  parser->end = end;
  parser->start = sign + 1;
  parser->content_ok = 1;

  DBUG_RETURN(parser);

frm_error:
  if (bad_format_errors) {
    my_error(ER_FPARSER_BAD_HEADER, MYF(0), file_name->str);
    DBUG_RETURN(0);
  } else
    DBUG_RETURN(parser);  // upper level have to check parser->ok()
}

/**
  parse LEX_STRING.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @return Pointer on symbol after string
  @retval 0	  error
*/

static const char *parse_string(const char *ptr, const char *end,
                                MEM_ROOT *mem_root, LEX_STRING *str) {
  // get string length
  const char *eol = strchr(ptr, '\n');

  if (eol >= end) return 0;

  str->length = eol - ptr;

  if (!(str->str = strmake_root(mem_root, ptr, str->length))) return 0;
  return eol + 1;
}

/**
  read escaped string from ptr to eol in already allocated str.

  @param ptr		  pointer on string beginning
  @param eol		  pointer on character after end of string
  @param str		  target string

  @retval
    false   OK
  @retval
    true    error
*/

static bool read_escaped_string(const char *ptr, const char *eol,
                                LEX_STRING *str) {
  char *write_pos = str->str;

  for (; ptr < eol; ptr++, write_pos++) {
    char c = *ptr;
    if (c == '\\') {
      ptr++;
      if (ptr >= eol) return true;
      /*
        Should be in sync with write_escaped_string() and
        parse_quoted_escaped_string()
      */
      switch (*ptr) {
        case '\\':
          *write_pos = '\\';
          break;
        case 'n':
          *write_pos = '\n';
          break;
        case '0':
          *write_pos = '\0';
          break;
        case 'z':
          *write_pos = 26;
          break;
        case '\'':
          *write_pos = '\'';
          break;
        default:
          return true;
      }
    } else
      *write_pos = c;
  }
  str->str[str->length = write_pos - str->str] = '\0';  // just for safety
  return false;
}

/**
  parse @\n delimited escaped string.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @return Pointer on symbol after string
  @retval
    0	  error
*/

static const char *parse_escaped_string(const char *ptr, const char *end,
                                        MEM_ROOT *mem_root, LEX_STRING *str) {
  const char *eol = strchr(ptr, '\n');

  if (eol == 0 || eol >= end ||
      !(str->str = (char *)alloc_root(mem_root, (eol - ptr) + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;

  return eol + 1;
}

/**
  parse '' delimited escaped string.

  @param ptr		  pointer on string beginning
  @param end		  pointer on symbol after parsed string end (still owned
                         by buffer and can be accessed
  @param mem_root	  MEM_ROOT for parameter allocation
  @param str		  pointer on string, where results should be stored

  @return Pointer on symbol after string
  @retval
    0	  error
*/

static const char *parse_quoted_escaped_string(const char *ptr, const char *end,
                                               MEM_ROOT *mem_root,
                                               LEX_STRING *str) {
  const char *eol;
  uint result_len = 0;
  bool escaped = 0;

  // starting '
  if (*(ptr++) != '\'') return 0;

  // find ending '
  for (eol = ptr; (*eol != '\'' || escaped) && eol < end; eol++) {
    if (!(escaped = (*eol == '\\' && !escaped))) result_len++;
  }

  // process string
  if (eol >= end ||
      !(str->str = (char *)alloc_root(mem_root, result_len + 1)) ||
      read_escaped_string(ptr, eol, str))
    return 0;

  return eol + 1;
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

bool get_file_options_ulllist(const char *&ptr, const char *end,
                              const char *line, uchar *base,
                              File_option *parameter, MEM_ROOT *mem_root) {
  List<ulonglong> *nlist = (List<ulonglong> *)(base + parameter->offset);
  ulonglong *num;
  nlist->empty();
  // list parsing
  while (ptr < end) {
    int not_used;
    char *num_end = const_cast<char *>(end);
    if (!(num = (ulonglong *)alloc_root(mem_root, sizeof(ulonglong))) ||
        nlist->push_back(num, mem_root))
      goto nlist_err;
    *num = my_strtoll10(ptr, &num_end, &not_used);
    ptr = num_end;
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
  if (*(ptr++) != '\n') goto nlist_err;
  return false;

nlist_err_w_message:
  my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0), parameter->name.str, line);
nlist_err:
  return true;
}

/**
  parse parameters.

  @param base                base address for parameter writing (structure like
                             TABLE)
  @param mem_root            MEM_ROOT for parameters allocation
  @param parameters          parameters description
  @param required            number of required parameters in above list. If the
  file contains more parameters than "required", they will be ignored. If the
  file contains less parameters then "required", non-existing parameters will
                             remain their values.
  @param hook                hook called for unknown keys

  @retval
    false   OK
  @retval
    true    error
*/

bool File_parser::parse(uchar *base, MEM_ROOT *mem_root,
                        struct File_option *parameters, uint required,
                        Unknown_key_hook *hook) const {
  uint first_param = 0, found = 0;
  const char *ptr = start;
  const char *eol;
  LEX_STRING *str;
  List<LEX_STRING> *list;
  DBUG_ENTER("File_parser::parse");

  while (ptr < end && found < required) {
    const char *line = ptr;
    if (*ptr == '#') {
      // it is comment
      if (!(ptr = strchr(ptr, '\n'))) {
        my_error(ER_FPARSER_EOF_IN_COMMENT, MYF(0), line);
        DBUG_RETURN(true);
      }
      ptr++;
    } else {
      File_option *parameter = parameters + first_param,
                  *parameters_end = parameters + required;
      size_t len = 0;
      for (; parameter < parameters_end; parameter++) {
        len = parameter->name.length;
        // check length
        if (len < static_cast<size_t>(end - ptr) && ptr[len] != '=') continue;
        // check keyword
        if (memcmp(parameter->name.str, ptr, len) == 0) break;
      }

      if (parameter < parameters_end) {
        found++;
        /*
          if we found first parameter, start search from next parameter
          next time.
          (this small optimisation should work, because they should be
          written in same order)
        */
        if (parameter == parameters + first_param) first_param++;

        // get value
        ptr += (len + 1);
        switch (parameter->type) {
          case FILE_OPTIONS_STRING: {
            if (!(ptr =
                      parse_string(ptr, end, mem_root,
                                   (LEX_STRING *)(base + parameter->offset)))) {
              my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                       parameter->name.str, line);
              DBUG_RETURN(true);
            }
            break;
          }
          case FILE_OPTIONS_ESTRING: {
            if (!(ptr = parse_escaped_string(
                      ptr, end, mem_root,
                      (LEX_STRING *)(base + parameter->offset)))) {
              my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                       parameter->name.str, line);
              DBUG_RETURN(true);
            }
            break;
          }
          case FILE_OPTIONS_ULONGLONG:
            if (!(eol = strchr(ptr, '\n'))) {
              my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                       parameter->name.str, line);
              DBUG_RETURN(true);
            }
            {
              int not_used;
              *((ulonglong *)(base + parameter->offset)) =
                  my_strtoll10(ptr, 0, &not_used);
            }
            ptr = eol + 1;
            break;
          case FILE_OPTIONS_TIMESTAMP: {
            /* string have to be allocated already */
            LEX_STRING *val = (LEX_STRING *)(base + parameter->offset);
            /* yyyy-mm-dd HH:MM:SS = 19(PARSE_FILE_TIMESTAMPLENGTH) characters
             */
            if (ptr[PARSE_FILE_TIMESTAMPLENGTH] != '\n') {
              my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0),
                       parameter->name.str, line);
              DBUG_RETURN(true);
            }
            memcpy(val->str, ptr, PARSE_FILE_TIMESTAMPLENGTH);
            val->str[val->length = PARSE_FILE_TIMESTAMPLENGTH] = '\0';
            ptr += (PARSE_FILE_TIMESTAMPLENGTH + 1);
            break;
          }
          case FILE_OPTIONS_STRLIST: {
            list = (List<LEX_STRING> *)(base + parameter->offset);

            list->empty();
            // list parsing
            while (ptr < end) {
              if (!(str = (LEX_STRING *)alloc_root(mem_root,
                                                   sizeof(LEX_STRING))) ||
                  list->push_back(str, mem_root))
                goto list_err;
              if (!(ptr = parse_quoted_escaped_string(ptr, end, mem_root, str)))
                goto list_err_w_message;
              switch (*ptr) {
                case '\n':
                  goto end_of_list;
                case ' ':
                  // we cant go over buffer bounds, because we have \0 at the
                  // end
                  ptr++;
                  break;
                default:
                  goto list_err_w_message;
              }
            }

          end_of_list:
            if (*(ptr++) != '\n') goto list_err;
            break;

          list_err_w_message:
            my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0), parameter->name.str,
                     line);
          list_err:
            DBUG_RETURN(true);
          }
          case FILE_OPTIONS_ULLLIST:
            if (get_file_options_ulllist(ptr, end, line, base, parameter,
                                         mem_root))
              DBUG_RETURN(true);
            break;
          default:
            DBUG_ASSERT(0);  // never should happened
        }
      } else {
        ptr = line;
        if (hook->process_unknown_string(ptr, base, mem_root, end)) {
          DBUG_RETURN(true);
        }
        // skip unknown parameter
        if (!(ptr = strchr(ptr, '\n'))) {
          my_error(ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER, MYF(0), line);
          DBUG_RETURN(true);
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

  DBUG_RETURN(false);
}

/**
  Dummy unknown key hook.

  @param[in,out] unknown_key       reference on the line with unknown
                                   parameter and the parsing point

  @note
    This hook used to catch no longer supported keys and process them for
    backward compatibility, but it will not slow down processing of modern
    format files.
    This hook does nothing except debug output.

  @retval
    false OK
  @retval
    true  Error
*/

bool File_parser_dummy_hook::process_unknown_string(
    const char *&unknown_key MY_ATTRIBUTE((unused)), uchar *, MEM_ROOT *,
    const char *) {
  DBUG_ENTER("file_parser_dummy_hook::process_unknown_string");
  DBUG_PRINT("info", ("Unknown key: '%60s'", unknown_key));
  DBUG_RETURN(false);
}
