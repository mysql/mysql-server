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

#include "heapdef.h"		/* Because of _hp_find_block */
#include <signal.h>

#define MAX_RECORDS 100000
#define MAX_KEYS 3

static int get_options(int argc, char *argv[]);
static int rnd(int max_value);
static sig_handler endprog(int sig_number);

static uint flag=0,verbose=0,testflag=0,recant=10000,silent=0;
static uint keys=MAX_KEYS;
static uint16 key1[1001];
static my_bool key3[MAX_RECORDS];

static int calc_check(byte *buf,uint length);

		/* Huvudprogrammet */

int main(argc,argv)
int argc;
char *argv[];
{
  register uint i,j;
  uint ant,n1,n2,n3;
  uint reclength,write_count,update,delete,check2,dupp_keys,found_key;
  int error;
  ulong pos;
  unsigned long key_check;
  char record[128],record2[128],record3[128],key[10];
  const char *filename,*filename2;
  HP_INFO *file,*file2;
  HP_KEYDEF keyinfo[MAX_KEYS];
  HP_KEYSEG keyseg[MAX_KEYS*5];
  HEAP_PTR position;
  MY_INIT(argv[0]);		/* init my_sys library & pthreads */

  filename= "test2";
  filename2= "test2_2";
  file=file2=0;
  get_options(argc,argv);
  reclength=37;

  write_count=update=delete=0;
  key_check=0;

  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
  keyinfo[0].flag= 0;
  keyinfo[0].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[0].seg[0].start=0;
  keyinfo[0].seg[0].length=6;
  keyinfo[1].seg=keyseg+1;
  keyinfo[1].keysegs=2;
  keyinfo[1].flag=0;
  keyinfo[1].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[1].seg[0].start=7;
  keyinfo[1].seg[0].length=6;
  keyinfo[1].seg[1].type=HA_KEYTYPE_TEXT;
  keyinfo[1].seg[1].start=0;			/* Tv}delad nyckel */
  keyinfo[1].seg[1].length=6;
  keyinfo[2].seg=keyseg+3;
  keyinfo[2].keysegs=1;
  keyinfo[2].flag=HA_NOSAME;
  keyinfo[2].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[2].seg[0].start=12;
  keyinfo[2].seg[0].length=8;

  bzero((char*) key1,sizeof(key1));
  bzero((char*) key3,sizeof(key3));

  printf("- Creating heap-file\n");
  if (heap_create(filename))
    goto err;
  if (!(file=heap_open(filename,2,keys,keyinfo,reclength,(ulong) flag*100000L,
		       (ulong) recant/2)))
    goto err;
  signal(SIGINT,endprog);

  printf("- Writing records:s\n");
  strmov(record,"          ..... key");

  for (i=0 ; i < recant ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(min(recant*5,MAX_RECORDS));
    sprintf(record,"%6d:%4d:%8d:Pos: %4d          ",n1,n2,n3,write_count);

    if (heap_write(file,record))
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
      if (key3[n3] == 1)
      {
	printf("Error: Didn't get error when writing second key: '%8d'\n",n3);
	goto err;
      }
      write_count++; key1[n1]++; key3[n3]=1;
      key_check+=n1;
    }
    if (testflag == 1 && heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }
  }
  if (testflag == 1)
    goto end;
  if (heap_check_heap(file))
  {
    puts("Heap keys crashed");
    goto err;
  }

  printf("- Delete\n");
  for (i=0 ; i < write_count/10 ; i++)
  {
    for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf(key,"%6d",j);
      if (heap_rkey(file,record,0,key))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
#ifdef NOT_USED
      if (file->current_ptr == _hp_find_block(&file->s->block,0) ||
	  file->current_ptr == _hp_find_block(&file->s->block,1))
	continue;			/* Don't remove 2 first records */
#endif
      if (heap_delete(file,record))
      {
	printf("error: %d; can't delete record: \"%s\"\n", my_errno,record);
	goto err;
      }
      delete++;
      key1[atoi(record+keyinfo[0].seg[0].start)]--;
      key3[atoi(record+keyinfo[2].seg[0].start)]=0;
      key_check-=atoi(record);
      if (testflag == 2 && heap_check_heap(file))
      {
	puts("Heap keys crashed");
	goto err;
      }
    }
    else
      puts("Warning: Skipping delete test because no dupplicate keys");
  }
  if (testflag==2) goto end;
  if (heap_check_heap(file))
  {
    puts("Heap keys crashed");
    goto err;
  }

  printf("- Update\n");
  for (i=0 ; i < write_count/10 ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(min(recant*2,MAX_RECORDS));
    sprintf(record2,"%6d:%4d:%8d:XXX: %4d          ",n1,n2,n3,update);
    if (rnd(2) == 1)
    {
      if (heap_scan_init(file))
	goto err;
      j=rnd(write_count-delete);
      while ((error=heap_scan(file,record) == HA_ERR_RECORD_DELETED) ||
	     (!error && j))
      {
	if (!error)
	  j--;
      }
      if (error)
	goto err;
    }
    else
    {
      for (j=rnd(1000)+1 ; j>0 && key1[j] == 0 ; j--) ;
      if (!key1[j])
	continue;
      sprintf(key,"%6d",j);
      if (heap_rkey(file,record,0,key))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
    }
    if (heap_update(file,record,record2))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY || key3[n3] == 0)
      {
	printf("error: %d; can't uppdate:\nFrom: \"%s\"\nTo:   \"%s\"\n",
	       my_errno,record,record2);
	goto err;
      }
      if (verbose)
	printf("Double key when tryed to uppdate:\nFrom: \"%s\"\nTo:   \"%s\"\n",record,record2);
    }
    else
    {
      key1[atoi(record+keyinfo[0].seg[0].start)]--;
      key3[atoi(record+keyinfo[2].seg[0].start)]=0;
      key1[n1]++; key3[n3]=1;
      update++;
      key_check=key_check-atoi(record)+n1;
    }
    if (testflag == 3 && heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }
  }
  if (testflag == 3) goto end;
  if (heap_check_heap(file))
  {
    puts("Heap keys crashed");
    goto err;
  }

  for (i=999, dupp_keys=found_key=0 ; i>0 ; i--)
  {
    if (key1[i] > dupp_keys) { dupp_keys=key1[i]; found_key=i; }
    sprintf(key,"%6d",found_key);
  }

  if (dupp_keys > 3)
  {
    if (!silent)
      printf("- Read first key - next - delete - next -> last\n");
    DBUG_PRINT("progpos",("first - next - delete - next -> last"));

    if (heap_rkey(file,record,0,key))
      goto err;
    if (heap_rnext(file,record3)) goto err;
    if (heap_delete(file,record3)) goto err;
    key_check-=atoi(record3);
    key1[atoi(record+keyinfo[0].seg[0].start)]--;
    key3[atoi(record+keyinfo[2].seg[0].start)]=0;
    delete++;
    ant=2;
    while ((error=heap_rnext(file,record3)) == 0 ||
	   error == HA_ERR_RECORD_DELETED)
      if (! error)
	ant++;
    if (ant != dupp_keys)
    {
      printf("next: I can only find: %d records of %d\n",
	     ant,dupp_keys);
      goto end;
    }
    dupp_keys--;
    if (heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }

    if (!silent)
      printf("- Read last key - delete - prev - prev - delete - prev -> first\n");

    if (heap_rlast(file,record3)) goto err;
    if (heap_delete(file,record3)) goto err;
    key_check-=atoi(record3);
    key1[atoi(record+keyinfo[0].seg[0].start)]--;
    key3[atoi(record+keyinfo[2].seg[0].start)]=0;
    delete++;
    if (heap_rprev(file,record3) || heap_rprev(file,record3))
      goto err;
    if (heap_delete(file,record3)) goto err;
    key_check-=atoi(record3);
    key1[atoi(record+keyinfo[0].seg[0].start)]--;
    key3[atoi(record+keyinfo[2].seg[0].start)]=0;
    delete++;
    ant=3;
    while ((error=heap_rprev(file,record3)) == 0 ||
	   error == HA_ERR_RECORD_DELETED)
    {
      if (! error)
	ant++;
    }
    if (ant != dupp_keys)
    {
      printf("next: I can only find: %d records of %d\n",
	     ant,dupp_keys);
      goto end;
    }
    dupp_keys-=2;
    if (heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }
  }
  else
    puts("Warning: Not enough duplicated keys:  Skipping delete key check");

  if (!silent)
    printf("- Read (first) - next - delete - next -> last\n");
  DBUG_PRINT("progpos",("first - next - delete - next -> last"));

  if (heap_scan_init(file))
    goto err;
  while ((error=heap_scan(file,record3) == HA_ERR_RECORD_DELETED)) ;
  if (error)
    goto err;
  if (heap_delete(file,record3)) goto err;
  key_check-=atoi(record3);
  delete++;
  key1[atoi(record+keyinfo[0].seg[0].start)]--;
  key3[atoi(record+keyinfo[2].seg[0].start)]=0;
  ant=0;
  while ((error=heap_scan(file,record3)) == 0 ||
	 error == HA_ERR_RECORD_DELETED)
    if (! error)
      ant++;
  if (ant != write_count-delete)
  {
    printf("next: Found: %d records of %d\n",ant,write_count-delete);
    goto end;
  }
  if (heap_check_heap(file))
  {
    puts("Heap keys crashed");
    goto err;
  }

  puts("- Test if: Read rrnd - same - rkey - same");
  DBUG_PRINT("progpos",("Read rrnd - same"));
  pos=rnd(write_count-delete-5)+5;
  heap_scan_init(file);
  i=5;
  while ((error=heap_scan(file,record)) == HA_ERR_RECORD_DELETED ||
	 (error == 0 && pos))
  {
    if (!error)
      pos--;
    if (i-- == 0)
    {
      bmove(record3,record,reclength);
      position=heap_position(file);
    }
  }
  if (error)
    goto err;
  bmove(record2,record,reclength);
  if (heap_rsame(file,record,-1) || heap_rsame(file,record2,2))
    goto err;
  if (bcmp(record2,record,reclength))
  {
    puts("heap_rsame didn't find right record");
    goto end;
  }

  puts("- Test of read through position");
  if (heap_rrnd(file,record,position))
    goto err;
  if (bcmp(record3,record,reclength))
  {
    puts("heap_frnd didn't find right record");
    goto end;
  }

  printf("- heap_info\n");
  {
    HEAPINFO info;
    heap_info(file,&info,0);
    /* We have to test with delete +1 as this may be the case if the last
       inserted row was a duplicate key */
    if (info.records != write_count-delete ||
	(info.deleted != delete && info.deleted != delete+1))
    {
      puts("Wrong info from heap_info");
      printf("Got: records: %ld(%d)  deleted: %ld(%d)\n",
	     info.records,write_count-delete,info.deleted,delete);
    }
  }

#ifdef OLD_HEAP_VERSION
  {
    uint check;
    printf("- Read through all records with rnd\n");
    if (heap_extra(file,HA_EXTRA_RESET) || heap_extra(file,HA_EXTRA_CACHE))
    {
      puts("got error from heap_extra");
      goto end;
    }
    ant=check=0;
    while ((error=heap_rrnd(file,record,(ulong) -1)) != HA_ERR_END_OF_FILE &&
	   ant < write_count + 10)
    {
      if (!error)
      {
	ant++;
	check+=calc_check(record,reclength);
      }
    }
    if (ant != write_count-delete)
    {
      printf("rrnd: I can only find: %d records of %d\n", ant,
	     write_count-delete);
      goto end;
    }
    if (heap_extra(file,HA_EXTRA_NO_CACHE))
    {
      puts("got error from heap_extra(HA_EXTRA_NO_CACHE)");
      goto end;
    }
  }
#endif

  printf("- Read through all records with scan\n");
  if (heap_extra(file,HA_EXTRA_RESET) || heap_extra(file,HA_EXTRA_CACHE))
  {
    puts("got error from heap_extra");
    goto end;
  }
  ant=check2=0;
  heap_scan_init(file);
  while ((error=heap_scan(file,record)) != HA_ERR_END_OF_FILE &&
	 ant < write_count + 10)
  {
    if (!error)
    {
      ant++;
      check2+=calc_check(record,reclength);
    }
  }
  if (ant != write_count-delete)
  {
    printf("scan: I can only find: %d records of %d\n", ant,
	   write_count-delete);
    goto end;
  }
#ifdef OLD_HEAP_VERSION
  if (check != check2)
  {
    puts("scan: Checksum didn't match reading with rrnd");
    goto end;
  }
#endif


  if (heap_extra(file,HA_EXTRA_NO_CACHE))
  {
    puts("got error from heap_extra(HA_EXTRA_NO_CACHE)");
    goto end;
  }

  for (i=999, dupp_keys=found_key=0 ; i>0 ; i--)
  {
    if (key1[i] > dupp_keys) { dupp_keys=key1[i]; found_key=i; }
    sprintf(key,"%6d",found_key);
  }
  printf("- Read through all keys with first-next-last-prev\n");
  ant=0;
  for (error=heap_rkey(file,record,0,key) ;
      ! error ;
       error=heap_rnext(file,record))
    ant++;
  if (ant != dupp_keys)
  {
    printf("first-next: I can only find: %d records of %d\n", ant,
	   dupp_keys);
    goto end;
  }

  ant=0;
  for (error=heap_rlast(file,record) ;
      ! error ;
      error=heap_rprev(file,record))
  {
    ant++;
    check2+=calc_check(record,reclength);
  }
  if (ant != dupp_keys)
  {
    printf("last-prev: I can only find: %d records of %d\n", ant,
	   dupp_keys);
    goto end;
  }

  if (testflag == 4) goto end;

  printf("- Reading through all rows through keys\n");
  if (!(file2=heap_open(filename,2,0,0,0,0,0)))
    goto err;
  if (heap_scan_init(file))
    goto err;
  while ((error=heap_scan(file,record)) != HA_ERR_END_OF_FILE)
  {
    if (error == 0)
    {
      if (heap_rkey(file2,record2,2,record+keyinfo[2].seg[0].start))
      {
	printf("can't find key3: \"%.8s\"\n",
	       record+keyinfo[2].seg[0].start);
	goto err;
      }
    }
  }
  heap_close(file2);

  printf("- Creating output heap-file 2\n");
  if (!(file2=heap_open(filename2,2,1,keyinfo,reclength,0L,0L)))
    goto err;

  printf("- Copying and removing records\n");
  if (heap_scan_init(file))
    goto err;
  while ((error=heap_scan(file,record)) != HA_ERR_END_OF_FILE)
  {
    if (error == 0)
    {
      if (heap_write(file2,record))
	goto err;
      key_check-=atoi(record);
      write_count++;
      if (heap_delete(file,record))
	goto err;
      delete++;
    }
    pos++;
  }
  if (heap_check_heap(file) || heap_check_heap(file2))
  {
    puts("Heap keys crashed");
    goto err;
  }

  if (my_errno != HA_ERR_END_OF_FILE)
    printf("error: %d from heap_rrnd\n",my_errno);
  if (key_check)
    printf("error: Some read got wrong: check is %ld\n",(long) key_check);

end:
  printf("\nFollowing test have been made:\n");
  printf("Write records: %d\nUpdate records: %d\nDelete records: %d\n", write_count,update,delete);
  heap_clear(file);
  if (heap_close(file) || (file2 && heap_close(file2)))
    goto err;
  heap_delete_all(filename2);
  heap_panic(HA_PANIC_CLOSE);
  my_end(MY_GIVE_INFO);
  return(0);
err:
  printf("Got error: %d when using heap-database\n",my_errno);
  VOID(heap_close(file));
  return(1);
} /* main */


	/* Read options */

static int get_options(int argc,char *argv[])
{
  char *pos,*progname;
  DEBUGGER_OFF;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'B':				/* Big file */
      flag=1;
      break;
    case 'v':				/* verbose */
      verbose=1;
      break;
    case 'm':				/* records */
      recant=atoi(++pos);
      break;
    case 's':
      silent=1;
      break;
    case 't':
      testflag=atoi(++pos);		/* testmod */
      break;
    case 'V':
    case 'I':
    case '?':
      printf("%s  Ver 1.1 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("TCX Datakonsult AB, by Monty, for your professional use\n");
      printf("Usage: %s [-?ABIKLsWv] [-m#] [-t#]\n",progname);
      exit(0);
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (++pos);
      break;
    }
  }
  return 0;
} /* get options */

	/* Generate a random value in intervall 0 <=x <= n */

static int rnd(max_value)
int max_value;
{
  return (int) ((rand() & 32767)/32767.0*max_value);
} /* rnd */


static sig_handler endprog(int sig_number __attribute__((unused)))
{
#ifndef THREAD
  if (my_dont_interrupt)
    my_remember_signal(sig_number,endprog);
  else
#endif
  {
    heap_panic(HA_PANIC_CLOSE);
    my_end(1);
    exit(1);
  }
}

static int calc_check(buf,length)
byte *buf;
uint length;
{
  int check=0;
  while (length--)
    check+= (int) (uchar) *(buf++);
  return check;
}
