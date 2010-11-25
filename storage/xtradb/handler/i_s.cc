/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file handler/i_s.cc
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/

#include <mysql_priv.h>
#include <mysqld_error.h>

#include <m_ctype.h>
#include <hash.h>
#include <myisampack.h>
#include <mysys_err.h>
#include <my_sys.h>
#include "i_s.h"
#include "innodb_patch_info.h"
#include <mysql/plugin.h>

extern "C" {
#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
#include "buf0buddy.h" /* for i_s_cmpmem */
#include "buf0buf.h" /* for buf_pool and PAGE_ZIP_MIN_SIZE */
#include "ha_prototypes.h" /* for innobase_convert_name() */
#include "srv0start.h" /* for srv_was_started */
#include "btr0btr.h" /* for btr_page_get_index_id */
#include "trx0rseg.h" /* for trx_rseg_struct */
#include "trx0sys.h" /* for trx_sys */
#include "dict0dict.h" /* for dict_sys */
#include "btr0pcur.h"
#include "buf0lru.h" /* for XTRA_LRU_[DUMP/RESTORE] */
}

static const char plugin_author[] = "Innobase Oy";

#define OK(expr)		\
	if ((expr) != 0) {	\
		DBUG_RETURN(1);	\
	}

#define RETURN_IF_INNODB_NOT_STARTED(plugin_name)			\
do {									\
	if (!srv_was_started) {						\
		push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,	\
				    ER_CANT_FIND_SYSTEM_REC,		\
				    "InnoDB: SELECTing from "		\
				    "INFORMATION_SCHEMA.%s but "	\
				    "the InnoDB storage engine "	\
				    "is not installed", plugin_name);	\
		DBUG_RETURN(0);						\
	}								\
} while (0)

#if !defined __STRICT_ANSI__ && defined __GNUC__ && (__GNUC__) > 2 && !defined __INTEL_COMPILER
#define STRUCT_FLD(name, value)	name: value
#else
#define STRUCT_FLD(name, value)	value
#endif

/* Don't use a static const variable here, as some C++ compilers (notably
HPUX aCC: HP ANSI C++ B3910B A.03.65) can't handle it. */
#define END_OF_ST_FIELD_INFO \
	{STRUCT_FLD(field_name,		NULL), \
	 STRUCT_FLD(field_length,	0), \
	 STRUCT_FLD(field_type,		MYSQL_TYPE_NULL), \
	 STRUCT_FLD(value,		0), \
	 STRUCT_FLD(field_flags,	0), \
	 STRUCT_FLD(old_name,		""), \
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)}

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
bool check_global_access(THD *thd, ulong want_access);

/*******************************************************************//**
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond);	/*!< in: condition (not used) */

/*******************************************************************//**
Unbind a dynamic INFORMATION_SCHEMA table.
@return	0 on success */
static
int
i_s_common_deinit(
/*==============*/
	void*	p);	/*!< in/out: table schema object */

/*******************************************************************//**
Auxiliary function to store time_t value in MYSQL_TYPE_DATETIME
field.
@return	0 on success */
static
int
field_store_time_t(
/*===============*/
	Field*	field,	/*!< in/out: target field for storage */
	time_t	time)	/*!< in: value to store */
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

/*******************************************************************//**
Auxiliary function to store char* value in MYSQL_TYPE_STRING field.
@return	0 on success */
static
int
field_store_string(
/*===============*/
	Field*		field,	/*!< in/out: target field for storage */
	const char*	str)	/*!< in: NUL-terminated utf-8 string,
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

/*******************************************************************//**
Auxiliary function to store ulint value in MYSQL_TYPE_LONGLONG field.
If the value is ULINT_UNDEFINED then the field it set to NULL.
@return	0 on success */
static
int
field_store_ulint(
/*==============*/
	Field*	field,	/*!< in/out: target field for storage */
	ulint	n)	/*!< in: value to store */
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

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_patches */
static ST_FIELD_INFO	innodb_patches_fields_info[] =
{
#define IDX_PATCH_NAME		0
	{STRUCT_FLD(field_name,		"name"),
	 STRUCT_FLD(field_length,	255),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_PATCH_DESCR		1
	{STRUCT_FLD(field_name,		"description"),
	 STRUCT_FLD(field_length,	255),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_PATCH_COMMENT		2
	{STRUCT_FLD(field_name,		"comment"),
	 STRUCT_FLD(field_length,	100),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_PATCH_LINK			3
	{STRUCT_FLD(field_name,		"link"),
	 STRUCT_FLD(field_length,	255),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static struct st_mysql_information_schema	i_s_info =
{
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

/***********************************************************************
Fill the dynamic table information_schema.innodb_patches */
static
int
innodb_patches_fill(
/*=============*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	int	i;
	Field**	fields;


	DBUG_ENTER("innodb_patches_fill");
	fields = table->field;

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);
	
	for (i = 0; innodb_enhancements[i].file; i++) {

   	field_store_string(fields[0],innodb_enhancements[i].file);
   	field_store_string(fields[1],innodb_enhancements[i].name);
   	field_store_string(fields[2],innodb_enhancements[i].comment);
   	field_store_string(fields[3],innodb_enhancements[i].link);

	if (schema_table_store_record(thd, table)) {
		status = 1;
		break;
	}

	}


	DBUG_RETURN(status);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_patches. */
static
int
innodb_patches_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("innodb_patches_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_patches_fields_info;
	schema->fill_table = innodb_patches_fill;

	DBUG_RETURN(0);
}


UNIV_INTERN struct st_mysql_plugin      i_s_innodb_patches =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "XTRADB_ENHANCEMENTS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, "Percona"),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Enhancements applied to InnoDB plugin"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, innodb_patches_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* reserved for dependency checking */
        /* void* */
        STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_patches_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "XTRADB_ENHANCEMENTS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, "Percona"),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Enhancements applied to InnoDB plugin"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, innodb_patches_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};


static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_type"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_index_fields_info[] =
{
	{STRUCT_FLD(field_name,		"index_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"n_recs"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"data_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"hashed"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"access_time"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"modified"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"dirty"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"old"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_blob_fields_info[] =
{
	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compressed"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"part_len"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"next_page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

  ulint		n_chunks, n_blocks;

	buf_chunk_t*	chunk;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	buf_pool_mutex_enter();
	
	chunk = buf_pool->chunks;
  
	for (n_chunks = buf_pool->n_chunks; n_chunks--; chunk++) {
		buf_block_t*	block		= chunk->blocks;

    for (n_blocks	= chunk->size; n_blocks--; block++) {
      const buf_frame_t* frame = block->frame;
  
      char page_type[64];

      switch(fil_page_get_type(frame))
      {
      case FIL_PAGE_INDEX:
        strcpy(page_type, "index");
        break;
      case FIL_PAGE_UNDO_LOG:
        strcpy(page_type, "undo_log");
        break;
      case FIL_PAGE_INODE:
        strcpy(page_type, "inode");
        break;
      case FIL_PAGE_IBUF_FREE_LIST:
        strcpy(page_type, "ibuf_free_list");
        break;
      case FIL_PAGE_TYPE_ALLOCATED:
        strcpy(page_type, "allocated");
        break;
      case FIL_PAGE_IBUF_BITMAP:
        strcpy(page_type, "bitmap");
        break;
      case FIL_PAGE_TYPE_SYS:
        strcpy(page_type, "sys");
        break;
      case FIL_PAGE_TYPE_TRX_SYS:
        strcpy(page_type, "trx_sys");
        break;
      case FIL_PAGE_TYPE_FSP_HDR:
        strcpy(page_type, "fsp_hdr");
        break;
      case FIL_PAGE_TYPE_XDES:
        strcpy(page_type, "xdes");
        break;
      case FIL_PAGE_TYPE_BLOB:
        strcpy(page_type, "blob");
        break;
      case FIL_PAGE_TYPE_ZBLOB:
        strcpy(page_type, "zblob");
        break;
      case FIL_PAGE_TYPE_ZBLOB2:
        strcpy(page_type, "zblob2");
        break;
      default:
        sprintf(page_type, "unknown (type=%li)", fil_page_get_type(frame));
      }
      
      field_store_string(table->field[0], page_type);
      table->field[1]->store(block->page.space);
      table->field[2]->store(block->page.offset);
      table->field[3]->store(0);
      table->field[4]->store(block->page.buf_fix_count);
      table->field[5]->store(block->page.flush_type);

      if (schema_table_store_record(thd, table)) {
        status = 1;
        break;
      }
      
    }      
	}

	buf_pool_mutex_exit();

	DBUG_RETURN(status);
}

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages_index. */
static
int
i_s_innodb_buffer_pool_pages_index_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

  ulint		n_chunks, n_blocks;
  dulint		index_id;

	buf_chunk_t*	chunk;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_index_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	buf_pool_mutex_enter();
	
	chunk = buf_pool->chunks;
  
	for (n_chunks = buf_pool->n_chunks; n_chunks--; chunk++) {
		buf_block_t*	block		= chunk->blocks;

		for (n_blocks	= chunk->size; n_blocks--; block++) {
			const buf_frame_t* frame = block->frame;
  
      if (fil_page_get_type(frame) == FIL_PAGE_INDEX) {
        index_id = btr_page_get_index_id(frame);
        table->field[0]->store(ut_conv_dulint_to_longlong(index_id));
        table->field[1]->store(block->page.space);
        table->field[2]->store(block->page.offset);
        table->field[3]->store(page_get_n_recs(frame));
        table->field[4]->store(page_get_data_size(frame));
        table->field[5]->store(block->is_hashed);
        table->field[6]->store(block->page.access_time);
        table->field[7]->store(block->page.newest_modification != 0);
        table->field[8]->store(block->page.oldest_modification != 0);
        table->field[9]->store(block->page.old);
        table->field[10]->store(0);
        table->field[11]->store(block->page.buf_fix_count);
        table->field[12]->store(block->page.flush_type);
          
        if (schema_table_store_record(thd, table)) {
          status = 1;
          break;
        }
      }      
    }
	}

	buf_pool_mutex_exit();

	DBUG_RETURN(status);
}

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages_index. */
static
int
i_s_innodb_buffer_pool_pages_blob_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

  ulint		n_chunks, n_blocks;
	buf_chunk_t*	chunk;
	page_zip_des_t*	block_page_zip;

	ulint		part_len;
	ulint		next_page_no;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_blob_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	buf_pool_mutex_enter();
	
	chunk = buf_pool->chunks;
    
	for (n_chunks = buf_pool->n_chunks; n_chunks--; chunk++) {
		buf_block_t*	block		= chunk->blocks;
    block_page_zip = buf_block_get_page_zip(block);

    for (n_blocks	= chunk->size; n_blocks--; block++) {
      const buf_frame_t* frame = block->frame;

      if (fil_page_get_type(frame) == FIL_PAGE_TYPE_BLOB) {

        if (UNIV_LIKELY_NULL(block_page_zip)) {
          part_len = 0; /* hmm, can't figure it out */
  
          next_page_no = mach_read_from_4(
            buf_block_get_frame(block)
            + FIL_PAGE_NEXT);        
        } else {
          part_len = mach_read_from_4(
            buf_block_get_frame(block)
            + FIL_PAGE_DATA
            + 0 /*BTR_BLOB_HDR_PART_LEN*/);
  
          next_page_no = mach_read_from_4(
            buf_block_get_frame(block)
            + FIL_PAGE_DATA
            + 4 /*BTR_BLOB_HDR_NEXT_PAGE_NO*/);
        }

        table->field[0]->store(block->page.space);
        table->field[1]->store(block->page.offset);
        table->field[2]->store(block_page_zip != NULL);
        table->field[3]->store(part_len);

        if(next_page_no == FIL_NULL)
        {
          table->field[4]->store(0);
        } else {
          table->field[4]->store(block->page.offset);
        }

        table->field[5]->store(0);
        table->field[6]->store(block->page.buf_fix_count);
        table->field[7]->store(block->page.flush_type);
  
        if (schema_table_store_record(thd, table)) {
          status = 1;
          break;
        }

      }
    }      
	}

	buf_pool_mutex_exit();

	DBUG_RETURN(status);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_fill;

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_index_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_index_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_index_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_index_fill;

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_blob_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_blob_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_blob_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_blob_fill;

	DBUG_RETURN(0);
}


UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

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

UNIV_INTERN struct st_maria_plugin      i_s_innodb_buffer_pool_pages_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "InnoDB buffer pool pages"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100 /* 1.0 */),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_index =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_INDEX"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool index pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_index_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

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

UNIV_INTERN struct st_maria_plugin i_s_innodb_buffer_pool_pages_index_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_INDEX"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "InnoDB buffer pool index pages"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_index_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100 /* 1.0 */),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_blob =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_BLOB"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool blob pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_blob_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

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

UNIV_INTERN struct st_maria_plugin i_s_innodb_buffer_pool_pages_blob_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_BLOB"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "InnoDB buffer pool blob pages"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_blob_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100 /* 1.0 */),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};


/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_trx */
static ST_FIELD_INFO	innodb_trx_fields_info[] =
{
#define IDX_TRX_ID		0
	{STRUCT_FLD(field_name,		"trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_STATE		1
	{STRUCT_FLD(field_name,		"trx_state"),
	 STRUCT_FLD(field_length,	TRX_QUE_STATE_STR_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_STARTED		2
	{STRUCT_FLD(field_name,		"trx_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_REQUESTED_LOCK_ID	3
	{STRUCT_FLD(field_name,		"trx_requested_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_WAIT_STARTED	4
	{STRUCT_FLD(field_name,		"trx_wait_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_WEIGHT		5
	{STRUCT_FLD(field_name,		"trx_weight"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_MYSQL_THREAD_ID	6
	{STRUCT_FLD(field_name,		"trx_mysql_thread_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_QUERY		7
	{STRUCT_FLD(field_name,		"trx_query"),
	 STRUCT_FLD(field_length,	TRX_I_S_TRX_QUERY_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_trx
table with it.
@return	0 on success */
static
int
fill_innodb_trx_from_cache(
/*=======================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: used to call
					schema_table_store_record() */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_trx_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_TRX);

	for (i = 0; i < rows_num; i++) {

		i_s_trx_row_t*	row;
		char		trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_trx_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_TRX, i);

		/* trx_id */
		ut_snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, row->trx_id);
		OK(field_store_string(fields[IDX_TRX_ID], trx_id));

		/* trx_state */
		OK(field_store_string(fields[IDX_TRX_STATE],
				      row->trx_state));

		/* trx_started */
		OK(field_store_time_t(fields[IDX_TRX_STARTED],
				      (time_t) row->trx_started));

		/* trx_requested_lock_id */
		/* trx_wait_started */
		if (row->trx_wait_started != 0) {

			OK(field_store_string(
				   fields[IDX_TRX_REQUESTED_LOCK_ID],
				   trx_i_s_create_lock_id(
					   row->requested_lock_row,
					   lock_id, sizeof(lock_id))));
			/* field_store_string() sets it no notnull */

			OK(field_store_time_t(
				   fields[IDX_TRX_WAIT_STARTED],
				   (time_t) row->trx_wait_started));
			fields[IDX_TRX_WAIT_STARTED]->set_notnull();
		} else {

			fields[IDX_TRX_REQUESTED_LOCK_ID]->set_null();
			fields[IDX_TRX_WAIT_STARTED]->set_null();
		}

		/* trx_weight */
		OK(fields[IDX_TRX_WEIGHT]->store((longlong) row->trx_weight,
						 true));

		/* trx_mysql_thread_id */
		OK(fields[IDX_TRX_MYSQL_THREAD_ID]->store(
			   row->trx_mysql_thread_id));

		/* trx_query */
		OK(field_store_string(fields[IDX_TRX_QUERY],
				      row->trx_query));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_trx
@return	0 on success */
static
int
innodb_trx_init(
/*============*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_trx_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_trx_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}


UNIV_INTERN struct st_mysql_plugin	i_s_innodb_trx =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TRX"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

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
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};


UNIV_INTERN struct st_maria_plugin      i_s_innodb_trx_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_TRX"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

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
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_locks */
static ST_FIELD_INFO	innodb_locks_fields_info[] =
{
#define IDX_LOCK_ID		0
	{STRUCT_FLD(field_name,		"lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TRX_ID		1
	{STRUCT_FLD(field_name,		"lock_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_MODE		2
	{STRUCT_FLD(field_name,		"lock_mode"),
	 /* S[,GAP] X[,GAP] IS[,GAP] IX[,GAP] AUTO_INC UNKNOWN */
	 STRUCT_FLD(field_length,	32),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TYPE		3
	{STRUCT_FLD(field_name,		"lock_type"),
	 STRUCT_FLD(field_length,	32 /* RECORD|TABLE|UNKNOWN */),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TABLE		4
	{STRUCT_FLD(field_name,		"lock_table"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_INDEX		5
	{STRUCT_FLD(field_name,		"lock_index"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_SPACE		6
	{STRUCT_FLD(field_name,		"lock_space"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_PAGE		7
	{STRUCT_FLD(field_name,		"lock_page"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_REC		8
	{STRUCT_FLD(field_name,		"lock_rec"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_DATA		9
	{STRUCT_FLD(field_name,		"lock_data"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_DATA_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_locks
table with it.
@return	0 on success */
static
int
fill_innodb_locks_from_cache(
/*=========================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: MySQL client connection */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_locks_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCKS);

	for (i = 0; i < rows_num; i++) {

		i_s_locks_row_t*	row;

		/* note that the decoded database or table name is
		never expected to be longer than NAME_LEN;
		NAME_LEN for database name
		2 for surrounding quotes around database name
		NAME_LEN for table name
		2 for surrounding quotes around table name
		1 for the separating dot (.)
		9 for the #mysql50# prefix */
		char			buf[2 * NAME_LEN + 14];
		const char*		bufend;

		char			lock_trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_locks_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCKS, i);

		/* lock_id */
		trx_i_s_create_lock_id(row, lock_id, sizeof(lock_id));
		OK(field_store_string(fields[IDX_LOCK_ID],
				      lock_id));

		/* lock_trx_id */
		ut_snprintf(lock_trx_id, sizeof(lock_trx_id),
			    TRX_ID_FMT, row->lock_trx_id);
		OK(field_store_string(fields[IDX_LOCK_TRX_ID], lock_trx_id));

		/* lock_mode */
		OK(field_store_string(fields[IDX_LOCK_MODE],
				      row->lock_mode));

		/* lock_type */
		OK(field_store_string(fields[IDX_LOCK_TYPE],
				      row->lock_type));

		/* lock_table */
		bufend = innobase_convert_name(buf, sizeof(buf),
					       row->lock_table,
					       strlen(row->lock_table),
					       thd, TRUE);
		OK(fields[IDX_LOCK_TABLE]->store(buf, bufend - buf,
						 system_charset_info));

		/* lock_index */
		if (row->lock_index != NULL) {

			bufend = innobase_convert_name(buf, sizeof(buf),
						       row->lock_index,
						       strlen(row->lock_index),
						       thd, FALSE);
			OK(fields[IDX_LOCK_INDEX]->store(buf, bufend - buf,
							 system_charset_info));
			fields[IDX_LOCK_INDEX]->set_notnull();
		} else {

			fields[IDX_LOCK_INDEX]->set_null();
		}

		/* lock_space */
		OK(field_store_ulint(fields[IDX_LOCK_SPACE],
				     row->lock_space));

		/* lock_page */
		OK(field_store_ulint(fields[IDX_LOCK_PAGE],
				     row->lock_page));

		/* lock_rec */
		OK(field_store_ulint(fields[IDX_LOCK_REC],
				     row->lock_rec));

		/* lock_data */
		OK(field_store_string(fields[IDX_LOCK_DATA],
				      row->lock_data));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_locks
@return	0 on success */
static
int
innodb_locks_init(
/*==============*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_locks_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_locks_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_locks =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_LOCKS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

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
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_locks_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_LOCKS"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

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
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_lock_waits */
static ST_FIELD_INFO	innodb_lock_waits_fields_info[] =
{
#define IDX_REQUESTING_TRX_ID	0
	{STRUCT_FLD(field_name,		"requesting_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_REQUESTED_LOCK_ID	1
	{STRUCT_FLD(field_name,		"requested_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BLOCKING_TRX_ID	2
	{STRUCT_FLD(field_name,		"blocking_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BLOCKING_LOCK_ID	3
	{STRUCT_FLD(field_name,		"blocking_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the
INFORMATION_SCHEMA.innodb_lock_waits table with it.
@return	0 on success */
static
int
fill_innodb_lock_waits_from_cache(
/*==============================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: used to call
					schema_table_store_record() */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	requested_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	char	blocking_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_lock_waits_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCK_WAITS);

	for (i = 0; i < rows_num; i++) {

		i_s_lock_waits_row_t*	row;

		char	requesting_trx_id[TRX_ID_MAX_LEN + 1];
		char	blocking_trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_lock_waits_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCK_WAITS, i);

		/* requesting_trx_id */
		ut_snprintf(requesting_trx_id, sizeof(requesting_trx_id),
			    TRX_ID_FMT, row->requested_lock_row->lock_trx_id);
		OK(field_store_string(fields[IDX_REQUESTING_TRX_ID],
				      requesting_trx_id));

		/* requested_lock_id */
		OK(field_store_string(
			   fields[IDX_REQUESTED_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->requested_lock_row,
				   requested_lock_id,
				   sizeof(requested_lock_id))));

		/* blocking_trx_id */
		ut_snprintf(blocking_trx_id, sizeof(blocking_trx_id),
			    TRX_ID_FMT, row->blocking_lock_row->lock_trx_id);
		OK(field_store_string(fields[IDX_BLOCKING_TRX_ID],
				      blocking_trx_id));

		/* blocking_lock_id */
		OK(field_store_string(
			   fields[IDX_BLOCKING_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->blocking_lock_row,
				   blocking_lock_id,
				   sizeof(blocking_lock_id))));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
innodb_lock_waits_init(
/*===================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_lock_waits_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_lock_waits_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_lock_waits =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_LOCK_WAITS"),

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
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_lock_waits_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_LOCK_WAITS"),

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
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/*******************************************************************//**
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (not used) */
{
	const char*		table_name;
	int			ret;
	trx_i_s_cache_t*	cache;

	DBUG_ENTER("trx_i_s_common_fill_table");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	/* minimize the number of places where global variables are
	referenced */
	cache = trx_i_s_cache;

	/* which table we have to fill? */
	table_name = tables->schema_table_name;
	/* or table_name = tables->schema_table->table_name; */

	RETURN_IF_INNODB_NOT_STARTED(table_name);

	/* update the cache */
	trx_i_s_cache_start_write(cache);
	trx_i_s_possibly_fetch_data_into_cache(cache);
	trx_i_s_cache_end_write(cache);

	if (trx_i_s_cache_is_truncated(cache)) {

		/* XXX show warning to user if possible */
		fprintf(stderr, "Warning: data in %s truncated due to "
			"memory limit of %d bytes\n", table_name,
			TRX_I_S_MEM_LIMIT);
	}

	ret = 0;

	trx_i_s_cache_start_read(cache);

	if (innobase_strcasecmp(table_name, "innodb_trx") == 0) {

		if (fill_innodb_trx_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (innobase_strcasecmp(table_name, "innodb_locks") == 0) {

		if (fill_innodb_locks_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (innobase_strcasecmp(table_name, "innodb_lock_waits") == 0) {

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

/* Fields of the dynamic table information_schema.innodb_cmp. */
static ST_FIELD_INFO	i_s_cmp_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_size"),
	 STRUCT_FLD(field_length,	5),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Compressed Page Size"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Compressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_ops_ok"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of"
					" Successful Compressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Compressions,"
		    " in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"uncompress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Decompressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"uncompress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Decompressions,"
		    " in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};


/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp or
innodb_cmp_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_fill_low(
/*=============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

	DBUG_ENTER("i_s_cmp_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (uint i = 0; i < PAGE_ZIP_NUM_SSIZE - 1; i++) {
		page_zip_stat_t*	zip_stat = &page_zip_stat[i];

		table->field[0]->store(PAGE_ZIP_MIN_SIZE << i);

		/* The cumulated counts are not protected by any
		mutex.  Thus, some operation in page0zip.c could
		increment a counter between the time we read it and
		clear it.  We could introduce mutex protection, but it
		could cause a measureable performance hit in
		page0zip.c. */
		table->field[1]->store(zip_stat->compressed);
		table->field[2]->store(zip_stat->compressed_ok);
		table->field[3]->store(
			(ulong) (zip_stat->compressed_usec / 1000000));
		table->field[4]->store(zip_stat->decompressed);
		table->field[5]->store(
			(ulong) (zip_stat->decompressed_usec / 1000000));

		if (reset) {
			memset(zip_stat, 0, sizeof *zip_stat);
		}

		if (schema_table_store_record(thd, table)) {
			status = 1;
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_fill(
/*=========*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_reset_fill(
/*===============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp.
@return	0 on success */
static
int
i_s_cmp_init(
/*=========*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_fields_info;
	schema->fill_table = i_s_cmp_fill;

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp_reset.
@return	0 on success */
static
int
i_s_cmp_reset_init(
/*===============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_reset_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_fields_info;
	schema->fill_table = i_s_cmp_reset_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmp =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_cmp_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_CMP"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Statistics for the InnoDB compression"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_cmp_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmp_reset =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP_RESET"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression;"
		   " reset cumulated counts"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_reset_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_cmp_reset_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_CMP_RESET"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Statistics for the InnoDB compression;"
                   " reset cumulated counts"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_cmp_reset_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};
/* Fields of the dynamic table information_schema.innodb_cmpmem. */
static ST_FIELD_INFO	i_s_cmpmem_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_size"),
	 STRUCT_FLD(field_length,	5),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Buddy Block Size"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"pages_used"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Currently in Use"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"pages_free"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Currently Available"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"relocation_ops"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Relocations"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"relocation_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Relocations,"
		    " in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem or
innodb_cmpmem_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_fill_low(
/*================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

	DBUG_ENTER("i_s_cmpmem_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	//buf_pool_mutex_enter();
	mutex_enter(&zip_free_mutex);

	for (uint x = 0; x <= BUF_BUDDY_SIZES; x++) {
		buf_buddy_stat_t*	buddy_stat = &buf_buddy_stat[x];

		table->field[0]->store(BUF_BUDDY_LOW << x);
		table->field[1]->store(buddy_stat->used);
		table->field[2]->store(UNIV_LIKELY(x < BUF_BUDDY_SIZES)
				       ? UT_LIST_GET_LEN(buf_pool->zip_free[x])
				       : 0);
		table->field[3]->store((longlong) buddy_stat->relocated, true);
		table->field[4]->store(
			(ulong) (buddy_stat->relocated_usec / 1000000));

		if (reset) {
			/* This is protected by buf_pool_mutex. */
			buddy_stat->relocated = 0;
			buddy_stat->relocated_usec = 0;
		}

		if (schema_table_store_record(thd, table)) {
			status = 1;
			break;
		}
	}

	//buf_pool_mutex_exit();
	mutex_exit(&zip_free_mutex);
	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_fill(
/*============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_reset_fill(
/*==================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmpmem.
@return	0 on success */
static
int
i_s_cmpmem_init(
/*============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmpmem_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmpmem_fields_info;
	schema->fill_table = i_s_cmpmem_fill;

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmpmem_reset.
@return	0 on success */
static
int
i_s_cmpmem_reset_init(
/*==================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmpmem_reset_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmpmem_fields_info;
	schema->fill_table = i_s_cmpmem_reset_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmpmem =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMPMEM"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmpmem_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_cmpmem_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_CMPMEM"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_cmpmem_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmpmem_reset =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMPMEM_RESET"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool;"
		   " reset cumulated counts"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmpmem_reset_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_cmpmem_reset_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_CMPMEM_RESET"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool;"
                   " reset cumulated counts"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_cmpmem_reset_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, INNODB_VERSION_SHORT),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/*******************************************************************//**
Unbind a dynamic INFORMATION_SCHEMA table.
@return	0 on success */
static
int
i_s_common_deinit(
/*==============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_common_deinit");

	/* Do nothing */

	DBUG_RETURN(0);
}

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_rseg_fields_info[] =
{
	{STRUCT_FLD(field_name,		"rseg_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"zip_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"max_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"curr_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static
int
i_s_innodb_rseg_fill(
/*=================*/
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	trx_rseg_t*	rseg;

	DBUG_ENTER("i_s_innodb_rseg_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg) {
		table->field[0]->store(rseg->id);
		table->field[1]->store(rseg->space);
		table->field[2]->store(rseg->zip_size);
		table->field[3]->store(rseg->page_no);
		table->field[4]->store(rseg->max_size);
		table->field[5]->store(rseg->curr_size);

		if (schema_table_store_record(thd, table)) {
			status = 1;
			break;
		}

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}

	DBUG_RETURN(status);
}

static
int
i_s_innodb_rseg_init(
/*=================*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_rseg_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_rseg_fields_info;
	schema->fill_table = i_s_innodb_rseg_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_rseg =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_RSEG"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB rollback segment information"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_rseg_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

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

UNIV_INTERN struct st_maria_plugin      i_s_innodb_rseg_maria =
{
        /* the plugin type (a MYSQL_XXX_PLUGIN value) */
        /* int */
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

        /* pointer to type-specific plugin descriptor */
        /* void* */
        STRUCT_FLD(info, &i_s_info),

        /* plugin name */
        /* const char* */
        STRUCT_FLD(name, "INNODB_RSEG"),

        /* plugin author (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(author, plugin_author),

        /* general descriptive text (for SHOW PLUGINS) */
        /* const char* */
        STRUCT_FLD(descr, "InnoDB rollback segment information"),

        /* the plugin license (PLUGIN_LICENSE_XXX) */
        /* int */
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

        /* the function to invoke when plugin is loaded */
        /* int (*)(void*); */
        STRUCT_FLD(init, i_s_innodb_rseg_init),

        /* the function to invoke when plugin is unloaded */
        /* int (*)(void*); */
        STRUCT_FLD(deinit, i_s_common_deinit),

        /* plugin version (for SHOW PLUGINS) */
        /* unsigned int */
        STRUCT_FLD(version, 0x0100 /* 1.0 */),

        /* struct st_mysql_show_var* */
        STRUCT_FLD(status_vars, NULL),

        /* struct st_mysql_sys_var** */
        STRUCT_FLD(system_vars, NULL),

        /* string version */
        /* const char * */
        STRUCT_FLD(version_info, "1.0"),

        /* Maturity */
        /* int */
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_table_stats_info[] =
{
	{STRUCT_FLD(field_name,		"table_schema"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"table_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"rows"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"clust_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"other_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"modified"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_index_stats_info[] =
{
	{STRUCT_FLD(field_name,		"table_schema"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"table_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"index_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fields"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"row_per_keys"),
	 STRUCT_FLD(field_length,	256),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"index_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"leaf_pages"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static
int
i_s_innodb_table_stats_fill(
/*========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	int	status	= 0;
	dict_table_t*	table;

	DBUG_ENTER("i_s_innodb_table_stats_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	mutex_enter(&(dict_sys->mutex));

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		char	buf[NAME_LEN * 2 + 2];
		char*	ptr;

		if (table->stat_clustered_index_size == 0) {
			table = UT_LIST_GET_NEXT(table_LRU, table);
			continue;
		}

		buf[NAME_LEN * 2 + 1] = 0;
		strncpy(buf, table->name, NAME_LEN * 2 + 1);
		ptr = strchr(buf, '/');
		if (ptr) {
			*ptr = '\0';
			++ptr;
		} else {
			ptr = buf;
		}

		field_store_string(i_s_table->field[0], buf);
		field_store_string(i_s_table->field[1], ptr);
		i_s_table->field[2]->store(table->stat_n_rows, 1);
		i_s_table->field[3]->store(table->stat_clustered_index_size);
		i_s_table->field[4]->store(table->stat_sum_of_other_index_sizes);
		i_s_table->field[5]->store(table->stat_modified_counter);

		if (schema_table_store_record(thd, i_s_table)) {
			status = 1;
			break;
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	mutex_exit(&(dict_sys->mutex));

	DBUG_RETURN(status);
}

static
int
i_s_innodb_index_stats_fill(
/*========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	int	status	= 0;
	dict_table_t*	table;
	dict_index_t*	index;

	DBUG_ENTER("i_s_innodb_index_stats_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	mutex_enter(&(dict_sys->mutex));

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		if (table->stat_clustered_index_size == 0) {
			table = UT_LIST_GET_NEXT(table_LRU, table);
			continue;
		}

		ib_int64_t	n_rows = table->stat_n_rows;

		if (n_rows < 0) {
			n_rows = 0;
		}

		index = dict_table_get_first_index(table);

		while (index) {
			char	buff[256+1];
			char	row_per_keys[256+1];
			char	buf[NAME_LEN * 2 + 2];
			char*	ptr;
			ulint	i;

			buf[NAME_LEN * 2 + 1] = 0;
			strncpy(buf, table->name, NAME_LEN * 2 + 1);
			ptr = strchr(buf, '/');
			if (ptr) {
				*ptr = '\0';
				++ptr;
			} else {
				ptr = buf;
			}

			field_store_string(i_s_table->field[0], buf);
			field_store_string(i_s_table->field[1], ptr);
			field_store_string(i_s_table->field[2], index->name);
			i_s_table->field[3]->store(index->n_uniq);

			row_per_keys[0] = '\0';

			/* It is remained optimistic operation still for now */
			//dict_index_stat_mutex_enter(index);
			if (index->stat_n_diff_key_vals) {
				for (i = 1; i <= index->n_uniq; i++) {
					ib_int64_t	rec_per_key;
					if (index->stat_n_diff_key_vals[i]) {
						rec_per_key = n_rows / index->stat_n_diff_key_vals[i];
					} else {
						rec_per_key = n_rows;
					}
					ut_snprintf(buff, 256, (i == index->n_uniq)?"%llu":"%llu, ",
						 rec_per_key);
					strncat(row_per_keys, buff, 256 - strlen(row_per_keys));
				}
			}
			//dict_index_stat_mutex_exit(index);

			field_store_string(i_s_table->field[4], row_per_keys);

			i_s_table->field[5]->store(index->stat_index_size);
			i_s_table->field[6]->store(index->stat_n_leaf_pages);

			if (schema_table_store_record(thd, i_s_table)) {
				status = 1;
				break;
			}

			index = dict_table_get_next_index(index);
		}

		if (status == 1) {
			break;
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	mutex_exit(&(dict_sys->mutex));

	DBUG_RETURN(status);
}

static
int
i_s_innodb_table_stats_init(
/*========================*/
	void*   p)
{
	DBUG_ENTER("i_s_innodb_table_stats_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_table_stats_info;
	schema->fill_table = i_s_innodb_table_stats_fill;

	DBUG_RETURN(0);
}

static
int
i_s_innodb_index_stats_init(
/*========================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_index_stats_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_index_stats_info;
	schema->fill_table = i_s_innodb_index_stats_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_table_stats =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_TABLE_STATS"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB table statistics in memory"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_table_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_table_stats_maria =
{
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
        STRUCT_FLD(info, &i_s_info),
        STRUCT_FLD(name, "INNODB_TABLE_STATS"),
        STRUCT_FLD(author, plugin_author),
        STRUCT_FLD(descr, "InnoDB table statistics in memory"),
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
        STRUCT_FLD(init, i_s_innodb_table_stats_init),
        STRUCT_FLD(deinit, i_s_common_deinit),
        STRUCT_FLD(version, 0x0100 /* 1.0 */),
        STRUCT_FLD(status_vars, NULL),
        STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_index_stats =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_INDEX_STATS"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB index statistics in memory"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_index_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_index_stats_maria =
{
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
        STRUCT_FLD(info, &i_s_info),
        STRUCT_FLD(name, "INNODB_INDEX_STATS"),
        STRUCT_FLD(author, plugin_author),
        STRUCT_FLD(descr, "InnoDB index statistics in memory"),
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
        STRUCT_FLD(init, i_s_innodb_index_stats_init),
        STRUCT_FLD(deinit, i_s_common_deinit),
        STRUCT_FLD(version, 0x0100 /* 1.0 */),
        STRUCT_FLD(status_vars, NULL),
        STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_admin_command_info[] =
{
	{STRUCT_FLD(field_name,		"result_message"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

#ifndef INNODB_COMPATIBILITY_HOOKS
#error InnoDB needs MySQL to be built with #define INNODB_COMPATIBILITY_HOOKS
#endif

extern "C" {
char **thd_query(MYSQL_THD thd);
}

static
int
i_s_innodb_admin_command_fill(
/*==========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	char**	query_str;
	char*	ptr;
	char	quote	= '\0';
	const char*	command_head = "XTRA_";

	DBUG_ENTER("i_s_innodb_admin_command_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	if(thd_sql_command(thd) != SQLCOM_SELECT) {
		field_store_string(i_s_table->field[0],
			"SELECT command is only accepted.");
		goto end_func;
	}

	query_str = thd_query(thd);
	ptr = *query_str;
	
	for (; *ptr; ptr++) {
		if (*ptr == quote) {
			quote = '\0';
		} else if (quote) {
		} else if (*ptr == '`' || *ptr == '"') {
			quote = *ptr;
		} else {
			long	i;
			for (i = 0; command_head[i]; i++) {
				if (toupper((int)(unsigned char)(ptr[i]))
				    != toupper((int)(unsigned char)
				      (command_head[i]))) {
					goto nomatch;
				}
			}
			break;
nomatch:
			;
		}
	}

	if (!*ptr) {
		field_store_string(i_s_table->field[0],
			"No XTRA_* command in the SQL statement."
			" Please add /*!XTRA_xxxx*/ to the SQL.");
		goto end_func;
	}

	if (!strncasecmp("XTRA_HELLO", ptr, 10)) {
		/* This is example command XTRA_HELLO */

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: administration command test for XtraDB"
				" 'XTRA_HELLO' was detected.\n");

		field_store_string(i_s_table->field[0],
			"Hello!");
		goto end_func;
	}
	else if (!strncasecmp("XTRA_LRU_DUMP", ptr, 13)) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: administration command 'XTRA_LRU_DUMP'"
				" was detected.\n");

		if (buf_LRU_file_dump()) {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_DUMP was succeeded.");
		} else {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_DUMP was failed.");
		}

		goto end_func;
	}
	else if (!strncasecmp("XTRA_LRU_RESTORE", ptr, 16)) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: administration command 'XTRA_LRU_RESTORE'"
				" was detected.\n");

		if (buf_LRU_file_restore()) {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_RESTORE was succeeded.");
		} else {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_RESTORE was failed.");
		}

		goto end_func;
	}

	field_store_string(i_s_table->field[0],
		"Undefined XTRA_* command.");
	goto end_func;

end_func:
	if (schema_table_store_record(thd, i_s_table)) {
		DBUG_RETURN(1);
	} else {
		DBUG_RETURN(0);
	}
}

static
int
i_s_innodb_admin_command_init(
/*==========================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_admin_command_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_admin_command_info;
	schema->fill_table = i_s_innodb_admin_command_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_admin_command =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "XTRADB_ADMIN_COMMAND"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "XtraDB specific command acceptor"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_admin_command_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin      i_s_innodb_admin_command_maria =
{
        STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
        STRUCT_FLD(info, &i_s_info),
        STRUCT_FLD(name, "XTRADB_ADMIN_COMMAND"),
        STRUCT_FLD(author, plugin_author),
        STRUCT_FLD(descr, "XtraDB specific command acceptor"),
        STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
        STRUCT_FLD(init, i_s_innodb_admin_command_init),
        STRUCT_FLD(deinit, i_s_common_deinit),
        STRUCT_FLD(version, 0x0100 /* 1.0 */),
        STRUCT_FLD(status_vars, NULL),
        STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_STABLE)
};

static ST_FIELD_INFO	i_s_innodb_sys_tables_info[] =
{
	{STRUCT_FLD(field_name,		"SCHEMA"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"N_COLS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"TYPE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"MIX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"MIX_LEN"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"CLUSTER_NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	 END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_sys_indexes_info[] =
{
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"N_FIELDS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"TYPE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"PAGE_NO"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	 END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_sys_stats_info[] =
{
	{STRUCT_FLD(field_name,		"INDEX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"KEY_COLS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"DIFF_VALS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static
int
copy_string_field(
/*==============*/
	TABLE*			table,
	int			table_field,
	const rec_t*		rec,
	int			rec_field)
{
	int		status;
	const byte*	data;
	ulint		len;

	/*fprintf(stderr, "copy_string_field %d %d\n", table_field, rec_field);*/

	data = rec_get_nth_field_old(rec, rec_field, &len);
	if (len == UNIV_SQL_NULL) {
		table->field[table_field]->set_null();
		status = 0; /* success */
	} else {
		table->field[table_field]->set_notnull();
		status = table->field[table_field]->store(
			(char *) data, len, system_charset_info);
	}

	return status;
}

static
int
copy_name_fields(
/*=============*/
	TABLE*			table,
	int			table_field_1,
	const rec_t*		rec,
	int			rec_field)
{
	int		status;
	const byte*	data;
	ulint		len;

	data = rec_get_nth_field_old(rec, rec_field, &len);
	if (len == UNIV_SQL_NULL) {
		table->field[table_field_1]->set_null();
		table->field[table_field_1 + 1]->set_null();
		status = 0; /* success */
	} else {
		char	buf[NAME_LEN * 2 + 2];
		char*	ptr;

		if (len > NAME_LEN * 2 + 1) {
			table->field[table_field_1]->set_null();
			status = field_store_string(table->field[table_field_1 + 1],
						    "###TOO LONG NAME###");
			goto end_func;
		}

		strncpy(buf, (char*)data, len);
		buf[len] = '\0';
		ptr = strchr(buf, '/');
		if (ptr) {
			*ptr = '\0';
			++ptr;

			status = field_store_string(table->field[table_field_1], buf);
			status |= field_store_string(table->field[table_field_1 + 1], ptr);
		} else {
			table->field[table_field_1]->set_null();
			status = field_store_string(table->field[table_field_1 + 1], buf);
		}
	}

end_func:
	return status;
}

static
int
copy_int_field(
/*===========*/
	TABLE*			table,
	int			table_field,
	const rec_t*		rec,
	int			rec_field)
{
	int		status;
	const byte*	data;
	ulint		len;

	/*fprintf(stderr, "copy_int_field %d %d\n", table_field, rec_field);*/

	data = rec_get_nth_field_old(rec, rec_field, &len);
	if (len == UNIV_SQL_NULL) {
		table->field[table_field]->set_null();
		status = 0; /* success */
	} else {
		table->field[table_field]->set_notnull();
		status = table->field[table_field]->store(
			mach_read_from_4(data), true);
	}

	return status;
}

static
int
copy_id_field(
/*==========*/
	TABLE*			table,
	int			table_field,
	const rec_t*		rec,
	int			rec_field)
{
	int		status;
	const byte*	data;
	ulint		len;

	/*fprintf(stderr, "copy_id_field %d %d\n", table_field, rec_field);*/

	data = rec_get_nth_field_old(rec, rec_field, &len);
	if (len == UNIV_SQL_NULL) {
		table->field[table_field]->set_null();
		status = 0; /* success */
	} else {
		table->field[table_field]->set_notnull();
		status = table->field[table_field]->store(
			ut_conv_dulint_to_longlong(mach_read_from_8(data)), true);
	}

	return status;
}

static
int
copy_sys_tables_rec(
/*================*/
	TABLE*			table,
	const dict_index_t*	index,
	const rec_t*		rec
)
{
	int	status;
	int	field;

	/* NAME */
	field = dict_index_get_nth_col_pos(index, 0);
	status = copy_name_fields(table, 0, rec, field);
	if (status) {
		return status;
	}
	/* ID */
	field = dict_index_get_nth_col_pos(index, 1);
	status = copy_id_field(table, 2, rec, field);
	if (status) {
		return status;
	}
	/* N_COLS */
	field = dict_index_get_nth_col_pos(index, 2);
	status = copy_int_field(table, 3, rec, field);
	if (status) {
		return status;
	}
	/* TYPE */
	field = dict_index_get_nth_col_pos(index, 3);
	status = copy_int_field(table, 4, rec, field);
	if (status) {
		return status;
	}
	/* MIX_ID */
	field = dict_index_get_nth_col_pos(index, 4);
	status = copy_id_field(table, 5, rec, field);
	if (status) {
		return status;
	}
	/* MIX_LEN */
	field = dict_index_get_nth_col_pos(index, 5);
	status = copy_int_field(table, 6, rec, field);
	if (status) {
		return status;
	}
	/* CLUSTER_NAME */
	field = dict_index_get_nth_col_pos(index, 6);
	status = copy_string_field(table, 7, rec, field);
	if (status) {
		return status;
	}
	/* SPACE */
	field = dict_index_get_nth_col_pos(index, 7);
	status = copy_int_field(table, 8, rec, field);
	if (status) {
		return status;
	}

	return 0;
}

static
int
copy_sys_indexes_rec(
/*=================*/
	TABLE*			table,
	const dict_index_t*	index,
	const rec_t*		rec
)
{
	int	status;
	int	field;

	/* TABLE_ID */
	field = dict_index_get_nth_col_pos(index, 0);
	status = copy_id_field(table, 0, rec, field);
	if (status) {
		return status;
	}
	/* ID */
	field = dict_index_get_nth_col_pos(index, 1);
	status = copy_id_field(table, 1, rec, field);
	if (status) {
		return status;
	}
	/* NAME */
	field = dict_index_get_nth_col_pos(index, 2);
	status = copy_string_field(table, 2, rec, field);
	if (status) {
		return status;
	}
	/* N_FIELDS */
	field = dict_index_get_nth_col_pos(index, 3);
	status = copy_int_field(table, 3, rec, field);
	if (status) {
		return status;
	}
	/* TYPE */
	field = dict_index_get_nth_col_pos(index, 4);
	status = copy_int_field(table, 4, rec, field);
	if (status) {
		return status;
	}
	/* SPACE */
	field = dict_index_get_nth_col_pos(index, 5);
	status = copy_int_field(table, 5, rec, field);
	if (status) {
		return status;
	}
	/* PAGE_NO */
	field = dict_index_get_nth_col_pos(index, 6);
	status = copy_int_field(table, 6, rec, field);
	if (status) {
		return status;
	}

	return 0;
}

static
int
copy_sys_stats_rec(
/*===============*/
	TABLE*			table,
	const dict_index_t*	index,
	const rec_t*		rec
)
{
	int	status;
	int	field;

	/* INDEX_ID */
	field = dict_index_get_nth_col_pos(index, 0);
	status = copy_id_field(table, 0, rec, field);
	if (status) {
		return status;
	}
	/* KEY_COLS */
	field = dict_index_get_nth_col_pos(index, 1);
	status = copy_int_field(table, 1, rec, field);
	if (status) {
		return status;
	}
	/* DIFF_VALS */
	field = dict_index_get_nth_col_pos(index, 2);
	status = copy_id_field(table, 2, rec, field);
	if (status) {
		return status;
	}

	return 0;
}

static
int
i_s_innodb_schema_table_fill(
/*=========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	int		status	= 0;
	TABLE*		table	= (TABLE *) tables->table;
	const char*	table_name = tables->schema_table_name;
	dict_table_t*	innodb_table;
	dict_index_t*	index;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mtr_t		mtr;
	int		id;

	DBUG_ENTER("i_s_innodb_schema_table_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	if (innobase_strcasecmp(table_name, "innodb_sys_tables") == 0) {
		id = 0;
	} else if (innobase_strcasecmp(table_name, "innodb_sys_indexes") == 0) {
		id = 1;
	} else if (innobase_strcasecmp(table_name, "innodb_sys_stats") == 0) {
		id = 2;
	} else {
		DBUG_RETURN(1);
	}


	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	if (id == 0) {
		innodb_table = dict_table_get_low("SYS_TABLES");
	} else if (id == 1) {
		innodb_table = dict_table_get_low("SYS_INDEXES");
	} else {
		innodb_table = dict_table_get_low("SYS_STATS");
	}
	index = UT_LIST_GET_FIRST(innodb_table->indexes);

	btr_pcur_open_at_index_side(TRUE, index, BTR_SEARCH_LEAF, &pcur,
				    TRUE, &mtr);
	for (;;) {
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);

		rec = btr_pcur_get_rec(&pcur);
		if (!btr_pcur_is_on_user_rec(&pcur)) {
			/* end of index */
			btr_pcur_close(&pcur);
			mtr_commit(&mtr);
			break;
		}
		if (rec_get_deleted_flag(rec, 0)) {
			/* record marked as deleted */
			btr_pcur_close(&pcur);
			mtr_commit(&mtr);
			continue;
		}

		if (id == 0) {
			status = copy_sys_tables_rec(table, index, rec);
		} else if (id == 1) {
			status = copy_sys_indexes_rec(table, index, rec);
		} else {
			status = copy_sys_stats_rec(table, index, rec);
		}
		if (status) {
			btr_pcur_close(&pcur);
			mtr_commit(&mtr);
			break;
		}

#if 0
		btr_pcur_store_position(&pcur, &mtr);
		mtr_commit(&mtr);

		status = schema_table_store_record(thd, table);
		if (status) {
			btr_pcur_close(&pcur);
			break;
		}

		mtr_start(&mtr);
		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
#else
		status = schema_table_store_record(thd, table);
		if (status) {
			btr_pcur_close(&pcur);
			mtr_commit(&mtr);
			break;
		}
#endif
	}

	mutex_exit(&(dict_sys->mutex));

	DBUG_RETURN(status);
}

static
int
i_s_innodb_sys_tables_init(
/*=======================*/
	void*   p)
{
	DBUG_ENTER("i_s_innodb_sys_tables_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_sys_tables_info;
	schema->fill_table = i_s_innodb_schema_table_fill;

	DBUG_RETURN(0);
}

static
int
i_s_innodb_sys_indexes_init(
/*========================*/
	void*   p)
{
	DBUG_ENTER("i_s_innodb_sys_indexes_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_sys_indexes_info;
	schema->fill_table = i_s_innodb_schema_table_fill;

	DBUG_RETURN(0);
}

static
int
i_s_innodb_sys_stats_init(
/*======================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_sys_stats_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_sys_stats_info;
	schema->fill_table = i_s_innodb_schema_table_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin   i_s_innodb_sys_tables =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_TABLES"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_TABLES table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_tables_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin   i_s_innodb_sys_tables_maria =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_TABLES"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_TABLES table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_tables_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_BETA)
};

UNIV_INTERN struct st_mysql_plugin   i_s_innodb_sys_indexes =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_INDEXES"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_INDEXES table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_indexes_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin   i_s_innodb_sys_indexes_maria =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_INDEXES"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_INDEXES table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_indexes_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_BETA)
};

UNIV_INTERN struct st_mysql_plugin   i_s_innodb_sys_stats =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_STATS"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_STATS table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL)
};

UNIV_INTERN struct st_maria_plugin   i_s_innodb_sys_stats_maria =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_SYS_STATS"),
	STRUCT_FLD(author, plugin_author),
	STRUCT_FLD(descr, "InnoDB SYS_STATS table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_sys_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
        STRUCT_FLD(version_info, "1.0"),
        STRUCT_FLD(maturity, MariaDB_PLUGIN_MATURITY_BETA)
};

