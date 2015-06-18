#ifndef GSTREAM_INCLUDED
#define GSTREAM_INCLUDED

/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include "my_global.h"                          /* NULL, NullS */
#include "my_sys.h"                             /* MY_ALLOW_ZERO_PTR */
#include "m_ctype.h"           /* my_charset_latin1, my_charset_bin */

typedef struct charset_info_st CHARSET_INFO;
typedef struct st_mysql_lex_string LEX_STRING;

class Gis_read_stream
{
public:
  enum enum_tok_types
  {
    unknown,
    eostream,
    word,
    numeric,
    l_bra,
    r_bra,
    comma
  };

  Gis_read_stream(const CHARSET_INFO *charset, const char *buffer, int size)
    :m_cur(buffer), m_limit(buffer + size), m_err_msg(NULL), m_charset(charset)
  {}
  Gis_read_stream(): m_cur(NullS), m_limit(NullS), m_err_msg(NullS)
  {}
  ~Gis_read_stream()
  {
    my_free(m_err_msg);
  }

  enum enum_tok_types get_next_toc_type();
  bool get_next_word(LEX_STRING *);
  bool get_next_number(double *);
  bool check_next_symbol(char);

  bool is_end_of_stream()
  {
    return get_next_toc_type() == eostream;
  }

  inline void skip_space()
  {
    while ((m_cur < m_limit) && my_isspace(&my_charset_latin1, *m_cur))
      m_cur++;
  }
  /* Skip next character, if match. Return 1 if no match */
  inline bool skip_char(char skip)
  {
    skip_space();
    if ((m_cur >= m_limit) || *m_cur != skip)
      return 1;					/* Didn't find char */
    m_cur++;
    return 0;
  }
  void set_error_msg(const char *msg);

  // caller should free this pointer
  char *get_error_msg()
  {
    char *err_msg = m_err_msg;
    m_err_msg= NullS;
    return err_msg;
  }

protected:
  const char *m_cur;
  const char *m_limit;
  char *m_err_msg;
  const CHARSET_INFO *m_charset;
};

#endif /* GSTREAM_INCLUDED */
