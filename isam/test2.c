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

/* Test av isam-databas: stor test */

#ifndef USE_MY_FUNC		/* We want to be able to dbug this !! */
#define USE_MY_FUNC
#endif
#ifdef DBUG_OFF
#undef DBUG_OFF
#endif
#ifndef SAFEMALLOC
#define SAFEMALLOC
#endif

#include "isamdef.h"

#define STANDAR_LENGTH 37
#define NISAM_KEYS 6
#if !defined(MSDOS) && !defined(labs)
#define labs(a) abs(a)
#endif

static void get_options(int argc, char *argv[]);
static uint rnd(uint max_value);
static void fix_length(byte *record,uint length);
static void put_blob_in_record(char *blob_pos,char **blob_buffer);
static void copy_key(struct st_isam_info *info,uint inx,
		     uchar *record,uchar *key);

static	int verbose=0,testflag=0,pack_type=HA_SPACE_PACK,
	    first_key=0,async_io=0,key_cacheing=0,write_cacheing=0,locking=0,
	    rec_pointer_size=0,pack_fields=1,use_log=0;
static uint keys=NISAM_KEYS,recant=1000;
static uint use_blob=0;
static uint16 key1[1000],key3[5000];
static char record[300],record2[300],key[100],key2[100],
	    read_record[300],read_record2[300],read_record3[300];


		/* Test program */

int main(argc,argv)
int argc;
char *argv[];
{
  uint i;
  int j,n1,n2,n3,error,k;
  uint write_count,update,dupp_keys,delete,start,length,blob_pos,
       reclength,ant;
  ulong lastpos,range_records,records;
  N_INFO *file;
  N_KEYDEF keyinfo[10];
  N_RECINFO recinfo[10];
  N_ISAMINFO info;
  char *filename,*blob_buffer;
  MY_INIT(argv[0]);

  filename= (char*) "test2.ISM";
  get_options(argc,argv);
  if (! async_io)
    my_disable_async_io=1;

  reclength=STANDAR_LENGTH+60+(use_blob ? 8 : 0);
  blob_pos=STANDAR_LENGTH+60;
  keyinfo[0].seg[0].base.start=0;
  keyinfo[0].seg[0].base.length=6;
  keyinfo[0].seg[0].base.type=HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].base.flag=(uint8) pack_type;
  keyinfo[0].seg[1].base.type=0;
  keyinfo[0].base.flag = (uint8) (pack_type ? HA_PACK_KEY : 0);
  keyinfo[1].seg[0].base.start=7;
  keyinfo[1].seg[0].base.length=6;
  keyinfo[1].seg[0].base.type=HA_KEYTYPE_BINARY;
  keyinfo[1].seg[0].base.flag=0;
  keyinfo[1].seg[1].base.start=0;			/* Tv}delad nyckel */
  keyinfo[1].seg[1].base.length=6;
  keyinfo[1].seg[1].base.type=HA_KEYTYPE_NUM;
  keyinfo[1].seg[1].base.flag=HA_REVERSE_SORT;
  keyinfo[1].seg[2].base.type=0;
  keyinfo[1].base.flag =0;
  keyinfo[2].seg[0].base.start=12;
  keyinfo[2].seg[0].base.length=8;
  keyinfo[2].seg[0].base.type=HA_KEYTYPE_BINARY;
  keyinfo[2].seg[0].base.flag=HA_REVERSE_SORT;
  keyinfo[2].seg[1].base.type=0;
  keyinfo[2].base.flag =HA_NOSAME;
  keyinfo[3].seg[0].base.start=0;
  keyinfo[3].seg[0].base.length=reclength-(use_blob ? 8 : 0);
  keyinfo[3].seg[0].base.type=HA_KEYTYPE_TEXT;
  keyinfo[3].seg[0].base.flag=(uint8) pack_type;
  keyinfo[3].seg[1].base.type=0;
  keyinfo[3].base.flag = (uint8) (pack_type ? HA_PACK_KEY : 0);
  keyinfo[4].seg[0].base.start=0;
  keyinfo[4].seg[0].base.length=5;
  keyinfo[4].seg[0].base.type=HA_KEYTYPE_TEXT;
  keyinfo[4].seg[0].base.flag=0;
  keyinfo[4].seg[1].base.type=0;
  keyinfo[4].base.flag = (uint8) (pack_type ? HA_PACK_KEY : 0);
  keyinfo[5].seg[0].base.start=0;
  keyinfo[5].seg[0].base.length=4;
  keyinfo[5].seg[0].base.type=HA_KEYTYPE_TEXT;
  keyinfo[5].seg[0].base.flag=(uint8) pack_type;
  keyinfo[5].seg[1].base.type=0;
  keyinfo[5].base.flag = (uint8) (pack_type ? HA_PACK_KEY : 0);

  recinfo[0].base.type=pack_fields ? FIELD_SKIPP_PRESPACE : 0;
  recinfo[0].base.length=7;
  recinfo[1].base.type=pack_fields ? FIELD_SKIPP_PRESPACE : 0;
  recinfo[1].base.length=5;
  recinfo[2].base.type=pack_fields ? FIELD_SKIPP_PRESPACE : 0;
  recinfo[2].base.length=9;
  recinfo[3].base.type=FIELD_NORMAL;
  recinfo[3].base.length=STANDAR_LENGTH-7-5-9-4;
  recinfo[4].base.type=pack_fields ? FIELD_SKIPP_ZERO : 0;
  recinfo[4].base.length=4;
  recinfo[5].base.type=pack_fields ? FIELD_SKIPP_ENDSPACE : 0;
  recinfo[5].base.length=60;
  if (use_blob)
  {
    recinfo[6].base.type=FIELD_BLOB;
    recinfo[6].base.length=4+sizeof(char*);	/* 4 byte ptr, 4 byte length */
    recinfo[7].base.type= FIELD_LAST;
  }
  else
    recinfo[6].base.type= FIELD_LAST;

  write_count=update=dupp_keys=delete=0;
  blob_buffer=0;

  for (i=999 ; i>0 ; i--) key1[i]=0;
  for (i=4999 ; i>0 ; i--) key3[i]=0;

  printf("- Creating isam-file\n");
  /*  DBUG_PUSH(""); */
  my_delete(filename,MYF(0));		/* Remove old locks under gdb */
  file= 0;
  if (nisam_create(filename,keys,&keyinfo[first_key],&recinfo[0],
		   (ulong) (rec_pointer_size ? (1L << (rec_pointer_size*8))/
			    reclength : 0),100l,0,0,0L))
    goto err;
  if (use_log)
    nisam_log(1);
  if (!(file=nisam_open(filename,2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;
  printf("- Writing key:s\n");
  if (key_cacheing)
    init_key_cache(IO_SIZE*16,(uint) IO_SIZE*4*10);	/* Use a small cache */
  if (locking)
    nisam_lock_database(file,F_WRLCK);
  if (write_cacheing)
    nisam_extra(file,HA_EXTRA_WRITE_CACHE);

  for (i=0 ; i < recant ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf(record,"%6d:%4d:%8d:Pos: %4d    ",n1,n2,n3,write_count);
    longstore(record+STANDAR_LENGTH-4,(long) i);
    fix_length(record,(uint) STANDAR_LENGTH+rnd(60));
    put_blob_in_record(record+blob_pos,&blob_buffer);
    DBUG_PRINT("test",("record: %d",i));

    if (nisam_write(file,record))
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
      for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
      if (!j)
	for (j=999 ; j>0 && key1[j] == 0 ; j--) ;
      sprintf(key,"%6d",j);
      if (nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT))
      {
	printf("Test in loop: Can't find key: \"%s\"\n",key);
	goto err;
      }
    }
  }
  if (testflag==1) goto end;

  if (write_cacheing)
    if (nisam_extra(file,HA_EXTRA_NO_CACHE))
    {
      puts("got error from nisam_extra(HA_EXTRA_NO_CACHE)");
      goto end;
    }

  printf("- Delete\n");
  for (i=0 ; i<recant/10 ; i++)
  {
    for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf(key,"%6d",j);
      if (nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      if (nisam_delete(file,read_record))
      {
	printf("error: %d; can't delete record: \"%s\"\n", my_errno,read_record);
	goto err;
      }
      delete++;
      key1[atoi(read_record+keyinfo[0].seg[0].base.start)]--;
      key3[atoi(read_record+keyinfo[2].seg[0].base.start)]=0;
    }
  }
  if (testflag==2) goto end;

  printf("- Update\n");
  for (i=0 ; i<recant/10 ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(5000);
    sprintf(record2,"%6d:%4d:%8d:XXX: %4d     ",n1,n2,n3,update);
    longstore(record2+STANDAR_LENGTH-4,(long) i);
    fix_length(record2,(uint) STANDAR_LENGTH+rnd(60));

    for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf(key,"%6d",j);
      if (nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      if (use_blob)
      {
	if (i & 1)
	  put_blob_in_record(record+blob_pos,&blob_buffer);
	else
	  bmove(record+blob_pos,read_record+blob_pos,8);
      }
      if (nisam_update(file,read_record,record2))
      {
	if (my_errno != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0)
	{
	  printf("error: %d; can't uppdate:\nFrom: \"%s\"\nTo:   \"%s\"\n",
		 my_errno,read_record,record2);
	  goto err;
	}
	if (verbose)
	  printf("Double key when tryed to uppdate:\nFrom: \"%s\"\nTo:   \"%s\"\n",record,record2);
      }
      else
      {
	key1[atoi(read_record+keyinfo[0].seg[0].base.start)]--;
	key3[atoi(read_record+keyinfo[2].seg[0].base.start)]=0;
	key1[n1]++; key3[n3]=1;
	update++;
      }
    }
  }
  if (testflag==3) goto end;

  printf("- Same key: first - next -> last - prev -> first\n");
  DBUG_PRINT("progpos",("first - next -> last - prev -> first"));
  for (i=999, dupp_keys=j=0 ; i>0 ; i--)
  {
    if (key1[i] >dupp_keys) { dupp_keys=key1[i]; j=i; }
  }
  sprintf(key,"%6d",j);
  if (verbose) printf("	 Using key: \"%s\"  Keys: %d\n",key,dupp_keys);
  if (nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT)) goto err;
  if (nisam_rsame(file,read_record2,-1)) goto err;
  if (memcmp(read_record,read_record2,reclength) != 0)
  {
    printf("nisam_rsame didn't find same record\n");
    goto end;
  }
  nisam_info(file,&info,0);
  if (nisam_rfirst(file,read_record2,0) ||
      nisam_rsame_with_pos(file,read_record2,0,info.recpos) ||
      memcmp(read_record,read_record2,reclength) != 0)
  {
    printf("nisam_rsame_with_pos didn't find same record\n");
    goto end;
  }
  {
    int skr=nisam_rnext(file,read_record2,0);
    if ((skr && my_errno != HA_ERR_END_OF_FILE) ||
	nisam_rprev(file,read_record2,-1) ||
	memcmp(read_record,read_record2,reclength) != 0)
    {
      printf("nisam_rsame_with_pos lost position\n");
      goto end;
    }
  }
  ant=1;
  start=keyinfo[0].seg[0].base.start; length=keyinfo[0].seg[0].base.length;
  while (nisam_rnext(file,read_record2,0) == 0 &&
	 memcmp(read_record2+start,key,length) == 0) ant++;
  if (ant != dupp_keys)
  {
    printf("next: I can only find: %d keys of %d\n",ant,dupp_keys);
    goto end;
  }
  ant=0;
  while (nisam_rprev(file,read_record3,0) == 0 &&
	 bcmp(read_record3+start,key,length) == 0) ant++;
  if (ant != dupp_keys)
  {
    printf("prev: I can only find: %d records of %d\n",ant,dupp_keys);
    goto end;
  }

  printf("- All keys: first - next -> last - prev -> first\n");
  DBUG_PRINT("progpos",("All keys: first - next -> last - prev -> first"));
  ant=1;
  if (nisam_rfirst(file,read_record,0))
  {
    printf("Can't find first record\n");
    goto end;
  }
  while (nisam_rnext(file,read_record3,0) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - delete)
  {
    printf("next: I found: %d records of %d\n",ant,write_count - delete);
    goto end;
  }
  if (nisam_rlast(file,read_record2,0) ||
      bcmp(read_record2,read_record3,reclength))
  {
    printf("Can't find last record\n");
    DBUG_DUMP("record2",(byte*) read_record2,reclength);
    DBUG_DUMP("record3",(byte*) read_record3,reclength);
    goto end;
  }
  ant=1;
  while (nisam_rprev(file,read_record3,0) == 0 && ant < write_count+10)
    ant++;
  if (ant != write_count - delete)
  {
    printf("prev: I found: %d records of %d\n",ant,write_count);
    goto end;
  }
  if (bcmp(read_record,read_record3,reclength))
  {
    printf("Can't find first record\n");
    goto end;
  }

  printf("- Test if: Read first - next - prev - prev - next == first\n");
  DBUG_PRINT("progpos",("- Read first - next - prev - prev - next == first"));
  if (nisam_rfirst(file,read_record,0) ||
      nisam_rnext(file,read_record3,0) ||
      nisam_rprev(file,read_record3,0) ||
      nisam_rprev(file,read_record3,0) == 0 ||
      nisam_rnext(file,read_record3,0))
      goto err;
  if (bcmp(read_record,read_record3,reclength) != 0)
     printf("Can't find first record\n");

  printf("- Test if: Read last - prev - next - next - prev == last\n");
  DBUG_PRINT("progpos",("Read last - prev - next - next - prev == last"));
  if (nisam_rlast(file,read_record2,0) ||
      nisam_rprev(file,read_record3,0) ||
      nisam_rnext(file,read_record3,0) ||
      nisam_rnext(file,read_record3,0) == 0 ||
      nisam_rprev(file,read_record3,0))
      goto err;
  if (bcmp(read_record2,read_record3,reclength))
     printf("Can't find last record\n");

  puts("- Test read key-part");
  strmov(key2,key);
  for(i=strlen(key2) ; i-- > 1 ;)
  {
    key2[i]=0;
    if (nisam_rkey(file,read_record,0,key2,(uint) i,HA_READ_KEY_EXACT)) goto err;
    if (bcmp(read_record+start,key,(uint) i))
    {
      puts("Didn't find right record");
      goto end;
    }
  }
  if (dupp_keys > 2)
  {
    printf("- Read key (first) - next - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - next - delete - next -> last"));
    if (nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT)) goto err;
    if (nisam_rnext(file,read_record3,0)) goto err;
    if (nisam_delete(file,read_record3)) goto err;
    delete++;
    ant=1;
    while (nisam_rnext(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-1)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-1);
      goto end;
    }
  }
  if (dupp_keys>4)
  {
    printf("- Read last of key - prev - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - prev - delete - prev -> first"));
    if (nisam_rprev(file,read_record3,0)) goto err;
    if (nisam_rprev(file,read_record3,0)) goto err;
    if (nisam_delete(file,read_record3)) goto err;
    delete++;
    ant=1;
    while (nisam_rprev(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-2)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-2);
      goto end;
    }
  }
  if (dupp_keys > 6)
  {
    printf("- Read first - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - delete - next -> last"));
    if (nisam_rkey(file,read_record3,0,key,0,HA_READ_KEY_EXACT)) goto err;
    if (nisam_delete(file,read_record3)) goto err;
    delete++;
    ant=1;
    if (nisam_rnext(file,read_record,0))
      goto err;					/* Skall finnas poster */
    while (nisam_rnext(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-3)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-3);
      goto end;
    }

    printf("- Read last - delete - prev -> first\n");
    DBUG_PRINT("progpos",("last - delete - prev -> first"));
    if (nisam_rprev(file,read_record3,0)) goto err;
    if (nisam_delete(file,read_record3)) goto err;
    delete++;
    ant=0;
    while (nisam_rprev(file,read_record3,0) == 0 &&
	   bcmp(read_record3+start,key,length) == 0) ant++;
    if (ant != dupp_keys-4)
    {
      printf("next: I can only find: %d keys of %d\n",ant,dupp_keys-4);
      goto end;
    }
  }

  puts("- Test if: Read rrnd - same");
  DBUG_PRINT("progpos",("Read rrnd - same"));
  for (i=0 ; i < write_count ; i++)
  {
    if (nisam_rrnd(file,read_record,i == 0 ? 0L : NI_POS_ERROR) == 0)
      break;
  }
  if (i == write_count)
    goto err;

  bmove(read_record2,read_record,reclength);
  for (i=2 ; i-- > 0 ;)
  {
    if (nisam_rsame(file,read_record2,(int) i)) goto err;
    if (bcmp(read_record,read_record2,reclength) != 0)
    {
      printf("is_rsame didn't find same record\n");
      goto end;
    }
  }
  puts("- Test nisam_records_in_range");
  nisam_info(file,&info,HA_STATUS_VARIABLE);
  for (i=0 ; i < info.keys ; i++)
  {
    if (nisam_rfirst(file,read_record,(int) i) ||
	nisam_rlast(file,read_record2,(int) i))
      goto err;
    copy_key(file,(uint) i,(uchar*) read_record,(uchar*) key);
    copy_key(file,(uint) i,(uchar*) read_record2,(uchar*) key2);
    range_records=nisam_records_in_range(file,(int) i,key,0,HA_READ_KEY_EXACT,
				      key2,0,HA_READ_AFTER_KEY);
    if (range_records < info.records*8/10 ||
	range_records > info.records*12/10)
    {
      printf("ni_records_range returned %lu; Should be about %lu\n",
	     range_records,info.records);
      goto end;
    }
    if (verbose)
    {
      printf("ni_records_range returned %ld;  Exact is %ld  (diff: %4.2g %%)\n",
	     range_records,info.records,
	     labs((long) range_records - (long) info.records)*100.0/
	     info.records);

    }
  }
  for (i=0 ; i < 5 ; i++)
  {
    for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
    for (k=rnd(1000) ; k>0 && key1[k] == 0 ; k--) ;
    if (j != 0 && k != 0)
    {
      if (j > k)
	swap(int,j,k);
      sprintf(key,"%6d",j);
      sprintf(key2,"%6d",k);
      range_records=nisam_records_in_range(file,0,key,0,HA_READ_AFTER_KEY,
					key2,0,HA_READ_BEFORE_KEY);
      records=0;
      for (j++ ; j < k ; j++)
	records+=key1[j];
      if ((long) range_records < (long) records*7/10-2 ||
	  (long) range_records > (long) records*13/10+2)
      {
	printf("ni_records_range returned %ld; Should be about %ld\n",
	       range_records,records);
	goto end;
      }
      if (verbose && records)
      {
	printf("ni_records_range returned %ld;  Exact is %ld  (diff: %4.2g %%)\n",
	       range_records,records,
	       labs((long) range_records-(long) records)*100.0/records);

      }
    }
    }

  printf("- nisam_info\n");
  nisam_info(file,&info,0);
  if (info.records != write_count-delete || info.deleted > delete + update
      || info.keys != keys)
  {
    puts("Wrong info from nisam_info");
    printf("Got: records: %ld  delete: %ld  i_keys: %d\n",
	   info.records,info.deleted,info.keys);
  }
  if (verbose)
  {
    char buff[80];
    get_date(buff,3,info.create_time);
    printf("info: Created %s\n",buff);
    get_date(buff,3,info.isamchk_time);
    printf("info: checked %s\n",buff);
    get_date(buff,3,info.update_time);
    printf("info: Modified %s\n",buff);
  }

  nisam_panic(HA_PANIC_WRITE);
  nisam_panic(HA_PANIC_READ);
  if (nisam_is_changed(file))
    puts("Warning: nisam_is_changed reported that datafile was changed");

  printf("- nisam_extra(CACHE) + nisam_rrnd.... + nisam_extra(NO_CACHE)\n");
  if (nisam_extra(file,HA_EXTRA_RESET) || nisam_extra(file,HA_EXTRA_CACHE))
  {
    if (locking || (!use_blob && !pack_fields))
    {
      puts("got error from nisam_extra(HA_EXTRA_CACHE)");
      goto end;
    }
  }
  ant=0;
  while ((error=nisam_rrnd(file,record,NI_POS_ERROR)) >= 0 &&
	 ant < write_count + 10)
	ant+= error ? 0 : 1;
  if (ant != write_count-delete)
  {
    printf("rrnd with cache: I can only find: %d records of %d\n",
	   ant,write_count-delete);
    goto end;
  }
  if (nisam_extra(file,HA_EXTRA_NO_CACHE))
  {
    puts("got error from nisam_extra(HA_EXTRA_NO_CACHE)");
    goto end;
  }

  if (testflag == 4) goto end;

  printf("- Removing keys\n");
  lastpos = NI_POS_ERROR;
  /* DBUG_POP(); */
  nisam_extra(file,HA_EXTRA_RESET);
  while ((error=nisam_rrnd(file,read_record,NI_POS_ERROR)) >=0)
  {
    nisam_info(file,&info,1);
    if (lastpos >= info.recpos && lastpos != NI_POS_ERROR)
    {
      printf("nisam_rrnd didn't advance filepointer; old: %ld, new: %ld\n",
	     lastpos,info.recpos);
      goto err;
    }
    lastpos=info.recpos;
    if (error == 0)
    {
      if (nisam_rsame(file,read_record,-1))
      {
	printf("can't find record %lx\n",info.recpos);
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
	    printf("found blob with wrong info at %ld\n",lastpos);
	    use_blob=0;
	    break;
	  }
	}
      }
      if (nisam_delete(file,read_record))
      {
	printf("can't delete record: %s\n",read_record);
	goto err;
      }
      delete++;
    }
  }
  if (my_errno != HA_ERR_END_OF_FILE && my_errno != HA_ERR_RECORD_DELETED)
    printf("error: %d from nisam_rrnd\n",my_errno);
  if (write_count != delete)
  {
    printf("Deleted only %d of %d records\n",write_count,delete);
    goto err;
  }
end:
  if (nisam_close(file))
    goto err;
  nisam_panic(HA_PANIC_CLOSE);			/* Should close log */
  printf("\nFollowing test have been made:\n");
  printf("Write records: %d\nUpdate records: %d\nSame-key-read: %d\nDelete records: %d\n", write_count,update,dupp_keys,delete);
  if (rec_pointer_size)
    printf("Record pointer size: %d\n",rec_pointer_size);
  if (key_cacheing)
    puts("Key cacheing used");
  if (write_cacheing)
    puts("Write cacheing used");
  if (async_io && locking)
    puts("Asyncron io with locking used");
  else if (locking)
    puts("Locking used");
  if (use_blob)
    puts("blobs used");
  end_key_cache();
  if (blob_buffer)
    my_free(blob_buffer,MYF(0));
  my_end(MY_CHECK_ERROR | MY_GIVE_INFO);
  return(0);
err:
  printf("got error: %d when using NISAM-database\n",my_errno);
  if (file)
    VOID(nisam_close(file));
  return(1);
} /* main */


	/* l{ser optioner */
	/* OBS! intierar endast DEBUG - ingen debuggning h{r ! */

static void get_options(argc,argv)
int argc;
char *argv[];
{
  char *pos,*progname;
  DEBUGGER_OFF;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'b':
      if (*++pos)
	nisam_block_size= MY_ALIGN(atoi(pos),512);
      set_if_bigger(nisam_block_size,8192);	/* Max block size */
      set_if_smaller(nisam_block_size,1024);
      break;
    case 'B':
      use_blob=1;
      break;
    case 'K':				/* Use key cacheing */
      key_cacheing=1;
      break;
    case 'W':				/* Use write cacheing */
      write_cacheing=1;
      if (*++pos)
	my_default_record_cache_size=atoi(pos);
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
      recant=atoi(++pos);
      break;
    case 'f':
      if ((first_key=atoi(++pos)) <0 || first_key >= NISAM_KEYS)
	first_key=0;
      break;
    case 'k':
      if ((keys=(uint) atoi(++pos)) < 1 ||
	   keys > (uint) (NISAM_KEYS-first_key))
	keys=NISAM_KEYS-first_key;
      break;
    case 'P':
      pack_type=0;			/* Don't use DIFF_LENGTH */
      break;
    case 'R':				/* Length of record pointer */
      rec_pointer_size=atoi(++pos);
      if (rec_pointer_size > 3)
	rec_pointer_size=0;
      break;
    case 'S':
      pack_fields=0;			/* Static-length-records */
      break;
    case 't':
      testflag=atoi(++pos);		/* testmod */
      break;
    case '?':
    case 'I':
    case 'V':
      printf("%s  Ver 1.4 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("TCX Datakonsult AB, by Monty, for your professional use\n");
      printf("Usage: %s [-?ABIKLPRSVWltv] [-b#] [-k#] [-f#] [-m#] [-t#]\n",progname);
      exit(0);
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (++pos);
      break;
    default:
      printf("Illegal option: '%c'\n",*pos);
      break;
    }
  }
  return;
} /* get options */

	/* Ge ett randomv{rde inom ett intervall 0 <=x <= n */

static uint rnd(max_value)
uint max_value;
{
  return (uint) ((rand() & 32767)/32767.0*max_value);
} /* rnd */


	/* G|r en record av skiftande length */

static void fix_length(rec,length)
byte *rec;
uint length;
{
  bmove(rec+STANDAR_LENGTH,
	"0123456789012345678901234567890123456789012345678901234567890",
	length-STANDAR_LENGTH);
  strfill(rec+length,STANDAR_LENGTH+60-length,' ');
} /* fix_length */


	/* Put maybe a blob in record */

static void put_blob_in_record(blob_pos,blob_buffer)
char *blob_pos,**blob_buffer;
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
      longstore(blob_pos,length);
      bmove(blob_pos+4,(char*) blob_buffer,sizeof(char*));
    }
    else
    {
      longstore(blob_pos,0);
    }
  }
  return;
}


static void copy_key(info,inx,rec,key_buff)
N_INFO *info;
uint inx;
uchar *rec,*key_buff;
{
  N_KEYSEG *keyseg;

  for (keyseg=info->s->keyinfo[inx].seg ; keyseg->base.type ; keyseg++)
  {
    memcpy(key_buff,rec+keyseg->base.start,(size_t) keyseg->base.length);
    key_buff+=keyseg->base.length;
  }
  return;
}
