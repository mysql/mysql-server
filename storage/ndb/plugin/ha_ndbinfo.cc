/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ha_ndbinfo.h"

#include <algorithm>  // std::min(),std::max()
#include <vector>

#include <mysql/plugin.h>

#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/sql_table.h"  // build_table_filename
#include "sql/table.h"
#include "storage/ndb/include/ndbapi/ndb_cluster_connection.hpp"
#include "storage/ndb/plugin/ndb_dummy_ts.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_tdc.h"
#include "storage/ndb/src/ndbapi/NdbInfo.hpp"

static MYSQL_THDVAR_UINT(
    max_rows, /* name */
    PLUGIN_VAR_RQCMDARG,
    "Specify max number of rows to fetch per roundtrip to cluster",
    nullptr, /* check func. */
    nullptr, /* update func. */
    10,      /* default */
    1,       /* min */
    256,     /* max */
    0        /* block */
);

static MYSQL_THDVAR_UINT(
    max_bytes, /* name */
    PLUGIN_VAR_RQCMDARG,
    "Specify approx. max number of bytes to fetch per roundtrip to cluster",
    nullptr, /* check func. */
    nullptr, /* update func. */
    0,       /* default */
    0,       /* min */
    65535,   /* max */
    0        /* block */
);

static MYSQL_THDVAR_BOOL(show_hidden, /* name */
                         PLUGIN_VAR_RQCMDARG,
                         "Control if tables should be visible or not",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         false    /* default */
);

static char *opt_ndbinfo_dbname = const_cast<char *>("ndbinfo");
static MYSQL_SYSVAR_STR(database,           /* name */
                        opt_ndbinfo_dbname, /* var */
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_NOCMDOPT,
                        "Name of the database used by ndbinfo",
                        nullptr, /* check func. */
                        nullptr, /* update func. */
                        nullptr  /* default */
);

static char *opt_ndbinfo_table_prefix = const_cast<char *>("ndb$");
static MYSQL_SYSVAR_STR(table_prefix,             /* name */
                        opt_ndbinfo_table_prefix, /* var */
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_NOCMDOPT,
                        "Prefix used for all virtual tables loaded from NDB",
                        nullptr, /* check func. */
                        nullptr, /* update func. */
                        nullptr  /* default */
);

static Uint32 opt_ndbinfo_version = NDB_VERSION_D;
static MYSQL_SYSVAR_UINT(version,             /* name */
                         opt_ndbinfo_version, /* var */
                         PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY |
                             PLUGIN_VAR_NOPERSIST,
                         "Compile version for ndbinfo",
                         nullptr, /* check func. */
                         nullptr, /* update func. */
                         0,       /* default */
                         0,       /* min */
                         0,       /* max */
                         0        /* block */
);

static bool opt_ndbinfo_offline;

static void offline_update(THD *, SYS_VAR *, void *, const void *save) {
  DBUG_TRACE;

  const bool new_offline = (*(static_cast<const bool *>(save)) != 0);
  if (new_offline == opt_ndbinfo_offline) {
    // No change
    return;
  }

  // Set offline mode, any tables opened from here on will
  // be opened in the new mode
  opt_ndbinfo_offline = new_offline;

  // Close any open tables which may be in the old mode
  (void)ndb_tdc_close_cached_tables();
}

static MYSQL_SYSVAR_BOOL(offline,             /* name */
                         opt_ndbinfo_offline, /* var */
                         PLUGIN_VAR_NOCMDOPT,
                         "Set ndbinfo in offline mode, tables and views can "
                         "be opened even if they don't exist or have different "
                         "definition in NDB. No rows will be returned.",
                         nullptr,        /* check func. */
                         offline_update, /* update func. */
                         0               /* default */
);

static NdbInfo *g_ndbinfo;

extern Ndb_cluster_connection *g_ndb_cluster_connection;

static bool ndbcluster_is_disabled(void) {
  /*
    ndbinfo uses the same connection as ndbcluster
    to avoid using up another nodeid, this also means that
    if ndbcluster is not enabled, ndbinfo won't start
  */
  if (g_ndb_cluster_connection) return false;
  assert(g_ndbinfo == nullptr);
  return true;
}

static handler *create_handler(handlerton *hton, TABLE_SHARE *table, bool,
                               MEM_ROOT *mem_root) {
  return new (mem_root) ha_ndbinfo(hton, table);
}

struct ha_ndbinfo_impl {
  const NdbInfo::Table *m_table;
  NdbInfoScanOperation *m_scan_op;
  std::vector<const NdbInfoRecAttr *> m_columns;
  bool m_first_use;

  enum struct Table_Status {
    CLOSED,
    OFFLINE_NDBINFO_OFFLINE,  // Table offline as ndbinfo is offline
    OFFLINE_DISCONNECTED,     // Table offline as cluster is disconnected
    OFFLINE_UPGRADING,        // Table offline due to an ongoing upgrade
    OPEN                      // Table is online and accessible
  } m_status;

  ha_ndbinfo_impl()
      : m_table(nullptr),
        m_scan_op(nullptr),
        m_first_use(true),
        m_status(Table_Status::CLOSED) {}
};

ha_ndbinfo::ha_ndbinfo(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg), m_impl(*new ha_ndbinfo_impl) {}

ha_ndbinfo::~ha_ndbinfo() { delete &m_impl; }

enum ndbinfo_error_codes { ERR_INCOMPAT_TABLE_DEF = 40001 };

static struct error_message {
  int error;
  const char *message;
} error_messages[] = {
    {ERR_INCOMPAT_TABLE_DEF, "Incompatible table definitions"},
    {HA_ERR_NO_CONNECTION, "Connection to NDB failed"},

    {0, nullptr}};

static const char *find_error_message(int error) {
  struct error_message *err = error_messages;
  while (err->error && err->message) {
    if (err->error == error) {
      assert(err->message);
      return err->message;
    }
    err++;
  }
  return nullptr;
}

static int err2mysql(int error) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));
  assert(error != 0);
  switch (error) {
    case NdbInfo::ERR_ClusterFailure:
      return HA_ERR_NO_CONNECTION;
      break;
    case NdbInfo::ERR_OutOfMemory:
      return HA_ERR_OUT_OF_MEM;
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
  return HA_ERR_INTERNAL_ERROR;
}

bool ha_ndbinfo::get_error_message(int error, String *buf) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));

  const char *message = find_error_message(error);
  if (!message) return false;

  buf->set(message, (uint32)strlen(message), &my_charset_bin);
  DBUG_PRINT("exit", ("message: %s", buf->ptr()));
  return false;
}

static void generate_sql(const NdbInfo::Table *ndb_tab, BaseString &sql) {
  sql.appfmt("'CREATE TABLE `%s`.`%s%s` (", opt_ndbinfo_dbname,
             opt_ndbinfo_table_prefix, ndb_tab->getName());

  const char *separator = "";
  for (unsigned i = 0; i < ndb_tab->columns(); i++) {
    const NdbInfo::Column *col = ndb_tab->getColumn(i);

    sql.appfmt("%s", separator);
    separator = ", ";

    sql.appfmt("`%s` ", col->m_name.c_str());

    switch (col->m_type) {
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

static void warn_incompatible(const NdbInfo::Table *ndb_tab, bool fatal,
                              const char *format, ...)
    MY_ATTRIBUTE((format(printf, 3, 4)));

static void warn_incompatible(const NdbInfo::Table *ndb_tab, bool fatal,
                              const char *format, ...) {
  BaseString msg;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("table_name: %s, fatal: %d", ndb_tab->getName(), fatal));
  assert(format != nullptr);

  va_list args;
  char explanation[128];
  va_start(args, format);
  vsnprintf(explanation, sizeof(explanation), format, args);
  va_end(args);

  msg.assfmt(
      "Table '%s%s' is defined differently in NDB, %s. The "
      "SQL to regenerate is: ",
      opt_ndbinfo_table_prefix, ndb_tab->getName(), explanation);
  generate_sql(ndb_tab, msg);

  const Sql_condition::enum_severity_level level =
      (fatal ? Sql_condition::SL_WARNING : Sql_condition::SL_NOTE);
  push_warning(current_thd, level, ERR_INCOMPAT_TABLE_DEF, msg.c_str());
}

int ha_ndbinfo::create(const char *, TABLE *, HA_CREATE_INFO *, dd::Table *) {
  DBUG_TRACE;
  return 0;
}

bool ha_ndbinfo::is_open() const {
  return m_impl.m_status == ha_ndbinfo_impl::Table_Status::OPEN;
}

bool ha_ndbinfo::is_closed() const {
  return m_impl.m_status == ha_ndbinfo_impl::Table_Status::CLOSED;
}

bool ha_ndbinfo::is_offline() const {
  return m_impl.m_status ==
             ha_ndbinfo_impl::Table_Status::OFFLINE_NDBINFO_OFFLINE ||
         m_impl.m_status ==
             ha_ndbinfo_impl::Table_Status::OFFLINE_DISCONNECTED ||
         m_impl.m_status == ha_ndbinfo_impl::Table_Status::OFFLINE_UPGRADING;
}

int ha_ndbinfo::open(const char *name, int mode, uint, const dd::Table *) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("name: %s, mode: %d", name, mode));

  assert(is_closed());

  if (mode == O_RDWR) {
    if (table->db_stat & HA_TRY_READ_ONLY) {
      DBUG_PRINT("info", ("Telling server to use readonly mode"));
      return EROFS;  // Read only fs
    }
    // Find any commands that does not allow open readonly
    assert(false);
  }

  if (opt_ndbinfo_offline || ndbcluster_is_disabled()) {
    // Mark table as being offline and allow it to be opened
    m_impl.m_status = ha_ndbinfo_impl::Table_Status::OFFLINE_NDBINFO_OFFLINE;
    return 0;
  }

  int err = g_ndbinfo->openTable(name, &m_impl.m_table);
  if (err) {
    assert(m_impl.m_table == nullptr);
    ndb_log_info("NdbInfo::openTable failed for %s", name);
    if (err == NdbInfo::ERR_NoSuchTable) {
      if (g_ndb_cluster_connection->get_min_db_version() < NDB_VERSION_D) {
        // The table does not exist but there is a data node from a lower
        // version connected to this Server. This is in the middle of an upgrade
        // and the possibility is that the data node does not have this ndbinfo
        // table definition yet. So we open this table in an offline mode so as
        // to allow the upgrade to continue further. The table will be reopened
        // properly after the upgrade completes.
        m_impl.m_status = ha_ndbinfo_impl::Table_Status::OFFLINE_UPGRADING;
        return 0;
      }
      return HA_ERR_NO_SUCH_TABLE;
    }
    if (err == NdbInfo::ERR_ClusterFailure) {
      /* Not currently connected to cluster, but open in offline mode */
      m_impl.m_status = ha_ndbinfo_impl::Table_Status::OFFLINE_DISCONNECTED;
      return 0;
    }
    return err2mysql(err);
  }

  /*
    Check table def. to detect incompatible differences which should
    return an error. Differences which only generate a warning
    is checked on first use
  */
  DBUG_PRINT("info", ("Comparing MySQL's table def against NDB"));
  const NdbInfo::Table *ndb_tab = m_impl.m_table;
  for (uint i = 0; i < table->s->fields; i++) {
    const Field *field = table->field[i];

    // Check that field is NULLable, unless the table is virtual.
    if ((const_cast<Field *>(field)->is_nullable() == false) &&
        !m_impl.m_table->getVirtualTable()) {
      warn_incompatible(ndb_tab, true, "column '%s' is NOT NULL",
                        field->field_name);
      delete m_impl.m_table;
      m_impl.m_table = nullptr;
      return ERR_INCOMPAT_TABLE_DEF;
    }

    // Check if column exist in NDB
    const NdbInfo::Column *col = ndb_tab->getColumn(field->field_name);
    if (!col) {
      // The column didn't exist
      continue;
    }

    // Check compatible field and column type
    bool compatible = false;
    switch (col->m_type) {
      case NdbInfo::Column::Number:
        if (field->type() == MYSQL_TYPE_LONG ||
            field->real_type() == MYSQL_TYPE_ENUM ||
            field->real_type() == MYSQL_TYPE_SET)
          compatible = true;
        stats.mean_rec_length += 4;
        break;
      case NdbInfo::Column::Number64:
        if (field->type() == MYSQL_TYPE_LONGLONG) compatible = true;
        stats.mean_rec_length += 8;
        break;
      case NdbInfo::Column::String:
        if (field->type() == MYSQL_TYPE_VARCHAR) compatible = true;
        stats.mean_rec_length += 16;
        break;
      default:
        assert(false);
        break;
    }
    if (!compatible) {
      // The column type is not compatible
      warn_incompatible(ndb_tab, true, "column '%s' is not compatible",
                        field->field_name);
      ndb_log_info("Incompatible ndbinfo column: %s, type: %d,%d",
                   field->field_name, field->type(), field->real_type());
      delete m_impl.m_table;
      m_impl.m_table = nullptr;
      return ERR_INCOMPAT_TABLE_DEF;
    }
  }

  /* Increase "ref_length" to allow a whole row to be stored in "ref" */
  ref_length = 0;
  for (uint i = 0; i < table->s->fields; i++)
    ref_length += table->field[i]->pack_length();
  DBUG_PRINT("info", ("ref_length: %u", ref_length));

  // Mark table as opened
  m_impl.m_status = ha_ndbinfo_impl::Table_Status::OPEN;

  return 0;
}

int ha_ndbinfo::close(void) {
  DBUG_TRACE;

  if (is_offline()) return 0;

  assert(is_open());
  if (m_impl.m_table) {
    g_ndbinfo->closeTable(m_impl.m_table);
    m_impl.m_table = nullptr;
    m_impl.m_status = ha_ndbinfo_impl::Table_Status::CLOSED;
  }
  return 0;
}

int ha_ndbinfo::rnd_init(bool scan) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("scan: %d", scan));

  if (!is_open()) {
    switch (m_impl.m_status) {
      case ha_ndbinfo_impl::Table_Status::OFFLINE_NDBINFO_OFFLINE: {
        push_warning(current_thd, Sql_condition::SL_NOTE, 1,
                     "'NDBINFO' has been started in offline mode "
                     "since the 'NDBCLUSTER' engine is disabled "
                     "or @@global.ndbinfo_offline is turned on "
                     "- no rows can be returned");
        return 0;
      }
      case ha_ndbinfo_impl::Table_Status::OFFLINE_DISCONNECTED:
        return HA_ERR_NO_CONNECTION;
      case ha_ndbinfo_impl::Table_Status::OFFLINE_UPGRADING: {
        // Upgrade in progress.
        push_warning(current_thd, Sql_condition::SL_NOTE, 1,
                     "This table is not available as the data nodes are not "
                     "upgraded yet - no rows can be returned");
        // Close the table in MySQL Server's table definition cache to force
        // reload it the next time.
        const std::string db_name(table_share->db.str, table_share->db.length);
        const std::string table_name(table_share->table_name.str,
                                     table_share->table_name.length);
        ndb_tdc_close_cached_table(current_thd, db_name.c_str(),
                                   table_name.c_str());
        return 0;
      }
      default:
        // Should not happen
        assert(false);
        return 0;
    }
  }

  if (m_impl.m_scan_op) {
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
    assert(scan);  // "only makes sense if scan=1" (from rnd_init() description)

    DBUG_PRINT("info", ("Closing scan to position it back to first row"));

    // Release the scan operation
    g_ndbinfo->releaseScanOperation(m_impl.m_scan_op);
    m_impl.m_scan_op = nullptr;

    // Release pointers to the columns
    m_impl.m_columns.clear();
  }

  assert(m_impl.m_scan_op == nullptr);  // No scan already ongoing

  if (m_impl.m_first_use) {
    m_impl.m_first_use = false;

    /*
      Check table def. and generate warnings for incompatibilites
      which is allowed but should generate a warning.
      (Done this late due to different code paths in MySQL Server for
      prepared statement protocol, where warnings from 'handler::open'
      are lost).
    */
    uint fields_found_in_ndb = 0;
    const NdbInfo::Table *ndb_tab = m_impl.m_table;
    for (uint i = 0; i < table->s->fields; i++) {
      const Field *field = table->field[i];
      const NdbInfo::Column *col = ndb_tab->getColumn(field->field_name);
      if (!col) {
        // The column didn't exist
        warn_incompatible(ndb_tab, true, "column '%s' does not exist",
                          field->field_name);
        continue;
      }
      fields_found_in_ndb++;
    }

    if (fields_found_in_ndb < ndb_tab->columns()) {
      // There are more columns available in NDB
      warn_incompatible(ndb_tab, false, "there are more columns available");
    }
  }

  if (!scan) {
    // Just an init to read using 'rnd_pos'
    DBUG_PRINT("info", ("not scan"));
    return 0;
  }

  THD *thd = current_thd;
  int err;
  NdbInfoScanOperation *scan_op = nullptr;
  if ((err = g_ndbinfo->createScanOperation(m_impl.m_table, &scan_op,
                                            THDVAR(thd, max_rows),
                                            THDVAR(thd, max_bytes))) != 0)
    return err2mysql(err);

  if ((err = scan_op->readTuples()) != 0) {
    // Release the scan operation
    g_ndbinfo->releaseScanOperation(scan_op);
    return err2mysql(err);
  }

  /* Read all columns specified in read_set */
  for (uint i = 0; i < table->s->fields; i++) {
    Field *field = table->field[i];
    if (bitmap_is_set(table->read_set, i))
      m_impl.m_columns.push_back(scan_op->getValue(field->field_name));
    else
      m_impl.m_columns.push_back(nullptr);
  }

  if ((err = scan_op->execute()) != 0) {
    // Release pointers to the columns
    m_impl.m_columns.clear();
    // Release the scan operation
    g_ndbinfo->releaseScanOperation(scan_op);
    return err2mysql(err);
  }

  m_impl.m_scan_op = scan_op;
  return 0;
}

int ha_ndbinfo::rnd_end() {
  DBUG_TRACE;

  if (is_offline()) return 0;

  assert(is_open());

  if (m_impl.m_scan_op) {
    g_ndbinfo->releaseScanOperation(m_impl.m_scan_op);
    m_impl.m_scan_op = nullptr;
  }
  m_impl.m_columns.clear();

  return 0;
}

int ha_ndbinfo::rnd_next(uchar *buf) {
  int err;
  DBUG_TRACE;

  if (is_offline()) return HA_ERR_END_OF_FILE;

  assert(is_open());

  if (!m_impl.m_scan_op) {
    /*
     It should be impossible to come here without a scan operation.
     But apparently it's not safe to assume that rnd_next() isn't
     called even though rnd_init() returned an error. Thus double check
     that the scan operation exists and bail out in case it doesn't.
    */
    return HA_ERR_INTERNAL_ERROR;
  }

  if ((err = m_impl.m_scan_op->nextResult()) == 0) return HA_ERR_END_OF_FILE;

  if (err != 1) return err2mysql(err);

  return unpack_record(buf);
}

int ha_ndbinfo::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_TRACE;
  assert(is_open());
  assert(m_impl.m_scan_op == nullptr);  // No scan started

  /* Copy the saved row into "buf" and set all fields to not null */
  memcpy(buf, pos, ref_length);
  for (uint i = 0; i < table->s->fields; i++) table->field[i]->set_notnull();

  return 0;
}

void ha_ndbinfo::position(const uchar *record) {
  DBUG_TRACE;
  assert(is_open());
  assert(m_impl.m_scan_op);

  /* Save away the whole row in "ref" */
  memcpy(ref, record, ref_length);
}

int ha_ndbinfo::info(uint flag) {
  DBUG_TRACE;
  if (m_impl.m_table != nullptr) {
    stats.table_in_mem_estimate = m_impl.m_table->getVirtualTable() ? 1.0 : 0.0;
    if (flag & HA_STATUS_VARIABLE)
      stats.records = m_impl.m_table->getRowsEstimate();
  }
  if (table->key_info) table->key_info->set_records_per_key(0, 1.0F);
  return 0;
}

static int unpack_unexpected_field(Field *f) {
  ndb_log_error(
      "unexpected field '%s', type: %u, real_type: %u, pack_length: %u",
      f->field_name, f->type(), f->real_type(), f->pack_length());
  assert(false); /* stop here on debug build */
  return HA_ERR_INTERNAL_ERROR;
}

static int unpack_unexpected_value(Field *f, const Uint32 value) {
  ndb_log_error(
      "unexpected value %u for field '%s', real_type: %u, pack_length: %u",
      value, f->field_name, f->real_type(), f->pack_length());
  assert(false); /* stop here on debug build */
  return HA_ERR_INTERNAL_ERROR;
}

int ha_ndbinfo::unpack_record(uchar *dst_row) {
  DBUG_TRACE;
  ptrdiff_t dst_offset = dst_row - table->record[0];

  for (uint i = 0; i < table->s->fields; i++) {
    Field *field = table->field[i];
    const NdbInfoRecAttr *record = m_impl.m_columns[i];
    if (!record || record->isNULL()) {
      field->set_null();
      continue;
    }
    field->set_notnull();
    field->move_field_offset(dst_offset);
    switch (field->type()) {
      case (MYSQL_TYPE_VARCHAR): {
        DBUG_PRINT("info", ("str: %s", record->c_str()));
        Field_varstring *vfield = (Field_varstring *)field;
        /* Field_bit in DBUG requires the bit set in write_set for store(). */
        my_bitmap_map *old_map =
            dbug_tmp_use_all_columns(table, table->write_set);
        (void)vfield->store(record->c_str(),
                            std::min(record->length(), field->field_length) - 1,
                            field->charset());
        dbug_tmp_restore_column_map(table->write_set, old_map);
        break;
      }

      case (MYSQL_TYPE_LONG): {
        memcpy(field->field_ptr(), record->ptr(), sizeof(Uint32));
        break;
      }

      case (MYSQL_TYPE_LONGLONG): {
        memcpy(field->field_ptr(), record->ptr(), sizeof(Uint64));
        break;
      }

      case (MYSQL_TYPE_STRING): {
        const Uint32 value = record->u_32_value();
        unsigned char val8;
        uint16 val16;

        if (!(field->real_type() == MYSQL_TYPE_SET ||
              field->real_type() == MYSQL_TYPE_ENUM))
          return unpack_unexpected_field(field);

        switch (field->pack_length()) {
          case 1:
            if (unlikely(value > 255))
              return unpack_unexpected_value(field, value);
            val8 = value;
            *(field->field_ptr()) = val8;
            break;
          case 2:
            if (unlikely(value > 65535))
              return unpack_unexpected_value(field, value);
            val16 = value;
            memcpy(field->field_ptr(), &val16, sizeof(Uint16));
            break;
          default:
            return unpack_unexpected_field(field);
        }
        break;
      }

      default:
        return unpack_unexpected_field(field);
    }

    field->move_field_offset(-dst_offset);
  }
  return 0;
}

ulonglong ha_ndbinfo::table_flags() const {
  ulonglong flags = HA_NO_TRANSACTIONS | HA_NO_BLOBS | HA_NO_AUTO_INCREMENT;

  // m_table could be null; sometimes table_flags() is called prior to open()
  if (m_impl.m_table != nullptr && m_impl.m_table->rowCountIsExact())
    flags |= HA_COUNT_ROWS_INSTANT | HA_STATS_RECORDS_IS_EXACT;

  return flags;
}

//
// INDEXED READS on VirtualTables
//

ulong ha_ndbinfo::index_flags(uint, uint, bool) const {
  return HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_READ_RANGE;
}

int ha_ndbinfo::index_init(uint index, bool) {
  assert(index == 0);
  active_index = index;  // required
  int err = rnd_init(true);
  if (err != 0) return err;
  m_impl.m_scan_op->initIndex(index);
  return 0;
}

int ha_ndbinfo::index_end() { return rnd_end(); }

int ha_ndbinfo::index_read(uchar *buf, const uchar *key,
                           uint key_len [[maybe_unused]],
                           enum ha_rkey_function flag) {
  assert(key != nullptr);
  assert(key_len == sizeof(int));

  NdbInfoScanOperation::Seek seek(
      NdbInfoScanOperation::Seek::Mode::value,
      flag < HA_READ_AFTER_KEY,                                   // inclusive
      flag == HA_READ_KEY_OR_PREV || flag == HA_READ_BEFORE_KEY,  // low
      flag == HA_READ_KEY_OR_NEXT || flag == HA_READ_AFTER_KEY);  // high

  int index_value = *(const int *)key;
  bool found = m_impl.m_scan_op->seek(seek, index_value);
  return found ? rnd_next(buf) : HA_ERR_KEY_NOT_FOUND;
}

int ha_ndbinfo::index_read_map(uchar *buf, const uchar *key,
                               key_part_map keypart_map,
                               enum ha_rkey_function find_flag) {
  return index_read(
      buf, key, calculate_key_len(table, active_index, keypart_map), find_flag);
}

// read_last wants the last row with a given index value.
// All indexes are unique, so it is equivalent to read.
int ha_ndbinfo::index_read_last_map(uchar *buf, const uchar *key,
                                    key_part_map keypart_map) {
  return index_read(buf, key,
                    calculate_key_len(table, active_index, keypart_map),
                    HA_READ_KEY_EXACT);
}

int ha_ndbinfo::index_next(uchar *buf) {
  bool found = m_impl.m_scan_op->seek(
      NdbInfoScanOperation::Seek(NdbInfoScanOperation::Seek::Mode::next));
  return found ? rnd_next(buf) : HA_ERR_END_OF_FILE;
}

int ha_ndbinfo::index_prev(uchar *buf) {
  bool found = m_impl.m_scan_op->seek(
      NdbInfoScanOperation::Seek(NdbInfoScanOperation::Seek::Mode::previous));
  return found ? rnd_next(buf) : HA_ERR_END_OF_FILE;
}

int ha_ndbinfo::index_first(uchar *buf) {
  m_impl.m_scan_op->seek(
      NdbInfoScanOperation::Seek(NdbInfoScanOperation::Seek::Mode::first));
  return rnd_next(buf);
}

int ha_ndbinfo::index_last(uchar *buf) {
  m_impl.m_scan_op->seek(
      NdbInfoScanOperation::Seek(NdbInfoScanOperation::Seek::Mode::last));
  return rnd_next(buf);
}

static int ndbinfo_find_files(handlerton *, THD *thd, const char *db,
                              const char *, const char *, bool dir,
                              List<LEX_STRING> *files) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', dir: %d", db, dir));

  const bool show_hidden = THDVAR(thd, show_hidden);

  if (show_hidden) return 0;  // Don't filter out anything

  if (dir) {
    if (!ndbcluster_is_disabled()) return 0;

    // Hide our database when ndbcluster is disabled
    LEX_STRING *dir_name;
    List_iterator<LEX_STRING> it(*files);
    while ((dir_name = it++)) {
      if (strcmp(dir_name->str, opt_ndbinfo_dbname)) continue;

      DBUG_PRINT("info", ("Hiding own database '%s'", dir_name->str));
      it.remove();
    }

    return 0;
  }

  assert(db);
  if (strcmp(db, opt_ndbinfo_dbname)) return 0;  // Only hide files in "our" db

  /* Hide all files that start with "our" prefix */
  LEX_STRING *file_name;
  List_iterator<LEX_STRING> it(*files);
  while ((file_name = it++)) {
    if (is_prefix(file_name->str, opt_ndbinfo_table_prefix)) {
      DBUG_PRINT("info", ("Hiding '%s'", file_name->str));
      it.remove();
    }
  }

  return 0;
}

extern bool ndbinfo_define_dd_tables(List<const Plugin_table> *);

static bool ndbinfo_dict_init(dict_init_mode_t, uint,
                              List<const Plugin_table> *table_list,
                              List<const Plugin_tablespace> *) {
  return ndbinfo_define_dd_tables(table_list);
}

static int ndbinfo_init(void *plugin) {
  DBUG_TRACE;

  handlerton *hton = (handlerton *)plugin;
  hton->create = create_handler;
  hton->flags = HTON_TEMPORARY_NOT_SUPPORTED | HTON_ALTER_NOT_SUPPORTED;
  hton->find_files = ndbinfo_find_files;
  hton->dict_init = ndbinfo_dict_init;

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

  if (ndbcluster_is_disabled()) {
    // Starting in limited mode since ndbcluster is disabled
    return 0;
  }

  char prefix[FN_REFLEN];
  build_table_filename(prefix, sizeof(prefix) - 1, opt_ndbinfo_dbname,
                       opt_ndbinfo_table_prefix, "", 0);
  ndb_log_info("ndbinfo prefix: '%s'", prefix);
  assert(g_ndb_cluster_connection);
  g_ndbinfo = new (std::nothrow) NdbInfo(g_ndb_cluster_connection, prefix);
  if (!g_ndbinfo) {
    ndb_log_error("Failed to create NdbInfo");
    return 1;
  }

  if (!g_ndbinfo->init()) {
    ndb_log_error("Failed to init NdbInfo");

    delete g_ndbinfo;
    g_ndbinfo = nullptr;

    return 1;
  }

  return 0;
}

static int ndbinfo_deinit(void *) {
  DBUG_TRACE;

  if (g_ndbinfo) {
    delete g_ndbinfo;
    g_ndbinfo = nullptr;
  }

  return 0;
}

SYS_VAR *ndbinfo_system_variables[] = {MYSQL_SYSVAR(max_rows),
                                       MYSQL_SYSVAR(max_bytes),
                                       MYSQL_SYSVAR(show_hidden),
                                       MYSQL_SYSVAR(database),
                                       MYSQL_SYSVAR(table_prefix),
                                       MYSQL_SYSVAR(version),
                                       MYSQL_SYSVAR(offline),

                                       nullptr};

struct st_mysql_storage_engine ndbinfo_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

struct st_mysql_plugin ndbinfo_plugin = {
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &ndbinfo_storage_engine,
    "ndbinfo",
    PLUGIN_AUTHOR_ORACLE,
    "MySQL Cluster system information storage engine",
    PLUGIN_LICENSE_GPL,
    ndbinfo_init,             /* plugin init */
    nullptr,                  /* plugin uninstall check */
    ndbinfo_deinit,           /* plugin deinit */
    0x0001,                   /* plugin version */
    nullptr,                  /* status variables */
    ndbinfo_system_variables, /* system variables */
    nullptr,                  /* config options */
    0};
