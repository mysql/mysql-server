/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code
   added support for long options (my_getopt) 22.5.2002 by Jani Tolonen */

#include "ma_ftdefs.h"
#include "maria_ft_eval.h"
#include <stdarg.h>
#include <my_getopt.h>

static void print_error(int exit_code, const char *fmt,...);
static void get_options(int argc, char *argv[]);
static int create_record(char *pos, FILE *file);
static void usage();

static struct my_option my_long_options[] =
{
  {"", 's', "", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'q', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'S', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", '#', "", 0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'V', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", '?', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'h', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int main(int argc, char *argv[])
{
  MARIA_HA *file;
  int i,j;

  MY_INIT(argv[0]);
  get_options(argc,argv);
  bzero((char*)recinfo,sizeof(recinfo));

  maria_init();
  /* First define 2 columns */
  recinfo[0].type=FIELD_SKIP_ENDSPACE;
  recinfo[0].length=docid_length;
  recinfo[1].type=FIELD_BLOB;
  recinfo[1].length= 4+maria_portable_sizeof_char_ptr;

  /* Define a key over the first column */
  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
  keyinfo[0].block_length= 0;                   /* Default block length */
  keyinfo[0].seg[0].type= HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].flag= HA_BLOB_PART;
  keyinfo[0].seg[0].start=recinfo[0].length;
  keyinfo[0].seg[0].length=key_length;
  keyinfo[0].seg[0].null_bit=0;
  keyinfo[0].seg[0].null_pos=0;
  keyinfo[0].seg[0].bit_start=4;
  keyinfo[0].seg[0].language=MY_CHARSET_CURRENT;
  keyinfo[0].flag = HA_FULLTEXT;

  if (!silent)
    printf("- Creating isam-file\n");
  if (maria_create(filename,1,keyinfo,2,recinfo,0,NULL,(MARIA_CREATE_INFO*) 0,0))
    goto err;
  if (!(file=maria_open(filename,2,0)))
    goto err;
  if (!silent)
    printf("Initializing stopwords\n");
  maria_ft_init_stopwords(stopwordlist);

  if (!silent)
    printf("- Writing key:s\n");

  my_errno=0;
  i=0;
  while (create_record(record,df))
  {
    error=maria_write(file,record);
    if (error)
      printf("I= %2d  maria_write: %d  errno: %d\n",i,error,my_errno);
    i++;
  }
  fclose(df);

  if (maria_close(file)) goto err;
  if (!silent)
    printf("- Reopening file\n");
  if (!(file=maria_open(filename,2,0))) goto err;
  if (!silent)
    printf("- Reading rows with key\n");
  for (i=1;create_record(record,qf);i++)
  {
    FT_DOCLIST *result;
    double w;
    int t, err;

    result=maria_ft_nlq_init_search(file,0,blob_record,(uint) strlen(blob_record),1);
    if (!result)
    {
      printf("Query %d failed with errno %3d\n",i,my_errno);
      goto err;
    }
    if (!silent)
      printf("Query %d. Found: %d.\n",i,result->ndocs);
    for (j=0;(err=maria_ft_nlq_read_next(result, read_record))==0;j++)
    {
      t=uint2korr(read_record);
      w=maria_ft_nlq_get_relevance(result);
      printf("%d %.*s %f\n",i,t,read_record+2,w);
    }
    if (err != HA_ERR_END_OF_FILE)
    {
      printf("maria_ft_read_next %d failed with errno %3d\n",j,my_errno);
      goto err;
    }
    maria_ft_nlq_close_search(result);
  }

  if (maria_close(file)) goto err;
  maria_end();
  my_end(MY_CHECK_ERROR);

  return (0);

 err:
  printf("got error: %3d when using maria-database\n",my_errno);
  return 1;			/* skip warning */

}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 's':
    if (stopwordlist && stopwordlist != maria_ft_precompiled_stopwords)
      break;
    {
      FILE *f; char s[HA_FT_MAXLEN]; int i=0,n=SWL_INIT;

      if (!(stopwordlist=(const char**) malloc(n*sizeof(char *))))
	print_error(1,"malloc(%d)",n*sizeof(char *));
      if (!(f=fopen(argument,"r")))
	print_error(1,"fopen(%s)",argument);
      while (!feof(f))
      {
	if (!(fgets(s,HA_FT_MAXLEN,f)))
	  print_error(1,"fgets(s,%d,%s)",HA_FT_MAXLEN,argument);
	if (!(stopwordlist[i++]=strdup(s)))
	  print_error(1,"strdup(%s)",s);
	if (i >= n)
	{
	  n+=SWL_PLUS;
	  if (!(stopwordlist=(const char**) realloc((char*) stopwordlist,
						    n*sizeof(char *))))
	    print_error(1,"realloc(%d)",n*sizeof(char *));
	}
      }
      fclose(f);
      stopwordlist[i]=NULL;
      break;
    }
  case 'q': silent=1; break;
  case 'S': if (stopwordlist==maria_ft_precompiled_stopwords) stopwordlist=NULL; break;
  case '#':
    DBUG_PUSH (argument);
    break;
  case 'V':
  case '?':
  case 'h':
    usage();
    exit(1);
  }
  return 0;
}


static void get_options(int argc, char *argv[])
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!(d_file=argv[optind])) print_error(1,"No d_file");
  if (!(df=fopen(d_file,"r")))
    print_error(1,"fopen(%s)",d_file);
  if (!(q_file=argv[optind+1])) print_error(1,"No q_file");
  if (!(qf=fopen(q_file,"r")))
    print_error(1,"fopen(%s)",q_file);
  return;
} /* get options */


static int create_record(char *pos, FILE *file)
{
  uint tmp; char *ptr;

  bzero((char *)pos,MAX_REC_LENGTH);

  /* column 1 - VARCHAR */
  if (!(fgets(pos+2,MAX_REC_LENGTH-32,file)))
  {
    if (feof(file))
      return 0;
    else
      print_error(1,"fgets(docid) - 1");
  }
  tmp=(uint) strlen(pos+2)-1;
  int2store(pos,tmp);
  pos+=recinfo[0].length;

  /* column 2 - BLOB */

  if (!(fgets(blob_record,MAX_BLOB_LENGTH,file)))
    print_error(1,"fgets(docid) - 2");
  tmp=(uint) strlen(blob_record);
  int4store(pos,tmp);
  ptr=blob_record;
  memcpy_fixed(pos+4,&ptr,sizeof(char*));
  return 1;
}

/* VARARGS */

static void print_error(int exit_code, const char *fmt,...)
{
  va_list args;

  va_start(args,fmt);
  fprintf(stderr,"%s: error: ",my_progname);
  VOID(vfprintf(stderr, fmt, args));
  VOID(fputc('\n',stderr));
  fflush(stderr);
  va_end(args);
  exit(exit_code);
}


static void usage()
{
  printf("%s [options]\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
