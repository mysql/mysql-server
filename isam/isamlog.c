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

/* write whats in isam.log */

#ifndef USE_MY_FUNC
#define USE_MY_FUNC
#endif

#include "isamdef.h"
#include <my_tree.h>
#include <stdarg.h>
#ifdef HAVE_GETRUSAGE
#include <sys/resource.h>
#endif

#define FILENAME(A) (A ? A->show_name : "Unknown")

struct file_info {
  long process;
  int  filenr,id;
  my_string name,show_name,record;
  N_INFO *isam;
  bool closed,used;
  ulong accessed;
};

struct test_if_open_param {
  my_string name;
  int max_id;
};

struct st_access_param
{
  ulong min_accessed;
  struct file_info *found;
};

#define NO_FILEPOS (ulong) ~0L

extern int main(int argc,char * *argv);
static void get_options(int *argc,char ***argv);
static int examine_log(my_string file_name,char **table_names);
static int read_string(IO_CACHE *file,gptr *to,uint length);
static int file_info_compare(void *a,void *b);
static int test_if_open(struct file_info *key,element_count count,
			struct test_if_open_param *param);
static void fix_blob_pointers(N_INFO *isam,byte *record);
static uint set_maximum_open_files(uint);
static int test_when_accessed(struct file_info *key,element_count count,
			      struct st_access_param *access_param);
static void file_info_free(struct file_info *info);
static int close_some_file(TREE *tree);
static int reopen_closed_file(TREE *tree,struct file_info *file_info);
static int find_record_with_key(struct file_info *file_info,byte *record);
static void printf_log(const char *str,...);
static bool cmp_filename(struct file_info *file_info,my_string name);

static uint verbose=0,update=0,test_info=0,max_files=0,re_open_count=0,
  recover=0,prefix_remove=0;
static my_string log_filename=0,filepath=0,write_filename=0,record_pos_file=0;
static ulong com_count[10][3],number_of_commands=(ulong) ~0L,start_offset=0,
	     record_pos= NO_FILEPOS,isamlog_filepos,isamlog_process;
static const char *command_name[]=
{"open","write","update","delete","close","extra","lock","re-open",NullS};


int main(argc,argv)
int argc;
char **argv;
{
  int error,i,first;
  ulong total_count,total_error,total_recover;
  MY_INIT(argv[0]);

  log_filename=nisam_log_filename;
  get_options(&argc,&argv);
 /* Nr of isam-files */
  max_files=(set_maximum_open_files(min(max_files,8))-6)/2;

  if (update)
    printf("Trying to %s isamfiles according to log '%s'\n",
	   (recover ? "recover" : "update"),log_filename);
  error= examine_log(log_filename,argv);
  if (update && ! error)
    puts("isamfile:s updated successfully");
  total_count=total_error=total_recover=0;
  for (i=first=0 ; command_name[i] ; i++)
  {
    if (com_count[i][0])
    {
      if (!first++)
      {
	if (verbose || update)
	  puts("");
	puts("Commands   Used count    Errors   Recover errors");
      }
      printf("%-12s%9ld%10ld%17ld\n",command_name[i],com_count[i][0],
	     com_count[i][1],com_count[i][2]);
      total_count+=com_count[i][0];
      total_error+=com_count[i][1];
      total_recover+=com_count[i][2];
    }
  }
  if (total_count)
    printf("%-12s%9ld%10ld%17ld\n","Total",total_count,total_error,
	   total_recover);
  if (re_open_count)
    printf("Had to do %d re-open because of too few possibly open files\n",
	   re_open_count);
  VOID(nisam_panic(HA_PANIC_CLOSE));
  my_end(test_info ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  exit(error);
  return 0;				/* No compiler warning */
} /* main */


static void get_options(argc,argv)
register int *argc;
register char ***argv;
{
  int help,version;
  const char *usage;
  char *pos, option;

  help=0;
  usage="Usage: %s [-?iruvIV] [-c #] [-f #] [-F filepath/] [-o #] [-R file recordpos] [-w write_file] [log-filename [table ...]] \n";
  pos= (char*) "";

  while (--*argc > 0 && *(pos = *(++*argv)) == '-' ) {
    while (*++pos)
    {
      version=0;
      switch((option=*pos)) {
      case '#':
	DBUG_PUSH (++pos);
	pos= (char*) " ";			/* Skipp rest of arg */
	break;
      case 'c':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	number_of_commands=(ulong) atol(pos);
	pos= (char*) " ";
	break;
      case 'u':
	update=1;
	break;
      case 'f':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	max_files=(uint) atoi(pos);
	pos= (char*) " ";
	break;
      case 'i':
	test_info=1;
	break;
      case 'o':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	start_offset=(ulong) atol(pos);
	pos= (char*) " ";
	break;
      case 'p':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	prefix_remove=atoi(pos);
	break;
      case 'r':
	update=1;
	recover++;
	break;
      case 'R':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	record_pos_file=pos;
	if (!--*argc)
	  goto err;
	record_pos=(ulong) atol(*(++*argv));
	pos= (char*) " ";
	break;
      case 'v':
	verbose++;
	break;
      case 'w':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	write_filename=pos;
	pos= (char*) " ";
	break;
      case 'F':
	if (! *++pos)
	{
	  if (!--*argc)
	    goto err;
	  else
	    pos= *(++*argv);
	}
	filepath=pos;
	pos= (char*) " ";
	break;
      case 'V':
	version=1;
	/* Fall through */
      case 'I':
      case '?':
	printf("%s  Ver 3.2 for %s at %s\n",my_progname,SYSTEM_TYPE,
	       MACHINE_TYPE);
	puts("TCX Datakonsult AB, by Monty, for your professional use\n");
	if (version)
	  break;
	puts("Write info about whats in a nisam log file.");
	printf("If no file name is given %s is used\n",log_filename);
	puts("");
	printf(usage,my_progname);
	puts("");
	puts("Options: -? or -I \"Info\"     -V \"version\"   -c \"do only # commands\"");
	puts("         -f \"max open files\" -F \"filepath\"  -i \"extra info\"");
	puts("         -o \"offset\"         -p # \"remove # components from path\"");
	puts("         -r \"recover\"        -R \"file recordposition\"");
	puts("         -u \"update\"         -v \"verbose\"   -w \"write file\"");
	puts("\nOne can give a second and a third '-v' for more verbose.");
	puts("Normaly one does a update (-u).");
	puts("If a recover is done all writes and all possibly updates and deletes is done\nand errors are only counted.");
	puts("If one gives table names as arguments only these tables will be updated\n");
	help=1;
	break;
      default:
	printf("illegal option: \"-%c\"\n",*pos);
	break;
      }
    }
  }
  if (! *argc)
  {
    if (help)
    exit(0);
    (*argv)++;
  }
  if (*argc >= 1)
  {
    log_filename=pos;
    (*argc)--;
    (*argv)++;
  }
  return;
 err:
  VOID(fprintf(stderr,"option \"%c\" used without or with wrong argument\n",
	       option));
  exit(1);
}


static int examine_log(my_string file_name, char **table_names)
{
  uint command,result,files_open;
  ulong access_time,length;
  uint32 filepos;
  int lock_command,ni_result;
  char isam_file_name[FN_REFLEN];
  uchar head[20];
  gptr	buff;
  struct test_if_open_param open_param;
  IO_CACHE cache;
  File file;
  FILE *write_file;
  enum ha_extra_function extra_command;
  TREE tree;
  struct file_info file_info,*curr_file_info;
  DBUG_ENTER("examine_log");

  if ((file=my_open(file_name,O_RDONLY,MYF(MY_WME))) < 0)
    DBUG_RETURN(1);
  write_file=0;
  if (write_filename)
  {
    if (!(write_file=my_fopen(write_filename,O_WRONLY,MYF(MY_WME))))
    {
      my_close(file,MYF(0));
      DBUG_RETURN(1);
    }
  }

  init_io_cache(&cache,file,0,READ_CACHE,start_offset,0,MYF(0));
  bzero((gptr) com_count,sizeof(com_count));
  init_tree(&tree,0,0,sizeof(file_info),(qsort_cmp2) file_info_compare,1,
	    (tree_element_free) file_info_free, NULL);
  VOID(init_key_cache(KEY_CACHE_SIZE,(uint) (10*4*(IO_SIZE+MALLOC_OVERHEAD))));

  files_open=0; access_time=0;
  while (access_time++ != number_of_commands &&
	 !my_b_read(&cache,(byte*) head,9))
  {
    isamlog_filepos=my_b_tell(&cache)-9L;
    file_info.filenr=uint2korr(head+1);
    isamlog_process=file_info.process=(long) uint4korr(head+3);
    result=uint2korr(head+7);
    if ((curr_file_info=(struct file_info*) tree_search(&tree,&file_info)))
    {
      curr_file_info->accessed=access_time;
      if (update && curr_file_info->used && curr_file_info->closed)
      {
	if (reopen_closed_file(&tree,curr_file_info))
	{
	  command=sizeof(com_count)/sizeof(com_count[0][0])/3;
	  result=0;
	  goto com_err;
	}
      }
    }
    command=(uint) head[0];
    if (command < sizeof(com_count)/sizeof(com_count[0][0])/3 &&
	(!curr_file_info || curr_file_info->used))
    {
      com_count[command][0]++;
      if (result)
	com_count[command][1]++;
    }
    switch ((enum nisam_log_commands) command) {
    case LOG_OPEN:
      com_count[command][0]--;			/* Must be counted explicite */
      if (result)
	com_count[command][1]--;

      if (curr_file_info)
	printf("\nWarning: %s is opened twice with same process and filenumber\n",
	       curr_file_info->show_name);
      if (my_b_read(&cache,(byte*) head,2))
	goto err;
      file_info.name=0;
      file_info.show_name=0;
      file_info.record=0;
      if (read_string(&cache,(gptr*) &file_info.name,(uint) uint2korr(head)))
	goto err;
      {
	uint i;
	char *pos=file_info.name,*to;
	for (i=0 ; i < prefix_remove ; i++)
	{
	  char *next;
	  if (!(next=strchr(pos,FN_LIBCHAR)))
	    break;
	  pos=next+1;
	}
	to=isam_file_name;
	if (filepath)
	{
	  strmov(isam_file_name,filepath);
	  convert_dirname(isam_file_name);
	  to=strend(isam_file_name);
	}
	strmov(to,pos);
	fn_ext(isam_file_name)[0]=0;	/* Remove extension */
      }
      open_param.name=file_info.name;
      open_param.max_id=0;
      VOID(tree_walk(&tree,(tree_walk_action) test_if_open,(void*) &open_param,
		     left_root_right));
      file_info.id=open_param.max_id+1;
      file_info.show_name=my_memdup(isam_file_name,
				    (uint) strlen(isam_file_name)+6,
				    MYF(MY_WME));
      if (file_info.id > 1)
	sprintf(strend(file_info.show_name),"<%d>",file_info.id);
      file_info.closed=1;
      file_info.accessed=access_time;
      file_info.used=1;
      if (table_names[0])
      {
	char **name;
	file_info.used=0;
	for (name=table_names ; *name ; name++)
	{
	  if (!strcmp(*name,isam_file_name))
	    file_info.used=1;			/* Update/log only this */
	}
      }
      if (update && file_info.used)
      {
	if (files_open >= max_files)
	{
	  if (close_some_file(&tree))
	    goto com_err;
	  files_open--;
	}
	if (!(file_info.isam= nisam_open(isam_file_name,O_RDWR,
				      HA_OPEN_WAIT_IF_LOCKED)))
	  goto com_err;
	if (!(file_info.record=my_malloc(file_info.isam->s->base.reclength,
					 MYF(MY_WME))))
	  goto end;
	files_open++;
	file_info.closed=0;
      }
      VOID(tree_insert(&tree,(gptr) &file_info,0));
      if (file_info.used)
      {
	if (verbose && !record_pos_file)
	  printf_log("%s: open",file_info.show_name);
	com_count[command][0]++;
	if (result)
	  com_count[command][1]++;
      }
      break;
    case LOG_CLOSE:
      if (verbose && !record_pos_file &&
	  (!table_names[0] || (curr_file_info && curr_file_info->used)))
	printf_log("%s: %s -> %d",FILENAME(curr_file_info),
	       command_name[command],result);
      if (curr_file_info)
      {
	if (!curr_file_info->closed)
	  files_open--;
	file_info_free(curr_file_info);
	VOID(tree_delete(&tree,(gptr) curr_file_info));
      }
      break;
    case LOG_EXTRA:
      if (my_b_read(&cache,(byte*) head,sizeof(extra_command)))
	goto err;
      memcpy_fixed(&extra_command,head,sizeof(extra_command));
      if (verbose && !record_pos_file &&
	  (!table_names[0] || (curr_file_info && curr_file_info->used)))
	printf_log("%s: %s(%d) -> %d",FILENAME(curr_file_info),
		   command_name[command], extra_command,result);
      if (update && curr_file_info && !curr_file_info->closed)
      {
	if (nisam_extra(curr_file_info->isam,extra_command) != (int) result)
	{
	  VOID(fprintf(stderr,
		       "Warning: error %d, expected %d on command %s at %lx\n",
		       my_errno,result,command_name[command],isamlog_filepos));
	}
      }
      break;
    case LOG_DELETE:
      if (my_b_read(&cache,(byte*) head,sizeof(filepos)))
	goto err;
      memcpy_fixed(&filepos,head,sizeof(filepos));
      if (verbose && (!record_pos_file ||
		      ((record_pos == filepos || record_pos == NO_FILEPOS) &&
		       !cmp_filename(curr_file_info,record_pos_file))) &&
	  (!table_names[0] || (curr_file_info && curr_file_info->used)))
	printf_log("%s: %s at %ld -> %d",FILENAME(curr_file_info),
		   command_name[command],(long) filepos,result);
      if (update && curr_file_info && !curr_file_info->closed)
      {
	if (nisam_rrnd(curr_file_info->isam,curr_file_info->record,filepos))
	{
	  if (!recover)
	    goto com_err;
	  com_count[command][2]++;		/* Mark error */
	}
	ni_result=nisam_delete(curr_file_info->isam,curr_file_info->record);
	if ((ni_result == 0 && result) ||
	    (ni_result && (uint) my_errno != result))
	{
	  if (!recover)
	    goto com_err;
	  if (ni_result)
	    com_count[command][2]++;		/* Mark error */
	}
      }
      break;
    case LOG_WRITE:
    case LOG_UPDATE:
      if (my_b_read(&cache,(byte*) head,8))
	goto err;
      filepos=uint4korr(head);
      length=uint4korr(head+4);
      buff=0;
      if (read_string(&cache,&buff,(uint) length))
	goto err;
      if ((!record_pos_file ||
	  ((record_pos == filepos || record_pos == NO_FILEPOS) &&
	   !cmp_filename(curr_file_info,record_pos_file))) &&
	  (!table_names[0] || (curr_file_info && curr_file_info->used)))
      {
	if (write_file &&
	    (my_fwrite(write_file,buff,length,MYF(MY_WAIT_IF_FULL | MY_NABP))))
	  goto end;
	if (verbose)
	  printf_log("%s: %s at %ld, length=%ld -> %d",
		     FILENAME(curr_file_info),
		     command_name[command], filepos,length,result);
      }
      if (update && curr_file_info && !curr_file_info->closed)
      {
	if (curr_file_info->isam->s->base.blobs)
	  fix_blob_pointers(curr_file_info->isam,buff);
	if ((enum nisam_log_commands) command == LOG_UPDATE)
	{
	  if (nisam_rrnd(curr_file_info->isam,curr_file_info->record,filepos))
	  {
	    if (!recover)
	    {
	      result=0;
	      goto com_err;
	    }
	    if (recover == 1 || result ||
		find_record_with_key(curr_file_info,buff))
	    {
	      com_count[command][2]++;		/* Mark error */
	      break;
	    }
	  }
	  ni_result=nisam_update(curr_file_info->isam,curr_file_info->record,
			      buff);
	  if ((ni_result == 0 && result) ||
	      (ni_result && (uint) my_errno != result))
	  {
	    if (!recover)
	      goto com_err;
	    if (ni_result)
	      com_count[command][2]++;		/* Mark error */
	  }
	}
	else
	{
	  ni_result=nisam_write(curr_file_info->isam,buff);
	  if ((ni_result == 0 && result) ||
	      (ni_result && (uint) my_errno != result))
	  {
	    if (!recover)
	      goto com_err;
	    if (ni_result)
	      com_count[command][2]++;		/* Mark error */
	  }
	  if (! recover && filepos != curr_file_info->isam->lastpos)
	  {
	    printf("Warning: Wrote at position: %ld, should have been %ld",
		   curr_file_info->isam->lastpos,(long) filepos);
	    goto com_err;
	  }
	}
      }
      my_free(buff,MYF(0));
      break;
    case LOG_LOCK:
      if (my_b_read(&cache,(byte*) head,sizeof(lock_command)))
	goto err;
      memcpy_fixed(&lock_command,head,sizeof(lock_command));
      if (verbose && !record_pos_file &&
	  (!table_names[0] || (curr_file_info && curr_file_info->used)))
	printf_log("%s: %s(%d) -> %d\n",FILENAME(curr_file_info),
		   command_name[command],lock_command,result);
      if (update && curr_file_info && !curr_file_info->closed)
      {
	if (nisam_lock_database(curr_file_info->isam,lock_command) !=
	    (int) result)
	  goto com_err;
      }
      break;
    default:
      VOID(fprintf(stderr,
		   "Error: found unknown command %d in logfile, aborted\n",
		   command));
      goto end;
    }
  }
  end_key_cache();
  delete_tree(&tree);
  VOID(end_io_cache(&cache));
  VOID(my_close(file,MYF(0)));
  if (write_file && my_fclose(write_file,MYF(MY_WME)))
    DBUG_RETURN(1);
  DBUG_RETURN(0);

 err:
  VOID(fprintf(stderr,"Got error %d when reading from logfile\n",my_errno));
  goto end;
 com_err:
  VOID(fprintf(stderr,"Got error %d, expected %d on command %s at %lx\n",
	       my_errno,result,command_name[command],isamlog_filepos));
 end:
  end_key_cache();
  delete_tree(&tree);
  VOID(end_io_cache(&cache));
  VOID(my_close(file,MYF(0)));
  if (write_file)
    VOID(my_fclose(write_file,MYF(MY_WME)));
  DBUG_RETURN(1);
}


static int read_string(file,to,length)
IO_CACHE *file;
reg1 gptr *to;
reg2 uint length;
{
  DBUG_ENTER("read_string");

  if (*to)
    my_free((gptr) *to,MYF(0));
  if (!(*to= (gptr) my_malloc(length+1,MYF(MY_WME))) ||
      my_b_read(file,(byte*) *to,length))
  {
    if (*to)
      my_free(*to,MYF(0));
    *to= 0;
    DBUG_RETURN(1);
  }
  *((char*) *to+length)= '\0';
  DBUG_RETURN (0);
}				/* read_string */


static int file_info_compare(a,b)
void *a;
void *b;
{
  long lint;

  if ((lint=((struct file_info*) a)->process -
       ((struct file_info*) b)->process))
    return lint < 0L ? -1 : 1;
  return ((struct file_info*) a)->filenr - ((struct file_info*) b)->filenr;
}

	/* ARGSUSED */

static int test_if_open (key,count,param)
struct file_info *key;
element_count count __attribute__((unused));
struct test_if_open_param *param;
{
  if (!strcmp(key->name,param->name) && key->id > param->max_id)
    param->max_id=key->id;
  return 0;
}


static void fix_blob_pointers(info,record)
N_INFO *info;
byte *record;
{
  byte *pos;
  N_BLOB *blob,*end;

  pos=record+info->s->base.reclength;
  for (end=info->blobs+info->s->base.blobs, blob= info->blobs;
       blob != end ;
       blob++)
  {
    bmove(record+blob->offset+blob->pack_length,&pos,sizeof(char*));
    pos+=_calc_blob_length(blob->pack_length,record+blob->offset);
  }
}

static uint set_maximum_open_files(maximum_files)
uint maximum_files;
{
#if defined(HAVE_GETRUSAGE) && defined(RLIMIT_NOFILE)
  struct rlimit rlimit;
  int old_max;

  if (maximum_files > MY_NFILE)
    maximum_files=MY_NFILE;			/* Don't crash my_open */

  if (!getrlimit(RLIMIT_NOFILE,&rlimit))
  {
    old_max=rlimit.rlim_max;
    if (maximum_files && (int) maximum_files > old_max)
      rlimit.rlim_max=maximum_files;
    rlimit.rlim_cur=rlimit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE,&rlimit))
    {
      if (old_max != (int) maximum_files)
      {						/* Set as much as we can */
	rlimit.rlim_max=rlimit.rlim_cur=old_max;
	setrlimit(RLIMIT_NOFILE,&rlimit);
      }
    }
    getrlimit(RLIMIT_NOFILE,&rlimit);		/* Read if broken setrlimit */
    if (maximum_files && maximum_files < rlimit.rlim_cur)
      VOID(fprintf(stderr,"Warning: Error from setrlimit: Max open files is %d\n",old_max));
    return rlimit.rlim_cur;
  }
#endif
  return min(maximum_files,MY_NFILE);
}

	/* close the file with hasn't been accessed for the longest time */
	/* ARGSUSED */

static int test_when_accessed (key,count,access_param)
struct file_info *key;
element_count count __attribute__((unused));
struct st_access_param *access_param;
{
  if (key->accessed < access_param->min_accessed && ! key->closed)
  {
    access_param->min_accessed=key->accessed;
    access_param->found=key;
  }
  return 0;
}


static void file_info_free(fileinfo)
struct file_info *fileinfo;
{
  if (update)
  {
    if (!fileinfo->closed)
      VOID(nisam_close(fileinfo->isam));
    if (fileinfo->record)
      my_free(fileinfo->record,MYF(0));
  }
  my_free(fileinfo->name,MYF(0));
  my_free(fileinfo->show_name,MYF(0));
}



static int close_some_file(tree)
TREE *tree;
{
  struct st_access_param access_param;

  access_param.min_accessed=LONG_MAX;
  access_param.found=0;

  VOID(tree_walk(tree,(tree_walk_action) test_when_accessed,
		 (void*) &access_param,left_root_right));
  if (!access_param.found)
    return 1;			/* No open file that is possibly to close */
  if (nisam_close(access_param.found->isam))
    return 1;
  access_param.found->closed=1;
  return 0;
}


static int reopen_closed_file(tree,fileinfo)
TREE *tree;
struct file_info *fileinfo;
{
  char name[FN_REFLEN];
  if (close_some_file(tree))
    return 1;				/* No file to close */
  strmov(name,fileinfo->show_name);
  if (fileinfo->id > 1)
    *strrchr(name,'<')='\0';		/* Remove "<id>" */

  if (!(fileinfo->isam= nisam_open(name,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)))
    return 1;
  fileinfo->closed=0;
  re_open_count++;
  return 0;
}

	/* Try to find record with uniq key */

static int find_record_with_key(file_info,record)
struct file_info *file_info;
byte *record;
{
  uint key;
  N_INFO *info=file_info->isam;
  uchar tmp_key[N_MAX_KEY_BUFF];

  for (key=0 ; key < info->s->state.keys ; key++)
  {
    if (info->s->keyinfo[key].base.flag & HA_NOSAME)
    {
      VOID(_nisam_make_key(info,key,tmp_key,record,0L));
      return nisam_rkey(info,file_info->record,(int) key,(char*) tmp_key,0,
		     HA_READ_KEY_EXACT);
    }
  }
  return 1;
}


static void printf_log(const char *format,...)
{
  va_list args;
  va_start(args,format);
  if (verbose > 2)
    printf("%9ld:",isamlog_filepos);
  if (verbose > 1)
    printf("%5ld ",isamlog_process);	/* Write process number */
  (void) vprintf((char*) format,args);
  putchar('\n');
  va_end(args);
}


static bool cmp_filename(file_info,name)
struct file_info *file_info;
my_string name;
{
  if (!file_info)
    return 1;
  return strcmp(file_info->name,name) ? 1 : 0;
}
