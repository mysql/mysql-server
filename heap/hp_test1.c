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

/* Test av heap-database */
/* Programmet skapar en heap-databas. Till denna skrivs ett antal poster.
   Databasen st{ngs. D{refter |ppnas den p} nytt och en del av posterna
   raderas.
*/

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "heap.h"

static int get_options(int argc, char *argv[]);

static int flag=0,verbose=0,remove_ant=0,flags[50];

int main(int argc, char **argv)
{
  int i,j,error,deleted;
  HP_INFO *file;
  char record[128],key[32];
  const char *filename;
  HP_KEYDEF keyinfo[10];
  HP_KEYSEG keyseg[4];
  MY_INIT(argv[0]);

  filename= "test1";
  get_options(argc,argv);

  keyinfo[0].keysegs=1;
  keyinfo[0].seg=keyseg;
  keyinfo[0].seg[0].type=HA_KEYTYPE_BINARY;
  keyinfo[0].seg[0].start=1;
  keyinfo[0].seg[0].length=6;
  keyinfo[0].flag = HA_NOSAME;

  deleted=0;
  bzero((gptr) flags,sizeof(flags));

  printf("- Creating heap-file\n");
  heap_create(filename);
  if (!(file=heap_open(filename,2,1,keyinfo,30,(ulong) flag*100000l,10l)))
    goto err;
  printf("- Writing records:s\n");
  strmov(record,"          ..... key           ");

  for (i=49 ; i>=1 ; i-=2 )
  {
    j=i%25 +1;
    sprintf(key,"%6d",j);
    bmove(record+1,key,6);
    error=heap_write(file,record);
    if (heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }
    flags[j]=1;
    if (verbose || error) printf("J= %2d  heap_write: %d  my_errno: %d\n",
       j,error,my_errno);
  }
  if (heap_close(file))
    goto err;
  printf("- Reopening file\n");
  if (!(file=heap_open(filename,2,1,keyinfo,30,(ulong) flag*100000l,10l)))
    goto err;

  printf("- Removing records\n");
  for (i=1 ; i<=10 ; i++)
  {
    if (i == remove_ant) { VOID(heap_close(file)) ; return (0) ; }
    sprintf(key,"%6d",(j=(int) ((rand() & 32767)/32767.*25)));
    if ((error = heap_rkey(file,record,0,key)))
    {
      if (verbose || (flags[j] == 1 ||
		      (error && my_errno != HA_ERR_KEY_NOT_FOUND)))
	printf("key: %s  rkey:   %3d  my_errno: %3d\n",key,error,my_errno);
    }
    else
    {
      error=heap_delete(file,record);
      if (error || verbose)
	printf("key: %s  delete: %d  my_errno: %d\n",key,error,my_errno);
      flags[j]=0;
      if (! error)
	deleted++;
    }
    if (heap_check_heap(file))
    {
      puts("Heap keys crashed");
      goto err;
    }
  }

  printf("- Reading records with key\n");
  for (i=1 ; i<=25 ; i++)
  {
    sprintf(key,"%6d",i);
    bmove(record+1,key,6);
    my_errno=0;
    error=heap_rkey(file,record,0,key);
    if (verbose ||
	(error == 0 && flags[i] != 1) ||
	(error && (flags[i] != 0 || my_errno != HA_ERR_KEY_NOT_FOUND)))
    {
      printf("key: %s  rkey: %3d  my_errno: %3d  record: %s\n",
	      key,error,my_errno,record+1);
    }
  }

#ifdef OLD_HEAP_VERSION
  {
    int found;
    printf("- Reading records with position\n");
    for (i=1,found=0 ; i <= 30 ; i++)
    {
      my_errno=0;
      if ((error=heap_rrnd(file,record,i == 1 ? 0L : (ulong) -1)) ==
	  HA_ERR_END_OF_FILE)
      {
	if (found != 25-deleted)
	  printf("Found only %d of %d records\n",found,25-deleted);
	break;
      }
      if (!error)
	found++;
      if (verbose || (error != 0 && error != HA_ERR_RECORD_DELETED))
      {
	printf("pos: %2d  ni_rrnd: %3d  my_errno: %3d  record: %s\n",
	       i-1,error,my_errno,record+1);
      }
    }
  }
#endif

  if (heap_close(file) || heap_panic(HA_PANIC_CLOSE))
    goto err;
  my_end(MY_GIVE_INFO);
  return(0);
err:
  printf("got error: %d when using heap-database\n",my_errno);
  return(1);
} /* main */


/* Read options */

static int get_options(int argc, char **argv)
{
  char *pos;

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'B':				/* Big file */
      flag=1;
      break;
    case 'v':				/* verbose */
      verbose=1;
      break;
    case 'm':
      remove_ant=atoi(++pos);
      break;
    case 'V':
      printf("hp_test1    Ver 3.0 \n");
      exit(0);
    case '#':
      DBUG_PUSH (++pos);
      break;
    }
  }
  return 0;
} /* get options */
