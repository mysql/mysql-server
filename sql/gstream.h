#ifndef GSTREAM_H
#define GSTREAM_H

#ifdef WITHOUT_MYSQL
  #include ".\rtree\myisamdef.h"
#else
  #include "mysql_priv.h"
#endif

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
    comma,
  };
  GTextReadStream(const char *buffer, int size) : 
    m_cur(buffer), m_limit(buffer + size), m_last_text_position(buffer), m_err_msg(NULL) {}
  GTextReadStream() : m_cur(NULL), m_limit(NULL), m_err_msg(NULL) {}

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

#endif



