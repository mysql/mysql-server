/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Test av hash libarary: stor test */

#include <my_global.h>
#include <my_sys.h>
#include <hash.h>
#include <m_string.h>

#define MAX_RECORDS 100000
#define MAX_KEYS 3

static int get_options(int argc, char *argv[]);
static int do_test();
static int rnd(int max_value);

static uint testflag=0,recant=10000,reclength=37;
static uint16 key1[1000];

#ifdef DBUG_OFF
#define hash_check(A) 0
#else
my_bool hash_check(HASH *hash);
#endif
void free_record(void *record);

static byte *hash2_key(const byte *rec,uint *length,
		       my_bool not_used __attribute__((unused)))
{
  *length=(uint) (uchar) rec[reclength-1];
  return (byte*) rec;
}

		/* Huvudprogrammet */

int main(int argc,char *argv[])
{
  MY_INIT(argv[0]);
  DBUG_PROCESS(argv[0]);

  get_options(argc,argv);

  exit(do_test());
}

static int do_test()
{
  register uint i,j;
  uint n1,n2,n3;
  uint write_count,update,delete;
  ulong pos;
  unsigned long key_check;
  char *record,*recpos,oldrecord[120],key[10];
  HASH hash,hash2;
  DBUG_ENTER("do_test");

  write_count=update=delete=0;
  key_check=0;
  bzero((char*) key1,sizeof(key1[0])*1000);

  printf("- Creating hash\n");
  if (hash_init(&hash,recant/2,0,6,0,free_record,0))
    goto err;
  printf("- Writing records:s\n");

  for (i=0 ; i < recant ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(min(recant*5,MAX_RECORDS));
    record= (char*) my_malloc(reclength,MYF(MY_FAE));
    sprintf(record,"%6d:%4d:%8d:Pos: %4d      ",n1,n2,n3,write_count);
    if (hash_insert(&hash,record))
    {
      printf("Error: %d in write at record: %d\n",my_errno,i);
      goto err;
    }
    key1[n1]++;
    key_check+=n1;
    write_count++;
  }
  if (hash_check(&hash))
  {
    puts("Heap keys crashed");
    goto err;
  }
  printf("- Delete\n");
  for (i=0 ; i < write_count/10 ; i++)
  {
    for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
    if (j != 0)
    {
      sprintf(key,"%6d",j);
      if (!(recpos=hash_search(&hash,key,0)))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      key1[atoi(recpos)]--;
      key_check-=atoi(recpos);
      memcpy(oldrecord,recpos,reclength);
      if (hash_delete(&hash,recpos))
      {
	printf("error: %d; can't delete record: \"%s\"\n", my_errno,oldrecord);
	goto err;
      }
      delete++;
      if (testflag == 2 && hash_check(&hash))
      {
	puts("Heap keys crashed");
	goto err;
      }
    }
  }
  if (hash_check(&hash))
  {
    puts("Hash keys crashed");
    goto err;
  }

  printf("- Update\n");
  for (i=0 ; i < write_count/10 ; i++)
  {
    n1=rnd(1000); n2=rnd(100); n3=rnd(min(recant*2,MAX_RECORDS));
    for (j=rnd(1000) ; j>0 && key1[j] == 0 ; j--) ;
    if (j)
    {
      sprintf(key,"%6d",j);
      if (!(recpos=hash_search(&hash,key,0)))
      {
	printf("can't find key1: \"%s\"\n",key);
	goto err;
      }
      key1[atoi(recpos)]--;
      key_check=key_check-atoi(recpos)+n1;
      key1[n1]++;
      sprintf(recpos,"%6d:%4d:%8d:XXX: %4d      ",n1,n2,n3,update);
      update++;
      if (hash_update(&hash,recpos,key,0))
      {
	printf("can't update key1: \"%s\"\n",key);
	goto err;
      }
      if (testflag == 3 && hash_check(&hash))
      {
	printf("Heap keys crashed for %d update\n",update);
	goto err;
      }
    }
  }
  if (hash_check(&hash))
  {
    puts("Heap keys crashed");
    goto err;
  }

  for (j=0 ; j < 1000 ; j++)
    if (key1[j] > 1)
      break;
  if (key1[j] > 1)
  {
    printf("- Testing identical read\n");
    sprintf(key,"%6d",j);
    pos=1;
    if (!(recpos=hash_search(&hash,key,0)))
    {
      printf("can't find key1: \"%s\"\n",key);
      goto err;
    }
    while (hash_next(&hash,key,0) && pos < (ulong) (key1[j]+10))
      pos++;
    if (pos != (ulong) key1[j])
    {
      printf("Found %ld copies of key: %s. Should be %d",pos,key,key1[j]);
      goto err;
    }
  }
  printf("- Creating output heap-file 2\n");
  if (hash_init(&hash2,hash.records,0,0,hash2_key,free_record,0))
    goto err;

  printf("- Copying and removing records\n");
  pos=0;
  while ((recpos=hash_element(&hash,0)))
  {
    record=(byte*) my_malloc(reclength,MYF(MY_FAE));
    memcpy(record,recpos,reclength);
    record[reclength-1]=rnd(5)+1;
    if (hash_insert(&hash2,record))
    {
      printf("Got error when inserting record: %*s",reclength,record);
      goto err;
    }
    key_check-=atoi(record);
    write_count++;
    if (hash_delete(&hash,recpos))
    {
      printf("Got error when deleting record: %*s",reclength,recpos);
      goto err;
    }
    if (testflag==4)
    {
      if (hash_check(&hash) || hash_check(&hash2))
      {
	puts("Hash keys crashed");
	goto err;
      }
    }
    pos++;
  }
  if (hash_check(&hash) || hash_check(&hash2))
  {
    puts("Hash keys crashed");
    goto err;
  }
  if (key_check != 0)
  {
    printf("Key check didn't get to 0 (%ld)\n",key_check);
  }

  printf("\nFollowing test have been made:\n");
  printf("Write records: %d\nUpdate records: %d\nDelete records: %d\n", write_count,
	 update,delete);
  hash_free(&hash); hash_free(&hash2);
  my_end(MY_GIVE_INFO);
  DBUG_RETURN(0);
err:
  printf("Got error: %d when using hashing\n",my_errno);
  DBUG_RETURN(-1);
} /* main */


	/* l{ser optioner */
	/* OBS! intierar endast DEBUG - ingen debuggning h{r ! */

static int get_options(int argc, char **argv)
{
  char *pos,*progname;
  DEBUGGER_OFF;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'm':				/* records */
      recant=atoi(++pos);
      break;
    case 't':
      testflag=atoi(++pos);		/* testmod */
      break;
    case 'V':
    case 'I':
    case '?':
      printf("%s  Ver 1.0 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("TCX Datakonsult AB, by Monty, for your professional use\n");
      printf("Usage: %s [-?ABIKLWv] [-m#] [-t#]\n",progname);
      exit(0);
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (++pos);
      break;
    }
  }
  return 0;
} /* get options */

	/* Ge ett randomv{rde inom ett intervall 0 <=x <= n */

static int rnd(int max_value)
{
  return (int) ((rand() & 32767)/32767.0*max_value);
} /* rnd */


void free_record(void *record)
{
  my_free(record,MYF(0));
}
