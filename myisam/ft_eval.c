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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include "ftdefs.h"
#include "ft_eval.h"
#include <stdarg.h>
#include <getopt.h>

static void print_error(int exit_code, const char *fmt,...);
static void get_options(int argc, char *argv[]);
static int create_record(char *pos, FILE *file);

int main(int argc,char *argv[])
{
  MI_INFO *file;
  int i,j;

  MY_INIT(argv[0]);
  get_options(argc,argv);
  bzero((char*)recinfo,sizeof(recinfo));

  /* First define 2 columns */
  recinfo[0].type=FIELD_SKIPP_ENDSPACE;
  recinfo[0].length=docid_length;
  recinfo[1].type=FIELD_BLOB;
  recinfo[1].length= 4+mi_portable_sizeof_char_ptr;

  /* Define a key over the first column */
  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
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
  if (mi_create(filename,1,keyinfo,2,recinfo,0,NULL,(MI_CREATE_INFO*) 0,0))
    goto err;
  if (!(file=mi_open(filename,2,0)))
    goto err;
  if (!silent)
    printf("Initializing stopwords\n");
  ft_init_stopwords(stopwordlist);

  if (!silent)
    printf("- Writing key:s\n");

  my_errno=0;
  i=0;
  while(create_record(record,df))
  {
    error=mi_write(file,record);
    if (error)
      printf("I= %2d  mi_write: %d  errno: %d\n",i,error,my_errno);
    i++;
  }
  fclose(df);

  if (mi_close(file)) goto err;
  if (!silent)
    printf("- Reopening file\n");
  if (!(file=mi_open(filename,2,0))) goto err;
  if (!silent)
    printf("- Reading rows with key\n");
  for(i=1;create_record(record,qf);i++) {
    FT_DOCLIST *result; double w; int t,err;

    result=ft_init_search(file,0,blob_record,(uint) strlen(blob_record),1);
    if(!result) {
      printf("Query %d failed with errno %3d\n",i,my_errno);
      goto err;
    }
    if (!silent)
      printf("Query %d. Found: %d.\n",i,result->ndocs);
    for(j=0;(err=ft_read_next(result, read_record))==0;j++) {
      t=uint2korr(read_record);
      w=ft_get_relevance(result);
      printf("%d %.*s %f\n",i,t,read_record+2,w);
    }
    if(err != HA_ERR_END_OF_FILE) {
      printf("ft_read_next %d failed with errno %3d\n",j,my_errno);
      goto err;
    }
    ft_close_search(result);
  }

  if (mi_close(file)) goto err;
  my_end(MY_CHECK_ERROR);

  return (0);
err:
  printf("got error: %3d when using myisam-database\n",my_errno);
  return 1;			/* skipp warning */

}

static void get_options(int argc,char *argv[])
{
  int c;
  char *options=(char*) "Vh#:qSs:";

  while ((c=getopt(argc,argv,options)) != -1)
  {
    switch(c) {
    case 's':
      if(stopwordlist && stopwordlist!=ft_precompiled_stopwords) break;
      {
	FILE *f; char s[HA_FT_MAXLEN]; int i=0,n=SWL_INIT;

	if(!(stopwordlist=(const char**) malloc(n*sizeof(char *))))
	  print_error(1,"malloc(%d)",n*sizeof(char *));
	if(!(f=fopen(optarg,"r")))
	  print_error(1,"fopen(%s)",optarg);
	while(!feof(f)) {
	  if(!(fgets(s,HA_FT_MAXLEN,f)))
	    print_error(1,"fgets(s,%d,%s)",HA_FT_MAXLEN,optarg);
	  if(!(stopwordlist[i++]=strdup(s)))
	    print_error(1,"strdup(%s)",s);
	  if(i>=n) {
	    n+=SWL_PLUS;
	    if(!(stopwordlist=(const char**) realloc((char*) stopwordlist,n*sizeof(char *))))
	      print_error(1,"realloc(%d)",n*sizeof(char *));
	  }
	}
	fclose(f);
	stopwordlist[i]=NULL;
	break;
      }
    case 'q': silent=1; break;
    case 'S': if(stopwordlist==ft_precompiled_stopwords) stopwordlist=NULL; break;
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (optarg);
      break;
    case 'V':
    case '?':
    case 'h':
    default:
      printf("%s -[%s] <d_file> <q_file>\n", argv[0], options);
      exit(0);
    }
  }
  if(!(d_file=argv[optind])) print_error(1,"No d_file");
  if(!(df=fopen(d_file,"r")))
    print_error(1,"fopen(%s)",d_file);
  if(!(q_file=argv[optind+1])) print_error(1,"No q_file");
  if(!(qf=fopen(q_file,"r")))
    print_error(1,"fopen(%s)",q_file);
  return;
} /* get options */

static int create_record(char *pos, FILE *file)
{ uint tmp; char *ptr;

  bzero((char *)pos,MAX_REC_LENGTH);

  /* column 1 - VARCHAR */
  if(!(fgets(pos+2,MAX_REC_LENGTH-32,file)))
  {
    if(feof(file)) return 0; else print_error(1,"fgets(docid) - 1");
  }
  tmp=(uint) strlen(pos+2)-1;
  int2store(pos,tmp);
  pos+=recinfo[0].length;

  /* column 2 - BLOB */

  if(!(fgets(blob_record,MAX_BLOB_LENGTH,file)))
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
