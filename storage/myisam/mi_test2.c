/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Test av isam-databas: stor test */

#ifdef DBUG_OFF
#undef DBUG_OFF
#endif
#include "myisamdef.h"
#include <m_ctype.h>
#include <my_bit.h>

#define STANDARD_LENGTH 37
#define MYISAM_KEYS 6
#define MAX_PARTS 4
#if !defined(labs)
#define labs(a) abs(a)
#endif

static void get_options(int argc, char *argv[]);
static uint rnd(uint max_value);
static void fix_length(uchar *record,uint length);
static void put_blob_in_record(uchar *blob_pos,char **blob_buffer);
static void copy_key(struct st_myisam_info *info,uint inx,
		     uchar *record,uchar *key);

static	int verbose=0,testflag=0,
	    first_key=0,async_io=0,key_cacheing=0,write_cacheing=0,locking=0,
            rec_pointer_size=0,pack_fields=1,use_log=0,silent=0,
            opt_quick_mode=0;
static int pack_seg=HA_SPACE_PACK,pack_type=HA_PACK_KEY,remove_count=-1,
	   create_flag=0;
static ulong key_cache_size=IO_SIZE*16;
static uint key_cache_block_size= KEY_CACHE_BLOCK_SIZE;

static uint keys=MYISAM_KEYS,recant=1000;
static uint use_blob=0;
static uint16 key1[1001],key3[5000];
static uchar record[300],record2[300],key[100],key2[100];
static uchar read_record[300],read_record2[300],read_record3[300];
static HA_KEYSEG glob_keyseg[MYISAM_KEYS][MAX_PARTS];

		/* Test program */

int main(int argc, char *argv[])
{
  uint i;
  int j,n1,n2,n3,error,k;
  uint write_count,update,dupp_keys,opt_delete,start,length,blob_pos,
       reclength,ant,found_parts;
  my_off_t lastpos;
  ha_rows range_records,records;
  MI_INFO *file;
  MI_KEYDEF keyinfo[10];
  MI_COLUMNDEF recinfo[10];
  MI_ISAMINFO info;
  const char *filename;
  char *blob_buffer;
  MI_CREATE_INFO create_info;
  MY_INIT(argv[0]);

  filename= "test2";
  get_options(argc,argv);

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
  keyinfo[1].block_length= MI_MIN_KEY_BLOCK_LENGTH;  /* Diff blocklength */
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
  for (i=4999 ; i>0 ; i--) key3[i]=0;

  if (!silent)
    printf("- Creating isam-file\n");
  /*  DBUG_PUSH(""); */
  /* my_delete(filename,MYF(0)); */	/* Remove old locks under gdb */
  file= 0;
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows=(ha_rows) (rec_pointer_size ?
				  (1L << (rec_pointer_size*8))/
				  reclength : 0);
  create_info.reloc_rows=(ha_rows) 100;
  if (mi_create(filename,keys,&keyinfo[first_key],
		use_blob ? 7 : 6, &recinfo[0],
		0,(MI_UNIQUEDEF*) 0,
		&create_info,create_flag))
    goto err;
  if (use_log)
    mi_log(1);
  if (!(file=mi_open(filename,2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;
  if (!silent)
    printf("- Writing key:s\n");
  if (key_cacheing)
    init_key_cache(dflt_key_cache,key_cache_block_size,key_cache_size,0,0);
  if (locking)
    mi_lock_database(file,F_WRLCK);
  if (write_cacheing)
    mi_extra(file,HA_EXTRA_WRITE_CACHE,0);
  if (opt_quick_mode)
    mi_extra(file,HA_EXTRA_QUICK,0);

  for (i=0 ; i < recant ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf((char*) record,"%6d:%4d:%8d:Pos: %4d    ",n1,n2,n3,write_count);
    int4store(record+STANDARD_LENGTH-4,(long) i);
    fix_length(record,(uint) STANDARD_LENGTH+rnd(60));
    put_blob_in_record(record+blob_pos,&blob_buffer);
    DBUG_PRINT("test",("record: %d",i));

    if (mi_write(file,record))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0)
      {
	printf("Error: %d in write at record: %d\n",my_errno,i);
	goto err;
      }
      if (verbose) printf("   Double key: %d\n",n3);
    }
    else
    {
      if (key3[n3] == 1 && first_key <3 && first_key+keys >= 3)
      {
	printf("Error: Didn't get error when writing second key: '%8d'\n",n3);
	goto err;
      }
      write_count++; key1[n1]++; key3[n3]=1;
    }

    /* Check if we can find key without flushing database */
    if (i == recant/2)
    {
      for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
      if (!j)
	for (j=999 ; j>0 && key1[j] == 0 ; j--) ;
      sprintf((char*) key,"%6d",j);
      if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("Test in loop: Can't find key: \"%s\"\n",key);
	goto err;
      }
    }
  }
  if (testflag==1) goto end;

  if (write_cacheing)
  {
    if (mi_extra(file,HA_EXTRA_NO_CACHE,0))
    {
      puts("got error from mi_extra(HA_EXTRA_NO_CACHE)");
      goto end;
    }
  }
  if (key_cacheing)
    resize_key_cache(dflt_key_cache,key_cache_block_size,key_cache_size*2,0,0);

  if (!silent)
    printf("- Delete\n");
  for (i=0 ; i<recant/10 ; i++)
  {
    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf((char*) key,"%6d",j);
      if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      if (opt_delete == (uint) remove_count)		/* While testing */
	goto end;
      if (mi_delete(file,read_record))
      {
	printf("error: %d; can't delete record: \"%s\"\n", my_errno,read_record);
	goto err;
      }
      opt_delete++;
      key1[atoi((char*) read_record+keyinfo[0].seg[0].start)]--;
      key3[atoi((char*) read_record+keyinfo[2].seg[0].start)]=0;
    }
    else
      puts("Warning: Skipping delete test because no dupplicate keys");
  }
  if (testflag==2) goto end;

  if (!silent)
    printf("- Update\n");
  for (i=0 ; i<recant/10 ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf((char*) record2,"%6d:%4d:%8d:XXX: %4d     ",n1,n2,n3,update);
    int4store(record2+STANDARD_LENGTH-4,(long) i);
    fix_length(record2,(uint) STANDARD_LENGTH+rnd(60));

    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf((char*) key,"%6d",j);
      if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n",(char*) key);
	goto err;
      }
      if (use_blob)
      {
	if (i & 1)
	  put_blob_in_record(record+blob_pos,&blob_buffer);
	else
	  memmove(record + blob_pos, read_record + blob_pos, 8);
      }
      if (mi_update(file,read_record,record2))
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
  if (testflag == 3)
    goto end;

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

    if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (mi_rsame(file,read_record2,-1))
      goto err;
    if (memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("mi_rsame didn't find same record\n");
      goto end;
    }
    info.recpos=mi_position(file);
    if (mi_rfirst(file,read_record2,0) ||
	mi_rsame_with_pos(file,read_record2,0,info.recpos) ||
	memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("mi_rsame_with_pos didn't find same record\n");
      goto end;
    }
    {
      int skr=mi_rnext(file,read_record2,0);
      if ((skr && my_errno != HA_ERR_END_OF_FILE) ||
	  mi_rprev(file,read_record2,-1) ||
	  memcmp(read_record,read_record2,reclength) != 0)
      {
	printf("mi_rsame_with_pos lost position\n");
	goto end;
      }
    }
    ant=1;
    while (mi_rnext(file,read_record2,0) == 0 &&
	   memcmp(read_record2+start,key,length) == 0) ant++;
    if (ant != dupp_keys)
    {
      printf("next: Found: %d keys of %d\n",ant,dupp_keys);
      goto end;
    }
    ant=0;
    while (mi_rprev(file,read_record3,0) == 0 &&
	   memcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys)
    {
      printf("prev: Found: %d records of %d\n",ant,dupp_keys);
      goto end;
    }

    /* Check of mi_rnext_same */
    if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    ant=1;
    while (!mi_rnext_same(file,read_record3) && ant < dupp_keys+10)
      ant++;
    if (ant != dupp_keys || my_errno != HA_ERR_END_OF_FILE)
    {
      printf("mi_rnext_same: Found: %d records of %d\n",ant,dupp_keys);
      goto end;
    }
  }

  if (!silent)
    printf("- All keys: first - next -> last - prev -> first\n");
  DBUG_PRINT("progpos",("All keys: first - next -> last - prev -> first"));
  ant=1;
  if (mi_rfirst(file,read_record,0))
  {
    printf("Can't find first record\n");
    goto end;
  }
  while ((error=mi_rnext(file,read_record3,0)) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - opt_delete || error != HA_ERR_END_OF_FILE)
  {
    printf("next: I found: %d records of %d (error: %d)\n",
	   ant, write_count - opt_delete, error);
    goto end;
  }
  if (mi_rlast(file,read_record2,0) ||
      memcmp(read_record2,read_record3,reclength))
  {
    printf("Can't find last record\n");
    DBUG_DUMP("record2",(uchar*) read_record2,reclength);
    DBUG_DUMP("record3",(uchar*) read_record3,reclength);
    goto end;
  }
  ant=1;
  while (mi_rprev(file,read_record3,0) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - opt_delete)
  {
    printf("prev: I found: %d records of %d\n",ant,write_count);
    goto end;
  }
  if (memcmp(read_record,read_record3,reclength))
  {
    printf("Can't find first record\n");
    goto end;
  }

  if (!silent)
    printf("- Test if: Read first - next - prev - prev - next == first\n");
  DBUG_PRINT("progpos",("- Read first - next - prev - prev - next == first"));
  if (mi_rfirst(file,read_record,0) ||
      mi_rnext(file,read_record3,0) ||
      mi_rprev(file,read_record3,0) ||
      mi_rprev(file,read_record3,0) == 0 ||
      mi_rnext(file,read_record3,0))
      goto err;
  if (memcmp(read_record,read_record3,reclength) != 0)
     printf("Can't find first record\n");

  if (!silent)
    printf("- Test if: Read last - prev - next - next - prev == last\n");
  DBUG_PRINT("progpos",("Read last - prev - next - next - prev == last"));
  if (mi_rlast(file,read_record2,0) ||
      mi_rprev(file,read_record3,0) ||
      mi_rnext(file,read_record3,0) ||
      mi_rnext(file,read_record3,0) == 0 ||
      mi_rprev(file,read_record3,0))
      goto err;
  if (memcmp(read_record2,read_record3,reclength))
     printf("Can't find last record\n");
  if (dupp_keys > 2)
  {
    if (!silent)
      printf("- Read key (first) - next - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - next - delete - next -> last"));
    if (mi_rkey(file,read_record,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (mi_rnext(file,read_record3,0)) goto err;
    if (mi_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    while (mi_rnext(file,read_record3,0) == 0 &&
	   memcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-1)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-1);
      goto end;
    }
  }
  if (dupp_keys>4)
  {
    if (!silent)
      printf("- Read last of key - prev - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - prev - delete - prev -> first"));
    if (mi_rprev(file,read_record3,0)) goto err;
    if (mi_rprev(file,read_record3,0)) goto err;
    if (mi_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    while (mi_rprev(file,read_record3,0) == 0 &&
	   memcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-2)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-2);
      goto end;
    }
  }
  if (dupp_keys > 6)
  {
    if (!silent)
      printf("- Read first - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - delete - next -> last"));
    if (mi_rkey(file,read_record3,0,key,HA_WHOLE_KEY,HA_READ_KEY_EXACT))
      goto err;
    if (mi_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=1;
    if (mi_rnext(file,read_record,0))
      goto err;					/* Skall finnas poster */
    while (mi_rnext(file,read_record3,0) == 0 &&
	   memcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-3)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-3);
      goto end;
    }

    if (!silent)
      printf("- Read last - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - delete - prev -> first"));
    if (mi_rprev(file,read_record3,0)) goto err;
    if (mi_delete(file,read_record3)) goto err;
    opt_delete++;
    ant=0;
    while (mi_rprev(file,read_record3,0) == 0 &&
	   memcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-4)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-4);
      goto end;
    }
  }

  if (!silent)
    puts("- Test if: Read rrnd - same");
  DBUG_PRINT("progpos",("Read rrnd - same"));
  for (i=0 ; i < write_count ; i++)
  {
    if (mi_rrnd(file,read_record,i == 0 ? 0L : HA_OFFSET_ERROR) == 0)
      break;
  }
  if (i == write_count)
    goto err;

  memmove(read_record2, read_record, reclength);
  for (i=min(2,keys) ; i-- > 0 ;)
  {
    if (mi_rsame(file,read_record2,(int) i)) goto err;
    if (memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("is_rsame didn't find same record\n");
      goto end;
    }
  }
  if (!silent)
    puts("- Test mi_records_in_range");
  mi_status(file,&info,HA_STATUS_VARIABLE);
  for (i=0 ; i < info.keys ; i++)
  {
    key_range min_key, max_key;
    if (mi_rfirst(file,read_record,(int) i) ||
	mi_rlast(file,read_record2,(int) i))
      goto err;
    copy_key(file,(uint) i,(uchar*) read_record,(uchar*) key);
    copy_key(file,(uint) i,(uchar*) read_record2,(uchar*) key2);
    min_key.key= key;
    min_key.keypart_map= HA_WHOLE_KEY;
    min_key.flag= HA_READ_KEY_EXACT;
    max_key.key= key2;
    max_key.keypart_map= HA_WHOLE_KEY;
    max_key.flag= HA_READ_AFTER_KEY;

    range_records= mi_records_in_range(file,(int) i, &min_key, &max_key);
    if (range_records < info.records*8/10 ||
	range_records > info.records*12/10)
    {
      printf("mi_records_range returned %ld; Should be about %ld\n",
	     (long) range_records,(long) info.records);
      goto end;
    }
    if (verbose)
    {
      printf("mi_records_range returned %ld;  Exact is %ld  (diff: %4.2g %%)\n",
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
      min_key.length= USE_WHOLE_KEY;
      min_key.flag= HA_READ_AFTER_KEY;
      max_key.key= key2;
      max_key.length= USE_WHOLE_KEY;
      max_key.flag= HA_READ_BEFORE_KEY;
      range_records= mi_records_in_range(file, 0, &min_key, &max_key);
      records=0;
      for (j++ ; j < k ; j++)
	records+=key1[j];
      if ((long) range_records < (long) records*7/10-2 ||
	  (long) range_records > (long) records*14/10+2)
      {
	printf("mi_records_range for key: %d returned %lu; Should be about %lu\n",
	       i, (ulong) range_records, (ulong) records);
	goto end;
      }
      if (verbose && records)
      {
	printf("mi_records_range returned %lu;  Exact is %lu  (diff: %4.2g %%)\n",
	       (ulong) range_records, (ulong) records,
	       labs((long) range_records-(long) records)*100.0/records);

      }
    }
    }

  if (!silent)
    printf("- mi_info\n");
  mi_status(file,&info,HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (info.records != write_count-opt_delete || info.deleted > opt_delete + update
      || info.keys != keys)
  {
    puts("Wrong info from mi_info");
    printf("Got: records: %lu  delete: %lu  i_keys: %d\n",
	   (ulong) info.records, (ulong) info.deleted, info.keys);
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

  mi_panic(HA_PANIC_WRITE);
  mi_panic(HA_PANIC_READ);
  if (mi_is_changed(file))
    puts("Warning: mi_is_changed reported that datafile was changed");

  if (!silent)
    printf("- mi_extra(CACHE) + mi_rrnd.... + mi_extra(NO_CACHE)\n");
  if (mi_reset(file) || mi_extra(file,HA_EXTRA_CACHE,0))
  {
    if (locking || (!use_blob && !pack_fields))
    {
      puts("got error from mi_extra(HA_EXTRA_CACHE)");
      goto end;
    }
  }
  ant=0;
  while ((error=mi_rrnd(file,record,HA_OFFSET_ERROR)) != HA_ERR_END_OF_FILE &&
	 ant < write_count + 10)
	ant+= error ? 0 : 1;
  if (ant != write_count-opt_delete)
  {
    printf("rrnd with cache: I can only find: %d records of %d\n",
	   ant,write_count-opt_delete);
    goto end;
  }
  if (mi_extra(file,HA_EXTRA_NO_CACHE,0))
  {
    puts("got error from mi_extra(HA_EXTRA_NO_CACHE)");
    goto end;
  }

  ant=0;
  mi_scan_init(file);
  while ((error=mi_scan(file,record)) != HA_ERR_END_OF_FILE &&
	 ant < write_count + 10)
	ant+= error ? 0 : 1;
  if (ant != write_count-opt_delete)
  {
    printf("scan with cache: I can only find: %d records of %d\n",
	   ant,write_count-opt_delete);
    goto end;
  }

  if (testflag == 4) goto end;

  if (!silent)
    printf("- Removing keys\n");
  DBUG_PRINT("progpos",("Removing keys"));
  lastpos = HA_OFFSET_ERROR;
  /* DBUG_POP(); */
  mi_reset(file);
  found_parts=0;
  while ((error=mi_rrnd(file,read_record,HA_OFFSET_ERROR)) !=
	 HA_ERR_END_OF_FILE)
  {
    info.recpos=mi_position(file);
    if (lastpos >= info.recpos && lastpos != HA_OFFSET_ERROR)
    {
      printf("mi_rrnd didn't advance filepointer; old: %ld, new: %ld\n",
	     (long) lastpos, (long) info.recpos);
      goto err;
    }
    lastpos=info.recpos;
    if (error == 0)
    {
      if (opt_delete == (uint) remove_count)		/* While testing */
	goto end;
      if (mi_rsame(file,read_record,-1))
      {
	printf("can't find record %lx\n",(long) info.recpos);
	goto err;
      }
      if (use_blob)
      {
	ulong blob_length,pos;
	uchar *ptr;
	longget(blob_length,read_record+blob_pos+4);
	ptr=(uchar*) blob_length;
	longget(blob_length,read_record+blob_pos);
	for (pos=0 ; pos < blob_length ; pos++)
	{
	  if (ptr[pos] != (uchar) (blob_length+pos))
	  {
	    printf("found blob with wrong info at %ld\n",(long) lastpos);
	    use_blob=0;
	    break;
	  }
	}
      }
      if (mi_delete(file,read_record))
      {
	printf("can't delete record: %6.6s,  delete_count: %d\n",
	       read_record, opt_delete);
	goto err;
      }
      opt_delete++;
    }
    else
      found_parts++;
  }
  if (my_errno != HA_ERR_END_OF_FILE && my_errno != HA_ERR_RECORD_DELETED)
    printf("error: %d from mi_rrnd\n",my_errno);
  if (write_count != opt_delete)
  {
    printf("Deleted only %d of %d records (%d parts)\n",opt_delete,write_count,
	   found_parts);
    goto err;
  }
end:
  if (mi_close(file))
    goto err;
  mi_panic(HA_PANIC_CLOSE);			/* Should close log */
  if (!silent)
  {
    printf("\nFollowing test have been made:\n");
    printf("Write records: %d\nUpdate records: %d\nSame-key-read: %d\nDelete records: %d\n", write_count,update,dupp_keys,opt_delete);
    if (rec_pointer_size)
      printf("Record pointer size:  %d\n",rec_pointer_size);
    printf("myisam_block_size:    %lu\n", myisam_block_size);
    if (key_cacheing)
    {
      puts("Key cache used");
      printf("key_cache_block_size: %u\n", key_cache_block_size);
      if (write_cacheing)
	puts("Key cache resized");
    }
    if (write_cacheing)
      puts("Write cacheing used");
    if (write_cacheing)
      puts("quick mode");
    if (async_io && locking)
      puts("Asyncron io with locking used");
    else if (locking)
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
           dflt_key_cache->blocks_used,
           dflt_key_cache->global_blocks_changed,
           (ulong) dflt_key_cache->global_cache_w_requests,
           (ulong) dflt_key_cache->global_cache_write,
           (ulong) dflt_key_cache->global_cache_r_requests,
           (ulong) dflt_key_cache->global_cache_read);
  }
  end_key_cache(dflt_key_cache,1);
  if (blob_buffer)
    my_free(blob_buffer);
  my_end(silent ? MY_CHECK_ERROR : MY_CHECK_ERROR | MY_GIVE_INFO);
  return(0);
err:
  printf("got error: %d when using MyISAM-database\n",my_errno);
  if (file)
    (void) mi_close(file);
  return(1);
} /* main */


	/* l{ser optioner */
	/* OBS! intierar endast DEBUG - ingen debuggning h{r ! */

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
      use_blob=1;
      break;
    case 'K':				/* Use key cacheing */
      key_cacheing=1;
      if (*++pos)
	key_cache_size=atol(pos);
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
	srand(atoi(pos));
      break;
    case 'l':
      use_log=1;
      break;
    case 'L':
      locking=1;
      break;
    case 'A':				/* use asyncron io */
      async_io=1;
      if (*++pos)
	my_default_record_cache_size=atoi(pos);
      break;
    case 'v':				/* verbose */
      verbose=1;
      break;
    case 'm':				/* records */
      if ((recant=atoi(++pos)) < 10)
      {
	fprintf(stderr,"record count must be >= 10\n");
	exit(1);
      }
      break;
    case 'e':				/* myisam_block_length */
      if ((myisam_block_size= atoi(++pos)) < MI_MIN_KEY_BLOCK_LENGTH ||
	  myisam_block_size > MI_MAX_KEY_BLOCK_LENGTH)
      {
	fprintf(stderr,"Wrong myisam_block_length\n");
	exit(1);
      }
      myisam_block_size= my_round_up_to_next_power(myisam_block_size);
      break;
    case 'E':				/* myisam_block_length */
      if ((key_cache_block_size=atoi(++pos)) < MI_MIN_KEY_BLOCK_LENGTH ||
	  key_cache_block_size > MI_MAX_KEY_BLOCK_LENGTH)
      {
	fprintf(stderr,"Wrong key_cache_block_size\n");
	exit(1);
      }
      key_cache_block_size= my_round_up_to_next_power(key_cache_block_size);
      break;
    case 'f':
      if ((first_key=atoi(++pos)) < 0 || first_key >= MYISAM_KEYS)
	first_key=0;
      break;
    case 'k':
      if ((keys=(uint) atoi(++pos)) < 1 ||
	   keys > (uint) (MYISAM_KEYS-first_key))
	keys=MYISAM_KEYS-first_key;
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
      break;
    case 's':
      silent=1;
      break;
    case 't':
      testflag=atoi(++pos);		/* testmod */
      break;
    case 'q':
      opt_quick_mode=1;
      break;
    case 'c':
      create_flag|= HA_CREATE_CHECKSUM;
      break;
    case 'D':
      create_flag|=HA_CREATE_DELAY_KEY_WRITE;
      break;
    case '?':
    case 'I':
    case 'V':
      printf("%s  Ver 1.2 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("By Monty, for your professional use\n");
      printf("Usage: %s [-?AbBcDIKLPRqSsVWltv] [-k#] [-f#] [-m#] [-e#] [-E#] [-t#]\n",
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
  memmove(rec + STANDARD_LENGTH,
          "0123456789012345678901234567890123456789012345678901234567890",
          length - STANDARD_LENGTH);
  strfill((char*) rec+length,STANDARD_LENGTH+60-length,' ');
} /* fix_length */


	/* Put maybe a blob in record */

static void put_blob_in_record(uchar *blob_pos, char **blob_buffer)
{
  ulong i,length;
  if (use_blob)
  {
    if (rnd(10) == 0)
    {
      if (! *blob_buffer &&
	  !(*blob_buffer=my_malloc((uint) use_blob,MYF(MY_WME))))
      {
	use_blob=0;
	return;
      }
      length=rnd(use_blob);
      for (i=0 ; i < length ; i++)
	(*blob_buffer)[i]=(char) (length+i);
      int4store(blob_pos,length);
      memcpy(blob_pos+4, blob_buffer, sizeof(char*));
    }
    else
    {
      int4store(blob_pos,0);
    }
  }
  return;
}


static void copy_key(MI_INFO *info,uint inx,uchar *rec,uchar *key_buff)
{
  HA_KEYSEG *keyseg;

  for (keyseg=info->s->keyinfo[inx].seg ; keyseg->type ; keyseg++)
  {
    memcpy(key_buff,rec+keyseg->start,(size_t) keyseg->length);
    key_buff+=keyseg->length;
  }
  return;
}

#include "mi_extrafunc.h"
