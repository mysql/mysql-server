
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_STRING_REF_HPP
#define DENA_STRING_REF_HPP

#include <vector>
#include <string.h>

namespace dena {

struct string_wref {
  typedef char value_type;
  char *begin() const { return start; }
  char *end() const { return start + length; }
  size_t size() const { return length; }
 private:
  char *start;
  size_t length;
 public:
  string_wref(char *s = 0, size_t len = 0) : start(s), length(len) { }
};

struct string_ref {
  typedef const char value_type;
  const char *begin() const { return start; }
  const char *end() const { return start + length; }
  size_t size() const { return length; }
 private:
  const char *start;
  size_t length;
 public:
  string_ref(const char *s = 0, size_t len = 0) : start(s), length(len) { }
  string_ref(const char *s, const char *f) : start(s), length(f - s) { }
  string_ref(const string_wref& w) : start(w.begin()), length(w.size()) { }
};

template <size_t N> inline bool
operator ==(const string_ref& x, const char (& y)[N]) {
  return (x.size() == N - 1) && (::memcmp(x.begin(), y, N - 1) == 0);
}

inline bool
operator ==(const string_ref& x, const string_ref& y) {
  return (x.size() == y.size()) &&
    (::memcmp(x.begin(), y.begin(), x.size()) == 0);
}

inline bool
operator !=(const string_ref& x, const string_ref& y) {
  return (x.size() != y.size()) ||
    (::memcmp(x.begin(), y.begin(), x.size()) != 0);
}

};

#endif

