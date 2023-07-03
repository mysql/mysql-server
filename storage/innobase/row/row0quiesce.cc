/*****************************************************************************

Copyright (c) 2012, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file row/row0quiesce.cc
 Quiesce a tablespace.

 Created 2012-02-08 by Sunny Bains.
 *******************************************************/

#include <errno.h>
#include <my_aes.h>

#include "dd/cache/dictionary_client.h"
#include "sql/dd/dictionary.h"
#include "sql/dd/types/column_type_element.h"

#include "dict0dd.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "row0mysql.h"
#include "row0quiesce.h"
#include "srv0start.h"
#include "trx0purge.h"

#include "my_dbug.h"

/** Write the meta data (index user fields) config file.
 @return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_quiesce_write_index_fields(
    const dict_index_t *index, /*!< in: write the meta data for
                               this index */
    FILE *file,                /*!< in: file to write to */
    THD *thd)                  /*!< in/out: session */
{
  /* This row will store prefix_len, fixed_len,
  and in IB_EXPORT_CFG_VERSION_V4, is_ascending */
  byte row[sizeof(uint32_t) * 3];
  size_t row_len = sizeof(row);

  for (ulint i = 0; i < index->n_fields; ++i) {
    byte *ptr = row;
    const dict_field_t *field = &index->fields[i];

    mach_write_to_4(ptr, field->prefix_len);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, field->fixed_len);
    ptr += sizeof(uint32_t);

    /* In IB_EXPORT_CFG_VERSION_V4 we also write the is_ascending boolean. */
    mach_write_to_4(ptr, field->is_ascending);

    DBUG_EXECUTE_IF("ib_export_use_cfg_version_3",
                    row_len = sizeof(uint32_t) * 2;);

    DBUG_EXECUTE_IF("ib_export_io_write_failure_9", close(fileno(file)););

    if (fwrite(row, 1, row_len, file) != row_len) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing index fields.");

      return (DB_IO_ERROR);
    }

    /* Include the NUL byte in the length. */
    uint32_t len = static_cast<uint32_t>(strlen(field->name) + 1);
    ut_a(len > 1);

    mach_write_to_4(row, len);

    DBUG_EXECUTE_IF("ib_export_io_write_failure_10", close(fileno(file)););

    if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
        fwrite(field->name, 1, len, file) != len) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing index column.");

      return (DB_IO_ERROR);
    }
  }

  return (DB_SUCCESS);
}
/** Write the meta data config file index information
@param[in]      index   write metadata for this index
@param[in,out]  file    file to write to
@param[in,out]  thd     session
@return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_quiesce_write_one_index(
    const dict_index_t *index, FILE *file, THD *thd) {
  dberr_t err;
  byte *ptr;
  byte row[sizeof(space_index_t) + sizeof(uint32_t) * 8];

  ptr = row;

  static_assert(sizeof(space_index_t) == 8);
  mach_write_to_8(ptr, index->id);
  ptr += sizeof(space_index_t);

  mach_write_to_4(ptr, index->space);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->page);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->type);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->trx_id_offset);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_user_defined_cols);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_uniq);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_nullable);
  ptr += sizeof(uint32_t);

  mach_write_to_4(ptr, index->n_fields);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_12", close(fileno(file)););

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing index meta-data.");

    return (DB_IO_ERROR);
  }

  /* Write the length of the index name.
  NUL byte is included in the length. */
  uint32_t len = static_cast<uint32_t>(strlen(index->name) + 1);
  ut_a(len > 1);

  mach_write_to_4(row, len);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_1", close(fileno(file)););

  if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
      fwrite(index->name, 1, len, file) != len) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing index name.");

    return (DB_IO_ERROR);
  }

  err = row_quiesce_write_index_fields(index, file, thd);
  return (err);
}

/** Write the meta data config file index information.
 @return DB_SUCCESS or error code. */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t
    row_quiesce_write_indexes(const dict_table_t *table, /*!< in: write the meta
                                                         data for this table */
                              FILE *file, /*!< in: file to write to */
                              THD *thd)   /*!< in/out: session */
{
  byte row[sizeof(uint32_t)];

  /* Write the number of indexes in the table. */
  uint32_t num_indexes = 0;
  uint32_t flags = fil_space_get_flags(table->space);
  bool has_sdi = FSP_FLAGS_HAS_SDI(flags);

  if (has_sdi) {
    num_indexes += 1;
  }

  num_indexes += static_cast<uint32_t>(UT_LIST_GET_LEN(table->indexes));
  ut_ad(num_indexes != 0);

  mach_write_to_4(row, num_indexes);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_11", close(fileno(file)););

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing index count.");

    return (DB_IO_ERROR);
  }

  /* Write SDI Index */
  if (has_sdi) {
    dict_mutex_enter_for_mysql();

    dict_index_t *index = dict_sdi_get_index(table->space);

    dict_mutex_exit_for_mysql();

    ut_ad(index != nullptr);
    const auto err = row_quiesce_write_one_index(index, file, thd);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  /* Write the table indexes meta data. */
  for (const dict_index_t *index : table->indexes) {
    const auto err = row_quiesce_write_one_index(index, file, thd);
    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

/** Write the metadata (table columns) config file. Serialise the contents
of dict_col_t default value part if exists.
@param[in]  col         column to which the default value belongs
@param[in]  col_name    column name
@param[in,out]  file        file to write to
@param[in]  dict_table  InnoDB table cache
@param[in]  thd         THD
@return DB_SUCCESS or DB_IO_ERROR. */
static dberr_t row_quiesce_write_dropped_col_metadata(
    const dict_col_t *col, const char *col_name, FILE *file,
    const dict_table_t *dict_table, THD *thd) {
  /* Total metadata to be written
    1 byte for is NULLABLE
    1 byte for is_unsigned
    4 bytes for char_length
    4 bytes for column type
    4 bytes for numeric scale
    8 bytes for collation id */
  constexpr size_t METADATA_SIZE = 22;

  byte row[METADATA_SIZE];

  ut_ad(col->is_instant_dropped());

  const dd::Table *dd_table = nullptr;
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  /* Find schema and table name */
  std::string dict_name(dict_table->name.m_name);
  std::string schema_name;
  std::string table_name;
  size_t table_begin = dict_name.find(dict_name::SCHEMA_SEPARATOR);
  /* schema name must be part of table name */
  ut_ad(table_begin != std::string::npos);

  schema_name.assign(dict_name.substr(0, table_begin));
  table_begin += dict_name::SCHEMA_SEPARATOR_LEN;

  /* If partition table, skip partition name. */
  size_t part_name_begin = dict_name.find(dict_name::PART_SEPARATOR);
  if (part_name_begin != std::string::npos) {
    table_name.assign(dict_name, table_begin, part_name_begin - table_begin);
  } else {
    table_name.assign(dict_name.substr(table_begin));
  }

  if (client->acquire(dd::String_type(schema_name.c_str()),
                      dd::String_type(table_name.c_str()), &dd_table)) {
    ut_ad(false);
    return (DB_ERROR);
  }

  if (dd_table == nullptr) {
    ut_ad(false);
    return DB_ERROR;
  }

  const dd::Column *column = dd_find_column(dd_table, col_name);
  ut_ad(column != nullptr);

  byte *ptr = row;

  /* 1 byte for is NULLABLE */
  mach_write_to_1(ptr, column->is_nullable());
  ptr++;

  /* 1 byte for is_unsigned */
  mach_write_to_1(ptr, column->is_unsigned());
  ptr++;

  /* 4 bytes for char_length() */
  mach_write_to_4(ptr, column->char_length());
  ptr += 4;

  /* 4 bytes for column type */
  mach_write_to_4(ptr, (uint32_t)column->type());
  ptr += 4;

  /* 4 bytes for numeric scale */
  mach_write_to_4(ptr, column->numeric_scale());
  ptr += 4;

  /* 8 bytes for collation id */
  mach_write_to_8(ptr, column->collation_id());
  ptr += 8;

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    return (DB_IO_ERROR);
  }

  /* Write elements for enum column type.
  [4]     bytes : number of elements
  For each element
    [4]     bytes : element name length (len+1)
    [len+1] bytes : element name */
  if (column->type() == dd::enum_column_types::ENUM ||
      column->type() == dd::enum_column_types::SET) {
    byte _row[sizeof(uint32_t)];

    /* Write element count */
    mach_write_to_4(_row, column->elements().size());
    if (fwrite(_row, 1, sizeof(_row), file) != sizeof(_row)) {
      ib_senderrf(
          thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno, strerror(errno),
          "while writing enum column element count for column %s.", col_name);

      return (DB_IO_ERROR);
    }

    /* Write out the enum/set column element name as [len, byte array]. */
    for (const auto *source_elem : column->elements()) {
      const char *elem_name = source_elem->name().c_str();
      uint32_t len = strlen(elem_name) + 1;
      ut_a(len > 1);

      mach_write_to_4(_row, len);

      if (fwrite(_row, 1, sizeof(len), file) != sizeof(len) ||
          fwrite(elem_name, 1, len, file) != len) {
        ib_senderrf(
            thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno, strerror(errno),
            "while writing enum column element name for column %s.", col_name);

        return (DB_IO_ERROR);
      }
    }
  }

  return (DB_SUCCESS);
}

/** Write the metadata (table columns) config file. Serialise the contents
of dict_col_t default value part if exists.
@param[in]      col     column to which the default value belongs
@param[in]      file    file to write to
@return DB_SUCCESS or DB_IO_ERROR. */
[[nodiscard]] static dberr_t row_quiesce_write_default_value(
    const dict_col_t *col, FILE *file) {
  byte row[6];

  if (col->instant_default != nullptr) {
    bool null = (col->instant_default->len == UNIV_SQL_NULL);
    uint8_t to_write = 2;

    mach_write_to_1(&row[0], 1);
    mach_write_to_1(&row[1], null);

    if (!null) {
      mach_write_to_4(&row[2], col->instant_default->len);
      to_write += 4;
    }

    if (fwrite(row, 1, to_write, file) != to_write ||
        (!null &&
         fwrite(col->instant_default->value, 1, col->instant_default->len,
                file) != col->instant_default->len)) {
      return (DB_IO_ERROR);
    }
  } else {
    row[0] = 0;

    if (fwrite(row, 1, 1, file) != 1) {
      return (DB_IO_ERROR);
    }
  }

  return (DB_SUCCESS);
}

/** Write the meta data (table columns) config file. Serialise the contents of
 dict_col_t structure, along with the column name. All fields are serialized
 as uint32_t.
 @return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_quiesce_write_table(
    const dict_table_t *table, /*!< in: write the meta
                               data for this table */
    FILE *file,                /*!< in: file to write to */
    THD *thd)                  /*!< in/out: session */
{
  dict_col_t *col;
  byte row[sizeof(uint32_t) * 7];

  col = table->cols;

  for (ulint i = 0; i < table->get_total_cols(); ++i, ++col) {
    byte *ptr = row;

    mach_write_to_4(ptr, col->prtype);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->mtype);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->len);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->mbminmaxlen);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->ind);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->ord_part);
    ptr += sizeof(uint32_t);

    mach_write_to_4(ptr, col->max_prefix);

    DBUG_EXECUTE_IF("ib_export_io_write_failure_2", close(fileno(file)););

    if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing table column data.");

      return (DB_IO_ERROR);
    }

    /* Write out the column name as [len, byte array]. The len
    includes the NUL byte. */
    uint32_t len;
    const char *col_name;

    col_name = table->get_col_name(dict_col_get_no(col));

    /* Include the NUL byte in the length. */
    len = static_cast<uint32_t>(strlen(col_name) + 1);
    ut_a(len > 1);

    mach_write_to_4(row, len);

    DBUG_EXECUTE_IF("ib_export_io_write_failure_3", close(fileno(file)););

    if (fwrite(row, 1, sizeof(len), file) != sizeof(len) ||
        fwrite(col_name, 1, len, file) != len) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing column name.");

      return (DB_IO_ERROR);
    }

    /* Write column's INSTANT metadata */
    uint32_t cfg_version = IB_EXPORT_CFG_VERSION_V7;
    DBUG_EXECUTE_IF("ib_export_use_cfg_version_3",
                    cfg_version = IB_EXPORT_CFG_VERSION_V3;);
    DBUG_EXECUTE_IF("ib_export_use_cfg_version_99",
                    cfg_version = IB_EXPORT_CFG_VERSION_V99;);
    if (cfg_version >= IB_EXPORT_CFG_VERSION_V7) {
      byte row[2 + sizeof(uint32_t)];
      byte *ptr = row;

      /* version added */
      byte value =
          col->is_instant_added() ? col->get_version_added() : UINT8_UNDEFINED;
      mach_write_to_1(ptr, value);
      ptr++;

      /* version dropped */
      value = col->is_instant_dropped() ? col->get_version_dropped()
                                        : UINT8_UNDEFINED;
      mach_write_to_1(ptr, value);
      ptr++;

      /* physical position */
      mach_write_to_4(ptr, col->get_phy_pos());

      if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
        ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                    strerror(errno),
                    "while writing table column instant metadata.");
        return (DB_IO_ERROR);
      }

      /* Write DD::Column specific info for dropped columns */
      if (col->is_instant_dropped()) {
        const char *col_name = table->get_col_name(dict_col_get_no(col));
        if (row_quiesce_write_dropped_col_metadata(col, col_name, file, table,
                                                   thd) != DB_SUCCESS) {
          ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                      strerror(errno),
                      "while writing dropped column metadata.");
          return (DB_IO_ERROR);
        }
      }
    }

    if (row_quiesce_write_default_value(col, file) != DB_SUCCESS) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing table column default.");
      return (DB_IO_ERROR);
    }
  }

  return (DB_SUCCESS);
}

/** Write the meta data config file header.
@param[in]      table   write the meta data for this table
@param[in]      file    file to write to
@param[in,out]  thd     session
@return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_quiesce_write_header(const dict_table_t *table,
                                                      FILE *file, THD *thd) {
  byte value[sizeof(uint32_t)];

  /* Write the current meta-data version number. */
  uint32_t cfg_version = IB_EXPORT_CFG_VERSION_V7;
  DBUG_EXECUTE_IF("ib_export_use_cfg_version_3",
                  cfg_version = IB_EXPORT_CFG_VERSION_V3;);
  DBUG_EXECUTE_IF("ib_export_use_cfg_version_99",
                  cfg_version = IB_EXPORT_CFG_VERSION_V99;);
  mach_write_to_4(value, cfg_version);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_4", close(fileno(file)););

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing meta-data version number.");

    return (DB_IO_ERROR);
  }

  /* Write the server hostname. */
  const char *hostname = server_get_hostname();

  /* Play it safe and check for NULL. */
  if (hostname == nullptr) {
    static const char NullHostname[] = "Hostname unknown";

    ib::warn(ER_IB_MSG_1013) << "Unable to determine server hostname.";

    hostname = NullHostname;
  }

  /* The server hostname includes the NUL byte. */
  uint32_t len = static_cast<uint32_t>(strlen(hostname) + 1);
  mach_write_to_4(value, len);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_5", close(fileno(file)););

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value) ||
      fwrite(hostname, 1, len, file) != len) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing hostname.");

    return (DB_IO_ERROR);
  }

  /* The table name includes the NUL byte. */
  ut_a(table->name.m_name != nullptr);
  len = static_cast<uint32_t>(strlen(table->name.m_name) + 1);

  /* Write the table name. */
  mach_write_to_4(value, len);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_6", close(fileno(file)););

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value) ||
      fwrite(table->name.m_name, 1, len, file) != len) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing table name.");

    return (DB_IO_ERROR);
  }

  byte row[sizeof(uint32_t) * 3];

  /* Write the next autoinc value. */
  mach_write_to_8(row, table->autoinc);

  DBUG_EXECUTE_IF("ib_export_io_write_failure_7", close(fileno(file)););

  if (fwrite(row, 1, sizeof(uint64_t), file) != sizeof(uint64_t)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing table autoinc value.");

    return (DB_IO_ERROR);
  }

  byte *ptr = row;

  /* Write the system page size. */
  mach_write_to_4(ptr, UNIV_PAGE_SIZE);
  ptr += sizeof(uint32_t);

  /* Write the table->flags. */
  mach_write_to_4(ptr, table->flags);
  ptr += sizeof(uint32_t);

  /* Write the number of columns in the table. In case of INSTANT, include
  dropped columns as well. */
  mach_write_to_4(ptr, table->get_total_cols());

  DBUG_EXECUTE_IF("ib_export_io_write_failure_8", close(fileno(file)););

  if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing table meta-data.");

    return (DB_IO_ERROR);
  }

  if (cfg_version >= IB_EXPORT_CFG_VERSION_V5) {
    /* write number of nullable column before first instant column */
    mach_write_to_4(value, table->first_index()->get_instant_nullable());

    if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing table meta-data.");

      return (DB_IO_ERROR);
    }
  }

  /* Write table instant metadata */
  if (cfg_version >= IB_EXPORT_CFG_VERSION_V7) {
    byte row[sizeof(uint32_t) * 5];
    byte *ptr = row;

    /* Write initial column count */
    mach_write_to_4(ptr, table->initial_col_count);
    ptr += sizeof(uint32_t);

    /* Write current column count */
    mach_write_to_4(ptr, table->current_col_count);
    ptr += sizeof(uint32_t);

    /* Write total column count */
    mach_write_to_4(ptr, table->total_col_count);
    ptr += sizeof(uint32_t);

    /* Write number of instantly dropped columns */
    mach_write_to_4(ptr, table->get_n_instant_drop_cols());
    ptr += sizeof(uint32_t);

    /* Write current row version */
    mach_write_to_4(ptr, table->current_row_version);

    if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing table meta-data.");

      return (DB_IO_ERROR);
    }
  }

  /* Write the space flags */
  uint32_t space_flags = fil_space_get_flags(table->space);
  ut_ad(space_flags != UINT32_UNDEFINED);
  mach_write_to_4(value, space_flags);

  if (fwrite(&value, 1, sizeof(value), file) != sizeof(value)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing space_flags.");

    return (DB_IO_ERROR);
  }

  if (cfg_version >= IB_EXPORT_CFG_VERSION_V6) {
    /* Write compression type info. */
    uint8_t compression_type =
        static_cast<uint8_t>(fil_get_compression(table->space));
    mach_write_to_1(value, compression_type);

    if (fwrite(&value, 1, sizeof(uint8_t), file) != sizeof(uint8_t)) {
      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), "while writing compression type info.");

      return DB_IO_ERROR;
    }
  }

  return (DB_SUCCESS);
}

/** Write the table meta data after quiesce.
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t row_quiesce_write_cfg(
    dict_table_t *table, /*!< in: write the meta data for
                                 this table */
    THD *thd)            /*!< in/out: session */
{
  dberr_t err;
  char name[OS_FILE_MAX_PATH];

  dd_get_meta_data_filename(table, nullptr, name, sizeof(name));

  ib::info(ER_IB_MSG_1014) << "Writing table metadata to '" << name << "'";

  FILE *file = fopen(name, "w+b");

  if (file == nullptr) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_CANT_CREATE_FILE, name, errno,
                strerror(errno));

    err = DB_IO_ERROR;
  } else {
    err = row_quiesce_write_header(table, file, thd);

    if (err == DB_SUCCESS) {
      err = row_quiesce_write_table(table, file, thd);
    }

    if (err == DB_SUCCESS) {
      err = row_quiesce_write_indexes(table, file, thd);
    }

    if (fflush(file) != 0) {
      char msg[BUFSIZ];

      snprintf(msg, sizeof(msg), "%s flush() failed", name);

      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), msg);
    }

    if (fclose(file) != 0) {
      char msg[BUFSIZ];

      snprintf(msg, sizeof(msg), "%s flose() failed", name);

      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), msg);
    }
  }

  return (err);
}

/** Write the transfer key to CFP file.
@param[in]      encryption_metadata  the encryption key and iv to store in file
@param[in]      file            file to write to
@param[in]      thd             session
@return DB_SUCCESS or error code. */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t
    row_quiesce_write_transfer_key(
        const Encryption_metadata &encryption_metadata, FILE *file, THD *thd) {
  byte key_size[sizeof(uint32_t)];
  byte row[Encryption::KEY_LEN * 3];
  byte *ptr = row;
  byte *transfer_key = ptr;
  lint elen;
  ut_a(encryption_metadata.can_encrypt());

  /* Write the encryption key size. */
  mach_write_to_4(key_size, Encryption::KEY_LEN);

  if (fwrite(&key_size, 1, sizeof(key_size), file) != sizeof(key_size)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing key size.");

    return (DB_IO_ERROR);
  }

  /* Generate and write the transfer key. */
  Encryption::random_value(transfer_key);
  if (fwrite(transfer_key, 1, Encryption::KEY_LEN, file) !=
      Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing transfer key.");

    return (DB_IO_ERROR);
  }

  ptr += Encryption::KEY_LEN;

  /* Encrypt tablespace key. */
  elen = my_aes_encrypt(encryption_metadata.m_key, Encryption::KEY_LEN, ptr,
                        transfer_key, Encryption::KEY_LEN, my_aes_256_ecb,
                        nullptr, false);

  if (elen == MY_AES_BAD_DATA) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while encrypt tablespace key.");
    return (DB_ERROR);
  }

  /* Write encrypted tablespace key */
  if (fwrite(ptr, 1, Encryption::KEY_LEN, file) != Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing encrypted tablespace key.");

    return (DB_IO_ERROR);
  }
  ptr += Encryption::KEY_LEN;

  /* Encrypt tablespace iv. */
  elen = my_aes_encrypt(encryption_metadata.m_iv, Encryption::KEY_LEN, ptr,
                        transfer_key, Encryption::KEY_LEN, my_aes_256_ecb,
                        nullptr, false);

  if (elen == MY_AES_BAD_DATA) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while encrypt tablespace iv.");
    return (DB_ERROR);
  }

  /* Write encrypted tablespace iv */
  if (fwrite(ptr, 1, Encryption::KEY_LEN, file) != Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while writing encrypted tablespace iv.");

    return (DB_IO_ERROR);
  }

  return (DB_SUCCESS);
}

/** Write the encryption data after quiesce.
@param[in]      table           write the data for this table
@param[in]      thd             session
@return DB_SUCCESS or error code */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t
    row_quiesce_write_cfp(dict_table_t *table, THD *thd) {
  dberr_t err;
  char name[OS_FILE_MAX_PATH];

  /* If table is not encrypted, return. */
  if (!dd_is_table_in_encrypted_tablespace(table)) {
    return (DB_SUCCESS);
  }

  /* Get the encryption key and iv from space */
  /* For encrypted table, before we discard the tablespace,
  we need to save the encryption information into table, otherwise,
  this information will be lost in fil_discard_tablespace along
  with fil_space_free(). */

  fil_space_t *space = fil_space_get(table->space);
  ut_ad(space != nullptr && FSP_FLAGS_GET_ENCRYPTION(space->flags));

  srv_get_encryption_data_filename(table, name, sizeof(name));

  ib::info(ER_IB_MSG_1015) << "Writing table encryption data to '" << name
                           << "'";

  FILE *file = fopen(name, "w+b");

  if (file == nullptr) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_CANT_CREATE_FILE, name, errno,
                strerror(errno));

    err = DB_IO_ERROR;
  } else {
    err =
        row_quiesce_write_transfer_key(space->m_encryption_metadata, file, thd);

    if (fflush(file) != 0) {
      char msg[BUFSIZ];

      snprintf(msg, sizeof(msg), "%s flush() failed", name);

      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), msg);

      err = DB_IO_ERROR;
    }

    if (fclose(file) != 0) {
      char msg[BUFSIZ];

      snprintf(msg, sizeof(msg), "%s flose() failed", name);

      ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                  strerror(errno), msg);
      err = DB_IO_ERROR;
    }
  }

  return (err);
}

/** Check whether a table has an FTS index defined on it.
 @return true if an FTS index exists on the table */
static bool row_quiesce_table_has_fts_index(
    const dict_table_t *table) /*!< in: quiesce this table */
{
  bool exists = false;

  dict_mutex_enter_for_mysql();

  for (const dict_index_t *index : table->indexes) {
    if (index->type & DICT_FTS) {
      exists = true;
      break;
    }
  }

  dict_mutex_exit_for_mysql();

  return (exists);
}

/** Quiesce the tablespace that the table resides in.
@param[in] table Quiesce this table
@param[in,out] trx Transaction/session */
void row_quiesce_table_start(dict_table_t *table, trx_t *trx) {
  ut_a(trx->mysql_thd != nullptr);
  ut_a(srv_n_purge_threads > 0);
  ut_ad(!srv_read_only_mode);

  ut_a(trx->mysql_thd != nullptr);

  ut_ad(fil_space_get(table->space) != nullptr);

  ib::info(ER_IB_MSG_1016) << "Sync to disk of " << table->name << " started.";

  if (trx_purge_state() != PURGE_STATE_DISABLED) {
    trx_purge_stop();
  }

  for (ulint count = 0;
       ibuf_merge_space(table->space) != 0 && !trx_is_interrupted(trx);
       ++count) {
    if (!(count % 20)) {
      ib::info(ER_IB_MSG_1017)
          << "Merging change buffer entries for " << table->name;
    }
  }

  if (!trx_is_interrupted(trx)) {
    extern ib_mutex_t master_key_id_mutex;

    if (dd_is_table_in_encrypted_tablespace(table)) {
      /* Take the mutex to make sure master_key_id doesn't change (eg: key
      rotation). */
      mutex_enter(&master_key_id_mutex);
    }

    buf_LRU_flush_or_remove_pages(table->space, BUF_REMOVE_FLUSH_WRITE, trx);

    if (dd_is_table_in_encrypted_tablespace(table)) {
      mutex_exit(&master_key_id_mutex);
    }

    if (trx_is_interrupted(trx)) {
      ib::warn(ER_IB_MSG_1018) << "Quiesce aborted!";

    } else if (row_quiesce_write_cfg(table, trx->mysql_thd) != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_1019) << "There was an error writing to the"
                                  " meta data file";
    } else if (row_quiesce_write_cfp(table, trx->mysql_thd) != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_1020) << "There was an error writing to the"
                                  " encryption info file";
    } else {
      ib::info(ER_IB_MSG_1021) << "Table " << table->name << " flushed to disk";
    }
  } else {
    ib::warn(ER_IB_MSG_1022) << "Quiesce aborted!";
  }

  dberr_t err = row_quiesce_set_state(table, QUIESCE_COMPLETE, trx);
  ut_a(err == DB_SUCCESS);
}

/** Cleanup after table quiesce.
@param[in] table Quiesce this table
@param[in,out] trx Transaction/session */
void row_quiesce_table_complete(dict_table_t *table, trx_t *trx) {
  ulint count = 0;

  ut_a(trx->mysql_thd != nullptr);

  /* We need to wait for the operation to complete if the
  transaction has been killed. */

  while (table->quiesce != QUIESCE_COMPLETE) {
    /* Print a warning after every minute. */
    if (!(count % 60)) {
      ib::warn(ER_IB_MSG_1023)
          << "Waiting for quiesce of " << table->name << " to complete";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ++count;
  }

  /* Remove the .cfg file now that the user has resumed
  normal operations. Otherwise it will cause problems when
  the user tries to drop the database (remove directory). */
  char cfg_name[OS_FILE_MAX_PATH];

  dd_get_meta_data_filename(table, nullptr, cfg_name, sizeof(cfg_name));

  os_file_delete_if_exists(innodb_data_file_key, cfg_name, nullptr);

  ib::info(ER_IB_MSG_1024) << "Deleting the meta-data file '" << cfg_name
                           << "'";

  if (dd_is_table_in_encrypted_tablespace(table)) {
    char cfp_name[OS_FILE_MAX_PATH];

    srv_get_encryption_data_filename(table, cfp_name, sizeof(cfp_name));

    os_file_delete_if_exists(innodb_data_file_key, cfp_name, nullptr);

    ib::info(ER_IB_MSG_1025)
        << "Deleting the meta-data file '" << cfp_name << "'";
  }

  if (trx_purge_state() != PURGE_STATE_DISABLED) {
    trx_purge_run();
  }

  dberr_t err = row_quiesce_set_state(table, QUIESCE_NONE, trx);
  ut_a(err == DB_SUCCESS);
}

/** Set a table's quiesce state.
 @return DB_SUCCESS or error code. */
dberr_t row_quiesce_set_state(
    dict_table_t *table, /*!< in: quiesce this table */
    ib_quiesce_t state,  /*!< in: quiesce state to set */
    trx_t *trx)          /*!< in/out: transaction */
{
  ut_a(srv_n_purge_threads > 0);

  if (srv_read_only_mode) {
    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_READ_ONLY_MODE);

    return (DB_UNSUPPORTED);

  } else if (table->is_temporary()) {
    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
                ER_CANNOT_DISCARD_TEMPORARY_TABLE);

    return (DB_UNSUPPORTED);
  } else if (table->space == TRX_SYS_SPACE) {
    char table_name[MAX_FULL_NAME_LEN + 1];

    innobase_format_name(table_name, sizeof(table_name), table->name.m_name);

    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
                ER_TABLE_IN_SYSTEM_TABLESPACE, table_name);

    return (DB_UNSUPPORTED);

  } else if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    std::ostringstream err_msg;
    err_msg << "FLUSH TABLES FOR EXPORT on table " << table->name
            << " in a general tablespace.";
    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_NOT_SUPPORTED_YET,
                err_msg.str().c_str());

    return (DB_UNSUPPORTED);
  } else if (row_quiesce_table_has_fts_index(table)) {
    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_NOT_SUPPORTED_YET,
                "FLUSH TABLES on tables that have an FTS index."
                " FTS auxiliary tables will not be flushed.");

  } else if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
    /* If this flag is set then the table may not have any active
    FTS indexes but it will still have the auxiliary tables. */

    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_NOT_SUPPORTED_YET,
                "FLUSH TABLES on a table that had an FTS index,"
                " created on a hidden column, the"
                " auxiliary tables haven't been dropped as yet."
                " FTS auxiliary tables will not be flushed.");
  }

  if (srv_thread_is_active(srv_threads.m_trx_recovery_rollback)) {
    /* We should wait until rollback after recovery end,
    to lock the table consistently. */
    srv_threads.m_trx_recovery_rollback.wait();
  }
  if (trx_purge_state() != PURGE_STATE_DISABLED) {
    /* We should stop purge to lock the table consistently. */
    trx_purge_stop();
  }

  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);

  dict_table_x_lock_indexes(table);

  if (trx_purge_state() != PURGE_STATE_DISABLED) {
    trx_purge_run();
  }

  switch (state) {
    case QUIESCE_START:
      break;

    case QUIESCE_COMPLETE:
      ut_a(table->quiesce == QUIESCE_START);
      break;

    case QUIESCE_NONE:
      ut_a(table->quiesce == QUIESCE_COMPLETE);
      break;
  }

  table->quiesce = state;

  dict_table_x_unlock_indexes(table);

  row_mysql_unlock_data_dictionary(trx);

  return (DB_SUCCESS);
}
