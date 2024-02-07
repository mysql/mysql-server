/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/**
@file clone/include/clone_os.h
Clone Plugin: OS specific routines for IO and network

*/

#ifndef CLONE_OS_H
#define CLONE_OS_H

#include "my_sys.h"
#include "mysqld_error.h"
#include "mysys_err.h"
#include "sql/handler.h"

/** Alignment required for direct IO  */
const int CLONE_OS_ALIGN = 4 * 1024;

/** Default maximum concurrency for clone */
const uint CLONE_DEF_CON = 16;

/** Copy data from file to buffer
@param[in]     from_file   source file descriptor
@param[in,out] to_buffer   buffer to copy data
@param[in]     length      buffer/data length
@param[in]     src_name    source file name
@return error code */
int clone_os_copy_file_to_buf(Ha_clone_file from_file, uchar *to_buffer,
                              uint length, const char *src_name);

/** Check zero copy support
@return true if supports zero copy */
bool clone_os_supports_zero_copy();

/** Copy data from one file to another. File descriptors should be positioned
by caller. Uses sendfile on linux to avoid copy to user buffer. If sendfile
fails it switches to read/write similar to other platforms.
@param[in]	from_file	source file descriptor
@param[in]	to_file		destination file descriptor
@param[in]	length		length of data in bytes to copy
@param[in]	buffer		intermediate buffer for data transfer
@param[in]	buff_len	intermediate buffer length
@param[in]	src_name	source file name
@param[in]	dest_name	destination file name
@return error code */
int clone_os_copy_file_to_file(Ha_clone_file from_file, Ha_clone_file to_file,
                               uint length, uchar *buffer, uint buff_len,
                               const char *src_name, const char *dest_name);

/** Copy data from buffer to file. File descriptor should be positioned
by caller.
@param[in]	from_buffer	source buffer
@param[in]	to_file		destination file descriptor
@param[in]	length		length of data in bytes to copy
@param[in]	dest_name	destination file name
@return error code */
int clone_os_copy_buf_to_file(uchar *from_buffer, Ha_clone_file to_file,
                              uint length, const char *dest_name);

/** Send data from buffer to network.
@param[in]	from_buffer	source buffer
@param[in]	length		length of data in bytes to copy
@param[in]	socket		network socket
@param[in]	src_name	source file name
@return error code */
int clone_os_send_from_buf(uchar *from_buffer, uint length, my_socket socket,
                           const char *src_name);

/** Send data from file to network. File descriptor should be positioned
by caller.
@param[in]	from_file	source file descriptor
@param[in]	length		length of data in bytes to copy
@param[in]	socket		network socket
@param[in]	src_name	source file name
@return error code */
int clone_os_send_from_file(Ha_clone_file from_file, uint length,
                            my_socket socket, const char *src_name);

/** Receive data from network to buffer.
@param[in]	to_buffer	destination buffer
@param[in]	length		length of data in bytes to copy
@param[in]	socket		network socket
@param[in]	dest_name	destination file name
@return error code */
int clone_os_recv_to_buf(uchar *to_buffer, uint length, my_socket socket,
                         const char *dest_name);

/** Receive data from network to file. File descriptor should be positioned
by caller.
@param[in]	to_file		destination file descriptor
@param[in]	length		length of data in bytes to copy
@param[in]	socket		network socket
@param[in]	dest_name	destination file name
@return error code */
int clone_os_recv_to_file(Ha_clone_file to_file, uint length, my_socket socket,
                          const char *dest_name);

/** Check if a shared object is present and can be loaded.
@param[in]	path	shared object file name and path
@return true iff shared object could be loaded successfully. */
bool clone_os_test_load(std::string &path);

/** Align pointer to CLONE_OS_ALIGN[4k].
@param[in]	pointer	unaligned input
@return aligned pointer */
inline uchar *clone_os_align(uchar *pointer) {
  auto pointer_numeric = reinterpret_cast<uint64_t>(pointer);
  auto align = static_cast<uint64_t>(CLONE_OS_ALIGN - 1);

  auto aligned_ptr =
      reinterpret_cast<uchar *>((pointer_numeric + align) & ~align);

  return (aligned_ptr);
}

#endif /* CLONE_OS_H */
