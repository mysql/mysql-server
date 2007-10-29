/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Functions to read and parse geometrical data.
  NOTE: These functions assumes that the string is end \0 terminated!
*/

#include "mysql_priv.h"

enum Gis_read_stream::enum_tok_types Gis_read_stream::get_next_toc_type()
{
  skip_space();
  if (m_cur >= m_limit)
    return eostream;
  if (my_isvar_start(&my_charset_bin, *m_cur))
    return word;
  if ((*m_cur >= '0' && *m_cur <= '9') || *m_cur == '-' || *m_cur == '+')
    return numeric;
  if (*m_cur == '(')
    return l_bra;
  if (*m_cur == ')')
    return r_bra;
  if (*m_cur == ',')
    return comma;
  return unknown;
}


bool Gis_read_stream::get_next_word(LEX_STRING *res)
{
  skip_space();
  res->str= (char*) m_cur;
  /* The following will also test for \0 */
  if ((m_cur >= m_limit) || !my_isvar_start(&my_charset_bin, *m_cur))
    return 1;

  /*
    We can't combine the following increment with my_isvar() because
    my_isvar() is a macro that would cause side effects
  */
  m_cur++;
  while ((m_cur < m_limit) && my_isvar(&my_charset_bin, *m_cur))
    m_cur++;

  res->length= (uint32) (m_cur - res->str);
  return 0;
}


/*
  Read a floating point number

  NOTE: Number must start with a digit or sign. It can't start with a decimal
  point
*/

bool Gis_read_stream::get_next_number(double *d)
{
  char *endptr;
  int err;

  skip_space();

  if ((m_cur >= m_limit) ||
      (*m_cur < '0' || *m_cur > '9') && *m_cur != '-' && *m_cur != '+')
  {
    set_error_msg("Numeric constant expected");
    return 1;
  }

  *d = my_strntod(m_charset, (char *)m_cur,
		  (uint) (m_limit-m_cur), &endptr, &err);
  if (err)
    return 1;
  if (endptr)
    m_cur = endptr;
  return 0;
}


bool Gis_read_stream::check_next_symbol(char symbol)
{
  skip_space();
  if ((m_cur >= m_limit) || (*m_cur != symbol))
  {
    char buff[32];
    strmov(buff, "'?' expected");
    buff[2]= symbol;
    set_error_msg(buff);
    return 1;
  }
  m_cur++;
  return 0;
}


/*
  Remember error message.
*/

void Gis_read_stream::set_error_msg(const char *msg)
{
  size_t len= strlen(msg);			// ok in this context
  m_err_msg= (char *) my_realloc(m_err_msg, (uint) len + 1, MYF(MY_ALLOW_ZERO_PTR));
  memcpy(m_err_msg, msg, len + 1);
}
