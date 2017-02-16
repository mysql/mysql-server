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

/* Test av locking */

#ifndef _WIN32 /*no fork() in Windows*/

#include "maria_def.h"
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif


#if defined(HAVE_LRAND48)
#define rnd(X) (lrand48() % X)
#define rnd_init(X) srand48(X)
#else
#define rnd(X) (random() % X)
#define rnd_init(X) srandom(X)
#endif


const char *filename= "test3";
uint tests=10,forks=10,pagecacheing=0;

static void get_options(int argc, char *argv[]);
void start_test(int id);
int test_read(MARIA_HA *,int),test_write(MARIA_HA *,int,int),
    test_update(MARIA_HA *,int,int),test_rrnd(MARIA_HA *,int);

struct record {
  uchar id[8];
  uchar nr[4];
  uchar text[10];
} record;


int main(int argc,char **argv)
{
  int status,wait_ret;
  uint i=0;
  MARIA_KEYDEF keyinfo[10];
  MARIA_COLUMNDEF recinfo[10];
  HA_KEYSEG keyseg[10][2];
  MY_INIT(argv[0]);
  get_options(argc,argv);

  fprintf(stderr, "WARNING! this program is to test 'external locking'"
          " (when several processes share a table through file locking)"
          " which is not supported by Maria at all; expect errors."
          " We may soon remove this program.\n");
  maria_init();
  bzero((char*) keyinfo,sizeof(keyinfo));
  bzero((char*) recinfo,sizeof(recinfo));
  bzero((char*) keyseg,sizeof(keyseg));
  keyinfo[0].seg= &keyseg[0][0];
  keyinfo[0].seg[0].start=0;
  keyinfo[0].seg[0].length=8;
  keyinfo[0].seg[0].type=HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].flag=HA_SPACE_PACK;
  keyinfo[0].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[0].keysegs=1;
  keyinfo[0].flag = (uint8) HA_PACK_KEY;
  keyinfo[0].block_length= 0;                   /* Default block length */
  keyinfo[1].seg= &keyseg[1][0];
  keyinfo[1].seg[0].start=8;
  keyinfo[1].seg[0].length=4;		/* Long is always 4 in maria */
  keyinfo[1].seg[0].type=HA_KEYTYPE_LONG_INT;
  keyinfo[1].seg[0].flag=0;
  keyinfo[1].key_alg=HA_KEY_ALG_BTREE;
  keyinfo[1].keysegs=1;
  keyinfo[1].flag =HA_NOSAME;
  keyinfo[1].block_length= 0;                   /* Default block length */

  recinfo[0].type=0;
  recinfo[0].length=sizeof(record.id);
  recinfo[1].type=0;
  recinfo[1].length=sizeof(record.nr);
  recinfo[2].type=0;
  recinfo[2].length=sizeof(record.text);

  puts("- Creating maria-file");
  my_delete(filename,MYF(0));		/* Remove old locks under gdb */
  if (maria_create(filename,BLOCK_RECORD, 2, &keyinfo[0],2,&recinfo[0],0,
                   (MARIA_UNIQUEDEF*) 0, (MARIA_CREATE_INFO*) 0,0))
    exit(1);

  rnd_init(0);
  printf("- Starting %d processes\n",forks); fflush(stdout);
  for (i=0 ; i < forks; i++)
  {
    if (!fork())
    {
      start_test(i+1);
      sleep(1);
      return 0;
    }
    (void)rnd(1);
  }

  for (i=0 ; i < forks ; i++)
    while ((wait_ret=wait(&status)) && wait_ret == -1);
  maria_end();
  return 0;
}


static void get_options(int argc, char **argv)
{
  char *pos,*progname;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'f':
      forks=atoi(++pos);
      break;
    case 't':
      tests=atoi(++pos);
      break;
    case 'K':				/* Use key cacheing */
      pagecacheing=1;
      break;
    case 'A':				/* All flags */
      pagecacheing=1;
      break;
   case '?':
    case 'I':
    case 'V':
      printf("%s  Ver 1.0 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("By Monty, for your professional use\n");
      puts("Test av locking with threads\n");
      printf("Usage: %s [-?lKA] [-f#] [-t#]\n",progname);
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
}


void start_test(int id)
{
  uint i;
  int error,lock_type;
  MARIA_INFO isam_info;
  MARIA_HA *file,*file1,*file2=0,*lock;

  if (!(file1=maria_open(filename,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)) ||
      !(file2=maria_open(filename,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)))
  {
    fprintf(stderr,"Can't open isam-file: %s\n",filename);
    exit(1);
  }
  if (pagecacheing && rnd(2) == 0)
    init_pagecache(maria_pagecache, 65536L, 0, 0, MARIA_KEY_BLOCK_LENGTH,
                   MY_WME);
  printf("Process %d, pid: %ld\n",id,(long) getpid()); fflush(stdout);

  for (error=i=0 ; i < tests && !error; i++)
  {
    file= (rnd(2) == 1) ? file1 : file2;
    lock=0 ; lock_type=0;
    if (rnd(10) == 0)
    {
      if (maria_lock_database(lock=(rnd(2) ? file1 : file2),
			   lock_type=(rnd(2) == 0 ? F_RDLCK : F_WRLCK)))
      {
	fprintf(stderr,"%2d: start: Can't lock table %d\n",id,my_errno);
	error=1;
	break;
      }
    }
    switch (rnd(4)) {
    case 0: error=test_read(file,id); break;
    case 1: error=test_rrnd(file,id); break;
    case 2: error=test_write(file,id,lock_type); break;
    case 3: error=test_update(file,id,lock_type); break;
    }
    if (lock)
      maria_lock_database(lock,F_UNLCK);
  }
  if (!error)
  {
    maria_status(file1,&isam_info,HA_STATUS_VARIABLE);
    printf("%2d: End of test.  Records:  %ld  Deleted:  %ld\n",
	   id,(long) isam_info.records, (long) isam_info.deleted);
    fflush(stdout);
  }

  maria_close(file1);
  maria_close(file2);
  if (error)
  {
    printf("%2d: Aborted\n",id); fflush(stdout);
    exit(1);
  }
}


int test_read(MARIA_HA *file,int id)
{
  uint i,lock,found,next,prev;
  ulong find;

  lock=0;
  if (rnd(2) == 0)
  {
    lock=1;
    if (maria_lock_database(file,F_RDLCK))
    {
      fprintf(stderr,"%2d: Can't lock table %d\n",id,my_errno);
      return 1;
    }
  }

  found=next=prev=0;
  for (i=0 ; i < 100 ; i++)
  {
    find=rnd(100000);
    if (!maria_rkey(file,record.id,1,(uchar*) &find, HA_WHOLE_KEY,
                    HA_READ_KEY_EXACT))
      found++;
    else
    {
      if (my_errno != HA_ERR_KEY_NOT_FOUND)
      {
	fprintf(stderr,"%2d: Got error %d from read in read\n",id,my_errno);
	return 1;
      }
      else if (!maria_rnext(file,record.id,1))
	next++;
      else
      {
	if (my_errno != HA_ERR_END_OF_FILE)
	{
	  fprintf(stderr,"%2d: Got error %d from rnext in read\n",id,my_errno);
	  return 1;
	}
	else if (!maria_rprev(file,record.id,1))
	  prev++;
	else
	{
	  if (my_errno != HA_ERR_END_OF_FILE)
	  {
	    fprintf(stderr,"%2d: Got error %d from rnext in read\n",
		    id,my_errno);
	    return 1;
	  }
	}
      }
    }
  }
  if (lock)
  {
    if (maria_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"%2d: Can't unlock table\n",id);
      return 1;
    }
  }
  printf("%2d: read:   found: %5d  next: %5d   prev: %5d\n",
	 id,found,next,prev);
  fflush(stdout);
  return 0;
}


int test_rrnd(MARIA_HA *file,int id)
{
  uint count,lock;

  lock=0;
  if (rnd(2) == 0)
  {
    lock=1;
    if (maria_lock_database(file,F_RDLCK))
    {
      fprintf(stderr,"%2d: Can't lock table (%d)\n",id,my_errno);
      maria_close(file);
      return 1;
    }
    if (rnd(2) == 0)
      maria_extra(file,HA_EXTRA_CACHE,0);
  }

  count=0;
  if (maria_rrnd(file,record.id,0L))
  {
    if (my_errno == HA_ERR_END_OF_FILE)
      goto end;
    fprintf(stderr,"%2d: Can't read first record (%d)\n",id,my_errno);
    return 1;
  }
  for (count=1 ; !maria_rrnd(file,record.id,HA_OFFSET_ERROR) ;count++) ;
  if (my_errno != HA_ERR_END_OF_FILE)
  {
    fprintf(stderr,"%2d: Got error %d from rrnd\n",id,my_errno);
    return 1;
  }

end:
  if (lock)
  {
    maria_extra(file,HA_EXTRA_NO_CACHE,0);
    if (maria_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"%2d: Can't unlock table\n",id);
      exit(0);
    }
  }
  printf("%2d: rrnd:   %5d\n",id,count); fflush(stdout);
  return 0;
}


int test_write(MARIA_HA *file,int id,int lock_type)
{
  uint i,tries,count,lock;

  lock=0;
  if (rnd(2) == 0 || lock_type == F_RDLCK)
  {
    lock=1;
    if (maria_lock_database(file,F_WRLCK))
    {
      if (lock_type == F_RDLCK && my_errno == EDEADLK)
      {
	printf("%2d: write:  deadlock\n",id); fflush(stdout);
	return 0;
      }
      fprintf(stderr,"%2d: Can't lock table (%d)\n",id,my_errno);
      maria_close(file);
      return 1;
    }
    if (rnd(2) == 0)
      maria_extra(file,HA_EXTRA_WRITE_CACHE,0);
  }

  sprintf((char*) record.id,"%7ld", (long) getpid());
  strnmov((char*) record.text,"Testing...", sizeof(record.text));

  tries=(uint) rnd(100)+10;
  for (i=count=0 ; i < tries ; i++)
  {
    uint32 tmp=rnd(80000)+20000;
    int4store(record.nr,tmp);
    if (!maria_write(file,record.id))
      count++;
    else
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY)
      {
	fprintf(stderr,"%2d: Got error %d (errno %d) from write\n",id,my_errno,
		errno);
	return 1;
      }
    }
  }
  if (lock)
  {
    maria_extra(file,HA_EXTRA_NO_CACHE,0);
    if (maria_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"%2d: Can't unlock table\n",id);
      exit(0);
    }
  }
  printf("%2d: write:  %5d\n",id,count); fflush(stdout);
  return 0;
}


int test_update(MARIA_HA *file,int id,int lock_type)
{
  uint i,lock,found,next,prev,update;
  uint32 tmp;
  char find[4];
  struct record new_record;

  lock=0;
  if (rnd(2) == 0 || lock_type == F_RDLCK)
  {
    lock=1;
    if (maria_lock_database(file,F_WRLCK))
    {
      if (lock_type == F_RDLCK && my_errno == EDEADLK)
      {
	printf("%2d: write:  deadlock\n",id); fflush(stdout);
	return 0;
      }
      fprintf(stderr,"%2d: Can't lock table (%d)\n",id,my_errno);
      return 1;
    }
  }
  bzero((char*) &new_record,sizeof(new_record));
  strmov((char*) new_record.text,"Updated");

  found=next=prev=update=0;
  for (i=0 ; i < 100 ; i++)
  {
    tmp=rnd(100000);
    int4store(find,tmp);
    if (!maria_rkey(file,record.id,1,(uchar*) find, HA_WHOLE_KEY,
                    HA_READ_KEY_EXACT))
      found++;
    else
    {
      if (my_errno != HA_ERR_KEY_NOT_FOUND)
      {
	fprintf(stderr,"%2d: Got error %d from read in update\n",id,my_errno);
	return 1;
      }
      else if (!maria_rnext(file,record.id,1))
	next++;
      else
      {
	if (my_errno != HA_ERR_END_OF_FILE)
	{
	  fprintf(stderr,"%2d: Got error %d from rnext in update\n",
		  id,my_errno);
	  return 1;
	}
	else if (!maria_rprev(file,record.id,1))
	  prev++;
	else
	{
	  if (my_errno != HA_ERR_END_OF_FILE)
	  {
	    fprintf(stderr,"%2d: Got error %d from rnext in update\n",
		    id,my_errno);
	    return 1;
	  }
	  continue;
	}
      }
    }
    memcpy(new_record.id,record.id,sizeof(record.id));
    tmp=rnd(20000)+40000;
    int4store(new_record.nr,tmp);
    if (!maria_update(file,record.id,new_record.id))
      update++;
    else
    {
      if (my_errno != HA_ERR_RECORD_CHANGED &&
	  my_errno != HA_ERR_RECORD_DELETED &&
	  my_errno != HA_ERR_FOUND_DUPP_KEY)
      {
	fprintf(stderr,"%2d: Got error %d from update\n",id,my_errno);
	return 1;
      }
    }
  }
  if (lock)
  {
    if (maria_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"Can't unlock table,id, error%d\n",my_errno);
      return 1;
    }
  }
  printf("%2d: update: %5d\n",id,update); fflush(stdout);
  return 0;
}

#include "ma_check_standalone.h"

#else /* _WIN32 */

#include <stdio.h>

int main()
{
	fprintf(stderr,"this test has not been ported to Windows\n");
	return 0;
}

#endif /* _WIN32 */

