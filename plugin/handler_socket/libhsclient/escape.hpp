
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdint.h>

#include "string_buffer.hpp"
#include "string_ref.hpp"
#include "string_util.hpp"

#ifndef DENA_ESCAPE_HPP
#define DENA_ESCAPE_HPP

namespace dena {

void escape_string(char *& wp, const char *start, const char *finish);
void escape_string(string_buffer& ar, const char *start, const char *finish);
bool unescape_string(char *& wp, const char *start, const char *finish);
  /* unescaped_string() works even if wp == start */
bool unescape_string(string_buffer& ar, const char *start, const char *finish);

uint32_t read_ui32(char *& start, char *finish);
void write_ui32(string_buffer& buf, uint32_t v);
void write_ui64(string_buffer& buf, uint64_t v);

inline bool
is_null_expression(const char *start, const char *finish)
{
  return (finish == start + 1 && start[0] == 0);
}

inline void
read_token(char *& start, char *finish)
{
  char *const p = memchr_char(start, '\t', finish - start);
  if (p == 0) {
    start = finish;
  } else {
    start = p;
  }
}

inline void
skip_token_delim_fold(char *& start, char *finish)
{
  while (start != finish && start[0] == '\t') {
    ++start;
  }
}

inline void
skip_one(char *& start, char *finish)
{
  if (start != finish) {
    ++start;
  }
}

};

#endif

