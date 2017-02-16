
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_STRING_UTIL_HPP
#define DENA_STRING_UTIL_HPP

#include <string>
#include <string.h>
#include <stdint.h>

#include "string_buffer.hpp"
#include "string_ref.hpp"

namespace dena {

inline const char *
memchr_char(const char *s, int c, size_t n)
{
  return static_cast<const char *>(memchr(s, c, n));
}

inline char *
memchr_char(char *s, int c, size_t n)
{
  return static_cast<char *>(memchr(s, c, n));
}

string_wref get_token(char *& wp, char *wp_end, char delim);
uint32_t atoi_uint32_nocheck(const char *start, const char *finish);
std::string to_stdstring(uint32_t v);
void append_uint32(string_buffer& buf, uint32_t v);
long long atoll_nocheck(const char *start, const char *finish);

int errno_string(const char *s, int en, std::string& err_r);

size_t split(char delim, const string_ref& buf, string_ref *parts,
  size_t parts_len);
size_t split(char delim, const string_wref& buf, string_wref *parts,
  size_t parts_len);
size_t split(char delim, const string_ref& buf,
  std::vector<string_ref>& parts_r);
size_t split(char delim, const string_wref& buf,
  std::vector<string_wref>& parts_r);

};

#endif

