#include "mysql_priv.h"

int GTextReadStream::get_next_toc_type() const
{
  const char *cur = m_cur;
  while ((*cur)&&(strchr(" \t\r\n",*cur)))
  {
    cur++;
  }
  if (!(*cur))
  {
    return eostream;
  }

  if (((*cur>='a') && (*cur<='z')) || ((*cur>='A') && (*cur<='Z')) ||
      (*cur=='_'))
  {
    return word;
  }

  if (((*cur>='0') && (*cur<='9')) || (*cur=='-') || (*cur=='+') ||
      (*cur=='.'))
  {
    return numeric;
  }

  if (*cur == '(')
  {
    return l_bra;
  }
  
  if (*cur == ')')
  {
    return r_bra;
  }

  if (*cur == ',')
  {
    return comma;
  }

  return unknown;
}

const char *GTextReadStream::get_next_word(int *word_len)
{
  const char *cur = m_cur;
  while ((*cur)&&(strchr(" \t\r\n",*cur)))
  {
    cur++;
  }
  m_last_text_position = cur;

  if (!(*cur))
  {
    return 0;
  }

  const char *wd_start = cur;

  if (((*cur<'a') || (*cur>'z')) && ((*cur<'A') || (*cur>'Z')) && (*cur!='_'))
  {
    return NULL;
  }

  ++cur;

  while (((*cur>='a') && (*cur<='z')) || ((*cur>='A') && (*cur<='Z')) ||
	 (*cur=='_') || ((*cur>='0') && (*cur<='9')))
  {
    ++cur;
  }

  *word_len = cur - wd_start;

  m_cur = cur;

  return wd_start;
}

int GTextReadStream::get_next_number(double *d)
{
  const char *cur = m_cur;
  while ((*cur)&&(strchr(" \t\r\n",*cur)))
  {
    cur++;
  }

  m_last_text_position = cur;
  if (!(*cur))
  {
    set_error_msg("Numeric constant expected");
    return 1;
  }

  if (((*cur<'0') || (*cur>'9')) && (*cur!='-') && (*cur!='+') && (*cur!='.'))
  {
    set_error_msg("Numeric constant expected");
    return 1;
  }

  char *endptr;

  *d = strtod(cur, &endptr);

  if (endptr)
  {
    m_cur = endptr;
  }

  return 0;
}

char GTextReadStream::get_next_symbol()
{
  const char *cur = m_cur;
  while ((*cur)&&(strchr(" \t\r\n",*cur)))
  {
    cur++;
  }
  if (!(*cur))
  {
    return 0;
  }

  m_cur = cur + 1;
  m_last_text_position = cur;

  return *cur;
}

void GTextReadStream::set_error_msg(const char *msg)
{
  size_t len = strlen(msg);
  m_err_msg = (char *)my_realloc(m_err_msg, len + 1, MYF(MY_ALLOW_ZERO_PTR));
  memcpy(m_err_msg, msg, len + 1);
}


