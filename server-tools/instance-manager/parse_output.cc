/*
   Copyright (c) 2005-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "parse_output.h"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

#include <stdio.h>

#include "parse.h"
#include "portability.h"

/**************************************************************************
  Private module implementation.
**************************************************************************/

namespace { /* no-indent */

/*************************************************************************/

void trim_space(const char **text, uint *word_len)
{
  const char *start= *text;
  while (*start != 0 && *start == ' ')
    start++;
  *text= start;

  int len= strlen(start);
  const char *end= start + len - 1;
  while (end > start && my_isspace(&my_charset_latin1, *end))
    end--;
  *word_len= (end - start)+1;
}

/*************************************************************************/

/**
  @brief A facade to the internal workings of optaining the output from an
  executed system process.
*/

class Mysqld_output_parser
{
public:
  Mysqld_output_parser()
  { }

  virtual ~Mysqld_output_parser()
  { }

public:
  bool parse(const char *command,
             const char *option_name_str,
             uint option_name_length,
             char *option_value_buf,
             size_t option_value_buf_size,
             enum_option_type option_type);

protected:
  /**
    @brief Run a process and attach stdout- and stdin-pipes to it.

    @param command The path to the process to be executed

    @return Error status.
      @retval TRUE An error occurred
      @retval FALSE Operation was a success
  */

  virtual bool run_command(const char *command)= 0;


  /**
    @brief Read a sequence of bytes from the executed process' stdout pipe.

    The sequence is terminated by either '\0', LF or CRLF tokens. The
    terminating token is excluded from the result.

    @param line_buffer A pointer to a character buffer
    @param line_buffer_size The size of the buffer in bytes

    @return Error status.
      @retval TRUE An error occured
      @retval FALSE Operation was a success
  */

  virtual bool read_line(char *line_buffer,
                         uint line_buffer_size)= 0;


  /**
    @brief Release any resources needed after a execution and parsing.
  */

  virtual bool cleanup()= 0;
};

/*************************************************************************/

bool Mysqld_output_parser::parse(const char *command,
                                 const char *option_name_str,
                                 uint option_name_length,
                                 char *option_value_buf,
                                 size_t option_value_buf_size,
                                 enum_option_type option_type)
{
  /* should be enough to store the string from the output */
  const int LINE_BUFFER_SIZE= 512;
  char line_buffer[LINE_BUFFER_SIZE];

  if (run_command(command))
    return TRUE;

  while (true)
  {
    if (read_line(line_buffer, LINE_BUFFER_SIZE))
    {
      cleanup();
      return TRUE;
    }

    uint found_word_len= 0;
    char *linep= line_buffer;

    line_buffer[sizeof(line_buffer) - 1]= '\0';        /* safety */

    /* Find the word(s) we are looking for in the line. */

    linep= strstr(linep, option_name_str);

    if (!linep)
      continue;

    linep+= option_name_length;

    switch (option_type)
    {
    case GET_VALUE:
      trim_space((const char**) &linep, &found_word_len);

      if (option_value_buf_size <= found_word_len)
      {
        cleanup();
        return TRUE;
      }

      strmake(option_value_buf, linep, found_word_len);

      break;

    case GET_LINE:
      strmake(option_value_buf, linep, option_value_buf_size - 1);

      break;
    }

    cleanup();

    return FALSE;
  }
}

/**************************************************************************
  Platform-specific implementation: UNIX.
**************************************************************************/

#ifndef __WIN__

class Mysqld_output_parser_unix : public Mysqld_output_parser
{
public:
  Mysqld_output_parser_unix() :
    m_stdout(NULL)
  { }

protected:
  virtual bool run_command(const char *command);

  virtual bool read_line(char *line_buffer,
                         uint line_buffer_size);

  virtual bool cleanup();

private:
  FILE *m_stdout;
};

bool Mysqld_output_parser_unix::run_command(const char *command)
{
  if (!(m_stdout= popen(command, "r")))
    return TRUE;

  /*
    We want fully buffered stream. We also want system to allocate
    appropriate buffer.
  */

  setvbuf(m_stdout, NULL, _IOFBF, 0);

  return FALSE;
}

bool Mysqld_output_parser_unix::read_line(char *line_buffer,
                                          uint line_buffer_size)
{
  char *retbuff = fgets(line_buffer, line_buffer_size, m_stdout);
  /* Remove any tailing new line charaters */
  if (line_buffer[line_buffer_size-1] == LF)
   line_buffer[line_buffer_size-1]= '\0';
  return (retbuff == NULL);
}

bool Mysqld_output_parser_unix::cleanup()
{
  if (m_stdout)
    pclose(m_stdout);

  return FALSE;
}

#else /* Windows */

/**************************************************************************
  Platform-specific implementation: Windows.
**************************************************************************/

class Mysqld_output_parser_win : public Mysqld_output_parser
{
public:
  Mysqld_output_parser_win() :
      m_internal_buffer(NULL),
      m_internal_buffer_offset(0),
      m_internal_buffer_size(0)
  { }

protected:
  virtual bool run_command(const char *command);
  virtual bool read_line(char *line_buffer,
                         uint line_buffer_size);
  virtual bool cleanup();

private:
  HANDLE m_h_child_stdout_wr;
  HANDLE m_h_child_stdout_rd;
  uint m_internal_buffer_offset;
  uint m_internal_buffer_size;
  char *m_internal_buffer;
};

bool Mysqld_output_parser_win::run_command(const char *command)
{
  BOOL op_status;

  SECURITY_ATTRIBUTES sa_attr;
  sa_attr.nLength= sizeof(SECURITY_ATTRIBUTES);
  sa_attr.bInheritHandle= TRUE;
  sa_attr.lpSecurityDescriptor= NULL;

  op_status= CreatePipe(&m_h_child_stdout_rd,
                        &m_h_child_stdout_wr,
                        &sa_attr,
                        0 /* Use system-default buffer size. */);

  if (!op_status)
    return TRUE;

  SetHandleInformation(m_h_child_stdout_rd, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFO si_start_info;
  ZeroMemory(&si_start_info, sizeof(STARTUPINFO));
  si_start_info.cb= sizeof(STARTUPINFO);
  si_start_info.hStdError= m_h_child_stdout_wr;
  si_start_info.hStdOutput= m_h_child_stdout_wr;
  si_start_info.dwFlags|= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION pi_proc_info;

  op_status= CreateProcess(NULL,           /* Application name. */
                           (char*)command, /* Command line. */
                           NULL,           /* Process security attributes. */
                           NULL,           /* Primary thread security attr.*/
                           TRUE,           /* Handles are inherited. */
                           0,              /* Creation flags. */
                           NULL,           /* Use parent's environment. */
                           NULL,           /* Use parent's curr. directory. */
                           &si_start_info, /* STARTUPINFO pointer. */
                           &pi_proc_info); /* Rec. PROCESS_INFORMATION. */

  if (!op_status)
  {
    CloseHandle(m_h_child_stdout_rd);
    CloseHandle(m_h_child_stdout_wr);

    return TRUE;
  }

  /* Close unnessary handles. */

  CloseHandle(pi_proc_info.hProcess);
  CloseHandle(pi_proc_info.hThread);

  return FALSE;
}

bool Mysqld_output_parser_win::read_line(char *line_buffer,
                                         uint line_buffer_size)
{
  DWORD dw_read_count= m_internal_buffer_size;
  bzero(line_buffer,line_buffer_size);
  char *buff_ptr= line_buffer;
  char ch;

  while ((unsigned)(buff_ptr - line_buffer) < line_buffer_size)
  {
    do
    {
      ReadFile(m_h_child_stdout_rd, &ch,
               1, &dw_read_count, NULL);
    } while ((ch == CR || ch == LF) && buff_ptr == line_buffer);

    if (dw_read_count == 0)
      return TRUE;

    if (ch == CR || ch == LF)
      break;

    *buff_ptr++ = ch;
  }

  return FALSE;
}

bool Mysqld_output_parser_win::cleanup()
{
  /* Close all handles. */

  CloseHandle(m_h_child_stdout_wr);
  CloseHandle(m_h_child_stdout_rd);

  return FALSE;
}
#endif

/*************************************************************************/

} /* End of private module implementation. */

/*************************************************************************/

/**
  @brief Parse output of the given command

  @param      command               The command to execute.
  @param      option_name_str       Option name.
  @param      option_name_length    Length of the option name.
  @param[out] option_value_buf      The buffer to store option value.
  @param      option_value_buf_size Size of the option value buffer.
  @param      option_type           Type of the option:
                                      - GET_LINE if we want to get all the
                                        line after the option name;
                                      - GET_VALUE otherwise.

  Execute the process by running "command". Find the "option name" and
  return the next word if "option_type" is GET_VALUE. Return the rest of
  the parsed string otherwise.

  @note This function has a separate windows implementation.

  @return The error status.
    @retval FALSE Ok, the option name has been found.
    @retval TRUE Error occured or the option name is not found.
*/

bool parse_output_and_get_value(const char *command,
                                const char *option_name_str,
                                uint option_name_length,
                                char *option_value_buf,
                                size_t option_value_buf_size,
                                enum_option_type option_type)
{
#ifndef __WIN__
  Mysqld_output_parser_unix parser;
#else /* __WIN__ */
  Mysqld_output_parser_win parser;
#endif

  return parser.parse(command,
                      option_name_str,
                      option_name_length,
                      option_value_buf,
                      option_value_buf_size,
                      option_type);
}
