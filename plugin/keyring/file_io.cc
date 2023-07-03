/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "plugin/keyring/file_io.h"

#include "my_config.h"

#include <assert.h>
#include <errno.h>
#include <mysql/psi/mysql_file.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sstream>
#include <utility>

#include "mysqld_error.h"
#include "mysys_err.h"
#include "sql/current_thd.h"
#include "sql/mysqld.h"
#include "sql/sql_error.h"

namespace keyring {

bool is_super_user() {
  THD *thd = current_thd;
  MYSQL_SECURITY_CONTEXT sec_ctx;
  my_svc_bool has_super_privilege = false;

  assert(thd != nullptr);

  if (thd == nullptr || thd_get_security_context(thd, &sec_ctx) ||
      security_context_get_option(sec_ctx, "privilege_super",
                                  &has_super_privilege))
    return false;

  return has_super_privilege;
}

File File_io::open(PSI_file_key file_data_key [[maybe_unused]],
                   const char *filename, int flags, myf myFlags) {
  File file = mysql_file_open(file_data_key, filename, flags, MYF(0));
  if (file < 0 && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    uint error_message_number = EE_FILENOTFOUND;
    if (my_errno() == EMFILE) error_message_number = EE_OUT_OF_FILERESOURCES;
    my_warning(error_message_number, filename, my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return file;
}

int File_io::close(File file, myf myFlags) {
  int result = mysql_file_close(file, MYF(0));
  if (result && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_BADCLOSE, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return result;
}

size_t File_io::read(File file, uchar *buffer, size_t count, myf myFlags) {
  size_t bytes_read = mysql_file_read(file, buffer, count, MYF(0));

  if (bytes_read != count && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_READ, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return bytes_read;
}

size_t File_io::write(File file, const uchar *buffer, size_t count,
                      myf myFlags) {
  size_t bytes_written = mysql_file_write(file, buffer, count, MYF(0));

  if (bytes_written != count && (myFlags & (MY_WME))) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_WRITE, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return bytes_written;
}

my_off_t File_io::seek(File file, my_off_t pos, int whence, myf myFlags) {
  my_off_t moved_to_position = mysql_file_seek(file, pos, whence, MYF(0));

  if (moved_to_position == MY_FILEPOS_ERROR && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_CANT_SEEK, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return moved_to_position;
}

my_off_t File_io::tell(File file, myf myFlags) {
  my_off_t position = mysql_file_tell(file, MYF(0));

  if ((position == ((my_off_t)-1)) && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_CANT_SEEK, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return position;
}

int File_io::sync(File file, myf myFlags) {
  int result = my_sync(file, MYF(0));

  if (result && (myFlags & MY_WME)) {
    char error_buffer[MYSYS_STRERROR_SIZE];
    my_warning(EE_SYNC, my_filename(file), my_errno(),
               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
  }
  return result;
}

int File_io::fstat(File file, MY_STAT *stat_area, myf myFlags) {
  int result = my_fstat(file, stat_area);

  if (result && (myFlags & MY_WME)) {
    std::stringstream error_message;
    error_message << "Error while reading stat for " << my_filename(file)
                  << ". Please check if file " << my_filename(file)
                  << " was not removed. OS returned this error: "
                  << strerror(errno);
    if (current_thd != nullptr && is_super_user())
      push_warning(current_thd, Sql_condition::SL_WARNING, errno,
                   error_message.str().c_str());
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_GET_FILE_STAT,
                my_filename(file), my_filename(file), strerror(errno));
  }
  return result;
}

bool File_io::remove(const char *filename, myf myFlags) {
  if (::remove(filename) != 0 && (myFlags & MY_WME)) {
    std::stringstream error_message;
    error_message << "Could not remove file " << filename
                  << " OS retuned this error: " << strerror(errno);
    if (current_thd != nullptr && is_super_user())
      push_warning(current_thd, Sql_condition::SL_WARNING, errno,
                   error_message.str().c_str());
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_REMOVE_FILE, filename,
                strerror(errno));
    return true;
  }
  return false;
}

bool File_io::truncate(File file, myf myFlags) {
#ifdef _WIN32
  LARGE_INTEGER length;
  length.QuadPart = 0;
  HANDLE hFile = my_get_osfhandle(file);

  if ((hFile == INVALID_HANDLE_VALUE ||
       !SetFilePointerEx(hFile, length, NULL, FILE_BEGIN) ||
       !SetEndOfFile(hFile)) &&
      (myFlags & MY_WME)) {
    my_osmaperr(GetLastError());
    set_my_errno(errno);
//    char error_buffer[MYSYS_STRERROR_SIZE];
//    my_warning(EE_CANT_SEEK, my_filename(file), my_errno(),
//               my_strerror(error_buffer, sizeof(error_buffer), my_errno()));
//    return true;
//  }
#elif defined(HAVE_FTRUNCATE)
  if (ftruncate(file, (off_t)0) && (myFlags & MY_WME)) {
#else
  assert(0);
#endif
    std::stringstream error_message;
    error_message << "Could not truncate file " << my_filename(file)
                  << ". OS retuned this error: " << strerror(errno);
    if (current_thd != nullptr && is_super_user())
      push_warning(current_thd, Sql_condition::SL_WARNING, errno,
                   error_message.str().c_str());
    logger->log(ERROR_LEVEL, ER_KEYRING_FAILED_TO_TRUNCATE_FILE,
                my_filename(file), strerror(errno));
    return true;
  }
  //#else
  //  assert(0);
  //#endif
  return false;
}  // namespace keyring

void File_io::my_warning(int nr, ...) {
  va_list args;
  const char *format;

  if (!(format = my_get_err_msg(nr))) {
    std::stringstream error_message;
    error_message << "Unknown error " << nr;
    if (current_thd != nullptr && is_super_user())
      push_warning(current_thd, Sql_condition::SL_WARNING, nr,
                   error_message.str().c_str());
    logger->log(ERROR_LEVEL, ER_KEYRING_UNKNOWN_ERROR, nr);
  } else {
    char warning[MYSQL_ERRMSG_SIZE];

    va_start(args, nr);
    vsnprintf(warning, sizeof(warning), format, args);
    va_end(args);
    if (current_thd != nullptr && is_super_user())
      push_warning(current_thd, Sql_condition::SL_WARNING, nr, warning);
    logger->log(ERROR_LEVEL, ER_KEYRING_FILE_IO_ERROR, warning);
  }
}

}  // namespace keyring
