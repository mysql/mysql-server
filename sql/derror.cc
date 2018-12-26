/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "derror.h"
#include "mysys_err.h"
#include "mysqld.h"                             // lc_messages_dir
#include "sql_class.h"                          // THD
#include "log.h"

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

static const char *ERRMSG_FILE = "errmsg.sys";
static const int NUM_SECTIONS=
  sizeof(errmsg_section_start) / sizeof(errmsg_section_start[0]);

/*
  Error messages are stored sequentially in an array.
  But logically error messages are organized in sections where
  each section contains errors that are consecutively numbered.
  This function maps from a "logical" mysql_errno to an array
  index and returns the string.
*/
const char* MY_LOCALE_ERRMSGS::lookup(int mysql_errno)
{
  int offset= 0; // Position where the current section starts in the array.
  for (int i= 0; i < NUM_SECTIONS; i++)
  {
    if (mysql_errno >= errmsg_section_start[i] &&
        mysql_errno < (errmsg_section_start[i] + errmsg_section_size[i]))
      return errmsgs[mysql_errno - errmsg_section_start[i] + offset];
    offset+= errmsg_section_size[i];
  }
  return "Invalid error code";
}


const char* ER_DEFAULT(int mysql_errno)
{
  return my_default_lc_messages->errmsgs->lookup(mysql_errno);
}


const char* ER_THD(const THD *thd, int mysql_errno)
{
  return thd->variables.lc_messages->errmsgs->lookup(mysql_errno);
}


C_MODE_START
static const char *get_server_errmsgs(int mysql_errno)
{
  if (current_thd)
    return ER_THD(current_thd, mysql_errno);
  return ER_DEFAULT(mysql_errno);
}
C_MODE_END


bool init_errmessage()
{
  DBUG_ENTER("init_errmessage");

  /* Read messages from file. */
  (void)my_default_lc_messages->errmsgs->read_texts();

  if (!my_default_lc_messages->errmsgs->is_loaded())
    DBUG_RETURN(true); /* Fatal error, not able to allocate memory. */

  /* Register messages for use with my_error(). */
  for (int i= 0; i < NUM_SECTIONS; i++)
  {
    if (my_error_register(get_server_errmsgs,
                          errmsg_section_start[i],
                          errmsg_section_start[i] +
                          errmsg_section_size[i] - 1))
    {
      my_default_lc_messages->errmsgs->destroy();
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}


void deinit_errmessage()
{
  for (int i= 0; i < NUM_SECTIONS; i++)
  {
    my_error_unregister(errmsg_section_start[i],
                        errmsg_section_start[i] +
                        errmsg_section_size[i] - 1);
  }
}


/**
  Read text from packed textfile in language-directory.

  @retval false          On success
  @retval true           On failure

  @note If we can't read messagefile then it's panic- we can't continue.
*/

bool MY_LOCALE_ERRMSGS::read_texts()
{
  uint i;
  uint no_of_errmsgs;
  size_t length;
  File file;
  char name[FN_REFLEN];
  char lang_path[FN_REFLEN];
  uchar *start_of_errmsgs= NULL;
  uchar *pos= NULL;
  uchar head[32];
  uint error_messages= 0;
  DBUG_ENTER("read_texts");

  for (int i= 0; i < NUM_SECTIONS; i++)
    error_messages+= errmsg_section_size[i];

  convert_dirname(lang_path, language, NullS);
  (void) my_load_path(lang_path, lang_path, lc_messages_dir);
  if ((file= mysql_file_open(key_file_ERRMSG,
                             fn_format(name, ERRMSG_FILE, lang_path, "", 4),
                             O_RDONLY | O_SHARE | O_BINARY,
                             MYF(0))) < 0)
  {
    /*
      Trying pre-5.5 sematics of the --language parameter.
      It included the language-specific part, e.g.:

      --language=/path/to/english/
    */
    if ((file= mysql_file_open(key_file_ERRMSG,
                               fn_format(name, ERRMSG_FILE,
                                         lc_messages_dir, "", 4),
                               O_RDONLY | O_SHARE | O_BINARY,
                               MYF(0))) < 0)
    {
      sql_print_error("Can't find error-message file '%s'. Check error-message"
                      " file location and 'lc-messages-dir' configuration"
                      " directive.", name);
      goto open_err;
    }

    sql_print_warning("Using pre 5.5 semantics to load error messages from %s.",
                      lc_messages_dir);

    sql_print_warning("If this is not intended, refer to the documentation for "
                      "valid usage of --lc-messages-dir and --language "
                      "parameters.");
  }

  // Read the header from the file
  if (mysql_file_read(file, (uchar*) head, 32, MYF(MY_NABP)))
    goto read_err;
  if (head[0] != (uchar) 254 || head[1] != (uchar) 254 ||
      head[2] != 3 || head[3] != 1 || head[4] != 1)
    goto read_err;

  error_message_charset_info= system_charset_info;
  length= uint4korr(head+6);
  no_of_errmsgs= uint4korr(head+10);

  if (no_of_errmsgs < error_messages)
  {
    sql_print_error("Error message file '%s' had only %d error messages,\n\
                    but it should contain at least %d error messages.\n\
                    Check that the above file is the right version for \
                    this program!",
		    name,no_of_errmsgs,error_messages);
    (void) mysql_file_close(file, MYF(MY_WME));
    goto open_err;
  }

  // Free old language and allocate for the new one
  my_free(errmsgs);
  if (!(errmsgs= (const char**)
	my_malloc(key_memory_errmsgs,
                  length+no_of_errmsgs*sizeof(char*), MYF(0))))
  {
    sql_print_error("Not enough memory for messagefile '%s'", name);
    (void) mysql_file_close(file, MYF(MY_WME));
    DBUG_RETURN(true);
  }

  // Get pointer to Section2.
  start_of_errmsgs= (uchar*) (errmsgs + no_of_errmsgs);

  /*
    Temporarily read message offsets into Section2.
    We cannot read these 4 byte offsets directly into Section1,
    as pointer size vary between processor architecture.
  */
  if (mysql_file_read(file, start_of_errmsgs, (size_t) no_of_errmsgs*4,
                      MYF(MY_NABP)))
    goto read_err_init;

  // Copy the message offsets to Section1.
  for (i= 0, pos= start_of_errmsgs; i< no_of_errmsgs; i++)
  {
    errmsgs[i]= (char*) start_of_errmsgs+uint4korr(pos);
    pos+= 4;
  }

  // Copy all the error text messages into Section2.
  if (mysql_file_read(file, start_of_errmsgs, length, MYF(MY_NABP)))
    goto read_err_init;

  (void) mysql_file_close(file, MYF(0));
  DBUG_RETURN(false);

read_err_init:
  for (uint i= 0; i < error_messages; ++i)
    errmsgs[i]= "";
read_err:
  sql_print_error("Can't read from messagefile '%s'", name);
  (void) mysql_file_close(file, MYF(MY_WME));
open_err:
  if (!errmsgs)
  {
    /*
      Allocate and initialize errmsgs to empty string in order to avoid access
      to errmsgs during another failure in abort operation
    */
    if ((errmsgs= (const char**) my_malloc(key_memory_errmsgs,
                                           error_messages *
                                           sizeof(char*), MYF(0))))
    {
      for (uint i= 0; i < error_messages; ++i)
        errmsgs[i]= "";
    }
  }
  DBUG_RETURN(true);
} /* read_texts */


void MY_LOCALE_ERRMSGS::destroy()
{
  my_free(errmsgs);
}
