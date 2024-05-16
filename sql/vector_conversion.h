/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef VECTOR_CONV_INCLUDED
#define VECTOR_CONV_INCLUDED

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <cstring>
#include "sql_string.h"

static inline bool from_string_to_vector(const char *input, uint32_t input_len,
                                         char *const output,
                                         uint32_t *max_output_dims) {
  if (input == nullptr || input[0] != '[' || input_len == 0 ||
      input[input_len - 1] != ']') {
    *max_output_dims = 0;
    return true;
  }

  // Check for memory region overlap
  size_t output_len = sizeof(float) * (*max_output_dims);
  String temp_output(output, output_len, nullptr);
  if (output + output_len >= input && input + input_len >= output) {
    temp_output = String(output_len);
  }

  const char *const input_end = input + input_len - 1;
  input = input + 1;
  uint32_t dim = 0;
  char *end = nullptr;
  bool with_success = false;
  errno = 0;
  for (float fnum = strtof(input, &end); input != end;
       fnum = strtof(input, &end)) {
    input = end;
    if (errno == ERANGE || dim >= *max_output_dims || std::isnan(fnum) ||
        std::isinf(fnum)) {
      errno = 0;
      break;
    }
    memcpy(temp_output.ptr() + dim * sizeof(float), &fnum, sizeof(float));

    if (*input == ',') {
      input = input + 1;
      dim++;
    } else if (*input == ']' && input == input_end) {
      with_success = true;
      dim++;
      break;
    } else {
      break;
    }
  }

  if (temp_output.ptr() != output) {
    memcpy(output, temp_output.ptr(), dim * sizeof(float));
  }

  *max_output_dims = dim;
  return !with_success;
}

static inline bool from_vector_to_string(const char *const input,
                                         uint32_t input_dims,
                                         char *const output,
                                         uint32_t *max_output_len) {
  const uint32_t end_cushion = 12;
  if (input == nullptr || *max_output_len < end_cushion) {
    return true;
  }

  // Check for memory region overlap
  size_t input_len = input_dims * sizeof(float);
  String temp_output(output, *max_output_len, nullptr);
  if (output + *max_output_len >= input && input + input_len >= output) {
    temp_output = String(*max_output_len);
  }

  char *write_ptr = temp_output.ptr();
  uint32_t total_length = 1;
  write_ptr[0] = '[';
  write_ptr += 1;
  for (uint32_t i = 0; i < input_dims; i++) {
    int remaining_bytes = *max_output_len - total_length;
    int nchars = 0;
    if (*max_output_len <= total_length + end_cushion) {
      nchars = snprintf(write_ptr, remaining_bytes, "...");
      i = input_dims;
    } else {
      char delimiter = (i == input_dims - 1) ? ']' : ',';
      float input_value = 0;
      memcpy(&input_value, input + i * sizeof(float), sizeof(float));
      nchars = snprintf(write_ptr, remaining_bytes, "%.5e%c", input_value,
                        delimiter);
      if (nchars < 0 || nchars >= remaining_bytes) {
        return true; /* LCOV_EXCL_LINE */
      }
    }
    write_ptr += nchars;
    total_length += nchars;
  }

  if (temp_output.ptr() != output) {
    memcpy(output, temp_output.ptr(), total_length);
  }

  *max_output_len = total_length;
  return false;
}

static inline uint32_t get_dimensions(const uint32_t length,
                                      const uint32_t precision) {
  if (length == 0 || (length % precision > 0)) {
    return UINT32_MAX;
  }
  return length / precision;
}

#endif /* VECTOR_CONV_INCLUDED */