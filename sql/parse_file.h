/* -*- C++ -*- */
/* Copyright (C) 2004 MySQL AB

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

#ifndef _PARSE_FILE_H_
#define _PARSE_FILE_H_

#define PARSE_FILE_TIMESTAMPLENGTH 19

enum file_opt_type {
  FILE_OPTIONS_STRING,		/**< String (LEX_STRING) */
  FILE_OPTIONS_ESTRING,		/**< Escaped string (LEX_STRING) */
  FILE_OPTIONS_ULONGLONG,	/**< ulonglong parameter (ulonglong) */
  FILE_OPTIONS_TIMESTAMP,	/**< timestamp (LEX_STRING have to be
				   allocated with length 20 (19+1) */
  FILE_OPTIONS_STRLIST,         /**< list of escaped strings
                                   (List<LEX_STRING>) */
  FILE_OPTIONS_ULLLIST          /**< list of ulonglong values
                                   (List<ulonglong>) */
};

struct File_option
{
  LEX_STRING name;		/**< Name of the option */
  int offset;			/**< offset to base address of value */
  file_opt_type type;		/**< Option type */
};


/**
  This hook used to catch no longer supported keys and process them for
  backward compatibility.
*/

class Unknown_key_hook
{
public:
  Unknown_key_hook() {}                       /* Remove gcc warning */
  virtual ~Unknown_key_hook() {}              /* Remove gcc warning */
  virtual bool process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end)= 0;
};


/** Dummy hook for parsers which do not need hook for unknown keys. */

class File_parser_dummy_hook: public Unknown_key_hook
{
public:
  File_parser_dummy_hook() {}                 /* Remove gcc warning */
  virtual bool process_unknown_string(char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, char *end);
};

extern File_parser_dummy_hook file_parser_dummy_hook;

bool get_file_options_ulllist(char *&ptr, char *end, char *line,
                              uchar* base, File_option *parameter,
                              MEM_ROOT *mem_root);

char *
parse_escaped_string(char *ptr, char *end, MEM_ROOT *mem_root, LEX_STRING *str);

class File_parser;
File_parser *sql_parse_prepare(const LEX_STRING *file_name,
			       MEM_ROOT *mem_root, bool bad_format_errors);

my_bool
sql_create_definition_file(const LEX_STRING *dir, const  LEX_STRING *file_name,
			   const LEX_STRING *type,
			   uchar* base, File_option *parameters);
my_bool rename_in_schema_file(THD *thd,
                              const char *schema, const char *old_name,
                              const char *new_db, const char *new_name);

class File_parser: public Sql_alloc
{
  char *buff, *start, *end;
  LEX_STRING file_type;
  my_bool content_ok;
public:
  File_parser() :buff(0), start(0), end(0), content_ok(0)
    { file_type.str= 0; file_type.length= 0; }

  my_bool ok() { return content_ok; }
  LEX_STRING *type() { return &file_type; }
  my_bool parse(uchar* base, MEM_ROOT *mem_root,
		struct File_option *parameters, uint required,
                Unknown_key_hook *hook);

  friend File_parser *sql_parse_prepare(const LEX_STRING *file_name,
					MEM_ROOT *mem_root,
					bool bad_format_errors);
};
#endif /* _PARSE_FILE_H_ */
