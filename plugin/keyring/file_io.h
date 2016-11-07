/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_FILE_IO_H
#define MYSQL_FILE_IO_H

#include "logger.h"

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
    my_bool truncate(File file, myf myFlags);
    my_bool remove(const char *filename, myf myFlags);
  protected:
    ILogger *logger;

    void my_warning(int nr, ...);
  };
} //namespace keyring
#endif //MYSQL_FILE_IO_H
