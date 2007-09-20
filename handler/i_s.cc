/******************************************************
InnoDB INFORMATION SCHEMA tables interface to MySQL.

(c) 2007 Innobase Oy

Created July 18, 2007 Vasil Dimov
*******************************************************/

#include <strings.h>
#include <mysql_priv.h>
#include <mysqld_error.h>

#include <m_ctype.h>
#include <hash.h>
#include <myisampack.h>
#include <mysys_err.h>
#include <my_sys.h>
#include "i_s.h"
#include <mysql/plugin.h>

extern "C" {
#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
}

#define OK(expr)		\
	if ((expr) != 0) {	\
		DBUG_RETURN(1);	\
	}

#if defined(__GNUC__) && (__GNUC__) > 2 && !defined(__INTEL_COMPILER)
#define STRUCT_FLD(name, value)	name: value
#else
#define STRUCT_FLD(name, value)	value
#endif

/*
Use the following types mapping:

C type	ST_FIELD_INFO::field_type
---------------------------------
long			MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS)

long unsigned		MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS, field_flags=MY_I_S_UNSIGNED)

char*			MYSQL_TYPE_STRING
(field_length=n)

float			MYSQL_TYPE_FLOAT
(field_length=0 is ignored)

void*			MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS, field_flags=MY_I_S_UNSIGNED)

boolean (if else)	MYSQL_TYPE_LONG
(field_length=1)

time_t			MYSQL_TYPE_DATETIME
(field_length=0 ignored)
---------------------------------
*/

/* XXX these are defined in mysql_priv.h inside #ifdef MYSQL_SERVER */
bool schema_table_store_record(THD *thd, TABLE *table);
void localtime_to_TIME(MYSQL_TIME *to, struct tm *from);

/***********************************************************************
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits */
static
int
trx_i_s_common_fill_table(
/*======================*/
				/* out: 0 on success */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond);	/* in: condition (not used) */

/***********************************************************************
Unbind a dynamic INFORMATION_SCHEMA table. */
static
int
trx_i_s_common_deinit(
/*==================*/
			/* out: 0 on success */
	void*	p);	/* in/out: table schema object */

/***********************************************************************
Auxiliary function to store time_t value in MYSQL_TYPE_DATETIME
field. */
static
int
field_store_time_t(
/*===============*/
			/* out: 0 on success */
	Field*	field,	/* in/out: target field for storage */
	time_t	time)	/* in: value to store */
{
	MYSQL_TIME	my_time;
	struct tm	tm_time;

#if 0
	/* use this if you are sure that `variables' and `time_zone'
	are always initialized */
	thd->variables.time_zone->gmt_sec_to_TIME(
		&my_time, (my_time_t) time);
#else
	localtime_r(&time, &tm_time);
	localtime_to_TIME(&my_time, &tm_time);
	my_time.time_type = MYSQL_TIMESTAMP_DATETIME;
#endif

	return(field->store_time(&my_time, MYSQL_TIMESTAMP_DATETIME));
}

/***********************************************************************
Auxiliary function to store char* value in MYSQL_TYPE_STRING field. */
static
int
field_store_string(
/*===============*/
				/* out: 0 on success */
	Field*		field,	/* in/out: target field for storage */
	const char*	str)	/* in: NUL-terminated utf-8 string,
				or NULL */
{
	int	ret;

	if (str != NULL) {

		ret = field->store(str, strlen(str),
				   system_charset_info);
		field->set_notnull();
	} else {

		ret = 0; /* success */
		field->set_null();
	}

	return(ret);
}

/***********************************************************************
Auxiliary function to store ulint value in MYSQL_TYPE_LONGLONG field.
If the value is ULINT_UNDEFINED then the field it set to NULL. */
static
int
field_store_ulint(
/*==============*/
			/* out: 0 on success */
	Field*	field,	/* in/out: target field for storage */
	ulint	n)	/* in: value to store */
{
	int	ret;

	if (n != ULINT_UNDEFINED) {

		ret = field->store(n);
		field->set_notnull();
	} else {

		ret = 0; /* success */
		field->set_null();
	}

	return(ret);
}

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_trx */
static ST_FIELD_INFO	innodb_trx_fields_info[] =
{
#define IDX_TRX_ID		0
	{STRUCT_FLD(field_name,		"trx_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		"")},

#define IDX_TRX_STATE		1
	{STRUCT_FLD(field_name,		"trx_state"),
	 STRUCT_FLD(field_length,	TRX_QUE_STATE_STR_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_TRX_STARTED		2
	{STRUCT_FLD(field_name,		"trx_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_TRX_WAIT_LOCK_ID	3
	{STRUCT_FLD(field_name,		"trx_wait_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

#define IDX_TRX_WAIT_STARTED	4
	{STRUCT_FLD(field_name,		"trx_wait_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

#define IDX_TRX_MYSQL_THREAD_ID	5
	{STRUCT_FLD(field_name,		"trx_mysql_thread_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		"")},

	{STRUCT_FLD(field_name,		NULL),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_NULL),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")}
};

/***********************************************************************
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_trx
table with it. */
static
int
fill_innodb_trx_from_cache(
/*=======================*/
					/* out: 0 on success */
	trx_i_s_cache_t*	cache,	/* in: cache to read from */
	THD*			thd,	/* in: used to call
					schema_table_store_record() */
	TABLE*			table)	/* in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN];
	ulint	i;

	DBUG_ENTER("fill_innodb_trx_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_TRX);

	for (i = 0; i < rows_num; i++) {

		i_s_trx_row_t*	row;

		row = (i_s_trx_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_TRX, i);

		/* trx_id */
		OK(fields[IDX_TRX_ID]->store(row->trx_id));

		/* trx_state */
		OK(field_store_string(fields[IDX_TRX_STATE],
				      row->trx_state));

		/* trx_started */
		OK(field_store_time_t(fields[IDX_TRX_STARTED],
				      (time_t) row->trx_started));

		/* trx_wait_lock_id */
		/* trx_wait_started */
		if (row->trx_wait_started != 0) {

			OK(field_store_string(
				   fields[IDX_TRX_WAIT_LOCK_ID],
				   trx_i_s_create_lock_id(
					   row->wait_lock_row,
					   lock_id, sizeof(lock_id))));
			/* field_store_string() sets it no notnull */

			OK(field_store_time_t(
				   fields[IDX_TRX_WAIT_STARTED],
				   (time_t) row->trx_wait_started));
			fields[IDX_TRX_WAIT_STARTED]->set_notnull();
		} else {

			fields[IDX_TRX_WAIT_LOCK_ID]->set_null();
			fields[IDX_TRX_WAIT_STARTED]->set_null();
		}

		/* trx_mysql_thread_id */
		OK(fields[IDX_TRX_MYSQL_THREAD_ID]->store(
			   row->trx_mysql_thread_id));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table INFORMATION_SCHEMA.innodb_trx */
static
int
innodb_trx_init(
/*============*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_trx_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_trx_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

static st_mysql_information_schema	innodb_trx_info =
{
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

struct st_mysql_plugin	i_s_innodb_trx =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &innodb_trx_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "innodb_trx"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Innobase Oy"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB transactions"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_trx_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, trx_i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_locks */
static ST_FIELD_INFO	innodb_locks_fields_info[] =
{
#define IDX_LOCK_ID		0
	{STRUCT_FLD(field_name,		"lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_TRX_ID		1
	{STRUCT_FLD(field_name,		"lock_trx_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_MODE		2
	{STRUCT_FLD(field_name,		"lock_mode"),
	 STRUCT_FLD(field_length,	32 /* S|X|IS|IX|AUTO_INC|UNKNOWN */),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_TYPE		3
	{STRUCT_FLD(field_name,		"lock_type"),
	 STRUCT_FLD(field_length,	32 /* RECORD|TABLE|UNKNOWN */),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_TABLE		4
	{STRUCT_FLD(field_name,		"lock_table"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_INDEX		5
	{STRUCT_FLD(field_name,		"lock_index"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_SPACE		6
	{STRUCT_FLD(field_name,		"lock_space"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_PAGE		7
	{STRUCT_FLD(field_name,		"lock_page"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

#define IDX_LOCK_REC		8
	{STRUCT_FLD(field_name,		"lock_rec"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		"")},

	{STRUCT_FLD(field_name,		NULL),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_NULL),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")}
};

/***********************************************************************
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_locks
table with it. */
static
int
fill_innodb_locks_from_cache(
/*=========================*/
					/* out: 0 on success */
	trx_i_s_cache_t*	cache,	/* in: cache to read from */
	THD*			thd,	/* in: used to call
					schema_table_store_record() */
	TABLE*			table)	/* in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN];
	ulint	i;

	DBUG_ENTER("fill_innodb_locks_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCKS);

	for (i = 0; i < rows_num; i++) {

		i_s_locks_row_t*	row;

		row = (i_s_locks_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCKS, i);

		/* lock_id */
		OK(field_store_string(
			   fields[IDX_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row, lock_id, sizeof(lock_id))));

		/* lock_trx_id */
		OK(fields[IDX_LOCK_TRX_ID]->store(row->lock_trx_id));

		/* lock_mode */
		OK(field_store_string(fields[IDX_LOCK_MODE],
				      row->lock_mode));

		/* lock_type */
		OK(field_store_string(fields[IDX_LOCK_TYPE],
				      row->lock_type));

		/* lock_table */
		OK(field_store_string(fields[IDX_LOCK_TABLE],
				      row->lock_table));

		/* lock_index */
		OK(field_store_string(fields[IDX_LOCK_INDEX],
				      row->lock_index));

		/* lock_space */
		OK(field_store_ulint(fields[IDX_LOCK_SPACE],
				     row->lock_space));

		/* lock_page */
		OK(field_store_ulint(fields[IDX_LOCK_PAGE],
				     row->lock_page));

		/* lock_rec */
		OK(field_store_ulint(fields[IDX_LOCK_REC],
				     row->lock_rec));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table INFORMATION_SCHEMA.innodb_locks */
static
int
innodb_locks_init(
/*==============*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_locks_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_locks_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

static st_mysql_information_schema	innodb_locks_info =
{
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

struct st_mysql_plugin	i_s_innodb_locks =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &innodb_locks_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "innodb_locks"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Innobase Oy"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB conflicting locks"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_locks_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, trx_i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_lock_waits */
static ST_FIELD_INFO	innodb_lock_waits_fields_info[] =
{
#define IDX_WAIT_LOCK_ID	0
	{STRUCT_FLD(field_name,		"wait_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

#define IDX_WAITED_LOCK_ID	1
	{STRUCT_FLD(field_name,		"waited_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")},

	{STRUCT_FLD(field_name,		NULL),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_NULL),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"")}
};

/***********************************************************************
Read data from cache buffer and fill the
INFORMATION_SCHEMA.innodb_lock_waits table with it. */
static
int
fill_innodb_lock_waits_from_cache(
/*==============================*/
					/* out: 0 on success */
	trx_i_s_cache_t*	cache,	/* in: cache to read from */
	THD*			thd,	/* in: used to call
					schema_table_store_record() */
	TABLE*			table)	/* in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	wait_lock_id[TRX_I_S_LOCK_ID_MAX_LEN];
	char	waited_lock_id[TRX_I_S_LOCK_ID_MAX_LEN];
	ulint	i;

	DBUG_ENTER("fill_innodb_lock_waits_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCK_WAITS);

	for (i = 0; i < rows_num; i++) {

		i_s_lock_waits_row_t*	row;

		row = (i_s_lock_waits_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCK_WAITS, i);

		/* wait_lock_id */
		OK(field_store_string(
			   fields[IDX_WAIT_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->wait_lock_row,
				   wait_lock_id,
				   sizeof(wait_lock_id))));

		/* waited_lock_id */
		OK(field_store_string(
			   fields[IDX_WAITED_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->waited_lock_row,
				   waited_lock_id,
				   sizeof(waited_lock_id))));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table INFORMATION_SCHEMA.innodb_lock_waits */
static
int
innodb_lock_waits_init(
/*===================*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_lock_waits_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_lock_waits_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

static st_mysql_information_schema	innodb_lock_waits_info =
{
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

struct st_mysql_plugin	i_s_innodb_lock_waits =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &innodb_lock_waits_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "innodb_lock_waits"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Innobase Oy"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB which lock is blocking which"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_lock_waits_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, trx_i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

/***********************************************************************
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits */
static
int
trx_i_s_common_fill_table(
/*======================*/
				/* out: 0 on success */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (not used) */
{
	const char*		table_name;
	int			ret;
	trx_i_s_cache_t*	cache;

	DBUG_ENTER("trx_i_s_common_fill_table");

	/* minimize the number of places where global variables are
	referenced */
	cache = trx_i_s_cache;

	/* update the cache */
	trx_i_s_cache_start_write(cache);
	trx_i_s_possibly_fetch_data_into_cache(cache);
	trx_i_s_cache_end_write(cache);

	/* which table we have to fill? */
	table_name = tables->schema_table_name;
	/* or table_name = tables->schema_table->table_name; */

	ret = 0;

	trx_i_s_cache_start_read(cache);

	if (strcasecmp(table_name, "innodb_trx") == 0) {

		if (fill_innodb_trx_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (strcasecmp(table_name, "innodb_locks") == 0) {

		if (fill_innodb_locks_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (strcasecmp(table_name, "innodb_lock_waits") == 0) {

		if (fill_innodb_lock_waits_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else {

		/* huh! what happened!? */
		fprintf(stderr,
			"InnoDB: trx_i_s_common_fill_table() was "
			"called to fill unknown table: %s.\n"
			"This function only knows how to fill "
			"innodb_trx, innodb_locks and "
			"innodb_lock_waits tables.\n", table_name);

		ret = 1;
	}

	trx_i_s_cache_end_read(cache);

#if 0
	DBUG_RETURN(ret);
#else
	/* if this function returns something else than 0 then a
	deadlock occurs between the mysqld server and mysql client,
	see http://bugs.mysql.com/29900 ; when that bug is resolved
	we can enable the DBUG_RETURN(ret) above */
	DBUG_RETURN(0);
#endif
}

/***********************************************************************
Unbind a dynamic INFORMATION_SCHEMA table. */
static
int
trx_i_s_common_deinit(
/*==================*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("trx_i_s_common_deinit");

	/* Do nothing */

	DBUG_RETURN(0);
}
