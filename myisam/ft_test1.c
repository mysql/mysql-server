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
#include "ft_test1.h"
#include <getopt.h>

static int key_field=FIELD_VARCHAR,extra_field=FIELD_SKIPP_ENDSPACE;
static uint key_length=200,extra_length=50;
static int key_type=HA_KEYTYPE_TEXT;
static int verbose=0,silent=0,skip_update=0,
	   no_keys=0,no_stopwords=0,no_search=0,no_fulltext=0;
static int create_flag=0,error=0;

#define MAX_REC_LENGTH 300
static char record[MAX_REC_LENGTH],read_record[MAX_REC_LENGTH];

static int run_test(const char *filename);
static void get_options(int argc, char *argv[]);
static void create_record(char *, int);

int main(int argc,char *argv[])
{
  MY_INIT(argv[0]);

  get_options(argc,argv);

  exit(run_test("FT1"));
}

static MI_COLUMNDEF recinfo[3];
static MI_KEYDEF keyinfo[2];
static MI_KEYSEG keyseg[10];

static int run_test(const char *filename)
{
  MI_INFO *file;
  int i,j;
  my_off_t pos;

  bzero((char*) recinfo,sizeof(recinfo));

  /* First define 2 columns */
  recinfo[0].type=extra_field;
  recinfo[0].length= (extra_field == FIELD_BLOB ? 4 + mi_portable_sizeof_char_ptr :
	      extra_length);
  if (extra_field == FIELD_VARCHAR)
    recinfo[0].length+=2;
  recinfo[1].type=key_field;
  recinfo[1].length= (key_field == FIELD_BLOB ? 4+mi_portable_sizeof_char_ptr :
		      key_length);
  if (key_field == FIELD_VARCHAR)
    recinfo[1].length+=2;

  /* Define a key over the first column */
  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
  keyinfo[0].seg[0].type= key_type;
  keyinfo[0].seg[0].flag= (key_field == FIELD_BLOB)?HA_BLOB_PART:
			  (key_field == FIELD_VARCHAR)?HA_VAR_LENGTH:0;
  keyinfo[0].seg[0].start=recinfo[0].length;
  keyinfo[0].seg[0].length=key_length;
  keyinfo[0].seg[0].null_bit= 0;
  keyinfo[0].seg[0].null_pos=0;
  keyinfo[0].seg[0].language=MY_CHARSET_CURRENT;
  keyinfo[0].flag = (no_fulltext?HA_PACK_KEY:HA_FULLTEXT);

  if (!silent)
    printf("- Creating isam-file\n");
  if (mi_create(filename,(no_keys?0:1),keyinfo,2,recinfo,0,NULL,
		(MI_CREATE_INFO*) 0, create_flag))
    goto err;
  if (!(file=mi_open(filename,2,0)))
    goto err;

  if (!silent)
    printf("- %s stopwords\n",no_stopwords?"Skipping":"Initializing");
  ft_init_stopwords(no_stopwords?NULL:ft_precompiled_stopwords);

  if (!silent)
    printf("- Writing key:s\n");

  my_errno=0;
  for (i=NUPD ; i<NDATAS; i++ )
  {
    create_record(record,i);
    error=mi_write(file,record);
    if (verbose || error)
      printf("I= %2d  mi_write: %d  errno: %d, record: %s\n",
	i,error,my_errno,data[i].f0);
  }

  if (!skip_update)
  {
    if (!silent)
      printf("- Updating rows\n");

    /* Read through all rows and update them */
    pos=(ha_rows) 0;
    i=0;
    while ((error=mi_rrnd(file,read_record,pos)) == 0)
    {
      create_record(record,NUPD-i-1);
      if (mi_update(file,read_record,record))
      {
	printf("Can't update row: %.*s, error: %d\n",
	       keyinfo[0].seg[0].length,record,my_errno);
      }
      if(++i == NUPD) break;
      pos=HA_OFFSET_ERROR;
    }
    if (i != NUPD)
      printf("Found %d of %d rows\n", i,NUPD);
  }

  if (mi_close(file)) goto err;
  if(no_search) return 0;
  if (!silent)
    printf("- Reopening file\n");
  if (!(file=mi_open(filename,2,0))) goto err;
  if (!silent)
    printf("- Reading rows with key\n");
  for (i=0 ; i < NQUERIES ; i++)
  { FT_DOCLIST *result;
    result=ft_init_search(file,0,(char*) query[i],strlen(query[i]),1);
    if(!result) {
      printf("Query %d: `%s' failed with errno %3d\n",i,query[i],my_errno);
      continue;
    }
    printf("Query %d: `%s'. Found: %d. Top five documents:\n",
	    i,query[i],result->ndocs);
    for(j=0;j<5;j++) { double w; int err;
	err=ft_read_next(result, read_record);
	if(err==HA_ERR_END_OF_FILE) {
	    printf("No more matches!\n");
	    break;
	} else if (err) {
	    printf("ft_read_next %d failed with errno %3d\n",j,my_errno);
	    break;
	}
        w=ft_get_relevance(result);
	if(key_field == FIELD_VARCHAR) {
	    uint l;
	    char *p;
	    p=recinfo[0].length+read_record;
	    l=uint2korr(p);
	    printf("%10.7f: %.*s\n",w,(int) l,p+2);
	} else
	    printf("%10.7f: %.*s\n",w,recinfo[1].length,
			  recinfo[0].length+read_record);
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

static char blob_key[MAX_REC_LENGTH];
/* static char blob_record[MAX_REC_LENGTH+20*20]; */

void create_record(char *pos, int n)
{
  bzero((char*) pos,MAX_REC_LENGTH);
  if (recinfo[0].type == FIELD_BLOB)
  {
    uint tmp;
    char *ptr;
    strncpy(blob_key,data[n].f0,keyinfo[0].seg[0].length);
    tmp=strlen(blob_key);
    int4store(pos,tmp);
    ptr=blob_key;
    memcpy_fixed(pos+4,&ptr,sizeof(char*));
    pos+=recinfo[0].length;
  }
  else if (recinfo[0].type == FIELD_VARCHAR)
  {
    uint tmp;
    strncpy(pos+2,data[n].f0,keyinfo[0].seg[0].length);
    tmp=strlen(pos+2);
    int2store(pos,tmp);
    pos+=recinfo[0].length;
  }
  else
  {
    strncpy(pos,data[n].f0,keyinfo[0].seg[0].length);
    pos+=recinfo[0].length;
  }
  if (recinfo[1].type == FIELD_BLOB)
  {
    uint tmp;
    char *ptr;
    strncpy(blob_key,data[n].f2,keyinfo[0].seg[0].length);
    tmp=strlen(blob_key);
    int4store(pos,tmp);
    ptr=blob_key;
    memcpy_fixed(pos+4,&ptr,sizeof(char*));
    pos+=recinfo[1].length;
  }
  else if (recinfo[1].type == FIELD_VARCHAR)
  {
    uint tmp;
    strncpy(pos+2,data[n].f2,keyinfo[0].seg[0].length);
    tmp=strlen(pos+2);
    int2store(pos,tmp);
    pos+=recinfo[1].length;
  }
  else
  {
    strncpy(pos,data[n].f2,keyinfo[0].seg[0].length);
    pos+=recinfo[1].length;
  }
}

/* Read options */

static void get_options(int argc,char *argv[])
{
  int c;
  const char *options="hVvsNSKFU#:";

  while ((c=getopt(argc,argv,options)) != -1)
  {
    switch(c) {
    case 'v': verbose=1; break;
    case 's': silent=1; break;
    case 'F': no_fulltext=1; no_search=1;
    case 'U': skip_update=1; break;
    case 'K': no_keys=no_search=1; break;
    case 'N': no_search=1; break;
    case 'S': no_stopwords=1; break;
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (optarg);
      break;
    case 'V':
    case '?':
    case 'h':
    default:
      printf("%s -[%s]\n", argv[0], options);
      exit(0);
    }
  }
  return;
} /* get options */
