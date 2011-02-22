
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdio.h>

#include "escape.hpp"
#include "string_buffer.hpp"
#include "fatal.hpp"
#include "string_util.hpp"

#define DBG_OP(x)
#define DBG_BUF(x)

namespace dena {

enum special_char_t {
  special_char_escape_prefix = 0x01,         /* SOH */
  special_char_noescape_min = 0x10,          /* DLE */
  special_char_escape_shift = 0x40,          /* '@' */
};

void
escape_string(char *& wp, const char *start, const char *finish)
{
  while (start != finish) {
    const unsigned char c = *start;
    if (c >= special_char_noescape_min) {
      wp[0] = c; /* no need to escape */
    } else {
      wp[0] = special_char_escape_prefix;
      ++wp;
      wp[0] = c + special_char_escape_shift;
    }
    ++start;
    ++wp;
  }
}

void
escape_string(string_buffer& ar, const char *start, const char *finish)
{
  const size_t buflen = (finish - start) * 2;
  char *const wp_begin = ar.make_space(buflen);
  char *wp = wp_begin;
  escape_string(wp, start, finish);
  ar.space_wrote(wp - wp_begin);
}

bool
unescape_string(char *& wp, const char *start, const char *finish)
{
  /* works even if wp == start */
  while (start != finish) {
    const unsigned char c = *start;
    if (c != special_char_escape_prefix) {
      wp[0] = c;
    } else if (start + 1 != finish) {
      ++start;
      const unsigned char cn = *start;
      if (cn < special_char_escape_shift) {
	return false;
      }
      wp[0] = cn - special_char_escape_shift;
    } else {
      return false;
    }
    ++start;
    ++wp;
  }
  return true;
}

bool
unescape_string(string_buffer& ar, const char *start, const char *finish)
{
  const size_t buflen = finish - start;
  char *const wp_begin = ar.make_space(buflen);
  char *wp = wp_begin;
  const bool r = unescape_string(wp, start, finish);
  ar.space_wrote(wp - wp_begin);
  return r;
}

uint32_t
read_ui32(char *& start, char *finish)
{
  char *const n_begin = start;
  read_token(start, finish);
  char *const n_end = start;
  uint32_t v = 0;
  for (char *p = n_begin; p != n_end; ++p) {
    const char ch = p[0];
    if (ch >= '0' && ch <= '9') {
      v *= 10;
      v += (ch - '0');
    }
  }
  return v;
}

void
write_ui32(string_buffer& buf, uint32_t v)
{
  char *wp = buf.make_space(32);
  int len = snprintf(wp, 32, "%u", v);
  if (len > 0) {
    buf.space_wrote(len);
  }
}

};

