/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ut/ut.cc
 Various utilities for Innobase.

 Created 5/11/1994 Heikki Tuuri
 ********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include <sys/types.h>
#include <time.h>

#include <iomanip>

#include "univ.i"
#include "ut/ut.h"

/** Prints the contents of a memory buffer in hex and ascii. */
void ut_print_buf(FILE *file,      /*!< in: file where to print */
                  const void *buf, /*!< in: memory buffer */
                  ulint len)       /*!< in: length of the buffer */
{
  const byte *data;
  ulint i;

  UNIV_MEM_ASSERT_RW(buf, len);

  fprintf(file, " len " ULINTPF "; hex ", len);

  for (data = (const byte *)buf, i = 0; i < len; i++) {
    fprintf(file, "%02lx", (ulong)*data++);
  }

  fputs("; asc ", file);

  data = (const byte *)buf;

  for (i = 0; i < len; i++) {
    int c = (int)*data++;
    putc(isprint(c) ? c : ' ', file);
  }

  putc(';', file);
}

/** Prints the contents of a memory buffer in hex.
@param[in,out] o Output stream
@param[in] buf Memory buffer
@param[in] len Length of the buffer */
void ut_print_buf_hex(std::ostream &o, const void *buf, ulint len) {
  auto ptr = reinterpret_cast<const byte *>(buf);
  const auto end = ptr + len;

  o << "(0x";
  while (ptr < end) {
    o << std::setfill('0') << std::setw(2) << std::hex << (int)*ptr;
    ++ptr;
  }
  o << ")";
}

/** Prints the contents of a memory buffer in hex and ascii.
@param[in,out] o Output stream
@param[in] buf Memory buffer
@param[in] len Length of the buffer */
void ut_print_buf(std::ostream &o, const void *buf, ulint len) {
  const byte *data;
  ulint i;

  UNIV_MEM_ASSERT_RW(buf, len);

  for (data = static_cast<const byte *>(buf), i = 0; i < len; i++) {
    int c = static_cast<int>(*data++);
    o << (isprint(c) ? static_cast<char>(c) : ' ');
  }

  ut_print_buf_hex(o, buf, len);
}

/** Prints a timestamp to a file. */
void ut_print_timestamp(FILE *file) /*!< in: file where to print */
{
#ifdef _WIN32
  SYSTEMTIME cal_tm;

  GetLocalTime(&cal_tm);

  fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %s", (int)cal_tm.wYear,
          (int)cal_tm.wMonth, (int)cal_tm.wDay, (int)cal_tm.wHour,
          (int)cal_tm.wMinute, (int)cal_tm.wSecond,
          to_string(std::this_thread::get_id(), true).c_str());
#else
  struct tm *cal_tm_ptr;
  time_t tm;

  struct tm cal_tm;
  time(&tm);
  localtime_r(&tm, &cal_tm);
  cal_tm_ptr = &cal_tm;
  fprintf(file, "%d-%02d-%02d %02d:%02d:%02d %s", cal_tm_ptr->tm_year + 1900,
          cal_tm_ptr->tm_mon + 1, cal_tm_ptr->tm_mday, cal_tm_ptr->tm_hour,
          cal_tm_ptr->tm_min, cal_tm_ptr->tm_sec,
          to_string(std::this_thread::get_id()).c_str());
#endif /* _WIN32 */
}

/** Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */
void ut_sprintf_timestamp(char *buf) /*!< in: buffer where to sprintf */
{
#ifdef _WIN32
  SYSTEMTIME cal_tm;

  GetLocalTime(&cal_tm);

  sprintf(buf, "%02d%02d%02d %2d:%02d:%02d", (int)cal_tm.wYear % 100,
          (int)cal_tm.wMonth, (int)cal_tm.wDay, (int)cal_tm.wHour,
          (int)cal_tm.wMinute, (int)cal_tm.wSecond);
#else
  struct tm *cal_tm_ptr;
  time_t tm;

  struct tm cal_tm;
  time(&tm);
  localtime_r(&tm, &cal_tm);
  cal_tm_ptr = &cal_tm;
  sprintf(buf, "%02d%02d%02d %2d:%02d:%02d", cal_tm_ptr->tm_year % 100,
          cal_tm_ptr->tm_mon + 1, cal_tm_ptr->tm_mday, cal_tm_ptr->tm_hour,
          cal_tm_ptr->tm_min, cal_tm_ptr->tm_sec);
#endif
}

/** Like ut_strlcpy, but if src doesn't fit in dst completely, copies the last
 (size - 1) bytes of src, not the first.
 @return strlen(src) */
ulint ut_strlcpy_rev(char *dst,       /*!< in: destination buffer */
                     const char *src, /*!< in: source buffer */
                     ulint size)      /*!< in: size of destination buffer */
{
  ulint src_size = strlen(src);

  if (size != 0) {
    ulint n = std::min(src_size, size - 1);

    memcpy(dst, src + src_size - n, n + 1);
  }

  return (src_size);
}
