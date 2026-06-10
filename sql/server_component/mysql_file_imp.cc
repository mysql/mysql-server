/* Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_file_imp.h"

#include <fcntl.h>
#include <mysql/components/service_implementation.h>
#include "my_io.h"
#include "my_sys.h"

/**
  Values defined as 0.
*/
static_assert(!MY_FILE_O_RDONLY);
static_assert(!MY_FILE_SEEK_SET);

/**
  Translate a flag from service defined to platform specific.
*/
static int translate_open_flags(int flags) {
  int f = 0;

  if (MY_FILE_O_WRONLY == (flags & MY_FILE_O_WRONLY)) f |= O_WRONLY;
  if (MY_FILE_O_RDWR == (flags & MY_FILE_O_RDWR)) f |= O_RDWR;
  if (MY_FILE_O_CREAT == (flags & MY_FILE_O_CREAT)) f |= O_CREAT;
  if (MY_FILE_O_EXCL == (flags & MY_FILE_O_EXCL)) f |= O_EXCL;
  if (MY_FILE_O_TRUNC == (flags & MY_FILE_O_TRUNC)) f |= O_TRUNC;
  if (MY_FILE_O_APPEND == (flags & MY_FILE_O_APPEND)) f |= O_APPEND;
  if (MY_FILE_O_NONBLOCK == (flags & MY_FILE_O_NONBLOCK)) f |= O_NONBLOCK;
  if (MY_FILE_O_NOFOLLOW == (flags & MY_FILE_O_NOFOLLOW)) f |= O_NOFOLLOW;

#ifdef _WIN32
  if (MY_FILE_O_TEXT == (flags & MY_FILE_O_TEXT)) f |= O_TEXT;
  if (MY_FILE_O_BINARY == (flags & MY_FILE_O_BINARY)) f |= O_BINARY;
  if (MY_FILE_O_RAW == (flags & MY_FILE_O_RAW)) f |= O_RAW;
  if (MY_FILE_O_TEMPORARY == (flags & MY_FILE_O_TEMPORARY)) f |= O_TEMPORARY;
  if (MY_FILE_O_NOINHERIT == (flags & MY_FILE_O_NOINHERIT)) f |= O_NOINHERIT;
  if (MY_FILE_O_SEQUENTIAL == (flags & MY_FILE_O_SEQUENTIAL)) f |= O_SEQUENTIAL;
  if (MY_FILE_O_RANDOM == (flags & MY_FILE_O_RANDOM)) f |= O_RANDOM;
#else
  if (MY_FILE_O_ACCMODE == (flags & MY_FILE_O_ACCMODE)) f |= O_ACCMODE;
  if (MY_FILE_O_NOCTTY == (flags & MY_FILE_O_NOCTTY)) f |= O_NOCTTY;
  if (MY_FILE_O_SYNC == (flags & MY_FILE_O_SYNC)) f |= O_SYNC;
/* Missing on Solaris. */
#ifdef FASYNC
  if (MY_FILE_FASYNC == (flags & MY_FILE_FASYNC)) f |= FASYNC;
#endif
/* Missing on Solaris. */
#ifdef O_DIRECT
  if (MY_FILE_O_DIRECT == (flags & MY_FILE_O_DIRECT)) f |= O_DIRECT;
#endif
/* Missing on macOS. */
#ifdef O_LARGEFILE
  if (MY_FILE_O_LARGEFILE == (flags & MY_FILE_O_LARGEFILE)) f |= O_LARGEFILE;
#endif
  if (MY_FILE_O_DIRECTORY == (flags & MY_FILE_O_DIRECTORY)) f |= O_DIRECTORY;
/* Missing on Solaris. */
#ifdef O_NOATIME
  if (MY_FILE_O_NOATIME == (flags & MY_FILE_O_NOATIME)) f |= O_NOATIME;
#endif
  if (MY_FILE_O_CLOEXEC == (flags & MY_FILE_O_CLOEXEC)) f |= O_CLOEXEC;
#endif

  return f;
}

static int translate_permission_flags(int flags) {
  int f = 0;

#if defined _WIN32
  if (flags & (MY_FILE_PERMISSION_USER_READ | MY_FILE_PERMISSION_GROUP_READ |
               MY_FILE_PERMISSION_OTHERS_READ))
    f |= _S_IREAD;
  if (flags & (MY_FILE_PERMISSION_USER_WRITE | MY_FILE_PERMISSION_GROUP_WRITE |
               MY_FILE_PERMISSION_OTHERS_WRITE))
    f |= _S_IWRITE;
#else
  if (MY_FILE_PERMISSION_USER_READ == (flags & MY_FILE_PERMISSION_USER_READ))
    f |= S_IRUSR;
  if (MY_FILE_PERMISSION_USER_WRITE == (flags & MY_FILE_PERMISSION_USER_WRITE))
    f |= S_IWUSR;
  if (MY_FILE_PERMISSION_USER_EXECUTE ==
      (flags & MY_FILE_PERMISSION_USER_EXECUTE))
    f |= S_IXUSR;
  if (MY_FILE_PERMISSION_GROUP_READ == (flags & MY_FILE_PERMISSION_GROUP_READ))
    f |= S_IRGRP;
  if (MY_FILE_PERMISSION_GROUP_WRITE ==
      (flags & MY_FILE_PERMISSION_GROUP_WRITE))
    f |= S_IWGRP;
  if (MY_FILE_PERMISSION_GROUP_EXECUTE ==
      (flags & MY_FILE_PERMISSION_GROUP_EXECUTE))
    f |= S_IXGRP;
  if (MY_FILE_PERMISSION_OTHERS_READ ==
      (flags & MY_FILE_PERMISSION_OTHERS_READ))
    f |= S_IROTH;
  if (MY_FILE_PERMISSION_OTHERS_WRITE ==
      (flags & MY_FILE_PERMISSION_OTHERS_WRITE))
    f |= S_IWOTH;
#endif

  return f;
}

static int translate_seek_flags(int flags) {
  int f = 0;

  if (MY_FILE_SEEK_CUR == (flags & MY_FILE_SEEK_CUR)) f |= MY_SEEK_CUR;
  if (MY_FILE_SEEK_END == (flags & MY_FILE_SEEK_END)) f |= MY_SEEK_END;

  return f;
}

DEFINE_METHOD(FILE_h, mysql_component_mysql_file_imp::open,
              (const char *file_name, int open_flags)) {
  File *file = new (std::nothrow) File;

  if (file == nullptr) return nullptr;

  int f = translate_open_flags(open_flags);

  *file = my_open(file_name, f, MYF(0));

  if (*file < 0) {
    delete file;
    file = nullptr;
  }

  return reinterpret_cast<FILE_h>(file);
}

DEFINE_METHOD(FILE_h, mysql_component_mysql_file_imp::create,
              (const char *file_name, int open_flags, int permission_flags)) {
  File *file = new (std::nothrow) File;

  if (file == nullptr) return nullptr;

  int of = translate_open_flags(open_flags);
  int pf = translate_permission_flags(permission_flags);

  *file = my_create(file_name, pf, of, MYF(0));

  if (*file < 0) {
    delete file;
    file = nullptr;
  }

  return reinterpret_cast<FILE_h>(file);
}

DEFINE_METHOD(int, mysql_component_mysql_file_imp::close, (FILE_h file)) {
  File *f = reinterpret_cast<File *>(file);

  int r = my_close(*f, MYF(0));

  delete f;

  return r;
}

DEFINE_METHOD(size_t, mysql_component_mysql_file_imp::write,
              (FILE_h file, const unsigned char *data, size_t size)) {
  size_t res = my_write(*reinterpret_cast<File *>(file), data, size, MYF(0));
  return res == MY_FILE_ERROR ? MY_FILE_ERROR_IO : res;
}

DEFINE_METHOD(size_t, mysql_component_mysql_file_imp::read,
              (FILE_h file, unsigned char *data, size_t size)) {
  size_t res = my_read(*reinterpret_cast<File *>(file), data, size, MYF(0));
  return res == MY_FILE_ERROR ? MY_FILE_ERROR_IO : res;
}

DEFINE_METHOD(int, mysql_component_mysql_file_imp::flush, (FILE_h file)) {
  return my_sync(*reinterpret_cast<File *>(file), MYF(0));
}

DEFINE_METHOD(unsigned long long, mysql_component_mysql_file_imp::seek,
              (FILE_h file, unsigned long long pos, int whence)) {
  int f = translate_seek_flags(whence);

  my_off_t res = my_seek(*reinterpret_cast<File *>(file), pos, f, MYF(0));
  return res == MY_FILEPOS_ERROR ? MY_FILE_ERROR_POS
                                 : static_cast<unsigned long long>(res);
}

DEFINE_METHOD(unsigned long long, mysql_component_mysql_file_imp::tell,
              (FILE_h file)) {
  my_off_t res = my_tell(*reinterpret_cast<File *>(file), MYF(0));
  return res == MY_FILEPOS_ERROR ? MY_FILE_ERROR_POS
                                 : static_cast<unsigned long long>(res);
}
