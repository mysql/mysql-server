/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  Read language depeneded messagefile
*/

#include "sql_priv.h"
#include "unireg.h"
#include "derror.h"
#include "mysys_err.h"
#include "mysqld.h"                             // lc_messages_dir
#include "derror.h"                             // read_texts
#include "sql_class.h"                          // THD

static void init_myfunc_errs(void);


C_MODE_START
static const char **get_server_errmsgs()
{
  if (!current_thd)
    return DEFAULT_ERRMSGS;
  return CURRENT_THD_ERRMSGS;
}
C_MODE_END

/**
  Read messages from errorfile.

  This function can be called multiple times to reload the messages.
  If it fails to load the messages, it will fail softly by initializing
  the errmesg pointer to an array of empty strings or by keeping the
  old array if it exists.

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/

bool init_errmessage(void)
{
  const char **errmsgs;
  DBUG_ENTER("init_errmessage");

  /*
    Get a pointer to the old error messages pointer array.
    read_texts() tries to free it.
  */
  errmsgs= my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST);

  /* Read messages from file. */
  read_texts(ERRMSG_FILE, my_default_lc_messages->errmsgs->language,
             errmsgs, ER_ERROR_LAST - ER_ERROR_FIRST + 1);
  if (!errmsgs)
    DBUG_RETURN(TRUE); /* Fatal error, not able to allocate memory. */

  /* Register messages for use with my_error(). */
  if (my_error_register(get_server_errmsgs, ER_ERROR_FIRST, ER_ERROR_LAST))
  {
    my_free(errmsgs);
    DBUG_RETURN(TRUE);
  }

  DEFAULT_ERRMSGS= errmsgs;             /* Init global variable */
  init_myfunc_errs();			/* Init myfunc messages */
  DBUG_RETURN(FALSE);
}


/**
  Read text from packed textfile in language-directory.

  @param  file_name      Name of packed text file.
  @param  language       Language of error text file.
  @param  errmsgs        Will contain all the error text messages.
                         [This is a OUT argument]
                         This buffer contains 2 sections.
                         Section1: Offsets to error message text.
                         Section2: Holds all error messages.

  @param  error_messages Total number of error messages.

  @retval false          On success
  @retval true           On failure

  @note If we can't read messagefile then it's panic- we can't continue.
*/

bool read_texts(const char *file_name, const char *language,
                const char **&errmsgs, uint error_messages)
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
  DBUG_ENTER("read_texts");

  convert_dirname(lang_path, language, NullS);
  (void) my_load_path(lang_path, lang_path, lc_messages_dir);
  if ((file= mysql_file_open(key_file_ERRMSG,
                             fn_format(name, file_name, lang_path, "", 4),
                             O_RDONLY | O_SHARE | O_BINARY,
                             MYF(0))) < 0)
  {
    /*
      Trying pre-5.4 sematics of the --language parameter.
      It included the language-specific part, e.g.:

      --language=/path/to/english/
    */
    if ((file= mysql_file_open(key_file_ERRMSG,
                               fn_format(name, file_name,
                                         lc_messages_dir, "", 4),
                               O_RDONLY | O_SHARE | O_BINARY,
                               MYF(0))) < 0)
    {
      sql_print_error("Can't find messagefile '%s'", name);
      goto open_err;
    }
    sql_print_error("An old style --language value with language \
                    specific part detected: %s", lc_messages_dir);
    sql_print_error("Use --lc-messages-dir without language \
                    specific part instead.");
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
	my_malloc((size_t) (length+no_of_errmsgs*sizeof(char*)), MYF(0))))
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
  for (uint i= 0; i <= ER_ERROR_LAST - ER_ERROR_FIRST; ++i)
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
    if ((errmsgs= (const char**) my_malloc((ER_ERROR_LAST-ER_ERROR_FIRST+1)*
                                            sizeof(char*), MYF(0))))
    {
      for (uint i= 0; i <= ER_ERROR_LAST - ER_ERROR_FIRST; ++i)
        errmsgs[i]= "";
    }
  }
  DBUG_RETURN(true);
} /* read_texts */


/**
  Initiates error-messages used by my_func-library.
*/

static void init_myfunc_errs()
{
  init_glob_errs();			/* Initiate english errors */
  if (!(specialflag & SPECIAL_ENGLISH))
  {
    EE(EE_FILENOTFOUND)   = ER(ER_FILE_NOT_FOUND);
    EE(EE_CANTCREATEFILE) = ER(ER_CANT_CREATE_FILE);
    EE(EE_READ)           = ER(ER_ERROR_ON_READ);
    EE(EE_WRITE)          = ER(ER_ERROR_ON_WRITE);
    EE(EE_BADCLOSE)       = ER(ER_ERROR_ON_CLOSE);
    EE(EE_OUTOFMEMORY)    = ER(ER_OUTOFMEMORY);
    EE(EE_DELETE)         = ER(ER_CANT_DELETE_FILE);
    EE(EE_LINK)           = ER(ER_ERROR_ON_RENAME);
    EE(EE_EOFERR)         = ER(ER_UNEXPECTED_EOF);
    EE(EE_CANTLOCK)       = ER(ER_CANT_LOCK);
    EE(EE_DIR)            = ER(ER_CANT_READ_DIR);
    EE(EE_STAT)           = ER(ER_CANT_GET_STAT);
    EE(EE_GETWD)          = ER(ER_CANT_GET_WD);
    EE(EE_SETWD)          = ER(ER_CANT_SET_WD);
    EE(EE_DISK_FULL)      = ER(ER_DISK_FULL);
  }
}
