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


#define MYSQL_CLIENT
#undef MYSQL_SERVER
#include <global.h>
#include <m_string.h>
#include <my_sys.h>
#include <getopt.h>
#include <thr_alarm.h>
#include <mysql.h>
#include "log_event.h"

#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES)

extern "C"
{
 int simple_command(MYSQL *mysql,enum enum_server_command command,
			      const char *arg,
		  uint length, my_bool skipp_check);
 int net_safe_read(MYSQL* mysql);
}

char server_version[SERVER_VERSION_LENGTH];
uint32 server_id = 0;

// needed by net_serv.c
ulong bytes_sent = 0L, bytes_received = 0L;
ulong mysqld_net_retry_count = 10L;
ulong net_read_timeout=  NET_READ_TIMEOUT;
ulong net_write_timeout= NET_WRITE_TIMEOUT;
uint test_flags = 0; 
FILE *result_file;

#ifndef DBUG_OFF
static const char* default_dbug_option = "d:t:o,/tmp/mysqlbinlog.trace";
#endif

static struct option long_options[] =
{
#ifndef DBUG_OFF
  {"debug", 	  optional_argument, 	0, '#'},
#endif
  {"help", 	  no_argument, 		0, '?'},
  {"host", 	  required_argument,	0, 'h'},
  {"offset", 	  required_argument,	0, 'o'},
  {"password",	  required_argument,	0, 'p'},
  {"port", 	  required_argument,	0, 'P'},
  {"position",	  required_argument,	0, 'j'},
  {"result-file", required_argument,    0, 'r'},
  {"short-form",  no_argument,		0, 's'},
  {"table", 	  required_argument, 	0, 't'},
  {"user",	  required_argument,	0, 'u'},
  {"version",	  no_argument, 		0, 'V'},
};

void sql_print_error(const char *format,...);

static bool short_form = 0;
static ulonglong offset = 0;
static const char* host = "localhost";
static int port = MYSQL_PORT;
static const char* user = "test";
static const char* pass = "";
static ulonglong position = 0;
static bool use_remote = 0;
static short binlog_flags = 0; 
static MYSQL* mysql = NULL;
static const char* table = 0;

static void dump_local_log_entries(const char* logname);
static void dump_remote_log_entries(const char* logname);
static void dump_log_entries(const char* logname);
static void dump_remote_file(NET* net, const char* fname);
static void dump_remote_table(NET* net, const char* db, const char* table);
static void die(const char* fmt, ...);
static MYSQL* safe_connect();


void sql_print_error(const char *format,...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

static void print_version()
{
  printf("%s  Ver 1.5 for %s at %s\n",my_progname,SYSTEM_TYPE, MACHINE_TYPE);
}


static void usage()
{
  print_version();
  puts("By Sasha, for your professional use\n\
This software comes with NO WARRANTY: see the file PUBLIC for details\n");

  printf("\
Dumps a MySQL binary log in a format usable for viewing or for pipeing to\n\
the mysql command line client\n\n");
  printf("Usage: %s [options] log-files\n",my_progname);
  puts("Options:");
#ifndef DBUG_OFF
  printf("-#, --debug[=...]       Output debug log.  (%s)\n",
	 default_dbug_option);
#endif
  printf("\
-?, --help		Display this help and exit\n\
-s, --short-form	Just show the queries, no extra info\n\
-o, --offset=N		Skip the first N entries\n\
-h, --host=server	Get the binlog from server\n\
-P, --port=port         Use port to connect to the remote server\n\
-u, --user=username     Connect to the remove server as username\n\
-p, --password=password Password to connect to remote server\n\
-r, --result-file=file  Direct output to a given file\n\
-j, --position=N	Start reading the binlog at position N\n\
-t, --table=name        Get raw table dump using COM_TABLE_DUMB\n\
-V, --version		Print version and exit.\n\
");
}

static void dump_remote_file(NET* net, const char* fname)
{
  char buf[FN_REFLEN+1];
  uint len = (uint) strlen(fname);
  buf[0] = 0;
  memcpy(buf + 1, fname, len + 1);
  if(my_net_write(net, buf, len +2) || net_flush(net))
    die("Failed  requesting the remote dump of %s", fname);
  for(;;)
    {
      uint packet_len = my_net_read(net);
      if(packet_len == 0)
	{
	  if(my_net_write(net, "", 0) || net_flush(net))
	    die("Failed sending the ack packet");

	  // we just need to send something, as the server will read but
	  // not examine the packet - this is because mysql_load() sends an OK when it is done
	  break;
	}
      else if(packet_len == packet_error)
	die("Failed reading a packet during the dump of %s ", fname);

      if(!short_form)
	(void)my_fwrite(result_file, (byte*) net->read_pos, packet_len,MYF(0));
    }

  fflush(result_file);
}

static int parse_args(int *argc, char*** argv)
{
  int c, opt_index = 0;

  result_file = stdout;
  while((c = getopt_long(*argc, *argv, "so:#::h:j:u:p:P:r:t:?V", long_options,
			 &opt_index)) != EOF)
  {
    switch(c)
    {
#ifndef DBUG_OFF
    case '#':
      DBUG_PUSH(optarg ? optarg : default_dbug_option);
      break;
#endif
    case 's':
      short_form = 1;
      break;

    case 'o':
      offset = strtoull(optarg,(char**) 0, 10);
      break;

    case 'j':
      position = strtoull(optarg,(char**) 0, 10);
      break;

    case 'h':
      use_remote = 1;
      host = my_strdup(optarg, MYF(0));
      break;
      
    case 'P':
      use_remote = 1;
      port = atoi(optarg);
      break;
      
    case 'p':
      use_remote = 1;
      pass = my_strdup(optarg, MYF(0));
      break;

    case 'r':
      if (!(result_file = my_fopen(optarg, O_WRONLY | O_BINARY, MYF(MY_WME))))
	exit(1);
      break;

    case 'u':
      use_remote = 1;
      user = my_strdup(optarg, MYF(0));
      break;

    case 't':
      table = my_strdup(optarg, MYF(0));
      break;

    case 'V':
      print_version();
      exit(0);

    case '?':
    default:
      usage();
      exit(0);

    }
  }

  (*argc)-=optind;
  (*argv)+=optind;

  return 0;
}

static MYSQL* safe_connect()
{
  MYSQL *local_mysql = mysql_init(NULL);
  if(!local_mysql)
    die("Failed on mysql_init");

  if(!mysql_real_connect(local_mysql, host, user, pass, 0, port, 0, 0))
    die("failed on connect: %s", mysql_error(local_mysql));

  return local_mysql;
}

static void dump_log_entries(const char* logname)
{
  if(use_remote)
    dump_remote_log_entries(logname);
  else
    dump_local_log_entries(logname);  
}

static void dump_remote_table(NET* net, const char* db, const char* table)
{
  char buf[1024];
  char * p = buf;
  uint table_len = (uint) strlen(table);
  uint db_len = (uint) strlen(db);
  if(table_len + db_len > sizeof(buf) - 2)
    die("Buffer overrun");
  
  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);
  
  if(simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
    die("Error sending the table dump command");

  for(;;)
  {
    uint packet_len = my_net_read(net);
    if(packet_len == 0) break; // end of file
    if(packet_len == packet_error)
      die("Error reading packet in table dump");
    my_fwrite(result_file, (byte*)net->read_pos, packet_len, MYF(MY_WME));
    fflush(result_file);
  }
}


static void dump_remote_log_entries(const char* logname)
{
  char buf[128];
  char last_db[FN_REFLEN+1] = "";
  uint len;
  NET* net = &mysql->net;
  if(!position) position = 4; // protect the innocent from spam
  if (position < 4)
  {
    position = 4;
    // warn the guity
    sql_print_error("Warning: The position in the binary log can't be less than 4.\nStarting from position 4\n");
  }
  int4store(buf, position);
  int2store(buf + 4, binlog_flags);
  len = (uint) strlen(logname);
  int4store(buf + 6, 0);
  memcpy(buf + 10, logname,len);
  if(simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
    die("Error sending the log dump command");
  
  for(;;)
  {
    len = net_safe_read(mysql);
    if (len == packet_error)
      die("Error reading packet from server: %s", mysql_error(mysql));
    if(len == 1 && net->read_pos[0] == 254)
      break; // end of data
    DBUG_PRINT("info",( "len= %u, net->read_pos[5] = %d\n",
			len, net->read_pos[5]));
    Log_event * ev = Log_event::read_log_event(
					  (const char*) net->read_pos + 1 ,
					  len - 1);
    if(ev)
    {
      ev->print(result_file, short_form, last_db);
      if(ev->get_type_code() == LOAD_EVENT)
	dump_remote_file(net, ((Load_log_event*)ev)->fname);
      delete ev;
    }
    else
      die("Could not construct log event object");
  }
}

static void dump_local_log_entries(const char* logname)
{
  File fd = -1;
  IO_CACHE cache,*file= &cache;
  ulonglong rec_count = 0;
  char last_db[FN_REFLEN+1] = "";

  if (logname && logname[0] != '-')
  {
    if ((fd = my_open(logname, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0)
      exit(1);
    if (init_io_cache(file, fd, 0, READ_CACHE, (my_off_t) position, 0,
		      MYF(MY_WME | MY_NABP)))
      exit(1);
  }
  else
  {
    if (init_io_cache(file, fileno(result_file), 0, READ_CACHE, (my_off_t) 0,
		      0, MYF(MY_WME | MY_NABP | MY_DONT_CHECK_FILESIZE)))
      exit(1);
    if (position)
    {
      /* skip 'position' characters from stdout */
      byte buff[IO_SIZE];
      my_off_t length,tmp;
      for (length= (my_off_t) position ; length > 0 ; length-=tmp)
      {
	tmp=min(length,sizeof(buff));
	if (my_b_read(file,buff, (uint) tmp))
	  exit(1);
      }
    }
    file->pos_in_file=position;
    file->seek_not_done=0;
  }

  if (!position)
  {
    char magic[4];
    if (my_b_read(file, (byte*) magic, sizeof(magic)))
      die("I/O error reading binlog magic number");
    if(memcmp(magic, BINLOG_MAGIC, 4))
      die("Bad magic number;  The file is probably not a MySQL binary log");
  }
 
  for (;;)
  {
    char llbuff[21];
    my_off_t old_off = my_b_tell(file);

    Log_event* ev = Log_event::read_log_event(file);
    if (!ev)
    {
      if (file->error)
	die("\
Could not read entry at offset %s : Error in log format or read error",
	    llstr(old_off,llbuff));
      // file->error == 0 means EOF, that's OK, we break in this case
      break;
    }
    if (rec_count >= offset)
    {
      if (!short_form)
        fprintf(result_file, "# at %s\n",llstr(old_off,llbuff));

      ev->print(result_file, short_form, last_db);
    }
    rec_count++;
    delete ev;
  }
  if(fd >= 0)
   my_close(fd, MYF(MY_WME));
  end_io_cache(file);
}


int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(&argc, (char***)&argv);

  if(!argc && !table)
  {
    usage();
    return -1;
  }

  if(use_remote)
  {
    mysql = safe_connect();
  }

  if (table)
  {
    if(!use_remote)
      die("You must specify connection parameter to get table dump");
    char* db = (char*)table;
    char* tbl = (char*) strchr(table, '.');
    if(!tbl)
      die("You must use database.table syntax to specify the table");
    *tbl++ = 0;
    dump_remote_table(&mysql->net, db, tbl);
  }
  else
  {
    while(--argc >= 0)
    {
      dump_log_entries(*(argv++));
    }
  }
  if (result_file != stdout)
    my_fclose(result_file, MYF(0));
  if (use_remote)
    mysql_close(mysql);
  return 0;
}

/*
  We must include this here as it's compiled with different options for
  the server
*/

#include "log_event.cc"
