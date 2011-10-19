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

/* Testing of the basic functions of a MARIA rtree table         */
/* Written by Alex Barkov who has a shared copyright to this code */


#include "maria_def.h"
#include "ma_control_file.h"
#include "ma_loghandler.h"
#include "ma_checkpoint.h"
#include "trnman.h"
#include <my_getopt.h>

#ifdef HAVE_RTREE_KEYS

#include "ma_rt_index.h"

#define MAX_REC_LENGTH 1024
#define ndims 2
#define KEYALG HA_KEY_ALG_RTREE

static int read_with_pos(MARIA_HA * file);
static void create_record(uchar *record,uint rownr);
static void create_record1(uchar *record,uint rownr);
static void print_record(uchar * record,my_off_t offs,const char * tail);
static  int run_test(const char *filename);
static void get_options(int argc, char *argv[]);
static void usage();

static double rt_data[]=
{
  /*1*/  0,10,0,10,
  /*2*/  5,15,0,10,
  /*3*/  0,10,5,15,
  /*4*/  10,20,10,20,
  /*5*/  0,10,0,10,
  /*6*/  5,15,0,10,
  /*7*/  0,10,5,15,
  /*8*/  10,20,10,20,
  /*9*/  0,10,0,10,
  /*10*/  5,15,0,10,
  /*11*/  0,10,5,15,
  /*12*/  10,20,10,20,
  /*13*/  0,10,0,10,
  /*14*/  5,15,0,10,
  /*15*/  0,10,5,15,
  /*16*/  10,20,10,20,
  /*17*/  5,15,0,10,
  /*18*/  0,10,5,15,
  /*19*/  10,20,10,20,
  /*20*/  0,10,0,10,

  /*1*/  100,110,0,10,
  /*2*/  105,115,0,10,
  /*3*/  100,110,5,15,
  /*4*/  110,120,10,20,
  /*5*/  100,110,0,10,
  /*6*/  105,115,0,10,
  /*7*/  100,110,5,15,
  /*8*/  110,120,10,20,
  /*9*/  100,110,0,10,
  /*10*/  105,115,0,10,
  /*11*/  100,110,5,15,
  /*12*/  110,120,10,20,
  /*13*/  100,110,0,10,
  /*14*/  105,115,0,10,
  /*15*/  100,110,5,15,
  /*16*/  110,120,10,20,
  /*17*/  105,115,0,10,
  /*18*/  100,110,5,15,
  /*19*/  110,120,10,20,
  /*20*/  100,110,0,10,
  -1
};

static int testflag, checkpoint, create_flag;
static my_bool silent, transactional, die_in_middle_of_transaction,
  opt_versioning;
static enum data_file_type record_type= DYNAMIC_RECORD;

int main(int argc, char *argv[])
{
  char buff[FN_REFLEN];  
  MY_INIT(argv[0]);
  maria_data_root= (char *)".";
  get_options(argc, argv);
  /* Maria requires that we always have a page cache */
  if (maria_init() ||
      (init_pagecache(maria_pagecache, maria_block_size * 16, 0, 0,
                      maria_block_size, MY_WME) == 0) ||
      ma_control_file_open(TRUE, TRUE) ||
      (init_pagecache(maria_log_pagecache,
                      TRANSLOG_PAGECACHE_SIZE, 0, 0,
                      TRANSLOG_PAGE_SIZE, MY_WME) == 0) ||
      translog_init(maria_data_root, TRANSLOG_FILE_SIZE,
                    0, 0, maria_log_pagecache,
                    TRANSLOG_DEFAULT_FLAGS, 0) ||
      (transactional && (trnman_init(0) || ma_checkpoint_init(0))))
  {
    fprintf(stderr, "Error in initialization\n");
    exit(1);
  }

  exit(run_test(fn_format(buff, "test1", maria_data_root, "", MYF(0))));
}


static int run_test(const char *filename)
{
  MARIA_HA        *file;
  MARIA_UNIQUEDEF   uniquedef;
  MARIA_CREATE_INFO create_info;
  MARIA_COLUMNDEF   recinfo[20];
  MARIA_KEYDEF      keyinfo[20];
  HA_KEYSEG      keyseg[20];
  key_range	range;

  int opt_unique=0;
  int key_type=HA_KEYTYPE_DOUBLE;
  int key_length=8;
  int null_fields=0;
  int nrecords=sizeof(rt_data)/(sizeof(double)*4);/* 40 */
  int rec_length=0;
  int uniques=0;
  int i, max_i;
  int error;
  int row_count=0;
  uchar record[MAX_REC_LENGTH];
  uchar read_record[MAX_REC_LENGTH];
  int upd= 10;
  ha_rows hrows;

  bzero(&uniquedef, sizeof(uniquedef));
  bzero(&create_info, sizeof(create_info));
  bzero(recinfo, sizeof(recinfo));
  bzero(keyinfo, sizeof(keyinfo));
  bzero(keyseg, sizeof(keyseg));

  /* Define a column for NULLs and DEL markers*/

  recinfo[0].type=FIELD_NORMAL;
  recinfo[0].length=1; /* For NULL bits */
  rec_length=1;

  /* Define 2*ndims columns for coordinates*/

  for (i=1; i<=2*ndims ;i++)
  {
    recinfo[i].type=FIELD_NORMAL;
    recinfo[i].length=key_length;
    rec_length+=key_length;
  }

  /* Define a key with 2*ndims segments */

  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=2*ndims;
  keyinfo[0].flag=0;
  keyinfo[0].key_alg=KEYALG;

  for (i=0; i<2*ndims; i++)
  {
    keyinfo[0].seg[i].type= key_type;
    keyinfo[0].seg[i].flag=0;          /* Things like HA_REVERSE_SORT */
    keyinfo[0].seg[i].start= (key_length*i)+1;
    keyinfo[0].seg[i].length=key_length;
    keyinfo[0].seg[i].null_bit= null_fields ? 2 : 0;
    keyinfo[0].seg[i].null_pos=0;
    keyinfo[0].seg[i].language=default_charset_info->number;
  }

  if (!silent)
    printf("- Creating isam-file\n");

  create_info.max_rows=10000000;
  create_info.transactional= transactional;

  if (maria_create(filename,
                   record_type,
                   1,            /*  keys   */
                   keyinfo,
                   1+2*ndims+opt_unique, /* columns */
                   recinfo,uniques,&uniquedef,&create_info,create_flag))
    goto err;

  if (!silent)
    printf("- Open isam-file\n");

  if (!(file=maria_open(filename,2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;
  maria_begin(file);
  if (opt_versioning)
    maria_versioning(file, 1);
  if (testflag == 1)
    goto end;
  if (checkpoint == 1 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;
  if (!silent)
    printf("- Writing key:s\n");

  for (i=0; i<nrecords; i++ )
  {
    create_record(record,i);
    error=maria_write(file,record);
    print_record(record,maria_position(file),"\n");
    if (!error)
    {
      row_count++;
    }
    else
    {
      fprintf(stderr, "maria_write: %d\n", error);
      goto err;
    }
  }

  if (maria_scan_init(file))
  {
    fprintf(stderr, "maria_scan_init failed\n");
    goto err;
  }
  if ((error=read_with_pos(file)))
    goto err;
  maria_scan_end(file);

  if (!silent)
    printf("- Reading rows with key\n");

  for (i=0 ; i < nrecords ; i++)
  {
    my_errno=0;
    create_record(record,i);

    bzero((char*) read_record,MAX_REC_LENGTH);
    error=maria_rkey(file,read_record,0,record+1,HA_WHOLE_KEY,HA_READ_MBR_EQUAL);

    if (error && error!=HA_ERR_KEY_NOT_FOUND)
    {
      fprintf(stderr,"     maria_rkey: %3d  errno: %3d\n",error,my_errno);
      goto err;
    }
    if (error == HA_ERR_KEY_NOT_FOUND)
    {
      print_record(record,maria_position(file),"  NOT FOUND\n");
      continue;
    }
    print_record(read_record,maria_position(file),"\n");
  }

  if (checkpoint == 2 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (testflag == 2)
    goto end;

  if (!silent)
    printf("- Deleting rows\n");
  if (maria_scan_init(file))
  {
    fprintf(stderr, "maria_scan_init failed\n");
    goto err;
  }

  for (i=0; i < nrecords/4; i++)
  {
    my_errno=0;
    bzero((char*) read_record,MAX_REC_LENGTH);
    error=maria_scan(file,read_record);
    if (error)
    {
      fprintf(stderr, "pos: %2d  maria_rrnd: %3d  errno: %3d\n", i, error,
              my_errno);
      goto err;
    }
    print_record(read_record,maria_position(file),"\n");

    error=maria_delete(file,read_record);
    if (error)
    {
      fprintf(stderr, "pos: %2d maria_delete: %3d errno: %3d\n", i, error,
              my_errno);
      goto err;
    }
  }
  maria_scan_end(file);

  if (testflag == 3)
    goto end;
  if (checkpoint == 3 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (!silent)
    printf("- Updating rows with position\n");
  if (maria_scan_init(file))
  {
    fprintf(stderr, "maria_scan_init failed\n");
    goto err;
  }

  /* We are looking for nrecords-necords/2 non-deleted records */
  for (i=0, max_i= nrecords - nrecords/2; i < max_i ; i++)
  {
    my_errno=0;
    bzero((char*) read_record,MAX_REC_LENGTH);
    error=maria_scan(file,read_record);
    if (error)
    {
      if (error==HA_ERR_RECORD_DELETED)
      {
        if (!silent)
          printf("found deleted record\n");
        /*
          In BLOCK_RECORD format, maria_scan() never returns deleted records,
          while in DYNAMIC format it can. Don't count such record:
        */
        max_i++;
        continue;
      }
      fprintf(stderr, "pos: %2d  maria_rrnd: %3d  errno: %3d\n",i , error,
              my_errno);
      goto err;
    }
    print_record(read_record,maria_position(file),"");
    create_record1(record,i+nrecords*upd);
    if (!silent)
      printf("\t-> ");
    print_record(record,maria_position(file),"\n");
    error=maria_update(file,read_record,record);
    if (error)
    {
      fprintf(stderr, "pos: %2d  maria_update: %3d  errno: %3d\n",i, error,
              my_errno);
      goto err;
    }
  }

  if (testflag == 4)
    goto end;
  if (checkpoint == 4 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (maria_scan_init(file))
  {
    fprintf(stderr, "maria_scan_init failed\n");
    goto err;
  }
  if ((error=read_with_pos(file)))
    goto err;
  maria_scan_end(file);

  if (!silent)
    printf("- Test maria_rkey then a sequence of maria_rnext_same\n");

  create_record(record, nrecords*4/5);
  print_record(record,0,"  search for\n");

  if ((error=maria_rkey(file,read_record,0,record+1,HA_WHOLE_KEY,
                        HA_READ_MBR_INTERSECT)))
  {
    fprintf(stderr, "maria_rkey: %3d  errno: %3d\n",error,my_errno);
    goto err;
  }
  print_record(read_record,maria_position(file),"  maria_rkey\n");
  row_count=1;

  for (;;)
  {
    if ((error=maria_rnext_same(file,read_record)))
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      fprintf(stderr, "maria_next: %3d  errno: %3d\n",error,my_errno);
      goto err;
    }
    print_record(read_record,maria_position(file),"  maria_rnext_same\n");
      row_count++;
  }
  if (!silent)
    printf("     %d rows\n",row_count);

  if (!silent)
    printf("- Test maria_rfirst then a sequence of maria_rnext\n");

  error=maria_rfirst(file,read_record,0);
  if (error)
  {
    fprintf(stderr, "maria_rfirst: %3d  errno: %3d\n",error,my_errno);
    goto err;
  }
  row_count=1;
  print_record(read_record,maria_position(file),"  maria_frirst\n");

  for (i=0;i<nrecords;i++)
  {
    if ((error=maria_rnext(file,read_record,0)))
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      fprintf(stderr, "maria_next: %3d  errno: %3d\n",error,my_errno);
      goto err;
    }
    print_record(read_record,maria_position(file),"  maria_rnext\n");
    row_count++;
  }
  if (!silent)
    printf("     %d rows\n",row_count);

  if (!silent)
    printf("- Test maria_records_in_range()\n");

  create_record1(record, nrecords*4/5);
  print_record(record,0,"\n");

  range.key= record+1;
  range.length= 1000;                           /* Big enough */
  range.flag= HA_READ_MBR_INTERSECT;
  hrows= maria_records_in_range(file,0, &range, (key_range*) 0);
  if (!silent)
    printf("     %ld rows\n", (long) hrows);

end:
  maria_scan_end(file);
  if (die_in_middle_of_transaction)
  {
    /* see similar code in ma_test2.c for comments */
    switch (die_in_middle_of_transaction) {
    case 1:
      _ma_flush_table_files(file, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                            FLUSH_RELEASE, FLUSH_RELEASE);
      break;
    case 2:
      if (translog_flush(file->trn->undo_lsn))
        goto err;
      break;
    case 3:
      break;
    case 4:
      _ma_flush_table_files(file, MARIA_FLUSH_DATA, FLUSH_RELEASE,
                            FLUSH_RELEASE);
      if (translog_flush(file->trn->undo_lsn))
        goto err;
      break;
    }
    if (!silent)
      printf("Dying on request without maria_commit()/maria_close()\n");
    exit(0);
  }
  if (maria_commit(file))
    goto err;
  if (maria_close(file)) goto err;
  maria_end();
  my_end(MY_CHECK_ERROR);

  return 0;

err:
  fprintf(stderr, "got error: %3d when using maria-database\n",my_errno);
  return 1;           /* skip warning */
}



static int read_with_pos (MARIA_HA * file)
{
  int error;
  int i;
  uchar read_record[MAX_REC_LENGTH];

  if (!silent)
    printf("- Reading rows with position\n");
  for (i=0;;i++)
  {
    my_errno=0;
    bzero((char*) read_record,MAX_REC_LENGTH);
    error=maria_scan(file,read_record);
    if (error)
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      if (error==HA_ERR_RECORD_DELETED)
        continue;
      fprintf(stderr, "pos: %2d  maria_rrnd: %3d  errno: %3d\n", i, error,
              my_errno);
      return error;
    }
    print_record(read_record,maria_position(file),"\n");
  }
  return 0;
}


#ifdef NOT_USED
static void bprint_record(char * record,
			  my_off_t offs __attribute__((unused)),
			  const char * tail)
{
  int i;
  char * pos;
  if (silent)
    return;
  i=(unsigned char)record[0];
  printf("%02X ",i);

  for( pos=record+1, i=0; i<32; i++,pos++){
    int b=(unsigned char)*pos;
    printf("%02X",b);
  }
  printf("%s",tail);
}
#endif


static void print_record(uchar *record,
			 my_off_t offs __attribute__((unused)),
			 const char * tail)
{
  int i;
  uchar *pos;
  double c;

  if (silent)
    return;
  printf("     rec=(%d)",(unsigned char)record[0]);
  for ( pos=record+1, i=0; i<2*ndims; i++)
   {
      memcpy(&c,pos,sizeof(c));
      float8get(c,pos);
      printf(" %.14g ",c);
      pos+=sizeof(c);
   }
   printf("pos=%ld",(long int)offs);
   printf("%s",tail);
}



static void create_record1(uchar *record, uint rownr)
{
   int i;
   uchar *pos;
   double c=rownr+10;

   bzero((char*) record,MAX_REC_LENGTH);
   record[0]=0x01; /* DEL marker */

   for ( pos=record+1, i=0; i<2*ndims; i++)
   {
      memcpy(pos,&c,sizeof(c));
      float8store(pos,c);
      pos+=sizeof(c);
   }
}

#ifdef NOT_USED

static void create_record0(char *record,uint rownr)
{
   int i;
   char * pos;
   double c=rownr+10;
   double c0=0;

   bzero((char*) record,MAX_REC_LENGTH);
   record[0]=0x01; /* DEL marker */

   for ( pos=record+1, i=0; i<ndims; i++)
   {
      memcpy(pos,&c0,sizeof(c0));
      float8store(pos,c0);
      pos+=sizeof(c0);
      memcpy(pos,&c,sizeof(c));
      float8store(pos,c);
      pos+=sizeof(c);
   }
}

#endif

static void create_record(uchar *record, uint rownr)
{
   int i;
   uchar *pos;
   double *data= rt_data+rownr*4;
   record[0]=0x01; /* DEL marker */
   for ( pos=record+1, i=0; i<ndims*2; i++)
   {
      float8store(pos,data[i]);
      pos+=8;
   }
}


static struct my_option my_long_options[] =
{
  {"checkpoint", 'H', "Checkpoint at specified stage", (uchar**) &checkpoint,
   (uchar**) &checkpoint, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"checksum", 'c', "Undocumented",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Undocumented",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"help", '?', "Display help and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h', "Path to the database root.", &maria_data_root,
   &maria_data_root, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"row-fixed-size", 'S', "Fixed size records",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"rows-in-block", 'M', "Store rows in block format",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Undocumented",
   (uchar**) &silent, (uchar**) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"testflag", 't', "Stop test at specified stage", (uchar**) &testflag,
   (uchar**) &testflag, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"test-undo", 'A',
   "Abort hard. Used for testing recovery with undo",
   (uchar**) &die_in_middle_of_transaction,
   (uchar**) &die_in_middle_of_transaction,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transactional", 'T',
   "Test in transactional mode. (Only works with block format)",
   (uchar**) &transactional, (uchar**) &transactional, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"versioning", 'C', "Use row versioning (only works with block format)",
   (uchar**) &opt_versioning,  (uchar**) &opt_versioning, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'c':
    create_flag|= HA_CREATE_CHECKSUM | HA_CREATE_PAGE_CHECKSUM;
    break;
  case 'M':
    record_type= BLOCK_RECORD;
    break;
  case 'S':
    record_type= STATIC_RECORD;
    break;
  case '#':
    DBUG_PUSH(argument);
    break;
  case '?':
    usage();
    exit(1);
  }
  return 0;
}


/* Read options */

static void get_options(int argc, char *argv[])
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  return;
} /* get options */


static void usage()
{
  printf("Usage: %s [options]\n\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

#include "ma_check_standalone.h"

#else
int main(int argc __attribute__((unused)),char *argv[] __attribute__((unused)))
{
  exit(0);
}
#endif /*HAVE_RTREE_KEYS*/
