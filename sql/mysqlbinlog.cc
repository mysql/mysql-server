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
#include "log_event.h"
#define MYSQL_SERVER			// We want the C++ version of net
#include <mysql.h>
#include "mini_client.h"

#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES)

char server_version[50];
uint32 server_id = 0;

// needed by net_serv.c
ulong bytes_sent = 0L, bytes_received = 0L;
ulong mysqld_net_retry_count = 10L;
ulong net_read_timeout=  NET_READ_TIMEOUT;
ulong net_write_timeout= NET_WRITE_TIMEOUT;
uint test_flags = 0; 

#ifndef DBUG_OFF
static const char* default_dbug_option = "d:t:o,/tmp/mysqlbinlog.trace";
#endif

static struct option long_options[] =
{
  {"short-form", no_argument, 0, 's'},
  {"table", required_argument, 0, 't'},
  {"offset", required_argument,0, 'o'},
  {"help", no_argument, 0, '?'},
  {"host", required_argument,0, 'h'},
  {"port", required_argument,0, 'P'},
  {"user", required_argument,0, 'u'},
  {"password", required_argument,0, 'p'},
  {"position", required_argument,0, 'j'},
#ifndef DBUG_OFF
  {"debug", optional_argument, 0, '#'}
#endif
};

void sql_print_error(const char *format,...);

static bool short_form = 0;
static int offset = 0;
static const char* host = "localhost";
static int port = MYSQL_PORT;
static const char* user = "test";
static const char* pass = "";
static long position = 0;
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

static void usage()
{
  printf("Usage: %s [options] log-files\n",my_progname);
  printf("Options:\n\
-s,--short-form		just show the queries, no extra info\n\
-o,--offset=N		skip the first N entries\n\
-h,--host=server	get the binlog from server\n\
-P,--port=port          use port to connect to the remove server\n\
-u,--user=username      connect to the remove server as username\n\
-p,--password=password  use this password to connect to remote server\n\
-j,--position=N		start reading the binlog at postion N\n\
-t,--table=name         get raw table dump using COM_TABLE_DUMB \n\
-?,--help		this message\n");
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
	(void)my_fwrite(stdout, (byte*) net->read_pos, packet_len, MYF(0));
    }

  fflush(stdout);
}

static int parse_args(int *argc, char*** argv)
{
  int c, opt_index = 0;

  while((c = getopt_long(*argc, *argv, "so:#::h:j:u:p:P:t:?", long_options,
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
      offset = atoi(optarg);
      break;

    case 'j':
      position = atoi(optarg);
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

    case 'u':
      use_remote = 1;
      user = my_strdup(optarg, MYF(0));
      break;

    case 't':
      table = my_strdup(optarg, MYF(0));
      break;

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
  MYSQL *local_mysql = mc_mysql_init(NULL);
  if(!local_mysql)
    die("Failed on mc_mysql_init");

  if(!mc_mysql_connect(local_mysql, host, user, pass, 0, port, 0, 0))
    die("failed on connect: %s", mc_mysql_error(local_mysql));

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
  
  if(mc_simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
    die("Error sending the table dump command");

  for(;;)
    {
      uint packet_len = my_net_read(net);
      if(packet_len == 0) break; // end of file
      if(packet_len == packet_error)
	die("Error reading packet in table dump");
      my_fwrite(stdout, (byte*)net->read_pos, packet_len, MYF(MY_WME));
      fflush(stdout);
    }
}


static void dump_remote_log_entries(const char* logname)
{
  char buf[128];
  uint len;
  NET* net = &mysql->net;
  if(!position) position = 4; // protect the innocent from spam
  if(position < 4)
    {
      position = 4;
      // warn the guity
      fprintf(stderr,
      "Warning: with the position so small you would hit the magic number\n\
Unfortunately, no sweepstakes today, adjusted position to 4\n");
    }
  int4store(buf, position);
  int2store(buf + 4, binlog_flags);
  len = (uint) strlen(logname);
  int4store(buf + 6, 0);
  memcpy(buf + 10, logname,len);
  if(mc_simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
    die("Error sending the log dump command");
  
  for(;;)
  {
    len = mc_net_safe_read(mysql);
    if (len == packet_error)
      die("Error reading packet from server: %s", mc_mysql_error(mysql));
    if(len == 1 && net->read_pos[0] == 254)
      break; // end of data
    DBUG_PRINT("info",( "len= %u, net->read_pos[5] = %d\n",
			len, net->read_pos[5]));
    Log_event * ev = Log_event::read_log_event(
					  (const char*) net->read_pos + 1 ,
					  len);
    if(ev)
    {
      ev->print(stdout, short_form);
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
  File fd;
  IO_CACHE cache,*file= &cache;
  int rec_count = 0;

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
    if (init_io_cache(file, fileno(stdout), 0, READ_CACHE, (my_off_t) 0,
		      0, MYF(MY_WME | MY_NABP | MY_DONT_CHECK_FILESIZE)))
      exit(1);
    if (position)
    {
      /* skip 'position' characters from stdout */
      char buff[IO_SIZE];
      my_off_t length,tmp;
      for (length=position ; length > 0 ; length-=tmp)
      {
	tmp=min(length,sizeof(buff));
	if (my_b_read(file,buff,tmp))
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
      die("Bad magic number");
  }
 
  while(1)
  {
    Log_event* ev = Log_event::read_log_event(file, 0);
    if (!ev)
    {
      if (file->error)
	die("Could not read entry at offset %ld : Error in log format or \
read error",
	    my_b_tell(file));
      break;
    }
    if (rec_count >= offset)
      ev->print(stdout, short_form);
    rec_count++;
    delete ev;
  }
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
    init_thr_alarm(10); // need to do this manually 
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
  if (use_remote)
    mc_mysql_close(mysql);
  return 0;
}

/*
  We must include this here as it's compiled with different options for
  the server
*/

#include "log_event.cc"
