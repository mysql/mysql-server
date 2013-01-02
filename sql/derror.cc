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
  const char **errmsgs, **ptr;
  DBUG_ENTER("init_errmessage");

  /*
    Get a pointer to the old error messages pointer array.
    read_texts() tries to free it.
  */
  errmsgs= my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST);

  /* Read messages from file. */
  if (read_texts(ERRMSG_FILE, my_default_lc_messages->errmsgs->language,
                 &errmsgs, ER_ERROR_LAST - ER_ERROR_FIRST + 1) &&
      !errmsgs)
  {
    if (!(errmsgs= (const char**) my_malloc((ER_ERROR_LAST-ER_ERROR_FIRST+1)*
                                            sizeof(char*), MYF(0))))
      DBUG_RETURN(TRUE);
    for (ptr= errmsgs; ptr < errmsgs + ER_ERROR_LAST - ER_ERROR_FIRST; ptr++)
	  *ptr= "";
  }

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

  If we can't read messagefile then it's panic- we can't continue.
*/

bool read_texts(const char *file_name, const char *language,
                const char ***point, uint error_messages)
{
  register uint i;
  uint count,funktpos,textcount;
  size_t length;
  File file;
  char name[FN_REFLEN];
  char lang_path[FN_REFLEN];
  uchar *buff;
  uchar head[32],*pos;
  DBUG_ENTER("read_texts");

  LINT_INIT(buff);
  funktpos=0;
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
                               fn_format(name, file_name, lc_messages_dir, "", 4),
                               O_RDONLY | O_SHARE | O_BINARY,
                               MYF(0))) < 0)
      goto err;

    sql_print_warning("Using pre 5.5 semantics to load error messages from %s.",
                      lc_messages_dir);
    
    sql_print_warning("If this is not intended, refer to the documentation for "
                      "valid usage of --lc-messages-dir and --language "
                      "parameters.");
  }

  funktpos=1;
  if (mysql_file_read(file, (uchar*) head, 32, MYF(MY_NABP)))
    goto err;
  if (head[0] != (uchar) 254 || head[1] != (uchar) 254 ||
      head[2] != 3 || head[3] != 1)
    goto err; /* purecov: inspected */
  textcount=head[4];

  error_message_charset_info= system_charset_info;
  length=uint4korr(head+6); count=uint4korr(head+10);

  if (count < error_messages)
  {
    sql_print_error("\
Error message file '%s' had only %d error messages,\n\
but it should contain at least %d error messages.\n\
Check that the above file is the right version for this program!",
		    name,count,error_messages);
    (void) mysql_file_close(file, MYF(MY_WME));
    DBUG_RETURN(1);
  }

  /* Free old language */
  my_free(*point);
  if (!(*point= (const char**)
	my_malloc((size_t) (length+count*sizeof(char*)),MYF(0))))
  {
    funktpos=2;					/* purecov: inspected */
    goto err;					/* purecov: inspected */
  }
  buff= (uchar*) (*point + count);

  if (mysql_file_read(file, buff, (size_t) count*4, MYF(MY_NABP)))
    goto err;
  for (i=0, pos= buff ; i< count ; i++)
  {
    (*point)[i]= (char*) buff+uint4korr(pos);
    pos+=4;
  }
  if (mysql_file_read(file, buff, length, MYF(MY_NABP)))
    goto err;

  for (i=1 ; i < textcount ; i++)
  {
    point[i]= *point +uint2korr(head+10+i+i);
  }
  (void) mysql_file_close(file, MYF(0));
  DBUG_RETURN(0);

err:
  sql_print_error((funktpos == 2) ? "Not enough memory for messagefile '%s'" :
                  ((funktpos == 1) ? "Can't read from messagefile '%s'" :
                   "Can't find messagefile '%s'"), name);
  if (file != FERR)
    (void) mysql_file_close(file, MYF(MY_WME));
  DBUG_RETURN(1);
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
