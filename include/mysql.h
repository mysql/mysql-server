/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#ifndef _mysql_h
#define _mysql_h

#ifdef __CYGWIN__     /* CYGWIN implements a UNIX API */
#undef WIN
#undef _WIN
#undef _WIN32
#undef _WIN64
#undef __WIN__
#endif

#ifdef	__cplusplus
extern "C" {
#endif
  
#ifndef _global_h				/* If not standard header */
#include <sys/types.h>
#ifdef __LCC__
#include <winsock.h>				/* For windows */
#endif
typedef char my_bool;
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__WIN__)
#define __WIN__
#endif
#if !defined(__WIN__)
#define STDCALL
#else
#define STDCALL __stdcall
#endif
typedef char * gptr;

#ifndef my_socket_defined
#ifdef __WIN__
#define my_socket SOCKET
#else
typedef int my_socket;
#endif /* __WIN__ */
#endif /* my_socket_defined */
#endif /* _global_h */

#include "mysql_com.h"
#include "mysql_version.h"

extern unsigned int mysql_port;
extern char *mysql_unix_port;

#define IS_PRI_KEY(n)	((n) & PRI_KEY_FLAG)
#define IS_NOT_NULL(n)	((n) & NOT_NULL_FLAG)
#define IS_BLOB(n)	((n) & BLOB_FLAG)
#define IS_NUM(t)	((t) <= FIELD_TYPE_INT24 || (t) == FIELD_TYPE_YEAR)
#define IS_NUM_FIELD(f)	 ((f)->flags & NUM_FLAG)
#define INTERNAL_NUM_FIELD(f) (((f)->type <= FIELD_TYPE_INT24 && ((f)->type != FIELD_TYPE_TIMESTAMP || (f)->length == 14 || (f)->length == 8)) || (f)->type == FIELD_TYPE_YEAR)

typedef struct st_mysql_field {
  char *name;			/* Name of column */
  char *table;			/* Table of column if column was a field */
  char *org_table;		/* Org table name if table was an alias */
  char *db;			/* Database for table */
  char *def;			/* Default value (set by mysql_list_fields) */
  unsigned long length;		/* Width of column */
  unsigned long max_length;	/* Max width of selected set */
  unsigned int flags;		/* Div flags */
  unsigned int decimals;	/* Number of decimals in field */
  enum enum_field_types type;	/* Type of field. Se mysql_com.h for types */
} MYSQL_FIELD;

typedef char **MYSQL_ROW;		/* return data as array of strings */
typedef unsigned int MYSQL_FIELD_OFFSET; /* offset to current field */

#if defined(NO_CLIENT_LONG_LONG)
typedef unsigned long my_ulonglong;
#elif defined (__WIN__)
typedef unsigned __int64 my_ulonglong;
#else
typedef unsigned long long my_ulonglong;
#endif

#define MYSQL_COUNT_ERROR (~(my_ulonglong) 0)

typedef struct st_mysql_rows {
  struct st_mysql_rows *next;		/* list of rows */
  MYSQL_ROW data;
} MYSQL_ROWS;

typedef MYSQL_ROWS *MYSQL_ROW_OFFSET;	/* offset to current row */

#ifndef ST_USED_MEM_DEFINED
#define ST_USED_MEM_DEFINED
typedef struct st_used_mem {			/* struct for once_alloc */
  struct st_used_mem *next;			/* Next block in use */
  unsigned int	left;				/* memory left in block  */
  unsigned int	size;				/* size of block */
} USED_MEM;
typedef struct st_mem_root {
  USED_MEM *free;
  USED_MEM *used;
  USED_MEM *pre_alloc;
  unsigned int	min_malloc;
  unsigned int	block_size;

  void (*error_handler)(void);
} MEM_ROOT;
#endif

typedef struct st_mysql_data {
  my_ulonglong rows;
  unsigned int fields;
  MYSQL_ROWS *data;
  MEM_ROOT alloc;
} MYSQL_DATA;

struct st_mysql_options {
  unsigned int connect_timeout,client_flag;
  unsigned int port;
  char *host,*init_command,*user,*password,*unix_socket,*db;
  char *my_cnf_file,*my_cnf_group, *charset_dir, *charset_name;
  char *ssl_key;				/* PEM key file */
  char *ssl_cert;				/* PEM cert file */
  char *ssl_ca;					/* PEM CA file */
  char *ssl_capath;				/* PEM directory of CA-s? */
  char *ssl_cipher;				/* cipher to use */
  my_bool use_ssl;				/* if to use SSL or not */
  my_bool compress,named_pipe;
 /*
   on connect, find out the replication role of the server, and
   establish connections to all the peers
 */
  my_bool rpl_probe;
 /* 
    each call to mysql_real_query() will parse it to tell if it is a read
    or a write, and direct it to the slave or the master
 */
  my_bool rpl_parse;
 /*
   if set, never read from a master,only from slave, when doing
   a read that is replication-aware
 */
  my_bool no_master_reads;
};

enum mysql_option { MYSQL_OPT_CONNECT_TIMEOUT, MYSQL_OPT_COMPRESS,
		    MYSQL_OPT_NAMED_PIPE, MYSQL_INIT_COMMAND,
		    MYSQL_READ_DEFAULT_FILE, MYSQL_READ_DEFAULT_GROUP,
		    MYSQL_SET_CHARSET_DIR, MYSQL_SET_CHARSET_NAME};

enum mysql_status { MYSQL_STATUS_READY,MYSQL_STATUS_GET_RESULT,
		    MYSQL_STATUS_USE_RESULT};

/*
  There are three types of queries - the ones that have to go to
  the master, the ones that go to a slave, and the adminstrative
  type which must happen on the pivot connectioin
*/
enum mysql_rpl_type { MYSQL_RPL_MASTER, MYSQL_RPL_SLAVE,
		      MYSQL_RPL_ADMIN };
  

typedef struct st_mysql {
  NET		net;			/* Communication parameters */
  gptr		connector_fd;		/* ConnectorFd for SSL */
  char		*host,*user,*passwd,*unix_socket,*server_version,*host_info,
		*info,*db;
  struct charset_info_st *charset;
  MYSQL_FIELD	*fields;
  MEM_ROOT	field_alloc;
  my_ulonglong affected_rows;
  my_ulonglong insert_id;		/* id if insert on table with NEXTNR */
  my_ulonglong extra_info;		/* Used by mysqlshow */
  unsigned long thread_id;		/* Id for connection in server */
  unsigned long packet_length;
  unsigned int	port,client_flag,server_capabilities;
  unsigned int	protocol_version;
  unsigned int	field_count;
  unsigned int 	server_status;
  unsigned int  server_language;
  struct st_mysql_options options;
  enum mysql_status status;
  my_bool	free_me;		/* If free in mysql_close */
  my_bool	reconnect;		/* set to 1 if automatic reconnect */
  char	        scramble_buff[9];

 /*
   Set if this is the original connection, not a master or a slave we have
   added though mysql_rpl_probe() or mysql_set_master()/ mysql_add_slave()
 */
  my_bool rpl_pivot;
  /* pointers to the master, and the next slave
    connections, points to itself if lone connection  */
  struct st_mysql* master, *next_slave;
  
  struct st_mysql* last_used_slave; /* needed for round-robin slave pick */
 /* needed for send/read/store/use result to work correctly with replication */
  struct st_mysql* last_used_con;
} MYSQL;


typedef struct st_mysql_res {
  my_ulonglong row_count;
  MYSQL_FIELD	*fields;
  MYSQL_DATA	*data;
  MYSQL_ROWS	*data_cursor;
  unsigned long *lengths;		/* column lengths of current row */
  MYSQL		*handle;		/* for unbuffered reads */
  MEM_ROOT	field_alloc;
  unsigned int	field_count, current_field;
  MYSQL_ROW	row;			/* If unbuffered read */
  MYSQL_ROW	current_row;		/* buffer to current row */
  my_bool	eof;			/* Used by mysql_fetch_row */
} MYSQL_RES;

#define MAX_MYSQL_MANAGER_ERR 256  
#define MAX_MYSQL_MANAGER_MSG 256

#define MANAGER_OK           200
#define MANAGER_INFO         250
#define MANAGER_ACCESS       401
#define MANAGER_CLIENT_ERR   450
#define MANAGER_INTERNAL_ERR 500


  
typedef struct st_mysql_manager
{
  Vio* vio;
  char *host,*user,*passwd;
  unsigned int port;
  my_bool free_me;
  my_bool eof;
  int cmd_status;
  int last_errno;
  char* net_buf,*net_buf_pos,*net_data_end;
  int net_buf_size;
  char last_error[MAX_MYSQL_MANAGER_ERR];
} MYSQL_MANAGER;
  
/* Set up and bring down the server; to ensure that applications will
 * work when linked against either the standard client library or the
 * embedded server library, these functions should be called. */
int mysql_server_init(int argc, char **argv, char **groups);
void mysql_server_end(void);

/* Set up and bring down a thread; these function should be called
 * for each thread in an application which opens at least one MySQL
 * connection.  All uses of the connection(s) should be between these
 * function calls. */
my_bool mysql_thread_init(void);
void mysql_thread_end(void);

/* Functions to get information from the MYSQL and MYSQL_RES structures */
/* Should definitely be used if one uses shared libraries */

my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *res);
unsigned int STDCALL mysql_num_fields(MYSQL_RES *res);
my_bool STDCALL mysql_eof(MYSQL_RES *res);
MYSQL_FIELD *STDCALL mysql_fetch_field_direct(MYSQL_RES *res,
					      unsigned int fieldnr);
MYSQL_FIELD * STDCALL mysql_fetch_fields(MYSQL_RES *res);
MYSQL_ROWS * STDCALL mysql_row_tell(MYSQL_RES *res);
unsigned int STDCALL mysql_field_tell(MYSQL_RES *res);

unsigned int STDCALL mysql_field_count(MYSQL *mysql);
my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql);
my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql);
unsigned int STDCALL mysql_errno(MYSQL *mysql);
char * STDCALL mysql_error(MYSQL *mysql);
char * STDCALL mysql_info(MYSQL *mysql);
unsigned long STDCALL mysql_thread_id(MYSQL *mysql);
const char * STDCALL mysql_character_set_name(MYSQL *mysql);

MYSQL *		STDCALL mysql_init(MYSQL *mysql);
int		STDCALL mysql_ssl_set(MYSQL *mysql, const char *key,
				      const char *cert, const char *ca,
				      const char *capath, const char *cipher);
int		STDCALL mysql_ssl_clear(MYSQL *mysql);
my_bool		STDCALL mysql_change_user(MYSQL *mysql, const char *user, 
					  const char *passwd, const char *db);
MYSQL *		STDCALL mysql_real_connect(MYSQL *mysql, const char *host,
					   const char *user,
					   const char *passwd,
					   const char *db,
					   unsigned int port,
					   const char *unix_socket,
					   unsigned int clientflag);
void		STDCALL mysql_close(MYSQL *sock);
int		STDCALL mysql_select_db(MYSQL *mysql, const char *db);
int		STDCALL mysql_query(MYSQL *mysql, const char *q);
int		STDCALL mysql_send_query(MYSQL *mysql, const char *q,
					 unsigned long length);
int		STDCALL mysql_read_query_result(MYSQL *mysql);
int		STDCALL mysql_real_query(MYSQL *mysql, const char *q,
					unsigned long length);
/* perform query on master */
int		STDCALL mysql_master_query(MYSQL *mysql, const char *q,
					unsigned long length);
int		STDCALL mysql_master_send_query(MYSQL *mysql, const char *q,
					unsigned long length);
/* perform query on slave */  
int		STDCALL mysql_slave_query(MYSQL *mysql, const char *q,
					unsigned long length);
int		STDCALL mysql_slave_send_query(MYSQL *mysql, const char *q,
					unsigned long length);

/*
  enable/disable parsing of all queries to decide if they go on master or
  slave
*/
void            STDCALL mysql_enable_rpl_parse(MYSQL* mysql);
void            STDCALL mysql_disable_rpl_parse(MYSQL* mysql);
/* get the value of the parse flag */  
int             STDCALL mysql_rpl_parse_enabled(MYSQL* mysql);

/*  enable/disable reads from master */
void            STDCALL mysql_enable_reads_from_master(MYSQL* mysql);
void            STDCALL mysql_disable_reads_from_master(MYSQL* mysql);
/* get the value of the master read flag */  
int             STDCALL mysql_reads_from_master_enabled(MYSQL* mysql);

enum mysql_rpl_type     STDCALL mysql_rpl_query_type(const char* q, int len);  

/* discover the master and its slaves */  
int             STDCALL mysql_rpl_probe(MYSQL* mysql);
  
/* set the master, close/free the old one, if it is not a pivot */
int             STDCALL mysql_set_master(MYSQL* mysql, const char* host,
					 unsigned int port,
					 const char* user,
					 const char* passwd);
int             STDCALL mysql_add_slave(MYSQL* mysql, const char* host,
					unsigned int port,
					const char* user,
					const char* passwd);
  
int		STDCALL mysql_shutdown(MYSQL *mysql);
int		STDCALL mysql_dump_debug_info(MYSQL *mysql);
int		STDCALL mysql_refresh(MYSQL *mysql,
				     unsigned int refresh_options);
int		STDCALL mysql_kill(MYSQL *mysql,unsigned long pid);
int		STDCALL mysql_ping(MYSQL *mysql);
char *		STDCALL mysql_stat(MYSQL *mysql);
char *		STDCALL mysql_get_server_info(MYSQL *mysql);
char *		STDCALL mysql_get_client_info(void);
char *		STDCALL mysql_get_host_info(MYSQL *mysql);
unsigned int	STDCALL mysql_get_proto_info(MYSQL *mysql);
MYSQL_RES *	STDCALL mysql_list_dbs(MYSQL *mysql,const char *wild);
MYSQL_RES *	STDCALL mysql_list_tables(MYSQL *mysql,const char *wild);
MYSQL_RES *	STDCALL mysql_list_fields(MYSQL *mysql, const char *table,
					 const char *wild);
MYSQL_RES *	STDCALL mysql_list_processes(MYSQL *mysql);
MYSQL_RES *	STDCALL mysql_store_result(MYSQL *mysql);
MYSQL_RES *	STDCALL mysql_use_result(MYSQL *mysql);
int		STDCALL mysql_options(MYSQL *mysql,enum mysql_option option,
				      const char *arg);
void		STDCALL mysql_free_result(MYSQL_RES *result);
void		STDCALL mysql_data_seek(MYSQL_RES *result,
					my_ulonglong offset);
MYSQL_ROW_OFFSET STDCALL mysql_row_seek(MYSQL_RES *result, MYSQL_ROW_OFFSET);
MYSQL_FIELD_OFFSET STDCALL mysql_field_seek(MYSQL_RES *result,
					   MYSQL_FIELD_OFFSET offset);
MYSQL_ROW	STDCALL mysql_fetch_row(MYSQL_RES *result);
unsigned long * STDCALL mysql_fetch_lengths(MYSQL_RES *result);
MYSQL_FIELD *	STDCALL mysql_fetch_field(MYSQL_RES *result);
unsigned long	STDCALL mysql_escape_string(char *to,const char *from,
					    unsigned long from_length);
unsigned long STDCALL mysql_real_escape_string(MYSQL *mysql,
					       char *to,const char *from,
					       unsigned long length);
void		STDCALL mysql_debug(const char *debug);
char *		STDCALL mysql_odbc_escape_string(MYSQL *mysql,
						 char *to,
						 unsigned long to_length,
						 const char *from,
						 unsigned long from_length,
						 void *param,
						 char *
						 (*extend_buffer)
						 (void *, char *to,
						  unsigned long *length));
void 		STDCALL myodbc_remove_escape(MYSQL *mysql,char *name);
unsigned int	STDCALL mysql_thread_safe(void);
MYSQL_MANAGER*  STDCALL mysql_manager_init(MYSQL_MANAGER* con);  
MYSQL_MANAGER*  STDCALL mysql_manager_connect(MYSQL_MANAGER* con,
					      const char* host,
					      const char* user,
					      const char* passwd,
					      unsigned int port);
void            STDCALL mysql_manager_close(MYSQL_MANAGER* con);
int             STDCALL mysql_manager_command(MYSQL_MANAGER* con,
						const char* cmd, int cmd_len);
int             STDCALL mysql_manager_fetch_line(MYSQL_MANAGER* con,
						  char* res_buf,
						 int res_buf_size);
#define mysql_reload(mysql) mysql_refresh((mysql),REFRESH_GRANT)

#ifdef USE_OLD_FUNCTIONS
MYSQL *		STDCALL mysql_connect(MYSQL *mysql, const char *host,
				      const char *user, const char *passwd);
int		STDCALL mysql_create_db(MYSQL *mysql, const char *DB);
int		STDCALL mysql_drop_db(MYSQL *mysql, const char *DB);
#define	 mysql_reload(mysql) mysql_refresh((mysql),REFRESH_GRANT)
#define HAVE_MYSQL_REAL_CONNECT
#endif

/*
  The following functions are mainly exported because of mysqlbinlog;
  They are not for general usage
*/

int simple_command(MYSQL *mysql,enum enum_server_command command,
		   const char *arg, unsigned long length, my_bool skipp_check);
unsigned long net_safe_read(MYSQL* mysql);

#ifdef	__cplusplus
}
#endif

#endif /* _mysql_h */
