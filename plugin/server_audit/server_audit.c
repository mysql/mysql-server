/* Copyright (C) 2013, 2015, Alexey Botchkov and SkySQL Ab

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


#define PLUGIN_VERSION 0x104
#define PLUGIN_STR_VERSION "1.4.0"

#define _my_thread_var loc_thread_var

#include <my_config.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

#ifndef _WIN32
#include <syslog.h>
#else
#define syslog(PRIORITY, FORMAT, INFO, MESSAGE_LEN, MESSAGE) do {}while(0)
static void closelog() {}
#define openlog(IDENT, LOG_NOWAIT, LOG_USER)  do {}while(0)

/* priorities */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

#define LOG_MAKEPRI(fac, pri)   (((fac) << 3) | (pri))

/* facility codes */
#define LOG_KERN        (0<<3)  /* kernel messages */
#define LOG_USER        (1<<3)  /* random user-level messages */
#define LOG_MAIL        (2<<3)  /* mail system */
#define LOG_DAEMON      (3<<3)  /* system daemons */
#define LOG_AUTH        (4<<3)  /* security/authorization messages */
#define LOG_SYSLOG      (5<<3)  /* messages generated internally by syslogd */
#define LOG_LPR         (6<<3)  /* line printer subsystem */
#define LOG_NEWS        (7<<3)  /* network news subsystem */
#define LOG_UUCP        (8<<3)  /* UUCP subsystem */
#define LOG_CRON        (9<<3)  /* clock daemon */
#define LOG_AUTHPRIV    (10<<3) /* security/authorization messages (private) */
#define LOG_FTP         (11<<3) /* ftp daemon */
#define LOG_LOCAL0      (16<<3) /* reserved for local use */
#define LOG_LOCAL1      (17<<3) /* reserved for local use */
#define LOG_LOCAL2      (18<<3) /* reserved for local use */
#define LOG_LOCAL3      (19<<3) /* reserved for local use */
#define LOG_LOCAL4      (20<<3) /* reserved for local use */
#define LOG_LOCAL5      (21<<3) /* reserved for local use */
#define LOG_LOCAL6      (22<<3) /* reserved for local use */
#define LOG_LOCAL7      (23<<3) /* reserved for local use */

#endif /*!_WIN32*/

/*
   Defines that can be used to reshape the pluging:
   #define MARIADB_ONLY
   #define USE_MARIA_PLUGIN_INTERFACE
*/

#if !defined(MYSQL_DYNAMIC_PLUGIN) && !defined(MARIADB_ONLY)
#define MARIADB_ONLY
#endif /*MYSQL_PLUGIN_DYNAMIC*/

#ifndef MARIADB_ONLY
#define MYSQL_SERVICE_LOGGER_INCLUDED
#endif /*MARIADB_ONLY*/

#include <my_base.h>
//#include <my_dir.h>
#include <typelib.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT NULL
#endif

#ifndef MARIADB_ONLY
#undef MYSQL_SERVICE_LOGGER_INCLUDED
#undef MYSQL_DYNAMIC_PLUGIN
#define FLOGGER_NO_PSI

/* How to access the pthread_mutex in mysql_mutex_t */
//#ifdef SAFE_MUTEX
//#define mysql_mutex_real_mutex(A) &(A)->m_mutex.mutex
//#elif defined(MY_PTHREAD_FASTMUTEX)
//#define mysql_mutex_real_mutex(A) &(A)->m_mutex.mutex
//#else
#define mysql_mutex_real_mutex(A) &(A)->m_mutex
//#endif

#define flogger_mutex_init(A,B,C) do{}while(0)
#define flogger_mutex_destroy(A) do{}while(0)
#define flogger_mutex_lock(A) do{}while(0)
#define flogger_mutex_unlock(A) do{}while(0)

static char **int_mysql_data_home;
static char *default_home= (char *)".";
#define mysql_data_home (*int_mysql_data_home)

#define FLOGGER_SKIP_INCLUDES
#define my_open(A, B, C) loc_open(A, B)
#define my_close(A, B) loc_close(A)
#define my_rename(A, B, C) loc_rename(A, B)
#define my_tell(A, B) loc_tell(A)
#define my_write(A, B, C, D) loc_write(A, B, C)
#define my_malloc(A, B) malloc(A)
#define my_free(A) free(A)
#ifdef my_errno
  #undef my_errno
#endif
static int loc_file_errno;
#define my_errno loc_file_errno
#ifdef my_vsnprintf
  #undef my_vsnprintf
#endif
#define my_vsnprintf vsnprintf
#define logger_open loc_logger_open
#define logger_close loc_logger_close
#define logger_write loc_logger_write
#define logger_rotate loc_logger_rotate
#define logger_init_mutexts loc_logger_init_mutexts


static size_t loc_write(File Filedes, const uchar *Buffer, size_t Count)
{
  size_t writtenbytes;
#ifdef _WIN32
  writtenbytes= my_win_write(Filedes, Buffer, Count);
#else
  writtenbytes= write(Filedes, Buffer, Count);
#endif
  return writtenbytes;
}


static File loc_open(const char *FileName, int Flags)
				/* Path-name of file */
				/* Read | write .. */
				/* Special flags */
{
  File fd;
#if defined(_WIN32)
  fd= my_win_open(FileName, Flags);
#elif !defined(NO_OPEN_3)
  fd = open(FileName, Flags, my_umask);     /* Normal unix */
#else
  fd = open((char *) FileName, Flags);
#endif
  my_errno= errno;
  return fd;
} 


static int loc_close(File fd)
{
  int err;
#ifndef _WIN32
  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);
#else
  err= my_win_close(fd);
#endif
  my_errno=errno;
  return err;
}


static int loc_rename(const char *from, const char *to)
{
  int error = 0;

#if defined(__WIN__)
  if (!MoveFileEx(from, to, MOVEFILE_COPY_ALLOWED |
                            MOVEFILE_REPLACE_EXISTING))
  {
    my_osmaperr(GetLastError());
#elif defined(HAVE_RENAME)
  if (rename(from,to))
  {
#else
  if (link(from, to) || unlink(from))
  {
#endif
    my_errno=errno;
    error = -1;
  }
  return error;
}


static my_off_t loc_seek(File fd, my_off_t pos, int whence)
{
  os_off_t newpos= -1;
#ifdef _WIN32
  newpos= my_win_lseek(fd, pos, whence);
#else
  newpos= lseek(fd, pos, whence);
#endif
  if (newpos == (os_off_t) -1)
  {
    my_errno= errno;
    return MY_FILEPOS_ERROR;
  }

  return (my_off_t) newpos;
}


static my_off_t loc_tell(File fd)
{
  os_off_t pos;
#if defined (HAVE_TELL) && !defined (_WIN32)
  pos= tell(fd);
#else
  pos= loc_seek(fd, 0L, MY_SEEK_CUR);
#endif
  if (pos == (os_off_t) -1)
  {
    my_errno= errno;
  }
  return (my_off_t) pos;
}

#ifdef HAVE_PSI_INTERFACE
#undef HAVE_PSI_INTERFACE
#include <mysql/service_logger.h>
#include "../../mysys/file_logger.c"
#define HAVE_PSI_INTERFACE
#else
#include <mysql/service_logger.h>
#include "../../mysys/file_logger.c"
#endif
#endif /*!MARIADB_ONLY*/

#undef flogger_mutex_init
#undef flogger_mutex_destroy
#undef flogger_mutex_lock
#undef flogger_mutex_unlock

#define flogger_mutex_init(A,B,C) pthread_mutex_init(mysql_mutex_real_mutex(B), C)
#define flogger_mutex_destroy(A) pthread_mutex_destroy(mysql_mutex_real_mutex(A))
#define flogger_mutex_lock(A) pthread_mutex_lock(mysql_mutex_real_mutex(A))
#define flogger_mutex_unlock(A) pthread_mutex_unlock(mysql_mutex_real_mutex(A))

#ifndef DBUG_OFF
#define PLUGIN_DEBUG_VERSION "-debug"
#else
#define PLUGIN_DEBUG_VERSION ""
#endif /*DBUG_OFF*/
/*
 Disable __attribute__() on non-gcc compilers.
*/
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(A)
#endif

#ifdef _WIN32
#define localtime_r(a, b) localtime_s(b, a)
#endif /*WIN32*/


extern char server_version[];
static const char *serv_ver= NULL;
static int started_mysql= 0;
static int mysql_57_started= 0;
static int debug_server_started= 0;
static int use_event_data_for_disconnect= 0;
static int started_mariadb= 0;
static int maria_55_started= 0;
static int maria_above_5= 0;
static char *incl_users, *excl_users,
            *file_path, *syslog_info;
static char path_buffer[FN_REFLEN];
static unsigned int mode, mode_readonly= 0;
static ulong output_type;
static ulong syslog_facility, syslog_priority;

static ulonglong events; /* mask for events to log */
static unsigned long long file_rotate_size;
static unsigned int rotations;
static my_bool rotate= TRUE;
static char logging;
static int internal_stop_logging= 0;
static char incl_user_buffer[1024];
static char excl_user_buffer[1024];
static char *big_buffer= NULL;
static size_t big_buffer_alloced= 0;
static unsigned int query_log_limit= 0;

static char servhost[256];
static size_t servhost_len;
static char *syslog_ident;
static char syslog_ident_buffer[128]= "mysql-server_auditing";

struct connection_info
{
  int header;
  unsigned long thread_id;
  unsigned long long query_id;
  char db[256];
  int db_length;
  char user[64];
  int user_length;
  char host[64];
  int host_length;
  char ip[64];
  int ip_length;
  const char *query;
  int query_length;
  char query_buffer[1024];
  time_t query_time;
  int log_always;
};

#define DEFAULT_FILENAME_LEN 16
static char default_file_name[DEFAULT_FILENAME_LEN+1]= "server_audit.log";

static void update_file_path(MYSQL_THD thd, struct st_mysql_sys_var *var,
                             void *var_ptr, const void *save);
static void update_file_rotate_size(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                    void *var_ptr, const void *save);
static void update_file_rotations(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                  void *var_ptr, const void *save);
static void update_incl_users(MYSQL_THD thd, struct st_mysql_sys_var *var,
                              void *var_ptr, const void *save);
static void update_excl_users(MYSQL_THD thd, struct st_mysql_sys_var *var,
                              void *var_ptr, const void *save);
static void update_output_type(MYSQL_THD thd, struct st_mysql_sys_var *var,
                               void *var_ptr, const void *save);
static void update_syslog_facility(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save);
static void update_syslog_priority(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save);
static void update_mode(MYSQL_THD thd, struct st_mysql_sys_var *var,
                        void *var_ptr, const void *save);
static void update_logging(MYSQL_THD thd, struct st_mysql_sys_var *var,
                           void *var_ptr, const void *save);
static void update_syslog_ident(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                void *var_ptr, const void *save);
static void rotate_log(MYSQL_THD thd, struct st_mysql_sys_var *var,
                       void *var_ptr, const void *save);

static MYSQL_SYSVAR_STR(incl_users, incl_users, PLUGIN_VAR_RQCMDARG,
       "Comma separated list of users to monitor.",
       NULL, update_incl_users, NULL);
static MYSQL_SYSVAR_STR(excl_users, excl_users, PLUGIN_VAR_RQCMDARG,
       "Comma separated list of users to exclude from auditing.",
       NULL, update_excl_users, NULL);
/* bits in the event filter. */
#define EVENT_CONNECT 1
#define EVENT_QUERY_ALL 2
#define EVENT_QUERY 58
#define EVENT_TABLE 4
#define EVENT_QUERY_DDL 8
#define EVENT_QUERY_DML 16
#define EVENT_QUERY_DCL 32

static const char *event_names[]=
{
  "CONNECT", "QUERY", "TABLE", "QUERY_DDL", "QUERY_DML", "QUERY_DCL",
  NULL
};
static TYPELIB events_typelib=
{
  array_elements(event_names) - 1, "", event_names, NULL
};
static MYSQL_SYSVAR_SET(events, events, PLUGIN_VAR_RQCMDARG,
       "Specifies the set of events to monitor. Can be CONNECT, QUERY, TABLE,"
           " QUERY_DDL, QUERY_DML, QUERY_DCL.",
       NULL, NULL, 0, &events_typelib);
#define OUTPUT_SYSLOG 0
#define OUTPUT_FILE 1
#define OUTPUT_NO 0xFFFF
static const char *output_type_names[]= { "syslog", "file", 0 };
static TYPELIB output_typelib=
{
    array_elements(output_type_names) - 1, "output_typelib",
    output_type_names, NULL
};
static MYSQL_SYSVAR_ENUM(output_type, output_type, PLUGIN_VAR_RQCMDARG,
       "Desired output type. Possible values - 'syslog', 'file'"
       " or 'null' as no output.", 0, update_output_type, OUTPUT_FILE,
       &output_typelib);
static MYSQL_SYSVAR_STR(file_path, file_path, PLUGIN_VAR_RQCMDARG,
       "Path to the log file.", NULL, update_file_path, default_file_name);
static MYSQL_SYSVAR_ULONGLONG(file_rotate_size, file_rotate_size,
       PLUGIN_VAR_RQCMDARG, "Maximum size of the log to start the rotation.",
       NULL, update_file_rotate_size,
       1000000, 100, ((long long) 0x7FFFFFFFFFFFFFFFLL), 1);
static MYSQL_SYSVAR_UINT(file_rotations, rotations,
       PLUGIN_VAR_RQCMDARG, "Number of rotations before log is removed.",
       NULL, update_file_rotations, 9, 0, 999, 1);
static MYSQL_SYSVAR_BOOL(file_rotate_now, rotate, PLUGIN_VAR_OPCMDARG,
       "Force log rotation now.", NULL, rotate_log, FALSE);
static MYSQL_SYSVAR_BOOL(logging, logging,
       PLUGIN_VAR_OPCMDARG, "Turn on/off the logging.", NULL,
       update_logging, 0);
static MYSQL_SYSVAR_UINT(mode, mode,
       PLUGIN_VAR_OPCMDARG, "Auditing mode.", NULL, update_mode, 0, 0, 1, 1);
static MYSQL_SYSVAR_STR(syslog_ident, syslog_ident, PLUGIN_VAR_RQCMDARG,
       "The SYSLOG identifier - the beginning of each SYSLOG record.",
       NULL, update_syslog_ident, syslog_ident_buffer);
static MYSQL_SYSVAR_STR(syslog_info, syslog_info,
       PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
       "The <info> string to be added to the SYSLOG record.", NULL, NULL, "");
static MYSQL_SYSVAR_UINT(query_log_limit, query_log_limit,
       PLUGIN_VAR_OPCMDARG, "Limit on the length of the query string in a record.",
       NULL, NULL, 1024, 0, 0x7FFFFFFF, 1);

char locinfo_ini_value[sizeof(struct connection_info)+4];

static MYSQL_THDVAR_STR(loc_info,
                        PLUGIN_VAR_NOSYSVAR | PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_MEMALLOC,
                        "Internal info", NULL, NULL, locinfo_ini_value);

static const char *syslog_facility_names[]=
{
  "LOG_USER", "LOG_MAIL", "LOG_DAEMON", "LOG_AUTH",
  "LOG_SYSLOG", "LOG_LPR", "LOG_NEWS", "LOG_UUCP",
  "LOG_CRON",
#ifdef LOG_AUTHPRIV
 "LOG_AUTHPRIV",
#endif
#ifdef LOG_FTP
 "LOG_FTP",
#endif
  "LOG_LOCAL0", "LOG_LOCAL1", "LOG_LOCAL2", "LOG_LOCAL3",
  "LOG_LOCAL4", "LOG_LOCAL5", "LOG_LOCAL6", "LOG_LOCAL7",
  0
};
static unsigned int syslog_facility_codes[]=
{
  LOG_USER, LOG_MAIL, LOG_DAEMON, LOG_AUTH,
  LOG_SYSLOG, LOG_LPR, LOG_NEWS, LOG_UUCP,
  LOG_CRON,
#ifdef LOG_AUTHPRIV
 LOG_AUTHPRIV,
#endif
#ifdef LOG_FTP
  LOG_FTP,
#endif
  LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
  LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7,
};
static TYPELIB syslog_facility_typelib=
{
    array_elements(syslog_facility_names) - 1, "syslog_facility_typelib",
    syslog_facility_names, NULL
};
static MYSQL_SYSVAR_ENUM(syslog_facility, syslog_facility, PLUGIN_VAR_RQCMDARG,
       "The 'facility' parameter of the SYSLOG record."
       " The default is LOG_USER.", 0, update_syslog_facility, 0/*LOG_USER*/,
       &syslog_facility_typelib);

static const char *syslog_priority_names[]=
{
  "LOG_EMERG", "LOG_ALERT", "LOG_CRIT", "LOG_ERR",
  "LOG_WARNING", "LOG_NOTICE", "LOG_INFO", "LOG_DEBUG",
  0
};

static unsigned int syslog_priority_codes[]=
{
  LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
  LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG,
};

static TYPELIB syslog_priority_typelib=
{
    array_elements(syslog_priority_names) - 1, "syslog_priority_typelib",
    syslog_priority_names, NULL
};
static MYSQL_SYSVAR_ENUM(syslog_priority, syslog_priority, PLUGIN_VAR_RQCMDARG,
       "The 'priority' parameter of the SYSLOG record."
       " The default is LOG_INFO.", 0, update_syslog_priority, 6/*LOG_INFO*/,
       &syslog_priority_typelib);


static struct st_mysql_sys_var* vars[] = {
    MYSQL_SYSVAR(incl_users),
    MYSQL_SYSVAR(excl_users),
    MYSQL_SYSVAR(events),
    MYSQL_SYSVAR(output_type),
    MYSQL_SYSVAR(file_path),
    MYSQL_SYSVAR(file_rotate_size),
    MYSQL_SYSVAR(file_rotations),
    MYSQL_SYSVAR(file_rotate_now),
    MYSQL_SYSVAR(logging),
    MYSQL_SYSVAR(mode),
    MYSQL_SYSVAR(syslog_info),
    MYSQL_SYSVAR(syslog_ident),
    MYSQL_SYSVAR(syslog_facility),
    MYSQL_SYSVAR(syslog_priority),
    MYSQL_SYSVAR(query_log_limit),
    MYSQL_SYSVAR(loc_info),
    NULL
};


/* Status variables for SHOW STATUS */
static int is_active= 0;
static long log_write_failures= 0;
static char current_log_buf[FN_REFLEN]= "";
static char last_error_buf[512]= "";

extern void *mysql_v4_descriptor;

static struct st_mysql_show_var audit_status[]=
{
  {"server_audit_active", (char *)&is_active, SHOW_BOOL},
  {"server_audit_current_log", current_log_buf, SHOW_CHAR},
  {"server_audit_writes_failed", (char *)&log_write_failures, SHOW_LONG},
  {"server_audit_last_error", last_error_buf, SHOW_CHAR},
  {0,0,0}
};

#if defined(HAVE_PSI_INTERFACE) && !defined(FLOGGER_NO_PSI)
/* These belong to the service initialization */
static PSI_mutex_key key_LOCK_operations;
static PSI_mutex_key key_LOCK_bigbuffer;
static PSI_mutex_info mutex_key_list[]=
{
  { &key_LOCK_operations, "SERVER_AUDIT_plugin::lock_operations",
    PSI_FLAG_GLOBAL},
  { &key_LOCK_bigbuffer, "SERVER_AUDIT_plugin::lock_bigbuffer",
    PSI_FLAG_GLOBAL}
};
#endif
static mysql_mutex_t lock_operations;
static mysql_mutex_t lock_bigbuffer;

/* The Percona server and partly MySQL don't support         */
/* launching client errors in the 'update_variable' methods. */
/* So the client errors just disabled for them.              */
/* The possible solution is to implement the 'check_variable'*/
/* methods there properly, but at the moment i'm not sure it */
/* worths doing.                                             */
#define CLIENT_ERROR if (!started_mysql) my_printf_error

static uchar *getkey_user(const char *entry, size_t *length,
                          my_bool nu __attribute__((unused)) )
{
  const char *e= entry;
  while (*e && *e != ' ' && *e != ',')
    ++e;
  *length= e - entry;
  return (uchar *) entry;
}


static void blank_user(char *user)
{
  for (; *user && *user != ','; user++)
    *user= ' ';
}


static void remove_user(char *user)
{
  char *start_user= user;
  while (*user != ',')
  {
    if (*user == 0)
    {
      *start_user= 0;
      return;
    }
    user++;
  }
  user++;
  while (*user == ' ')
    user++;

  do {
    *(start_user++)= *user;
  } while (*(user++));
}


static void remove_blanks(char *user)
{
  char *user_orig= user;
  char *user_to= user;
  char *start_tok;
  int blank_name;

  while (*user != 0)
  {
    start_tok= user;
    blank_name= 1;
    while (*user !=0 && *user != ',')
    {
      if (*user != ' ')
        blank_name= 0;
      user++;
    }
    if (!blank_name)
    {
      while (start_tok <= user)
        *(user_to++)= *(start_tok++);
    }
    if (*user == ',')
      user++;
  }
  if (user_to > user_orig && user_to[-1] == ',')
    user_to--;
  *user_to= 0;
}


struct user_name
{
  int name_len;
  char *name;
};


struct user_coll
{
  int n_users;
  struct user_name *users;
  int n_alloced;
};


static void coll_init(struct user_coll *c)
{
  c->n_users= 0;
  c->users= 0;
  c->n_alloced= 0;
}


static void coll_free(struct user_coll *c)
{
  if (c->users)
  {
    free(c->users);
    coll_init(c);
  }
}


static int cmp_users(const void *ia, const void *ib)
{
  const struct user_name *a= (const struct user_name *) ia;
  const struct user_name *b= (const struct user_name *) ib;
  int dl= a->name_len - b->name_len;
  if (dl != 0)
    return dl;

  return strncmp(a->name, b->name, a->name_len);
}


static char *coll_search(struct user_coll *c, const char *n, int len)
{
  struct user_name un;
  struct user_name *found;
  un.name_len= len;
  un.name= (char *) n;
  found= (struct user_name*)  bsearch(&un, c->users, c->n_users,
                                      sizeof(c->users[0]), cmp_users);
  return found ? found->name : 0;
}


static int coll_insert(struct user_coll *c, char *n, int len)
{
  if (c->n_users >= c->n_alloced)
  {
    c->n_alloced+= 128;
    if (c->users == NULL)
      c->users= malloc(c->n_alloced * sizeof(c->users[0]));
    else
      c->users= realloc(c->users, c->n_alloced * sizeof(c->users[0]));

    if (c->users == NULL)
      return 1;
  }
  c->users[c->n_users].name= n;
  c->users[c->n_users].name_len= len;
  c->n_users++;
  return 0;
}


static void coll_sort(struct user_coll *c)
{
  qsort(c->users, c->n_users, sizeof(c->users[0]), cmp_users);
}


static int user_coll_fill(struct user_coll *c, char *users,
                          struct user_coll *cmp_c, int take_over_cmp)
{
  char *orig_users= users;
  char *cmp_user= 0;
  size_t cmp_length;
  int refill_cmp_coll= 0;

  c->n_users= 0;

  while (*users)
  {
    while (*users == ' ')
      users++;
    if (!*users)
      return 0;

    (void) getkey_user(users, &cmp_length, FALSE);
    if (cmp_c)
    {
      cmp_user= coll_search(cmp_c, users, cmp_length);

      if (cmp_user && take_over_cmp)
      {
        internal_stop_logging= 1;
        CLIENT_ERROR(1, "User '%.*s' was removed from the"
            " server_audit_excl_users.",
            MYF(ME_JUST_WARNING), (int) cmp_length, users);
        internal_stop_logging= 0;
        blank_user(cmp_user);
        refill_cmp_coll= 1;
      }
      else if (cmp_user)
      {
        internal_stop_logging= 1;
        CLIENT_ERROR(1, "User '%.*s' is in the server_audit_incl_users, "
            "so wasn't added.", MYF(ME_JUST_WARNING), (int) cmp_length, users);
        internal_stop_logging= 0;
        remove_user(users);
        continue;
      }
    }
    if (coll_insert(c, users, cmp_length))
      return 1;
    while (*users && *users != ',')
      users++;
    if (!*users)
      break;
    users++;
  }

  if (refill_cmp_coll)
  {
    remove_blanks(excl_users);
    return user_coll_fill(cmp_c, excl_users, 0, 0);
  }

  if (users > orig_users && users[-1] == ',')
    users[-1]= 0;

  coll_sort(c);

  return 0;
}


enum sa_keywords
{
  SQLCOM_NOTHING=0,
  SQLCOM_DDL,
  SQLCOM_DML,
  SQLCOM_GRANT,
  SQLCOM_CREATE_USER,
  SQLCOM_CHANGE_MASTER,
  SQLCOM_CREATE_SERVER,
  SQLCOM_SET_OPTION,
  SQLCOM_ALTER_SERVER,
  SQLCOM_TRUNCATE,
  SQLCOM_QUERY_ADMIN,
  SQLCOM_DCL,
};

struct sa_keyword
{
  int length;
  const char *wd;
  struct sa_keyword *next;
  enum sa_keywords type;
};


struct sa_keyword xml_word=   {3, "XML", 0, SQLCOM_NOTHING};
struct sa_keyword user_word=   {4, "USER", 0, SQLCOM_NOTHING};
struct sa_keyword data_word=   {4, "DATA", 0, SQLCOM_NOTHING};
struct sa_keyword server_word= {6, "SERVER", 0, SQLCOM_NOTHING};
struct sa_keyword master_word= {6, "MASTER", 0, SQLCOM_NOTHING};
struct sa_keyword password_word= {8, "PASSWORD", 0, SQLCOM_NOTHING};
struct sa_keyword function_word= {8, "FUNCTION", 0, SQLCOM_NOTHING};
struct sa_keyword statement_word= {9, "STATEMENT", 0, SQLCOM_NOTHING};
struct sa_keyword procedure_word= {9, "PROCEDURE", 0, SQLCOM_NOTHING};


struct sa_keyword keywords_to_skip[]=
{
  {3, "SET", &statement_word, SQLCOM_QUERY_ADMIN},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword not_ddl_keywords[]=
{
  {4, "DROP", &function_word, SQLCOM_QUERY_ADMIN},
  {4, "DROP", &procedure_word, SQLCOM_QUERY_ADMIN},
  {4, "DROP", &user_word, SQLCOM_DCL},
  {6, "CREATE", &user_word, SQLCOM_DCL},
  {6, "CREATE", &function_word, SQLCOM_QUERY_ADMIN},
  {6, "CREATE", &procedure_word, SQLCOM_QUERY_ADMIN},
  {6, "RENAME", &user_word, SQLCOM_DCL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword ddl_keywords[]=
{
  {4, "DROP", 0, SQLCOM_DDL},
  {5, "ALTER", 0, SQLCOM_DDL},
  {6, "CREATE", 0, SQLCOM_DDL},
  {6, "RENAME", 0, SQLCOM_DDL},
  {8, "TRUNCATE", 0, SQLCOM_DDL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword dml_keywords[]=
{
  {2, "DO", 0, SQLCOM_DML},
  {4, "CALL", 0, SQLCOM_DML},
  {4, "LOAD", &data_word, SQLCOM_DML},
  {4, "LOAD", &xml_word, SQLCOM_DML},
  {6, "DELETE", 0, SQLCOM_DML},
  {6, "INSERT", 0, SQLCOM_DML},
  {6, "SELECT", 0, SQLCOM_DML},
  {6, "UPDATE", 0, SQLCOM_DML},
  {7, "HANDLER", 0, SQLCOM_DML},
  {7, "REPLACE", 0, SQLCOM_DML},
  {0, NULL, 0, SQLCOM_DML}
};


struct sa_keyword dcl_keywords[]=
{
  {6, "CREATE", &user_word, SQLCOM_DCL},
  {4, "DROP", &user_word, SQLCOM_DCL},
  {6, "RENAME", &user_word, SQLCOM_DCL},
  {5, "GRANT", 0, SQLCOM_DCL},
  {6, "REVOKE", 0, SQLCOM_DCL},
  {3, "SET", &password_word, SQLCOM_DCL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword passwd_keywords[]=
{
  {3, "SET", &password_word, SQLCOM_SET_OPTION},
  {5, "ALTER", &server_word, SQLCOM_ALTER_SERVER},
  {5, "GRANT", 0, SQLCOM_GRANT},
  {6, "CREATE", &user_word, SQLCOM_CREATE_USER},
  {6, "CREATE", &server_word, SQLCOM_CREATE_SERVER},
  {6, "CHANGE", &master_word, SQLCOM_CHANGE_MASTER},
  {0, NULL, 0, SQLCOM_NOTHING}
};

#define MAX_KEYWORD 9


static void error_header()
{
  struct tm tm_time;
  time_t curtime;

  (void) time(&curtime);
  (void) localtime_r(&curtime, &tm_time);

  (void) fprintf(stderr,"%02d%02d%02d %2d:%02d:%02d server_audit: ",
    tm_time.tm_year % 100, tm_time.tm_mon + 1,
    tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
}


static LOGGER_HANDLE *logfile;
static struct user_coll incl_user_coll, excl_user_coll;
static unsigned long long query_counter= 1;


static struct connection_info *get_loc_info(MYSQL_THD thd)
{
  return (struct connection_info *) THDVAR(thd, loc_info);
}


static int ci_needs_setup(const struct connection_info *ci)
{
  return ci->header != 0;
}


static void get_str_n(char *dest, int *dest_len, size_t dest_size,
                      const char *src, size_t src_len)
{
  if (src_len >= dest_size)
    src_len= dest_size - 1;

  memcpy(dest, src, src_len);
  dest[src_len]= 0;
  *dest_len= src_len;
}


static int get_user_host(const char *uh_line, unsigned int uh_len,
                         char *buffer, size_t buf_len,
                         size_t *user_len, size_t *host_len, size_t *ip_len)
{
  const char *buf_end= buffer + buf_len - 1;
  const char *buf_start;
  const char *uh_end= uh_line + uh_len;

  while (uh_line < uh_end && *uh_line != '[')
    ++uh_line;

  if (uh_line == uh_end)
    return 1;
  ++uh_line;

  buf_start= buffer;
  while (uh_line < uh_end && *uh_line != ']')
  {
    if (buffer == buf_end)
      return 1;
    *(buffer++)= *(uh_line++);
  }
  if (uh_line == uh_end)
    return 1;
  *user_len= buffer - buf_start;
  *(buffer++)= 0;

  while (uh_line < uh_end && *uh_line != '@')
    ++uh_line;
  if (uh_line == uh_end || *(++uh_line) == 0)
    return 1;
  ++uh_line;

  buf_start= buffer;
  while (uh_line < uh_end && *uh_line != ' ' && *uh_line != '[')
  {
    if (buffer == buf_end)
      break;
    *(buffer++)= *(uh_line++);
  }
  *host_len= buffer - buf_start;
  *(buffer++)= 0;

  while (uh_line < uh_end && *uh_line != '[')
    ++uh_line;

  buf_start= buffer;
  if (*uh_line == '[')
  {
    ++uh_line;
    while (uh_line < uh_end && *uh_line != ']')
      *(buffer++)= *(uh_line++);
  }
  *ip_len= buffer - buf_start;
  return 0;
}

#if defined(__WIN__) && !defined(S_ISDIR)
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif /*__WIN__ && !S_ISDIR*/

static int start_logging()
{
  last_error_buf[0]= 0;
  log_write_failures= 0;
  if (output_type == OUTPUT_FILE)
  {
    char alt_path_buffer[FN_REFLEN+1+DEFAULT_FILENAME_LEN];
    MY_STAT *f_stat;
    const char *alt_fname= file_path;

    while (*alt_fname == ' ')
      alt_fname++;

    if (*alt_fname == 0)
    {
      /* Empty string means the default file name. */
      alt_fname= default_file_name;
    }
    else
    {
      /* See if the directory exists with the name of file_path.    */
      /* Log file name should be [file_path]/server_audit.log then. */
      if ((f_stat= my_stat(file_path, (MY_STAT *)alt_path_buffer, MYF(0))) &&
          S_ISDIR(f_stat->st_mode))
      {
        size_t p_len= strlen(file_path);
        memcpy(alt_path_buffer, file_path, p_len);
        if (alt_path_buffer[p_len-1] != FN_LIBCHAR)
        {
          alt_path_buffer[p_len]= FN_LIBCHAR;
          p_len++;
        }
        memcpy(alt_path_buffer+p_len, default_file_name, DEFAULT_FILENAME_LEN);
        alt_path_buffer[p_len+DEFAULT_FILENAME_LEN]= 0;
        alt_fname= alt_path_buffer;
      }
    }

    logfile= logger_open(alt_fname, file_rotate_size, rotations);

    if (logfile == NULL)
    {
      error_header();
      fprintf(stderr, "Could not create file '%s'.\n",
              alt_fname);
      logging= 0;
      my_snprintf(last_error_buf, sizeof(last_error_buf),
                  "Could not create file '%s'.", alt_fname);
      is_active= 0;
      CLIENT_ERROR(1, "SERVER AUDIT plugin can't create file '%s'.",
          MYF(ME_JUST_WARNING), alt_fname);
      return 1;
    }
    error_header();
    fprintf(stderr, "logging started to the file %s.\n", alt_fname);
    strncpy(current_log_buf, alt_fname, sizeof(current_log_buf));
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    openlog(syslog_ident, LOG_NOWAIT, syslog_facility_codes[syslog_facility]);
    error_header();
    fprintf(stderr, "logging started to the syslog.\n");
    strncpy(current_log_buf, "[SYSLOG]", sizeof(current_log_buf));
  }
  is_active= 1;
  return 0;
}


static int stop_logging()
{
  last_error_buf[0]= 0;
  if (output_type == OUTPUT_FILE && logfile)
  {
    logger_close(logfile);
    logfile= NULL;
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    closelog();
  }
  error_header();
  fprintf(stderr, "logging was stopped.\n");
  is_active= 0;
  return 0;
}


static void setup_connection_simple(struct connection_info *ci)
{
  ci->db_length= 0;
  ci->user_length= 0;
  ci->host_length= 0;
  ci->ip_length= 0;
  ci->query_length= 0;
  ci->header= 0;
}


static void setup_connection_connect(struct connection_info *cn,
    const struct mysql_event_connection *event)
{
  cn->query_id= 0;
  cn->log_always= 0;
  cn->thread_id= event->thread_id;
  get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
            event->database, event->database_length);
  get_str_n(cn->user, &cn->user_length, sizeof(cn->db),
            event->user, event->user_length);
  get_str_n(cn->host, &cn->host_length, sizeof(cn->host),
            event->host, event->host_length);
  get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
            event->ip, event->ip_length);
  cn->header= 0;
}


#define SAFE_STRLEN(s) (s ? strlen(s) : 0)
static char empty_str[1]= { 0 };


static int is_space(char c)
{
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}


#define SKIP_SPACES(str) \
do { \
  while (is_space(*str)) \
    ++str; \
} while(0)




static void setup_connection_initdb(struct connection_info *cn,
    const struct mysql_event_general *event)
{
  size_t user_len, host_len, ip_len;
  char uh_buffer[512];

  cn->thread_id= event->general_thread_id;
  cn->query_id= 0;
  cn->log_always= 0;
  get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
            event->general_query, event->general_query_length);

  if (get_user_host(event->general_user, event->general_user_length,
                    uh_buffer, sizeof(uh_buffer),
                    &user_len, &host_len, &ip_len))
  {
    /* The user@host line is incorrect. */
    cn->user_length= 0;
    cn->host_length= 0;
    cn->ip_length= 0;
  }
  else
  {
    get_str_n(cn->user, &cn->user_length, sizeof(cn->db),
              uh_buffer, user_len);
    get_str_n(cn->host, &cn->host_length, sizeof(cn->host),
              uh_buffer+user_len+1, host_len);
    get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
              uh_buffer+user_len+1+host_len+1, ip_len);
  }
  cn->header= 0;
}


static void setup_connection_table(struct connection_info *cn,
    const struct mysql_event_table *event)
{
  cn->thread_id= event->thread_id;
  cn->query_id= query_counter++;
  cn->log_always= 0;
  get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
            event->database, event->database_length);
  get_str_n(cn->user, &cn->user_length, sizeof(cn->db),
            event->user, SAFE_STRLEN(event->user));
  get_str_n(cn->host, &cn->host_length, sizeof(cn->host),
            event->host, SAFE_STRLEN(event->host));
  get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
            event->ip, SAFE_STRLEN(event->ip));
  cn->header= 0;
}


static void setup_connection_query(struct connection_info *cn,
    const struct mysql_event_general *event)
{
  size_t user_len, host_len, ip_len;
  char uh_buffer[512];

  cn->thread_id= event->general_thread_id;
  cn->query_id= query_counter++;
  cn->log_always= 0;
  get_str_n(cn->db, &cn->db_length, sizeof(cn->db), "", 0);

  if (get_user_host(event->general_user, event->general_user_length,
                    uh_buffer, sizeof(uh_buffer),
                    &user_len, &host_len, &ip_len))
  {
    /* The user@host line is incorrect. */
    cn->user_length= 0;
    cn->host_length= 0;
    cn->ip_length= 0;
  }
  else
  {
    get_str_n(cn->user, &cn->user_length, sizeof(cn->db),
              uh_buffer, user_len);
    get_str_n(cn->host, &cn->host_length, sizeof(cn->host),
              uh_buffer+user_len+1, host_len);
    get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
              uh_buffer+user_len+1+host_len+1, ip_len);
  }
  cn->header= 0;
}


static void change_connection(struct connection_info *cn,
    const struct mysql_event_connection *event)
{
  get_str_n(cn->user, &cn->user_length, sizeof(cn->user),
            event->user, event->user_length);
  get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
            event->ip, event->ip_length);
}

static int write_log(const char *message, int len)
{
  if (output_type == OUTPUT_FILE)
  {
    if (logfile &&
        (is_active= (logger_write(logfile, message, len) == len)))
      return 0;
    ++log_write_failures;
    return 1;
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    syslog(syslog_facility_codes[syslog_facility] |
           syslog_priority_codes[syslog_priority],
           "%s %.*s", syslog_info, len, message);
  }
  return 0;
}


static size_t log_header(char *message, size_t message_len,
                      time_t *ts,
                      const char *serverhost, unsigned int serverhost_len,
                      const char *username, unsigned int username_len,
                      const char *host, unsigned int host_len,
                      const char *userip, unsigned int userip_len,
                      unsigned int connection_id, long long query_id,
                      const char *operation)
{
  struct tm tm_time;

  if (host_len == 0 && userip_len != 0)
  {
    host_len= userip_len;
    host= userip;
  }

  if (output_type == OUTPUT_SYSLOG)
    return my_snprintf(message, message_len,
        "%.*s,%.*s,%.*s,%d,%lld,%s",
        serverhost_len, serverhost,
        username_len, username,
        host_len, host,
        connection_id, query_id, operation);

  (void) localtime_r(ts, &tm_time);
  return my_snprintf(message, message_len,
      "%04d%02d%02d %02d:%02d:%02d,%.*s,%.*s,%.*s,%d,%lld,%s",
      tm_time.tm_year+1900, tm_time.tm_mon+1, tm_time.tm_mday,
      tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
      serverhost_len, serverhost,
      username_len, username,
      host_len, host,
      connection_id, query_id, operation);
}


static int log_connection(const struct connection_info *cn,
                          const struct mysql_event_connection *event,
                          const char *type)
{
  time_t ctime;
  size_t csize;
  char message[1024];

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    cn->user, cn->user_length,
                    cn->host, cn->host_length,
                    cn->ip, cn->ip_length,
                    event->thread_id, 0, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,,%d", cn->db_length, cn->db, event->status);
  message[csize]= '\n';
  return write_log(message, csize + 1);
}


static int log_connection_event(const struct mysql_event_connection *event,
                                const char *type)
{
  time_t ctime;
  size_t csize;
  char message[1024];

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    event->user, event->user_length,
                    event->host, event->host_length,
                    event->ip, event->ip_length,
                    event->thread_id, 0, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,,%d", event->database_length, event->database, event->status);
  message[csize]= '\n';
  return write_log(message, csize + 1);
}


static size_t escape_string(const char *str, unsigned int len,
                          char *result, size_t result_len)
{
  const char *res_start= result;
  const char *res_end= result + result_len - 2;
  while (len)
  {
    if (result >= res_end)
      break;
    if (*str == '\'')
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= '\'';
    }
    else if (*str == '\\')
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= '\\';
    }
    else if (is_space(*str))
      *(result++)= ' ';
    else
      *(result++)= *str;
    str++;
    len--;
  }
  *result= 0;
  return result - res_start;
}


static size_t escape_string_hide_passwords(const char *str, unsigned int len,
    char *result, size_t result_len,
    const char *word1, size_t word1_len,
    const char *word2, size_t word2_len,
    int next_text_string)
{
  const char *res_start= result;
  const char *res_end= result + result_len - 2;
  size_t d_len;
  char b_char;

  while (len)
  {
    if (len > word1_len + 1 && strncasecmp(str, word1, word1_len) == 0)
    {
      const char *next_s= str + word1_len;
      size_t c;

      if (next_text_string)
      {
        while (*next_s && *next_s != '\'' && *next_s != '"')
          ++next_s;
      }
      else
      {
        if (word2)
        {
          SKIP_SPACES(next_s);
          if (len < (next_s - str) + word2_len + 1 ||
              strncasecmp(next_s, word2, word2_len) != 0)
            goto no_password;
          next_s+= word2_len;
        }

        while (*next_s && *next_s != '\'' && *next_s != '"')
          ++next_s;
      }

      d_len= next_s - str;
      if (result + d_len + 5 > res_end)
        break;

      for (c=0; c<d_len; c++)
        result[c]= is_space(str[c]) ? ' ' : str[c];

      if (*next_s)
      {
        memmove(result + d_len, "*****", 5);
        result+= d_len + 5;
        b_char= *(next_s++);
      }
      else
        result+= d_len;

      while (*next_s)
      {
        if (*next_s == b_char)
        {
          ++next_s;
          break;
        }
        if (*next_s == '\\')
        {
          if (next_s[1])
            next_s++;
        }
        next_s++;
      }
      len-= next_s - str;
      str= next_s;
      continue;
    }
no_password:
    if (result >= res_end)
      break;
    if (*str == '\'')
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= '\'';
    }
    else if (*str == '\\')
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= '\\';
    }
    else if (is_space(*str))
      *(result++)= ' ';
    else
      *(result++)= *str;
    str++;
    len--;
  }
  *result= 0;
  return result - res_start;
}



static int do_log_user(const char *name)
{
  size_t len;

  if (!name)
    return 0;
  len= strlen(name);

  if (incl_user_coll.n_users)
    return coll_search(&incl_user_coll, name, len) != 0;

  if (excl_user_coll.n_users)
    return coll_search(&excl_user_coll, name, len) == 0;

  return 1;
}


static int get_next_word(const char *query, char *word)
{
  int len= 0;
  char c;
  while ((c= query[len]))
  {
    if (c >= 'a' && c <= 'z')
      word[len]= 'A' + (c-'a');
    else if (c >= 'A' && c <= 'Z')
      word[len]= c;
    else
      break;

    if (len++ == MAX_KEYWORD)
      return 0;
  }
  word[len]= 0;
  return len;
}


static int filter_query_type(const char *query, struct sa_keyword *kwd)
{
  int qwe_in_list;
  char fword[MAX_KEYWORD + 1], nword[MAX_KEYWORD + 1];
  int len, nlen= 0;
  const struct sa_keyword *l_keywords;

  while (*query && (is_space(*query) || *query == '(' || *query == '/'))
  {
    /* comment handling */
    if (*query == '/' && query[1] == '*')
    {
      if (query[2] == '!')
      {
        query+= 3;
        while (*query >= '0' && *query <= '9')
          query++;
        continue;
      }
      query+= 2;
      while (*query)
      {
        if (*query=='*' && query[1] == '/')
        {
          query+= 2;
          break;
        }
        query++;
      }
      continue;
    }
    query++;
  }

  qwe_in_list= 0;
  if (!(len= get_next_word(query, fword)))
    goto not_in_list;
  query+= len+1;

  l_keywords= kwd;
  while (l_keywords->length)
  {
    if (l_keywords->length == len && strncmp(l_keywords->wd, fword, len) == 0)
    {
      if (l_keywords->next)
      {
        if (nlen == 0)
        {
          while (*query && is_space(*query))
            query++;
          nlen= get_next_word(query, nword);
        }
        if (l_keywords->next->length != nlen ||
            strncmp(l_keywords->next->wd, nword, nlen) != 0)
          goto do_loop;
      }

      qwe_in_list= l_keywords->type;
      break;
    };
do_loop:
    l_keywords++;
  }

not_in_list:
  return qwe_in_list;
}


static int log_statement_ex(const struct connection_info *cn,
                            time_t ev_time, unsigned long thd_id,
                            const char *query, unsigned int query_len,
                            int error_code, const char *type)
{
  size_t csize;
  char message_loc[1024];
  char *message= message_loc;
  size_t message_size= sizeof(message_loc);
  char *uh_buffer;
  size_t uh_buffer_size;
  const char *db;
  unsigned int db_length;
  long long query_id;
  int result;

  if ((db= cn->db))
    db_length= cn->db_length;
  else
  {
    db= "";
    db_length= 0;
  }

  if (!(query_id= cn->query_id))
    query_id= query_counter++;

  if (query == 0)
  {
    /* Can happen after the error in mysqld_prepare_stmt() */
    query= cn->query;
    query_len= cn->query_length;
    if (query == 0 || query_len == 0)
      return 0;
  }

  if (query && !(events & EVENT_QUERY_ALL) &&
      (events & EVENT_QUERY))
  {
    const char *orig_query= query;

    if (filter_query_type(query, keywords_to_skip))
    {
      char fword[MAX_KEYWORD + 1];
      int len;
      do
      {
        len= get_next_word(query, fword);
        query+= len ? len : 1;
        if (len == 3 && strncmp(fword, "FOR", 3) == 0)
          break;
      } while (*query);

      if (*query == 0)
        return 0;
    }

    if (events & EVENT_QUERY_DDL)
    {
      if (!filter_query_type(query, not_ddl_keywords) &&
          filter_query_type(query, ddl_keywords))
        goto do_log_query;
    }
    if (events & EVENT_QUERY_DML)
    {
      if (filter_query_type(query, dml_keywords))
        goto do_log_query;
    }
    if (events & EVENT_QUERY_DCL)
    {
      if (filter_query_type(query, dcl_keywords))
        goto do_log_query;
    }

    return 0;
do_log_query:
    query= orig_query;
  }

  csize= log_header(message, message_size-1, &ev_time,
                    servhost, servhost_len,
                    cn->user, cn->user_length,cn->host, cn->host_length,
                    cn->ip, cn->ip_length, thd_id, query_id, type);

  csize+= my_snprintf(message+csize, message_size - 1 - csize,
      ",%.*s,\'", db_length, db);

  if (query_log_limit > 0 && query_len > query_log_limit)
    query_len= query_log_limit;

  if (query_len > (message_size - csize)/2)
  {
    flogger_mutex_lock(&lock_bigbuffer);
    if (big_buffer_alloced < (query_len * 2 + csize))
    {
      big_buffer_alloced= (query_len * 2 + csize + 4095) & ~4095L;
      big_buffer= realloc(big_buffer, big_buffer_alloced);
      if (big_buffer == NULL)
      {
        big_buffer_alloced= 0;
        return 0;
      }
    }

    memcpy(big_buffer, message, csize);
    message= big_buffer;
    message_size= big_buffer_alloced;
  }

  uh_buffer= message + csize;
  uh_buffer_size= message_size - csize;
  if (query_log_limit > 0 && uh_buffer_size > query_log_limit+2)
    uh_buffer_size= query_log_limit+2;

  switch (filter_query_type(query, passwd_keywords))
  {
    case SQLCOM_GRANT:
    case SQLCOM_CREATE_USER:
      csize+= escape_string_hide_passwords(query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "IDENTIFIED", 10, "BY", 2, 0);
      break;
    case SQLCOM_CHANGE_MASTER:
      csize+= escape_string_hide_passwords(query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "MASTER_PASSWORD", 15, "=", 1, 0);
      break;
    case SQLCOM_CREATE_SERVER:
    case SQLCOM_ALTER_SERVER:
      csize+= escape_string_hide_passwords(query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "PASSWORD", 8, NULL, 0, 0);
      break;
    case SQLCOM_SET_OPTION:
      csize+= escape_string_hide_passwords(query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "=", 1, NULL, 0, 1);
      break;
    default:
      csize+= escape_string(query, query_len,
                               uh_buffer, uh_buffer_size); 
      break;
  }
  csize+= my_snprintf(message+csize, message_size - 1 - csize,
                      "\',%d", error_code);
  message[csize]= '\n';
  result= write_log(message, csize + 1);
  if (message == big_buffer)
    flogger_mutex_unlock(&lock_bigbuffer);

  return result;
}


static int log_statement(const struct connection_info *cn,
                         const struct mysql_event_general *event,
                         const char *type)
{
  return log_statement_ex(cn, event->general_time, event->general_thread_id,
                          event->general_query, event->general_query_length,
                          event->general_error_code, type);
}


static int log_table(const struct connection_info *cn,
                     const struct mysql_event_table *event, const char *type)
{
  size_t csize;
  char message[1024];
  time_t ctime;

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    event->user, SAFE_STRLEN(event->user),
                    event->host, SAFE_STRLEN(event->host),
                    event->ip, SAFE_STRLEN(event->ip),
                    event->thread_id, cn->query_id, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
            ",%.*s,%.*s,",event->database_length, event->database,
                          event->table_length, event->table);
  message[csize]= '\n';
  return write_log(message, csize + 1);
}


static int log_rename(const struct connection_info *cn,
                      const struct mysql_event_table *event)
{
  size_t csize;
  char message[1024];
  time_t ctime;

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    event->user, SAFE_STRLEN(event->user),
                    event->host, SAFE_STRLEN(event->host),
                    event->ip, SAFE_STRLEN(event->ip),
                    event->thread_id, cn->query_id, "RENAME");
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
            ",%.*s,%.*s|%.*s.%.*s,",event->database_length, event->database,
                         event->table_length, event->table,
                         event->new_database_length, event->new_database,
                         event->new_table_length, event->new_table);
  message[csize]= '\n';
  return write_log(message, csize + 1);
}


static int event_query_command(const struct mysql_event_general *event)
{
  return (event->general_command_length == 5 &&
           strncmp(event->general_command, "Query", 5) == 0) ||
         (event->general_command_length == 7 &&
           (strncmp(event->general_command, "Execute", 7) == 0 ||
             (event->general_error_code != 0 &&
              strncmp(event->general_command, "Prepare", 7) == 0)));
}


static void update_general_user(struct connection_info *cn,
    const struct mysql_event_general *event)
{
  char uh_buffer[768];
  size_t user_len, host_len, ip_len;
  if (cn->user_length == 0 && cn->host_length == 0 && cn->ip_length == 0 &&
      get_user_host(event->general_user, event->general_user_length,
                    uh_buffer, sizeof(uh_buffer),
                    &user_len, &host_len, &ip_len) == 0)
  {
    get_str_n(cn->user, &cn->user_length, sizeof(cn->user), 
              uh_buffer, user_len);
    get_str_n(cn->host, &cn->host_length, sizeof(cn->host), 
              uh_buffer+user_len+1, host_len);
    get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip), 
              uh_buffer+user_len+1+host_len+1, ip_len);
  }

}


static struct connection_info ci_disconnect_buffer;

#define AA_FREE_CONNECTION 1
#define AA_CHANGE_USER 2

static void update_connection_info(struct connection_info *cn,
    unsigned int event_class, const void *ev, int *after_action)
{
  *after_action= 0;

  switch (event_class) {
  case MYSQL_AUDIT_GENERAL_CLASS:
  {
    const struct mysql_event_general *event =
      (const struct mysql_event_general *) ev;
    switch (event->event_subclass) {
      case MYSQL_AUDIT_GENERAL_LOG:
      {
        int init_db_command= event->general_command_length == 7 &&
          strncmp(event->general_command, "Init DB", 7) == 0;
        if (!ci_needs_setup(cn))
        {
          if (init_db_command)
          {
            /* Change DB */
            if (mysql_57_started)
              get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                  event->database, event->database_length);
            else
              get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                  event->general_query, event->general_query_length);
          }
          cn->query_id= mode ? query_counter++ : event->query_id;
          cn->query= event->general_query;
          cn->query_length= event->general_query_length;
          cn->query_time= (time_t) event->general_time;
          update_general_user(cn, event);
        }
        else if (init_db_command)
          setup_connection_initdb(cn, event);
        else if (event_query_command(event))
          setup_connection_query(cn, event);
        else
          setup_connection_simple(cn);
        break;
      }

      case MYSQL_AUDIT_GENERAL_STATUS:
        if (event_query_command(event))
        {
          if (ci_needs_setup(cn))
            setup_connection_query(cn, event);

          if (mode == 0 && cn->db_length == 0 && event->database_length > 0)
            get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                      event->database, event->database_length);

          if (event->general_error_code == 0)
          {
            /* We need to check if it's the USE command to change the DB */
            int use_command= event->general_query_length > 4 &&
              strncasecmp(event->general_query, "use ", 4) == 0;
            if (use_command)
            {
              /* Change DB */
              if (mode)
                get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                    event->general_query + 4, event->general_query_length - 4);
              else
                get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                    event->database, event->database_length);
            }
          }
          update_general_user(cn, event);
        }
        break;
      case MYSQL_AUDIT_GENERAL_ERROR:
        /*
          We need this because the MariaDB returns NULL query field for the
          MYSQL_AUDIT_GENERAL_STATUS in the mysqld_stmt_prepare.
          As a result we get empty QUERY field for errors.
        */
        if (ci_needs_setup(cn))
          setup_connection_query(cn, event);
        cn->query_id= mode ? query_counter++ : event->query_id;
        get_str_n(cn->query_buffer, &cn->query_length, sizeof(cn->query_buffer),
            event->general_query, event->general_query_length);
        cn->query= cn->query_buffer;
        cn->query_time= (time_t) event->general_time;
        break;
      default:;
    }
    break;
  }
  case MYSQL_AUDIT_TABLE_CLASS:
  {
    const struct mysql_event_table *event =
      (const struct mysql_event_table *) ev;
    if (ci_needs_setup(cn))
      setup_connection_table(cn, event);

    if (cn->user_length == 0 && cn->host_length == 0 && cn->ip_length == 0)
    {
      get_str_n(cn->user, &cn->user_length, sizeof(cn->user),
                event->user, SAFE_STRLEN(event->user));
      get_str_n(cn->host, &cn->host_length, sizeof(cn->host),
                event->host, SAFE_STRLEN(event->host));
      get_str_n(cn->ip, &cn->ip_length, sizeof(cn->ip),
                event->ip, SAFE_STRLEN(event->ip));
    }

    if (cn->db_length == 0 && event->database_length != 0)
      get_str_n(cn->db, &cn->db_length, sizeof(cn->db),
                event->database, event->database_length);

    if (mode == 0)
      cn->query_id= event->query_id;
    break;
  }
  case MYSQL_AUDIT_CONNECTION_CLASS:
  {
    const struct mysql_event_connection *event =
      (const struct mysql_event_connection *) ev;
    switch (event->event_subclass)
    {
      case MYSQL_AUDIT_CONNECTION_CONNECT:
        setup_connection_connect(cn, event);
        break;
      case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
        *after_action= AA_CHANGE_USER;
        break;
      default:;
    }
    break;
  }
  default:
    break;
  }
}


struct connection_info cn_error_buffer;


#define FILTER(MASK) (events == 0 || (events & MASK))
void auditing(MYSQL_THD thd, unsigned int event_class, const void *ev)
{
  struct connection_info *cn= 0;
  int after_action= 0;

  /* That one is important as this function can be called with      */
  /* &lock_operations locked when the server logs an error reported */
  /* by this plugin.                                                */
  if (internal_stop_logging)
    return;

  flogger_mutex_lock(&lock_operations);

  if (maria_55_started && debug_server_started &&
      event_class == MYSQL_AUDIT_GENERAL_CLASS)
  {
    /*
      There's a bug in MariaDB 5.5 that prevents using thread local
      variables in some cases.
      The 'select * from notexisting_table;' query produces such case.
      So just use the static buffer in this case.
    */
    const struct mysql_event_general *event =
      (const struct mysql_event_general *) ev;

    if (event->event_subclass == MYSQL_AUDIT_GENERAL_ERROR ||
        (event->event_subclass == MYSQL_AUDIT_GENERAL_STATUS && 
         event->general_query_length == 0 &&
         cn_error_buffer.query_id == event->query_id))
    {
      cn= &cn_error_buffer;
      cn->header= 1;
    }
    else
      cn= get_loc_info(thd);
  }
  else
  {
    cn= get_loc_info(thd);
  }

  update_connection_info(cn, event_class, ev, &after_action);

  if (!logging)
    goto exit_func;

  if (event_class == MYSQL_AUDIT_GENERAL_CLASS && FILTER(EVENT_QUERY) &&
      cn && do_log_user(cn->user))
  {
    const struct mysql_event_general *event =
      (const struct mysql_event_general *) ev;

    /*
      Only one subclass is logged.
    */
    if (event->event_subclass == MYSQL_AUDIT_GENERAL_STATUS &&
        event_query_command(event))
    {
      log_statement(cn, event, "QUERY");
    }
  }
  else if (event_class == MYSQL_AUDIT_TABLE_CLASS && FILTER(EVENT_TABLE) && cn)
  {
    const struct mysql_event_table *event =
      (const struct mysql_event_table *) ev;
    if (do_log_user(event->user))
    {
      switch (event->event_subclass)
      {
        case MYSQL_AUDIT_TABLE_LOCK:
          log_table(cn, event, event->read_only ? "READ" : "WRITE");
          break;
        case MYSQL_AUDIT_TABLE_CREATE:
          log_table(cn, event, "CREATE");
          break;
        case MYSQL_AUDIT_TABLE_DROP:
          log_table(cn, event, "DROP");
          break;
        case MYSQL_AUDIT_TABLE_RENAME:
          log_rename(cn, event);
          break;
        case MYSQL_AUDIT_TABLE_ALTER:
          log_table(cn, event, "ALTER");
          break;
        default:
          break;
      }
    }
  }
  else if (event_class == MYSQL_AUDIT_CONNECTION_CLASS &&
           FILTER(EVENT_CONNECT) && cn)
  {
    const struct mysql_event_connection *event =
      (const struct mysql_event_connection *) ev;
    switch (event->event_subclass)
    {
      case MYSQL_AUDIT_CONNECTION_CONNECT:
        log_connection(cn, event, event->status ? "FAILED_CONNECT": "CONNECT");
        break;
      case MYSQL_AUDIT_CONNECTION_DISCONNECT:
        if (use_event_data_for_disconnect)
          log_connection_event(event, "DISCONNECT");
        else
          log_connection(&ci_disconnect_buffer, event, "DISCONNECT");
        break;
      case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
        log_connection(cn, event, "CHANGEUSER");
        break;
      default:;
    }
  }
exit_func:
  /*
    This must work always, whether logging is ON or not.
  */
  if (after_action)
  {
    switch (after_action) {
    case AA_CHANGE_USER:
    {
      const struct mysql_event_connection *event =
        (const struct mysql_event_connection *) ev;
      change_connection(cn, event);
      break;
    }
    default:
      break;
    }
  }
  if (cn)
    cn->log_always= 0;
  flogger_mutex_unlock(&lock_operations);
}


struct mysql_event_general_v8
{
  unsigned int event_class;
  unsigned int event_subclass;
  int general_error_code;
  unsigned long general_thread_id;
  const char *general_user;
  unsigned int general_user_length;
  const char *general_command;
  unsigned int general_command_length;
  const char *general_query;
  unsigned int general_query_length;
  struct charset_info_st *general_charset;
  unsigned long long general_time;
  unsigned long long general_rows;
};


static void auditing_v8(MYSQL_THD thd, struct mysql_event_general_v8 *ev_v8)
{
#ifdef DBUG_OFF
  #ifdef __x86_64__
  static const int cmd_off= 4200;
  static const int db_off= 120;
  static const int db_len_off= 128;
  #else
  static const int cmd_off= 2668;
  static const int db_off= 60;
  static const int db_len_off= 64;
  #endif /*x86_64*/
#else
  #ifdef __x86_64__
  static const int cmd_off= 4432;
  static const int db_off= 120;
  static const int db_len_off= 128;
  #else
  static const int cmd_off= 2808;
  static const int db_off= 64;
  static const int db_len_off= 68;
  #endif /*x86_64*/
#endif /*DBUG_OFF*/

  struct mysql_event_general event;

  if (ev_v8->event_class != MYSQL_AUDIT_GENERAL_CLASS)
    return;

  event.event_subclass= ev_v8->event_subclass;
  event.general_error_code= ev_v8->general_error_code;
  event.general_thread_id= ev_v8->general_thread_id;
  event.general_user= ev_v8->general_user;
  event.general_user_length= ev_v8->general_user_length;
  event.general_command= ev_v8->general_command;
  event.general_command_length= ev_v8->general_command_length;
  event.general_query= ev_v8->general_query;
  event.general_query_length= ev_v8->general_query_length;
  event.general_charset= ev_v8->general_charset;
  event.general_time= ev_v8->general_time;
  event.general_rows= ev_v8->general_rows;
  event.database= 0;
  event.database_length= 0;

  if (event.general_query_length > 0)
  {
    event.event_subclass= MYSQL_AUDIT_GENERAL_STATUS;
    event.general_command= "Query";
    event.general_command_length= 5;
#ifdef __linux__
    event.database= *(char **) (((char *) thd) + db_off);
    event.database_length= *(size_t *) (((char *) thd) + db_len_off);
#endif /*__linux*/
  }
#ifdef __linux__
  else if (*((int *) (((char *)thd) + cmd_off)) == 2)
  {
    event.event_subclass= MYSQL_AUDIT_GENERAL_LOG;
    event.general_command= "Init DB";
    event.general_command_length= 7;
    event.general_query= *(char **) (((char *) thd) + db_off);
    event.general_query_length= *(size_t *) (((char *) thd) + db_len_off);
  }
#endif /*__linux*/
  auditing(thd, ev_v8->event_class, &event);
}


static void auditing_v13(MYSQL_THD thd, unsigned int *ev_v0)
{
  struct mysql_event_general event= *(const struct mysql_event_general *) (ev_v0+1);

  if (event.general_query_length > 0)
  {
    event.event_subclass= MYSQL_AUDIT_GENERAL_STATUS;
    event.general_command= "Query";
    event.general_command_length= 5;
  }
  auditing(thd, ev_v0[0], &event);
}


int get_db_mysql57(MYSQL_THD thd, char **name, int *len)
{
  int db_off;
  int db_len_off;
  if (debug_server_started)
  {
#ifdef __x86_64__
    db_off= 608;
    db_len_off= 616;
#else
    db_off= 0;
    db_len_off= 0;
#endif /*x86_64*/
  }
  else
  {
#ifdef __x86_64__
    db_off= 536;
    db_len_off= 544;
#else
    db_off= 0;
    db_len_off= 0;
#endif /*x86_64*/
  }

#ifdef __linux__
  *name= *(char **) (((char *) thd) + db_off);
  *len= *((int *) (((char*) thd) + db_len_off));
  if (*name && (*name)[*len] != 0)
    return 1;
  return 0;
#else
  return 1;
#endif
}
/*
   As it's just too difficult to #include "sql_class.h",
   let's just copy the necessary part of the system_variables
   structure here.
*/
typedef struct loc_system_variables
{
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint dynamic_variables_head;    /* largest valid variable offset */
  uint dynamic_variables_size;    /* how many bytes are in use */
  
  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  ulonglong optimizer_switch;
  ulonglong sql_mode; ///< which non-standard SQL behaviour should be enabled
  ulonglong option_bits; ///< OPTION_xxx constants, e.g. OPTION_PROFILING
  ulonglong join_buff_space_limit;
  ulonglong log_slow_filter; 
  ulonglong log_slow_verbosity; 
  ulonglong bulk_insert_buff_size;
  ulonglong join_buff_size;
  ulonglong sortbuff_size;
  ulonglong group_concat_max_len;
  ha_rows select_limit;
  ha_rows max_join_size;
  ha_rows expensive_subquery_limit;
  ulong auto_increment_increment, auto_increment_offset;
  ulong lock_wait_timeout;
  ulong join_cache_level;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong multi_range_count;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong mrr_buff_size;
  ulong div_precincrement;
  /* Total size of all buffers used by the subselect_rowid_merge_engine. */
  ulong rowid_merge_buff_size;
  ulong max_sp_recursion_depth;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;
  /* Flags for slow log filtering */
  ulong log_slow_rate_limit; 
  ulong binlog_format; ///< binlog format for this thd (see enum_binlog_format)
  ulong progress_report_time;
  my_bool binlog_annotate_row_events;
  my_bool binlog_direct_non_trans_update;
  my_bool sql_log_bin;
  ulong completion_type;
  ulong query_cache_type;
} LOC_SV;


static int server_audit_init(void *p __attribute__((unused)))
{
  const void *my_hash_init_ptr;

  if (!serv_ver)
  {
#ifdef _WIN32
    serv_ver= (const char *) GetProcAddress(0, "server_version");
#else
    serv_ver= server_version;
#endif /*_WIN32*/
  }
  my_hash_init_ptr= dlsym(RTLD_DEFAULT, "_my_hash_init");
  if (!my_hash_init_ptr)
  {
    maria_above_5= 1;
    my_hash_init_ptr= dlsym(RTLD_DEFAULT, "my_hash_init2");
  }

  if(!(int_mysql_data_home= dlsym(RTLD_DEFAULT, "mysql_data_home")))
  {
    if(!(int_mysql_data_home= dlsym(RTLD_DEFAULT, "?mysql_data_home@@3PADA")))
      int_mysql_data_home= &default_home;
  }

  if (!serv_ver || !my_hash_init_ptr)
    return 1;

  if (!started_mysql)
  {
    if (!maria_above_5 && serv_ver[4]=='3' && serv_ver[5]<'3')
    {
      mode= 1;
      mode_readonly= 1;
    }
  }

  if (gethostname(servhost, sizeof(servhost)))
    strcpy(servhost, "unknown");

  servhost_len= strlen(servhost);

  logger_init_mutexes();
#if defined(HAVE_PSI_INTERFACE) && !defined(FLOGGER_NO_PSI)
  if (PSI_server)
    PSI_server->register_mutex("server_audit", mutex_key_list, 1);
#endif
  flogger_mutex_init(key_LOCK_operations, &lock_operations, MY_MUTEX_INIT_FAST);
  flogger_mutex_init(key_LOCK_operations, &lock_bigbuffer, MY_MUTEX_INIT_FAST);

  coll_init(&incl_user_coll);
  coll_init(&excl_user_coll);

  if (incl_users)
  {
    if (excl_users)
    {
      incl_users= excl_users= NULL;
      error_header();
      fprintf(stderr, "INCL_DML_USERS and EXCL_DML_USERS specified"
                      " simultaneously - both set to empty\n");
    }
    update_incl_users(NULL, NULL, NULL, &incl_users);
  }
  else if (excl_users)
  {
    update_excl_users(NULL, NULL, NULL, &excl_users);
  }

  error_header();
  fprintf(stderr, "MariaDB Audit Plugin version %s%s STARTED.\n",
          PLUGIN_STR_VERSION, PLUGIN_DEBUG_VERSION);

  /* The Query Cache shadows TABLE events if the result is taken from it */
  /* so we warn users if both Query Cashe and TABLE events enabled.      */
  if (!started_mysql && FILTER(EVENT_TABLE))
  {
    ulonglong *qc_size= (ulonglong *) dlsym(RTLD_DEFAULT, "query_cache_size");
    if (qc_size == NULL || *qc_size != 0)
    {
      struct loc_system_variables *g_sys_var=
        (struct loc_system_variables *) dlsym(RTLD_DEFAULT,
                                          "global_system_variables");
      if (g_sys_var && g_sys_var->query_cache_type != 0)
      {
        error_header();
        fprintf(stderr, "Query cache is enabled with the TABLE events."
                        " Some table reads can be veiled.");
      }
    }
  }

  ci_disconnect_buffer.header= 10;
  ci_disconnect_buffer.thread_id= 0;
  ci_disconnect_buffer.query_id= 0;
  ci_disconnect_buffer.db_length= 0;
  ci_disconnect_buffer.user_length= 0;
  ci_disconnect_buffer.host_length= 0;
  ci_disconnect_buffer.ip_length= 0;
  ci_disconnect_buffer.query= empty_str;
  ci_disconnect_buffer.query_length= 0;

  if (logging)
    start_logging();

  return 0;
}


static int server_audit_init_mysql(void *p)
{
  started_mysql= 1;
  mode= 1;
  mode_readonly= 1;
  return server_audit_init(p);
}


static int server_audit_deinit(void *p __attribute__((unused)))
{
  coll_free(&incl_user_coll);
  coll_free(&excl_user_coll);

  if (output_type == OUTPUT_FILE && logfile)
    logger_close(logfile);
  else if (output_type == OUTPUT_SYSLOG)
    closelog();

  (void) free(big_buffer);
  flogger_mutex_destroy(&lock_operations);
  flogger_mutex_destroy(&lock_bigbuffer);

  error_header();
  fprintf(stderr, "STOPPED\n");
  return 0;
}


static void rotate_log(MYSQL_THD thd  __attribute__((unused)),
                       struct st_mysql_sys_var *var  __attribute__((unused)),
                       void *var_ptr  __attribute__((unused)),
                       const void *save  __attribute__((unused)))
{
  if (output_type == OUTPUT_FILE && logfile && *(my_bool*) save)
    (void) logger_rotate(logfile);
}


static struct st_mysql_audit mysql_descriptor =
{
  MYSQL_AUDIT_INTERFACE_VERSION,
  NULL,
  auditing,
  { MYSQL_AUDIT_GENERAL_CLASSMASK | MYSQL_AUDIT_CONNECTION_CLASSMASK }
};


mysql_declare_plugin(server_audit)
{
  MYSQL_AUDIT_PLUGIN,
  &mysql_descriptor,
  "SERVER_AUDIT",
  " Alexey Botchkov (MariaDB Corporation)",
  "Audit the server activity",
  PLUGIN_LICENSE_GPL,
  server_audit_init_mysql,
  server_audit_deinit,
  PLUGIN_VERSION,
  audit_status,
  vars,
  NULL,
  0
}
mysql_declare_plugin_end;


static struct st_mysql_audit maria_descriptor =
{
  MYSQL_AUDIT_INTERFACE_VERSION,
  NULL,
  auditing,
  { MYSQL_AUDIT_GENERAL_CLASSMASK |
    MYSQL_AUDIT_TABLE_CLASSMASK |
    MYSQL_AUDIT_CONNECTION_CLASSMASK }
};
maria_declare_plugin(server_audit)
{
  MYSQL_AUDIT_PLUGIN,
  &maria_descriptor,
  "SERVER_AUDIT",
  "Alexey Botchkov (MariaDB Corporation)",
  "Audit the server activity",
  PLUGIN_LICENSE_GPL,
  server_audit_init,
  server_audit_deinit,
  PLUGIN_VERSION,
  audit_status,
  vars,
  PLUGIN_STR_VERSION,
  MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;


static void mark_always_logged(MYSQL_THD thd)
{
  struct connection_info *cn;
  if (thd && (cn= get_loc_info(thd)))
    cn->log_always= 1;
}


static void log_current_query(MYSQL_THD thd)
{
  struct connection_info *cn;
  if (!thd)
    return;
  cn= get_loc_info(thd);
  if (!ci_needs_setup(cn) && FILTER(EVENT_QUERY) && do_log_user(cn->user))
  {
    log_statement_ex(cn, cn->query_time, thd_get_thread_id(thd),
        cn->query, cn->query_length, 0, "QUERY");
    cn->log_always= 1;
  }
}


static void update_file_path(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_name= (*(char **) save) ? *(char **) save : empty_str;

  if (!maria_55_started || !debug_server_started)
    flogger_mutex_lock(&lock_operations);
  internal_stop_logging= 1;
  error_header();
  fprintf(stderr, "Log file name was changed to '%s'.\n", new_name);

  if (logging)
    log_current_query(thd);

  if (logging && output_type == OUTPUT_FILE)
  {
    char *sav_path= file_path;

    file_path= new_name;
    internal_stop_logging= 1;
    stop_logging();
    if (start_logging())
    {
      file_path= sav_path;
      error_header();
      fprintf(stderr, "Reverting log filename back to '%s'.\n", file_path);
      logging= (start_logging() == 0);
      if (!logging)
      {
        error_header();
        fprintf(stderr, "Logging was disabled..\n");
        CLIENT_ERROR(1, "Logging was disabled.", MYF(ME_JUST_WARNING));
      }
      goto exit_func;
    }
    internal_stop_logging= 0;
  }

  strncpy(path_buffer, new_name, sizeof(path_buffer));
  file_path= path_buffer;
exit_func:
  internal_stop_logging= 0;
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_unlock(&lock_operations);
}


static void update_file_rotations(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  rotations= *(unsigned int *) save;
  error_header();
  fprintf(stderr, "Log file rotations was changed to '%d'.\n", rotations);

  if (!logging || output_type != OUTPUT_FILE)
    return;

  flogger_mutex_lock(&lock_operations);
  logfile->rotations= rotations;
  flogger_mutex_unlock(&lock_operations);
}


static void update_file_rotate_size(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  file_rotate_size= *(unsigned long long *) save;
  error_header();
  fprintf(stderr, "Log file rotate size was changed to '%lld'.\n",
          file_rotate_size);

  if (!logging || output_type != OUTPUT_FILE)
    return;

  flogger_mutex_lock(&lock_operations);
  logfile->size_limit= file_rotate_size;
  flogger_mutex_unlock(&lock_operations);
}


static void update_incl_users(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_users= (*(char **) save) ? *(char **) save : empty_str;
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_lock(&lock_operations);
  mark_always_logged(thd);
  strncpy(incl_user_buffer, new_users, sizeof(incl_user_buffer));
  incl_users= incl_user_buffer;
  user_coll_fill(&incl_user_coll, incl_users, &excl_user_coll, 1);
  error_header();
  fprintf(stderr, "server_audit_incl_users set to '%s'.\n", incl_users);
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_unlock(&lock_operations);
}


static void update_excl_users(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_users= (*(char **) save) ? *(char **) save : empty_str;
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_lock(&lock_operations);
  mark_always_logged(thd);
  strncpy(excl_user_buffer, new_users, sizeof(excl_user_buffer));
  excl_users= excl_user_buffer;
  user_coll_fill(&excl_user_coll, excl_users, &incl_user_coll, 0);
  error_header();
  fprintf(stderr, "server_audit_excl_users set to '%s'.\n", excl_users);
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_unlock(&lock_operations);
}


static void update_output_type(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_output_type= *((ulong *) save);
  if (output_type == new_output_type)
    return;

  flogger_mutex_lock(&lock_operations);
  internal_stop_logging= 1;
  if (logging)
  {
    log_current_query(thd);
    stop_logging();
  }

  output_type= new_output_type;
  error_header();
  fprintf(stderr, "Output was redirected to '%s'\n",
          output_type_names[output_type]);

  if (logging)
    start_logging();
  internal_stop_logging= 0;
  flogger_mutex_unlock(&lock_operations);
}


static void update_syslog_facility(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_facility= *((ulong *) save);
  if (syslog_facility == new_facility)
    return;

  mark_always_logged(thd);
  error_header();
  fprintf(stderr, "SysLog facility was changed from '%s' to '%s'.\n",
          syslog_facility_names[syslog_facility],
          syslog_facility_names[new_facility]);
  syslog_facility= new_facility;
}


static void update_syslog_priority(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_priority= *((ulong *) save);
  if (syslog_priority == new_priority)
    return;

  flogger_mutex_lock(&lock_operations);
  mark_always_logged(thd);
  flogger_mutex_unlock(&lock_operations);
  error_header();
  fprintf(stderr, "SysLog priority was changed from '%s' to '%s'.\n",
          syslog_priority_names[syslog_priority],
          syslog_priority_names[new_priority]);
  syslog_priority= new_priority;
}


static void update_logging(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char new_logging= *(char *) save;
  if (new_logging == logging)
    return;

  if (!maria_55_started || !debug_server_started)
    flogger_mutex_lock(&lock_operations);
  internal_stop_logging= 1;
  if ((logging= new_logging))
  {
    start_logging();
    if (!logging)
    {
      CLIENT_ERROR(1, "Logging was disabled.", MYF(ME_JUST_WARNING));
    }
  }
  else
  {
    log_current_query(thd);
    stop_logging();
  }

  internal_stop_logging= 0;
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_unlock(&lock_operations);
}


static void update_mode(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  unsigned int new_mode= *(unsigned int *) save;
  if (mode_readonly || new_mode == mode)
    return;

  if (!maria_55_started || !debug_server_started)
    flogger_mutex_lock(&lock_operations);
  internal_stop_logging= 1;
  mark_always_logged(thd);
  error_header();
  fprintf(stderr, "Logging mode was changed from %d to %d.\n", mode, new_mode);
  mode= new_mode;
  internal_stop_logging= 0;
  if (!maria_55_started || !debug_server_started)
    flogger_mutex_unlock(&lock_operations);
}


static void update_syslog_ident(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_ident= (*(char **) save) ? *(char **) save : empty_str;
  strncpy(syslog_ident_buffer, new_ident, sizeof(syslog_ident_buffer));
  syslog_ident= syslog_ident_buffer;
  error_header();
  fprintf(stderr, "SYSYLOG ident was changed to '%s'\n", syslog_ident);
  flogger_mutex_lock(&lock_operations);
  mark_always_logged(thd);
  if (logging && output_type == OUTPUT_SYSLOG)
  {
    stop_logging();
    start_logging();
  }
  flogger_mutex_unlock(&lock_operations);
}


struct st_my_thread_var *loc_thread_var(void)
{
  return 0;
}



#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  if (fdwReason != DLL_PROCESS_ATTACH)
    return 1;

  serv_ver= (const char *) GetProcAddress(0, "server_version");
#else
void __attribute__ ((constructor)) audit_plugin_so_init(void)
{
  serv_ver= server_version;
#endif /*_WIN32*/

  if (!serv_ver)
    goto exit;

  started_mariadb= strstr(serv_ver, "MariaDB") != 0;
  debug_server_started= strstr(serv_ver, "debug") != 0;

  if (started_mariadb)
  {
    if (serv_ver[0] == '1')
      use_event_data_for_disconnect= 1;
    else
      maria_55_started= 1;
  }
  else
  {
    /* Started MySQL. */
    if (serv_ver[0] == '5' && serv_ver[2] == '5')
    {
      int sc= serv_ver[4] - '0';
      if (serv_ver[5] >= '0' && serv_ver[5] <= '9')
        sc= sc * 10 + serv_ver[5] - '0';
      if (sc <= 10)
      {
        mysql_descriptor.interface_version= 0x0200;
        mysql_descriptor.event_notify= (void *) auditing_v8;
      }
      else if (sc < 14)
      {
        mysql_descriptor.interface_version= 0x0200;
        mysql_descriptor.event_notify= (void *) auditing_v13;
      }
    }
    else if (serv_ver[0] == '5' && serv_ver[2] == '6')
    {
      int sc= serv_ver[4] - '0';
      if (serv_ver[5] >= '0' && serv_ver[5] <= '9')
        sc= sc * 10 + serv_ver[5] - '0';
      if (sc >= 24)
        use_event_data_for_disconnect= 1;
    }
    else if (serv_ver[0] == '5' && serv_ver[2] == '7')
    {
      mysql_57_started= 1;
      _mysql_plugin_declarations_[0].info= mysql_v4_descriptor;
      use_event_data_for_disconnect= 1;
    }
    MYSQL_SYSVAR_NAME(loc_info).flags= PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC;
  }

  memset(locinfo_ini_value, 'O', sizeof(locinfo_ini_value)-1);
  locinfo_ini_value[sizeof(locinfo_ini_value)-1]= 0;

exit:
#ifdef _WIN32
  return 1;
#else
  return;
#endif
}

