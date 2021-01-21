/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UTIL_NDBXFRM_ITERATOR
#define UTIL_NDBXFRM_ITERATOR

#include <assert.h> // assert()
#include <stdlib.h> // abort()

#include "ndb_global.h" // require()

class ndbxfrm_input_iterator
{
public:
  using byte = unsigned char;
  ndbxfrm_input_iterator(const byte* begin, const byte* end, bool last)
    : m_begin{begin}, m_end{end}, m_last{last} {}
  const byte* cbegin() const { return m_begin; }
  const byte* cend() const { return m_end; }
  size_t size() const { return m_end - m_begin; }
  bool empty() const { return (m_end == m_begin); }
  bool last() const { return m_last; }
  void set_last() { m_last = true; }
  void advance(size_t n) { require(n <= size()); m_begin += n; }
  void reduce(size_t n) { require(n <= size()); m_end -= n; }
private:
  const byte* m_begin;
  const byte* m_end;
  bool m_last;
};

class ndbxfrm_input_reverse_iterator
{
public:
  using byte = unsigned char;
  ndbxfrm_input_reverse_iterator(const byte* begin, const byte* end, bool last)
    : m_begin{begin}, m_end{end}, m_last{last} {}
  const byte* cbegin() const { return m_begin; }
  const byte* cend() const { return m_end; }
  size_t size() const { return m_begin - m_end; }
  bool empty() const { return (m_end == m_begin); }
  bool last() const { return m_last; }
  void set_last() { m_last = true; }
  void advance(size_t n) { require(n <= size()); m_begin -= n; }
  void reduce(size_t n) { require(n <= size()); m_end += n; }
private:
  const byte* m_begin;
  const byte* m_end;
  bool m_last;
};

class ndbxfrm_output_iterator
{
public:
  using byte = unsigned char;
  ndbxfrm_output_iterator(byte* begin, byte* end, bool last)
    : m_begin{begin}, m_end{end}, m_last{last} {}
  byte* begin() const { return m_begin; }
  byte* end() const { return m_end; }
  size_t size() const { return m_end - m_begin; }
  bool empty() const { return (m_end == m_begin); }
  bool last() const { return m_last; }
  void set_last() { m_last = true; }
  void advance(size_t n) { require(n <= size()); m_begin += n; }
  void reduce(size_t n) { require(n <= size()); m_end -= n; }
private:
  byte* m_begin;
  byte* m_end;
  bool m_last;
};

class ndbxfrm_output_reverse_iterator
{
public:
  using byte = unsigned char;
  ndbxfrm_output_reverse_iterator(byte* begin, byte* end, bool last)
    : m_begin{begin}, m_end{end}, m_last{last} {}
  byte* begin() const { return m_begin; }
  byte* end() const { return m_end; }
  size_t size() const { return m_begin - m_end; }
  bool empty() const { return (m_end == m_begin); }
  bool last() const { return m_last; }
  void set_last() { m_last = true; }
  void advance(size_t n) { require(n <= size()); m_begin -= n; }
  void reduce(size_t n) { require(n <= size()); m_end += n; }
private:
  byte* m_begin;
  byte* m_end;
  bool m_last;
};

#endif
