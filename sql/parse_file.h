/* -*- C++ -*- */
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

#ifndef _PARSE_FILE_H_
#define _PARSE_FILE_H_

#define PARSE_FILE_TIMESTAMPLENGTH 19

enum file_opt_type {
  FILE_OPTIONS_STRING,		/* String (LEX_STRING) */
  FILE_OPTIONS_ESTRING,		/* Escaped string (LEX_STRING) */
  FILE_OPTIONS_ULONGLONG,	/* ulonglong parameter (ulonglong) */
  FILE_OPTIONS_REV,		/* Revision version number (ulonglong) */
  FILE_OPTIONS_TIMESTAMP,	/* timestamp (LEX_STRING have to be
				   allocated with length 20 (19+1) */
  FILE_OPTIONS_STRLIST          /* list of escaped strings
                                   (List<LEX_STRING>) */
};

struct File_option
{
  LEX_STRING name;		/* Name of the option */
  int offset;			/* offset to base address of value */
  file_opt_type type;		/* Option type */
};

class File_parser;
File_parser *sql_parse_prepare(const LEX_STRING *file_name,
			       MEM_ROOT *mem_root, bool bad_format_errors);

my_bool
sql_create_definition_file(const LEX_STRING *dir, const  LEX_STRING *file_name,
			   const LEX_STRING *type,
			   gptr base, File_option *parameters, uint versions);

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
  my_bool parse(gptr base, MEM_ROOT *mem_root,
		struct File_option *parameters, uint required);

  friend File_parser *sql_parse_prepare(const LEX_STRING *file_name,
					MEM_ROOT *mem_root,
					bool bad_format_errors);
};

#endif /* _PARSE_FILE_H_ */
