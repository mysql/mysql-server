/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */


/*
 *
 * fsck.mysql
 */

#include "libmysqlfs.h"
#include "mysqlcorbafs.h"
#include <getopt.h>
#define MAXPATHLEN 256

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <my_sys.h>
static long inodeNum;

extern DYNAMIC_ARRAY functions_array;
enum options {OPT_FTB=256, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC, OPT_KEYWORDS,
       OPT_LOCKS, OPT_DROP, OPT_OPTIMIZE, OPT_DELAYED, OPT_TABLES,
       OPT_CHARSETS_DIR, OPT_DEFAULT_CHARSET};

CHANGEABLE_VAR changeable_vars[] = {
  { "max_allowed_packet", (long*) &max_allowed_packet,24*1024*1024,4096,
  24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { "net_buffer_length", (long*) &net_buffer_length,1024*1024L-1025,4096,
  24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { 0, 0, 0, 0, 0, 0, 0}
};

CORBA_ORB               orb;
PortableServer_POA      poa;
CORBA_Environment       *ev;
PortableServer_ObjectId *objid;
static my_bool  verbose=0,opt_compress=0,extended_insert=0, lock_tables=0, 
    opt_quoted=0, opt_lock=0, opt_delayed=0, ignore_errors=0;

gptr fptr;
    
static const char *load_default_groups[]= { "mysqlcorbafs","client",0 };
static char *default_charset, *current_host, *current_user, *opt_password,
    *path,*fields_terminated=0, *lines_terminated=0, *enclosed=0, 
    *opt_enclosed=0, *escaped=0;

static struct option long_options[] =
{
  {"add-locks",    no_argument,    0,OPT_LOCKS},
  {"character-sets-dir",required_argument,0,    OPT_CHARSETS_DIR},
  {"compress",          no_argument,    0, 'C'},
  {"database",required_argument, 0, 'D'},
  {"debug",optional_argument, 0, '#'},
  {"default-character-set", required_argument,  0, OPT_DEFAULT_CHARSET},
  {"delayed-insert",no_argument,    0, OPT_DELAYED},
  {"fields-terminated-by", required_argument,   0, (int) OPT_FTB},
  {"fields-enclosed-by", required_argument,0, (int) OPT_ENC},
  {"fields-optionally-enclosed-by", required_argument, 0, (int) OPT_O_ENC},
  {"fields-escaped-by", required_argument,0, (int) OPT_ESC},
  {"functions",required_argument, 0, 'f'},
  {"help",   no_argument,    0,'?'},
  {"host",    required_argument,0, 'h'},
  {"lines-terminated-by", required_argument,    0, (int) OPT_LTB},
  {"lock-tables", no_argument,    0, 'l'},
  {"no-data",  no_argument,    0, 'd'},
  {"password",  optional_argument, 0, 'p'},
#ifdef __WIN__
  {"pipe",no_argument,0, 'W'},
#endif
  {"port",    required_argument,0, 'P'},
//  {"quick",    no_argument,0, 'q'},
  {"quote-names",no_argument,0, 'Q'},
  {"set-variable",required_argument,0, 'O'},
  {"socket",   required_argument,0, 'S'},
#include "sslopt-longopts.h"
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",    required_argument,0, 'u'},
#endif
  {"verbose", no_argument,0, 'v'},
  {"version", no_argument,0, 'V'},
  {0, 0, 0, 0}
};


/*
void
print_table_data(MYSQL_RES *result)
{
  String separator(256);
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  bool		*num_flag;

  num_flag=(bool*) my_alloca(sizeof(bool)*mysql_num_fields(result));
  if (info_flag)
  {
    print_field_types(result);
    mysql_field_seek(result,0);
  }
  separator.copy("+",1);
  while ((field = mysql_fetch_field(result)))
  {
    uint length=skip_column_names ? 0 : (uint) strlen(field->name);
    if (quick)
      length=max(length,field->length);
    else
      length=max(length,field->max_length);
    if (length < 4 && !IS_NOT_NULL(field->flags))
      length=4;					// Room for "NULL"
    field->max_length=length+1;
    separator.fill(separator.length()+length+2,'-');
    separator.append('+');
  }
  tee_puts(separator.c_ptr(), PAGER);
  if (!skip_column_names)
  {
    mysql_field_seek(result,0);
    (void) tee_fputs("|", PAGER);
    for (uint off=0; (field = mysql_fetch_field(result)) ; off++)
    {
      tee_fprintf(PAGER, " %-*s|",min(field->max_length,MAX_COLUMN_LENGTH),
		  field->name);
      num_flag[off]= IS_NUM(field->type);
    }
    (void) tee_fputs("\n", PAGER);
    tee_puts(separator.c_ptr(), PAGER);
  }

  while ((cur = mysql_fetch_row(result)))
  {
    (void) tee_fputs("|", PAGER);
    mysql_field_seek(result,0);
    for (uint off=0 ; off < mysql_num_fields(result); off++)
    {
      const char *str=cur[off] ? cur[off] : "NULL";
      field = mysql_fetch_field(result);
      uint length=field->max_length;
      if (length > MAX_COLUMN_LENGTH)
      {
	tee_fputs(str,PAGER); tee_fputs(" |",PAGER);
      }
      else
      tee_fprintf(PAGER, num_flag[off] ? "%*s |" : " %-*s|",
		  length, str);
    }
    (void) tee_fputs("\n", PAGER);
  }
  tee_puts(separator.c_ptr(), PAGER);
  my_afree((gptr) num_flag);
}

void
print_table_data_html(MYSQL_RES *result)
{
  MYSQL_ROW   cur;
  MYSQL_FIELD *field;

  mysql_field_seek(result,0);
  (void) tee_fputs("<TABLE BORDER=1><TR>", PAGER);
  if (!skip_column_names)
  {
    while((field = mysql_fetch_field(result)))
    {
      tee_fprintf(PAGER, "<TH>%s</TH>", (field->name ? 
					 (field->name[0] ? field->name : 
					  " &nbsp; ") : "NULL"));
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    (void) tee_fputs("<TR>", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      ulong *lengths=mysql_fetch_lengths(result);
      (void) tee_fputs("<TD>", PAGER);
      safe_put_field(cur[i],lengths[i]);
      (void) tee_fputs("</TD>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  (void) tee_fputs("</TABLE>", PAGER);
}


void
print_table_data_xml(MYSQL_RES *result)
{
  MYSQL_ROW   cur;
  MYSQL_FIELD *fields;

  mysql_field_seek(result,0);

  char *statement;
  statement=(char*) my_malloc(strlen(glob_buffer.ptr())*5+1, MYF(MY_WME));
  xmlencode(statement, (char*) glob_buffer.ptr());

  (void) my_chomp(strend(statement));

  tee_fprintf(PAGER,"<?xml version=\"1.0\"?>\n\n<resultset statement=\"%s\">", statement);

  my_free(statement,MYF(MY_ALLOW_ZERO_PTR));

  fields = mysql_fetch_fields(result);

  while ((cur = mysql_fetch_row(result)))
  {
    (void) tee_fputs("\n  <row>\n", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      char *data;
      ulong *lengths=mysql_fetch_lengths(result);
      data=(char*) my_malloc(lengths[i]*5+1, MYF(MY_WME));
      tee_fprintf(PAGER, "\t<%s>", (fields[i].name ?
				  (fields[i].name[0] ? fields[i].name :
				   " &nbsp; ") : "NULL"));
      xmlencode(data, cur[i]);
      safe_put_field(data, strlen(data));
      tee_fprintf(PAGER, "</%s>\n", (fields[i].name ?
				     (fields[i].name[0] ? fields[i].name :
				      " &nbsp; ") : "NULL"));
      my_free(data,MYF(MY_ALLOW_ZERO_PTR));
    }
    (void) tee_fputs("  </row>\n", PAGER);
  }
  (void) tee_fputs("</resultset>\n", PAGER);
}


void
print_table_data_vertically(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  uint		max_length=0;
  MYSQL_FIELD	*field;

  while ((field = mysql_fetch_field(result)))
  {
    uint length=(uint) strlen(field->name);
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  mysql_field_seek(result,0);
  for (uint row_count=1; (cur= mysql_fetch_row(result)); row_count++)
  {
    mysql_field_seek(result,0);
    tee_fprintf(PAGER, 
		"*************************** %d. row ***************************\n", row_count);
    for (uint off=0; off < mysql_num_fields(result); off++)
    {
      field= mysql_fetch_field(result);
      tee_fprintf(PAGER, "%*s: ",(int) max_length,field->name);
      tee_fprintf(PAGER, "%s\n",cur[off] ? (char*) cur[off] : "NULL");
    }
  }
}



*/



static my_bool test_if_special_chars(const char *str)
{
  for ( ; *str ; str++)
    if (!isvar(*str) && *str != '$')
      return 1;
            return 0;
} /* test_if_special_chars */

char *quote_name(char *name, char *buff)
{
  char *end;
  DBUG_ENTER("quote_name");
  if (!opt_quoted && !test_if_special_chars(name))
         return name;
  buff[0]=QUOTE_CHAR;
  *end=strmov(buff+1,name);
  end[0]=QUOTE_CHAR;
  end[1]=0;
  DBUG_RETURN(buff);
} /* quote_name */

/*
 * Allow the user to specify field terminator strings like:
 * "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
 * This is done by doubleing ' and add a end -\ if needed to avoid
 * syntax errors from the SQL parser.
 */

char *field_escape(char *to,const char *from,uint length)
{
  const char *end;
  uint end_backslashes=0;
  DBUG_ENTER("field_escape");
 
  {
    *to++= *from;
    if (*from == '\\')
      end_backslashes^=1;    /* find odd number of backslashes */
    else {
      if (*from == '\'' && !end_backslashes)
        *to++= *from;      /* We want a duplicate of "'" for MySQL */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
      *to++= '\\';
  DBUG_RETURN(to);
} /* field_escape */

void safe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  if (sock)
    mysql_close(sock);
  exit(error);
}
/* safe_exit */


/*
 * ** DBerror -- prints mysql error message and exits the program.
 */
void DBerror(MYSQL *mysql, const char *when)
{
  DBUG_ENTER("DBerror");
  my_printf_error(0,"Got error: %d: %s %s", MYF(0),
      mysql_errno(mysql), mysql_error(mysql), when);
  safe_exit(EX_MYSQLERR);
  DBUG_VOID_RETURN;
} /* DBerror */

void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,CORBAFS_VERSION,
      MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
} /* print_version */

void usage(void)
{
  uint i;
  print_version();
  puts("By Tõnu Samuel. Some code is partially from other geeks around the world");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Dumping definition and data mysql database or table");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  printf("\n\
  -#, --debug=...       Output debug log. Often this is 'd:t:o,filename`.\n\
  --character-sets-dir=...\n\
                        Directory where character sets are\n\
  -?, --help		Display this help message and exit.\n\
  -c, --complete-insert Use complete insert statements.\n\
  -C, --compress        Use compression in server/client protocol.\n\
  --default-character-set=...\n\
                        Set the default character set\n\
  -e, --extended-insert Allows utilization of the new, much faster\n\
                        INSERT syntax.\n\
  --add-locks		Add locks around insert statements.\n\
  --allow-keywords	Allow creation of column names that are keywords.\n\
  --delayed-insert      Insert rows with INSERT DELAYED.\n\
  -f, --force		Continue even if we get an sql-error.\n\
  -h, --host=...	Connect to host.\n");
puts("\
  -l, --lock-tables     Lock all tables for read.\n\
  -t, --no-create-info	Don't write table creation info.\n\
  -d, --no-data		No row information.\n\
  -O, --set-variable var=option\n\
                        give a variable a value. --help lists variables\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
                        If password is not given it's solicited on the tty.\n");
#ifdef __WIN__
  puts("-W, --pipe		Use named pipes to connect to server");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -q, --quick		Don't buffer query, dump directly to stdout.\n\
  -S, --socket=...	Socket file to use for connection.\n\
  --tables              Overrides option --databases (-B).\n");
#include "sslopt-usage.h"
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -v, --verbose		Print info about the various stages.\n\
  -V, --version		Output version information and exit.\n\
");
  print_defaults("my",load_default_groups);

  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (i=0 ; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %lu\n",
     changeable_vars[i].name,
     (ulong) *changeable_vars[i].varptr);
} /* usage */



static int get_options(int *argc,char ***argv)
{
  int c,option_index;
  my_bool tty_password=0;
  DBUG_ENTER("get_options");
  load_defaults("my",load_default_groups,argc,argv);
  set_all_changeable_vars(changeable_vars);
  while ((c=getopt_long(*argc,*argv,"#::p::h:u:O:P:S:T:EBaAcCdefFlnqtvVw:?Ix",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'e':
      extended_insert=1;
      break;
    case OPT_DEFAULT_CHARSET:
      default_charset= optarg;
      break;
    case OPT_CHARSETS_DIR:
      charsets_dir= optarg;
      break;
      
      ignore_errors=1;
      break;
    case 'h':
      my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
      current_host=my_strdup(optarg,MYF(MY_WME));
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user=optarg;
      break;
#endif
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
        usage();
        return(1);
      }
      break;
    case 'p':
      if (optarg)
      {
        char *start=optarg;
        my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
        opt_password=my_strdup(optarg,MYF(MY_FAE));
        while (*optarg) *optarg++= 'x';  /* Destroy argument */
        if (*start)
          start[1]=0;  /* Cut length of argument */
      } else
        tty_password=1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      opt_mysql_unix_port= optarg;
      break;
    case 'W':
#ifdef __WIN__
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
    case 'T':
      path= optarg;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case 'C':
      opt_compress=1;
      break;
    case 'l': lock_tables=1; break;
    case 'Q': opt_quoted=1; break;
    case 'v': verbose=1; break;
    case 'V': print_version(); exit(0);
    default:
      fprintf(stderr,"%s: Illegal option character '%c'\n",my_progname,opterr);
      /* Fall throught */
    case 'I':
    case '?':
      usage();
      exit(0);
    case (int) OPT_FTB:
      fields_terminated= optarg;
      break;
    case (int) OPT_LTB:
      lines_terminated= optarg;
      break;
    case (int) OPT_ENC:
      enclosed= optarg;
      break;
    case (int) OPT_O_ENC:
      opt_enclosed= optarg;
      break;
    case (int) OPT_ESC:
      escaped= optarg;
      break;
    case (int) OPT_LOCKS:
      opt_lock=1;
      break;
    case (int) OPT_OPTIMIZE:
      extended_insert=opt_lock=lock_tables=1;
      break;
    case (int) OPT_DELAYED:
      opt_delayed=1;
      break;
#include "sslopt-case.h"
    }
  }
  if (opt_delayed)
    opt_lock=0;				/* Can't have lock with delayed */
  if (!path && (enclosed || opt_enclosed || escaped || lines_terminated ||
		fields_terminated))
  {
    fprintf(stderr, "%s: You must use option --tab with --fields-...\n", my_progname);
    return(1);
  }

  if (enclosed && opt_enclosed)
  {
    fprintf(stderr, "%s: You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n", my_progname);
    return(1);
  }
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (tty_password)
    opt_password=get_tty_password(NullS);
  DBUG_RETURN(0);
} /* get_options */


/*** epv structures ***/

static PortableServer_ServantBase__epv impl_Inode_base_epv = {
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_CorbaFS_Inode__epv impl_Inode_epv = {
   NULL,			/* _private */
   (gpointer) & impl_Inode_getStatus,
   (gpointer) & impl_Inode_readpage,
   (gpointer) & impl_Inode_release,

};
static PortableServer_ServantBase__epv impl_FileSystem_base_epv = {
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_CorbaFS_FileSystem__epv impl_FileSystem_epv = {
   NULL,			/* _private */
   (gpointer) & impl_FileSystem_getInode,
   (gpointer) & impl_FileSystem_readdir,
   (gpointer) & impl_FileSystem_readlink,
};

/*** vepv structures ***/

static POA_CorbaFS_Inode__vepv impl_Inode_vepv = {
   &impl_Inode_base_epv,
   &impl_Inode_epv,
};
static POA_CorbaFS_FileSystem__vepv impl_FileSystem_vepv = {
   &impl_FileSystem_base_epv,
   &impl_FileSystem_epv,
};

/*** Stub implementations ***/

static CorbaFS_Inode
impl_Inode__create(PortableServer_POA poa, CORBA_Environment * ev)
{
   CorbaFS_Inode retval;
   impl_POA_CorbaFS_Inode *newservant;
   PortableServer_ObjectId *objid;

   DBUG_ENTER("impl_Inode__create");
   newservant = g_new0(impl_POA_CorbaFS_Inode, 1);
   newservant->servant.vepv = &impl_Inode_vepv;
   newservant->poa = poa;
   POA_CorbaFS_Inode__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   DBUG_RETURN(retval);
}

static void
impl_Inode__destroy(impl_POA_CorbaFS_Inode * servant,
   CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;

   DBUG_ENTER("impl_Inode__destroy");
   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_CorbaFS_Inode__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
   DBUG_VOID_RETURN;
}

static void
impl_Inode_getStatus(impl_POA_CorbaFS_Inode * servant,
   CORBA_unsigned_short * mode,
   CORBA_unsigned_long * uid,
   CORBA_unsigned_long * gid,
   CORBA_unsigned_long * size,
   CORBA_unsigned_long * inodeNum,
   CORBA_unsigned_short * numLinks,
   CORBA_long * atime,
   CORBA_long * mtime,
   CORBA_long * ctime, CORBA_Environment * ev)
{
   struct stat buf;
   char
      server[BUFLEN],
      database[BUFLEN],
      table[BUFLEN],
      key[BUFLEN],
      field[BUFLEN],
      value[BUFLEN];
            
   struct func_st *func;

   DBUG_ENTER("impl_Inode_getStatus");
   DBUG_PRINT("enter",("path: '%s', mode: '%o', uid: '%d', gid: '%d', size: '%d', 
               inodeNum: '%d', numLinks: '%d', atime: '%d',mtime: '%d', ctime: '%d'", 
               servant->path, mode, uid, gid, size, inodeNum, numLinks, atime, mtime, ctime));
   DBUG_PRINT("info",("func: %x",&func));
   if(parse(servant->path, server, database, table, field, value, &func)>0)
   {
      DBUG_PRINT("info",("ENOENT"));
      *mode=0;
   } else if (func != NULL){
      DBUG_PRINT("info",("func: %x",&func));
      DBUG_PRINT("info",("Argument is function at %x, returning S_IFREG",func));
      *mode = S_IFREG; // File
   } else if (*field){
      DBUG_PRINT("info",("Argument is file, returning S_IFREG"));
      *mode = S_IFREG; // File
   } else {
      DBUG_PRINT("info",("Argument is directory, returning S_IFDIR"));
      *mode = S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH ; // Dir
   }
     
   *mode |= S_IRUSR | S_IRGRP | S_IROTH; 
   *uid = 0;
   *gid = 0;
   *size = 4096;
   *inodeNum = servant->inodeNum;
   *numLinks = 1;
   *atime = 3;
   *mtime = 2;
   *ctime = 1;
       
//   lstat(servant->path, &buf);
//   *mode = buf.st_mode;
/*   *uid = buf.st_uid;
   *gid = buf.st_gid;
   *size = buf.st_size;
   *inodeNum = buf.st_ino;
   *numLinks = buf.st_nlink;
   *atime = buf.st_atime;
   *mtime = buf.st_mtime;
   *ctime = buf.st_ctime;*/
   DBUG_VOID_RETURN;
}

static void
impl_Inode_readpage(impl_POA_CorbaFS_Inode * servant,
   CorbaFS_Buffer ** buffer,
   CORBA_long size,
   CORBA_long offset, CORBA_Environment * ev)
{
   int type;
   int fd = -1, c = 0;
   int res;
   char
      server[BUFLEN],
      database[BUFLEN],
      table[BUFLEN],
      key[BUFLEN],
      field[BUFLEN],
      value[BUFLEN];
   struct func_st *func;

   DBUG_ENTER("impl_Inode_readpage");
   DBUG_PRINT("enter",("path: '%s'", servant->path));
   *buffer = CorbaFS_Buffer__alloc();
   (*buffer)->_maximum = size;
   (*buffer)->_buffer = CORBA_octet_allocbuf(size);
   printf("requested to read %d bytes\n",size);
   memset((*buffer)->_buffer, size, 0);
   type = parse(servant->path, server, database, table, field, value, &func);
   if (func != NULL) 
      res=db_function((*buffer)->_buffer, server, database, table, field, value, servant->path, func);
   else
      res=db_show_field((*buffer)->_buffer, database, table, field, path, value);
   if(res>0)
      (*buffer)->_length = strlen((*buffer)->_buffer);
   else 
      (*buffer)->_length = 0;
/*
        fd = open(servant->path, O_RDONLY);
        printf("Inode_readpage : fd = %d\n", fd);
        lseek(fd, offset, SEEK_SET);
        c = read(fd, (*buffer)->_buffer, size);
        printf("Inode_readpage : read %d bytes\n", c);
        (*buffer)->_length = c;
        close(fd);
*/
   DBUG_VOID_RETURN;
}

static void
impl_Inode_release(impl_POA_CorbaFS_Inode * servant,
			   CORBA_Environment * ev)
{
   DBUG_ENTER("impl_Inode_readpage");
   DBUG_PRINT("enter",("path: '%s'", servant->path));
   DBUG_VOID_RETURN;
}

/* 
 * This function is called when we get mounted
 */
CorbaFS_FileSystem 
impl_FileSystem__create(PortableServer_POA poa,
   CORBA_Environment * ev)
{
   CorbaFS_FileSystem retval;
   impl_POA_CorbaFS_FileSystem *newservant;
   PortableServer_ObjectId *objid;

   DBUG_ENTER("impl_FileSystem__create");
   newservant = g_new0(impl_POA_CorbaFS_FileSystem, 1);
   newservant->servant.vepv = &impl_FileSystem_vepv;
   newservant->poa = poa;
   POA_CorbaFS_FileSystem__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   DBUG_RETURN(retval);
}

/* 
 * This function is called when we get unmounted
 */
static void
impl_FileSystem__destroy(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;
   DBUG_ENTER("impl_FileSystem__destroy");

   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_CorbaFS_FileSystem__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
   DBUG_VOID_RETURN;
}

static CorbaFS_Inode
impl_FileSystem_getInode(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_char * path, CORBA_Environment * ev)
{
   CorbaFS_Inode retval;
   impl_POA_CorbaFS_Inode *inode;
   char
      database[BUFLEN],
      table[BUFLEN],
      key[BUFLEN],
      field[BUFLEN];
   char buffer[MAXDIRS][BUFLEN];
   int c;

   DBUG_ENTER("impl_FileSystem_getInode");
   DBUG_PRINT("enter",("path: '%s'", path));

   //FIXME: We should verify the existense of file/dir here
   //
   retval = impl_Inode__create(servant->poa, ev);
   inode = PortableServer_POA_reference_to_servant( servant->poa, retval, ev );
   inode->path = CORBA_string_dup(path);
   //FIXME: inodeNum Generation goes here
   //
   inode->inodeNum= inodeNum++;
#if 0
   inode->mode = 0040777; /* world-readable directory */
   inode->uid = 0;
   inode->gid = 0;
   inode->size = 4096;
   inode->inodeNum = inodeNum++;
   inode->numLinks = 1;
   inode->atime = 0;
   inode->mtime = 100;
   inode->ctime = 10000;
#endif
   DBUG_RETURN(retval);
}


static CorbaFS_DirEntSeq *
impl_FileSystem_readdir(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_char * path, CORBA_Environment * ev)
{
   CorbaFS_DirEntSeq *retval;
   CorbaFS_dirent *dirent;

   struct func_st *func;
   int c, c2,i;
   char
      server[BUFLEN],
      table[BUFLEN],
      field[BUFLEN],
      value[BUFLEN],
      buffer[MAXDIRS][BUFLEN],
      buffer2[MAXDIRS][BUFLEN],
      database[BUFLEN];

   DBUG_ENTER("impl_FileSystem_readdir");
   DBUG_PRINT("enter",("path: '%s'", path));
   retval = CorbaFS_DirEntSeq__alloc();
   retval->_maximum = 0;
   retval->_length = 0;

   parse(path, server, database, table, field, value, &func);
   if (func != NULL) {
      c2 = db_function((char *)buffer, server, database, table, field, value, path, func);
   } else if(!*server) {
      c2 = db_show_servers(buffer2,MAXDIRS);
      c = show_functions((char *)buffer, ROOT_FUNCTION);
   } else if(!*database) {
      c2 = db_show_databases(buffer2,MAXDIRS);
      c = show_functions((char *)buffer, SERVER_FUNCTION);
   } else if(!*table){
      c2 = db_show_tables(buffer2, database);
      c = show_functions((char *)buffer, DATABASE_FUNCTION);
   } else if(!*field){
      c2 = db_show_primary_keys(buffer2, database,table);
      if(c2>=0) {
         c = show_functions((char *)buffer, TABLE_FUNCTION);
      }
   } else {
      c2 = db_show_fields(buffer2, database, table, field);
      c = show_functions((char *)buffer, FIELD_FUNCTION);
      c = show_functions((char *)buffer, KEY_FUNCTION);
   }
   if(c2 < 0)
      c=c2=0; // Error occured in database routines

   /* Allocate space to hold all found entries plus "." and ".." */
   retval->_maximum = c + c2 + 2;
   retval->_buffer = CORBA_sequence_CorbaFS_dirent_allocbuf(retval->_maximum) ;
   dirent = retval->_buffer;

   i = 0;
   while (i < c) {
      long inode = 123L;
      dirent[i].inode = inode;
      dirent[i].name = CORBA_string_dup(buffer[i]);
      i++;
   }
   i = 0;
   while (i < c2) {
      long inode = 123L;
      dirent[c+i].inode = inode;
      dirent[c+i].name = CORBA_string_dup(buffer2[i]);
      i++;
   }
   dirent[c+i].inode = 123L;
   dirent[c+i].name = CORBA_string_dup(".");
   i++;
   dirent[c+i].inode = 123L;
   dirent[c+i].name = CORBA_string_dup("..");

   retval->_length = retval->_maximum;
   DBUG_RETURN(retval);
}

static CORBA_char *
impl_FileSystem_readlink(impl_POA_CorbaFS_FileSystem * servant,
				 CORBA_char * filename,
				 CORBA_Environment * ev)
{
   CORBA_char *retval = CORBA_OBJECT_NIL;
   char tmp[MAXPATHLEN + 1];
   int len;
   
   DBUG_ENTER("impl_FileSystem_readlink");
   DBUG_PRINT("enter",("path: '%s'", filename));

/*   len = readlink(filename, tmp, MAXPATHLEN);
   if (len != -1)
   {
           tmp[len] = '\0';
           retval = CORBA_string_dup(tmp);
   }

   printf("%s\n", retval);
  */ 
   DBUG_RETURN(retval);
}

int fix_filenames(char *buf)
{
   int i;
   for(i=0; i<strlen(buf);i++)
      if(buf[i]=='/')
         buf[i]='_';
}

int main(int argc, char *argv[]) {
  CorbaFS_FileSystem          fs;
  impl_POA_CorbaFS_FileSystem *fs_impl;
  FILE *f;
  PortableServer_POAManager pm;

  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);
  ev = g_new0(CORBA_Environment,1);
  CORBA_exception_init(ev);
  orb = CORBA_ORB_init(&argc, argv, "orbit-local-orb", ev);
  MY_INIT(argv[0]);

  /*
  ** Check out the args
  */
  if (get_options(&argc, &argv))
  {
    my_end(0);
    exit(EX_USAGE);
  }
  if (db_connect(current_host, current_user, opt_password))
    exit(EX_MYSQLERR);
  fptr = db_load_functions();
  db_load_formats();
  poa = (PortableServer_POA)
        CORBA_ORB_resolve_initial_references(orb, "RootPOA", ev);
  fs = impl_FileSystem__create(poa, ev);

  pm = PortableServer_POA__get_the_POAManager(poa, ev);
  PortableServer_POAManager_activate(pm, ev);

  fs_impl = PortableServer_POA_reference_to_servant( poa, fs, ev );
  objid = PortableServer_POA_servant_to_id( poa, fs_impl, ev );
  printf("CorbaFS-server:\n%s\n", CORBA_ORB_object_to_string(orb, fs, ev));
  f=fopen("/tmp/mysqlcorbafs.ior","w");
  fputs(CORBA_ORB_object_to_string(orb, fs, ev),f);
  fclose(f);
  CORBA_ORB_run(orb, ev);
  db_disconnect(current_host);

  return 0;
}

