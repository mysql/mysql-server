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

/* Test av locking */

#include "nisam.h"
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


const char *filename= "test3.ISM";
uint tests=10,forks=10,key_cacheing=0,use_log=0;

static void get_options(int argc, char *argv[]);
void start_test(int id);
int test_read(N_INFO *,int),test_write(N_INFO *,int,int),
    test_update(N_INFO *,int,int),test_rrnd(N_INFO *,int);

struct record {
  char id[8];
  uint32 nr;
  char text[10];
} record;


int main(int argc,char **argv)
{
  int status,wait_ret;
  uint i;
  N_KEYDEF keyinfo[10];
  N_RECINFO recinfo[10];
  MY_INIT(argv[0]);

  get_options(argc,argv);

  keyinfo[0].seg[0].base.start=0;
  keyinfo[0].seg[0].base.length=8;
  keyinfo[0].seg[0].base.type=HA_KEYTYPE_TEXT;
  keyinfo[0].seg[0].base.flag=HA_SPACE_PACK;
  keyinfo[0].seg[1].base.type=0;
  keyinfo[0].base.flag = (uint8) HA_PACK_KEY;
  keyinfo[1].seg[0].base.start=8;
  keyinfo[1].seg[0].base.length=sizeof(uint32);
  keyinfo[1].seg[0].base.type=HA_KEYTYPE_LONG_INT;
  keyinfo[1].seg[0].base.flag=0;
  keyinfo[1].seg[1].base.type=0;
  keyinfo[1].base.flag =HA_NOSAME;

  recinfo[0].base.type=0;
  recinfo[0].base.length=sizeof(record.id);
  recinfo[1].base.type=0;
  recinfo[1].base.length=sizeof(record.nr);
  recinfo[2].base.type=0;
  recinfo[2].base.length=sizeof(record.text);
  recinfo[3].base.type=FIELD_LAST;

  puts("- Creating isam-file");
  my_delete(filename,MYF(0));		/* Remove old locks under gdb */
  if (nisam_create(filename,2,&keyinfo[0],&recinfo[0],10000,0,0,0,0L))
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
    VOID(rnd(1));
  }

  for (i=0 ; i < forks ; i++)
    while ((wait_ret=wait(&status)) && wait_ret == -1);
  return 0;
}


static void get_options(argc,argv)
int argc;
char *argv[];
{
  char *pos,*progname;
  DEBUGGER_OFF;

  progname= argv[0];

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'l':
      use_log=1;
      break;
    case 'f':
      forks=atoi(++pos);
      break;
    case 't':
      tests=atoi(++pos);
      break;
    case 'K':				/* Use key cacheing */
      key_cacheing=1;
      break;
    case 'A':				/* All flags */
      use_log=key_cacheing=1;
      break;
   case '?':
    case 'I':
    case 'V':
      printf("%s  Ver 1.0 for %s at %s\n",progname,SYSTEM_TYPE,MACHINE_TYPE);
      puts("TCX Datakonsult AB, by Monty, for your professional use\n");
      puts("Test av locking with threads\n");
      printf("Usage: %s [-?lKA] [-f#] [-t#]\n",progname);
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
}


void start_test(int id)
{
  uint i;
  int error,lock_type;
  N_ISAMINFO isam_info;
  N_INFO *file,*file1,*file2,*lock;

  if (use_log)
    nisam_log(1);
  if (!(file1=nisam_open(filename,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)) ||
      !(file2=nisam_open(filename,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)))
  {
    fprintf(stderr,"Can't open isam-file: %s\n",filename);
    exit(1);
  }
  if (key_cacheing && rnd(2) == 0)
    init_key_cache(65536L,(uint) IO_SIZE*4*10);
  printf("Process %d, pid: %d\n",id,getpid()); fflush(stdout);

  for (error=i=0 ; i < tests && !error; i++)
  {
    file= (rnd(2) == 1) ? file1 : file2;
    lock=0 ; lock_type=0;
    if (rnd(10) == 0)
    {
      if (nisam_lock_database(lock=(rnd(2) ? file1 : file2),
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
      nisam_lock_database(lock,F_UNLCK);
  }
  if (!error)
  {
    nisam_info(file1,&isam_info,0);
    printf("%2d: End of test.  Records:  %ld  Deleted:  %ld\n",
	   id,isam_info.records,isam_info.deleted);
    fflush(stdout);
  }

  nisam_close(file1);
  nisam_close(file2);
  if (use_log)
    nisam_log(0);
  if (error)
  {
    printf("%2d: Aborted\n",id); fflush(stdout);
    exit(1);
  }
}


int test_read(N_INFO *file,int id)
{
  uint i,lock,found,next,prev;
  ulong find;

  lock=0;
  if (rnd(2) == 0)
  {
    lock=1;
    if (nisam_lock_database(file,F_RDLCK))
    {
      fprintf(stderr,"%2d: Can't lock table %d\n",id,my_errno);
      return 1;
    }
  }

  found=next=prev=0;
  for (i=0 ; i < 100 ; i++)
  {
    find=rnd(100000);
    if (!nisam_rkey(file,record.id,1,(byte*) &find,
		 sizeof(find),HA_READ_KEY_EXACT))
      found++;
    else
    {
      if (my_errno != HA_ERR_KEY_NOT_FOUND)
      {
	fprintf(stderr,"%2d: Got error %d from read in read\n",id,my_errno);
	return 1;
      }
      else if (!nisam_rnext(file,record.id,1))
	next++;
      else
      {
	if (my_errno != HA_ERR_END_OF_FILE)
	{
	  fprintf(stderr,"%2d: Got error %d from rnext in read\n",id,my_errno);
	  return 1;
	}
	else if (!nisam_rprev(file,record.id,1))
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
    if (nisam_lock_database(file,F_UNLCK))
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


int test_rrnd(N_INFO *file,int id)
{
  uint count,lock;

  lock=0;
  if (rnd(2) == 0)
  {
    lock=1;
    if (nisam_lock_database(file,F_RDLCK))
    {
      fprintf(stderr,"%2d: Can't lock table (%d)\n",id,my_errno);
      nisam_close(file);
      return 1;
    }
    if (rnd(2) == 0)
      nisam_extra(file,HA_EXTRA_CACHE);
  }

  count=0;
  if (nisam_rrnd(file,record.id,0L))
  {
    if (my_errno == HA_ERR_END_OF_FILE)
      goto end;
    fprintf(stderr,"%2d: Can't read first record (%d)\n",id,my_errno);
    return 1;
  }
  for (count=1 ; !nisam_rrnd(file,record.id,NI_POS_ERROR) ;count++) ;
  if (my_errno != HA_ERR_END_OF_FILE)
  {
    fprintf(stderr,"%2d: Got error %d from rrnd\n",id,my_errno);
    return 1;
  }

end:
  if (lock)
  {
    nisam_extra(file,HA_EXTRA_NO_CACHE);
    if (nisam_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"%2d: Can't unlock table\n",id);
      exit(0);
    }
  }
  printf("%2d: rrnd:   %5d\n",id,count); fflush(stdout);
  return 0;
}


int test_write(N_INFO *file,int id,int lock_type)
{
  uint i,tries,count,lock;

  lock=0;
  if (rnd(2) == 0 || lock_type == F_RDLCK)
  {
    lock=1;
    if (nisam_lock_database(file,F_WRLCK))
    {
      if (lock_type == F_RDLCK && my_errno == EDEADLK)
      {
	printf("%2d: write:  deadlock\n",id); fflush(stdout);
	return 0;
      }
      fprintf(stderr,"%2d: Can't lock table (%d)\n",id,my_errno);
      nisam_close(file);
      return 1;
    }
    if (rnd(2) == 0)
      nisam_extra(file,HA_EXTRA_WRITE_CACHE);
  }

  sprintf(record.id,"%7d",getpid());
  strmov(record.text,"Testing...");

  tries=(uint) rnd(100)+10;
  for (i=count=0 ; i < tries ; i++)
  {
    record.nr=rnd(80000)+20000;
    if (!nisam_write(file,record.id))
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
    nisam_extra(file,HA_EXTRA_NO_CACHE);
    if (nisam_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"%2d: Can't unlock table\n",id);
      exit(0);
    }
  }
  printf("%2d: write:  %5d\n",id,count); fflush(stdout);
  return 0;
}


int test_update(N_INFO *file,int id,int lock_type)
{
  uint i,lock,found,next,prev,update;
  ulong find;
  struct record new_record;

  lock=0;
  if (rnd(2) == 0 || lock_type == F_RDLCK)
  {
    lock=1;
    if (nisam_lock_database(file,F_WRLCK))
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
  strmov(new_record.text,"Updated");

  found=next=prev=update=0;
  for (i=0 ; i < 100 ; i++)
  {
    find=rnd(100000);
    if (!nisam_rkey(file,record.id,1,(byte*) &find,
		 sizeof(find),HA_READ_KEY_EXACT))
      found++;
    else
    {
      if (my_errno != HA_ERR_KEY_NOT_FOUND)
      {
	fprintf(stderr,"%2d: Got error %d from read in update\n",id,my_errno);
	return 1;
      }
      else if (!nisam_rnext(file,record.id,1))
	next++;
      else
      {
	if (my_errno != HA_ERR_END_OF_FILE)
	{
	  fprintf(stderr,"%2d: Got error %d from rnext in update\n",
		  id,my_errno);
	  return 1;
	}
	else if (!nisam_rprev(file,record.id,1))
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
    memcpy_fixed(new_record.id,record.id,sizeof(record.id));
    new_record.nr=rnd(20000)+40000;
    if (!nisam_update(file,record.id,new_record.id))
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
    if (nisam_lock_database(file,F_UNLCK))
    {
      fprintf(stderr,"Can't unlock table,id, error%d\n",my_errno);
      return 1;
    }
  }
  printf("%2d: update: %5d\n",id,update); fflush(stdout);
  return 0;
}
