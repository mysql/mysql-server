/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

#ifndef NDB_UTIL_CSTRBUF_H
#define NDB_UTIL_CSTRBUF_H

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include "portlib/ndb_compiler.h"
#include "util/span.h"

/*
 * cstrbuf - safe copying and formatting of strings into fixed size buffer
 *
 * Implementation do not use exceptions or heap allocation, except implicit
 * allocations that vsnprintf may do.
 *
 * cstrbuf can either keep its own fixed size buffer or refer to a fixed
 * size contiguous span of an existing buffer.
 *
 * Either way cstrbuf always start with clearing the buffer to contain an empty
 * string.
 *
 * The string value of cstrbuf is always null terminated.
 *
 * When appending strings and the string will not fit the buffer, the string
 * will be truncated, still null terminated in buffer.
 *
 * cstrbuf keep track of what length the untruncated string would have had.
 *
 * Examples (see also storage/ndb/src/common/util/cstrbuf.cpp)
 *
 * cstrbuf<10> buf; // cstrbuf keep its own char[10] buffer.
 * buf.append("Hello");
 * buf.appendf(" %s!", "world");
 * size_t full_length = buf.untruncated_length();
 * buf.replace_end_if_truncated("..."); // Note, value no longer truncated
 * printf("Say: length %zu: %s\n", full_length, buf.c_str());
 *
 * char cbuf[10];
 * cstrbuf buf(cbuf); // cstrbuf refers to cbuf
 *
 * std::array<char, 10> arr;
 * cstrbuf buf(arr); // cstrbuf refers to arr
 *
 * cstrbuf buf({cbuf + 2, cbuf + 8}); // cstrbuf refers to cbuf[2..8]
 *
 * cstrbuf buf({cbuf + 2, 6}); // cstrbuf refers to cbuf[2..8]
 *
 * For those cases that one only want to do one copy into a buffer there are
 * some free functions to use.
 *
 * if (cstrbuf_copy(buf, "Hello!") == 0) ... no truncation ...
 * if (cstrbuf_format(buf, "Hello %s!", name) == -1) ... some error ...
 *
 */

template <std::size_t Extent, bool Owning = true>
class cstrbuf
{
 public:
  cstrbuf() noexcept;
  explicit cstrbuf(ndb::span<char, Extent> buf) noexcept;
  explicit cstrbuf(const cstrbuf &) = delete;
  cstrbuf &operator=(const cstrbuf &) = delete;

  constexpr operator std::string_view() const noexcept;

  constexpr int append(const std::string_view other) noexcept;
  constexpr int append(std::size_t count, char ch) noexcept;

  [[nodiscard]] int appendf(const char fmt[], ...) noexcept
      ATTRIBUTE_FORMAT(printf, 2, 3);
  [[nodiscard]] int appendf(const char fmt[], std::va_list ap) noexcept
      ATTRIBUTE_FORMAT(printf, 2, 0);

  // buffer properties
  constexpr std::size_t extent() const noexcept;

  // string value properties
  constexpr bool is_truncated() const noexcept;
  constexpr std::size_t length() const noexcept;
  constexpr std::size_t untruncated_length() const noexcept;
  void clear() noexcept;
  const char *c_str() const noexcept;

  // delete size() since ambiguous if it should mean string size or buffer size
  constexpr std::size_t size() const noexcept = delete;

  /*
   * replace_end_if_truncated
   *
   * If cstrbuf is truncated the end of string is replaced with truncated_mark
   * and string will no longer be truncated. If string was truncated and end
   * replaced the function returns 1, else 0. Return value -1 is reserved for
   * errors, but currently not used.
   */
  template <std::size_t N>
  constexpr int replace_end_if_truncated(
      const char (&truncated_mark)[N]) noexcept;

  constexpr int replace_end_if_truncated(
      std::string_view truncated_mark) noexcept;

 private:
  constexpr char *next() noexcept { return &m_buf[m_next_pos]; }

  using Container = typename std::conditional<Owning,
                                              std::array<char, Extent>,
                                              ndb::span<char, Extent>>::type;

  Container m_buf;
  std::size_t m_next_pos;
};

// deduction guides - static extent

template <std::size_t Extent>
cstrbuf(char (&)[Extent]) -> cstrbuf<Extent, false>;

template <std::size_t Extent>
cstrbuf(std::array<char, Extent> &) -> cstrbuf<Extent, false>;

// deduction guides - ndb::span

template <std::size_t Extent = ndb::dynamic_extent>
cstrbuf(ndb::span<char, Extent>) -> cstrbuf<Extent, false>;

template <class Container>
cstrbuf(Container &) -> cstrbuf<ndb::dynamic_extent, false>;

// free functions

template <std::size_t Extent = ndb::dynamic_extent>
[[nodiscard]] int cstrbuf_copy(ndb::span<char, Extent> buf,
                               const std::string_view other) noexcept;

template <std::size_t Extent>
[[nodiscard]] int cstrbuf_copy(char (&buf)[Extent],
                               const std::string_view other) noexcept;

template <std::size_t Extent = ndb::dynamic_extent>
[[nodiscard]] int cstrbuf_format(ndb::span<char, Extent> buf,
                                 const char fmt[],
                                 ...) noexcept ATTRIBUTE_FORMAT(printf, 2, 3);

template <std::size_t Extent>
[[nodiscard]] int cstrbuf_format(char (&buf)[Extent],
                                 const char fmt[],
                                 ...) noexcept ATTRIBUTE_FORMAT(printf, 2, 3);

template <std::size_t Extent = ndb::dynamic_extent>
[[nodiscard]] int cstrbuf_format(ndb::span<char, Extent> buf,
                                 const char fmt[],
                                 std::va_list ap) noexcept
    ATTRIBUTE_FORMAT(printf, 2, 0);

template <std::size_t Extent>
[[nodiscard]] int cstrbuf_format(char (&buf)[Extent],
                                 const char fmt[],
                                 std::va_list ap) noexcept
    ATTRIBUTE_FORMAT(printf, 2, 0);

// implementation

template <std::size_t Extent, bool Owning>
inline cstrbuf<Extent, Owning>::cstrbuf() noexcept : m_next_pos(0)
{
  static_assert(Owning);
  if (!m_buf.empty())
  {
    *next() = '\0';
  }
}

template <std::size_t Extent, bool Owning>
inline cstrbuf<Extent, Owning>::cstrbuf(ndb::span<char, Extent> buf) noexcept
    : m_buf(buf), m_next_pos(0)
{
  static_assert(!Owning);
  if (!buf.empty())
  {
    *next() = '\0';
  }
}

template <std::size_t Extent, bool Owning>
inline constexpr cstrbuf<Extent, Owning>::operator std::string_view()
    const noexcept
{
  return {m_buf.data(), length()};
}

template <std::size_t Extent, bool Owning>
inline constexpr int cstrbuf<Extent, Owning>::append(
    const std::string_view other) noexcept
{
  if (!is_truncated() && other.length() > 0)
  {
    const std::size_t space_left = extent() - m_next_pos;
    const bool truncated = (other.length() >= space_left);
    const std::size_t len = (truncated ? space_left - 1 : other.length());
    std::copy_n(other.data(), len, next());
    next()[len] = '\0';
  }
  m_next_pos += other.length();
  return (is_truncated() ? 1 : 0);
}

template <std::size_t Extent, bool Owning>
inline constexpr int cstrbuf<Extent, Owning>::append(std::size_t count,
                                                     char ch) noexcept
{
  if (!is_truncated())
  {
    const std::size_t space_left = extent() - m_next_pos;
    const bool truncated = (count >= space_left);
    const std::size_t len = (truncated ? space_left - 1 : count);
    std::fill_n(next(), len, ch);
    next()[len] = '\0';
  }
  m_next_pos += count;
  return (is_truncated() ? 1 : 0);
}

template <std::size_t Extent, bool Owning>
inline constexpr bool cstrbuf<Extent, Owning>::is_truncated() const noexcept
{
  return (m_next_pos >= extent());
}

template <std::size_t Extent, bool Owning>
inline constexpr std::size_t cstrbuf<Extent, Owning>::length() const noexcept
{
  if (m_next_pos < extent()) return m_next_pos;
  if (extent() > 0) return extent() - 1;
  return 0;
}

template <std::size_t Extent, bool Owning>
inline constexpr std::size_t cstrbuf<Extent, Owning>::untruncated_length()
    const noexcept
{
  return m_next_pos;
}

template <std::size_t Extent, bool Owning>
inline void cstrbuf<Extent, Owning>::clear() noexcept
{
  if (!m_buf.empty())
  {
    m_next_pos = 0;
    *next() = '\0';
  }
}

template <std::size_t Extent, bool Owning>
inline const char *cstrbuf<Extent, Owning>::c_str() const noexcept
{
  return m_buf.data();
}

template <std::size_t Extent, bool Owning>
inline constexpr std::size_t cstrbuf<Extent, Owning>::extent() const noexcept
{
  if constexpr (Extent != ndb::dynamic_extent) assert(Extent == m_buf.size());
  return m_buf.size();
}

template <std::size_t Extent, bool Owning>
inline int cstrbuf<Extent, Owning>::appendf(const char fmt[], ...) noexcept
{
  std::va_list ap;
  va_start(ap, fmt);
  int r = appendf(fmt, ap);
  va_end(ap);
  return r;
}

template <std::size_t Extent, bool Owning>
inline int cstrbuf<Extent, Owning>::appendf(const char fmt[],
                                            std::va_list ap) noexcept
{
  int r;
  if (!is_truncated())
  {
    const std::size_t space_left = extent() - m_next_pos;
    r = std::vsnprintf(next(), space_left, fmt, ap);
  }
  else
  {
    r = std::vsnprintf(nullptr, 0, fmt, ap);
  }
  if (r < 0)
  {
    return r;
  }
  m_next_pos += r;
  return (is_truncated() ? 1 : 0);
}

template <std::size_t Extent, bool Owning>
template <std::size_t N>
inline constexpr int cstrbuf<Extent, Owning>::replace_end_if_truncated(
    const char (&truncated_mark)[N]) noexcept
{
  /*
   * N < Extent is intended since char[N] do not need to be null terminated.
   * This strict check will have the side effect that one can not pass a
   * truncation mark as string literal that would fill whole buffer. If one want
   * to do that one will need to declare a char array and initialize that without
   * null termination.
   */
  static_assert(Extent == ndb::dynamic_extent || N < Extent);

  if (!is_truncated()) return 0;

  const char *mark_end = std::find(truncated_mark, truncated_mark + N, '\0');
  std::string_view mark(truncated_mark, mark_end - truncated_mark);

  return replace_end_if_truncated(mark);
}

template <std::size_t Extent, bool Owning>
inline constexpr int cstrbuf<Extent, Owning>::replace_end_if_truncated(
    std::string_view truncated_mark) noexcept
{
  if (!is_truncated()) return 0;

  // make room for truncation mark, and clear truncation state
  if (extent() > truncated_mark.length())
    m_next_pos = extent() - truncated_mark.length() - 1;
  else
    m_next_pos = 0;

  append(truncated_mark);

  return 1;  // was truncated
}

template <std::size_t Extent>
inline int cstrbuf_copy(ndb::span<char, Extent> buf,
                        const std::string_view other) noexcept
{
  cstrbuf strbuf(buf);
  return strbuf.append(other);
}

template <std::size_t Extent>
[[nodiscard]] inline int cstrbuf_copy(char (&buf)[Extent],
                                      const std::string_view other) noexcept
{
  return cstrbuf_copy(ndb::span<char, Extent>(buf), other);
}

template <std::size_t Extent>
inline int cstrbuf_format(ndb::span<char, Extent> buf,
                          const char fmt[],
                          ...) noexcept
{
  std::va_list ap;
  va_start(ap, fmt);
  int r = cstrbuf_format(buf, fmt, ap);
  va_end(ap);
  return r;
}

template <std::size_t Extent>
inline int cstrbuf_format(char (&buf)[Extent], const char fmt[], ...) noexcept
{
  std::va_list ap;
  va_start(ap, fmt);
  int r = cstrbuf_format(buf, fmt, ap);
  va_end(ap);
  return r;
}

template <std::size_t Extent>
inline int cstrbuf_format(ndb::span<char, Extent> buf,
                          const char fmt[],
                          std::va_list ap) noexcept
{
  cstrbuf strbuf(buf);
  return strbuf.appendf(fmt, ap);
}

template <std::size_t Extent>
inline int cstrbuf_format(char (&buf)[Extent],
                          const char fmt[],
                          std::va_list ap) noexcept
{
  return cstrbuf_format(ndb::span<char, Extent>(buf), fmt, ap);
}

#endif
