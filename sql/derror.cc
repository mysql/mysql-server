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


/* Read language depeneded messagefile */

#include "mysql_priv.h"
#include "mysys_err.h"

static void read_texts(const char *file_name,const char ***point,
		       uint error_messages);
static void init_myfunc_errs(void);

	/* Read messages from errorfile */

void init_errmessage(void)
{
  DBUG_ENTER("init_errmessage");

  read_texts(ERRMSG_FILE,&errmsg[ERRMAPP],ER_ERROR_MESSAGES);
  errmesg=errmsg[ERRMAPP];		/* Init global variabel */
  init_myfunc_errs();			/* Init myfunc messages */
  DBUG_VOID_RETURN;
}


	/* Read text from packed textfile in language-directory */
	/* If we can't read messagefile then it's panic- we can't continue */

static void read_texts(const char *file_name,const char ***point,
		       uint error_messages)
{
  register uint i;
  uint ant,funktpos,length,textant;
  File file;
  char name[FN_REFLEN];
  const char *buff;
  uchar head[32],*pos;
  DBUG_ENTER("read_texts");

  LINT_INIT(buff);
  funktpos=0;
  if ((file=my_open(fn_format(name,file_name,language,"",4),
		    O_RDONLY | O_SHARE | O_BINARY,
		    MYF(0))) < 0)
    goto err; /* purecov: inspected */

  funktpos=1;
  if (my_read(file,(byte*) head,32,MYF(MY_NABP))) goto err;
  if (head[0] != (uchar) 254 || head[1] != (uchar) 254 ||
      head[2] != 2 || head[3] != 1)
    goto err; /* purecov: inspected */
  textant=head[4];
  length=uint2korr(head+6); ant=uint2korr(head+8);

  if (ant < error_messages)
  {
    fprintf(stderr,"\n%s: Fatal error: Error message file '%s' had only %d error messages, but it should have at least %d error messages.\n\
Check that the above file is the right version for this program!\n\n",
	    my_progname,name,ant,error_messages);
    VOID(my_close(file,MYF(MY_WME)));
    clean_up();				/* Clean_up frees everything */
    exit(1);				/* We can't continue */
  }

  x_free((gptr) *point);		/* Free old language */
  if (!(*point= (const char**)
	my_malloc((uint) (length+ant*sizeof(char*)),MYF(0))))
  {
    funktpos=2;					/* purecov: inspected */
    goto err;					/* purecov: inspected */
  }
  buff= (char*) (*point + ant);

  if (my_read(file,(byte*) buff,(uint) ant*2,MYF(MY_NABP))) goto err;
  for (i=0, pos= (uchar*) buff ; i< ant ; i++)
  {
    (*point)[i]=buff+uint2korr(pos);
    pos+=2;
  }
  if (my_read(file,(byte*) buff,(uint) length,MYF(MY_NABP))) goto err;

  for (i=1 ; i < textant ; i++)
  {
    point[i]= *point +uint2korr(head+10+i+i);
  }
  VOID(my_close(file,MYF(0)));
  DBUG_VOID_RETURN;

err:
  switch (funktpos) {
  case 2:
    buff="\n%s: Fatal error: Not enough memory for messagefile '%s'\n\n";
    break;
  case 1:
    buff="\n%s: Fatal error: Can't read from messagefile '%s'\n\n";
    break;
  default:
    buff="\n%s: Fatal error: Can't find messagefile '%s'\n\n";
    break;
  }
  if (file != FERR)
    VOID(my_close(file,MYF(MY_WME)));
  fprintf(stderr,buff,my_progname,name);
  clean_up();				/* Clean_up frees everything */
  exit(1);				/* We can't continue */
} /* read_texts */


	/* Initiates error-messages used by my_func-library */

static void init_myfunc_errs()
{
  init_glob_errs();			/* Initiate english errors */
  if (!(specialflag & SPECIAL_ENGLISH))
  {
    globerrs[EE_FILENOTFOUND % ERRMOD]	= ER(ER_FILE_NOT_FOUND);
    globerrs[EE_CANTCREATEFILE % ERRMOD]= ER(ER_CANT_CREATE_FILE);
    globerrs[EE_READ % ERRMOD]		= ER(ER_ERROR_ON_READ);
    globerrs[EE_WRITE % ERRMOD]		= ER(ER_ERROR_ON_WRITE);
    globerrs[EE_BADCLOSE % ERRMOD]	= ER(ER_ERROR_ON_CLOSE);
    globerrs[EE_OUTOFMEMORY % ERRMOD]	= ER(ER_OUTOFMEMORY);
    globerrs[EE_DELETE % ERRMOD]	= ER(ER_CANT_DELETE_FILE);
    globerrs[EE_LINK % ERRMOD]		= ER(ER_ERROR_ON_RENAME);
    globerrs[EE_EOFERR % ERRMOD]	= ER(ER_UNEXPECTED_EOF);
    globerrs[EE_CANTLOCK % ERRMOD]	= ER(ER_CANT_LOCK);
    globerrs[EE_DIR % ERRMOD]		= ER(ER_CANT_READ_DIR);
    globerrs[EE_STAT % ERRMOD]		= ER(ER_CANT_GET_STAT);
    globerrs[EE_GETWD % ERRMOD]		= ER(ER_CANT_GET_WD);
    globerrs[EE_SETWD % ERRMOD]		= ER(ER_CANT_SET_WD);
    globerrs[EE_DISK_FULL % ERRMOD]	= ER(ER_DISK_FULL);
  }
}
