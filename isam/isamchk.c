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

/* Descript, check and repair of ISAM tables */

#include "isamdef.h"

#include <m_ctype.h>
#include <stdarg.h>
#include <getopt.h>
#ifdef HAVE_SYS_VADVICE_H
#include <sys/vadvise.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
SET_STACK_SIZE(9000)			/* Minimum stack size for program */

#define T_VERBOSE	1
#define T_SILENT	2
#define T_DESCRIPT	4
#define T_EXTEND	8
#define T_INFO		16
#define T_REP		32
#define T_OPT		64		/* Not currently used */
#define T_FORCE_CREATE	128
#define T_WRITE_LOOP	256
#define T_UNPACK	512
#define T_STATISTICS	1024
#define T_VERY_SILENT	2048
#define T_SORT_RECORDS	4096
#define T_SORT_INDEX	8192
#define T_WAIT_FOREVER	16384
#define T_REP_BY_SORT	32768L


#define O_NEW_INDEX	1		/* Bits set in out_flag */
#define O_NEW_DATA	2

#if defined(_MSC_VER) && !defined(__WIN__)
#define USE_BUFFER_INIT		250L*1024L
#define READ_BUFFER_INIT	((uint) 32768-MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT	(uint) (65536L-MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER		(1024*16-MALLOC_OVERHEAD)
#else
#define USE_BUFFER_INIT		(((1024L*512L-MALLOC_OVERHEAD)/IO_SIZE)*IO_SIZE)
#define READ_BUFFER_INIT	(1024L*256L-MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT	(2048L*1024L-MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER		(4096-MALLOC_OVERHEAD)
#endif

#define NEED_MEM	((uint) 10*4*(IO_SIZE+32)+32) /* Nead for recursion */
#define MAXERR			20
#define BUFFERS_WHEN_SORTING	16		/* Alloc for sort-key-tree */
#define WRITE_COUNT		MY_HOW_OFTEN_TO_WRITE
#define INDEX_TMP_EXT		".TMM"
#define DATA_TMP_EXT		".TMD"
#define MYF_RW	MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL)

#define UPDATE_TIME		1
#define UPDATE_STAT		2
#define UPDATE_SORT		4

typedef struct st_isam_sort_key_blocks {	/* Used when sorting */
  uchar *buff,*end_pos;
  uchar lastkey[N_MAX_POSSIBLE_KEY_BUFF];
  uint last_length;
  int inited;
} ISAM_SORT_KEY_BLOCKS;

typedef struct st_isam_sort_info {
  N_INFO *info;
  enum data_file_type new_data_file_type;
  ISAM_SORT_KEY_BLOCKS *key_block,*key_block_end;
  uint key,find_length;
  ulong pos,max_pos,filepos,start_recpos,filelength,dupp,max_records,unique,
    buff_length;
  my_bool fix_datafile;
  char *record,*buff;
  N_KEYDEF *keyinfo;
  N_KEYSEG *keyseg;
} ISAM_SORT_INFO;

enum ic_options {OPT_CHARSETS_DIR_IC=256};

static ulong	use_buffers=0,read_buffer_length=0,write_buffer_length=0,
		sort_buffer_length=0,sort_key_blocks=0,crc=0,unique_count=0;
static uint testflag=0,out_flag=0,warning_printed=0,error_printed=0,
	    rep_quick=0,verbose=0,opt_follow_links=1;
static uint opt_sort_key=0,total_files=0,max_level=0,max_key=N_MAXKEY;
static ulong keydata=0,totaldata=0,key_blocks=0;
static ulong new_file_pos=0,record_checksum=0,key_file_blocks=0,decode_bits;
static ulong total_records=0,total_deleted=0;
static ulong search_after_block=NI_POS_ERROR;
static byte *record_buff;
static char **defaults_alloc;
static const char *type_names[]=
{ "?","text","binary", "short", "long", "float",
  "double","number","unsigned short",
  "unsigned long","longlong","ulonglong","int24",
  "uint24","int8","?",},
  *packed_txt="packed ",
  *diff_txt="stripped ",
  *field_pack[]={"","no endspace", "no prespace",
		 "no zeros", "blob", "constant", "table-lookup",
		 "always zero","?","?",};

static char temp_filename[FN_REFLEN], *isam_file_name, *default_charset; 
static IO_CACHE read_cache;
static ISAM_SORT_INFO sort_info;
static int tmpfile_createflag=O_RDWR | O_TRUNC | O_EXCL;

static const char *load_default_groups[]= { "isamchk",0 };

	/* Functions defined in this file */

extern int main(int argc,char * *argv);
extern void print_error _VARARGS((const char *fmt,...));
static void print_warning _VARARGS((const char *fmt,...));
static void print_info _VARARGS((const char *fmt,...));
static int nisamchk(char *filename);
static void get_options(int *argc,char * * *argv);
static int chk_del(N_INFO *info,uint testflag);
static int check_k_link(N_INFO *info,uint nr);
static int chk_size(N_INFO *info);
static int chk_key(N_INFO *info);
static int chk_index(N_INFO *info, N_KEYDEF *keyinfo, ulong page, uchar *buff,
		     ulong *keys, uint level);
static uint isam_key_length(N_INFO *info,N_KEYDEF *keyinfo);
static unsigned long calc_checksum(ulong count);
static int chk_data_link(N_INFO *info,int extend);
static int rep(N_INFO *info,char *name);
static int writekeys(N_INFO *info,byte *buff,ulong filepos);
static void descript(N_INFO *info,char *name);
static int movepoint(N_INFO *info,byte *record,ulong oldpos,ulong newpos,
		     uint prot_key);
static void lock_memory(void);
static int flush_blocks(File file);
static int sort_records(N_INFO *,my_string,uint,int);
static int sort_index(N_INFO *info,my_string name);
static int sort_record_index(N_INFO *info,N_KEYDEF *keyinfo,ulong page,
			     uchar *buff,uint sortkey,File new_file);
static int sort_one_index(N_INFO *info,N_KEYDEF *keyinfo,uchar *buff,
			  File new_file);
static int change_to_newfile(const char * filename,const char * old_ext,
			     const char * new_ext);
static int lock_file(File file,ulong start,int lock_type,const char* filetype,
		     const char *filename);
static int filecopy(File to,File from,ulong start,ulong length,
		    const char * type);

static void print_version(void);
static int rep_by_sort(N_INFO *info,my_string name);
static int sort_key_read(void *key);
static int sort_get_next_record(void);
static int sort_write_record(void);
static int sort_key_cmp(const void *not_used, const void *a,const void *b);
static int sort_key_write(const void *a);
static ulong get_record_for_key(N_INFO *info,N_KEYDEF *keyinfo,
				uchar *key);
static int sort_insert_key(reg1 ISAM_SORT_KEY_BLOCKS *key_block,uchar *key,
			   ulong prev_block);
static int sort_delete_record(void);
static void usage(void);
static int flush_pending_blocks(void);
static ISAM_SORT_KEY_BLOCKS	*alloc_key_blocks(uint blocks,uint buffer_length);
static int test_if_almost_full(N_INFO *info);
static int recreate_database(N_INFO **info,char *filename);
static void save_integer(byte *pos,uint pack_length,ulong value);
static int write_data_suffix(N_INFO *info);
static int update_state_info(N_INFO *info,uint update);


	/* Main program */

int main( int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);

#ifdef __EMX__
  _wildcard (&argc, &argv);
#endif

  get_options(&argc,(char***) &argv);
  nisam_quick_table_bits=(uint) decode_bits;
  error=0;
  while (--argc >= 0)
  {
    error|= nisamchk(*(argv++));
    VOID(fflush(stdout));
    VOID(fflush(stderr));
    if ((error_printed | warning_printed) && (testflag & T_FORCE_CREATE) &&
	(!(testflag & (T_REP | T_REP_BY_SORT | T_SORT_RECORDS |
		       T_SORT_INDEX))))
    {
      testflag|=T_REP;
      error|=nisamchk(argv[-1]);
      testflag&= ~T_REP;
      VOID(fflush(stdout));
      VOID(fflush(stderr));
    }
    if (argc && (!(testflag & T_SILENT) || testflag & T_INFO))
    {
      puts("\n---------\n");
      VOID(fflush(stdout));
    }
  }
  if (total_files > 1)
  {					/* Only if descript */
    if (!(testflag & T_SILENT) || testflag & T_INFO)
      puts("\n---------\n");
    printf("\nTotal of all %d ISAM-files:\nData records: %8lu   Deleted blocks: %8lu\n",total_files,total_records,total_deleted);
  }
  free_defaults(defaults_alloc);
  my_end(testflag & T_INFO ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  exit(error);
#ifndef _lint
  return 0;				/* No compiler warning */
#endif
} /* main */


static CHANGEABLE_VAR changeable_vars[] = {
  { "key_buffer_size",(long*) &use_buffers,(long) USE_BUFFER_INIT,
    (long) MALLOC_OVERHEAD, (long) ~0L,(long) MALLOC_OVERHEAD,(long) IO_SIZE },
  { "read_buffer_size", (long*) &read_buffer_length,(long) READ_BUFFER_INIT,
      (long) MALLOC_OVERHEAD,(long) ~0L,(long) MALLOC_OVERHEAD,(long) 1L },
  { "write_buffer_size", (long*) &write_buffer_length,(long) READ_BUFFER_INIT,
      (long) MALLOC_OVERHEAD,(long) ~0L,(long) MALLOC_OVERHEAD,(long) 1L },
  { "sort_buffer_size",(long*) &sort_buffer_length,(long) SORT_BUFFER_INIT,
      (long) (MIN_SORT_BUFFER+MALLOC_OVERHEAD),(long) ~0L,
      (long) MALLOC_OVERHEAD,(long) 1L },
  { "sort_key_blocks",(long*) &sort_key_blocks,BUFFERS_WHEN_SORTING,4L,100L,0L,
    1L },
  { "decode_bits",(long*) &decode_bits,9L,4L,17L,0L,1L },
  { NullS,(long*) 0,0L,0L,0L,0L,0L,} };


static struct option long_options[] =
{
  {"analyze",          no_argument,       0, 'a'},
  {"character-sets-dir", required_argument, 0, OPT_CHARSETS_DIR_IC},
#ifndef DBUG_OFF
  {"debug",            required_argument, 0, '#'},
#endif
  {"default-character-set", required_argument, 0, 'C'},
  {"description",      no_argument,       0, 'd'},
  {"extend-check",     no_argument,       0, 'e'},
  {"information",      no_argument,       0, 'i'},
  {"force",            no_argument,       0, 'f'},
  {"help",             no_argument,       0, '?'},
  {"keys-used",        required_argument, 0, 'k'},
  {"no-symlinks",      no_argument,       0, 'l'},
  {"quick",            no_argument,       0, 'q'},
  {"recover",          no_argument,       0, 'r'},
  {"safe-recover",     no_argument,       0, 'o'},
  {"block-search",     required_argument, 0, 'b'},
  {"set-variable",     required_argument, 0, 'O'},
  {"silent",           no_argument,       0, 's'},
  {"sort-index",       no_argument,       0, 'S'},
  {"sort-records",     required_argument, 0, 'R'},
  {"unpack",           no_argument,       0, 'u'},
  {"verbose",          no_argument,       0, 'v'},
  {"version",          no_argument,       0, 'V'},
  {"wait",             no_argument,       0, 'w'},
  {0, 0, 0, 0}
};

static void print_version(void)
{
  printf("%s  Ver 5.17 for %s at %s\n",my_progname,SYSTEM_TYPE,
	 MACHINE_TYPE);
}

static void usage(void)
{
  uint i;
  print_version();
  puts("TCX Datakonsult AB, by Monty, for your professional use");
  puts("This software comes with NO WARRANTY: see the PUBLIC for details.\n");
  puts("Description, check and repair of ISAM tables.");
  puts("Used without options all tables on the command will be checked for errors");
  printf("Usage: %s [OPTIONS] tables[.ISM]\n", my_progname);
  puts("\n\
  -a, --analyze	      Analyze distribution of keys. Will make some joins in\n\
		      MySQL faster.\n\
  -#, --debug=...     Output debug log. Often this is 'd:t:o,filename`\n\
  --character-sets-dir=...\n\
                      Directory where character sets are\n\
  -C, --default-character-set=...\n\
                      Set the default character set\n\
  -d, --description   Prints some information about table.\n\
  -e, --extend-check  Check the table VERY thoroughly.  One need use this\n\
                      only in extreme cases as isamchk should normally find\n\
                      all errors even without this switch\n\
  -f, --force         Overwrite old temporary files.\n\
		      If one uses -f when checking tables (running isamchk\n\
		      without -r), isamchk will automatically restart with\n\
		      -r on any wrong table.\n\
  -?, --help          Display this help and exit.\n\
  -i, --information   Print statistics information about the table\n\
  -k, --keys-used=#   Used with '-r'. Tell ISAM to update only the first\n\
		      # keys.  This can be used to get faster inserts!\n\
  -l, --no-symlinks   Do not follow symbolic links when repairing. Normally\n\
		      isamchk repairs the table a symlink points at.\n\
  -q, --quick         Used with -r to get a faster repair. (The data file\n\
                      isn't touched.) One can give a second '-q' to force\n\
                      isamchk to modify the original datafile.");
  puts("\
  -r, --recover       Can fix almost anything except unique keys that aren't\n\
                      unique.\n\
  -o, --safe-recover  Uses old recovery method; slower than '-r' but can\n\
		      handle a couple of cases that '-r' cannot handle.\n\
  -O, --set-variable var=option\n\
		      Change the value of a variable.\n\
  -s, --silent	      Only print errors.  One can use two -s to make isamchk\n\
		      very silent\n\
  -S, --sort-index    Sort index blocks.  This speeds up 'read-next' in\n\
		      applications\n\
  -R, --sort-records=#\n\
		      Sort records according to an index.  This makes your\n\
		      data much more localized and may speed up things\n\
		      (It may be VERY slow to do a sort the first time!)\n\
  -u, --unpack        Unpack file packed with pack_isam.\n\
  -v, --verbose       Print more information. This can be used with\n\
                      -d and -e. Use many -v for more verbosity!\n\
  -V, --version       Print version and exit.\n\
  -w, --wait          Wait if table is locked.");
  print_defaults("my",load_default_groups);
  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (i=0; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %lu\n",
           changeable_vars[i].name,
           *changeable_vars[i].varptr);
}

	/* Check table */

static int nisamchk(my_string filename)
{
  int error,lock_type,recreate;
  N_INFO *info;
  File datafile;
  char fixed_name[FN_REFLEN];
  ISAM_SHARE *share;
  DBUG_ENTER("nisamchk");

  out_flag=error=warning_printed=error_printed=recreate=0;
  datafile=0;
  isam_file_name=filename;			/* For error messages */
  if (!(info=nisam_open(filename,
		     (testflag & T_DESCRIPT) ? O_RDONLY : O_RDWR,
		     (testflag & T_WAIT_FOREVER) ? HA_OPEN_WAIT_IF_LOCKED :
		     (testflag & T_DESCRIPT) ? HA_OPEN_IGNORE_IF_LOCKED :
		     HA_OPEN_ABORT_IF_LOCKED)))
  {
    /* Avoid twice printing of isam file name */
    error_printed=1;
    switch (my_errno) {
    case HA_ERR_CRASHED:
      print_error("'%s' is not a ISAM-table",filename);
      break;
    case HA_ERR_OLD_FILE:
      print_error("'%s' is a old type of ISAM-table", filename);
      break;
    case HA_ERR_END_OF_FILE:
      print_error("Couldn't read compleat header from '%s'", filename);
      break;
    case EAGAIN:
      print_error("'%s' is locked. Use -w to wait until unlocked",filename);
      break;
    case ENOENT:
      print_error("File '%s' doesn't exist",filename);
      break;
    case EACCES:
      print_error("You don't have permission to use '%s'",filename);
      break;
    default:
      print_error("%d when opening ISAM-table '%s'",
		  my_errno,filename);
      break;
    }
    DBUG_RETURN(1);
  }
  share=info->s;
  share->base.options&= ~HA_OPTION_READ_ONLY_DATA; /* We are modifing it */
  share->r_locks=0;
  if ((testflag & (T_REP_BY_SORT | T_REP | T_STATISTICS |
		   T_SORT_RECORDS | T_SORT_INDEX)) &&
      ((testflag & T_UNPACK && share->data_file_type == COMPRESSED_RECORD) ||
       share->state_length != sizeof(share->state) ||
       uint2korr(share->state.header.base_info_length) !=
       sizeof(share->base) ||
       (max_key && ! share->state.keys && share->base.keys) ||
       test_if_almost_full(info) ||
       info->s->state.header.file_version[3] != nisam_file_magic[3]))
  {
    if (recreate_database(&info,filename))
    {
      VOID(fprintf(stderr,
		   "ISAM-table '%s' is not fixed because of errors\n",
	      filename));
      return(-1);
    }
    recreate=1;
    if (!(testflag & (T_REP | T_REP_BY_SORT)))
    {
      testflag|=T_REP_BY_SORT;			/* if only STATISTICS */
      if (!(testflag & T_SILENT))
	printf("- '%s' has old table-format. Recreating index\n",filename);
      if (!rep_quick)
	rep_quick=1;
    }
    share=info->s;
    share->r_locks=0;
  }

  if (testflag & T_DESCRIPT)
  {
    total_files++;
    total_records+=share->state.records; total_deleted+=share->state.del;
    descript(info,filename);
  }
  else
  {
    if (testflag & (T_REP+T_REP_BY_SORT+T_OPT+T_SORT_RECORDS+T_SORT_INDEX))
      lock_type = F_WRLCK;			/* table is changed */
    else
      lock_type= F_RDLCK;
    if (info->lock_type == F_RDLCK)
      info->lock_type=F_UNLCK;			/* Read only table */
    if (_nisam_readinfo(info,lock_type,0))
    {
      print_error("Can't lock indexfile of '%s', error: %d",
		  filename,my_errno);
      error_printed=0;
      goto end2;
    }
    share->w_locks++;				/* Mark (for writeinfo) */
    if (lock_file(info->dfile,0L,lock_type,"datafile of",filename))
      goto end;
    info->lock_type= F_EXTRA_LCK;		/* Simulate as locked */
    info->tmp_lock_type=lock_type;
    datafile=info->dfile;
    if (testflag & (T_REP+T_REP_BY_SORT+T_SORT_RECORDS+T_SORT_INDEX))
    {
      if (testflag & (T_REP+T_REP_BY_SORT))
	share->state.keys=min(share->base.keys,max_key);
      VOID(fn_format(fixed_name,filename,"",N_NAME_IEXT,
		     4+ (opt_follow_links ? 16 : 0)));

      if (rep_quick && (error=chk_del(info,testflag & ~T_VERBOSE)))
	print_error("Quick-recover aborted; Run recovery without switch 'q'");
      else
      {
	if (testflag & T_REP_BY_SORT &&
	    (share->state.keys || (rep_quick && !max_key && ! recreate)))
	  error=rep_by_sort(info,fixed_name);
	else if (testflag & (T_REP | T_REP_BY_SORT))
	  error=rep(info,fixed_name);
      }
      if (!error && testflag & T_SORT_RECORDS)
      {
	if (out_flag & O_NEW_DATA)
	{			/* Change temp file to org file */
	  VOID(lock_file(datafile,0L,F_UNLCK,"datafile of",filename));
	  VOID(my_close(datafile,MYF(MY_WME)));    /* Close old file */
	  VOID(my_close(info->dfile,MYF(MY_WME))); /* Close new file */
	  error|=change_to_newfile(fixed_name,N_NAME_DEXT,DATA_TMP_EXT);
	  if ((info->dfile=my_open(fn_format(temp_filename,fixed_name,"",
					     N_NAME_DEXT,2+4),
				   O_RDWR | O_SHARE,
				   MYF(MY_WME))) <= 0 ||
	      lock_file(info->dfile,0L,F_WRLCK,"datafile",temp_filename))
	    error=1;
	  out_flag&= ~O_NEW_DATA; /* We are using new datafile */
	  read_cache.file=info->dfile;
	}
	if (! error)
	  error=sort_records(info,fixed_name,opt_sort_key,
			     test(!(testflag & T_REP)));
	datafile=info->dfile;	/* This is now locked */
      }
      if (!error && testflag & T_SORT_INDEX)
	error=sort_index(info,fixed_name);
    }
    else
    {
      if (!(testflag & T_SILENT) || testflag & T_INFO)
	printf("Checking ISAM file: %s\n",filename);
      if (!(testflag & T_SILENT))
	printf("Data records: %7ld   Deleted blocks: %7ld\n",
	       share->state.records,share->state.del);
      share->state.keys=min(share->state.keys,max_key);
      error =chk_size(info);
      error|=chk_del(info,testflag);
      error|=chk_key(info);
      if (!rep_quick)
      {
	if (testflag & T_EXTEND)
	  VOID(init_key_cache(use_buffers,(uint) NEED_MEM));
	VOID(init_io_cache(&read_cache,datafile,(uint) read_buffer_length,
			  READ_CACHE,share->pack.header_length,1,
			  MYF(MY_WME)));
	lock_memory();
	error|=chk_data_link(info,testflag & T_EXTEND);
	error|=flush_blocks(share->kfile);
	VOID(end_io_cache(&read_cache));
      }
    }
  }
end:
  if (!(testflag & T_DESCRIPT))
  {
    if (info->update & (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED))
      error|=update_state_info(info,
			       ((testflag & (T_REP | T_REP_BY_SORT)) ?
				UPDATE_TIME | UPDATE_STAT : 0) |
			       ((testflag & T_SORT_RECORDS) ?
				UPDATE_SORT : 0));
    VOID(lock_file(share->kfile,0L,F_UNLCK,"indexfile",filename));
    if (datafile > 0)
      VOID(lock_file(datafile,0L,F_UNLCK,"datafile of",filename));
    info->update&= ~(HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  }
  share->w_locks--;
end2:
  if (datafile && datafile != info->dfile)
    VOID(my_close(datafile,MYF(MY_WME)));
  if (nisam_close(info))
  {
    print_error("%d when closing ISAM-table '%s'",my_errno,filename);
    DBUG_RETURN(1);
  }
  if (error == 0)
  {
    if (out_flag & O_NEW_DATA)
      error|=change_to_newfile(fixed_name,N_NAME_DEXT,DATA_TMP_EXT);
    if (out_flag & O_NEW_INDEX)
      error|=change_to_newfile(fixed_name,N_NAME_IEXT,INDEX_TMP_EXT);
  }
  VOID(fflush(stdout)); VOID(fflush(stderr));
  if (error_printed)
  {
    if (testflag & (T_REP+T_REP_BY_SORT+T_SORT_RECORDS+T_SORT_INDEX))
      VOID(fprintf(stderr,
		   "ISAM-table '%s' is not fixed because of errors\n",
		   filename));
    else if (! (error_printed & 2) && !(testflag & T_FORCE_CREATE))
      VOID(fprintf(stderr,
      "ISAM-table '%s' is corrupted\nFix it using switch \"-r\" or \"-o\"\n",
	      filename));
  }
  else if (warning_printed &&
	   ! (testflag & (T_REP+T_REP_BY_SORT+T_SORT_RECORDS+T_SORT_INDEX+
			  T_FORCE_CREATE)))
    VOID(fprintf(stderr, "ISAM-table '%s' is useable but should be fixed\n",
	    filename));
  VOID(fflush(stderr));
  DBUG_RETURN(error);
} /* nisamchk */


	 /* Read options */

static void get_options(register int *argc,register char ***argv)
{
  int c,option_index=0;

  load_defaults("my",load_default_groups,argc,argv);
  defaults_alloc=  *argv;
  set_all_changeable_vars(changeable_vars);
  if (isatty(fileno(stdout)))
    testflag|=T_WRITE_LOOP;
  while ((c=getopt_long(*argc,*argv,"adeif?lqrosSuvVw#:b:k:O:R:C:",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'a':
      testflag|= T_STATISTICS;
      break;
    case 'C':
      default_charset=optarg;
      break;
    case OPT_CHARSETS_DIR_IC:
      charsets_dir = optarg;
      break;
    case 'b':
      search_after_block=strtoul(optarg,NULL,10);
      break;
    case 's':				/* silent */
      if (testflag & T_SILENT)
	testflag|=T_VERY_SILENT;
      testflag|= T_SILENT;
      testflag&= ~T_WRITE_LOOP;
      break;
    case 'w':
      testflag|= T_WAIT_FOREVER;
      break;
    case 'd':				/* description if isam-file */
      testflag|= T_DESCRIPT;
      break;
    case 'e':				/* extend check */
      testflag|= T_EXTEND;
      break;
    case 'i':
      testflag|= T_INFO;
      break;
    case 'f':
      tmpfile_createflag= O_RDWR | O_TRUNC;
      testflag|=T_FORCE_CREATE;
      break;
    case 'k':
      max_key= (uint) atoi(optarg);
      break;
    case 'l':
      opt_follow_links=0;
      break;
    case 'r':				/* Repair table */
      testflag= (testflag & ~T_REP) | T_REP_BY_SORT;
      break;
    case 'o':
      testflag= (testflag & ~T_REP_BY_SORT) | T_REP;
      my_disable_async_io=1;	        /* More safety */
      break;
    case 'q':
      rep_quick++;
      break;
    case 'u':
      testflag|= T_UNPACK | T_REP_BY_SORT;
      break;
    case 'v':				/* Verbose */
      testflag|= T_VERBOSE;
      verbose++;
      break;
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
	usage();
	exit(1);
      }
      break;
    case 'R':				/* Sort records */
      testflag|= T_SORT_RECORDS;
      opt_sort_key=(uint) atoi(optarg)-1;
      if (opt_sort_key >= N_MAXKEY)
      {
	fprintf(stderr,
		"The value of the sort key is bigger than max key: %d.\n",
		N_MAXKEY);
	exit(1);
      }
      break;
    case 'S':                         /* Sort index */
      testflag|= T_SORT_INDEX;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o,/tmp/isamchk.trace");
      break;
    case 'V':
      print_version();
      exit(0);
    case '?':
      usage();
      exit(0);
    }
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (*argc == 0)
  {
    usage();
    exit(-1);
  }
  if ((testflag & T_UNPACK) && (rep_quick || (testflag & T_SORT_RECORDS)))
  {
    VOID(fprintf(stderr,
		 "%s: --unpack can't be used with --quick or --sort-records\n",
		 my_progname));
    exit(1);
  }
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  return;
} /* get options */


	/* Check delete links */

static int chk_del( reg1 N_INFO *info, uint test_flag)
{
  reg2 ulong i;
  uint j,delete_link_length;
  ulong empty,next_link;
  uchar buff[8];
  DBUG_ENTER("chk_del");
  if (!(test_flag & T_SILENT)) puts("- check delete-chain");

  record_checksum=0L;
  key_file_blocks=info->s->base.keystart;
  for (j =0 ; j < info->s->state.keys ; j++)
    if (check_k_link(info,j))
      goto wrong;
  delete_link_length=(info->s->base.options & HA_OPTION_PACK_RECORD) ? 8 : 5;

  next_link=info->s->state.dellink;
  if (info->s->state.del == 0)
  {
    if (test_flag & T_VERBOSE)
    {
      puts("No recordlinks");
    }
  }
  else
  {
    if (test_flag & T_VERBOSE)
      printf("Recordlinks:    ");
    empty=0;
    for (i= info->s->state.del ; i > 0L && next_link != NI_POS_ERROR ; i--)
    {
      if (test_flag & T_VERBOSE) printf("%10lu",next_link);
      if (next_link >= info->s->state.data_file_length)
	goto wrong;
      if (my_pread(info->dfile,(char*) buff,delete_link_length,
		  next_link,MYF(MY_NABP)))
      {
	if (test_flag & T_VERBOSE) puts("");
	print_error("Can't read delete-link at filepos: %lu",
		    (ulong) next_link);
	DBUG_RETURN(1);
      }
      if (*buff != '\0')
      {
	if (test_flag & T_VERBOSE) puts("");
	print_error("Record at pos: %lu is not remove-marked",
		    (ulong) next_link);
	goto wrong;
      }
      if (info->s->base.options & HA_OPTION_PACK_RECORD)
      {
	next_link=uint4korr(buff+4);
	empty+=uint3korr(buff+1);
      }
      else
      {
	record_checksum+=next_link;
	next_link=uint4korr(buff+1);
	empty+=info->s->base.reclength;
      }
      if (next_link == (uint32) ~0)	/* Fix for 64 bit long */
	next_link=NI_POS_ERROR;
    }
    if (empty != info->s->state.empty)
    {
      if (test_flag & T_VERBOSE) puts("");
      print_warning("Not used space is supposed to be: %lu but is: %lu",
		    (ulong) info->s->state.empty,(ulong) empty);
      info->s->state.empty=empty;
    }
    if (i != 0 || next_link != NI_POS_ERROR)
      goto wrong;

    if (test_flag & T_VERBOSE) puts("\n");
  }
  DBUG_RETURN(0);
wrong:
  if (test_flag & T_VERBOSE) puts("");
  print_error("delete-link-chain corrupted");
  DBUG_RETURN(1);
} /* chk_del */


	/* Kontrollerar l{nkarna i nyckelfilen */

static int check_k_link( register N_INFO *info, uint nr)
{
  ulong next_link,records;
  DBUG_ENTER("check_k_link");

  if (testflag & T_VERBOSE)
    printf("index %2d:       ",nr+1);

  next_link=info->s->state.key_del[nr];
  records= (info->s->state.key_file_length /
	    info->s->keyinfo[nr].base.block_length);
  while (next_link != NI_POS_ERROR && records > 0)
  {
    if (testflag & T_VERBOSE) printf("%10lu",next_link);
    if (next_link > info->s->state.key_file_length ||
	next_link & (info->s->blocksize-1))
      DBUG_RETURN(1);
    if (my_pread(info->s->kfile,(char*) &next_link,sizeof(long),next_link,
		 MYF(MY_NABP)))
      DBUG_RETURN(1);
    records--;
    key_file_blocks+=info->s->keyinfo[nr].base.block_length;
  }
  if (testflag & T_VERBOSE)
  {
    if (next_link != NI_POS_ERROR)
      printf("%10lu\n",next_link);
    else
      puts("");
  }
  DBUG_RETURN (next_link != NI_POS_ERROR);
} /* check_k_link */


	/* Kontrollerar storleken p} filerna */

static int chk_size(register N_INFO *info)
{
  int error=0;
  register my_off_t skr,size;
  DBUG_ENTER("chk_size");

  if (!(testflag & T_SILENT)) puts("- check file-size");

  size=my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0));
  if ((skr=(my_off_t) info->s->state.key_file_length) != size)
  {
    if (skr > size)
    {
      error=1;
      print_error("Size of indexfile is: %-8lu        Should be: %lu",
		  (ulong) size, (ulong)  skr);
    }
    else
      print_warning("Size of indexfile is: %-8lu      Should be: %lu",
		    (ulong) size,(ulong) skr);
  }
  if (!(testflag & T_VERY_SILENT) &&
      ! (info->s->base.options & HA_OPTION_COMPRESS_RECORD) &&
      info->s->state.key_file_length >
      (ulong) (ulong_to_double(info->s->base.max_key_file_length)*0.9))
    print_warning("Keyfile is almost full, %10lu of %10lu used",
		  info->s->state.key_file_length,
		  info->s->base.max_key_file_length-1);

  size=my_seek(info->dfile,0L,MY_SEEK_END,MYF(0));
  skr=(my_off_t) info->s->state.data_file_length;
  if (info->s->base.options & HA_OPTION_COMPRESS_RECORD)
    skr+= MEMMAP_EXTRA_MARGIN;
#ifdef USE_RELOC
  if (info->data_file_type == STATIC_RECORD &&
      skr < (my_off_t) info->s->base.reloc*info->s->base.min_pack_length)
    skr=(my_off_t) info->s->base.reloc*info->s->base.min_pack_length;
#endif
  if (skr != size)
  {
    info->s->state.data_file_length=(ulong) size; /* Skipp other errors */
    if (skr > size && skr != size + MEMMAP_EXTRA_MARGIN)
    {
      error=1;
      print_error("Size of datafile is: %-8lu         Should be: %lu",
		  (ulong) size,(ulong) skr);
    }
    else
    {
      print_warning("Size of datafile is: %-8lu       Should be: %lu",
		    (ulong) size,(ulong) skr);

    }
  }
  if (!(testflag & T_VERY_SILENT) &&
      !(info->s->base.options & HA_OPTION_COMPRESS_RECORD) &&
      info->s->state.data_file_length >
      (ulong) (ulong_to_double(info->s->base.max_data_file_length)*0.9))
    print_warning("Datafile is almost full, %10lu of %10lu used",
		  info->s->state.data_file_length,
		  info->s->base.max_data_file_length-1);
  DBUG_RETURN(error);
} /* chk_size */


	/* Kontrollerar nycklarna */

static int chk_key( register N_INFO *info)
{
  uint key;
  ulong keys,all_keydata,all_totaldata,key_totlength,length,
	init_checksum,old_record_checksum;
  ISAM_SHARE *share=info->s;
  N_KEYDEF *keyinfo;
  DBUG_ENTER("chk_key");

  if (!(testflag & T_SILENT)) puts("- check index reference");

  all_keydata=all_totaldata=key_totlength=old_record_checksum=0;
  init_checksum=record_checksum;
  if (!(share->base.options &
	(HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    old_record_checksum=calc_checksum(share->state.records+share->state.del-1)*
      share->base.reclength;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->state.keys ;
       key++,keyinfo++)
  {
    record_checksum=init_checksum;
    unique_count=0L;
    if ((!(testflag & T_SILENT)) && share->state.keys >1)
      printf ("- check data record references index: %d\n",key+1);
    if (share->state.key_root[key] == NI_POS_ERROR &&
	share->state.records == 0)
      continue;
    if (!_nisam_fetch_keypage(info,keyinfo,share->state.key_root[key],info->buff,
			   0))
    {
      print_error("Can't read indexpage from filepos: %lu",
		  (ulong) share->state.key_root[key]);
      DBUG_RETURN(-1);
    }
    key_file_blocks+=keyinfo->base.block_length;
    keys=keydata=totaldata=key_blocks=0; max_level=0;
    if (chk_index(info,keyinfo,share->state.key_root[key],info->buff,&keys,1))
      DBUG_RETURN(-1);
    if (keys != share->state.records)
    {
      print_error("Found %lu keys of %lu",(ulong) keys,
		  (ulong) share->state.records);
      DBUG_RETURN(-1);
    }
    if (!key && (share->base.options &
		 (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
      old_record_checksum=record_checksum;
    else if (old_record_checksum != record_checksum)
    {
      if (key)
	print_error("Key %u doesn't point at same records that key 1",
		    key+1);
      else
	print_error("Key 1 doesn't point at all records");
      DBUG_RETURN(-1);
    }
    length=(ulong) isam_key_length(info,keyinfo)*keys + key_blocks*2;
    if (testflag & T_INFO && totaldata != 0L && keys != 0L)
      printf("Key: %2d:  Keyblocks used: %3d%%  Packed: %4d%%  Max levels: %2d\n",
	     key+1,
	     (int) (keydata*100.0/totaldata),
	     (int) ((long) (length-keydata)*100.0/(double) length),
	     max_level);
    all_keydata+=keydata; all_totaldata+=totaldata; key_totlength+=length;
    share->base.rec_per_key[key]=
      unique_count ? ((share->state.records+unique_count/2)/
		      unique_count) : 1L;
  }
  if (testflag & T_INFO)
  {
    if (all_totaldata != 0L && share->state.keys != 1)
      printf("Total:    Keyblocks used: %3d%%  Packed: %4d%%\n\n",
	     (int) (all_keydata*100.0/all_totaldata),
	     (int) ((long) (key_totlength-all_keydata)*100.0/
		    (double) key_totlength));
    else if (all_totaldata != 0L && share->state.keys)
      puts("");
  }
  if (key_file_blocks != share->state.key_file_length)
    print_warning("Some data are unreferenced in keyfile");
  record_checksum-=init_checksum;		/* Remove delete links */
  if (testflag & T_STATISTICS)
    DBUG_RETURN(update_state_info(info,UPDATE_STAT));
  DBUG_RETURN(0);
} /* chk_key */


	/* Check if index is ok */

static int chk_index(N_INFO *info, N_KEYDEF *keyinfo, ulong page, uchar *buff,
		     ulong *keys,uint level)
{
  int flag;
  uint used_length,comp_flag,nod_flag;
  uchar key[N_MAX_POSSIBLE_KEY_BUFF],*temp_buff,*keypos,*endpos;
  ulong next_page,record;
  DBUG_ENTER("chk_index");
  DBUG_DUMP("buff",(byte*) buff,getint(buff));

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->base.block_length)))
  {
    print_error("Not Enough memory");
    DBUG_RETURN(-1);
  }

  if (keyinfo->base.flag & HA_NOSAME)
    comp_flag=SEARCH_FIND;			/* Not dupplicates */
  else
    comp_flag=SEARCH_SAME;			/* Keys in positionorder */
  nod_flag=test_if_nod(buff);
  used_length=getint(buff);
  keypos=buff+2+nod_flag;
  endpos=buff+used_length;

  keydata+=used_length; totaldata+=keyinfo->base.block_length;	/* INFO */
  key_blocks++;
  if (level > max_level)
    max_level=level;

  if (used_length > keyinfo->base.block_length)
  {
    print_error("Wrong pageinfo at page: %lu",(ulong) page);
    goto err;
  }
  for ( ;; )
  {
    if (nod_flag)
    {
      next_page=_nisam_kpos(nod_flag,keypos);
      if (next_page > info->s->state.key_file_length ||
	  (nod_flag && (next_page & (info->s->blocksize -1))))
      {
	my_off_t max_length=my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0));
	print_error("Wrong pagepointer: %lu at page: %lu",
		    (ulong) next_page,(ulong) page);

	if (next_page+info->s->blocksize > max_length)
	  goto err;
	info->s->state.key_file_length=(ulong) (max_length &
						~ (my_off_t)
						(info->s->blocksize-1));
      }
      if (!_nisam_fetch_keypage(info,keyinfo,next_page,temp_buff,0))
      {
	print_error("Can't read key from filepos: %lu",(ulong) next_page);
	goto err;
      }
      key_file_blocks+=keyinfo->base.block_length;
      if (chk_index(info,keyinfo,next_page,temp_buff,keys,level+1))
	goto err;
    }
    if (keypos >= endpos ||
	(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,key) == 0)
      break;
    if ((*keys)++ &&
	(flag=_nisam_key_cmp(keyinfo->seg,info->lastkey,key,0,comp_flag)) >=0)
    {
      DBUG_DUMP("old",(byte*) info->lastkey,
		_nisam_keylength(keyinfo,info->lastkey));
      DBUG_DUMP("new",(byte*) key,_nisam_keylength(keyinfo,key));

      if (comp_flag == SEARCH_FIND && flag == 0)
	print_error("Found dupplicated key at page %lu",(ulong) page);
      else
	print_error("Key in wrong position at page %lu",(ulong) page);
      goto err;
    }
    if (testflag & T_STATISTICS)
    {
      if (*keys == 1L ||
	  _nisam_key_cmp(keyinfo->seg,info->lastkey,key,0,SEARCH_FIND))
	unique_count++;
    }
    VOID(_nisam_move_key(keyinfo,(uchar*) info->lastkey,key));
    record= _nisam_dpos(info,nod_flag,keypos);
    if (record >= info->s->state.data_file_length)
    {
      print_error("Found key at page %lu that points to record outside datafile",page);
      DBUG_PRINT("test",("page: %lu  record: %lu  filelength: %lu",
			 (ulong) page,(ulong) record,
			 (ulong) info->s->state.data_file_length));
      DBUG_DUMP("key",(byte*) info->lastkey,info->s->base.max_key_length);
      goto err;
    }
    record_checksum+=record;
  }
  if (keypos != endpos)
  {
    print_error("Keyblock size at page %lu is not correct.  Block length: %d  key length: %d",(ulong) page, used_length, (keypos - buff));
    goto err;
  }
  my_afree((byte*) temp_buff);
  DBUG_RETURN(0);
 err:
  my_afree((byte*) temp_buff);
  DBUG_RETURN(1);
} /* chk_index */


	/* Calculate a checksum of 1+2+3+4...N = N*(N+1)/2 without overflow */

static ulong calc_checksum(count)
ulong count;
{
  ulong sum,a,b;
  DBUG_ENTER("calc_checksum");

  sum=0;
  a=count; b=count+1;
  if (a & 1)
    b>>=1;
  else
    a>>=1;
  while (b)
  {
    if (b & 1)
      sum+=a;
    a<<=1; b>>=1;
  }
  DBUG_PRINT("exit",("sum: %lx",sum));
  DBUG_RETURN(sum);
} /* calc_checksum */


	/* Calc length of key in normal isam */

static uint isam_key_length( N_INFO *info, reg1 N_KEYDEF *keyinfo)
{
  uint length;
  N_KEYSEG *keyseg;
  DBUG_ENTER("isam_key_length");

  length= info->s->rec_reflength;
  for (keyseg=keyinfo->seg ; keyseg->base.type ; keyseg++)
    length+= keyseg->base.length;

  DBUG_PRINT("exit",("length: %d",length));
  DBUG_RETURN(length);
} /* key_length */


	/* Check that record-link is ok */

static int chk_data_link(info,extend)
reg1 N_INFO *info;
int extend;
{
  int	error,got_error,flag;
  uint	key,left_length,b_type;
  ulong records,del_blocks,used,empty,pos,splitts,start_recpos,
	del_length,link_used,intern_record_checksum,start_block;
  byte	*record,*to;
  N_KEYDEF *keyinfo;
  BLOCK_INFO block_info;
  DBUG_ENTER("chk_data_link");

  if (! (info->s->base.options & (HA_OPTION_PACK_RECORD |
			       HA_OPTION_COMPRESS_RECORD)) &&
      ! extend)
    DBUG_RETURN(0);

  if (!(testflag & T_SILENT))
  {
    if (extend)
      puts("- check records and index references");
    else
      puts("- check record links");
  }

  if (!(record= (byte*) my_alloca(info->s->base.reclength)))
  {
    print_error("Not Enough memory");
    DBUG_RETURN(-1);
  }
  records=used=link_used=splitts=del_blocks=del_length=
    intern_record_checksum=crc=0L;
  LINT_INIT(left_length);  LINT_INIT(start_recpos);  LINT_INIT(to);
  got_error=error=0;
  empty=pos=info->s->pack.header_length;

  while (pos < info->s->state.data_file_length)
  {
    switch (info->s->data_file_type) {
    case STATIC_RECORD:
      if (my_b_read(&read_cache,(byte*) record,info->s->base.reclength))
	goto err;
      start_recpos=pos;
      pos+=info->s->base.reclength;
      splitts++;
      if (*record == '\0')
      {
	del_blocks++;
	del_length+=info->s->base.reclength;
	continue;					/* Record removed */
      }
      used+=info->s->base.reclength;
      break;
    case DYNAMIC_RECORD:
      flag=block_info.second_read=0;
      block_info.next_filepos=pos;
      do
      {
	if (_nisam_read_cache(&read_cache,(byte*) block_info.header,
			  (start_block=block_info.next_filepos),
			  sizeof(block_info.header),test(! flag) | 2))
	  goto err;
	b_type=_nisam_get_block_info(&block_info,-1,start_block);
	if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
		      BLOCK_FATAL_ERROR))
	{
	  if (b_type & BLOCK_SYNC_ERROR)
	  {
	    if (flag)
	    {
	      print_error("Unexpected byte: %d at link: %lu",
			  (int) block_info.header[0],(ulong) start_block);
	      goto err2;
	    }
	    pos=block_info.filepos+block_info.block_len;
	    goto next;
	  }
	  if (b_type & BLOCK_DELETED)
	  {
	    if (block_info.block_len < info->s->base.min_block_length ||
		block_info.block_len-4 > (uint) info->s->base.max_pack_length)
	    {
	      print_error("Deleted block with impossible length %u at %lu",
			 block_info.block_len,(ulong) pos);
	      goto err2;
	    }
	    del_blocks++;
	    del_length+=block_info.block_len;
	    pos=block_info.filepos+block_info.block_len;
	    splitts++;
	    goto next;
	  }
	  print_error("Wrong bytesec: %d-%d-%d at linkstart: %lu",
		      block_info.header[0],block_info.header[1],
		      block_info.header[2],(ulong) start_block);
	  goto err2;
	}
	if (info->s->state.data_file_length < block_info.filepos+
	    block_info.block_len)
	{
	  print_error("Recordlink that points outside datafile at %lu",
		      (ulong) pos);
	  got_error=1;
	  break;
	}
	splitts++;
	if (!flag++)				/* First block */
	{
	  start_recpos=pos;
	  pos=block_info.filepos+block_info.block_len;
	  if (block_info.rec_len > (uint) info->s->base.max_pack_length)
	  {
	    print_error("Found too long record at %lu",(ulong) start_recpos);
	    got_error=1;
	    break;
	  }
	  if (info->s->base.blobs)
	  {
	    if (!(to=fix_rec_buff_for_blob(info,block_info.rec_len)))
	    {
	      print_error("Not enough memory for blob at %lu",
			  (ulong) start_recpos);
	      got_error=1;
	      break;
	    }
	  }
	  else
	    to= info->rec_buff;
	  left_length=block_info.rec_len;
	}
	if (left_length < block_info.data_len)
	{
	  print_error("Found too long record at %lu",(ulong) start_recpos);
	  got_error=1; break;
	}
	if (_nisam_read_cache(&read_cache,(byte*) to,block_info.filepos,
			  (uint) block_info.data_len, test(flag == 1)))
	  goto err;
	to+=block_info.data_len;
	link_used+= block_info.filepos-start_block;
	used+= block_info.filepos - start_block + block_info.data_len;
	empty+=block_info.block_len-block_info.data_len;
	left_length-=block_info.data_len;
	if (left_length)
	{
	  if (b_type & BLOCK_LAST)
	  {
	    print_error("Record link to short for record at %lu",
			(ulong) start_recpos);
	    got_error=1;
	    break;
	  }
	  if (info->s->state.data_file_length < block_info.next_filepos)
	  {
	    print_error("Found next-recordlink that points outside datafile at %lu",
			(ulong) block_info.filepos);
	    got_error=1;
	    break;
	  }
	}
      } while (left_length);
      if (! got_error)
      {
	if (_nisam_rec_unpack(info,record,info->rec_buff,block_info.rec_len) ==
	    MY_FILE_ERROR)
	{
	  print_error("Found wrong record at %lu",(ulong) start_recpos);
	  got_error=1;
	}
	if (testflag & (T_EXTEND | T_VERBOSE))
	{
	  if (_nisam_rec_check(info,record))
	  {
	    print_error("Found wrong packed record at %lu",
			(ulong) start_recpos);
	    got_error=1;
	  }
	}
      }
      else if (!flag)
	pos=block_info.filepos+block_info.block_len;
      break;
    case COMPRESSED_RECORD:
      if (_nisam_read_cache(&read_cache,(byte*) block_info.header,pos, 3,1))
	goto err;
      start_recpos=pos;
      splitts++;
      VOID(_nisam_pack_get_block_info(&block_info,info->s->pack.ref_length,-1,
				   start_recpos));
      pos=start_recpos+info->s->pack.ref_length+block_info.rec_len;
      if (block_info.rec_len < (uint) info->s->min_pack_length ||
	  block_info.rec_len > (uint) info->s->max_pack_length)
      {
	print_error("Found block with wrong recordlength: %d at %lu",
		    block_info.rec_len,(ulong) start_recpos);
	got_error=1;
	break;
      }
      if (_nisam_read_cache(&read_cache,(byte*) info->rec_buff,
			block_info.filepos, block_info.rec_len,1))
	goto err;
      if (_nisam_pack_rec_unpack(info,record,info->rec_buff,block_info.rec_len))
      {
	print_error("Found wrong record at %lu",(ulong) start_recpos);
	got_error=1;
      }
      crc^=checksum(record,info->s->base.reclength);
      link_used+=info->s->pack.ref_length;
      used+=block_info.rec_len+info->s->pack.ref_length;
    }
    if (! got_error)
    {
      intern_record_checksum+=start_recpos;
      records++;
      if (testflag & T_WRITE_LOOP && records % WRITE_COUNT == 0)
      {
	printf("%lu\r",(ulong) records); VOID(fflush(stdout));
      }

      if (extend)
      {
	for (key=0,keyinfo= info->s->keyinfo; key<info->s->state.keys;
	     key++,keyinfo++)
	{
	  VOID(_nisam_make_key(info,key,info->lastkey,record,start_recpos));
	  if (_nisam_search(info,keyinfo,info->lastkey,0,SEARCH_SAME,
			 info->s->state.key_root[key]))
	  {
	    print_error("Record at: %10lu  Can't find key for index: %2d",
			start_recpos,key+1);
	    if (error++ > MAXERR || !(testflag & T_VERBOSE))
	      goto err2;
	  }
	}
      }
    }
    else
    {
      got_error=0;
      if (error++ > MAXERR || !(testflag & T_VERBOSE))
	goto err2;
    }
  next:;				/* Next record */
  }
  if (testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (records != info->s->state.records)
  {
    print_error("Record-count is not ok; is %-10lu   Should be: %lu",
		(ulong) records,(ulong) info->s->state.records);
    error=1;
  }
  else if (record_checksum != intern_record_checksum && info->s->state.keys)
  {
    print_error("Keypointers and records don't match");
    error=1;
  }
  if (used+empty+del_length != info->s->state.data_file_length)
  {
    print_warning("Found %lu record-data and %lu unused data and %lu deleted-data\nTotal %lu, Should be: %lu",
		  (ulong) used,(ulong) empty,(ulong) del_length,
		  (ulong) (used+empty+del_length),
		  (ulong) info->s->state.data_file_length);
  }
  if (del_blocks != info->s->state.del)
  {
    print_warning("Found %10lu deleted blocks       Should be: %lu",
		  (ulong) del_blocks,(ulong) info->s->state.del);
  }
  if (splitts != info->s->state.splitt)
  {
    print_warning("Found %10lu parts                Should be: %lu parts",
		  (ulong) splitts,(ulong) info->s->state.splitt);
  }
  if ((info->s->base.options & HA_OPTION_COMPRESS_RECORD) &&
      crc != info->s->state.uniq)
    print_warning("Wrong checksum for records; Restore uncompressed table");

  if (testflag & T_INFO)
  {
    if (warning_printed || error_printed)
      puts("");
    if (used != 0 && ! error_printed)
    {
      printf("Records:%17lu    M.recordlength:%8lu   Packed:%14.0f%%\n",
	     records, (used-link_used)/records,
	     (info->s->base.blobs ? 0 :
	      (ulong_to_double(info->s->base.reclength*records)-used)/
	      ulong_to_double(info->s->base.reclength*records)*100.0));
      printf("Recordspace used:%8.0f%%   Empty space:%11d%%  Blocks/Record: %6.2f\n",
	     (ulong_to_double(used-link_used)/ulong_to_double(used-link_used+empty)*100.0),
	     (!records ? 100 : (int) (ulong_to_double(del_length+empty)/used*100.0)),
	     ulong_to_double(splitts - del_blocks) / records);
    }
    printf("Record blocks:%12lu    Delete blocks:%10lu\n",
	   splitts-del_blocks,del_blocks);
    printf("Record data:  %12lu    Deleted data :%10lu\n",
	   used-link_used,del_length);
    printf("Lost space:   %12lu    Linkdata:    %10lu\n",
	   empty,link_used);
  }
  my_afree((gptr) record);
  DBUG_RETURN (error);
 err:
  print_error("got error: %d when reading datafile",my_errno);
 err2:
  my_afree((gptr) record);
  DBUG_RETURN(1);
} /* chk_data_link */


	/* Recover old table by reading each record and writing all keys */
	/* Save new datafile-name in temp_filename */

static int rep(info,name)
reg1 N_INFO *info;
my_string name;
{
  int error,got_error;
  uint i;
  ulong start_records,new_header_length,del;
  File new_file;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("rep");

  start_records=share->state.records;
  new_header_length=(testflag & T_UNPACK) ? 0L : share->pack.header_length;
  got_error=1;
  new_file= -1;
  if (!(testflag & T_SILENT))
  {
    printf("- recovering ISAM-table '%s'\n",name);
    printf("Data records: %lu\n",(ulong) share->state.records);
  }

  VOID(init_key_cache(use_buffers,NEED_MEM));
  if (init_io_cache(&read_cache,info->dfile,(uint) read_buffer_length,
		   READ_CACHE,share->pack.header_length,1,MYF(MY_WME)))
    goto err;
  if (!rep_quick)
    if (init_io_cache(&info->rec_cache,-1,(uint) write_buffer_length,
		      WRITE_CACHE, new_header_length, 1,
		      MYF(MY_WME | MY_WAIT_IF_FULL)))
      goto err;
  info->opt_flag|=WRITE_CACHE_USED;
  sort_info.start_recpos=0;
  sort_info.buff=0; sort_info.buff_length=0;
  if (!(sort_info.record=(byte*) my_alloca((uint) share->base.reclength)))
  {
    print_error("Not Enough memory");
    goto err;
  }

  if (!rep_quick)
  {
    if ((new_file=my_create(fn_format(temp_filename,name,"",DATA_TMP_EXT,
				      2+4),
			    0,tmpfile_createflag,MYF(0))) < 0)
    {
      print_error("Can't create new tempfile: '%s'",temp_filename);
      goto err;
    }
    if (filecopy(new_file,info->dfile,0L,new_header_length,"datafile-header"))
      goto err;
    share->state.dellink= NI_POS_ERROR;
    info->rec_cache.file=new_file;
    if (testflag & T_UNPACK)
      share->base.options&= ~HA_OPTION_COMPRESS_RECORD;
  }

  sort_info.info=info;
  sort_info.pos=sort_info.max_pos=share->pack.header_length;
  sort_info.filepos=new_header_length;
  read_cache.end_of_file=sort_info.filelength=(ulong)
    my_seek(info->dfile,0L,MY_SEEK_END,MYF(0));
  sort_info.dupp=0;
  sort_info.fix_datafile= (my_bool) (! rep_quick);
  sort_info.max_records=LONG_MAX;
  if ((sort_info.new_data_file_type=share->data_file_type) ==
      COMPRESSED_RECORD && testflag & T_UNPACK)
  {
    if (share->base.options & HA_OPTION_PACK_RECORD)
      sort_info.new_data_file_type = DYNAMIC_RECORD;
    else
      sort_info.new_data_file_type = STATIC_RECORD;
  }

  del=share->state.del;
  share->state.records=share->state.del=share->state.empty=
    share->state.splitt=0;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (i=0 ; i < N_MAXKEY ; i++)
    share->state.key_del[i]=share->state.key_root[i]= NI_POS_ERROR;
  share->state.key_file_length=share->base.keystart;

  lock_memory();				/* Everything is alloced */
  while (!(error=sort_get_next_record()))
  {
    if (writekeys(info,(byte*) sort_info.record,sort_info.filepos))
    {
      if (my_errno != HA_ERR_FOUND_DUPP_KEY) goto err;
      DBUG_DUMP("record",(byte*) sort_info.record,share->base.pack_reclength);
      print_info("Dupplicate key %2d for record at %10lu against new record at %10lu",info->errkey+1,sort_info.start_recpos,info->int_pos);
      if (testflag & T_VERBOSE)
      {
	VOID(_nisam_make_key(info,(uint) info->errkey,info->lastkey,
		     sort_info.record,0L));
	_nisam_print_key(stdout,share->keyinfo[info->errkey].seg,info->lastkey);
      }
      sort_info.dupp++;
      if (rep_quick == 1)
      {
	error_printed=1;
	goto err;
      }
      continue;
    }
    if (sort_write_record())
      goto err;
  }
  if (error > 0 || write_data_suffix(info) ||
      flush_io_cache(&info->rec_cache) || read_cache.error < 0)
    goto err;

  if (testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  if (my_chsize(share->kfile,share->state.key_file_length,MYF(0)))
  {
    print_warning("Can't change size of indexfile, error: %d",my_errno);
    goto err;
  }

  if (rep_quick && del+sort_info.dupp != share->state.del)
  {
    print_error("Couldn't fix table with quick recovery: Found wrong number of deleted records");
    print_error("Run recovery again without -q");
    got_error=1;
    goto err;
  }

  if (!rep_quick)
  {
    info->dfile=new_file;
    share->state.data_file_length=sort_info.filepos;
    share->state.splitt=share->state.records;		/* Only hole records */
    out_flag|=O_NEW_DATA;				/* Data in new file */
    share->state.version=(ulong) time((time_t*) 0);	/* Force reopen */
  }
  else
    share->state.data_file_length=sort_info.max_pos;

  if (!(testflag & T_SILENT))
  {
    if (start_records != share->state.records)
      printf("Data records: %lu\n",(ulong) share->state.records);
    if (sort_info.dupp)
      print_warning("%lu records have been removed",(ulong) sort_info.dupp);
  }

  got_error=0;
err:
  if (got_error)
  {
    if (! error_printed)
      print_error("%d for record at pos %lu",my_errno,
		  (ulong) sort_info.start_recpos);
    if (new_file >= 0)
    {
      VOID(my_close(new_file,MYF(0)));
      VOID(my_delete(temp_filename,MYF(MY_WME)));
    }
  }
  if (sort_info.record)
  {
    my_afree(sort_info.record);
  }
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  VOID(end_io_cache(&read_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  VOID(end_io_cache(&info->rec_cache));
  got_error|=flush_blocks(share->kfile);
  if (!got_error && testflag & T_UNPACK)
  {
    share->state.header.options[0]&= (uchar) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
    share->data_file_type=sort_info.new_data_file_type;
  }
  DBUG_RETURN(got_error);
} /* rep */


	/* Uppdaterar nyckelfilen i samband med reparation */

static int writekeys(register N_INFO *info,byte *buff,ulong filepos)
{
  register uint i;
  uchar *key;
  DBUG_ENTER("writekeys");

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->state.keys ; i++)
  {
    VOID(_nisam_make_key(info,i,key,buff,filepos));
    if (_nisam_ck_write(info,i,key)) goto err;
  }
  DBUG_RETURN(0);

 err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey=(int) i;			/* This key was found */
    while ( i-- > 0 )
    {
      VOID(_nisam_make_key(info,i,key,buff,filepos));
      if (_nisam_ck_delete(info,i,key)) break;
    }
  }
  DBUG_PRINT("error",("errno: %d",my_errno));
  DBUG_RETURN(-1);
} /* writekeys */


	 /* Write info about table */

static void descript(info,name)
reg1 N_INFO *info;
my_string name;
{
  uint key,field,start,len;
  reg3 N_KEYDEF *keyinfo;
  reg2 N_KEYSEG *keyseg;
  reg4 const char *text;
  char buff[40],length[10],*pos,*end;
  enum en_fieldtype type;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("describe");

  printf("\nISAM file:     %s\n",name);
  if (testflag & T_VERBOSE)
  {
    printf("Isam-version:  %d\n",(int) share->state.header.file_version[3]);
    if (share->base.create_time)
    {
      get_date(buff,1,share->base.create_time);
      printf("Creation time: %s\n",buff);
    }
    if (share->base.isamchk_time)
    {
      get_date(buff,1,share->base.isamchk_time);
      printf("Recover time:  %s\n",buff);
    }
  }
  printf("Data records:        %10lu  Deleted blocks:     %10lu\n",
	 share->state.records,share->state.del);
  if (testflag & T_SILENT)
    DBUG_VOID_RETURN;				/* This is enough */

  if (testflag & T_VERBOSE)
  {
#ifdef USE_RELOC
    printf("Init-relocation:     %10lu\n",share->base.reloc);
#endif
    printf("Datafile  Parts:     %10lu  Deleted data:       %10lu\n",
	   share->state.splitt,share->state.empty);
    printf("Datafile pointer (bytes):%6d  Keyfile pointer (bytes):%6d\n",
	   share->rec_reflength,share->base.key_reflength);
    if (info->s->base.reloc == 1L && info->s->base.records == 1L)
      puts("This is a one-record table");
    else
    {
      if (share->base.max_data_file_length != NI_POS_ERROR ||
	  share->base.max_key_file_length != NI_POS_ERROR)
	printf("Max datafile length: %10lu  Max keyfile length: %10lu\n",
	       share->base.max_data_file_length-1,
	       share->base.max_key_file_length-1);
    }
  }

  printf("Recordlength:        %10d\n",(int) share->base.reclength);
  VOID(fputs("Record format: ",stdout));
  if (share->base.options & HA_OPTION_COMPRESS_RECORD)
    puts("Compressed");
  else if (share->base.options & HA_OPTION_PACK_RECORD)
    puts("Packed");
  else
    puts("Fixed length");
  if (share->state.keys != share->base.keys)
    printf("Using only %d keys of %d possibly keys\n",share->state.keys,
	   share->base.keys);

  puts("\ntable description:");
  printf("Key Start Len Index   Type");
  if (testflag & T_VERBOSE)
    printf("                       Root  Blocksize    Rec/key");
  VOID(putchar('\n'));

  for (key=0, keyinfo= &share->keyinfo[0] ; key < share->base.keys;
       key++,keyinfo++)
  {
    keyseg=keyinfo->seg;
    if (keyinfo->base.flag & HA_NOSAME) text="unique ";
    else text="multip.";

    pos=buff;
    if (keyseg->base.flag & HA_REVERSE_SORT)
      *pos++ = '-';
    pos=strmov(pos,type_names[keyseg->base.type]);
    *pos++ = ' ';
    *pos=0;
    if (keyinfo->base.flag & HA_PACK_KEY)
      pos=strmov(pos,packed_txt);
    if (keyseg->base.flag & HA_SPACE_PACK)
      pos=strmov(pos,diff_txt);
    printf("%-4d%-6d%-3d %-8s%-21s",
	   key+1,keyseg->base.start+1,keyseg->base.length,text,buff);
    if (testflag & T_VERBOSE)
      printf(" %9ld  %9d  %9ld",
	     share->state.key_root[key],keyinfo->base.block_length,
	     share->base.rec_per_key[key]);
    VOID(putchar('\n'));
    while ((++keyseg)->base.type)
    {
      pos=buff;
      if (keyseg->base.flag & HA_REVERSE_SORT)
	*pos++ = '-';
      pos=strmov(pos,type_names[keyseg->base.type]);
      *pos++= ' ';
      if (keyseg->base.flag & HA_SPACE_PACK)
	pos=strmov(pos,diff_txt);
      *pos=0;
      printf("    %-6d%-3d         %-24s\n",
	     keyseg->base.start+1,keyseg->base.length,buff);
    }
  }
  if (verbose > 1)
  {
    printf("\nField Start Length Type");
    if (share->base.options & HA_OPTION_COMPRESS_RECORD)
      printf("                         Huff tree  Bits");
    VOID(putchar('\n'));
    if (verbose > 2 && share->base.pack_bits)
      printf("-           %-7d%-35s\n",share->base.pack_bits,"bit field");

    start=1;
    for (field=0 ; field < share->base.fields ; field++)
    {
      if (share->base.options & HA_OPTION_COMPRESS_RECORD)
	type=share->rec[field].base_type;
      else
	type=(enum en_fieldtype) share->rec[field].base.type;
      end=strmov(buff,field_pack[type]);
#ifndef NOT_PACKED_DATABASES
      if (share->base.options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->rec[field].pack_type & PACK_TYPE_SELECTED)
	  end=strmov(end,", not_always");
	if (share->rec[field].pack_type & PACK_TYPE_SPACE_FIELDS)
	  end=strmov(end,", no empty");
	if (share->rec[field].pack_type & PACK_TYPE_ZERO_FILL)
	{
	  sprintf(end,", zerofill(%d)",share->rec[field].space_length_bits);
	  end=strend(end);
	}
      }
      if (buff[0] == ',')
	strmov(buff,buff+2);
#endif
      len=(uint) (int2str((long) share->rec[field].base.length,length,10) -
		  length);
      if (type == FIELD_BLOB)
      {
	length[len]='+';
	VOID(int2str((long) sizeof(char*),length+len+1,10));
      }
      printf("%-6d%-6d%-7s%-35s",field+1,start,length,buff);
#ifndef NOT_PACKED_DATABASES
      if (share->base.options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->rec[field].huff_tree)
	  printf("%3d    %2d",
		 (uint) (share->rec[field].huff_tree-share->decode_trees)+1,
		 share->rec[field].huff_tree->quick_table_bits);
      }
#endif
      VOID(putchar('\n'));
      start+=share->rec[field].base.length;
      if (type == FIELD_BLOB)
	start+=sizeof(char*);
    }
  }
  DBUG_VOID_RETURN;
} /* describe */


	/* Change all key-pointers that points to a records */

static int movepoint(info,record,oldpos,newpos,prot_key)
register N_INFO *info;
byte *record;
ulong oldpos,newpos;
uint prot_key;
{
  register uint i;
  uchar *key;
  DBUG_ENTER("movepoint");

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->state.keys; i++)
  {
    if (i != prot_key)
    {
      VOID(_nisam_make_key(info,i,key,record,oldpos));
      if (info->s->keyinfo[i].base.flag & HA_NOSAME)
      {					/* Change pointer direct */
	uint nod_flag;
	N_KEYDEF *keyinfo;
	keyinfo=info->s->keyinfo+i;
	if (_nisam_search(info,keyinfo,key,USE_HOLE_KEY,
		       (uint) (SEARCH_SAME | SEARCH_SAVE_BUFF),
		       info->s->state.key_root[i]))
	  DBUG_RETURN(-1);
	nod_flag=test_if_nod(info->buff);
	_nisam_dpointer(info,info->int_keypos-nod_flag-
		     info->s->rec_reflength,newpos);
	if (_nisam_write_keypage(info,keyinfo,info->int_pos,info->buff))
	  DBUG_RETURN(-1);
      }
      else
      {					/* Change old key to new */
	if (_nisam_ck_delete(info,i,key))
	  DBUG_RETURN(-1);
	VOID(_nisam_make_key(info,i,key,record,newpos));
	if (_nisam_ck_write(info,i,key))
	  DBUG_RETURN(-1);
      }
    }
  }
  DBUG_RETURN(0);
} /* movepoint */


	/* Tell system that we want all memory for our cache */

static void lock_memory(void)
{
#ifdef SUN_OS				/* Key-cacheing thrases on sun 4.1 */
  int success;

  success = mlockall(MCL_CURRENT);	/* or plock(DATLOCK); */

  if (geteuid() == 0 && success != 0)
    print_warning("Failed to lock memory. errno %d",my_errno);
#endif
} /* lock_memory */


	/* Flush all changed blocks to disk */

static int flush_blocks(file)
File file;
{
  if (flush_key_blocks(file,FLUSH_RELEASE))
  {
    print_error("%d when trying to write bufferts",my_errno);
    return(1);
  }
  end_key_cache();
  return 0;
} /* flush_blocks */


	/* Sort records according to one key */

static int sort_records(info,name,sort_key,write_info)
register N_INFO *info;
my_string name;
uint sort_key;
int write_info;
{
  int got_error;
  uint key;
  N_KEYDEF *keyinfo;
  File new_file;
  uchar *temp_buff;
  ulong old_record_count;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("sort_records");

  keyinfo= &share->keyinfo[sort_key];
  got_error=1;
  temp_buff=0; record_buff=0;
  new_file= -1;

  if (sort_key >= share->state.keys)
  {
    print_error("Can't sort table '%s' on key %d. It has only %d keys",
		name,sort_key+1,share->state.keys);
    error_printed=0;
    DBUG_RETURN(-1);
  }
  if (!(testflag & T_SILENT))
  {
    printf("- Sorting records in ISAM-table '%s'\n",name);
    if (write_info)
      printf("Data records: %7lu   Deleted: %7lu\n",
	     share->state.records,share->state.del);
  }
  if (share->state.key_root[sort_key] == NI_POS_ERROR)
    DBUG_RETURN(0);				/* Nothing to do */

  init_key_cache(use_buffers,NEED_MEM);
  if (init_io_cache(&info->rec_cache,-1,(uint) write_buffer_length,
		   WRITE_CACHE,share->pack.header_length,1,
		   MYF(MY_WME | MY_WAIT_IF_FULL)))
    goto err;
  info->opt_flag|=WRITE_CACHE_USED;

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->base.block_length)))
  {
    print_error("Not Enough memory");
    goto err;
  }
  if (!(record_buff=(byte*) my_alloca((uint) share->base.reclength)))
  {
    print_error("Not Enough memory");
    goto err;
  }
  if ((new_file=my_create(fn_format(temp_filename,name,"",DATA_TMP_EXT,2+4),
			  0,tmpfile_createflag,MYF(0))) <= 0)
  {
    print_error("Can't create new tempfile: '%s'",temp_filename);
    goto err;
  }
  if (filecopy(new_file,info->dfile,0L,share->pack.header_length,
	       "datafile-header"))
    goto err;
  info->rec_cache.file=new_file;		/* Use this file for cacheing*/

  lock_memory();
  for (key=0 ; key < share->state.keys ; key++)
    share->keyinfo[key].base.flag|= HA_SORT_ALLOWS_SAME;

  if (my_pread(share->kfile,(byte*) temp_buff,
	       (uint) keyinfo->base.block_length,
	       share->state.key_root[sort_key],
	       MYF(MY_NABP+MY_WME)))
  {
    print_error("Can't read indexpage from filepos: %lu",
		(ulong) share->state.key_root[sort_key]);
    goto err;
  }

  /* Setup param for sort_write_record */
  bzero((char*) &sort_info,sizeof(sort_info));
  sort_info.info=info;
  sort_info.new_data_file_type=share->data_file_type;
  sort_info.fix_datafile=1;
  sort_info.filepos=share->pack.header_length;
  sort_info.record=record_buff;
  old_record_count=share->state.records;
  share->state.records=0;

  if (sort_record_index(info,keyinfo,share->state.key_root[sort_key],temp_buff,
			sort_key,new_file) ||
      write_data_suffix(info) ||
      flush_io_cache(&info->rec_cache))
    goto err;

  if (share->state.records != old_record_count)
  {
    print_error("found %lu of %lu records",
		(ulong) share->state.records,(ulong) old_record_count);
    goto err;
  }

	/* Put same locks as old file */
  if (lock_file(new_file,0L,F_WRLCK,"tempfile",temp_filename))
    goto err;
  VOID(lock_file(info->dfile,0L,F_UNLCK,"datafile of",name));
  VOID(my_close(info->dfile,MYF(MY_WME)));
  out_flag|=O_NEW_DATA;				/* Data in new file */

  info->dfile=new_file;				/* Use new indexfile */
  share->state.del=share->state.empty=0;
  share->state.dellink= NI_POS_ERROR;
  share->state.data_file_length=sort_info.filepos;
  share->state.splitt=share->state.records;	/* Only hole records */
  share->state.version=(ulong) time((time_t*) 0);

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  got_error=0;

err:
  if (got_error && new_file >= 0)
  {
    VOID(my_close(new_file,MYF(MY_WME)));
    VOID(my_delete(temp_filename,MYF(MY_WME)));
  }
  if (temp_buff)
  {
    my_afree((gptr) temp_buff);
  }
  if (record_buff)
  {
    my_afree(record_buff);
  }
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  VOID(end_io_cache(&info->rec_cache));
  share->base.sortkey=sort_key;
  DBUG_RETURN(flush_blocks(share->kfile) | got_error);
} /* sort_records */


	 /* Sort records recursive using one index */

static int sort_record_index(info,keyinfo,page,buff,sort_key,new_file)
N_INFO *info;
N_KEYDEF *keyinfo;
ulong page;
uchar *buff;
uint sort_key;
File new_file;
{
  uint	nod_flag,used_length;
  uchar *temp_buff,*keypos,*endpos;
  ulong next_page,rec_pos;
  uchar lastkey[N_MAX_KEY_BUFF];
  DBUG_ENTER("sort_record_index");

  nod_flag=test_if_nod(buff);
  temp_buff=0;

  if (nod_flag)
  {
    if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->base.block_length)))
    {
      print_error("Not Enough memory");
      DBUG_RETURN(-1);
    }
  }
  used_length=getint(buff);
  keypos=buff+2+nod_flag;
  endpos=buff+used_length;
  for ( ;; )
  {
    _sanity(__FILE__,__LINE__);
    if (nod_flag)
    {
      next_page=_nisam_kpos(nod_flag,keypos);
      if (my_pread(info->s->kfile,(byte*) temp_buff,
		  (uint) keyinfo->base.block_length, next_page,
		   MYF(MY_NABP+MY_WME)))
      {
	print_error("Can't read keys from filepos: %lu",(ulong) next_page);
	goto err;
      }
      if (sort_record_index(info,keyinfo,next_page,temp_buff,sort_key,
			    new_file))
	goto err;
    }
    _sanity(__FILE__,__LINE__);
    if (keypos >= endpos ||
	(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey) == 0)
      break;
    rec_pos= _nisam_dpos(info,nod_flag,keypos);

    if ((*info->s->read_rnd)(info,record_buff,rec_pos,0))
    {
      print_error("%d when reading datafile",my_errno);
      goto err;
    }
    if (rec_pos != sort_info.filepos)
    {
      _nisam_dpointer(info,keypos-nod_flag-info->s->rec_reflength,
		   sort_info.filepos);
      if (movepoint(info,record_buff,rec_pos,sort_info.filepos,sort_key))
      {
	print_error("%d when updating key-pointers",my_errno);
	goto err;
      }
    }
    if (sort_write_record())
      goto err;
  }
  bzero((byte*) buff+used_length,keyinfo->base.block_length-used_length);
  if (my_pwrite(info->s->kfile,(byte*) buff,(uint) keyinfo->base.block_length,
		page,MYF_RW))
  {
    print_error("%d when updating keyblock",my_errno);
    goto err;
  }
  if (temp_buff)
    my_afree((gptr) temp_buff);
  DBUG_RETURN(0);
err:
  if (temp_buff)
    my_afree((gptr) temp_buff);
  DBUG_RETURN(1);
} /* sort_record_index */


	/* Sort index for more efficent reads */

static int sort_index(info,name)
register N_INFO *info;
my_string name;
{
  reg2 uint key;
  reg1 N_KEYDEF *keyinfo;
  File new_file;
  ulong index_pos[N_MAXKEY];
  DBUG_ENTER("sort_index");

  if (!(testflag & T_SILENT))
    printf("- Sorting index for ISAM-table '%s'\n",name);

  if ((new_file=my_create(fn_format(temp_filename,name,"",INDEX_TMP_EXT,2+4),
			  0,tmpfile_createflag,MYF(0))) <= 0)
  {
    print_error("Can't create new tempfile: '%s'",temp_filename);
    DBUG_RETURN(-1);
  }
  if (filecopy(new_file,info->s->kfile,0L,(ulong) info->s->base.keystart,
	       "headerblock"))
    goto err;

  new_file_pos=info->s->base.keystart;
  for (key= 0,keyinfo= &info->s->keyinfo[0]; key < info->s->state.keys ;
       key++,keyinfo++)
  {
    if (info->s->state.key_root[key] != NI_POS_ERROR)
    {
      index_pos[key]=new_file_pos;		/* Write first block here */
      if (!_nisam_fetch_keypage(info,keyinfo,info->s->state.key_root[key],
			     info->buff,0))
      {
	print_error("Can't read indexpage from filepos: %lu",
		    (ulong) info->s->state.key_root[key]);
	goto err;
      }
      if (sort_one_index(info,keyinfo,info->buff,new_file))
	goto err;
    }
    else
      index_pos[key]= NI_POS_ERROR;		/* No blocks */
  }

	/* Put same locks as old file */
  if (lock_file(new_file,0L,F_WRLCK,"tempfile",temp_filename))
    goto err;
  info->s->state.version=(ulong) time((time_t*) 0);
  VOID(_nisam_writeinfo(info,1));			/* This unlocks table */
  VOID(my_close(info->s->kfile,MYF(MY_WME)));
  out_flag|=O_NEW_INDEX;			/* Data in new file */

  info->s->kfile=new_file;
  info->s->state.key_file_length=new_file_pos;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (key=0 ; key < info->s->state.keys ; key++)
  {
    info->s->state.key_root[key]=index_pos[key];
    info->s->state.key_del[key]= NI_POS_ERROR;
  }
  DBUG_RETURN(0);

err:
  VOID(my_close(new_file,MYF(MY_WME)));
  VOID(my_delete(temp_filename,MYF(MY_WME)));
  DBUG_RETURN(-1);
} /* sort_index */


	 /* Sort records recursive using one index */

static int sort_one_index(info,keyinfo,buff,new_file)
N_INFO *info;
N_KEYDEF *keyinfo;
uchar *buff;
File new_file;
{
  uint length,nod_flag,used_length;
  uchar *temp_buff,*keypos,*endpos;
  ulong new_page_pos,next_page;
  DBUG_ENTER("sort_one_index");

  temp_buff=0;
  new_page_pos=new_file_pos;
  new_file_pos+=keyinfo->base.block_length;

  if ((nod_flag=test_if_nod(buff)))
  {
    if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->base.block_length)))
    {
      print_error("Not Enough memory");
      DBUG_RETURN(-1);
    }

    used_length=getint(buff);
    keypos=buff+2+nod_flag;
    endpos=buff+used_length;
    for ( ;; )
    {
      if (nod_flag)
      {
	next_page=_nisam_kpos(nod_flag,keypos);
	_nisam_kpointer(info,keypos-nod_flag,new_file_pos); /* Save new pos */
	if (!_nisam_fetch_keypage(info,keyinfo,next_page,temp_buff,0))
	{
	  print_error("Can't read keys from filepos: %lu",
		      (ulong) next_page);
	  goto err;
	}
	if (sort_one_index(info,keyinfo,temp_buff,new_file))
	  goto err;
      }
      if (keypos >= endpos ||
	  ((*keyinfo->get_key)(keyinfo,nod_flag,&keypos,info->lastkey)) == 0)
	break;
    }
    my_afree((gptr) temp_buff);
  }

	/* Fill block with zero and write it to new file */

  length=getint(buff);
  bzero((byte*) buff+length,keyinfo->base.block_length-length);
  if (my_pwrite(new_file,(byte*) buff,(uint) keyinfo->base.block_length,
		new_page_pos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
  {
    print_error("Can't write indexblock, error: %d",my_errno);
    goto err;
  }
  DBUG_RETURN(0);
err:
  if (temp_buff)
    my_afree((gptr) temp_buff);
  DBUG_RETURN(1);
} /* sort_one_index */


	/* Change to use new file */
	/* Copy stats from old file to new file, deletes orginal and */
	/* changes new file name to old file name */

static int change_to_newfile(filename,old_ext,new_ext)
const char *filename,*old_ext,*new_ext;
{
  char old_filename[FN_REFLEN],new_filename[FN_REFLEN];

  return my_redel(fn_format(old_filename,filename,"",old_ext,2+4),
		  fn_format(new_filename,filename,"",new_ext,2+4),
		  MYF(MY_WME+MY_LINK_WARNING));
} /* change_to_newfile */


	/* Locks a hole file */
	/* Gives error-message if file can't be locked */

static int lock_file(file,start,lock_type,filetype,filename)
File file;
ulong start;
int lock_type;
const char *filetype,*filename;
{
#ifndef NO_LOCKING
  if (my_lock(file,lock_type,start,F_TO_EOF,
	      testflag & T_WAIT_FOREVER ? MYF(MY_SEEK_NOT_DONE) :
	      MYF(MY_SEEK_NOT_DONE |  MY_DONT_WAIT)))
  {
    print_error(" %d when %s %s '%s'",my_errno,
		lock_type == F_UNLCK ? "unlocking": "locking",
		filetype,filename);
    error_printed=2;		/* Don't give that data is crashed */
    return 1;
  }
#endif
  return 0;
} /* lock_file */


	/* Copy a block between two files */

static int filecopy(File to,File from,ulong start,ulong length,
		    const char *type)
{
  char tmp_buff[IO_SIZE],*buff;
  ulong buff_length;
  DBUG_ENTER("filecopy");

  buff_length=min(write_buffer_length,length);
  if (!(buff=my_malloc(buff_length,MYF(0))))
  {
    buff=tmp_buff; buff_length=IO_SIZE;
  }

  VOID(my_seek(from,start,MY_SEEK_SET,MYF(0)));
  while (length > buff_length)
  {
    if (my_read(from,(byte*) buff,buff_length,MYF(MY_NABP)) ||
	my_write(to,(byte*) buff,buff_length,MYF(MY_NABP | MY_WAIT_IF_FULL)))
      goto err;
    length-= buff_length;
  }
  if (my_read(from,(byte*) buff,(uint) length,MYF(MY_NABP)) ||
      my_write(to,(byte*) buff,(uint) length,MYF(MY_NABP | MY_WAIT_IF_FULL)))
    goto err;
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  DBUG_RETURN(0);
err:
  if (buff != tmp_buff)
    my_free(buff,MYF(0));
  print_error("Can't copy %s to tempfile, error %d",type,my_errno);
  DBUG_RETURN(1);
}


	/* Fix table using sorting */
	/* saves new table in temp_filename */

static int rep_by_sort(info,name)
reg1 N_INFO *info;
my_string name;
{
  int got_error;
  uint i,length;
  ulong start_records,new_header_length,del;
  File new_file;
  SORT_PARAM sort_param;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("rep_by_sort");

  start_records=share->state.records;
  got_error=1;
  new_file= -1;
  new_header_length=(testflag & T_UNPACK) ? 0 : share->pack.header_length;
  if (!(testflag & T_SILENT))
  {
    printf("- recovering ISAM-table '%s'\n",name);
    printf("Data records: %lu\n",(ulong) start_records);
  }
  bzero((char*) &sort_info,sizeof(sort_info));
  if (!(sort_info.key_block=alloc_key_blocks((uint) sort_key_blocks,
					     share->base.max_block))
      || init_io_cache(&read_cache,info->dfile,(uint) read_buffer_length,
		      READ_CACHE,share->pack.header_length,1,MYF(MY_WME)) ||
      (! rep_quick &&
       init_io_cache(&info->rec_cache,info->dfile,(uint) write_buffer_length,
		    WRITE_CACHE,new_header_length,1,
		    MYF(MY_WME | MY_WAIT_IF_FULL))))
    goto err;
  sort_info.key_block_end=sort_info.key_block+sort_key_blocks;
  info->opt_flag|=WRITE_CACHE_USED;
  info->rec_cache.file=info->dfile;		/* for sort_delete_record */

  if (!(sort_info.record=(byte*) my_alloca((uint) share->base.reclength)))
  {
    print_error("Not enough memory for extra record");
    goto err;
  }
  if (!rep_quick)
  {
    if ((new_file=my_create(fn_format(temp_filename,name,"",DATA_TMP_EXT,
				      2+4),
			    0,tmpfile_createflag,MYF(0))) < 0)
    {
      print_error("Can't create new tempfile: '%s'",temp_filename);
      goto err;
    }
    if (filecopy(new_file,info->dfile,0L,new_header_length,"datafile-header"))
      goto err;
    if (testflag & T_UNPACK)
      share->base.options&= ~HA_OPTION_COMPRESS_RECORD;
    share->state.dellink= NI_POS_ERROR;
    info->rec_cache.file=new_file;
  }

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (i=0 ; i < N_MAXKEY ; i++)
    share->state.key_del[i]=share->state.key_root[i]= NI_POS_ERROR;
  share->state.key_file_length=share->base.keystart;

  sort_info.info=info;
  if ((sort_info.new_data_file_type=share->data_file_type) ==
      COMPRESSED_RECORD && testflag & T_UNPACK)
  {
    if (share->base.options & HA_OPTION_PACK_RECORD)
      sort_info.new_data_file_type = DYNAMIC_RECORD;
    else
      sort_info.new_data_file_type = STATIC_RECORD;
  }

  sort_info.filepos=new_header_length;
  sort_info.dupp=0;
  read_cache.end_of_file=sort_info.filelength=
    (ulong) my_seek(read_cache.file,0L,MY_SEEK_END,MYF(0));

  if (share->data_file_type == DYNAMIC_RECORD)
    length=max(share->base.min_pack_length+1,share->base.min_block_length);
  else if (share->data_file_type == COMPRESSED_RECORD)
    length=share->base.min_block_length;
  else
    length=share->base.reclength;
  sort_param.max_records=sort_info.max_records=sort_info.filelength/length+1;
  sort_param.key_cmp=sort_key_cmp;
  sort_param.key_write=sort_key_write;
  sort_param.key_read=sort_key_read;
  sort_param.lock_in_memory=lock_memory;
  del=share->state.del;

  for (sort_info.key=0 ; sort_info.key < share->state.keys ; sort_info.key++)
  {
    if ((!(testflag & T_SILENT)))
      printf ("- Fixing index %d\n",sort_info.key+1);
    sort_info.max_pos=sort_info.pos=share->pack.header_length;
    sort_info.keyinfo=share->keyinfo+sort_info.key;
    sort_info.keyseg=sort_info.keyinfo->seg;
    sort_info.fix_datafile= (my_bool) (sort_info.key == 0 && ! rep_quick);
    sort_info.unique=0;
    sort_param.key_length=share->rec_reflength;
    for (i=0 ; sort_info.keyseg[i].base.type ; i++)
      sort_param.key_length+=sort_info.keyseg[i].base.length+
	(sort_info.keyseg[i].base.flag & HA_SPACE_PACK ? 1 : 0);
    share->state.records=share->state.del=share->state.empty=share->state.splitt=0;

    if (_create_index_by_sort(&sort_param,
			      (pbool) (!(testflag & T_VERBOSE)),
			      (uint) sort_buffer_length))
      goto err;
		/* Set for next loop */
    sort_param.max_records=sort_info.max_records=share->state.records;
    share->base.rec_per_key[sort_info.key]=
      sort_info.unique ? ((sort_info.max_records+sort_info.unique/2)/
			   sort_info.unique)
      : 1L;

    if (sort_info.fix_datafile)
    {
      info->dfile=new_file;
      share->state.data_file_length=sort_info.filepos;
      share->state.splitt=share->state.records; /* Only hole records */
      share->state.version=(ulong) time((time_t*) 0);
      out_flag|=O_NEW_DATA;			/* Data in new file */
      read_cache.end_of_file=sort_info.filepos;
      if (write_data_suffix(info) || end_io_cache(&info->rec_cache))
	goto err;
      share->data_file_type=sort_info.new_data_file_type;
      share->pack.header_length=new_header_length;
    }
    else
      share->state.data_file_length=sort_info.max_pos;

    if (flush_pending_blocks())
      goto err;

    read_cache.file=info->dfile;		/* re-init read cache */
    reinit_io_cache(&read_cache,READ_CACHE,share->pack.header_length,1,1);
  }

  if (testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }

  if (rep_quick && del+sort_info.dupp != share->state.del)
  {
    print_error("Couldn't fix table with quick recovery: Found wrong number of deleted records");
    print_error("Run recovery again without -q");
    got_error=1;
    goto err;
  }

  if (rep_quick != 1)
  {
    ulong skr=share->state.data_file_length+
      (share->base.options & HA_OPTION_COMPRESS_RECORD ?
       MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (share->data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength)
      if (my_chsize(info->dfile,skr,MYF(0)))
	print_warning("Can't change size of datafile,  error: %d",my_errno);
  }
  if (my_chsize(share->kfile,share->state.key_file_length,MYF(0)))
    print_warning("Can't change size of indexfile, error: %d",my_errno);

  if (!(testflag & T_SILENT))
  {
    if (start_records != share->state.records)
      printf("Data records: %lu\n",(ulong) share->state.records);
    if (sort_info.dupp)
      print_warning("%lu records have been removed",(ulong) sort_info.dupp);
  }
  got_error=0;

err:
  if (got_error)
  {
    if (! error_printed)
      print_error("%d when fixing table",my_errno);
    if (new_file >= 0)
    {
      VOID(end_io_cache(&info->rec_cache));
      VOID(my_close(new_file,MYF(0)));
      VOID(my_delete(temp_filename,MYF(MY_WME)));
    }
  }
  if (sort_info.key_block)
    my_free((gptr) sort_info.key_block,MYF(0));
  if (sort_info.record)
  {
    my_afree(sort_info.record);
  }
  VOID(end_io_cache(&read_cache));
  VOID(end_io_cache(&info->rec_cache));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  if (!got_error && testflag & T_UNPACK)
  {
    share->state.header.options[0]&= (uchar) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
  }
  DBUG_RETURN(got_error);
} /* rep_by_sort */


	/* Read next record and return next key */

static int sort_key_read(key)
void *key;
{
  int error;
  N_INFO *info;
  DBUG_ENTER("sort_key_read");

  info=sort_info.info;

  if ((error=sort_get_next_record()))
    DBUG_RETURN(error);
  if (info->s->state.records == sort_info.max_records)
  {
    print_error("Found too many records; Can`t continue");
    DBUG_RETURN(1);
  }
  VOID(_nisam_make_key(info,sort_info.key,key,sort_info.record,
		    sort_info.filepos));
  DBUG_RETURN(sort_write_record());
} /* sort_key_read */


	/* Read next record from file using parameters in sort_info */
	/* Return -1 if end of file, 0 if ok and > 0 if error */

static int sort_get_next_record()
{
  int searching;
  uint found_record,b_type,left_length;
  ulong pos;
  byte *to;
  BLOCK_INFO block_info;
  N_INFO *info;
  ISAM_SHARE *share;
  DBUG_ENTER("sort_get_next_record");

  info=sort_info.info;
  share=info->s;
  switch (share->data_file_type) {
  case STATIC_RECORD:
    for (;;)
    {
      if (my_b_read(&read_cache,sort_info.record,share->base.reclength))
	DBUG_RETURN(-1);
      sort_info.start_recpos=sort_info.pos;
      if (!sort_info.fix_datafile)
	sort_info.filepos=sort_info.pos;
      sort_info.max_pos=(sort_info.pos+=share->base.reclength);
      share->state.splitt++;
      if (*sort_info.record)
	DBUG_RETURN(0);
      if (!sort_info.fix_datafile)
      {
	share->state.del++;
	share->state.empty+=share->base.reclength;
      }
    }
  case DYNAMIC_RECORD:
    LINT_INIT(to);
    pos=sort_info.pos;
    searching=(sort_info.fix_datafile && (testflag & T_EXTEND));
    for (;;)
    {
      found_record=block_info.second_read= 0;
      left_length=1;
      do
      {
	if (pos > sort_info.max_pos)
	  sort_info.max_pos=pos;
	if (found_record && pos == search_after_block)
	  print_info("Block: %lu used by record at %lu",
		     search_after_block,
		     sort_info.start_recpos);
	if (_nisam_read_cache(&read_cache,(byte*) block_info.header,pos,
			  BLOCK_INFO_HEADER_LENGTH, test(! found_record) | 2))
	{
	  if (found_record)
	  {
	    print_info("Can't read whole record at %lu (errno: %d)",
		       (ulong) sort_info.start_recpos,errno);
	    goto try_next;
	  }
	  DBUG_RETURN(-1);
	}
	if (searching && ! sort_info.fix_datafile)
	{
	  error_printed=1;
	  DBUG_RETURN(1);	/* Something wrong with data */
	}
	if (((b_type=_nisam_get_block_info(&block_info,-1,pos)) &
	     (BLOCK_ERROR | BLOCK_FATAL_ERROR)) ||
	    ((b_type & BLOCK_FIRST) &&
	     (block_info.rec_len < (uint) share->base.min_pack_length ||
	      block_info.rec_len > (uint) share->base.max_pack_length)))
	{
	  uint i;
	  if (testflag & T_VERBOSE || searching == 0)
	    print_info("Wrong bytesec: %3d-%3d-%3d at %10lu; Skipped",
		       block_info.header[0],block_info.header[1],
		       block_info.header[2],pos);
	  if (found_record)
	    goto try_next;
	  block_info.second_read=0;
	  searching=1;
	  for (i=1 ; i < 11 ; i++) /* Skipp from read string */
	    if (block_info.header[i] >= 1 && block_info.header[i] <= 16)
	      break;
	  pos+=(ulong) i;
	  continue;
	}
	if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	    share->base.min_block_length ||
	    block_info.block_len-4 > (uint) share->base.max_pack_length)
	{
	  if (!searching)
	    print_info("Found block with impossible length %u at %lu; Skipped",
		       block_info.block_len+ (uint) (block_info.filepos-pos),
		       (ulong) pos);
	  if (found_record)
	    goto try_next;
	  searching=1;
	  pos++;
	  block_info.second_read=0;
	  continue;
	}
	if (b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
	{
	  if (!sort_info.fix_datafile && (b_type & BLOCK_DELETED))
	  {
	    share->state.empty+=block_info.block_len;
	    share->state.del++;
	    share->state.splitt++;
	  }
	  if (found_record)
	    goto try_next;
	  /* Check if impossible big deleted block */
	  if (block_info.block_len > share->base.max_pack_length +4)
	    searching=1;
	  if (searching)
	    pos++;
	  else
	    pos=block_info.filepos+block_info.block_len;
	  block_info.second_read=0;
	  continue;
	}

	share->state.splitt++;
	if (! found_record++)
	{
	  sort_info.find_length=left_length=block_info.rec_len;
	  sort_info.start_recpos=pos;
	  if (!sort_info.fix_datafile)
	    sort_info.filepos=sort_info.start_recpos;
	  if (sort_info.fix_datafile && (testflag & T_EXTEND))
	    sort_info.pos=block_info.filepos+1;
	  else
	    sort_info.pos=block_info.filepos+block_info.block_len;
	  if (share->base.blobs)
	  {
	    if (!(to=fix_rec_buff_for_blob(info,block_info.rec_len)))
	    {
	      print_error("Not enough memory for blob at %lu",
			  (ulong) sort_info.start_recpos);
	      DBUG_RETURN(-1);
	    }
	  }
	  else
	    to= info->rec_buff;
	}
	if (left_length < block_info.data_len || ! block_info.data_len)
	{
	  print_info("Found block with too small length at %lu; Skipped",
		     (ulong) sort_info.start_recpos);
	  goto try_next;
	}
	if (block_info.filepos + block_info.data_len > read_cache.end_of_file)
	{
	  print_info("Found block that points outside data file at %lu",
		     (ulong) sort_info.start_recpos);
	  goto try_next;
	}
	if (_nisam_read_cache(&read_cache,to,block_info.filepos,
			   block_info.data_len, test(found_record == 1)))
	{
	  print_info("Read error for block at: %lu (error: %d); Skipped",
		     (ulong) block_info.filepos,my_errno);
	  goto try_next;
	}
	left_length-=block_info.data_len;
	to+=block_info.data_len;
	pos=block_info.next_filepos;
	if (pos == NI_POS_ERROR && left_length)
	{
	  print_info("Wrong block with wrong total length starting at %lu",
		     (ulong) sort_info.start_recpos);
	  goto try_next;
	}
	if (pos + BLOCK_INFO_HEADER_LENGTH > read_cache.end_of_file)
	{
	  print_info("Found link that points at %lu (outside data file) at %lu",
		     (ulong) pos,(ulong) sort_info.start_recpos);
	  goto try_next;
	}
      } while (left_length);

      if (_nisam_rec_unpack(info,sort_info.record,info->rec_buff,
			 sort_info.find_length) != MY_FILE_ERROR)
      {
	if (read_cache.error < 0)
	  DBUG_RETURN(1);
	if ((testflag & (T_EXTEND | T_REP)) || searching)
	{
	  if (_nisam_rec_check(info, sort_info.record))
	  {
	    print_info("Found wrong packed record at %lu",
		       (ulong) sort_info.start_recpos);
	    goto try_next;
	  }
	}
	DBUG_RETURN(0);
      }
      if (!searching)
      {
	print_info("Found wrong packed record at %lu",
		   (ulong) sort_info.start_recpos);
      }
    try_next:
      pos=sort_info.start_recpos+1;
      searching=1;
    }
  case COMPRESSED_RECORD:
    for (searching=0 ;; searching=1, sort_info.pos++)
    {
      if (_nisam_read_cache(&read_cache,(byte*) block_info.header,sort_info.pos,
			share->pack.ref_length,1))
	DBUG_RETURN(-1);
      if (searching && ! sort_info.fix_datafile)
      {
	error_printed=1;
	DBUG_RETURN(1);		/* Something wrong with data */
      }
      sort_info.start_recpos=sort_info.pos;
      VOID(_nisam_pack_get_block_info(&block_info,share->pack.ref_length,-1,
				   sort_info.pos));
      if (!block_info.rec_len &&
	  sort_info.pos + MEMMAP_EXTRA_MARGIN == read_cache.end_of_file)
	DBUG_RETURN(-1);
      if (block_info.rec_len < (uint) share->min_pack_length ||
	  block_info.rec_len > (uint) share->max_pack_length)
      {
	if (! searching)
	  print_info("Found block with wrong recordlength: %d at %lu\n",
		     block_info.rec_len, (ulong) sort_info.pos);
	continue;
      }
      if (_nisam_read_cache(&read_cache,(byte*) info->rec_buff,
			block_info.filepos, block_info.rec_len,1))
      {
	if (! searching)
	  print_info("Couldn't read hole record from %lu",
		     (ulong) sort_info.pos);
	continue;
      }
      if (_nisam_pack_rec_unpack(info,sort_info.record,info->rec_buff,
			      block_info.rec_len))
      {
	if (! searching)
	  print_info("Found wrong record at %lu",(ulong) sort_info.pos);
	continue;
      }
      if (!sort_info.fix_datafile)
	sort_info.filepos=sort_info.pos;
      sort_info.max_pos=(sort_info.pos+=share->pack.ref_length+
			 block_info.rec_len);
      share->state.splitt++;
      info->packed_length=block_info.rec_len;
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);		/* Impossible */
}


	/* Write record to new file */

static int sort_write_record()
{
  int flag;
  uint block_length,reclength;
  byte *from;
  uchar *block_buff[3];
  N_INFO *info;
  ISAM_SHARE *share;
  DBUG_ENTER("sort_write_record");

  info=sort_info.info;
  share=info->s;
  if (sort_info.fix_datafile)
  {
    switch (sort_info.new_data_file_type) {
    case STATIC_RECORD:
      if (my_b_write(&info->rec_cache,sort_info.record, share->base.reclength))
      {
	print_error("%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_info.filepos+=share->base.reclength;
      break;
    case DYNAMIC_RECORD:
      if (! info->blobs)
	from=info->rec_buff;
      else
      {
	/* must be sure that local buffer is big enough */
	reclength=info->s->base.pack_reclength+
	  _calc_total_blob_length(info,sort_info.record)+
	  ALIGN_SIZE(MAX_DYN_BLOCK_HEADER)+N_SPLITT_LENGTH+
	  DYN_DELETE_BLOCK_HEADER;
	if (sort_info.buff_length < reclength)
	{
	  if (!(sort_info.buff=my_realloc(sort_info.buff, (uint) reclength,
					  MYF(MY_FREE_ON_ERROR |
					      MY_ALLOW_ZERO_PTR))))
	    DBUG_RETURN(1);
	  sort_info.buff_length=reclength;
	}
	from=sort_info.buff+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER);
      }
      reclength=_nisam_rec_pack(info,from,sort_info.record);
      block_length=reclength+ 3 +test(reclength > 65532L);
      if (block_length < share->base.min_block_length)
	block_length=share->base.min_block_length;
      flag=0;
      info->update|=HA_STATE_WRITE_AT_END;
      if (_nisam_write_part_record(info,0L,block_length,NI_POS_ERROR,
				&from,&reclength,&flag))
      {
	print_error("%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_info.filepos+=block_length;
      break;
    case COMPRESSED_RECORD:
      reclength=info->packed_length;
      save_integer((byte*) block_buff,share->pack.ref_length,reclength);
      if (my_b_write(&info->rec_cache,(byte*) block_buff,share->pack.ref_length)
	  || my_b_write(&info->rec_cache,(byte*) info->rec_buff,reclength))
      {
	print_error("%d when writing to datafile",my_errno);
	DBUG_RETURN(1);
      }
      sort_info.filepos+=reclength+share->pack.ref_length;
      break;
    }
  }
  share->state.records++;
  if (testflag & T_WRITE_LOOP && share->state.records % WRITE_COUNT == 0)
  {
    printf("%lu\r",(ulong) share->state.records); VOID(fflush(stdout));
  }
  DBUG_RETURN(0);
} /* sort_write_record */


	/* Compare two keys from _create_index_by_sort */

static int sort_key_cmp(const void *not_used __attribute__((unused)),
			const void *a,const void *b)
{
  return (_nisam_key_cmp(sort_info.keyseg,*((uchar**) a),*((uchar**) b),0,
		      SEARCH_SAME));
} /* sort_key_cmp */


static int sort_key_write( const void *a)
{
  int cmp=sort_info.key_block->inited ?
    _nisam_key_cmp(sort_info.keyseg,sort_info.key_block->lastkey,(uchar*) a,
	       0,SEARCH_FIND) : -1L;
  if ((sort_info.keyinfo->base.flag & HA_NOSAME) &&
      cmp == 0)
  {
    sort_info.dupp++;
    print_warning("Dupplicate key for record at %10lu against record at %10lu",
		  sort_info.info->lastpos=get_record_for_key(sort_info.info,
							     sort_info.keyinfo,
							     (uchar*) a),
		  get_record_for_key(sort_info.info,sort_info.keyinfo,
				     sort_info.key_block->lastkey));
    if (testflag & T_VERBOSE)
      _nisam_print_key(stdout,sort_info.keyseg,(uchar*) a);
    return(sort_delete_record());
  }
  if (cmp)
    sort_info.unique++;
#ifndef DBUG_OFF
  if (cmp > 0)
  {
    print_error("Fatal intern error: Keys are not in order from sort");
    return(1);
  }
#endif
  return (sort_insert_key(sort_info.key_block,(uchar*) a,NI_POS_ERROR));
} /* sort_key_write */


	/* get pointer to record from a key */

static ulong get_record_for_key( N_INFO *info, N_KEYDEF *keyinfo, uchar *key)
{
  return _nisam_dpos(info,0,key+_nisam_keylength(keyinfo,key));
} /* get_record_for_key */


	/* Insert a key in sort-key-blocks */

static int sort_insert_key(reg1 ISAM_SORT_KEY_BLOCKS *key_block,
			   uchar *key, ulong prev_block)
{
  uint a_length,t_length,nod_flag;
  ulong filepos;
  uchar *anc_buff,*lastkey;
  S_PARAM s_temp;
  N_INFO *info;
  DBUG_ENTER("sort_insert_key");

  anc_buff=key_block->buff;
  info=sort_info.info;
  lastkey=key_block->lastkey;
  nod_flag= (key_block == sort_info.key_block ? 0 :
	     sort_info.info->s->base.key_reflength);

  if (!key_block->inited)
  {
    key_block->inited=1;
    if (key_block == sort_info.key_block_end)
    {
      print_error("To many keyblocklevels; Try increasing sort_key_blocks");
      DBUG_RETURN(1);
    }
    a_length=2+nod_flag;
    key_block->end_pos=anc_buff+2;
    lastkey=0;					/* No previous key in block */
  }
  else
    a_length=getint(anc_buff);

	/* Save pointer to previous block */
  if (nod_flag)
    _nisam_kpointer(info,key_block->end_pos,prev_block);

  t_length=_nisam_get_pack_key_length(sort_info.keyinfo,nod_flag,
				   (uchar*) 0,lastkey,key,&s_temp);
  _nisam_store_key(sort_info.keyinfo,key_block->end_pos+nod_flag,&s_temp);
  a_length+=t_length;
  putint(anc_buff,a_length,nod_flag);
  key_block->end_pos+=t_length;
  if (a_length <= sort_info.keyinfo->base.block_length)
  {
    VOID(_nisam_move_key(sort_info.keyinfo,key_block->lastkey,key));
    key_block->last_length=a_length-t_length;
    DBUG_RETURN(0);
  }

	/* Fill block with end-zero and write filled block */
  putint(anc_buff,key_block->last_length,nod_flag);
  bzero((byte*) anc_buff+key_block->last_length,
	sort_info.keyinfo->base.block_length- key_block->last_length);
  if ((filepos=_nisam_new(info,sort_info.keyinfo)) == NI_POS_ERROR)
    return 1;
  if (my_pwrite(info->s->kfile,(byte*) anc_buff,
		(uint) sort_info.keyinfo->base.block_length,filepos,MYF_RW))
    DBUG_RETURN(1);
  DBUG_DUMP("buff",(byte*) anc_buff,getint(anc_buff));

	/* Write separator-key to block in next level */
  if (sort_insert_key(key_block+1,key_block->lastkey,filepos))
    DBUG_RETURN(1);

	/* clear old block and write new key in it */
  key_block->inited=0;
  DBUG_RETURN(sort_insert_key(key_block,key,prev_block));
} /* sort_insert_key */


	/* Delete record when we found a dupplicated key */

static int sort_delete_record()
{
  uint i;
  int old_file,error;
  uchar *key;
  N_INFO *info;
  DBUG_ENTER("sort_delete_record");

  if (rep_quick == 1)
  {
    VOID(fputs("Quick-recover aborted; Run recovery without switch 'q' or with switch -qq\n",stderr));
    error_printed=1;
    DBUG_RETURN(1);
  }
  info=sort_info.info;
  if (info->s->base.options & HA_OPTION_COMPRESS_RECORD)
  {
    VOID(fputs("Recover aborted; Can't run standard recovery on compressed tables\nwith errors in data-file\nUse switch '--safe-recover' to fix it\n",stderr));
    error_printed=1;
    DBUG_RETURN(1);
  }

  old_file=info->dfile;
  info->dfile=info->rec_cache.file;
  if (sort_info.key)
  {
    key=info->lastkey+info->s->base.max_key_length;
    if ((*info->s->read_rnd)(info,sort_info.record,info->lastpos,0) < 0)
    {
      print_error("Can't read record to be removed");
      info->dfile=old_file;
      DBUG_RETURN(1);
    }

    for (i=0 ; i < sort_info.key ; i++)
    {
      VOID(_nisam_make_key(info,i,key,sort_info.record,info->lastpos));
      if (_nisam_ck_delete(info,i,key))
      {
	print_error("Can't delete key %d from record to be removed",i+1);
	info->dfile=old_file;
	DBUG_RETURN(1);
      }
    }
  }
  error=flush_io_cache(&info->rec_cache) || (*info->s->delete_record)(info);
  info->dfile=old_file;				/* restore actual value */
  info->s->state.records--;
  DBUG_RETURN(error);
} /* sort_delete_record */


	/* Fix all pending blocks and flush everything to disk */

static int flush_pending_blocks()
{
  uint nod_flag,length;
  ulong filepos;
  N_INFO *info;
  ISAM_SORT_KEY_BLOCKS *key_block;
  DBUG_ENTER("flush_pending_blocks");

  filepos= NI_POS_ERROR;			/* if empty file */
  info=sort_info.info;
  nod_flag=0;
  for (key_block=sort_info.key_block ; key_block->inited ; key_block++)
  {
    key_block->inited=0;
    length=getint(key_block->buff);
    if (nod_flag)
      _nisam_kpointer(info,key_block->end_pos,filepos);
    if ((filepos=_nisam_new(info,sort_info.keyinfo)) == NI_POS_ERROR)
      DBUG_RETURN(1);
    bzero((byte*) key_block->buff+length,
	  sort_info.keyinfo->base.block_length-length);
    if (my_pwrite(info->s->kfile,(byte*) key_block->buff,
		 (uint) sort_info.keyinfo->base.block_length,filepos,MYF_RW))
      DBUG_RETURN(1);
    DBUG_DUMP("buff",(byte*) key_block->buff,length);
    nod_flag=1;
  }
  info->s->state.key_root[sort_info.key]=filepos;	/* Last is root for tree */
  DBUG_RETURN(0);
} /* flush_pending_blocks */


	/* alloc space and pointers for key_blocks */

static ISAM_SORT_KEY_BLOCKS *alloc_key_blocks(uint blocks, uint buffer_length)
{
  reg1 uint i;
  ISAM_SORT_KEY_BLOCKS *block;
  DBUG_ENTER("alloc_key_blocks");

  if (!(block=(ISAM_SORT_KEY_BLOCKS*) my_malloc((sizeof(ISAM_SORT_KEY_BLOCKS)+
						 buffer_length+IO_SIZE)*blocks,
						MYF(0))))
  {
    print_error("Not Enough memory for sort-key-blocks");
    return(0);
  }
  for (i=0 ; i < blocks ; i++)
  {
    block[i].inited=0;
    block[i].buff=(uchar*) (block+blocks)+(buffer_length+IO_SIZE)*i;
  }
  DBUG_RETURN(block);
} /* alloc_key_blocks */


	/* print warnings and errors */
	/* VARARGS */

static void print_info(const char * fmt,...)
{
  va_list args;

  va_start(args,fmt);
  VOID(vfprintf(stdout, fmt, args));
  VOID(fputc('\n',stdout));
  va_end(args);
  return;
}

/* VARARGS */

static void print_warning(const char * fmt,...)
{
  va_list args;
  DBUG_ENTER("print_warning");

  if (!warning_printed && !error_printed)
  {
    fflush(stdout);
    if (testflag & T_SILENT)
      fprintf(stderr,"%s: ISAM file %s\n",my_progname,isam_file_name);
  }
  warning_printed=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: warning: ",my_progname);
  VOID(vfprintf(stderr, fmt, args));
  VOID(fputc('\n',stderr));
  va_end(args);
  DBUG_VOID_RETURN;
}

/* VARARGS */

void print_error(const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("print_error");
  DBUG_PRINT("enter",("format: %s",fmt));

  if (!warning_printed && !error_printed)
  {
    fflush(stdout);
    if (testflag & T_SILENT)
      fprintf(stderr,"%s: ISAM file %s\n",my_progname,isam_file_name);
  }
  error_printed|=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: error: ",my_progname);
  VOID(vfprintf(stderr, fmt, args));
  VOID(fputc('\n',stderr));
  va_end(args);
  DBUG_VOID_RETURN;
}

	/* Check if file is almost full */

static int test_if_almost_full(N_INFO *info)
{
  double diff= 0.9;
  if (info->s->base.options & HA_OPTION_COMPRESS_RECORD)
  {						/* Fix problem with pack_isam */
    diff=1.0;
    if (info->s->base.rec_reflength == 4)
      info->s->base.max_data_file_length= (uint32) ~0L;
    else
      info->s->base.max_data_file_length=
	1L << (info->s->base.rec_reflength);
  }
  return (my_seek(info->s->kfile,0L,MY_SEEK_END,MYF(0)) >
	  (ulong) (info->s->base.max_key_file_length*diff) ||
	   my_seek(info->dfile,0L,MY_SEEK_END,MYF(0)) >
	   (ulong) (info->s->base.max_data_file_length*diff));
}

	/* Recreate table with bigger more alloced record-data */

static int recreate_database(N_INFO **org_info, char *filename)
{
  int error;
  N_INFO info;
  ISAM_SHARE share;
  N_KEYDEF *keyinfo;
  N_RECINFO *recinfo,*rec,*end;
  uint unpack;
  ulong max_records;
  char name[FN_REFLEN];

  error=1;					/* Default error */
  info= **org_info;
  share= *(*org_info)->s;
  unpack= (share.base.options & HA_OPTION_COMPRESS_RECORD) &&
    (testflag & T_UNPACK);
  if (!(keyinfo=(N_KEYDEF*) my_alloca(sizeof(N_KEYDEF)*share.base.keys)))
    return 0;
  memcpy((byte*) keyinfo,(byte*) share.keyinfo,
	 (size_t) (sizeof(N_KEYDEF)*share.base.keys));
  if (!(recinfo=(N_RECINFO*)
	my_alloca(sizeof(N_RECINFO)*(share.base.fields+1))))
  {
    my_afree((gptr) keyinfo);
    return 1;
  }
  memcpy((byte*) recinfo,(byte*) share.rec,
	 (size_t) (sizeof(N_RECINFO)*(share.base.fields+1)));
  for (rec=recinfo,end=recinfo+share.base.fields; rec != end ; rec++)
  {
    if (rec->base.type == (int) FIELD_BLOB)
      rec->base.length+=sizeof(char*);
    else if (unpack && !(share.base.options & HA_OPTION_PACK_RECORD))
      rec->base.type=(int) FIELD_NORMAL;
  }

  if (share.base.options & HA_OPTION_COMPRESS_RECORD)
    share.base.records=max_records=share.state.records;
  else if (share.base.min_pack_length)
    max_records=(ulong) (my_seek(info.dfile,0L,MY_SEEK_END,MYF(0)) /
			 (ulong) share.base.min_pack_length);
  else
    max_records=0;
  unpack= (share.base.options & HA_OPTION_COMPRESS_RECORD) &&
    (testflag & T_UNPACK);
  share.base.options&= ~HA_OPTION_TEMP_COMPRESS_RECORD;
  VOID(nisam_close(*org_info));
  if (nisam_create(fn_format(name,filename,"",N_NAME_IEXT,
			  4+ (opt_follow_links ? 16 : 0)),
		   share.base.keys,keyinfo,recinfo,
		   max(max_records,share.base.records),share.base.reloc,
		   HA_DONT_TOUCH_DATA,
		   share.base.options |
		   (unpack ? HA_OPTION_TEMP_COMPRESS_RECORD
		    : 0),
		   (ulong) my_seek(info.dfile,0L,MY_SEEK_END,MYF(0))))
  {
    print_error("Got error %d when trying to recreate indexfile",my_errno);
    goto end;
  }
  *org_info=nisam_open(name,O_RDWR,
		(testflag & T_WAIT_FOREVER) ? HA_OPEN_WAIT_IF_LOCKED :
		(testflag & T_DESCRIPT) ? HA_OPEN_IGNORE_IF_LOCKED :
		HA_OPEN_ABORT_IF_LOCKED);
  if (!*org_info)
  {
    print_error("Got error %d when trying to open re-created indexfile",
		my_errno);
    goto end;
  }
  /* We are modifing */
  (*org_info)->s->base.options&= ~HA_OPTION_READ_ONLY_DATA;
  VOID(_nisam_readinfo(*org_info,F_WRLCK,0));
  (*org_info)->s->state.records=share.state.records;
  if (share.base.create_time)
    (*org_info)->s->base.create_time=share.base.create_time;
  (*org_info)->s->state.uniq=(*org_info)->this_uniq=
    share.state.uniq;
  (*org_info)->s->state.del=share.state.del;
  (*org_info)->s->state.dellink=share.state.dellink;
  (*org_info)->s->state.empty=share.state.empty;
  (*org_info)->s->state.data_file_length=share.state.data_file_length;
  if (update_state_info(*org_info,UPDATE_TIME | UPDATE_STAT))
    goto end;
  error=0;
end:
  my_afree((gptr) keyinfo);
  my_afree((gptr) recinfo);
  return error;
}

	/* Store long in 1,2,3 or 4 bytes */

static void save_integer( byte *pos, uint pack_length, ulong value)
{
  switch (pack_length) {
  case 4: int4store(pos,value); break;
  case 3: int3store(pos,value); break;
  case 2: int2store(pos,(uint) value); break;
  case 1: pos[0]= (char) (uchar) value; break;
  default: break;
  }
  return;
}

	/* write suffix to data file if neaded */

static int write_data_suffix( N_INFO *info)
{
  if (info->s->base.options & HA_OPTION_COMPRESS_RECORD &&
      sort_info.fix_datafile)
  {
    char buff[MEMMAP_EXTRA_MARGIN];
    bzero(buff,sizeof(buff));
    if (my_b_write(&info->rec_cache,buff,sizeof(buff)))
    {
      print_error("%d when writing to datafile",my_errno);
      return 1;
    }
    read_cache.end_of_file+=sizeof(buff);
  }
  return 0;
}


	/* Update state and isamchk_time of indexfile */

static int update_state_info( N_INFO *info, uint update)
{
  ISAM_SHARE *share=info->s;
  uint base_pos=uint2korr(info->s->state.header.base_pos);

  if (update & (UPDATE_STAT | UPDATE_SORT | UPDATE_TIME))
  {
    if (offsetof(N_BASE_INFO,rec_per_key) >
	uint2korr(share->state.header.base_info_length))
    {
      VOID(fputs("Internal error: Trying to change base of old table\n",
		 stderr));
    }
    else
    {
      if (update & UPDATE_TIME)
      {
	share->base.isamchk_time= (long) time((time_t*) 0);
	if (!share->base.create_time)
	  share->base.create_time=share->base.isamchk_time;
	if (my_pwrite(share->kfile,(gptr) &share->base.create_time,
		      sizeof(long)*2,
		      base_pos+offsetof(N_BASE_INFO,create_time),
		      MYF(MY_NABP)))
	  goto err;
      }
      if (update & (UPDATE_STAT | UPDATE_SORT))
      {
	if (my_pwrite(share->kfile,(gptr) share->base.rec_per_key,
		      sizeof(long)*share->state.keys+sizeof(uint),
		      base_pos+offsetof(N_BASE_INFO,rec_per_key),
		      MYF(MY_NABP)))
	  goto err;
      }
    }
  }
  {						/* Force update of status */
    int error;
    uint r_locks=share->r_locks,w_locks=share->w_locks;
    share->r_locks=share->w_locks=0;
    error=_nisam_writeinfo(info,2);
    share->r_locks=r_locks; share->w_locks=w_locks;
    if (!error)
      return 0;
  }
err:
  print_error("%d when updateing keyfile",my_errno);
  return 1;
}
