/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_FILE_IO_H
#define MYSQL_FILE_IO_H

#include "my_inttypes.h"
#include "my_io.h"
#include "plugin/keyring/common/logger.h"

namespace keyring
{
  class File_io
  {
  public:
    File_io(ILogger *logger) : logger(logger)
    {}

    File open(PSI_file_key file_data_key, const char *filename, int flags,
              myf myFlags);
    int close(File file, myf myFlags);
    size_t read(File file, uchar *buffer, size_t count, myf myFlags);
    size_t write(File file, const uchar *buffer, size_t count, myf myFlags);
    my_off_t seek(File file, my_off_t pos, int whence, myf flags);
    my_off_t tell(File file, myf flags);
    int fstat(File file, MY_STAT *stat_area, myf myFlags);
    int sync(File file, myf myFlags);
    bool truncate(File file, myf myFlags);
    bool remove(const char *filename, myf myFlags);
  protected:
    ILogger *logger;

    void my_warning(int nr, ...);
  };
} //namespace keyring
#endif //MYSQL_FILE_IO_H
