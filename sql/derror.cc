/* Copyright (C) 2000-2005 MySQL AB
   Copyright (C) 2011 Monty Program Ab

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


/**
  @file

  @brief
  Read language depeneded messagefile
*/

#include "mysql_priv.h"
#include "mysys_err.h"

static bool check_error_mesg(const char *file_name, const char **errmsg);
static bool read_texts(const char *file_name,const char ***point,
		       uint error_messages);
static void init_myfunc_errs(void);

/**
  Read messages from errorfile.

  This function can be called multiple times to reload the messages.

  If it fails to load the messages:
   - If we already have error messages loaded, keep the old ones and
     return FALSE(ok)
  - Initializing the errmesg pointer to an array of empty strings
    and return TRUE (error)

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/

bool init_errmessage(void)
{
  const char **errmsgs, **ptr, **org_errmsgs;
  bool error= FALSE;
  DBUG_ENTER("init_errmessage");

  /*
    Get a pointer to the old error messages pointer array.
    read_texts() tries to free it.
  */
  org_errmsgs= my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST);

  /* Read messages from file. */
  if (read_texts(ERRMSG_FILE, &errmsgs, ER_ERROR_LAST - ER_ERROR_FIRST + 1) ||
      check_error_mesg(ERRMSG_FILE, errmsgs))
  {
    x_free(errmsgs);
    
    if (org_errmsgs)
    {
      /* Use old error messages */
      errmsgs= org_errmsgs;
    }
    else
    {
      /*
        No error messages.  Create a temporary empty error message so
        that we don't get a crash if some code wrongly tries to access
        a non existing error message.
      */
      if (!(errmsgs= (const char**) my_malloc((ER_ERROR_LAST-ER_ERROR_FIRST+1)*
                                              sizeof(char*), MYF(0))))
        DBUG_RETURN(TRUE);
      for (ptr= errmsgs; ptr < errmsgs + ER_ERROR_LAST - ER_ERROR_FIRST; ptr++)
        *ptr= "";
      error= TRUE;
    }
  }
  else
    x_free(org_errmsgs);                        // Free old language

  /* Register messages for use with my_error(). */
  if (my_error_register(errmsgs, ER_ERROR_FIRST, ER_ERROR_LAST))
  {
    x_free((uchar*) errmsgs);
    DBUG_RETURN(TRUE);
  }

  errmesg= errmsgs;		        /* Init global variabel */
  init_myfunc_errs();			/* Init myfunc messages */
  DBUG_RETURN(error);
}


/**
   Check the error messages array contains all relevant error messages
*/

static bool check_error_mesg(const char *file_name, const char **errmsg)
{
  /*
    The last MySQL error message can't be an empty string; If it is,
    it means that the error file doesn't contain all MySQL messages
    and is probably from an older version of MySQL / MariaDB.
  */
  if (errmsg[ER_LAST_MYSQL_ERROR_MESSAGE -1 - ER_ERROR_FIRST][0] == 0)
  {
    sql_print_error("Error message file '%s' is probably from and older "
                    "version of MariaDB / MYSQL as it doesn't contain all "
                    "error messages", file_name);
    return 1;
  }
  return 0;
}


/**
  Read text from packed textfile in language-directory.

  If we can't read messagefile then it's panic- we can't continue.

  @todo
    Convert the character set to server system character set
*/

static bool read_texts(const char *file_name,const char ***point,
		       uint error_messages)
{
  register uint i;
  uint count,funktpos,textcount;
  size_t length;
  File file;
  char name[FN_REFLEN];
  uchar *buff;
  uchar head[32],*pos;
  const char *errmsg;
  DBUG_ENTER("read_texts");

  *point= 0;

  LINT_INIT(buff);
  funktpos=0;
  if ((file=my_open(fn_format(name,file_name,language,"",4),
		    O_RDONLY | O_SHARE | O_BINARY,
		    MYF(0))) < 0)
    goto err; /* purecov: inspected */

  funktpos=1;
  if (my_read(file,(uchar*) head,32,MYF(MY_NABP))) goto err;
  funktpos=2;
  if (head[0] != (uchar) 254 || head[1] != (uchar) 254 ||
      head[2] != 2 || head[3] != 1)
    goto err; /* purecov: inspected */
  textcount=head[4];

  if (!head[30])
  {
    sql_print_error("Character set information not found in '%s'. \
Please install the latest version of this file.",name);
    goto err1;
  }
  
  /* TODO: Convert the character set to server system character set */
  if (!get_charset(head[30],MYF(MY_WME)))
  {
    sql_print_error("Character set #%d is not supported for messagefile '%s'",
                    (int)head[30],name);
    goto err1;
  }
  
  length=uint2korr(head+6); count=uint2korr(head+8);

  if (count < error_messages)
  {
    sql_print_error("\
Error message file '%s' had only %d error messages, but it should contain at least %d error messages.\nCheck that the above file is the right version for this program!",
		    name,count,error_messages);
    VOID(my_close(file,MYF(MY_WME)));
    DBUG_RETURN(1);
  }

  if (!(*point= (const char**)
	my_malloc((size_t) (length+count*sizeof(char*)),MYF(0))))
  {
    funktpos=3;					/* purecov: inspected */
    goto err;					/* purecov: inspected */
  }
  buff= (uchar*) (*point + count);

  if (my_read(file, buff, (size_t) count*2,MYF(MY_NABP)))
    goto err;
  for (i=0, pos= buff ; i< count ; i++)
  {
    (*point)[i]= (char*) buff+uint2korr(pos);
    pos+=2;
  }
  if (my_read(file, buff, length, MYF(MY_NABP)))
    goto err;

  for (i=1 ; i < textcount ; i++)
  {
    point[i]= *point +uint2korr(head+10+i+i);
  }
  VOID(my_close(file,MYF(0)));
  DBUG_RETURN(0);

err:
  switch (funktpos) {
  case 3:
    errmsg= "Not enough memory for messagefile '%s'";
    break;
  case 2:
    errmsg= "Incompatible header in messagefile '%s'. Probably from another version of MariaDB";
  case 1:
    errmsg= "Can't read from messagefile '%s'";
    break;
  default:
    errmsg= "Can't find messagefile '%s'";
    break;
  }
  sql_print_error(errmsg, name);
err1:
  if (file != FERR)
    VOID(my_close(file,MYF(MY_WME)));
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
