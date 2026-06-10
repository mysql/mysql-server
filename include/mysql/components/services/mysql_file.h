/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_FILE_IO_H
#define MYSQL_FILE_IO_H

#include <mysql/components/service.h>
#include <cstddef>

/**
  File access flags.
*/
/* Open for reading only. */
#define MY_FILE_O_RDONLY 0
/* Open for writing only. */
#define MY_FILE_O_WRONLY 0x1
/* Open for reading and writing.  */
#define MY_FILE_O_RDWR 0x2
/* Mask for access mode (combines O_RDONLY, O_WRONLY, O_RDWR). */
#define MY_FILE_O_ACCMODE (MY_FILE_O_WRONLY | MY_FILE_O_RDWR)
/* Create file if it does not exist. */
#define MY_FILE_O_CREAT 0x4
/* Error if file already exists. */
#define MY_FILE_O_EXCL 0x8
/* Do not assign controlling terminal. */
#define MY_FILE_O_NOCTTY 0x10
/* Truncate size to 0 if file exists. */
#define MY_FILE_O_TRUNC 0x20
/* Append on each write. */
#define MY_FILE_O_APPEND 0x40
/* Non-blocking mode. */
#define MY_FILE_O_NONBLOCK 0x80
/* Synchronous writes; ensure data is physically written. */
#define MY_FILE_O_SYNC 0x100
/* Enable signal-driven I/O. */
#define MY_FILE_FASYNC 0x200
/* Minimize cache effects, use direct I/O if possible. */
#define MY_FILE_O_DIRECT 0x400
/* Allow files larger than 2GB. */
#define MY_FILE_O_LARGEFILE 0x800
/* Fail if not a directory. */
#define MY_FILE_O_DIRECTORY 0x1000
/* Do not follow symbolic links. */
#define MY_FILE_O_NOFOLLOW 0x2000
/* Do not update access time. */
#define MY_FILE_O_NOATIME 0x4000
/* Set close-on-exec. */
#define MY_FILE_O_CLOEXEC 0x8000
/* Open as text file. */
#define MY_FILE_O_TEXT 0x10000
/* Open as binary file. */
#define MY_FILE_O_BINARY 0x20000
/* Open as raw device/file. */
#define MY_FILE_O_RAW 0x40000
/* Open as a temporary file. */
#define MY_FILE_O_TEMPORARY 0x80000
/* Prevent file from being inherited by child processes. */
#define MY_FILE_O_NOINHERIT 0x100000
/* Access file sequentially. */
#define MY_FILE_O_SEQUENTIAL 0x200000
/* Access file randomly. */
#define MY_FILE_O_RANDOM 0x400000

/**
  File permission flags.
*/
/* Read permission for the file owner (POSIX S_IRUSR). */
#define MY_FILE_PERMISSION_USER_READ 0x1
/* Write permission for the file owner (POSIX S_IWUSR). */
#define MY_FILE_PERMISSION_USER_WRITE 0x2
/* Execute/search permission for the file owner (POSIX S_IXUSR). */
#define MY_FILE_PERMISSION_USER_EXECUTE 0x4
/* Read permission for the group (POSIX S_IRGRP). */
#define MY_FILE_PERMISSION_GROUP_READ 0x8
/* Write permission for the group (POSIX S_IWGRP). */
#define MY_FILE_PERMISSION_GROUP_WRITE 0x10
/* Execute/search permission for the group (POSIX S_IXGRP). */
#define MY_FILE_PERMISSION_GROUP_EXECUTE 0x20
/* Read permission for others (POSIX S_IROTH). */
#define MY_FILE_PERMISSION_OTHERS_READ 0x40
/* Write permission for others (POSIX S_IWOTH). */
#define MY_FILE_PERMISSION_OTHERS_WRITE 0x80
/* Execute/search permission for others (POSIX S_IXOTH). */
#define MY_FILE_PERMISSION_OTHERS_EXECUTE 0x100
/* Read, write, and execute permissions for owner. */
#define MY_FILE_PERMISSION_USER_RWX                               \
  (MY_FILE_PERMISSION_USER_READ | MY_FILE_PERMISSION_USER_WRITE | \
   MY_FILE_PERMISSION_USER_EXECUTE)
/* Read, write, and execute permissions for group. */
#define MY_FILE_PERMISSION_GROUP_RWX                                \
  (MY_FILE_PERMISSION_GROUP_READ | MY_FILE_PERMISSION_GROUP_WRITE | \
   MY_FILE_PERMISSION_GROUP_EXECUTE)
/* Read, write, and execute permissions for others. */
#define MY_FILE_PERMISSION_OTHERS_RWX                                 \
  (MY_FILE_PERMISSION_OTHERS_READ | MY_FILE_PERMISSION_OTHERS_WRITE | \
   MY_FILE_PERMISSION_OTHERS_EXECUTE)

/*
  File positioning flags.
*/
/* Seek from the beginning of the file. */
#define MY_FILE_SEEK_SET 0
/* Seek from the current position in the file. */
#define MY_FILE_SEEK_CUR 0x1
/* Seek from the end of the file. */
#define MY_FILE_SEEK_END 0x2

/*
  Error codes.
*/
/* Read/write error. */
#define MY_FILE_ERROR_IO (~(size_t)0)
/* File position error code. */
#define MY_FILE_ERROR_POS (~(unsigned long long)0)

DEFINE_SERVICE_HANDLE(FILE_h);

/**
  @ingroup group_components_services_inventory

  File service allows file manipulation depending on the "mysql_file" component
  service implementation. Every function has additional my_flags argument that
  can be customized depending on the custom implementation.
*/
BEGIN_SERVICE_DEFINITION(mysql_file)

/**
  Open a file.

  @param file_name  File path.
  @param open_flags File open flags starting with MY_FILE_O prefix, e.g.
                    MY_FILE_O_RDONLY.

  @return Non zero file handle on success, otherwise failed.
*/
DECLARE_METHOD(FILE_h, open, (const char *file_name, int open_flags));

/**
  Create a new file.

  @param file_name        File path.
  @param open_flags       File open flags starting with MY_FILE_O prefix, e.g.
                          MY_FILE_O_RDONLY.
  @param permission_flags File permission specifier flags starting with
                          MY_FILE_PERMISSION prefix.

  @return Non zero file handle on success, otherwise failed.
*/
DECLARE_METHOD(FILE_h, create,
               (const char *file_name, int open_flags, int permission_flags));

/**
  Close a file previously opened or created using the open/create functions.

  @param file     File handle.

  @return Zero value on success, otherwise failed.
*/
DECLARE_METHOD(int, close, (FILE_h file));

/**
  Write data into a file

  @param file     File handle.
  @param data     Data to be written.
  @param size     Data size.

  @return Number of bytes written.
*/
DECLARE_METHOD(size_t, write,
               (FILE_h file, const unsigned char *data, size_t size));

/**
  Read data from a file.

  @param file     File handle.
  @param data     Data buffer, where data will be written.
  @param size     Data buffer size.

  @return Number of bytes read.
*/
DECLARE_METHOD(size_t, read, (FILE_h file, unsigned char *data, size_t size));

/**
  Flush written data into the file.

  @param file     File handle.

  @return Zero value on success, otherwise failed.
*/
DECLARE_METHOD(int, flush, (FILE_h file));

/**
  Go to the specified position within a file.

  @param file     File handle.
  @param pos      A new read/write position within a file.
  @param whence   File position flag starting with MY_FILE_SEEK prefix.

  @return Non MY_FILE_ERROR_POS value on success, otherwise failed.
*/
DECLARE_METHOD(unsigned long long, seek,
               (FILE_h file, unsigned long long pos, int whence));

/**
  Get current absolute position within a file.

  @param file     File handle.

  @return Position within a file on success or MY_FILE_ERROR_POS on failure.
*/
DECLARE_METHOD(unsigned long long, tell, (FILE_h file));

END_SERVICE_DEFINITION(mysql_file)

#endif /* MYSQL_FILE_IO_H */
