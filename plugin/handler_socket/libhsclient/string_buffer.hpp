
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_STRING_BUFFER_HPP
#define DENA_STRING_BUFFER_HPP

#include <vector>
#include <stdlib.h>
#include <string.h>

#include "util.hpp"
#include "allocator.hpp"
#include "fatal.hpp"

namespace dena {

struct string_buffer : private noncopyable {
  string_buffer() : buffer(0), begin_offset(0), end_offset(0), alloc_size(0) { }
  ~string_buffer() {
    DENA_FREE(buffer);
  }
  const char *begin() const {
    return buffer + begin_offset;
  }
  const char *end() const {
    return buffer + end_offset;
  }
  char *begin() {
    return buffer + begin_offset;
  }
  char *end() {
    return buffer + end_offset;
  }
  size_t size() const {
    return end_offset - begin_offset;
  }
  void clear() {
    begin_offset = end_offset = 0;
  }
  void resize(size_t len) {
    if (size() < len) {
      reserve(len);
      memset(buffer + end_offset, 0, len - size());
    }
    end_offset = begin_offset + len;
  }
  void reserve(size_t len) {
    if (alloc_size >= begin_offset + len) {
      return;
    }
    size_t asz = alloc_size;
    while (asz < begin_offset + len) {
      if (asz == 0) {
	asz = 16;
      }
      const size_t asz_n = asz << 1;
      if (asz_n < asz) {
	fatal_abort("string_buffer::resize() overflow");
      }
      asz = asz_n;
    }
    void *const p = DENA_REALLOC(buffer, asz);
    if (p == 0) {
      fatal_abort("string_buffer::resize() realloc");
    }
    buffer = static_cast<char *>(p);
    alloc_size = asz;
  }
  void erase_front(size_t len) {
    if (len >= size()) {
      clear();
    } else {
      begin_offset += len;
    }
  }
  char *make_space(size_t len) {
    reserve(size() + len);
    return buffer + end_offset;
  }
  void space_wrote(size_t len) {
    len = std::min(len, alloc_size - end_offset);
    end_offset = std::min(end_offset + len, alloc_size);
  }
  template <size_t N>
  void append_literal(const char (& str)[N]) {
    append(str, str + N - 1);
  }
  void append(const char *start, const char *finish) {
    const size_t len = finish - start;
    reserve(size() + len);
    memcpy(buffer + end_offset, start, len);
    end_offset += len;
  }
  void append_2(const char *s1, const char *f1, const char *s2,
    const char *f2) {
    const size_t l1 = f1 - s1;
    const size_t l2 = f2 - s2;
    reserve(end_offset + l1 + l2);
    memcpy(buffer + end_offset, s1, l1);
    memcpy(buffer + end_offset + l1, s2, l2);
    end_offset += l1 + l2;
  }
 private:
  char *buffer;
  size_t begin_offset;
  size_t end_offset;
  size_t alloc_size;
};

};

#endif

