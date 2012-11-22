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

/* Test av isam-databas: stor test */

#ifndef USE_MY_FUNC		/* We want to be able to dbug this !! */
#define USE_MY_FUNC
#endif
#include "maria_def.h"
#include "trnman.h"
#include <m_ctype.h>
#include <my_bit.h>
#include "ma_checkpoint.h"

#define STANDARD_LENGTH 37
#define MARIA_KEYS 6
#define MAX_PARTS 4
#if !defined(MSDOS) && !defined(labs)
#define labs(a) abs(a)
#endif

static void get_options(int argc, char *argv[]);
static uint rnd(uint max_value);
static void fix_length(uchar *record,uint length);
static void put_blob_in_record(uchar *blob_pos,char **blob_buffer,
                               ulong *length);
static void copy_key(MARIA_HA *info, uint inx, uchar *record, uchar *key);

static int verbose= 0, testflag= 0, first_key= 0, async_io= 0, pagecacheing= 0;
static int write_cacheing= 0, do_locking= 0, rec_pointer_size= 0;
static int silent= 0, opt_quick_mode= 0, transactional= 0, skip_update= 0;
static int die_in_middle_of_transaction= 0, pack_fields= 1;
static int pack_seg= HA_SPACE_PACK, pack_type= HA_PACK_KEY, remove_count= -1;
static int create_flag= 0, srand_arg= 0, checkpoint= 0;
static my_bool opt_versioning= 0;
static uint use_blob= 0, update_count= 0;
static ulong pagecache_size=8192*32;
static enum data_file_type record_type= DYNAMIC_RECORD;

static uint keys=MARIA_KEYS,recant=1000;
static uint16 key1[1001],key3[5001];
static uchar record[300],record2[300],key[100],key2[100];
static uchar read_record[300],read_record2[300],read_record3[300];
static HA_KEYSEG glob_keyseg[MARIA_KEYS][MAX_PARTS];

		/* Test program */

int main(int argc, char *argv[])
{
  uint i;
  int j,n1,n2,n3,error,k;
  uint write_count,update,dupp_keys,opt_delete,start,length,blob_pos,
       reclength,ant,found_parts;
  my_off_t lastpos;
  ha_rows range_records,records;
  MARIA_HA *file;
  MARIA_KEYDEF keyinfo[10];
  MARIA_COLUMNDEF recinfo[10];
  MARIA_INFO info;
  char *blob_buffer;
  MARIA_CREATE_INFO create_info;
  char filename[FN_REFLEN];

#ifdef SAFE_MUTEX
  safe_mutex_deadlock_detector= 1;
#endif
  MY_INIT(argv[0]);

  maria_data_root= (char *)".";
  get_options(argc,argv);
  fn_format(filename, "test2", maria_data_root, "", MYF(0));

  if (! async_io)
    my_disable_async_io=1;

  /* If we sync or not have no affect on this test */
  my_disable_sync= 1;

  /* Maria requires that we always have a page cache */
  if (maria_init() ||
      (init_pagecache(maria_pagecache, pagecache_size, 0, 0,
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
    fprintf(stderr, "Error in initialization");
    exit(1);
  }
  if (opt_versioning)
    init_thr_lock();

  reclength=STANDARD_LENGTH+60+(use_blob ? 8 : 0);
  blob_pos=STANDARD_LENGTH+60;
  keyinfo[0].seg= &glob_keyseg[0][0];
  keyinfo[0].seg[0].start=0;
  keyinfo[0].seg[0].length=6;
  keyinfo[0].seg[0].type=HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].language= default_charset_info->number;
  keyinfo[0].seg[0].flag=(uint8) pack_seg;
  keyinfo[0].seg[0].null_bit=0;
  keyinfo[0].seg[0].null_pos=0;
  keyinfo[0].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[0].keysegs=1;
  keyinfo[0].flag = pack_type;
  keyinfo[0].block_length= 0;                   /* Default block length */
  keyinfo[1].seg= &glob_keyseg[1][0];
  keyinfo[1].seg[0].start=7;
  keyinfo[1].seg[0].length=6;
  keyinfo[1].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[1].seg[0].flag=0;
  keyinfo[1].seg[0].null_bit=0;
  keyinfo[1].seg[0].null_pos=0;
  keyinfo[1].seg[1].start=0;			/* two part key */
  keyinfo[1].seg[1].length=6;
  keyinfo[1].seg[1].type=HA_KEYTYPE_NUM;
  keyinfo[1].seg[1].flag=HA_REVERSE_SORT;
  keyinfo[1].seg[1].null_bit=0;
  keyinfo[1].seg[1].null_pos=0;
  keyinfo[1].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[1].keysegs=2;
  keyinfo[1].flag =0;
  keyinfo[1].block_length= MARIA_MIN_KEY_BLOCK_LENGTH;  /* Diff blocklength */
  keyinfo[2].seg= &glob_keyseg[2][0];
  keyinfo[2].seg[0].start=12;
  keyinfo[2].seg[0].length=8;
  keyinfo[2].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[2].seg[0].flag=HA_REVERSE_SORT;
  keyinfo[2].seg[0].null_bit=0;
  keyinfo[2].seg[0].null_pos=0;
  keyinfo[2].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[2].keysegs=1;
  keyinfo[2].flag =HA_NOSAME;
  keyinfo[2].block_length= 0;                   /* Default block length */
  keyinfo[3].seg= &glob_keyseg[3][0];
  keyinfo[3].seg[0].start=0;
  keyinfo[3].seg[0].length=reclength-(use_blob ? 8 : 0);
  keyinfo[3].seg[0].type=HA_KEYTYPE_TEXT;
  keyinfo[3].seg[0].language=default_charset_info->number;
  keyinfo[3].seg[0].flag=(uint8) pack_seg;
  keyinfo[3].seg[0].null_bit=0;
  keyinfo[3].seg[0].null_pos=0;
  keyinfo[3].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[3].keysegs=1;
  keyinfo[3].flag = pack_type;
  keyinfo[3].block_length= 0;                   /* Default block length */
  keyinfo[4].seg= &glob_keyseg[4][0];
  keyinfo[4].seg[0].start=0;
  keyinfo[4].seg[0].length=5;
  keyinfo[4].seg[0].type=HA_KEYTYPE_TEXT;
  keyinfo[4].seg[0].language=default_charset_info->number;
  keyinfo[4].seg[0].flag=0;
  keyinfo[4].seg[0].null_bit=0;
  keyinfo[4].seg[0].null_pos=0;
  keyinfo[4].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[4].keysegs=1;
  keyinfo[4].flag = pack_type;
  keyinfo[4].block_length= 0;                   /* Default block length */
  keyinfo[5].seg= &glob_keyseg[5][0];
  keyinfo[5].seg[0].start=0;
  keyinfo[5].seg[0].length=4;
  keyinfo[5].seg[0].type=HA_KEYTYPE_TEXT;
  keyinfo[5].seg[0].language=default_charset_info->number;
  keyinfo[5].seg[0].flag=pack_seg;
  keyinfo[5].seg[0].null_bit=0;
  keyinfo[5].seg[0].null_pos=0;
  keyinfo[5].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[5].keysegs=1;
  keyinfo[5].flag = pack_type;
  keyinfo[5].block_length= 0;                   /* Default block length */

  recinfo[0].type=pack_fields ? FIELD_SKIP_PRESPACE : 0;
  recinfo[0].length=7;
  recinfo[0].null_bit=0;
  recinfo[0].null_pos=0;
  recinfo[1].type=pack_fields ? FIELD_SKIP_PRESPACE : 0;
  recinfo[1].length=5;
  recinfo[1].null_bit=0;
  recinfo[1].null_pos=0;
  recinfo[2].type=pack_fields ? FIELD_SKIP_PRESPACE : 0;
  recinfo[2].length=9;
  recinfo[2].null_bit=0;
  recinfo[2].null_pos=0;
  recinfo[3].type=FIELD_NORMAL;
  recinfo[3].length=STANDARD_LENGTH-7-5-9-4;
  recinfo[3].null_bit=0;
  recinfo[3].null_pos=0;
  recinfo[4].type=pack_fields ? FIELD_SKIP_ZERO : 0;
  recinfo[4].length=4;
  recinfo[4].null_bit=0;
  recinfo[4].null_pos=0;
  recinfo[5].type=pack_fields ? FIELD_SKIP_ENDSPACE : 0;
  recinfo[5].length=60;
  recinfo[5].null_bit=0;
  recinfo[5].null_pos=0;
  if (use_blob)
  {
    recinfo[6].type=FIELD_BLOB;
    recinfo[6].length=4+portable_sizeof_char_ptr;
    recinfo[6].null_bit=0;
    recinfo[6].null_pos=0;
  }

  write_count=update=dupp_keys=opt_delete=0;
  blob_buffer=0;

  for (i=1000 ; i>0 ; i--) key1[i]=0;
  for (i=5000 ; i>0 ; i--) key3[i]=0;

  if (!silent)
    printf("- Creating maria-file\n");
  file= 0;
  bzero((char*) &create_info,sizeof(create_info));
  create_info.max_rows=(ha_rows) (rec_pointer_size ?
				  (1L << (rec_pointer_size*8))/
				  reclength : 0);
  create_info.reloc_rows=(ha_rows) 100;
  create_info.transactional= transactional;
  if (maria_create(filename, record_type, keys,&keyinfo[first_key],
		use_blob ? 7 : 6, &recinfo[0],
		0,(MARIA_UNIQUEDEF*) 0,
		&create_info,create_flag))
    goto err;
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
  if (do_locking)
    maria_lock_database(file,F_WRLCK);
  if (write_cacheing)
    maria_extra(file,HA_EXTRA_WRITE_CACHE,0);
  if (opt_quick_mode)
    maria_extra(file,HA_EXTRA_QUICK,0);

  for (i=0 ; i < recant ; i++)
  {
    ulong blob_length;
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf((char*) record,"%6d:%4d:%8d:Pos: %4d    ",n1,n2,n3,write_count);
    int4store(record+STANDARD_LENGTH-4,(long) i);
    fix_length(record,(uint) STANDARD_LENGTH+rnd(60));
    put_blob_in_record(record+blob_pos,&blob_buffer, &blob_length);
    DBUG_PRINT("test",("record: %d  blob_length: %lu", i, blob_length));

    if (maria_write(file,record))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0)
      {
	printf("Error: %d in write at record: %d\n",my_errno,i);
	goto err;
      }
      if (verbose) printf("   Double key: %d at record# %d\n", n3, i);
    }
    else
    {
      if (key3[n3] == 1 && first_key <3 && first_key+keys >= 3)
      {
	printf("Error: Didn't get error when writing second key: '%8d'\n",n3);
	goto err2;
      }
      write_count++; key1[n1]++; key3[n3]=1;
    }

    /* Check if we can find key without flushing database */
    if (i % 10 == 0)
    {
      for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
      if (!j)
	for (j=999 ; j>0 && key1[j] == 0 ; j--) ;
      sprintf((char*) key,"%6d",j);
      if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("Test in loop: Can't find key: \"%s\"\n",key);
	goto err;
      }
    }
  }
  if (checkpoint == 2 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (write_cacheing)
  {
    if (maria_extra(file,HA_EXTRA_NO_CACHE,0))
    {
      puts("got error from maria_extra(HA_EXTRA_NO_CACHE)");
      goto err;
    }
  }

  if (testflag == 2)
    goto end;

#ifdef REMOVE_WHEN_WE_HAVE_RESIZE
  if (pagecacheing)
    resize_pagecache(maria_pagecache, maria_block_size,
                     pagecache_size * 2, 0, 0);
#endif
  if (!silent)
    printf("- Delete\n");
  if (srand_arg)
    srand(srand_arg);
  if (!update_count)
    update_count= recant/10;

  for (i=0 ; i < update_count ; i++)
  {
    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf((char*) key,"%6d",j);
      if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      if (bcmp(read_record+keyinfo[0].seg[0].start,
               key, keyinfo[0].seg[0].length))
      {
	printf("Found wrong record when searching for key: \"%s\"\n",key);
	goto err2;
      }
      if (opt_delete == (uint) remove_count)		/* While testing */
	goto end;
      if (maria_delete(file,read_record))
      {
	printf("error: %d; can't delete record: \"%s\"\n", my_errno,read_record);
	goto err;
      }
      opt_delete++;
      key1[atoi((char*) read_record+keyinfo[0].seg[0].start)]--;
      key3[atoi((char*) read_record+keyinfo[2].seg[0].start)]=0;
    }
    else
    {
      puts("Warning: Skipping delete test because no dupplicate keys");
      break;
    }
  }
  if (testflag == 3)
    goto end;
  if (checkpoint == 3 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (!silent)
    printf("- Update\n");
  if (srand_arg)
    srand(srand_arg);
  if (!update_count)
    update_count= recant/10;

  for (i=0 ; i < update_count ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf((char*) record2,"%6d:%4d:%8d:XXX: %4d     ",n1,n2,n3,update);
    int4store(record2+STANDARD_LENGTH-4,(long) i);
    fix_length(record2,(uint) STANDARD_LENGTH+rnd(60));

    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf((char*) key,"%6d",j);
      if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n", (char*) key);
	goto err;
      }
      if (bcmp(read_record+keyinfo[0].seg[0].start,
               key, keyinfo[0].seg[0].length))
      {
	printf("Found wrong record when searching for key: \"%s\"; Found \"%.*s\"\n",
               key, keyinfo[0].seg[0].length,
               read_record+keyinfo[0].seg[0].start);
	goto err2;
      }
      if (use_blob)
      {
        ulong blob_length;
	if (i & 1)
	  put_blob_in_record(record2+blob_pos,&blob_buffer, &blob_length);
	else
	  bmove(record2+blob_pos, read_record+blob_pos, 4 + sizeof(char*));
      }
      if (skip_update)
        continue;
      if (maria_update(file,read_record,record2))
      {
	if (my_errno != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0)
	{
	  printf("error: %d; can't update:\nFrom: \"%s\"\nTo:   \"%s\"\n",
		 my_errno,read_record,record2);
	  goto err;
	}
	if (verbose)
	  printf("Double key when tried to update:\nFrom: \"%s\"\nTo:   \"%s\"\n",record,record2);
      }
      else
      {
	key1[atoi((char*) read_record+keyinfo[0].seg[0].start)]--;
	key3[atoi((char*) read_record+keyinfo[2].seg[0].start)]=0;
	key1[n1]++; key3[n3]=1;
	update++;
      }
    }
  }
  if (testflag == 4)
    goto end;
  if (checkpoint == 4 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  for (i=999, dupp_keys=j=0 ; i>0 ; i--)
  {
    if (key1[i] > dupp_keys)
    {
      dupp_keys=key1[i]; j=i;
    }
  }
  sprintf((char*) key,"%6d",j);
  start=keyinfo[0].seg[0].start;
  length=keyinfo[0].seg[0].length;
  if (dupp_keys)
  {
    if (!silent)
      printf("- Same key: first - next -> last - prev -> first\n");
    DBUG_PRINT("progpos",("first - next -> last - prev -> first"));
    if (verbose) printf("	 Using key: \"%s\"  Keys: %d\n",key,dupp_keys);

    if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (maria_rsame(file,read_record2,-1))
      goto err;
    if (memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("maria_rsame didn't find same record\n");
      goto err2;
    }
    info.recpos=maria_position(file);
    if (maria_rfirst(file,read_record2,0) ||
	maria_rsame_with_pos(file,read_record2,0,info.recpos) ||
	memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("maria_rsame_with_pos didn't find same record\n");
      goto err2;
    }
    {
      int skr;
      info.recpos= maria_position(file);
      skr= maria_rnext(file,read_record2,0);
      if ((skr && my_errno != HA_ERR_END_OF_FILE) ||
	  maria_rprev(file,read_record2,0) ||
	  memcmp(read_record,read_record2,reclength) != 0 ||
          info.recpos != maria_position(file))
      {
	printf("maria_rsame_with_pos lost position\n");
	goto err;
      }
    }
    ant=1;
    while (maria_rnext(file,read_record2,0) == 0 &&
	   memcmp(read_record2+start,key,length) == 0) ant++;
    if (ant != dupp_keys)
    {
      printf("next: Found: %d keys of %d\n",ant,dupp_keys);
      goto err2;
    }
    ant=0;
    while (maria_rprev(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys)
    {
      printf("prev: Found: %d records of %d\n",ant,dupp_keys);
      goto err2;
    }

    /* Check of maria_rnext_same */
    if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    ant=1;
    while (!maria_rnext_same(file,read_record3) && ant < dupp_keys+10)
      ant++;
    if (ant != dupp_keys || my_errno != HA_ERR_END_OF_FILE)
    {
      printf("maria_rnext_same: Found: %d records of %d\n",ant,dupp_keys);
      goto err2;
    }
  }

  if (!silent)
    printf("- All keys: first - next -> last - prev -> first\n");
  DBUG_PRINT("progpos",("All keys: first - next -> last - prev -> first"));
  ant=1;
  if (maria_rfirst(file,read_record,0))
  {
    printf("Can't find first record\n");
    goto err;
  }
  while ((error=maria_rnext(file,read_record3,0)) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - opt_delete || error != HA_ERR_END_OF_FILE)
  {
    printf("next: I found: %d records of %d (error: %d)\n",
	   ant, write_count - opt_delete, error);
    goto err;
  }
  if (maria_rlast(file,read_record2,0) ||
      bcmp(read_record2,read_record3,reclength))
  {
    printf("Can't find last record\n");
    DBUG_DUMP("record2", read_record2, reclength);
    DBUG_DUMP("record3", read_record3, reclength);
    goto err2;
  }
  ant=1;
  while (maria_rprev(file,read_record3,0) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - opt_delete)
  {
    printf("prev: I found: %d records of %d\n",ant,write_count);
    goto err2;
  }
  if (bcmp(read_record,read_record3,reclength))
  {
    printf("Can't find first record\n");
    goto err2;
  }

  if (!silent)
    printf("- Test if: Read first - next - prev - prev - next == first\n");
  DBUG_PRINT("progpos",("- Read first - next - prev - prev - next == first"));
  if (maria_rfirst(file,read_record,0) ||
      maria_rnext(file,read_record3,0) ||
      maria_rprev(file,read_record3,0) ||
      maria_rprev(file,read_record3,0) == 0 ||
      maria_rnext(file,read_record3,0))
      goto err;
  if (bcmp(read_record,read_record3,reclength) != 0)
     printf("Can't find first record\n");

  if (!silent)
    printf("- Test if: Read last - prev - next - next - prev == last\n");
  DBUG_PRINT("progpos",("Read last - prev - next - next - prev == last"));
  if (maria_rlast(file,read_record2,0) ||
      maria_rprev(file,read_record3,0) ||
      maria_rnext(file,read_record3,0) ||
      maria_rnext(file,read_record3,0) == 0 ||
      maria_rprev(file,read_record3,0))
      goto err;
  if (bcmp(read_record2,read_record3,reclength))
     printf("Can't find last record\n");
#ifdef NOT_ANYMORE
  if (!silent)
    puts("- Test read key-part");
  strmov(key2,key);
  for(i=strlen(key2) ; i-- > 1 ;)
  {
    key2[i]=0;

    /* The following row is just to catch some bugs in the key code */
    bzero((char*) file->lastkey,file->s->base.max_key_length*2);
    if (maria_rkey(file,read_record,0,key2,(uint) i,HA_READ_PREFIX))
      goto err;
    if (bcmp(read_record+start,key,(uint) i))
    {
      puts("Didn't find right record");
      goto err2;
    }
  }
#endif
  if (dupp_keys > 2)
  {
    if (!silent)
      printf("- Read key (first) - next - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - next - delete - next -> last"));
    if (maria_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (maria_rnext(file,read_record3,0)) goto err;
    if (maria_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    while (maria_rnext(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-1)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-1);
      goto err2;
    }
  }
  if (dupp_keys>4)
  {
    if (!silent)
      printf("- Read last of key - prev - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - prev - delete - prev -> first"));
    if (maria_rprev(file,read_record3,0)) goto err;
    if (maria_rprev(file,read_record3,0)) goto err;
    if (maria_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    while (maria_rprev(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-2)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-2);
      goto err2;
    }
  }
  if (dupp_keys > 6)
  {
    if (!silent)
      printf("- Read first - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - delete - next -> last"));
    if (maria_rkey(file,read_record3,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (maria_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    if (maria_rnext(file,read_record,0))
      goto err;					/* Skall finnas poster */
    while (maria_rnext(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-3)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-3);
      goto err2;
    }

    if (!silent)
      printf("- Read last - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - delete - prev -> first"));
    if (maria_rprev(file,read_record3,0)) goto err;
    if (maria_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=0;
    while (maria_rprev(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-4)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-4);
      goto err2;
    }
  }

  if (!silent)
    puts("- Test if: Read rrnd - same");
  DBUG_PRINT("progpos",("Read rrnd - same"));
  assert(maria_scan_init(file) == 0);
  for (i=0 ; i < write_count ; i++)
  {
    int tmp;
    if ((tmp= maria_scan(file,read_record)) &&
        tmp != HA_ERR_END_OF_FILE &&
        tmp != HA_ERR_RECORD_DELETED)
    {
      printf("Got error %d when scanning table\n", tmp);
      break;
    }
    if (!tmp)
    {
      /* Remember position to last found row */
      info.recpos= maria_position(file);
      bmove(read_record2,read_record,reclength);
    }
  }
  maria_scan_end(file);
  if (i != write_count && i != write_count - opt_delete)
  {
    printf("Found wrong number of rows while scanning table\n");
    goto err2;
  }

  if (maria_rsame_with_pos(file,read_record,0,info.recpos))
    goto err;
  if (bcmp(read_record,read_record2,reclength) != 0)
  {
    printf("maria_rsame_with_pos didn't find same record\n");
    goto err2;
  }

  for (i=min(2,keys) ; i-- > 0 ;)
  {
    if (maria_rsame(file,read_record2,(int) i)) goto err;
    if (bcmp(read_record,read_record2,reclength) != 0)
    {
      printf("maria_rsame didn't find same record\n");
      goto err2;
    }
  }
  if (!silent)
    puts("- Test maria_records_in_range");
  maria_status(file,&info,HA_STATUS_VARIABLE);
  for (i=0 ; i < info.keys ; i++)
  {
    key_range min_key, max_key;
    if (maria_rfirst(file,read_record,(int) i) ||
	maria_rlast(file,read_record2,(int) i))
      goto err;
    copy_key(file,(uint) i, read_record,  key);
    copy_key(file,(uint) i, read_record2, key2);
    min_key.key= key;
    min_key.keypart_map= HA_WHOLE_KEY;
    min_key.flag= HA_READ_KEY_EXACT;
    max_key.key= key2;
    max_key.keypart_map= HA_WHOLE_KEY;
    max_key.flag= HA_READ_AFTER_KEY;

    range_records= maria_records_in_range(file,(int) i, &min_key, &max_key);
    if (range_records < info.records*8/10 ||
	range_records > info.records*12/10)
    {
      printf("maria_records_range returned %ld; Should be about %ld\n",
	     (long) range_records,(long) info.records);
      goto err2;
    }
    if (verbose)
    {
      printf("maria_records_range returned %ld;  Exact is %ld  (diff: %4.2g %%)\n",
	     (long) range_records, (long) info.records,
	     labs((long) range_records - (long) info.records)*100.0/
	     info.records);
    }
  }
  for (i=0 ; i < 5 ; i++)
  {
    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    for (k=rnd(1000)+1 ; k>0 && key1[k] == 0 ; k--) ;
    if (j != 0 && k != 0)
    {
      key_range min_key, max_key;
      if (j > k)
	swap_variables(int, j, k);
      sprintf((char*) key,"%6d",j);
      sprintf((char*) key2,"%6d",k);

      min_key.key= key;
      min_key.keypart_map= HA_WHOLE_KEY;
      min_key.flag= HA_READ_AFTER_KEY;
      max_key.key= key2;
      max_key.keypart_map= HA_WHOLE_KEY;
      max_key.flag= HA_READ_BEFORE_KEY;
      range_records= maria_records_in_range(file, 0, &min_key, &max_key);
      records=0;
      for (j++ ; j < k ; j++)
	records+=key1[j];
      if ((long) range_records < (long) records*7/10-2 ||
	  (long) range_records > (long) records*14/10+2)
      {
	printf("maria_records_range for key: %d returned %lu; Should be about %lu\n",
	       i, (ulong) range_records, (ulong) records);
	goto err2;
      }
      if (verbose && records)
      {
	printf("maria_records_range returned %lu;  Exact is %lu  (diff: %4.2g %%)\n",
	       (ulong) range_records, (ulong) records,
	       labs((long) range_records-(long) records)*100.0/records);

      }
    }
    }

  if (!silent)
    printf("- maria_info\n");
  maria_status(file,&info,HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (info.records != write_count-opt_delete ||
      info.deleted > opt_delete + update || info.keys != keys)
  {
    puts("Wrong info from maria_info");
    printf("Got: records: %lu  delete: %lu  i_keys: %d\n",
	   (ulong) info.records, (ulong) info.deleted, info.keys);
    goto err2;
  }
  if (verbose)
  {
    char buff[80];
    get_date(buff,3,info.create_time);
    printf("info: Created %s\n",buff);
    get_date(buff,3,info.check_time);
    printf("info: checked %s\n",buff);
    get_date(buff,3,info.update_time);
    printf("info: Modified %s\n",buff);
  }

  maria_panic(HA_PANIC_WRITE);
  maria_panic(HA_PANIC_READ);
  if (maria_is_changed(file))
    puts("Warning: maria_is_changed reported that datafile was changed");

  if (!silent)
    printf("- maria_extra(CACHE) + maria_rrnd.... + maria_extra(NO_CACHE)\n");
  if (maria_reset(file) || maria_extra(file,HA_EXTRA_CACHE,0))
  {
    if (do_locking || (!use_blob && !pack_fields))
    {
      puts("got error from maria_extra(HA_EXTRA_CACHE)");
      goto err;
    }
  }
  ant=0;
  assert(maria_scan_init(file) == 0);
  while ((error= maria_scan(file,record)) != HA_ERR_END_OF_FILE &&
	 ant < write_count + 10)
    ant+= error ? 0 : 1;
  maria_scan_end(file);
  if (ant != write_count-opt_delete)
  {
    printf("scan with cache: I can only find: %d records of %d\n",
	   ant,write_count-opt_delete);
    maria_scan_end(file);
    goto err2;
  }
  if (maria_extra(file,HA_EXTRA_NO_CACHE,0))
  {
    puts("got error from maria_extra(HA_EXTRA_NO_CACHE)");
    maria_scan_end(file);
    goto err;
  }
  maria_scan_end(file);

  ant=0;
  maria_scan_init(file);
  while ((error=maria_scan(file,record)) != HA_ERR_END_OF_FILE &&
	 ant < write_count + 10)
	ant+= error ? 0 : 1;
  if (ant != write_count-opt_delete)
  {
    printf("scan with cache: I can only find: %d records of %d\n",
	   ant,write_count-opt_delete);
    maria_scan_end(file);
    goto err2;
  }
  maria_scan_end(file);

  if (testflag == 5)
    goto end;
  if (checkpoint == 5 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

  if (!silent)
    printf("- Removing keys\n");
  DBUG_PRINT("progpos",("Removing keys"));
  lastpos = HA_OFFSET_ERROR;
  /* DBUG_POP(); */
  maria_reset(file);
  found_parts=0;
  maria_scan_init(file);
  while ((error= maria_scan(file,read_record)) != HA_ERR_END_OF_FILE)
  {
    info.recpos=maria_position(file);
    if (lastpos >= info.recpos && lastpos != HA_OFFSET_ERROR)
    {
      printf("maria_rrnd didn't advance filepointer; old: %ld, new: %ld\n",
	     (long) lastpos, (long) info.recpos);
      goto err2;
    }
    lastpos=info.recpos;
    if (error == 0)
    {
      if (opt_delete == (uint) remove_count)		/* While testing */
	goto end;
      if (rnd(2) == 1 && maria_rsame(file,read_record,-1))
      {
	printf("can't find record %lx\n",(long) info.recpos);
	goto err;
      }
      if (use_blob)
      {
	ulong blob_length,pos;
	uchar *ptr;
	memcpy(&ptr, read_record+blob_pos+4, sizeof(ptr));
        blob_length= uint4korr(read_record+blob_pos);
	for (pos=0 ; pos < blob_length ; pos++)
	{
	  if (ptr[pos] != (uchar) (blob_length+pos))
	  {
	    printf("Found blob with wrong info at %ld\n",(long) lastpos);
            maria_scan_end(file);
            my_errno= 0;
	    goto err2;
	  }
	}
      }
      if (maria_delete(file,read_record))
      {
	printf("can't delete record: %6.6s, delete_count: %d\n",
	       read_record, opt_delete);
        maria_scan_end(file);
	goto err;
      }
      opt_delete++;
    }
    else
      found_parts++;
  }
  if (my_errno != HA_ERR_END_OF_FILE && my_errno != HA_ERR_RECORD_DELETED)
    printf("error: %d from maria_rrnd\n",my_errno);
  if (write_count != opt_delete)
  {
    printf("Deleted only %d of %d records (%d parts)\n",opt_delete,write_count,
	   found_parts);
    maria_scan_end(file);
    goto err2;
  }
  if (testflag == 6)
    goto end;
  if (checkpoint == 6 && ma_checkpoint_execute(CHECKPOINT_MEDIUM, FALSE))
    goto err;

end:
  maria_scan_end(file);
  if (die_in_middle_of_transaction)
  {
    /* As commit record is not done, UNDO entries needs to be rolled back */
    switch (die_in_middle_of_transaction) {
    case 1:
      /*
        Flush changed data and index pages go to disk
        That will also flush log. Recovery will skip REDOs and apply UNDOs.
      */
      _ma_flush_table_files(file, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                            FLUSH_RELEASE, FLUSH_RELEASE);
      break;
    case 2:
      /*
        Just flush log. Pages are likely to not be on disk. Recovery will
        then execute REDOs and UNDOs.
      */
      if (translog_flush(file->trn->undo_lsn))
        goto err;
      break;
    case 3:
      /*
        Flush nothing. Pages and log are likely to not be on disk. Recovery
        will then do nothing.
      */
      break;
    case 4:
      /*
        Flush changed data pages go to disk. Changed index pages are not
        flushed. Recovery will skip some REDOs and apply UNDOs.
      */
      _ma_flush_table_files(file, MARIA_FLUSH_DATA, FLUSH_RELEASE,
                            FLUSH_RELEASE);
      /*
        We have to flush log separately as the redo for the last key page
        may not be flushed
      */
      if (translog_flush(file->trn->undo_lsn))
        goto err;
      break;
    }
    printf("Dying on request without maria_commit()/maria_close()\n");
    exit(0);
  }
  if (maria_commit(file))
    goto err;
  if (maria_close(file))
  {
    file= 0;
    goto err;
  }
  file= 0;
  maria_panic(HA_PANIC_CLOSE);			/* Should close log */
  if (!silent)
  {
    printf("\nFollowing test have been made:\n");
    printf("Write records: %d\nUpdate records: %d\nSame-key-read: %d\nDelete records: %d\n", write_count,update,dupp_keys,opt_delete);
    if (rec_pointer_size)
      printf("Record pointer size:  %d\n",rec_pointer_size);
    printf("maria_block_size:    %lu\n", maria_block_size);
    if (write_cacheing)
      puts("Key cache resized");
    if (write_cacheing)
      puts("Write cacheing used");
    if (write_cacheing)
      puts("quick mode");
    if (async_io && do_locking)
      puts("Asyncron io with locking used");
    else if (do_locking)
      puts("Locking used");
    if (use_blob)
      puts("blobs used");
    printf("key cache status: \n\
blocks used:%10lu\n\
not flushed:%10lu\n\
w_requests: %10lu\n\
writes:     %10lu\n\
r_requests: %10lu\n\
reads:      %10lu\n",
           maria_pagecache->blocks_used,
           maria_pagecache->global_blocks_changed,
           (ulong) maria_pagecache->global_cache_w_requests,
           (ulong) maria_pagecache->global_cache_write,
           (ulong) maria_pagecache->global_cache_r_requests,
           (ulong) maria_pagecache->global_cache_read);
  }
  maria_end();
  my_free(blob_buffer);
  my_end(silent ? MY_CHECK_ERROR : MY_CHECK_ERROR | MY_GIVE_INFO);
  return(0);
err:
  printf("got error: %d when using MARIA-database\n",my_errno);
err2:
  if (file)
  {
    if (maria_commit(file))
      printf("got error: %d when using MARIA-database\n",my_errno);
    maria_close(file);
  }
  maria_end();
  return(1);
} /* main */


/* Read options */

static void get_options(int argc, char **argv)
{
  char *pos,*progname;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'B':
      pack_type= HA_BINARY_PACK_KEY;
      break;
    case 'b':
      use_blob= 1000;
      if (*++pos)
        use_blob= atol(pos);
      break;
    case 'K':				/* Use key cacheing */
      pagecacheing=1;
      if (*++pos)
	pagecache_size=atol(pos);
      break;
    case 'W':				/* Use write cacheing */
      write_cacheing=1;
      if (*++pos)
	my_default_record_cache_size=atoi(pos);
      break;
    case 'd':
      remove_count= atoi(++pos);
      break;
    case 'i':
      if (*++pos)
	srand(srand_arg= atoi(pos));
      break;
    case 'L':
      do_locking=1;
      break;
    case 'a':				/* use asyncron io */
      async_io=1;
      if (*++pos)
	my_default_record_cache_size=atoi(pos);
      break;
    case 'v':				/* verbose */
      verbose=1;
      break;
    case 'm':				/* records */
      if ((recant=atoi(++pos)) < 10 && testflag > 2)
      {
	fprintf(stderr,"record count must be >= 10 (if testflag > 2)\n");
	exit(1);
      }
      break;
    case 'e':				/* maria_block_length */
    case 'E':
      if ((maria_block_size= atoi(++pos)) < MARIA_MIN_KEY_BLOCK_LENGTH ||
	  maria_block_size > MARIA_MAX_KEY_BLOCK_LENGTH)
      {
	fprintf(stderr,"Wrong maria_block_length\n");
	exit(1);
      }
      maria_block_size= my_round_up_to_next_power(maria_block_size);
      break;
    case 'f':
      if ((first_key=atoi(++pos)) < 0 || first_key >= MARIA_KEYS)
	first_key=0;
      break;
    case 'H':
      checkpoint= atoi(++pos);
      break;
    case 'h':
      maria_data_root= ++pos;
      break;
    case 'k':
      if ((keys=(uint) atoi(++pos)) < 1 ||
	   keys > (uint) (MARIA_KEYS-first_key))
	keys=MARIA_KEYS-first_key;
      break;
    case 'M':
      record_type= BLOCK_RECORD;
      break;
    case 'P':
      pack_type=0;			/* Don't use DIFF_LENGTH */
      pack_seg=0;
      break;
    case 'R':				/* Length of record pointer */
      rec_pointer_size=atoi(++pos);
      if (rec_pointer_size > 7)
	rec_pointer_size=0;
      break;
    case 'S':
      pack_fields=0;			/* Static-length-records */
      record_type= STATIC_RECORD;
      break;
    case 's':
      silent=1;
      break;
    case 't':
      testflag=atoi(++pos);		/* testmod */
      break;
    case 'T':
      transactional= 1;
      break;
    case 'A':
      die_in_middle_of_transaction= atoi(++pos);
      break;
    case 'u':
      update_count=atoi(++pos);
      if (!update_count)
        skip_update= 1;
      break;
    case 'q':
      opt_quick_mode=1;
      break;
    case 'c':
      create_flag|= HA_CREATE_CHECKSUM | HA_CREATE_PAGE_CHECKSUM;
      break;
    case 'D':
      create_flag|=HA_CREATE_DELAY_KEY_WRITE;
      break;
    case 'g':
      skip_update= TRUE;
      break;
    case 'C':
      opt_versioning= 1;
      break;
    case '?':
    case 'I':
    case 'V':
      printf("%s  Ver 1.2 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("By Monty, for testing Maria\n");
      printf("Usage: %s [-?AbBcCDIKLPRqSsTVWltv] [-k#] [-f#] [-m#] [-e#] [-E#] [-t#]\n",
	     progname);
      exit(0);
    case '#':
      DBUG_PUSH (++pos);
      break;
    default:
      printf("Illegal option: '%c'\n",*pos);
      break;
    }
  }
  return;
} /* get options */

	/* Get a random value 0 <= x <= n */

static uint rnd(uint max_value)
{
  return (uint) ((rand() & 32767)/32767.0*max_value);
} /* rnd */


	/* Create a variable length record */

static void fix_length(uchar *rec, uint length)
{
  bmove(rec+STANDARD_LENGTH,
	"0123456789012345678901234567890123456789012345678901234567890",
	length-STANDARD_LENGTH);
  strfill((char*) rec+length,STANDARD_LENGTH+60-length,' ');
} /* fix_length */


/* Put maybe a blob in record */

static int first_entry;

static void put_blob_in_record(uchar *blob_pos, char **blob_buffer,
                               ulong *blob_length)
{
  ulong i,length;
  *blob_length= 0;
  if (use_blob)
  {
    if (! *blob_buffer &&
        !(*blob_buffer=my_malloc((uint) use_blob,MYF(MY_WME))))
    {
      use_blob= 0;
      return;
    }
    if (rnd(10) == 0)
    {
      if (first_entry++ == 0)
      {
        /* Ensure we have at least one blob of max length in file */
        length= use_blob;
      }
      else
        length=rnd(use_blob);
      for (i=0 ; i < length ; i++)
	(*blob_buffer)[i]=(char) (length+i);
      int4store(blob_pos,length);
      memcpy(blob_pos+4, blob_buffer, sizeof(char*));
      *blob_length= length;
    }
    else
    {
      int4store(blob_pos,0);
    }
  }
  return;
}


static void copy_key(MARIA_HA *info,uint inx,uchar *rec,uchar *key_buff)
{
  HA_KEYSEG *keyseg;

  for (keyseg=info->s->keyinfo[inx].seg ; keyseg->type ; keyseg++)
  {
    memcpy(key_buff,rec+keyseg->start,(size_t) keyseg->length);
    key_buff+=keyseg->length;
  }
  return;
}

#include "ma_check_standalone.h"

