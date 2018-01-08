/*
   Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/ha_ndbinfo.h"

#include <mysql/plugin.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/derror.h"     // ER_THD
#include "sql/field.h"
#include "sql/ndb_log.h"
#include "sql/ndb_tdc.h"
#include "sql/sql_class.h"
#include "sql/sql_table.h"  // build_table_filename
#include "sql/table.h"
#include "storage/ndb/src/ndbapi/NdbInfo.hpp"
#include "sql/ndb_dummy_ts.h"

static MYSQL_THDVAR_UINT(
  max_rows,                          /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specify max number of rows to fetch per roundtrip to cluster",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  10,                                /* default */
  1,                                 /* min */
  256,                               /* max */
  0                                  /* block */
);

static MYSQL_THDVAR_UINT(
  max_bytes,                         /* name */
  PLUGIN_VAR_RQCMDARG,
  "Specify approx. max number of bytes to fetch per roundtrip to cluster",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  0,                                 /* default */
  0,                                 /* min */
  65535,                             /* max */
  0                                  /* block */
);

static MYSQL_THDVAR_BOOL(
  show_hidden,                       /* name */
  PLUGIN_VAR_RQCMDARG,
  "Control if tables should be visible or not",
  NULL,                              /* check func. */
  NULL,                              /* update func. */
  false                              /* default */
);

static char* opt_ndbinfo_dbname = (char*)"ndbinfo";
static MYSQL_SYSVAR_STR(
  database,                         /* name */
  opt_ndbinfo_dbname,               /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Name of the database used by ndbinfo",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);

static char* opt_ndbinfo_table_prefix = (char*)"ndb$";
static MYSQL_SYSVAR_STR(
  table_prefix,                     /* name */
  opt_ndbinfo_table_prefix,         /* var */
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Prefix to use for all virtual tables loaded from NDB",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  NULL                              /* default */
);

static Uint32 opt_ndbinfo_version = NDB_VERSION_D;
static MYSQL_SYSVAR_UINT(
  version,                          /* name */
  opt_ndbinfo_version,              /* var */
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY | PLUGIN_VAR_NOPERSIST,
  "Compile version for ndbinfo",
  NULL,                             /* check func. */
  NULL,                             /* update func. */
  0,                                /* default */
  0,                                /* min */
  0,                                /* max */
  0                                 /* block */
);

static bool opt_ndbinfo_offline;

static
void
offline_update(THD*, SYS_VAR*, void*, const void* save)
{
  DBUG_ENTER("offline_update");

  const bool new_offline =
    (*(static_cast<const bool*>(save)) != 0);
  if (new_offline == opt_ndbinfo_offline)
  {
    // No change
    DBUG_VOID_RETURN;
  }

  // Set offline mode, any tables opened from here on will
  // be opened in the new mode
  opt_ndbinfo_offline = new_offline;

  // Close any open tables which may be in the old mode
  (void)ndb_tdc_close_cached_tables();

  DBUG_VOID_RETURN;
}

static MYSQL_SYSVAR_BOOL(
  offline,                          /* name */
  opt_ndbinfo_offline,              /* var */
  PLUGIN_VAR_NOCMDOPT,
  "Set ndbinfo in offline mode, tables and views can "
  "be opened even if they don't exist or have different "
  "definition in NDB. No rows will be returned.",
  NULL,                             /* check func. */
  offline_update,                   /* update func. */
  0                                 /* default */
);


static NdbInfo* g_ndbinfo;

extern Ndb_cluster_connection* g_ndb_cluster_connection;

static bool
ndbcluster_is_disabled(void)
{
  /*
    ndbinfo uses the same connection as ndbcluster
    to avoid using up another nodeid, this also means that
    if ndbcluster is not enabled, ndbinfo won't start
  */
  if (g_ndb_cluster_connection)
    return false;
  assert(g_ndbinfo == NULL);
  return true;
}

static handler*
create_handler(handlerton *hton, TABLE_SHARE *table, bool, MEM_ROOT *mem_root)
{
  return new (mem_root) ha_ndbinfo(hton, table);
}

struct ha_ndbinfo_impl
{
  const NdbInfo::Table* m_table;
  NdbInfoScanOperation* m_scan_op;
  Vector<const NdbInfoRecAttr *> m_columns;
  bool m_first_use;

  // Indicates if table has been opened in offline mode
  // can only be reset by closing the table
  bool m_offline;

  ha_ndbinfo_impl() :
    m_table(NULL),
    m_scan_op(NULL),
    m_first_use(true),
    m_offline(false)
  {
  }
};

ha_ndbinfo::ha_ndbinfo(handlerton *hton, TABLE_SHARE *table_arg)
: handler(hton, table_arg), m_impl(*new ha_ndbinfo_impl)
{
}

ha_ndbinfo::~ha_ndbinfo()
{
  delete &m_impl;
}

enum ndbinfo_error_codes {
  ERR_INCOMPAT_TABLE_DEF = 40001
};

static
struct error_message {
  int error;
  const char* message;
} error_messages[] = {
  { ERR_INCOMPAT_TABLE_DEF, "Incompatible table definitions" },
  { HA_ERR_NO_CONNECTION, "Connection to NDB failed" },

  { 0, 0 }
};

static
const char* find_error_message(int error)
{
  struct error_message* err = error_messages;
  while (err->error && err->message)
  {
    if (err->error == error)
    {
      assert(err->message);
      return err->message;
    }
    err++;
  }
  return NULL;
}

static int err2mysql(int error)
{
  DBUG_ENTER("err2mysql");
  DBUG_PRINT("enter", ("error: %d", error));
  assert(error != 0);
  switch(error)
  {
  case NdbInfo::ERR_ClusterFailure:
    DBUG_RETURN(HA_ERR_NO_CONNECTION);
    break;
  case NdbInfo::ERR_OutOfMemory:
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    break;
  default:
    break;
  }
  {
    char errbuf[MYSQL_ERRMSG_SIZE];
    push_warning_printf(current_thd, Sql_condition::SL_WARNING, ER_GET_ERRNO,
                        ER_THD(current_thd, ER_GET_ERRNO), error,
                        my_strerror(errbuf, MYSQL_ERRMSG_SIZE, error));
  }
  DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
}

bool ha_ndbinfo::get_error_message(int error, String *buf)
{
  DBUG_ENTER("ha_ndbinfo::get_error_message");
  DBUG_PRINT("enter", ("error: %d", error));

  const char* message = find_error_message(error);
  if (!message)
    DBUG_RETURN(false);

  buf->set(message, (uint32)strlen(message), &my_charset_bin);
  DBUG_PRINT("exit", ("message: %s", buf->ptr()));
  DBUG_RETURN(false);
}

static void
generate_sql(const NdbInfo::Table* ndb_tab, BaseString& sql)
{
  sql.appfmt("'CREATE TABLE `%s`.`%s%s` (",
             opt_ndbinfo_dbname, opt_ndbinfo_table_prefix, ndb_tab->getName());

  const char* separator = "";
  for (unsigned i = 0; i < ndb_tab->columns(); i++)
  {
    const NdbInfo::Column* col = ndb_tab->getColumn(i);

    sql.appfmt("%s", separator);
    separator = ", ";

    sql.appfmt("`%s` ", col->m_name.c_str());

    switch(col->m_type)
    {
    case NdbInfo::Column::Number:
      sql.appfmt("INT UNSIGNED");
      break;
    case NdbInfo::Column::Number64:
      sql.appfmt("BIGINT UNSIGNED");
      break;
    case NdbInfo::Column::String:
      sql.appfmt("VARCHAR(512)");
      break;
    default:
      sql.appfmt("UNKNOWN");
      assert(false);
      break;
    }
  }
  sql.appfmt(") ENGINE=NDBINFO'");
}

/*
  Push a warning with explanation of the problem as well as the
  proper SQL so the user can regenerate the table definition
*/

static void
warn_incompatible(const NdbInfo::Table* ndb_tab, bool fatal,
             const char* format, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));

static void
warn_incompatible(const NdbInfo::Table* ndb_tab, bool fatal,
             const char* format, ...)
{
  BaseString msg;
  DBUG_ENTER("warn_incompatible");
  DBUG_PRINT("enter",("table_name: %s, fatal: %d", ndb_tab->getName(), fatal));
  DBUG_ASSERT(format != NULL);

  va_list args;
  char explanation[128];
  va_start(args,format);
  vsnprintf(explanation, sizeof(explanation), format, args);
  va_end(args);

  msg.assfmt("Table '%s%s' is defined differently in NDB, %s. The "
             "SQL to regenerate is: ",
             opt_ndbinfo_table_prefix, ndb_tab->getName(), explanation);
  generate_sql(ndb_tab, msg);

  const Sql_condition::enum_severity_level level =
    (fatal ? Sql_condition::SL_WARNING : Sql_condition::SL_NOTE);
  push_warning(current_thd, level, ERR_INCOMPAT_TABLE_DEF, msg.c_str());

  DBUG_VOID_RETURN;
}

int ha_ndbinfo::create(const char*, TABLE*, HA_CREATE_INFO*, dd::Table*)
{
  DBUG_ENTER("ha_ndbinfo::create");
  DBUG_RETURN(0);
}

bool ha_ndbinfo::is_open(void) const
{
  return m_impl.m_table != NULL;
}

bool ha_ndbinfo::is_offline(void) const
{
  return m_impl.m_offline;
}

int ha_ndbinfo::open(const char* name, int mode, uint, const dd::Table*)
{
  DBUG_ENTER("ha_ndbinfo::open");
  DBUG_PRINT("enter", ("name: %s, mode: %d", name, mode));

  assert(is_closed());
  assert(!is_offline()); // Closed table can not be offline

  if (mode == O_RDWR)
  {
    if (table->db_stat & HA_TRY_READ_ONLY)
    {
      DBUG_PRINT("info", ("Telling server to use readonly mode"));
      DBUG_RETURN(EROFS); // Read only fs
    }
    // Find any commands that does not allow open readonly
    DBUG_ASSERT(false);
  }

  if (opt_ndbinfo_offline ||
      ndbcluster_is_disabled())
  {
    // Mark table as being offline and allow it to be opened
    m_impl.m_offline = true;
    DBUG_RETURN(0);
  }

  int err = g_ndbinfo->openTable(name, &m_impl.m_table);
  if (err)
  {
    assert(m_impl.m_table == 0);
    if (err == NdbInfo::ERR_NoSuchTable)
      DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
    DBUG_RETURN(err2mysql(err));
  }

  /*
    Check table def. to detect incompatible differences which should
    return an error. Differences which only generate a warning
    is checked on first use
  */
  DBUG_PRINT("info", ("Comparing MySQL's table def against NDB"));
  const NdbInfo::Table* ndb_tab = m_impl.m_table;
  for (uint i = 0; i < table->s->fields; i++)
  {
    const Field* field = table->field[i];

    // Check if field is NULLable
    if (const_cast<Field*>(field)->real_maybe_null() == false)
    {
      // Only NULLable fields supported
      warn_incompatible(ndb_tab, true,
                        "column '%s' is NOT NULL",
                        field->field_name);
      delete m_impl.m_table; m_impl.m_table= 0;
      DBUG_RETURN(ERR_INCOMPAT_TABLE_DEF);
    }

    // Check if column exist in NDB
    const NdbInfo::Column* col = ndb_tab->getColumn(field->field_name);
    if (!col)
    {
      // The column didn't exist
      continue;
    }

    // Check compatible field and column type
    bool compatible = false;
    switch(col->m_type)
    {
    case NdbInfo::Column::Number:
      if (field->type() == MYSQL_TYPE_LONG)
        compatible = true;
      break;
    case NdbInfo::Column::Number64:
      if (field->type() == MYSQL_TYPE_LONGLONG)
        compatible = true;
      break;
    case NdbInfo::Column::String:
      if (field->type() == MYSQL_TYPE_VARCHAR)
        compatible = true;
      break;
    default:
      assert(false);
      break;
    }
    if (!compatible)
    {
      // The column type is not compatible
      warn_incompatible(ndb_tab, true,
                        "column '%s' is not compatible",
                        field->field_name);
      delete m_impl.m_table; m_impl.m_table= 0;
      DBUG_RETURN(ERR_INCOMPAT_TABLE_DEF);
    }
  }

  /* Increase "ref_length" to allow a whole row to be stored in "ref" */
  ref_length = 0;
  for (uint i = 0; i < table->s->fields; i++)
    ref_length += table->field[i]->pack_length();
  DBUG_PRINT("info", ("ref_length: %u", ref_length));

  DBUG_RETURN(0);
}

int ha_ndbinfo::close(void)
{
  DBUG_ENTER("ha_ndbinfo::close");

  if (is_offline())
    DBUG_RETURN(0);

  assert(is_open());
  if (m_impl.m_table)
  {
    g_ndbinfo->closeTable(m_impl.m_table);
    m_impl.m_table = NULL;
  }
  DBUG_RETURN(0);
}

int ha_ndbinfo::rnd_init(bool scan)
{
  DBUG_ENTER("ha_ndbinfo::rnd_init");
  DBUG_PRINT("info", ("scan: %d", scan));

  if (is_offline())
  {
    push_warning(current_thd, Sql_condition::SL_NOTE, 1,
                 "'NDBINFO' has been started in offline mode "
                 "since the 'NDBCLUSTER' engine is disabled "
                 "or @@global.ndbinfo_offline is turned on "
                 "- no rows can be returned");
    DBUG_RETURN(0);
  }

  assert(is_open());

  if (m_impl.m_scan_op)
  {
    /*
      It should be impossible to come here with an already open
      scan, assumption is that rnd_end() would be called to indicate
      that the previous scan should be closed or perhaps like it says
      in decsription of rnd_init() that it "may be called two times". Once
      to open the cursor and once to position te cursor at first row.

      Unfortunately the assumption and description of rnd_init() is not
      correct. The rnd_init function is used on an open scan to reposition
      it back to first row. For ha_ndbinfo this means closing
      the scan and letting it be reopened.
    */
    assert(scan); // "only makes sense if scan=1" (from rnd_init() description)

    DBUG_PRINT("info", ("Closing scan to position it back to first row"));

    // Release the scan operation
    g_ndbinfo->releaseScanOperation(m_impl.m_scan_op);
    m_impl.m_scan_op = NULL;

    // Release pointers to the columns
    m_impl.m_columns.clear();
  }

  assert(m_impl.m_scan_op == NULL); // No scan already ongoing

  if (m_impl.m_first_use)
  {
    m_impl.m_first_use = false;

    /*
      Check table def. and generate warnings for incompatibilites
      which is allowed but should generate a warning.
      (Done this late due to different code paths in MySQL Server for
      prepared statement protocol, where warnings from 'handler::open'
      are lost).
    */
    uint fields_found_in_ndb = 0;
    const NdbInfo::Table* ndb_tab = m_impl.m_table;
    for (uint i = 0; i < table->s->fields; i++)
    {
      const Field* field = table->field[i];
      const NdbInfo::Column* col = ndb_tab->getColumn(field->field_name);
      if (!col)
      {
        // The column didn't exist
        warn_incompatible(ndb_tab, true,
                          "column '%s' does not exist",
                          field->field_name);
        continue;
      }
      fields_found_in_ndb++;
    }

    if (fields_found_in_ndb < ndb_tab->columns())
    {
      // There are more columns available in NDB
      warn_incompatible(ndb_tab, false,
                        "there are more columns available");
    }
  }

  if (!scan)
  {
    // Just an init to read using 'rnd_pos'
    DBUG_PRINT("info", ("not scan"));
    DBUG_RETURN(0);
  }

  THD* thd = current_thd;
  int err;
  NdbInfoScanOperation* scan_op = NULL;
  if ((err = g_ndbinfo->createScanOperation(m_impl.m_table,
                                            &scan_op,
                                            THDVAR(thd, max_rows),
                                            THDVAR(thd, max_bytes))) != 0)
    DBUG_RETURN(err2mysql(err));

  if ((err = scan_op->readTuples()) != 0)
  {
    // Release the scan operation
    g_ndbinfo->releaseScanOperation(scan_op);
    DBUG_RETURN(err2mysql(err));
  }

  /* Read all columns specified in read_set */
  for (uint i = 0; i < table->s->fields; i++)
  {
    Field *field = table->field[i];
    if (bitmap_is_set(table->read_set, i))
      m_impl.m_columns.push_back(scan_op->getValue(field->field_name));
    else
      m_impl.m_columns.push_back(NULL);
  }

  if ((err = scan_op->execute()) != 0)
  {
    // Release pointers to the columns
    m_impl.m_columns.clear();
    // Release the scan operation
    g_ndbinfo->releaseScanOperation(scan_op);
    DBUG_RETURN(err2mysql(err));
  }

  m_impl.m_scan_op = scan_op;
  DBUG_RETURN(0);
}

int ha_ndbinfo::rnd_end()
{
  DBUG_ENTER("ha_ndbinfo::rnd_end");

  if (is_offline())
    DBUG_RETURN(0);

  assert(is_open());

  if (m_impl.m_scan_op)
  {
    g_ndbinfo->releaseScanOperation(m_impl.m_scan_op);
    m_impl.m_scan_op = NULL;
  }
  m_impl.m_columns.clear();

  DBUG_RETURN(0);
}

int ha_ndbinfo::rnd_next(uchar *buf)
{
  int err;
  DBUG_ENTER("ha_ndbinfo::rnd_next");

  if (is_offline())
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  assert(is_open());

  if (!m_impl.m_scan_op)
  {
    /*
     It should be impossible to come here without a scan operation.
     But apparently it's not safe to assume that rnd_next() isn't
     called even though rnd_init() returned an error. Thus double check
     that the scan operation exists and bail out in case it doesn't.
    */
    DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
  }

  if ((err = m_impl.m_scan_op->nextResult()) == 0)
    DBUG_RETURN(HA_ERR_END_OF_FILE);

  if (err != 1)
    DBUG_RETURN(err2mysql(err));

  unpack_record(buf);

  DBUG_RETURN(0);
}

int ha_ndbinfo::rnd_pos(uchar *buf, uchar *pos)
{
  DBUG_ENTER("ha_ndbinfo::rnd_pos");
  assert(is_open());
  assert(m_impl.m_scan_op == NULL); // No scan started

  /* Copy the saved row into "buf" and set all fields to not null */
  memcpy(buf, pos, ref_length);
  for (uint i = 0; i < table->s->fields; i++)
    table->field[i]->set_notnull();

  DBUG_RETURN(0);
}

void ha_ndbinfo::position(const uchar *record)
{
  DBUG_ENTER("ha_ndbinfo::position");
  assert(is_open());
  assert(m_impl.m_scan_op);

  /* Save away the whole row in "ref" */
  memcpy(ref, record, ref_length);

  DBUG_VOID_RETURN;
}

int ha_ndbinfo::info(uint)
{
  DBUG_ENTER("ha_ndbinfo::info");
  DBUG_RETURN(0);
}

void
ha_ndbinfo::unpack_record(uchar *dst_row)
{
  DBUG_ENTER("ha_ndbinfo::unpack_record");
  my_ptrdiff_t dst_offset = dst_row - table->record[0];

  for (uint i = 0; i < table->s->fields; i++)
  {
    Field *field = table->field[i];
    const NdbInfoRecAttr* record = m_impl.m_columns[i];
    if (record && !record->isNULL())
    {
      field->set_notnull();
      field->move_field_offset(dst_offset);
      switch (field->type()) {

      case (MYSQL_TYPE_VARCHAR):
      {
        DBUG_PRINT("info", ("str: %s", record->c_str()));
        Field_varstring* vfield = (Field_varstring *) field;
        /* Field_bit in DBUG requires the bit set in write_set for store(). */
        my_bitmap_map *old_map =
          dbug_tmp_use_all_columns(table, table->write_set);
        (void)vfield->store(record->c_str(),
                            MIN(record->length(), field->field_length)-1,
                            field->charset());
        dbug_tmp_restore_column_map(table->write_set, old_map);
        break;
      }

      case (MYSQL_TYPE_LONG):
      {
        memcpy(field->ptr, record->ptr(), sizeof(Uint32));
        break;
      }

      case (MYSQL_TYPE_LONGLONG):
      {
        memcpy(field->ptr, record->ptr(), sizeof(Uint64));
        break;
      }

      default:
        ndb_log_error("Found unexpected field type %u", field->type());
        break;
      }

      field->move_field_offset(-dst_offset);
    }
    else
    {
      field->set_null();
    }
  }
  DBUG_VOID_RETURN;
}


static int
ndbinfo_find_files(handlerton*, THD* thd,
                   const char *db, const char*,
                   const char*, bool dir, List<LEX_STRING> *files)
{
  DBUG_ENTER("ndbinfo_find_files");
  DBUG_PRINT("enter", ("db: '%s', dir: %d", db, dir));

  const bool show_hidden = THDVAR(thd, show_hidden);

  if(show_hidden)
    DBUG_RETURN(0); // Don't filter out anything

  if (dir)
  {
    if (!ndbcluster_is_disabled())
      DBUG_RETURN(0);

    // Hide our database when ndbcluster is disabled
    LEX_STRING *dir_name;
    List_iterator<LEX_STRING> it(*files);
    while ((dir_name=it++))
    {
      if (strcmp(dir_name->str, opt_ndbinfo_dbname))
        continue;

      DBUG_PRINT("info", ("Hiding own database '%s'", dir_name->str));
      it.remove();
    }

    DBUG_RETURN(0);
  }

  DBUG_ASSERT(db);
  if (strcmp(db, opt_ndbinfo_dbname))
    DBUG_RETURN(0); // Only hide files in "our" db

  /* Hide all files that start with "our" prefix */
  LEX_STRING *file_name;
  List_iterator<LEX_STRING> it(*files);
  while ((file_name=it++))
  {
    if (is_prefix(file_name->str, opt_ndbinfo_table_prefix))
    {
      DBUG_PRINT("info", ("Hiding '%s'", file_name->str));
      it.remove();
    }
  }

  DBUG_RETURN(0);
}

static
int
ndbinfo_init(void *plugin)
{
  DBUG_ENTER("ndbinfo_init");

  handlerton *hton = (handlerton *) plugin;
  hton->create = create_handler;
  hton->flags =
    HTON_TEMPORARY_NOT_SUPPORTED |
    HTON_ALTER_NOT_SUPPORTED;
  hton->find_files = ndbinfo_find_files;

  {
    // Install dummy callbacks to avoid writing <tablename>_<id>.SDI files
    // in the data directory, those are just cumbersome having to delete
    // and or rename on the other MySQL servers
    hton->sdi_create = ndb_dummy_ts::sdi_create;
    hton->sdi_drop = ndb_dummy_ts::sdi_drop;
    hton->sdi_get_keys = ndb_dummy_ts::sdi_get_keys;
    hton->sdi_get = ndb_dummy_ts::sdi_get;
    hton->sdi_set = ndb_dummy_ts::sdi_set;
    hton->sdi_delete = ndb_dummy_ts::sdi_delete;
  }

  if (ndbcluster_is_disabled())
  {
    // Starting in limited mode since ndbcluster is disabled
     DBUG_RETURN(0);
  }

  char prefix[FN_REFLEN];
  build_table_filename(prefix, sizeof(prefix) - 1,
                       opt_ndbinfo_dbname, opt_ndbinfo_table_prefix, "", 0);
  DBUG_PRINT("info", ("prefix: '%s'", prefix));
  assert(g_ndb_cluster_connection);
  g_ndbinfo = new (std::nothrow) NdbInfo(g_ndb_cluster_connection, prefix,
                                         opt_ndbinfo_dbname,
                                         opt_ndbinfo_table_prefix);
  if (!g_ndbinfo)
  {
    ndb_log_error("Failed to create NdbInfo");
    DBUG_RETURN(1);
  }

  if (!g_ndbinfo->init())
  {
    ndb_log_error("Failed to init NdbInfo");

    delete g_ndbinfo;
    g_ndbinfo = NULL;

    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static
int
ndbinfo_deinit(void*)
{
  DBUG_ENTER("ndbinfo_deinit");

  if (g_ndbinfo)
  {
    delete g_ndbinfo;
    g_ndbinfo = NULL;
  }

  DBUG_RETURN(0);
}

SYS_VAR* ndbinfo_system_variables[]= {
  MYSQL_SYSVAR(max_rows),
  MYSQL_SYSVAR(max_bytes),
  MYSQL_SYSVAR(show_hidden),
  MYSQL_SYSVAR(database),
  MYSQL_SYSVAR(table_prefix),
  MYSQL_SYSVAR(version),
  MYSQL_SYSVAR(offline),

  NULL
};

struct st_mysql_storage_engine ndbinfo_storage_engine=
{
  MYSQL_HANDLERTON_INTERFACE_VERSION
};

struct st_mysql_plugin ndbinfo_plugin =
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &ndbinfo_storage_engine,
  "ndbinfo",
  "Sun Microsystems Inc.",
  "MySQL Cluster system information storage engine",
  PLUGIN_LICENSE_GPL,
  ndbinfo_init,               /* plugin init */
  NULL,                       /* plugin uninstall check */
  ndbinfo_deinit,             /* plugin deinit */
  0x0001,                     /* plugin version */
  NULL,                       /* status variables */
  ndbinfo_system_variables,   /* system variables */
  NULL,                       /* config options */
  0
};

template class Vector<const NdbInfoRecAttr*>;
