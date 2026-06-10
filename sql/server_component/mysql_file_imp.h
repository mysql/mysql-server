/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_FILE_IMP_H
#define MYSQL_FILE_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_file.h>

/**
  Implementation of the "mysql_file" component service that uses server's file
  manipulation API.
*/
class mysql_component_mysql_file_imp {
 public:
  /**
    Wrapper around my_open function.

    @see my_open
  */
  static DEFINE_METHOD(FILE_h, open, (const char *file_name, int flags));

  /**
    Wrapper around my_create function.

    @see my_create
  */
  static DEFINE_METHOD(FILE_h, create,
                       (const char *file_name, int flags,
                        int permission_flags));

  /**
    Wrapper around my_close function.

    @see my_close
  */
  static DEFINE_METHOD(int, close, (FILE_h file));

  /**
    Wrapper around my_write function.

    @see my_write
  */
  static DEFINE_METHOD(size_t, write,
                       (FILE_h file, const unsigned char *data, size_t size));

  /**
    Wrapper around my_read function.

    @see my_read
  */
  static DEFINE_METHOD(size_t, read,
                       (FILE_h file, unsigned char *data, size_t size));

  /**
    Wrapper around my_flush function.

    @see my_flush
  */
  static DEFINE_METHOD(int, flush, (FILE_h file));

  /**
    Wrapper around my_seek function.

    @see my_seek
  */
  static DEFINE_METHOD(unsigned long long, seek,
                       (FILE_h file, unsigned long long pos, int whence));

  /**
    Wrapper around my_tell function.

    @see my_tell
  */
  static DEFINE_METHOD(unsigned long long, tell, (FILE_h file));
};

#endif /* MYSQL_CURRENT_THREAD_READER_IMP_H */
