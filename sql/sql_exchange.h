/* Copyright (c) 2017, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_EXCHANGE_INCLUDED
#define SQL_EXCHANGE_INCLUDED

#include "lex_string.h"
#include "sql_string.h"

struct Parse_context;

enum enum_source_type { LOAD_SOURCE_FILE, LOAD_SOURCE_URL, LOAD_SOURCE_S3 };

enum enum_filetype {
  FILETYPE_CSV,
  FILETYPE_XML,
  FILETYPE_TEXT,
  FILETYPE_PARQUET,
  FILETYPE_JSON
};

enum enum_destination {
  UNDEFINED_DEST,
  OBJECT_STORE_DEST,
  DUMPFILE_DEST,
  OUTFILE_DEST
};

enum class enum_with_header {
  WITHOUT_HEADER = 0,
  WITH_HEADER = 1,
  DEFAULT_HEADER = 2
};

enum class enum_trim_spaces {
  WITHOUT_TRIM_SPACES = 0,
  WITH_TRIM_SPACES = 1,
  DEFAULT_TRIM_SPACES = 2
};

/**
  Helper for the sql_exchange class
*/

class Line_separators {
 public:
  const String *line_term{nullptr};
  const String *line_start{nullptr};

  void merge_line_separators(const Line_separators *line_sep) {
    if (line_sep == nullptr) return;
    if (line_sep->line_term != nullptr) line_term = line_sep->line_term;
    if (line_sep->line_start != nullptr) line_start = line_sep->line_start;
  }

  void assign_default_values(enum_destination dumpfile,
                             enum_filetype filetype_arg);
};

/**
  Helper for the sql_exchange class
*/

class Field_separators {
 public:
  const String *field_term{nullptr};
  const String *escaped{nullptr};
  const String *enclosed{nullptr};
  bool opt_enclosed{false};
  bool not_enclosed{false};
  const String *date_format{nullptr};
  const String *time_format{nullptr};
  const String *datetime_format{nullptr};
  enum_trim_spaces trim_spaces{enum_trim_spaces::DEFAULT_TRIM_SPACES};
  const String *null_value{nullptr};
  const String *empty_value{nullptr};

  void merge_field_separators(const Field_separators *field_sep) {
    if (field_sep == nullptr) {
      return;
    }
    if (field_sep->field_term != nullptr) field_term = field_sep->field_term;
    if (field_sep->escaped != nullptr) escaped = field_sep->escaped;
    if (field_sep->enclosed != nullptr) enclosed = field_sep->enclosed;
    // TODO: a bug?
    // OPTIONALLY ENCLOSED BY x ENCLOSED BY y == OPTIONALLY ENCLOSED BY y
    if (field_sep->opt_enclosed) opt_enclosed = field_sep->opt_enclosed;
    if (field_sep->not_enclosed) not_enclosed = field_sep->not_enclosed;
    if (field_sep->date_format != nullptr) {
      date_format = field_sep->date_format;
    }
    if (field_sep->time_format != nullptr) {
      time_format = field_sep->time_format;
    }
    if (field_sep->datetime_format != nullptr) {
      datetime_format = field_sep->datetime_format;
    }
    if (field_sep->trim_spaces != enum_trim_spaces::DEFAULT_TRIM_SPACES) {
      trim_spaces = field_sep->trim_spaces;
    }

    if (field_sep->null_value != nullptr) {
      null_value = field_sep->null_value;
    }
    if (field_sep->empty_value != nullptr) {
      empty_value = field_sep->empty_value;
    }
  }

  void assign_default_values(enum_filetype filetype_arg);
};

class URI_information {
 public:
  const String *uri{nullptr};

  void merge_uri_info_separators(URI_information *uri_info) {
    if (uri_info == nullptr) {
      return;
    }
    if (uri_info->uri != nullptr) uri = uri_info->uri;
  }
};

/**
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class File_information {
 public:
  const char *filetype_str{nullptr};
  /// @cond Doxygen_is_confused
  enum enum_filetype filetype { FILETYPE_TEXT };
  /// @endcond
  const String *compression{nullptr};
  enum_with_header with_header{enum_with_header::DEFAULT_HEADER};
  const CHARSET_INFO *cs{nullptr};

  File_information() {}

  explicit File_information(enum_destination dumpfile_flag) {
    if (dumpfile_flag == OBJECT_STORE_DEST) {
      filetype = FILETYPE_CSV;
    } else {
      filetype = FILETYPE_TEXT;
    }
  }

  explicit File_information(enum_filetype filetype_arg)
      : filetype(filetype_arg) {}

  void merge_file_information(const File_information *file_info) {
    if (file_info == nullptr) {
      return;
    }
    if (file_info->filetype_str != nullptr) {
      filetype_str = file_info->filetype_str;
    }
    if (file_info->compression != nullptr) {
      compression = file_info->compression;
    }
    if (file_info->with_header != enum_with_header::DEFAULT_HEADER) {
      with_header = file_info->with_header;
    }
    if (file_info->cs != nullptr) {
      cs = file_info->cs;
    }
  }

  void assign_default_values();
  bool do_contextualize();
};

class sql_exchange final {
 public:
  Field_separators field;
  Line_separators line;
  URI_information uri_info;
  File_information file_info;
  /* load XML, Added by Arnold & Erik */
  const char *file_name{nullptr};
  enum enum_destination dumpfile;
  unsigned long skip_lines{0};

  LEX_CSTRING outfile_json = NULL_CSTR;
  sql_exchange(const char *name, enum_destination dumpfile_flag,
               enum_filetype filetype);
  sql_exchange(const char *name, enum_destination dumpfile_flag);
  explicit sql_exchange(enum_destination dumpfile_flag);
  bool escaped_given(void);
  void assign_default_values();
  bool do_contextualize(Parse_context *pc);
};

#endif
