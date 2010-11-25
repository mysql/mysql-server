/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase XT
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
 * PBXT System tables.
 *
 */

/*

DROP TABLE IF EXISTS pbms_repository;
CREATE TABLE pbms_repository (
	Repository_id     INT COMMENT 'The reppository file number',
	Repo_blob_offset  BIGINT COMMENT 'The offset of the BLOB in the repository file',
	Blob_size         BIGINT COMMENT 'The size of the BLOB in bytes',
	Head_size         SMALLINT UNSIGNED COMMENT 'The size of the BLOB header - preceeds the BLOB data',
	Access_code       INT COMMENT 'The 4-byte authorisation code required to access the BLOB - part of the BLOB URL',
	Creation_time     TIMESTAMP COMMENT 'The time the BLOB was created',
	Last_ref_time     TIMESTAMP COMMENT 'The last time the BLOB was referenced',
	Last_access_time  TIMESTAMP COMMENT 'The last time the BLOB was accessed (read)',
	Content_type      CHAR(128) COMMENT 'The content type of the BLOB - returned by HTTP GET calls',
	Blob_data         LONGBLOB COMMENT 'The data of this BLOB'
) ENGINE=PBMS;

	PRIMARY KEY (Repository_id, Repo_blob_offset)

DROP TABLE IF EXISTS pbms_reference;
CREATE TABLE pbms_reference (
	Table_name        CHAR(64) COMMENT 'The name of the referencing table',
	Blob_id           BIGINT COMMENT 'The BLOB reference number - part of the BLOB URL',
	Column_name       CHAR(64) COMMENT 'The column name of the referencing field',
	Row_condition     VARCHAR(255) COMMENT 'This condition identifies the row in the table',
	Blob_url          VARCHAR(200) COMMENT 'The BLOB URL for HTTP GET access',
	Repository_id     INT COMMENT 'The repository file number of the BLOB',
	Repo_blob_offset  BIGINT COMMENT 'The offset in the repository file',
	Blob_size         BIGINT COMMENT 'The size of the BLOB in bytes',
	Deletion_time     TIMESTAMP COMMENT 'The time the BLOB was deleted',
	Remove_in         INT COMMENT 'The number of seconds before the reference/BLOB is removed perminently',
	Temp_log_id       INT COMMENT 'Temporary log number of the referencing deletion entry',
	Temp_log_offset   BIGINT COMMENT 'Temporary log offset of the referencing deletion entry'
) ENGINE=PBMS;

	PRIMARY KEY (Table_name, Blob_id, Column_name, Condition)
*/

#ifndef __SYSTAB_XT_H__
#define __SYSTAB_XT_H__

#include "ccutils_xt.h"
#include "discover_xt.h"
#include "thread_xt.h"

struct XTSystemTableShare;
struct XTDatabase;

class XTOpenSystemTable : public XTObject {
public:
	XTSystemTableShare		*ost_share;
	TABLE					*ost_my_table;
	struct XTDatabase		*ost_db;

	XTOpenSystemTable(XTThreadPtr self, struct XTDatabase *db, XTSystemTableShare *share, TABLE *table);
	virtual ~XTOpenSystemTable();

	virtual bool use() { return true; }
	virtual bool unuse() { return true; }
	virtual bool seqScanInit() { return true; }
	virtual bool seqScanNext(char *XT_UNUSED(buf), bool *eof) {
		*eof = true;
		return false;
	}
	virtual int	getRefLen() { return 4; }
	virtual xtWord4 seqScanPos(xtWord1 *XT_UNUSED(buf)) {
		return 0;
	}
	virtual bool seqScanRead(xtWord4 XT_UNUSED(rec_id), char *XT_UNUSED(buf)) {
		return true;
	}

private:
};

class XTLocationTable : public XTOpenSystemTable {
	u_int	lt_index;

public:
	XTLocationTable(XTThreadPtr self, struct XTDatabase *db, XTSystemTableShare *share, TABLE *table);
	virtual ~XTLocationTable();

	virtual bool use();
	virtual bool unuse();
	virtual bool seqScanInit();
	virtual bool seqScanNext(char *buf, bool *eof);
	virtual void loadRow(char *buf, xtWord4 row_id);
	virtual xtWord4 seqScanPos(xtWord1 *buf);
	virtual bool seqScanRead(xtWord4 rec_id, char *buf);
};

class XTStatisticsTable : public XTOpenSystemTable {
	u_int				tt_index;
	XTStatisticsRec		tt_statistics;

public:
	XTStatisticsTable(XTThreadPtr self, struct XTDatabase *db, XTSystemTableShare *share, TABLE *table);
	virtual ~XTStatisticsTable();

	virtual bool use();
	virtual bool unuse();
	virtual bool seqScanInit();
	virtual bool seqScanNext(char *buf, bool *eof);
	virtual void loadRow(char *buf, xtWord4 row_id);
	virtual xtWord4 seqScanPos(xtWord1 *buf);
	virtual bool seqScanRead(xtWord4 rec_id, char *buf);
};

typedef struct XTSystemTableShare {
	u_int						sts_id;
	const char					*sts_path;
	THR_LOCK					*sts_my_lock;
	DT_FIELD_INFO				*sts_info;
	DT_KEY_INFO					*sts_keys;
	xtBool						sts_exists;

	static void					startUp(XTThreadPtr self);
	static void					shutDown(XTThreadPtr self);
	
	static bool					isSystemTable(const char *table_path);
	static void					setSystemTableDeleted(const char *table_path);
	static bool					doesSystemTableExist();
	static void					createSystemTables(XTThreadPtr self, struct XTDatabase *db);
	static XTOpenSystemTable	*openSystemTable(XTThreadPtr self, const char *table_path, TABLE *table);
	static void					releaseSystemTable(XTOpenSystemTable *tab);
} XTSystemTableShareRec, *XTSystemTableSharePtr;

#endif
