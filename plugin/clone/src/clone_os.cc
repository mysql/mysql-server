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
@file clone/src/clone_os.cc
Clone Plugin: OS specific routines for IO and network

*/

#include "plugin/clone/include/clone_os.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

#ifdef _WIN32

/** Zero copy optimization */
static bool s_zero_copy = false;

/** Check and assert that file is a HANDLE */
#define CLONE_OS_CHECK_FILE(file) \
  assert(file.type == Ha_clone_file::FILE_HANDLE)

/** Implement read for Windows
@param[in]	file	file descriptor
@param[out]	buffer	read buffer
@param[in]	buf_len	length of buffer in bytes
@return error code */
static ssize_t os_read(Ha_clone_file file, uchar *buffer, uint buf_len) {
  assert(file.type == Ha_clone_file::FILE_HANDLE);

  auto file_hdl = static_cast<HANDLE>(file.file_handle);

  DWORD bytes_read;
  auto result =
      ReadFile(file_hdl, buffer, (DWORD)buf_len, &bytes_read, nullptr);

  if (!result) {
    auto win_error = GetLastError();

    if (win_error == ERROR_HANDLE_EOF) {
      return (0);
    }

    my_osmaperr(win_error);
    return (-1);
  }

  return (bytes_read);
}

/** Implement write for Windows
@param[in]	file	file descriptor
@param[in]	buffer	write buffer
@param[in]	buf_len	length of buffer in bytes
@return error code */
static ssize_t os_write(Ha_clone_file file, uchar *buffer, uint buf_len) {
  assert(file.type == Ha_clone_file::FILE_HANDLE);
  auto file_hdl = static_cast<HANDLE>(file.file_handle);

  DWORD bytes_written;
  auto result =
      WriteFile(file_hdl, buffer, (DWORD)buf_len, &bytes_written, nullptr);

  if (!result) {
    auto win_error = GetLastError();
    my_osmaperr(win_error);

    return (-1);
  }

  return (bytes_written);
}

#else

/** Zero copy optimization */
static bool s_zero_copy = true;

/** Check and assert that file is a descriptor */
#define CLONE_OS_CHECK_FILE(file) assert(file.type == Ha_clone_file::FILE_DESC)

/** Map to read system call for non-windows platforms. */
#define os_read(file, buffer, len) read(file.file_desc, buffer, len)

/** Map to write system call for non-windows platforms. */
#define os_write(file, buffer, len) write(file.file_desc, buffer, len)

#endif

bool clone_os_supports_zero_copy() { return (s_zero_copy); }

/** Read data from file to buffer.
@param[in]	from_file	source file descriptor
@param[in]	buffer		buffer for reading data
@param[in]	request_size	length of data to read
@param[in]	src_name	source file name
@param[out]	read_size	length of data actually read
@return error code */
static int read_from_file(Ha_clone_file from_file, uchar *buffer,
                          uint request_size, const char *src_name,
                          uint &read_size) {
  ssize_t ret_size = 0;

  do {
    errno = 0;
    ret_size = os_read(from_file, buffer, request_size);

    if (errno == EINTR) {
      DBUG_PRINT("debug", ("clone read() interrupted"));
    }

  } while (ret_size < 0 && errno == EINTR);

  if (ret_size == -1 || ret_size == 0) {
    char errbuf[MYSYS_STRERROR_SIZE];

    const int error = ER_ERROR_ON_READ;

    my_error(error, MYF(0), src_name, errno,
             my_strerror(errbuf, sizeof(errbuf), errno));

    return (error);
  }

  read_size = static_cast<uint>(ret_size);

  return (0);
}

int clone_os_copy_file_to_buf(Ha_clone_file from_file, uchar *to_buffer,
                              uint length, const char *src_name) {
  CLONE_OS_CHECK_FILE(from_file);

  /* Assert buffer alignment to CLONE_OS_ALIGN[4K] for O_DIRECT */
  assert(to_buffer == clone_os_align(to_buffer));

  auto len_left = length;

  while (len_left > 0) {
    uint ret_length = 0;
    auto error =
        read_from_file(from_file, to_buffer, len_left, src_name, ret_length);

    if (error != 0) {
      DBUG_PRINT("debug", ("Error: clone read failed."
                           " Length left = %u",
                           len_left));

      return (error);
    }

    len_left -= ret_length;
    to_buffer += ret_length;
  }

  return (0);
}

int clone_os_copy_file_to_file(Ha_clone_file from_file, Ha_clone_file to_file,
                               uint length, uchar *buffer, uint buff_len,
                               const char *src_name, const char *dest_name) {
  CLONE_OS_CHECK_FILE(from_file);

  CLONE_OS_CHECK_FILE(to_file);

#ifdef __linux__

  while (s_zero_copy && (buffer == nullptr) && length > 0) {
    auto ret_size =
        sendfile(to_file.file_desc, from_file.file_desc, nullptr, length);

    if (ret_size == -1 || ret_size == 0) {
      DBUG_PRINT("debug", ("sendfile returned Error (-1) or (0)"
                           " src file: %s dest file: %s"
                           " OS Error no: %d mesg = %s"
                           " Fallback to read/write.",
                           src_name, dest_name, errno, strerror(errno)));

      s_zero_copy = false;
      break;
    }

    auto actual_size = static_cast<uint>(ret_size);

    assert(length >= actual_size);
    length -= actual_size;
  }

  if (length == 0) {
    return (0);
  }
#endif
  int error;
  uchar buf_stack[2 * CLONE_OS_ALIGN];

  /* Use stack buffer if no transfer buffer is passed. */
  if (buffer == nullptr || buff_len < (2 * CLONE_OS_ALIGN)) {
    buffer = buf_stack;

    /* Align buffer to CLONE_OS_ALIGN for O_DIRECT */
    buffer = clone_os_align(buffer);
    buff_len = CLONE_OS_ALIGN;
  }

  /* Assert buffer alignment to CLONE_OS_ALIGN for O_DIRECT */
  assert(buffer == clone_os_align(buffer));

  while (length > 0) {
    auto request_size = (length > buff_len) ? buff_len : length;
    uint actual_size = 0;

    error =
        read_from_file(from_file, buffer, request_size, src_name, actual_size);

    if (error != 0) {
      DBUG_PRINT("debug", ("Error: clone read failed."
                           " Length left = %u",
                           length));

      return (error);
    }

    assert(length >= actual_size);
    length -= actual_size;

    request_size = actual_size;

    error = clone_os_copy_buf_to_file(buffer, to_file, request_size, dest_name);

    if (error != 0) {
      return (error);
    }
  }

  return (0);
}

int clone_os_copy_buf_to_file(uchar *from_buffer, Ha_clone_file to_file,
                              uint length, const char *dest_name) {
  CLONE_OS_CHECK_FILE(to_file);

  while (length > 0) {
    errno = 0;
    auto ret_size = os_write(to_file, from_buffer, length);

    if (errno == EINTR) {
      DBUG_PRINT("debug", ("clone write() interrupted"));
      continue;
    }

    if (ret_size == -1) {
      char errbuf[MYSYS_STRERROR_SIZE];

      my_error(ER_ERROR_ON_WRITE, MYF(0), dest_name, errno,
               my_strerror(errbuf, sizeof(errbuf), errno));

      DBUG_PRINT("debug", ("Error: clone write failed."
                           " Length left = %u",
                           length));

      return (ER_ERROR_ON_WRITE);
    }

    auto actual_size = static_cast<uint>(ret_size);

    assert(length >= actual_size);

    length -= actual_size;
    from_buffer += actual_size;
  }

  return (0);
}

int clone_os_send_from_buf(uchar *from_buffer [[maybe_unused]],
                           uint length [[maybe_unused]],
                           my_socket socket [[maybe_unused]],
                           const char *src_name [[maybe_unused]]) {
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Remote Clone Send");
  return (ER_NOT_SUPPORTED_YET);
}

int clone_os_send_from_file(Ha_clone_file from_file [[maybe_unused]],
                            uint length [[maybe_unused]],
                            my_socket socket [[maybe_unused]],
                            const char *src_name [[maybe_unused]]) {
  CLONE_OS_CHECK_FILE(from_file);

  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Remote Clone Send");
  return (ER_NOT_SUPPORTED_YET);
}

int clone_os_recv_to_buf(uchar *to_buffer [[maybe_unused]],
                         uint length [[maybe_unused]],
                         my_socket socket [[maybe_unused]],
                         const char *dest_name [[maybe_unused]]) {
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Remote Clone Receive");
  return (ER_NOT_SUPPORTED_YET);
}

int clone_os_recv_to_file(Ha_clone_file to_file [[maybe_unused]],
                          uint length [[maybe_unused]],
                          my_socket socket [[maybe_unused]],
                          const char *dest_name [[maybe_unused]]) {
  CLONE_OS_CHECK_FILE(to_file);

  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Remote Clone Receive");
  return (ER_NOT_SUPPORTED_YET);
}

bool clone_os_test_load(std::string &path) {
  char dlpath[FN_REFLEN];

  unpack_filename(dlpath, path.c_str());
  auto handle = dlopen(dlpath, RTLD_NOW);

  if (handle == nullptr) {
    return false;
  }

  dlclose(handle);
  return true;
}
