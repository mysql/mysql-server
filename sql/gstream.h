/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


class GTextReadStream
{
public:
  enum TokTypes
  {
    unknown,
    eostream,
    word,
    numeric,
    l_bra,
    r_bra,
    comma
  };

  GTextReadStream(const char *buffer, int size)
    :m_cur(buffer), m_limit(buffer + size), m_last_text_position(buffer),
    m_err_msg(NULL)
  {}
  GTextReadStream(): m_cur(NULL), m_limit(NULL), m_err_msg(NULL)
  {}

  ~GTextReadStream()
  {
    my_free(m_err_msg, MYF(MY_ALLOW_ZERO_PTR));
  }

  int get_next_toc_type() const;
  const char *get_next_word(int *word_len);
  int get_next_number(double *d);
  char get_next_symbol();

  const char *get_last_text_position() const
  {
    return m_last_text_position;
  }

  void set_error_msg(const char *msg);

  // caller should free this pointer
  char *get_error_msg()
  {
    char *err_msg = m_err_msg;
    m_err_msg = NULL;
    return err_msg;
  }

protected:
  const char *m_cur;
  const char *m_limit;
  const char *m_last_text_position;
  char *m_err_msg;
};
