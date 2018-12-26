/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
** Common definition between mysql server & client
*/

#ifndef _mysql_com_h
#define _mysql_com_h
#include "binary_log_types.h"
#include "my_command.h"
#define HOSTNAME_LENGTH 60
#define SYSTEM_CHARSET_MBMAXLEN 3
#define FILENAME_CHARSET_MBMAXLEN 5
#define NAME_CHAR_LEN	64              /* Field/table name length */
#define USERNAME_CHAR_LENGTH 32
#define USERNAME_CHAR_LENGTH_STR "32"
#ifndef NAME_LEN
#define NAME_LEN                (NAME_CHAR_LEN*SYSTEM_CHARSET_MBMAXLEN)
#endif
#define USERNAME_LENGTH         (USERNAME_CHAR_LENGTH*SYSTEM_CHARSET_MBMAXLEN)
#define CONNECT_STRING_MAXLEN 1024

#define MYSQL_AUTODETECT_CHARSET_NAME "auto"

#define SERVER_VERSION_LENGTH 60
#define SQLSTATE_LENGTH 5

/*
  Maximum length of comments
*/
#define TABLE_COMMENT_INLINE_MAXLEN 180 /* pre 6.0: 60 characters */
#define TABLE_COMMENT_MAXLEN 2048
#define COLUMN_COMMENT_MAXLEN 1024
#define INDEX_COMMENT_MAXLEN 1024
#define TABLE_PARTITION_COMMENT_MAXLEN 1024

/*
  Maximum length of protocol packet.
  OK packet length limit also restricted to this value as any length greater
  than this value will have first byte of OK packet to be 254 thus does not
  provide a means to identify if this is OK or EOF packet.
*/
#define MAX_PACKET_LENGTH (256L*256L*256L-1)

/*
  USER_HOST_BUFF_SIZE -- length of string buffer, that is enough to contain
  username and hostname parts of the user identifier with trailing zero in
  MySQL standard format:
  user_name_part@host_name_part\0
*/
#define USER_HOST_BUFF_SIZE HOSTNAME_LENGTH + USERNAME_LENGTH + 2

#define LOCAL_HOST	"localhost"
#define LOCAL_HOST_NAMEDPIPE "."


#if defined(_WIN32)
#define MYSQL_NAMEDPIPE "MySQL"
#define MYSQL_SERVICENAME "MySQL"
#endif /* _WIN32 */

/* The length of the header part for each generated column in the .frm file. */
#define FRM_GCOL_HEADER_SIZE 4
/*
  Maximum length of the expression statement defined for generated columns.
*/
#define GENERATED_COLUMN_EXPRESSION_MAXLEN 65535 - FRM_GCOL_HEADER_SIZE
/*
  Length of random string sent by server on handshake; this is also length of
  obfuscated password, received from client
*/
#define SCRAMBLE_LENGTH 20
#define AUTH_PLUGIN_DATA_PART_1_LENGTH 8
/* length of password stored in the db: new passwords are preceeded with '*' */
#define SCRAMBLED_PASSWORD_CHAR_LENGTH (SCRAMBLE_LENGTH*2+1)


#define NOT_NULL_FLAG	1		/* Field can't be NULL */
#define PRI_KEY_FLAG	2		/* Field is part of a primary key */
#define UNIQUE_KEY_FLAG 4		/* Field is part of a unique key */
#define MULTIPLE_KEY_FLAG 8		/* Field is part of a key */
#define BLOB_FLAG	16		/* Field is a blob */
#define UNSIGNED_FLAG	32		/* Field is unsigned */
#define ZEROFILL_FLAG	64		/* Field is zerofill */
#define BINARY_FLAG	128		/* Field is binary   */

/* The following are only sent to new clients */
#define ENUM_FLAG	256		/* field is an enum */
#define AUTO_INCREMENT_FLAG 512		/* field is a autoincrement field */
#define TIMESTAMP_FLAG	1024		/* Field is a timestamp */
#define SET_FLAG	2048		/* field is a set */
#define NO_DEFAULT_VALUE_FLAG 4096	/* Field doesn't have default value */
#define ON_UPDATE_NOW_FLAG 8192         /* Field is set to NOW on UPDATE */
#define NUM_FLAG	32768		/* Field is num (for clients) */
#define PART_KEY_FLAG	16384		/* Intern; Part of some key */
#define GROUP_FLAG	32768		/* Intern: Group field */
#define UNIQUE_FLAG	65536		/* Intern: Used by sql_yacc */
#define BINCMP_FLAG	131072		/* Intern: Used by sql_yacc */
#define GET_FIXED_FIELDS_FLAG (1 << 18) /* Used to get fields in item tree */
#define FIELD_IN_PART_FUNC_FLAG (1 << 19)/* Field part of partition func */
/**
  Intern: Field in TABLE object for new version of altered table,
          which participates in a newly added index.
*/
#define FIELD_IN_ADD_INDEX (1 << 20)
#define FIELD_IS_RENAMED (1<< 21)       /* Intern: Field is being renamed */
#define FIELD_FLAGS_STORAGE_MEDIA 22    /* Field storage media, bit 22-23 */
#define FIELD_FLAGS_STORAGE_MEDIA_MASK (3 << FIELD_FLAGS_STORAGE_MEDIA)
#define FIELD_FLAGS_COLUMN_FORMAT 24    /* Field column format, bit 24-25 */
#define FIELD_FLAGS_COLUMN_FORMAT_MASK (3 << FIELD_FLAGS_COLUMN_FORMAT)
#define FIELD_IS_DROPPED (1<< 26)       /* Intern: Field is being dropped */
#define EXPLICIT_NULL_FLAG (1<< 27)     /* Field is explicitly specified as
                                           NULL by the user */

#define REFRESH_GRANT		1	/* Refresh grant tables */
#define REFRESH_LOG		2	/* Start on new log file */
#define REFRESH_TABLES		4	/* close all tables */
#define REFRESH_HOSTS		8	/* Flush host cache */
#define REFRESH_STATUS		16	/* Flush status variables */
#define REFRESH_THREADS		32	/* Flush thread cache */
#define REFRESH_SLAVE           64      /* Reset master info and restart slave
					   thread */
#define REFRESH_MASTER          128     /* Remove all bin logs in the index
					   and truncate the index */
#define REFRESH_ERROR_LOG       256 /* Rotate only the erorr log */
#define REFRESH_ENGINE_LOG      512 /* Flush all storage engine logs */
#define REFRESH_BINARY_LOG     1024 /* Flush the binary log */
#define REFRESH_RELAY_LOG      2048 /* Flush the relay log */
#define REFRESH_GENERAL_LOG    4096 /* Flush the general log */
#define REFRESH_SLOW_LOG       8192 /* Flush the slow query log */

/* The following can't be set with mysql_refresh() */
#define REFRESH_READ_LOCK	16384	/* Lock tables for read */
#define REFRESH_FAST		32768	/* Intern flag */

/* RESET (remove all queries) from query cache */
#define REFRESH_QUERY_CACHE	65536
#define REFRESH_QUERY_CACHE_FREE 0x20000L /* pack query cache */
#define REFRESH_DES_KEY_FILE	0x40000L
#define REFRESH_USER_RESOURCES	0x80000L
#define REFRESH_FOR_EXPORT      0x100000L /* FLUSH TABLES ... FOR EXPORT */
#define REFRESH_OPTIMIZER_COSTS 0x200000L /* FLUSH OPTIMIZER_COSTS */

#define CLIENT_LONG_PASSWORD	1	/* new more secure passwords */
#define CLIENT_FOUND_ROWS	2	/* Found instead of affected rows */
#define CLIENT_LONG_FLAG	4	/* Get all column flags */
#define CLIENT_CONNECT_WITH_DB	8	/* One can specify db on connect */
#define CLIENT_NO_SCHEMA	16	/* Don't allow database.table.column */
#define CLIENT_COMPRESS		32	/* Can use compression protocol */
#define CLIENT_ODBC		64	/* Odbc client */
#define CLIENT_LOCAL_FILES	128	/* Can use LOAD DATA LOCAL */
#define CLIENT_IGNORE_SPACE	256	/* Ignore spaces before '(' */
#define CLIENT_PROTOCOL_41	512	/* New 4.1 protocol */
#define CLIENT_INTERACTIVE	1024	/* This is an interactive client */
#define CLIENT_SSL              2048	/* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE   4096    /* IGNORE sigpipes */
#define CLIENT_TRANSACTIONS	8192	/* Client knows about transactions */
#define CLIENT_RESERVED         16384   /* Old flag for 4.1 protocol  */
#define CLIENT_RESERVED2        32768   /* Old flag for 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS (1UL << 16) /* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS    (1UL << 17) /* Enable/disable multi-results */
#define CLIENT_PS_MULTI_RESULTS (1UL << 18) /* Multi-results in PS-protocol */

#define CLIENT_PLUGIN_AUTH  (1UL << 19) /* Client supports plugin authentication */
#define CLIENT_CONNECT_ATTRS (1UL << 20) /* Client supports connection attributes */

/* Enable authentication response packet to be larger than 255 bytes. */
#define CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA (1UL << 21)

/* Don't close the connection for a connection with expired password. */
#define CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS (1UL << 22)

/**
  Capable of handling server state change information. Its a hint to the
  server to include the state change information in Ok packet.
*/
#define CLIENT_SESSION_TRACK (1UL << 23)
/* Client no longer needs EOF packet */
#define CLIENT_DEPRECATE_EOF (1UL << 24)

#define CLIENT_SSL_VERIFY_SERVER_CERT (1UL << 30)
#define CLIENT_REMEMBER_OPTIONS (1UL << 31)

#ifdef HAVE_COMPRESS
#define CAN_CLIENT_COMPRESS CLIENT_COMPRESS
#else
#define CAN_CLIENT_COMPRESS 0
#endif

/* Gather all possible capabilites (flags) supported by the server */
#define CLIENT_ALL_FLAGS  (CLIENT_LONG_PASSWORD \
                           | CLIENT_FOUND_ROWS \
                           | CLIENT_LONG_FLAG \
                           | CLIENT_CONNECT_WITH_DB \
                           | CLIENT_NO_SCHEMA \
                           | CLIENT_COMPRESS \
                           | CLIENT_ODBC \
                           | CLIENT_LOCAL_FILES \
                           | CLIENT_IGNORE_SPACE \
                           | CLIENT_PROTOCOL_41 \
                           | CLIENT_INTERACTIVE \
                           | CLIENT_SSL \
                           | CLIENT_IGNORE_SIGPIPE \
                           | CLIENT_TRANSACTIONS \
                           | CLIENT_RESERVED \
                           | CLIENT_RESERVED2 \
                           | CLIENT_MULTI_STATEMENTS \
                           | CLIENT_MULTI_RESULTS \
                           | CLIENT_PS_MULTI_RESULTS \
                           | CLIENT_SSL_VERIFY_SERVER_CERT \
                           | CLIENT_REMEMBER_OPTIONS \
                           | CLIENT_PLUGIN_AUTH \
                           | CLIENT_CONNECT_ATTRS \
                           | CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA \
                           | CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS \
                           | CLIENT_SESSION_TRACK \
                           | CLIENT_DEPRECATE_EOF \
)

/*
  Switch off the flags that are optional and depending on build flags
  If any of the optional flags is supported by the build it will be switched
  on before sending to the client during the connection handshake.
*/
#define CLIENT_BASIC_FLAGS (((CLIENT_ALL_FLAGS & ~CLIENT_SSL) \
                                               & ~CLIENT_COMPRESS) \
                                               & ~CLIENT_SSL_VERIFY_SERVER_CERT)

/**
  Is raised when a multi-statement transaction
  has been started, either explicitly, by means
  of BEGIN or COMMIT AND CHAIN, or
  implicitly, by the first transactional
  statement, when autocommit=off.
*/
#define SERVER_STATUS_IN_TRANS     1
#define SERVER_STATUS_AUTOCOMMIT   2	/* Server in auto_commit mode */
#define SERVER_MORE_RESULTS_EXISTS 8    /* Multi query - next query exists */
#define SERVER_QUERY_NO_GOOD_INDEX_USED 16
#define SERVER_QUERY_NO_INDEX_USED      32
/**
  The server was able to fulfill the clients request and opened a
  read-only non-scrollable cursor for a query. This flag comes
  in reply to COM_STMT_EXECUTE and COM_STMT_FETCH commands.
*/
#define SERVER_STATUS_CURSOR_EXISTS 64
/**
  This flag is sent when a read-only cursor is exhausted, in reply to
  COM_STMT_FETCH command.
*/
#define SERVER_STATUS_LAST_ROW_SENT 128
#define SERVER_STATUS_DB_DROPPED        256 /* A database was dropped */
#define SERVER_STATUS_NO_BACKSLASH_ESCAPES 512
/**
  Sent to the client if after a prepared statement reprepare
  we discovered that the new statement returns a different 
  number of result set columns.
*/
#define SERVER_STATUS_METADATA_CHANGED 1024
#define SERVER_QUERY_WAS_SLOW          2048

/**
  To mark ResultSet containing output parameter values.
*/
#define SERVER_PS_OUT_PARAMS            4096

/**
  Set at the same time as SERVER_STATUS_IN_TRANS if the started
  multi-statement transaction is a read-only transaction. Cleared
  when the transaction commits or aborts. Since this flag is sent
  to clients in OK and EOF packets, the flag indicates the
  transaction status at the end of command execution.
*/
#define SERVER_STATUS_IN_TRANS_READONLY 8192

/**
  This status flag, when on, implies that one of the state information has
  changed on the server because of the execution of the last statement.
*/
#define SERVER_SESSION_STATE_CHANGED (1UL << 14)

/**
  Server status flags that must be cleared when starting
  execution of a new SQL statement.
  Flags from this set are only added to the
  current server status by the execution engine, but 
  never removed -- the execution engine expects them 
  to disappear automagically by the next command.
*/
#define SERVER_STATUS_CLEAR_SET (SERVER_QUERY_NO_GOOD_INDEX_USED| \
                                 SERVER_QUERY_NO_INDEX_USED|\
                                 SERVER_MORE_RESULTS_EXISTS|\
                                 SERVER_STATUS_METADATA_CHANGED |\
                                 SERVER_QUERY_WAS_SLOW |\
                                 SERVER_STATUS_DB_DROPPED |\
                                 SERVER_STATUS_CURSOR_EXISTS|\
                                 SERVER_STATUS_LAST_ROW_SENT|\
                                 SERVER_SESSION_STATE_CHANGED)

#define MYSQL_ERRMSG_SIZE	512
#define NET_READ_TIMEOUT	30		/* Timeout on read */
#define NET_WRITE_TIMEOUT	60		/* Timeout on write */
#define NET_WAIT_TIMEOUT	8*60*60		/* Wait for new query */

#define ONLY_KILL_QUERY         1


struct st_vio;					/* Only C */
typedef struct st_vio Vio;

#define MAX_TINYINT_WIDTH       3       /* Max width for a TINY w.o. sign */
#define MAX_SMALLINT_WIDTH      5       /* Max width for a SHORT w.o. sign */
#define MAX_MEDIUMINT_WIDTH     8       /* Max width for a INT24 w.o. sign */
#define MAX_INT_WIDTH           10      /* Max width for a LONG w.o. sign */
#define MAX_BIGINT_WIDTH        20      /* Max width for a LONGLONG */
#define MAX_CHAR_WIDTH		255	/* Max length for a CHAR colum */
#define MAX_BLOB_WIDTH		16777216	/* Default width for blob */

typedef struct st_net {
  Vio *vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  my_socket fd;					/* For Perl DBI/dbd */
  /*
    The following variable is set if we are doing several queries in one
    command ( as in LOAD TABLE ... FROM MASTER ),
    and do not want to confuse the client with OK at the wrong time
  */
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout, read_timeout, retry_count;
  int fcntl;
  unsigned int *return_status;
  unsigned char reading_or_writing;
  char save_char;
  my_bool unused1; /* Please remove with the next incompatible ABI change */
  my_bool unused2; /* Please remove with the next incompatible ABI change */
  my_bool compress;
  my_bool unused3; /* Please remove with the next incompatible ABI change. */
  /*
    Pointer to query object in query cache, do not equal NULL (0) for
    queries in cache that have not stored its results yet
  */
  /*
    Unused, please remove with the next incompatible ABI change.
  */
  unsigned char *unused;
  unsigned int last_errno;
  unsigned char error; 
  my_bool unused4; /* Please remove with the next incompatible ABI change. */
  my_bool unused5; /* Please remove with the next incompatible ABI change. */
  /** Client library error message buffer. Actually belongs to struct MYSQL. */
  char last_error[MYSQL_ERRMSG_SIZE];
  /** Client library sqlstate buffer. Set along with the error message. */
  char sqlstate[SQLSTATE_LENGTH+1];
  /**
    Extension pointer, for the caller private use.
    Any program linking with the networking library can use this pointer,
    which is handy when private connection specific data needs to be
    maintained.
    The mysqld server process uses this pointer internally,
    to maintain the server internal instrumentation for the connection.
  */
  void *extension;
} NET;


#define packet_error (~(unsigned long) 0)
/* For backward compatibility */
#define CLIENT_MULTI_QUERIES    CLIENT_MULTI_STATEMENTS    
#define FIELD_TYPE_DECIMAL     MYSQL_TYPE_DECIMAL
#define FIELD_TYPE_NEWDECIMAL  MYSQL_TYPE_NEWDECIMAL
#define FIELD_TYPE_TINY        MYSQL_TYPE_TINY
#define FIELD_TYPE_SHORT       MYSQL_TYPE_SHORT
#define FIELD_TYPE_LONG        MYSQL_TYPE_LONG
#define FIELD_TYPE_FLOAT       MYSQL_TYPE_FLOAT
#define FIELD_TYPE_DOUBLE      MYSQL_TYPE_DOUBLE
#define FIELD_TYPE_NULL        MYSQL_TYPE_NULL
#define FIELD_TYPE_TIMESTAMP   MYSQL_TYPE_TIMESTAMP
#define FIELD_TYPE_LONGLONG    MYSQL_TYPE_LONGLONG
#define FIELD_TYPE_INT24       MYSQL_TYPE_INT24
#define FIELD_TYPE_DATE        MYSQL_TYPE_DATE
#define FIELD_TYPE_TIME        MYSQL_TYPE_TIME
#define FIELD_TYPE_DATETIME    MYSQL_TYPE_DATETIME
#define FIELD_TYPE_YEAR        MYSQL_TYPE_YEAR
#define FIELD_TYPE_NEWDATE     MYSQL_TYPE_NEWDATE
#define FIELD_TYPE_ENUM        MYSQL_TYPE_ENUM
#define FIELD_TYPE_SET         MYSQL_TYPE_SET
#define FIELD_TYPE_TINY_BLOB   MYSQL_TYPE_TINY_BLOB
#define FIELD_TYPE_MEDIUM_BLOB MYSQL_TYPE_MEDIUM_BLOB
#define FIELD_TYPE_LONG_BLOB   MYSQL_TYPE_LONG_BLOB
#define FIELD_TYPE_BLOB        MYSQL_TYPE_BLOB
#define FIELD_TYPE_VAR_STRING  MYSQL_TYPE_VAR_STRING
#define FIELD_TYPE_STRING      MYSQL_TYPE_STRING
#define FIELD_TYPE_CHAR        MYSQL_TYPE_TINY
#define FIELD_TYPE_INTERVAL    MYSQL_TYPE_ENUM
#define FIELD_TYPE_GEOMETRY    MYSQL_TYPE_GEOMETRY
#define FIELD_TYPE_BIT         MYSQL_TYPE_BIT


/* Shutdown/kill enums and constants */ 

/* Bits for THD::killable. */
#define MYSQL_SHUTDOWN_KILLABLE_CONNECT    (unsigned char)(1 << 0)
#define MYSQL_SHUTDOWN_KILLABLE_TRANS      (unsigned char)(1 << 1)
#define MYSQL_SHUTDOWN_KILLABLE_LOCK_TABLE (unsigned char)(1 << 2)
#define MYSQL_SHUTDOWN_KILLABLE_UPDATE     (unsigned char)(1 << 3)

enum mysql_enum_shutdown_level {
  /*
    We want levels to be in growing order of hardness (because we use number
    comparisons). Note that DEFAULT does not respect the growing property, but
    it's ok.
  */
  SHUTDOWN_DEFAULT = 0,
  /* wait for existing connections to finish */
  SHUTDOWN_WAIT_CONNECTIONS= MYSQL_SHUTDOWN_KILLABLE_CONNECT,
  /* wait for existing trans to finish */
  SHUTDOWN_WAIT_TRANSACTIONS= MYSQL_SHUTDOWN_KILLABLE_TRANS,
  /* wait for existing updates to finish (=> no partial MyISAM update) */
  SHUTDOWN_WAIT_UPDATES= MYSQL_SHUTDOWN_KILLABLE_UPDATE,
  /* flush InnoDB buffers and other storage engines' buffers*/
  SHUTDOWN_WAIT_ALL_BUFFERS= (MYSQL_SHUTDOWN_KILLABLE_UPDATE << 1),
  /* don't flush InnoDB buffers, flush other storage engines' buffers*/
  SHUTDOWN_WAIT_CRITICAL_BUFFERS= (MYSQL_SHUTDOWN_KILLABLE_UPDATE << 1) + 1,
  /* Now the 2 levels of the KILL command */
  KILL_QUERY= 254,
  KILL_CONNECTION= 255
};


enum enum_cursor_type
{
  CURSOR_TYPE_NO_CURSOR= 0,
  CURSOR_TYPE_READ_ONLY= 1,
  CURSOR_TYPE_FOR_UPDATE= 2,
  CURSOR_TYPE_SCROLLABLE= 4
};


/* options for mysql_set_option */
enum enum_mysql_set_option
{
  MYSQL_OPTION_MULTI_STATEMENTS_ON,
  MYSQL_OPTION_MULTI_STATEMENTS_OFF
};

/*
  Type of state change information that the server can include in the Ok
  packet.
  Note : 1) session_state_type shouldn't go past 255 (i.e. 1-byte boundary).
         2) Modify the definition of SESSION_TRACK_END when a new member is
	    added.
*/
enum enum_session_state_type
{
  SESSION_TRACK_SYSTEM_VARIABLES,                       /* Session system variables */
  SESSION_TRACK_SCHEMA,                          /* Current schema */
  SESSION_TRACK_STATE_CHANGE,                  /* track session state changes */
  SESSION_TRACK_GTIDS,
  SESSION_TRACK_TRANSACTION_CHARACTERISTICS,  /* Transaction chistics */
  SESSION_TRACK_TRANSACTION_STATE             /* Transaction state */
};

#define SESSION_TRACK_BEGIN SESSION_TRACK_SYSTEM_VARIABLES

#define SESSION_TRACK_END SESSION_TRACK_TRANSACTION_STATE

#define IS_SESSION_STATE_TYPE(T) \
  (((int)(T) >= SESSION_TRACK_BEGIN) && ((T) <= SESSION_TRACK_END))

#define net_new_transaction(net) ((net)->pkt_nr=0)

#ifdef __cplusplus
extern "C" {
#endif

my_bool	my_net_init(NET *net, Vio* vio);
void my_net_local_init(NET *net);
void net_end(NET *net);
void net_clear(NET *net, my_bool check_buffer);
void net_claim_memory_ownership(NET *net);
my_bool net_realloc(NET *net, size_t length);
my_bool	net_flush(NET *net);
my_bool	my_net_write(NET *net,const unsigned char *packet, size_t len);
my_bool	net_write_command(NET *net,unsigned char command,
			  const unsigned char *header, size_t head_len,
			  const unsigned char *packet, size_t len);
my_bool net_write_packet(NET *net, const unsigned char *packet, size_t length);
unsigned long my_net_read(NET *net);

#ifdef MY_GLOBAL_INCLUDED
void my_net_set_write_timeout(NET *net, uint timeout);
void my_net_set_read_timeout(NET *net, uint timeout);
#endif

struct rand_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};

#ifdef __cplusplus
}
#endif

  /* The following is for user defined functions */

enum Item_result {STRING_RESULT=0, REAL_RESULT, INT_RESULT, ROW_RESULT,
                  DECIMAL_RESULT};

typedef struct st_udf_args
{
  unsigned int arg_count;		/* Number of arguments */
  enum Item_result *arg_type;		/* Pointer to item_results */
  char **args;				/* Pointer to argument */
  unsigned long *lengths;		/* Length of string arguments */
  char *maybe_null;			/* Set to 1 for all maybe_null args */
  char **attributes;                    /* Pointer to attribute name */
  unsigned long *attribute_lengths;     /* Length of attribute arguments */
  void *extension;
} UDF_ARGS;

  /* This holds information about the result */

typedef struct st_udf_init
{
  my_bool maybe_null;          /* 1 if function can return NULL */
  unsigned int decimals;       /* for real functions */
  unsigned long max_length;    /* For string functions */
  char *ptr;                   /* free pointer for function data */
  my_bool const_item;          /* 1 if function always returns the same value */
  void *extension;
} UDF_INIT;
/* 
  TODO: add a notion for determinism of the UDF. 
  See Item_udf_func::update_used_tables ()
*/

  /* Constants when using compression */
#define NET_HEADER_SIZE 4		/* standard header size */
#define COMP_HEADER_SIZE 3		/* compression header extra size */

  /* Prototypes to password functions */

#ifdef __cplusplus
extern "C" {
#endif

/*
  These functions are used for authentication by client and server and
  implemented in sql/password.c
*/

void randominit(struct rand_struct *, unsigned long seed1,
                unsigned long seed2);
double my_rnd(struct rand_struct *);
void create_random_string(char *to, unsigned int length, struct rand_struct *rand_st);

void hash_password(unsigned long *to, const char *password, unsigned int password_len);
void make_scrambled_password_323(char *to, const char *password);
void scramble_323(char *to, const char *message, const char *password);
my_bool check_scramble_323(const unsigned char *reply, const char *message,
                           unsigned long *salt);
void get_salt_from_password_323(unsigned long *res, const char *password);
void make_password_from_salt_323(char *to, const unsigned long *salt);

void make_scrambled_password(char *to, const char *password);
void scramble(char *to, const char *message, const char *password);
my_bool check_scramble(const unsigned char *reply, const char *message,
                       const unsigned char *hash_stage2);
void get_salt_from_password(unsigned char *res, const char *password);
void make_password_from_salt(char *to, const unsigned char *hash_stage2);
char *octet2hex(char *to, const char *str, unsigned int len);

/* end of password.c */

my_bool generate_sha256_scramble(unsigned char *dst, size_t dst_size,
                                 const char *src, size_t src_size, const char *rnd,
                                 size_t rnd_size);

char *get_tty_password(const char *opt_message);
const char *mysql_errno_to_sqlstate(unsigned int mysql_errno);

/* Some other useful functions */

my_bool my_thread_init(void);
void my_thread_end(void);

#ifdef MY_GLOBAL_INCLUDED
ulong STDCALL net_field_length(uchar **packet);
ulong STDCALL net_field_length_checked(uchar **packet, ulong max_length);
my_ulonglong net_field_length_ll(uchar **packet);
uchar *net_store_length(uchar *pkg, ulonglong length);
unsigned int net_length_size(ulonglong num);
#endif

#ifdef __cplusplus
}
#endif

#define NULL_LENGTH ((unsigned long) ~0) /* For net_store_length */
#define MYSQL_STMT_HEADER       4
#define MYSQL_LONG_DATA_HEADER  6

#define NOT_FIXED_DEC           31
#endif
