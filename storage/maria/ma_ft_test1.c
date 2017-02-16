/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code
   added support for long options (my_getopt) 22.5.2002 by Jani Tolonen */

#include "ma_ftdefs.h"
#include "maria_ft_test1.h"
#include <my_getopt.h>

static int key_field=FIELD_VARCHAR,extra_field=FIELD_SKIP_ENDSPACE;
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
static void usage();

static struct my_option my_long_options[] =
{
  {"", 'v', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", '?', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'h', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'V', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'v', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 's', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'N', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'S', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'K', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'F', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", 'U', "", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"", '#', "", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int main(int argc, char *argv[])
{
  MY_INIT(argv[0]);

  get_options(argc,argv);
  maria_init();

  exit(run_test("FT1"));
}

static MARIA_COLUMNDEF recinfo[3];
static MARIA_KEYDEF keyinfo[2];
static HA_KEYSEG keyseg[10];

static int run_test(const char *filename)
{
  MARIA_HA *file;
  int i,j;
  my_off_t pos;

  bzero((char*) recinfo,sizeof(recinfo));

  /* First define 2 columns */
  recinfo[0].type=extra_field;
  recinfo[0].length= (extra_field == FIELD_BLOB ? 4 + portable_sizeof_char_ptr :
	      extra_length);
  if (extra_field == FIELD_VARCHAR)
    recinfo[0].length+= HA_VARCHAR_PACKLENGTH(extra_length);
  recinfo[1].type=key_field;
  recinfo[1].length= (key_field == FIELD_BLOB ? 4+portable_sizeof_char_ptr :
		      key_length);
  if (key_field == FIELD_VARCHAR)
    recinfo[1].length+= HA_VARCHAR_PACKLENGTH(key_length);

  /* Define a key over the first column */
  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
  keyinfo[0].block_length= 0;                   /* Default block length */
  keyinfo[0].seg[0].type= key_type;
  keyinfo[0].seg[0].flag= (key_field == FIELD_BLOB) ? HA_BLOB_PART:
			  (key_field == FIELD_VARCHAR) ? HA_VAR_LENGTH_PART:0;
  keyinfo[0].seg[0].start=recinfo[0].length;
  keyinfo[0].seg[0].length=key_length;
  keyinfo[0].seg[0].null_bit= 0;
  keyinfo[0].seg[0].null_pos=0;
  keyinfo[0].seg[0].language= default_charset_info->number;
  keyinfo[0].flag = (no_fulltext?HA_PACK_KEY:HA_FULLTEXT);

  if (!silent)
    printf("- Creating isam-file\n");
  if (maria_create(filename,(no_keys?0:1),keyinfo,2,recinfo,0,NULL,
		(MARIA_CREATE_INFO*) 0, create_flag))
    goto err;
  if (!(file=maria_open(filename,2,0)))
    goto err;

  if (!silent)
    printf("- %s stopwords\n",no_stopwords?"Skipping":"Initializing");
  maria_ft_init_stopwords(no_stopwords?NULL:maria_ft_precompiled_stopwords);

  if (!silent)
    printf("- Writing key:s\n");

  my_errno=0;
  for (i=NUPD ; i<NDATAS; i++ )
  {
    create_record(record,i);
    error=maria_write(file,record);
    if (verbose || error)
      printf("I= %2d  maria_write: %d  errno: %d, record: %s\n",
	i,error,my_errno,data[i].f0);
  }

  if (!skip_update)
  {
    if (!silent)
      printf("- Updating rows\n");

    /* Read through all rows and update them */
    pos=(ha_rows) 0;
    i=0;
    while ((error=maria_rrnd(file,read_record,pos)) == 0)
    {
      create_record(record,NUPD-i-1);
      if (maria_update(file,read_record,record))
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

  if (maria_close(file)) goto err;
  if(no_search) return 0;
  if (!silent)
    printf("- Reopening file\n");
  if (!(file=maria_open(filename,2,0))) goto err;
  if (!silent)
    printf("- Reading rows with key\n");
  for (i=0 ; i < NQUERIES ; i++)
  {
    FT_DOCLIST *result;
    result=maria_ft_nlq_init_search(file,0,(char*) query[i],strlen(query[i]),1);
    if(!result)
    {
      printf("Query %d: `%s' failed with errno %3d\n",i,query[i],my_errno);
      continue;
    }
    printf("Query %d: `%s'. Found: %d. Top five documents:\n",
           i,query[i],result->ndocs);
    for (j=0;j<5;j++)
    {
      double w; int err;
      err= maria_ft_nlq_read_next(result, read_record);
      if (err==HA_ERR_END_OF_FILE)
      {
        printf("No more matches!\n");
        break;
      }
      else if (err)
      {
        printf("maria_ft_read_next %d failed with errno %3d\n",j,my_errno);
        break;
      }
      w=maria_ft_nlq_get_relevance(result);
      if (key_field == FIELD_VARCHAR)
      {
        uint l;
        char *p;
        p=recinfo[0].length+read_record;
        l=uint2korr(p);
        printf("%10.7f: %.*s\n",w,(int) l,p+2);
      }
      else
        printf("%10.7f: %.*s\n",w,recinfo[1].length,
               recinfo[0].length+read_record);
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

static char blob_key[MAX_REC_LENGTH];
/* static char blob_record[MAX_REC_LENGTH+20*20]; */

void create_record(char *pos, int n)
{
  bzero((char*) pos,MAX_REC_LENGTH);
  if (recinfo[0].type == FIELD_BLOB)
  {
    uint tmp;
    char *ptr;
    strnmov(blob_key,data[n].f0,keyinfo[0].seg[0].length);
    tmp=strlen(blob_key);
    int4store(pos,tmp);
    ptr=blob_key;
    memcpy_fixed(pos+4,&ptr,sizeof(char*));
    pos+=recinfo[0].length;
  }
  else if (recinfo[0].type == FIELD_VARCHAR)
  {
    uint tmp;
    /* -1 is here because pack_length is stored in seg->length */
    uint pack_length= HA_VARCHAR_PACKLENGTH(keyinfo[0].seg[0].length-1);
    strnmov(pos+pack_length,data[n].f0,keyinfo[0].seg[0].length);
    tmp=strlen(pos+pack_length);
    if (pack_length == 1)
      *pos= (char) tmp;
    else
      int2store(pos,tmp);
    pos+=recinfo[0].length;
  }
  else
  {
    strnmov(pos,data[n].f0,keyinfo[0].seg[0].length);
    pos+=recinfo[0].length;
  }
  if (recinfo[1].type == FIELD_BLOB)
  {
    uint tmp;
    char *ptr;
    strnmov(blob_key,data[n].f2,keyinfo[0].seg[0].length);
    tmp=strlen(blob_key);
    int4store(pos,tmp);
    ptr=blob_key;
    memcpy_fixed(pos+4,&ptr,sizeof(char*));
    pos+=recinfo[1].length;
  }
  else if (recinfo[1].type == FIELD_VARCHAR)
  {
    uint tmp;
    /* -1 is here because pack_length is stored in seg->length */
    uint pack_length= HA_VARCHAR_PACKLENGTH(keyinfo[0].seg[0].length-1);
    strnmov(pos+pack_length,data[n].f2,keyinfo[0].seg[0].length);
    tmp=strlen(pos+1);
    if (pack_length == 1)
      *pos= (char) tmp;
    else
      int2store(pos,tmp);
    pos+=recinfo[1].length;
  }
  else
  {
    strnmov(pos,data[n].f2,keyinfo[0].seg[0].length);
    pos+=recinfo[1].length;
  }
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case 'v': verbose=1; break;
  case 's': silent=1; break;
  case 'F': no_fulltext=1; no_search=1;
  case 'U': skip_update=1; break;
  case 'K': no_keys=no_search=1; break;
  case 'N': no_search=1; break;
  case 'S': no_stopwords=1; break;
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

/* Read options */

static void get_options(int argc,char *argv[])
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);
  return;
} /* get options */


static void usage()
{
  printf("%s [options]\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
