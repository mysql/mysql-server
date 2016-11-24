#ifndef MB_WC_INCLUDED
#define MB_WC_INCLUDED

/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mb_wc.h

  Definitions of mb_wc (multibyte to wide character, ie., effectively
  “parse a UTF-8 character”) functions for UTF-8 (both three- and four-byte).
  These are available both as inline functions, as C-style thunks so that they
  can fit into MY_CHARSET_HANDLER, and as functors.

  The functors exist so that you can specialize a class on them and get them
  inlined instead of having to call them through the function pointer in
  MY_CHARSET_HANDLER; mb_wc is in itself so cheap (the most common case is
  just a single byte load and a predictable compare) that the call overhead
  in a tight loop is significant, and these routines tend to take up a lot
  of CPU time when sorting. Typically, at the outermost level, you'd simply
  compare cs->cset->mb_wc with my_mb_wc_{utf8,utf8mb4}_thunk, and if so,
  instantiate your function with the given class. If it doesn't match,
  you can use Mb_wc_through_function_pointer, which calls through the
  function pointer as usual. (It will cache the function pointer for you,
  which is typically faster than looking it up all the time -- the compiler
  cannot always figure out on its own that it doesn't change.)

  The Mb_wc_* classes should be sent by _value_, not by reference, since
  they are never larger than two pointers (and usually simply zero).
*/

#include "m_ctype.h"
#include "my_compiler.h"

#define IS_CONTINUATION_BYTE(c) (((c) ^ 0x80) < 0x40)

static int my_mb_wc_utf8(my_wc_t *pwc, const uchar *s, const uchar *e);
static int my_mb_wc_utf8mb4(my_wc_t *pwc, const uchar *s, const uchar *e);

/**
  Functor that converts a UTF-8 multibyte sequence (up to three bytes)
  to a wide character.
*/
struct Mb_wc_utf8
{
  Mb_wc_utf8() {}

  ALWAYS_INLINE
  int operator() (my_wc_t *pwc, const uchar *s, const uchar *e) const
  {
    return my_mb_wc_utf8(pwc, s, e);
  }
};

/**
  Functor that converts a UTF-8 multibyte sequence (up to four bytes)
  to a wide character.
*/
struct Mb_wc_utf8mb4
{
  Mb_wc_utf8mb4() {}

  ALWAYS_INLINE
  int operator() (my_wc_t *pwc, const uchar *s, const uchar *e) const
  {
    return my_mb_wc_utf8mb4(pwc, s, e);
  }
};

/**
  Functor that uses a function pointer to convert a multibyte sequence
  to a wide character.
*/
class Mb_wc_through_function_pointer
{
public:
  explicit Mb_wc_through_function_pointer(const CHARSET_INFO *cs)
    : m_funcptr(cs->cset->mb_wc), m_cs(cs)
  {}

  int operator() (my_wc_t *pwc, const uchar *s, const uchar *e) const
  {
    return m_funcptr(m_cs, pwc, s, e);
  }

private:
  typedef int (*mbwc_func_t)(const CHARSET_INFO *, my_wc_t *, const uchar *, const uchar *);

  const mbwc_func_t m_funcptr;
  const CHARSET_INFO * const m_cs;
};

/**
  Parses a single UTF-8 character from a byte string.

  @param[out] pwc the parsed character, if any
  @param s the string to read from
  @param e the end of the string; will not read past this

  @return the number of bytes read from s, or a value <= 0 for failure
    (see m_ctype.h)
*/
static ALWAYS_INLINE int my_mb_wc_utf8(my_wc_t *pwc, const uchar *s, const uchar *e)
{
  uchar c;

  if (s >= e)
    return MY_CS_TOOSMALL;

  c= s[0];
  if (c < 0x80)
  {
    *pwc = c;
    return 1;
  }
  else if (c < 0xc2)
    return MY_CS_ILSEQ;
  else if (c < 0xe0)
  {
    if (s+2 > e) /* We need 2 characters */
      return MY_CS_TOOSMALL2;

    if (!(IS_CONTINUATION_BYTE(s[1])))
      return MY_CS_ILSEQ;

    *pwc = ((my_wc_t) (c & 0x1f) << 6) | (my_wc_t) (s[1] ^ 0x80);
    return 2;
  }
  else if (c < 0xf0)
  {
    if (s+3 > e) /* We need 3 characters */
      return MY_CS_TOOSMALL3;

    if (!(IS_CONTINUATION_BYTE(s[1]) && IS_CONTINUATION_BYTE(s[2]) &&
          (c >= 0xe1 || s[1] >= 0xa0)))
      return MY_CS_ILSEQ;

    *pwc = ((my_wc_t) (c & 0x0f) << 12)   |
           ((my_wc_t) (s[1] ^ 0x80) << 6) |
            (my_wc_t) (s[2] ^ 0x80);

    return 3;
  }
  return MY_CS_ILSEQ;
}

/**
  Parses a single UTF-8 character from a byte string. The difference
  between this and my_mb_wc_utf8 is that this function also can handle
  four-byte UTF-8 characters.

  @param[out] pwc the parsed character, if any
  @param s the string to read from
  @param e the end of the string; will not read past this

  @return the number of bytes read from s, or a value <= 0 for failure
    (see m_ctype.h)
*/
static ALWAYS_INLINE int
my_mb_wc_utf8mb4(my_wc_t *pwc, const uchar *s, const uchar *e)
{
  uchar c;

  if (s >= e)
    return MY_CS_TOOSMALL;

  c= s[0];
  if (c < 0x80)
  {
    *pwc= c;
    return 1;
  }
  else if (c < 0xc2)
    return MY_CS_ILSEQ;
  else if (c < 0xe0)
  {
    if (s + 2 > e) /* We need 2 characters */
      return MY_CS_TOOSMALL2;

    if (!(IS_CONTINUATION_BYTE(s[1])))
      return MY_CS_ILSEQ;

    *pwc= ((my_wc_t) (c & 0x1f) << 6) | (my_wc_t) (s[1] ^ 0x80);
    return 2;
  }
  else if (c < 0xf0)
  {
    if (s + 3 > e) /* We need 3 characters */
      return MY_CS_TOOSMALL3;

    if (!(IS_CONTINUATION_BYTE(s[1]) && IS_CONTINUATION_BYTE(s[2]) &&
          (c >= 0xe1 || s[1] >= 0xa0)))
      return MY_CS_ILSEQ;

    *pwc= ((my_wc_t) (c & 0x0f) << 12)   |
          ((my_wc_t) (s[1] ^ 0x80) << 6) |
           (my_wc_t) (s[2] ^ 0x80);
    return 3;
  }
  else if (c < 0xf5)
  {
    if (s + 4 > e) /* We need 4 characters */
      return MY_CS_TOOSMALL4;

    /*
      UTF-8 quick four-byte mask:
      11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      Encoding allows to encode U+00010000..U+001FFFFF
      
      The maximum character defined in the Unicode standard is U+0010FFFF.
      Higher characters U+00110000..U+001FFFFF are not used.
      
      11110000.10010000.10xxxxxx.10xxxxxx == F0.90.80.80 == U+00010000 (min)
      11110100.10001111.10111111.10111111 == F4.8F.BF.BF == U+0010FFFF (max)
      
      Valid codes:
      [F0][90..BF][80..BF][80..BF]
      [F1][80..BF][80..BF][80..BF]
      [F2][80..BF][80..BF][80..BF]
      [F3][80..BF][80..BF][80..BF]
      [F4][80..8F][80..BF][80..BF]
    */

    if (!(IS_CONTINUATION_BYTE(s[1]) &&
          IS_CONTINUATION_BYTE(s[2]) &&
          IS_CONTINUATION_BYTE(s[3]) &&
          (c >= 0xf1 || s[1] >= 0x90) &&
          (c <= 0xf3 || s[1] <= 0x8F)))
      return MY_CS_ILSEQ;
    *pwc = ((my_wc_t) (c & 0x07) << 18)    |
           ((my_wc_t) (s[1] ^ 0x80) << 12) |
           ((my_wc_t) (s[2] ^ 0x80) << 6)  |
            (my_wc_t) (s[3] ^ 0x80);
    return 4;
  }
  return MY_CS_ILSEQ;
}

// Non-inlined versions of the above. These are used as function pointers
// in MY_CHARSET_HANDLER structs, and you can compare againt them to see
// if using the Mb_wc_utf8* functors would be appropriate.

extern "C" int
my_mb_wc_utf8_thunk(const CHARSET_INFO *cs,
                    my_wc_t *pwc, const uchar *s, const uchar *e);

extern "C" int
my_mb_wc_utf8mb4_thunk(const CHARSET_INFO *cs,
                       my_wc_t *pwc, const uchar *s, const uchar *e);

#endif  // MB_WC_INCLUDED
