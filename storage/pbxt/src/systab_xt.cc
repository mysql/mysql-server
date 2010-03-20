/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Paul McCullagh
 *
 * 2007-07-18
 *
 * H&G2JCtL
 *
 * System tables.
 *
 */

#include "xt_config.h"

#include <stdlib.h>
#include <time.h>
#ifdef DRIZZLED
#include <drizzled/server_includes.h>
#include <drizzled/current_session.h>
#endif

#include "ha_pbxt.h"
#include "systab_xt.h"
#include "discover_xt.h"
#include "table_xt.h"
#include "strutil_xt.h"
#include "database_xt.h"
#include "trace_xt.h"

#if MYSQL_VERSION_ID >= 50120
#define byte uchar
#endif

/*
 * -------------------------------------------------------------------------
 * SYSTEM TABLE DEFINITIONS
 */

//--------------------------------
static DT_FIELD_INFO xt_location_info[] =
{
	{ "Path",				128,	NULL, MYSQL_TYPE_VARCHAR,	(CHARSET_INFO *) system_charset_info,	0,	"The location of PBXT tables"},
	{ "Table_count",		0,		NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,		"The number of PBXT table in this location"},
	{ NULL,					0,		NULL, MYSQL_TYPE_STRING,	NULL,					0, NULL}
};

static DT_FIELD_INFO xt_statistics_info[] =
{
	{ "ID",					0,	NULL, MYSQL_TYPE_LONG,			NULL,					NOT_NULL_FLAG,		"The ID of the statistic"},
	{ "Name",				40,	NULL, MYSQL_TYPE_VARCHAR,		(CHARSET_INFO *) system_charset_info,	0,	"The name of the statistic"},
	{ "Value",				0,	NULL,	MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,		"The accumulated value"},
	{ NULL,					0,	NULL, MYSQL_TYPE_STRING,		NULL,					0, NULL}
};

/*
static DT_FIELD_INFO xt_reference_info[] =
{
	{"Table_name",		128,					NULL, MYSQL_TYPE_STRING,	system_charset_info,	NOT_NULL_FLAG,	"The name of the referencing table"},
	{"Blob_id",			NULL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The BLOB reference number - part of the BLOB URL"},
	{"Column_name",		50,						NULL, MYSQL_TYPE_STRING,	system_charset_info,	NOT_NULL_FLAG,	"The column name of the referencing field"},
	{"Row_condition",	50,						NULL, MYSQL_TYPE_VARCHAR,	system_charset_info,	0,				"This condition identifies the row in the table"},
	{"Blob_url",		50,						NULL, MYSQL_TYPE_VARCHAR,	system_charset_info,	NOT_NULL_FLAG,	"The BLOB URL for HTTP GET access"},
	{"Repository_id",	NULL,					NULL, MYSQL_TYPE_LONG,		NULL,					NOT_NULL_FLAG,	"The repository file number of the BLOB"},
	{"Repo_blob_offset",NULL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The offset in the repository file"},
	{"Blob_size",		NULL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The size of the BLOB in bytes"},
	{"Deletion_time",	NULL,					NULL, MYSQL_TYPE_TIMESTAMP,	NULL,					0,				"The time the BLOB was deleted"},
	{"Remove_in",		NULL,					NULL, MYSQL_TYPE_LONG,		NULL,					0,				"The number of seconds before the reference/BLOB is removed perminently"},
	{"Temp_log_id",		NULL,					NULL, MYSQL_TYPE_LONG,		NULL,					0,				"Temporary log number of the referencing deletion entry"},
	{"Temp_log_offset",	NULL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					0,				"Temporary log offset of the referencing deletion entry"},
	{NULL,				NULL,					NULL, MYSQL_TYPE_STRING,	NULL, 0,											NULL}
};
*/

#define XT_SYSTAB_INVALID			0
#define XT_SYSTAB_LOCATION_ID		1
#define XT_SYSTAB_STATISTICS_ID		2

static THR_LOCK sys_location_lock;
static THR_LOCK sys_statistics_lock;
static xtBool	sys_lock_inited = FALSE;

static XTSystemTableShareRec xt_internal_tables[] =
{
	{ XT_SYSTAB_LOCATION_ID,	"pbxt.location", &sys_location_lock, xt_location_info, NULL, FALSE},
	{ XT_SYSTAB_STATISTICS_ID,	"pbxt.statistics", &sys_statistics_lock, xt_statistics_info, NULL, FALSE},
	{ XT_SYSTAB_INVALID,		NULL, NULL, NULL, NULL, FALSE}
};


/*
static int pbms_discover_handler(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen)
{
	int err = 1, i = 0;
	MY_STAT stat_info;

	// Check that the database exists!
	if ((!db) || ! my_stat(db,&stat_info,MYF(0)))
		return err;
		
	while (pbms_internal_tables[i].name) {
		if (!strcasecmp(name, pbms_internal_tables[i].name)) {
			err = ms_create_table_frm(hton, thd, db, name, pbms_internal_tables[i].info, pbms_internal_tables[i].keys, frmblob, frmlen);
			break;
		}
		i++;
	}
	
	return err;
}
*/

/*
 * -------------------------------------------------------------------------
 * MYSQL UTILITIES
 */

static void xt_my_set_notnull_in_record(Field *field, char *record)
{
	if (field->null_ptr)
		record[(uint) (field->null_ptr - (uchar *) field->table->record[0])] &= (uchar) ~field->null_bit;
}

/*
 * -------------------------------------------------------------------------
 * OPEN SYSTEM TABLES
 */

XTOpenSystemTable::XTOpenSystemTable(XTThreadPtr self, XTDatabaseHPtr db, XTSystemTableShare *share, TABLE *table):
XTObject()
{
	ost_share = share;
	ost_my_table = table;
	ost_db = db;
	xt_heap_reference(self, db);
}

XTOpenSystemTable::~XTOpenSystemTable()
{
	XTSystemTableShare::releaseSystemTable(this);
}

/*
 * -------------------------------------------------------------------------
 * LOCATION TABLE
 */

XTLocationTable::XTLocationTable(XTThreadPtr self, XTDatabaseHPtr db, XTSystemTableShare *share, TABLE *table):
XTOpenSystemTable(self, db, share, table)
{
}

XTLocationTable::~XTLocationTable()
{
	unuse();
}

bool XTLocationTable::use()
{
	return true;
}

bool XTLocationTable::unuse()
{
	return true;
}


bool XTLocationTable::seqScanInit()
{
	lt_index = 0;
	return true;
}

bool XTLocationTable::seqScanNext(char *buf, bool *eof)
{
	bool ok = true;

	*eof = false;

	xt_ht_lock(NULL, ost_db->db_tables);
	if (lt_index >= xt_sl_get_size(ost_db->db_table_paths)) {
		ok = false;
		*eof = true;
		goto done;
	}
	loadRow(buf, lt_index);
	lt_index++;

	done:
	xt_ht_unlock(NULL, ost_db->db_tables);
	return ok;
#ifdef xxx
	csWord4		last_access;
	csWord4		last_ref;
	csWord4		creation_time;
	csWord4		access_code;
	csWord2		cont_type;
	size_t		ref_size;
	csWord2		head_size;
	csWord8		blob_size;
	uint32		len;
	Field		*curr_field;
	byte		*save;
	MX_BITMAP	*save_write_set;

	last_access = CS_GET_DISK_4(blob->rb_last_access_4);
	last_ref = CS_GET_DISK_4(blob->rb_last_ref_4);
	creation_time = CS_GET_DISK_4(blob->rb_create_time_4);
	cont_type = CS_GET_DISK_2(blob->rb_cont_type_2);
	ref_size = CS_GET_DISK_1(blob->rb_ref_size_1);
	head_size = CS_GET_DISK_2(blob->rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob->rb_blob_size_6);
	access_code = CS_GET_DISK_4(blob->rb_auth_code_4);

	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
	save_write_set = table->write_set;
	table->write_set = NULL;

	memset(buf, 0xFF, table->s->null_bytes);
 	for (Field **field=table->field ; *field ; field++) {
 		curr_field = *field;

		save = curr_field->ptr;
#if MYSQL_VERSION_ID < 50114
		curr_field->ptr = (byte *) buf + curr_field->offset();
#else
		curr_field->ptr = (byte *) buf + curr_field->offset(curr_field->table->record[0]);
#endif
		switch (curr_field->field_name[0]) {
			case 'A':
				ASSERT(strcmp(curr_field->field_name, "Access_code") == 0);
				curr_field->store(access_code, true);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
			case 'R':
				switch (curr_field->field_name[6]) {
					case 't':
						// Repository_id     INT
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRepoFile->myRepo->getRepoID(), true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
					case 'l':
						// Repo_blob_offset  BIGINT
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iRepoOffset, true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
				}
				break;
			case 'B':
				switch (curr_field->field_name[5]) {
					case 's':
						// Blob_size         BIGINT
						ASSERT(strcmp(curr_field->field_name, "Blob_size") == 0);
						curr_field->store(blob_size, true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
					case 'd':
						// Blob_data         LONGBLOB
						ASSERT(strcmp(curr_field->field_name, "Blob_data") == 0);
						if (blob_size <= 0xFFFFFFF) {
							iBlobBuffer->setLength((u_int) blob_size);
							len = iRepoFile->read(iBlobBuffer->getBuffer(0), iRepoOffset + head_size, (size_t) blob_size, 0);
							((Field_blob *) curr_field)->set_ptr(len, (byte *) iBlobBuffer->getBuffer(0));
							xt_my_set_notnull_in_record(curr_field, buf);
						}
						break;
				}
				break;
			case 'H':
				// Head_size         SMALLINT UNSIGNED
				ASSERT(strcmp(curr_field->field_name, "Head_size") == 0);
				curr_field->store(head_size, true);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
			case 'C':
				switch (curr_field->field_name[1]) {
					case 'r':
						// Creation_time     TIMESTAMP
						ASSERT(strcmp(curr_field->field_name, "Creation_time") == 0);
						curr_field->store(ms_my_1970_to_mysql_time(creation_time), true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
					case 'o':
						// Content_type      CHAR(128)
						ASSERT(strcmp(curr_field->field_name, "Content_type") == 0);
						CSString *cont_type_str = ost_share->mySysDatabase->getContentType(cont_type);
						if (cont_type_str) {
							curr_field->store(cont_type_str->getCString(), cont_type_str->length(), &my_charset_utf8_general_ci);
							cont_type_str->release();
							xt_my_set_notnull_in_record(curr_field, buf);
						}
						break;
				}
				break;
			case 'L':
				switch (curr_field->field_name[5]) {
					case 'r':
						// Last_ref_time     TIMESTAMP
						ASSERT(strcmp(curr_field->field_name, "Last_ref_time") == 0);
						curr_field->store(ms_my_1970_to_mysql_time(last_ref), true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
					case 'a':
						// Last_access_time  TIMESTAMP
						ASSERT(strcmp(curr_field->field_name, "Last_access_time") == 0);
						curr_field->store(ms_my_1970_to_mysql_time(last_access), true);
						xt_my_set_notnull_in_record(curr_field, buf);
						break;
				}
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return true;
#endif
}

void XTLocationTable::loadRow(char *buf, xtWord4 row_id)
{
	TABLE			*table = ost_my_table;
	Field			*curr_field;
	XTTablePathPtr	tp_ptr;
	byte			*save;
	MX_BITMAP		*save_write_set;

	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
	save_write_set = table->write_set;
	table->write_set = NULL;

	memset(buf, 0xFF, table->s->null_bytes);

	tp_ptr = *((XTTablePathPtr *) xt_sl_item_at(ost_db->db_table_paths, row_id));

 	for (Field **field=table->field ; *field ; field++) {
 		curr_field = *field;

		save = curr_field->ptr;
#if MYSQL_VERSION_ID < 50114
		curr_field->ptr = (byte *) buf + curr_field->offset();
#else
		curr_field->ptr = (byte *) buf + curr_field->offset(curr_field->table->record[0]);
#endif
		switch (curr_field->field_name[0]) {
			case 'P':
				// Path			VARCHAR(128)
				ASSERT_NS(strcmp(curr_field->field_name, "Path") == 0);
				curr_field->store(tp_ptr->tp_path, strlen(tp_ptr->tp_path), &my_charset_utf8_general_ci);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
			case 'T':
				// Table_count   INT
				ASSERT_NS(strcmp(curr_field->field_name, "Table_count") == 0);
				curr_field->store(tp_ptr->tp_tab_count, true);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
		}
		curr_field->ptr = save;
	}
	table->write_set = save_write_set;
}

xtWord4 XTLocationTable::seqScanPos(xtWord1 *XT_UNUSED(buf))
{
	return lt_index-1;
}

bool XTLocationTable::seqScanRead(xtWord4 rec_id, char *buf)
{
	loadRow(buf, rec_id);
	return true;
}

/*
 * -------------------------------------------------------------------------
 * STATISTICS TABLE
 */

XTStatisticsTable::XTStatisticsTable(XTThreadPtr self, XTDatabaseHPtr db, XTSystemTableShare *share, TABLE *table):
XTOpenSystemTable(self, db, share, table)
{
}

XTStatisticsTable::~XTStatisticsTable()
{
	unuse();
}

bool XTStatisticsTable::use()
{
	return true;
}

bool XTStatisticsTable::unuse()
{
	return true;
}


bool XTStatisticsTable::seqScanInit()
{
	tt_index = 0;
	xt_gather_statistics(&tt_statistics);
	return true;
}

bool XTStatisticsTable::seqScanNext(char *buf, bool *eof)
{
	bool ok = true;

	*eof = false;

	if (tt_index >= XT_STAT_CURRENT_MAX) {
		ok = false;
		*eof = true;
		goto done;
	}
	loadRow(buf, tt_index);
	tt_index++;

	done:
	return ok;
}

void XTStatisticsTable::loadRow(char *buf, xtWord4 rec_id)
{
	TABLE			*table = ost_my_table;
	MX_BITMAP		*save_write_set;
	Field			*curr_field;
	byte			*save;
	const char		*stat_name;
	u_llong			stat_value;

	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
	save_write_set = table->write_set;
	table->write_set = NULL;

	memset(buf, 0xFF, table->s->null_bytes);

	stat_name = xt_get_stat_meta_data(rec_id)->sm_name;
	stat_value = xt_get_statistic(&tt_statistics, ost_db, rec_id);

 	for (Field **field=table->field ; *field ; field++) {
 		curr_field = *field;

		save = curr_field->ptr;
#if MYSQL_VERSION_ID < 50114
		curr_field->ptr = (byte *) buf + curr_field->offset();
#else
		curr_field->ptr = (byte *) buf + curr_field->offset(curr_field->table->record[0]);
#endif
		switch (curr_field->field_name[0]) {
			case 'I':
				// Value BIGINT
				ASSERT_NS(strcmp(curr_field->field_name, "ID") == 0);
				curr_field->store(rec_id+1, true);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
			case 'N':
				// Name VARCHAR(40)
				ASSERT_NS(strcmp(curr_field->field_name, "Name") == 0);
				curr_field->store(stat_name, strlen(stat_name), &my_charset_utf8_general_ci);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
			case 'V':
				// Value BIGINT
				ASSERT_NS(strcmp(curr_field->field_name, "Value") == 0);
				curr_field->store(stat_value, true);
				xt_my_set_notnull_in_record(curr_field, buf);
				break;
		}
		curr_field->ptr = save;
	}
	table->write_set = save_write_set;
}

xtWord4 XTStatisticsTable::seqScanPos(xtWord1 *XT_UNUSED(buf))
{
	return tt_index-1;
}

bool XTStatisticsTable::seqScanRead(xtWord4 rec_id, char *buf)
{
	loadRow(buf, rec_id);
	return true;
}

/*
 * -------------------------------------------------------------------------
 * SYSTEM TABLE SHARES
 */

static void st_path_to_table_name(size_t size, char *buffer, const char *path)
{
	char *str;

	xt_strcpy(size, buffer, xt_last_2_names_of_path(path));
	xt_remove_extension(buffer);
	if ((str = strchr(buffer, '\\')))
		*str = '.';
	if ((str = strchr(buffer, '/')))
		*str = '.';
}

void XTSystemTableShare::startUp(XTThreadPtr XT_UNUSED(self))
{
	thr_lock_init(&sys_location_lock);
	thr_lock_init(&sys_statistics_lock);
	sys_lock_inited = TRUE;
}

void XTSystemTableShare::shutDown(XTThreadPtr XT_UNUSED(self))
{
	if (sys_lock_inited) {
		thr_lock_delete(&sys_location_lock);
		thr_lock_delete(&sys_statistics_lock);
		sys_lock_inited = FALSE;
	}
}

bool XTSystemTableShare::isSystemTable(const char *table_path)
{
	int		i = 0;
	char	tab_name[100];

	st_path_to_table_name(100, tab_name, table_path);
	while (xt_internal_tables[i].sts_path) {
		if (strcasecmp(tab_name, xt_internal_tables[i].sts_path) == 0)
			return true;
		i++;
	}
	return false;
}

void XTSystemTableShare::setSystemTableDeleted(const char *table_path)
{
	int		i = 0;
	char	tab_name[100];

	st_path_to_table_name(100, tab_name, table_path);
	while (xt_internal_tables[i].sts_path) {
		if (strcasecmp(tab_name, xt_internal_tables[i].sts_path) == 0) {
			xt_internal_tables[i].sts_exists = FALSE;
			break;
		}
		i++;
	}
}

bool XTSystemTableShare::doesSystemTableExist()
{
	int i = 0;

	while (xt_internal_tables[i].sts_path) {
		if (xt_internal_tables[i].sts_exists)
			return true;
		i++;
	}
	return false;
}

void XTSystemTableShare::createSystemTables(XTThreadPtr XT_UNUSED(self), XTDatabaseHPtr XT_UNUSED(db))
{
	int		i = 0;

	while (xt_internal_tables[i].sts_path) {
		if (!xt_create_table_frm(pbxt_hton,
			current_thd, "pbxt",
			strchr(xt_internal_tables[i].sts_path, '.') + 1,
			xt_internal_tables[i].sts_info,
			xt_internal_tables[i].sts_keys,
			TRUE /*do not recreate*/))
			xt_internal_tables[i].sts_exists = TRUE;
		i++;
	}
}

XTOpenSystemTable *XTSystemTableShare::openSystemTable(XTThreadPtr self, const char *table_path, TABLE *table)
{
	XTSystemTableShare	*share;
	XTOpenSystemTable	*otab = NULL;
	int					i = 0;
	char				tab_name[100];

	st_path_to_table_name(100, tab_name, table_path);
	while (xt_internal_tables[i].sts_path) {
		if (strcasecmp(tab_name, xt_internal_tables[i].sts_path) == 0) {
			share = &xt_internal_tables[i];
			goto found;
		}
		i++;
	}
	return NULL;

	found:
	share->sts_exists = TRUE;
	switch (share->sts_id) {
		case XT_SYSTAB_LOCATION_ID:
			if (!(otab = new XTLocationTable(self, self->st_database, share, table)))
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			break;
		case XT_SYSTAB_STATISTICS_ID:
			if (!(otab = new XTStatisticsTable(self, self->st_database, share, table)))
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			break;
		default:
			xt_throw_taberr(XT_CONTEXT, XT_ERR_TABLE_NOT_FOUND, (XTPathStrPtr) table_path);
			break;
	}	

	return otab;
}

void XTSystemTableShare::releaseSystemTable(XTOpenSystemTable *tab)
{
	if (tab->ost_db) {
		XTThreadPtr self = xt_get_self();

		try_(a) {
			xt_heap_release(self, tab->ost_db);
		}
		catch_(a) {
		}
		cont_(a);
		tab->ost_db = NULL;
	}
}
