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

#include "isamdef.h"

static void get_options(int argc, char *argv[]);

static int rec_pointer_size=0,verbose=0,remove_ant=0,pack_keys=1,flags[50],
  packed_field=FIELD_SKIPP_PRESPACE;

int main(argc,argv)
int argc;
char *argv[];
{
  N_INFO *file;
  int i,j,error,deleted,found;
  char record[128],key[32],*filename,read_record[128];
  N_KEYDEF keyinfo[10];
  N_RECINFO recinfo[10];
  MY_INIT(argv[0]);

  filename= (char*) "test1";
  my_init();
  get_options(argc,argv);
  keyinfo[0].seg[0].base.type=HA_KEYTYPE_NUM;
  keyinfo[0].seg[0].base.flag=(uint8) (pack_keys ?
				       HA_PACK_KEY | HA_SPACE_PACK : 0);
  keyinfo[0].seg[0].base.start=0;
  keyinfo[0].seg[0].base.length=6;
  keyinfo[0].seg[1].base.type=HA_KEYTYPE_END;
  keyinfo[0].base.flag = (uint8) (pack_keys ?
				  HA_NOSAME | HA_PACK_KEY : HA_NOSAME);

  recinfo[0].base.type=packed_field; recinfo[0].base.length=6;
  recinfo[1].base.type=FIELD_NORMAL; recinfo[1].base.length=24;
  recinfo[2].base.type=FIELD_LAST;

  deleted=0;
  bzero((byte*) flags,sizeof(flags));

  printf("- Creating isam-file\n");
  if (nisam_create(filename,1,keyinfo,recinfo,
		(ulong) (rec_pointer_size ? (1L << (rec_pointer_size*8))/40 :
			 0),10l,0,0,0L))
    goto err;
  if (!(file=nisam_open(filename,2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;
  printf("- Writing key:s\n");
  strmov(record,"      ..... key"); strappend(record,30,' ');

  my_errno=0;
  for (i=49 ; i>=1 ; i-=2 )
  {
    j=i%25 +1;
    sprintf(key,"%6d",j);
    bmove(record,key,6);
    error=nisam_write(file,record);
    flags[j]=1;
    if (verbose || error)
      printf("J= %2d  nisam_write: %d  errno: %d\n", j,error,my_errno);
  }
  if (nisam_close(file)) goto err;
  printf("- Reopening file\n");
  if (!(file=nisam_open(filename,2,HA_OPEN_ABORT_IF_LOCKED))) goto err;
  printf("- Removing keys\n");
  for (i=1 ; i<=10 ; i++)
  {
    if (i == remove_ant) { VOID(nisam_close(file)) ; exit(0) ; }
    sprintf(key,"%6d",(j=(int) ((rand() & 32767)/32767.*25)));
    my_errno=0;
    if ((error = nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT)))
    {
      if (verbose || (flags[j] == 1 ||
		      (error && my_errno != HA_ERR_KEY_NOT_FOUND)))
	printf("key: %s  nisam_rkey:   %3d  errno: %3d\n",key,error,my_errno);
    }
    else
    {
      error=nisam_delete(file,read_record);
      if (verbose || error)
	printf("key: %s  nisam_delete: %3d  errno: %3d\n",key,error,my_errno);
      flags[j]=0;
      if (! error)
	deleted++;
    }
  }
  printf("- Reading records with key\n");
  for (i=1 ; i<=25 ; i++)
  {
    sprintf(key,"%6d",i);
    bmove(record,key,6);
    my_errno=0;
    error=nisam_rkey(file,read_record,0,key,0,HA_READ_KEY_EXACT);
    if (verbose ||
	(error == 0 && flags[i] != 1) ||
	(error && (flags[i] != 0 || my_errno != HA_ERR_KEY_NOT_FOUND)))
    {
      printf("key: %s  nisam_rkey: %3d  errno: %3d  record: %s\n",
	      key,error,my_errno,record+1);
    }
  }

  printf("- Reading records with position\n");
  for (i=1,found=0 ; i <= 30 ; i++)
  {
    my_errno=0;
    if ((error=nisam_rrnd(file,read_record,i == 1 ? 0L : NI_POS_ERROR)) == -1)
    {
      if (found != 25-deleted)
	printf("Found only %d of %d records\n",found,25-deleted);
      break;
    }
    if (!error)
      found++;
    if (verbose || (error != 0 && error != 1))
    {
      printf("pos: %2d  nisam_rrnd: %3d  errno: %3d  record: %s\n",
	     i-1,error,my_errno,read_record+1);
    }
  }
  if (nisam_close(file)) goto err;
  my_end(MY_CHECK_ERROR);

  exit(0);
err:
  printf("got error: %3d when using nisam-database\n",my_errno);
  exit(1);
  return 0;			/* skipp warning */
} /* main */


	/* l{ser optioner */
	/* OBS! intierar endast DEBUG - ingen debuggning h{r ! */

static void get_options(argc,argv)
int argc;
char *argv[];
{
  char *pos;

  while (--argc >0 && *(pos = *(++argv)) == '-' ) {
    switch(*++pos) {
    case 'R':				/* Length of record pointer */
      rec_pointer_size=atoi(++pos);
      if (rec_pointer_size > 3)
	rec_pointer_size=0;
      break;
    case 'P':
      pack_keys=0;			/* Don't use packed key */
      break;
    case 'S':
      packed_field=FIELD_NORMAL;	/* static-size record*/
      break;
    case 'v':				/* verbose */
      verbose=1;
      break;
    case 'm':
      remove_ant=atoi(++pos);
      break;
    case 'V':
      printf("isamtest1	 Ver 1.0 \n");
      exit(0);
    case '#':
      DEBUGGER_ON;
      DBUG_PUSH (++pos);
      break;
    }
  }
  return;
} /* get options */
