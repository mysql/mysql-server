/* Copyright (C) 2001 MySQL AB

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
#include "client_priv.h"
#include <time.h>
#include "log_event.h"

#define BIN_LOG_HEADER_SIZE	4
#define PROBE_HEADER_LEN	(EVENT_LEN_OFFSET+4)


#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES)

char server_version[SERVER_VERSION_LENGTH];
ulong server_id = 0;

// needed by net_serv.c
ulong bytes_sent = 0L, bytes_received = 0L;
ulong mysqld_net_retry_count = 10L;
uint test_flags = 0; 

static FILE *result_file;

#ifndef DBUG_OFF
static const char* default_dbug_option = "d:t:o,/tmp/mysqlbinlog.trace";
#endif

void sql_print_error(const char *format, ...);

static bool one_database = 0;
static const char* database;
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

static struct my_option my_long_options[] =
{
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log.", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"database", 'd', "List entries for just this database (local log only)",
   (gptr*) &database, (gptr*) &database, 0, GET_STR_ALLOC, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Get the binlog from server", (gptr*) &host, (gptr*) &host,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"offset", 'o', "Skip the first N entries", (gptr*) &offset, (gptr*) &offset,
   0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p', "Password to connect to remote server",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Use port to connect to the remote server",
   (gptr*) &port, (gptr*) &port, 0, GET_INT, REQUIRED_ARG, MYSQL_PORT, 0, 0,
   0, 0, 0},
  {"position", 'j', "Start reading the binlog at position N",
   (gptr*) &position, (gptr*) &position, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"result-file", 'r', "Direct output to a given file", 0, 0, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"short-form", 's', "Just show the queries, no extra info",
   (gptr*) &short_form, (gptr*) &short_form, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"table", 't', "Get raw table dump using COM_TABLE_DUMB", (gptr*) &table,
   (gptr*) &table, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "Connect to the remote server as username",
   (gptr*) &user, (gptr*) &user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"version", 'V', "Print version and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


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
  printf("%s Ver 2.3 for %s at %s\n", my_progname, SYSTEM_TYPE, MACHINE_TYPE);
}


static void usage()
{
  print_version();
  puts("By Monty and Sasha, for your professional use\n\
This software comes with NO WARRANTY:  This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n");

  printf("\
Dumps a MySQL binary log in a format usable for viewing or for piping to\n\
the mysql command line client\n\n");
  printf("Usage: %s [options] log-files\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
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


extern "C" my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
#ifndef DBUG_OFF
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
#endif
  case 'd':
    one_database = 1;
    break;
  case 'h':
    use_remote = 1;
    break;
  case 'P':
    use_remote = 1;
    break;
  case 'p':
    use_remote = 1;
    pass = my_strdup(argument, MYF(0));
    break;
  case 'r':
    if (!(result_file = my_fopen(argument, O_WRONLY | O_BINARY, MYF(MY_WME))))
      exit(1);
    break;
  case 'u':
    use_remote = 1;
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static int parse_args(int *argc, char*** argv)
{
  int ho_error;

  result_file = stdout;
  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  return 0;
}

static MYSQL* safe_connect()
{
  MYSQL *local_mysql = mysql_init(NULL);
  if(!local_mysql)
    die("Failed on mysql_init");

  if (!mysql_real_connect(local_mysql, host, user, pass, 0, port, 0, 0))
    die("failed on connect: %s", mysql_error(local_mysql));

  return local_mysql;
}

static void dump_log_entries(const char* logname)
{
  if (use_remote)
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
  if (table_len + db_len > sizeof(buf) - 2)
    die("Buffer overrun");

  *p++ = db_len;
  memcpy(p, db, db_len);
  p += db_len;
  *p++ = table_len;
  memcpy(p, table, table_len);

  if (simple_command(mysql, COM_TABLE_DUMP, buf, p - buf + table_len, 1))
    die("Error sending the table dump command");

  for (;;)
  {
    uint packet_len = my_net_read(net);
    if (packet_len == 0) break; // end of file
    if (packet_len == packet_error)
      die("Error reading packet in table dump");
    my_fwrite(result_file, (byte*)net->read_pos, packet_len, MYF(MY_WME));
    fflush(result_file);
  }
}

static int check_master_version(MYSQL* mysql)
{
  MYSQL_RES* res = 0;
  MYSQL_ROW row;
  const char* version;
  int old_format = 0;

  if (mysql_query(mysql, "SELECT VERSION()") ||
      !(res = mysql_store_result(mysql)))
  {
    mysql_close(mysql);
    die("Error checking master version: %s",
		    mysql_error(mysql));
  }
  if (!(row = mysql_fetch_row(res)))
  {
    mysql_free_result(res);
    mysql_close(mysql);
    die("Master returned no rows for SELECT VERSION()");
    return 1;
  }
  if (!(version = row[0]))
  {
    mysql_free_result(res);
    mysql_close(mysql);
    die("Master reported NULL for the version");
  }

  switch (*version) {
  case '3':
    old_format = 1;
    break;
  case '4':
  case '5':
    old_format = 0;
    break;
  default:
    sql_print_error("Master reported unrecognized MySQL version '%s'",
		    version);
    mysql_free_result(res);
    mysql_close(mysql);
    return 1;
  }
  mysql_free_result(res);
  return old_format;
}


static void dump_remote_log_entries(const char* logname)
{
  char buf[128];
  char last_db[FN_REFLEN+1] = "";
  uint len;
  NET* net = &mysql->net;
  int old_format;
  old_format = check_master_version(mysql);

  if (!position)
    position = BIN_LOG_HEADER_SIZE; // protect the innocent from spam
  if (position < BIN_LOG_HEADER_SIZE)
  {
    position = BIN_LOG_HEADER_SIZE;
    // warn the guity
    sql_print_error("Warning: The position in the binary log can't be less than %d.\nStarting from position %d\n", BIN_LOG_HEADER_SIZE, BIN_LOG_HEADER_SIZE);
  }
  int4store(buf, position);
  int2store(buf + BIN_LOG_HEADER_SIZE, binlog_flags);
  len = (uint) strlen(logname);
  int4store(buf + 6, 0);
  memcpy(buf + 10, logname,len);
  if (simple_command(mysql, COM_BINLOG_DUMP, buf, len + 10, 1))
    die("Error sending the log dump command");

  for (;;)
  {
    const char *error;
    len = net_safe_read(mysql);
    if (len == packet_error)
      die("Error reading packet from server: %s", mysql_error(mysql));
    if (len == 1 && net->read_pos[0] == 254)
      break; // end of data
    DBUG_PRINT("info",( "len= %u, net->read_pos[5] = %d\n",
			len, net->read_pos[5]));
    Log_event *ev = Log_event::read_log_event((const char*) net->read_pos + 1 ,
					      len - 1, &error, old_format);
    if (ev)
    {
      ev->print(result_file, short_form, last_db);
      if (ev->get_type_code() == LOAD_EVENT)
	dump_remote_file(net, ((Load_log_event*)ev)->fname);
      delete ev;
    }
    else
      die("Could not construct log event object");
  }
}


static int check_header(IO_CACHE* file)
{
  byte header[BIN_LOG_HEADER_SIZE];
  byte buf[PROBE_HEADER_LEN];
  int old_format=0;

  my_off_t pos = my_b_tell(file);
  my_b_seek(file, (my_off_t)0);
  if (my_b_read(file, header, sizeof(header)))
    die("Failed reading header;  Probably an empty file");
  if (memcmp(header, BINLOG_MAGIC, sizeof(header)))
    die("File is not a binary log file");
  if (!my_b_read(file, buf, sizeof(buf)))
  {
    if (buf[4] == START_EVENT)
    {
      uint event_len;
      event_len = uint4korr(buf + 4);
      old_format = (event_len < LOG_EVENT_HEADER_LEN + START_HEADER_LEN);
    }
  }
  my_b_seek(file, pos);
  return old_format;
}


static void dump_local_log_entries(const char* logname)
{
  File fd = -1;
  IO_CACHE cache,*file= &cache;
  ulonglong rec_count = 0;
  char last_db[FN_REFLEN+1];
  byte tmp_buff[BIN_LOG_HEADER_SIZE];
  bool old_format = 0;

  last_db[0]=0;

  if (logname && logname[0] != '-')
  {
    if ((fd = my_open(logname, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0)
      exit(1);
    if (init_io_cache(file, fd, 0, READ_CACHE, (my_off_t) position, 0,
		      MYF(MY_WME | MY_NABP)))
      exit(1);
    old_format = check_header(file);
  }
  else
  {
    if (init_io_cache(file, fileno(result_file), 0, READ_CACHE, (my_off_t) 0,
		      0, MYF(MY_WME | MY_NABP | MY_DONT_CHECK_FILESIZE)))
      exit(1);
    old_format = check_header(file);
    if (position)
    {
      /* skip 'position' characters from stdout */
      byte buff[IO_SIZE];
      my_off_t length,tmp;
      for (length= (my_off_t) position ; length > 0 ; length-=tmp)
      {
	tmp=min(length,sizeof(buff));
	if (my_b_read(file, buff, (uint) tmp))
	  exit(1);
      }
    }
    file->pos_in_file=position;
    file->seek_not_done=0;
  }

  if (!position)
    my_b_read(file, tmp_buff, BIN_LOG_HEADER_SIZE); // Skip header
  for (;;)
  {
    char llbuff[21];
    my_off_t old_off = my_b_tell(file);

    Log_event* ev = Log_event::read_log_event(file, old_format);
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
      // see if we should skip this event (only care about queries for now)
      if (one_database)
      {
        if (ev->get_type_code() == QUERY_EVENT)
        {
          //const char * log_dbname = ev->get_db();
          const char * log_dbname = ((Query_log_event*)ev)->db;
          //printf("entry: %llu, database: %s\n", rec_count, log_dbname);

          if ((log_dbname != NULL) && (strcmp(log_dbname, database)))
          {
            //printf("skipping, %s is not %s\n", log_dbname, database);
            rec_count++;
            delete ev;
            continue; // next
          }
#ifndef DBUG_OFF
          else
          {
            printf("no skip\n");
          }
#endif
        }
#ifndef DBUG_OFF
        else
        {
          const char * query_type = ev->get_type_str();
          printf("not query -- %s\n", query_type);
        }
#endif
      }
      if (!short_form)
        fprintf(result_file, "# at %s\n",llstr(old_off,llbuff));

      ev->print(result_file, short_form, last_db);
    }
    rec_count++;
    delete ev;
  }
  if (fd >= 0)
   my_close(fd, MYF(MY_WME));
  end_io_cache(file);
}


int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(&argc, (char***)&argv);

  if (!argc && !table)
  {
    usage();
    return -1;
  }

  if (use_remote)
    mysql = safe_connect();

  if (table)
  {
    if (!use_remote)
      die("You must specify connection parameter to get table dump");
    char* db = (char*) table;
    char* tbl = (char*) strchr(table, '.');
    if (!tbl)
      die("You must use database.table syntax to specify the table");
    *tbl++ = 0;
    dump_remote_table(&mysql->net, db, tbl);
  }
  else
  {
    while (--argc >= 0)
      dump_log_entries(*(argv++));
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

#ifdef __WIN__
#include "log_event.cpp"
#else
#include "log_event.cc"
#endif

FIX_GCC_LINKING_PROBLEM
