/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mysql_json_encode_imp.h"
#include "my_inttypes.h"
#include "mysql/strings/m_ctype.h"

DEFINE_METHOD(const unsigned char *, mysql_json_encode_imp::encode,
              (const unsigned char *src, const unsigned char *src_end,
               const unsigned char *src_data_end, unsigned char *dst,
               unsigned char *dst_end, const CHARSET_INFO_h charset,
               unsigned char **dst_out)) {
  const CHARSET_INFO *ch = reinterpret_cast<const CHARSET_INFO *>(charset);

  my_charset_conv_mb_wc mb_wc = ch->cset->mb_wc;
  my_charset_conv_wc_mb wc_mb = my_charset_utf8mb4_general_ci.cset->wc_mb;

  /*
    Decrease 5 bytes, so we do not have to perform checks, whether we have
    enough data in the dst buffer. 5 bytes, because the longest value that
    can be written into the buffer is "\uXXXX".
   */
  dst_end -= 5;

  while (src < src_end && dst < dst_end) {
    my_wc_t wc;
    int res;

    if ((res = (*mb_wc)(ch, &wc, src, src_data_end)) <= 0) {
      /*
        Input character could not be successfully consumed.
      */

      /*
        MY_CS_TOOSMALL value should never happen, but include it into the
        condition below, so the code will not fall into the undefined behavior.
      */
      assert(res != MY_CS_TOOSMALL);

      if (res == MY_CS_ILSEQ) {
        /*
          Character could not be decoded. Consume just one byte.
        */
        src += 1;
      } else if (res <= MY_CS_TOOSMALL) {
        /*
          One or more bytes are missing in the input buffer. End encoding.
        */
        src = src_data_end;
      } else {
        /*
          Bytes consumed, but character not decoded.
        */
        src += (-res);
      }
      /*
        Append question mark to the output buffer.
      */
      *dst++ = '?';

      /*
        Continue with the next byte.
      */
      continue;
    }

    switch (wc) {
      case '"':
      case '\\':
      case '/':
        *dst++ = '\\';
        *dst++ = static_cast<unsigned char>(wc);
        break;
      case '\b':
        *dst++ = '\\';
        *dst++ = 'b';
        break;
      case '\f':
        *dst++ = '\\';
        *dst++ = 'f';
        break;
      case '\n':
        *dst++ = '\\';
        *dst++ = 'n';
        break;
      case '\r':
        *dst++ = '\\';
        *dst++ = 'r';
        break;
      case '\t':
        *dst++ = '\\';
        *dst++ = 't';
        break;
      default:
        if (wc < 0x20) {
          /*
            Control characters must be escaped in JSON.
          */
          int r = sprintf(reinterpret_cast<char *>(dst), "\\u%04x",
                          static_cast<unsigned char>(wc));
          dst += r;
        } else if (wc < 0x80) {
          *dst++ = static_cast<unsigned char>(wc);
        } else {
          int out_res;

          if ((out_res = (*wc_mb)(&my_charset_utf8mb4_general_ci, wc, dst,
                                  dst + 4)) > 0) {
            dst += out_res;
          } else {
            *dst++ = '?';
          }
        }
    }

    src += res;
  }

  *dst_out = dst;

  assert(src <= src_data_end);
  return src;
}
