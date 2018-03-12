/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define MYSQL_SERVER 1
#include "storage/myisam/ha_myisam.h"

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <algorithm>
#include <new>

#include "lex_string.h"
#include "m_ctype.h"
#include "my_bit.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "myisam.h"
#include "myisampack.h"
#include "mysql/plugin.h"
#include "sql/current_thd.h"
#include "sql/derror.h"
#include "sql/key.h"  // key_copy
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/sql_plugin.h"
#include "sql/sql_table.h"  // tablename_to_filename
#include "sql/system_variables.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/rt_index.h"

#include "mysql/components/services/log_builtins.h"

using std::max;
using std::min;

ulonglong myisam_recover_options;
static ulong opt_myisam_block_size;

/* Interface to mysqld, to check system tables supported by SE */
static bool myisam_is_supported_system_table(const char *,
                                             const char *table_name,
                                             bool is_sql_layer_system_table);

/* bits in myisam_recover_options */
const char *myisam_recover_names[] = {"DEFAULT", "BACKUP", "FORCE",
                                      "QUICK",   "OFF",    NullS};
TYPELIB myisam_recover_typelib = {array_elements(myisam_recover_names) - 1, "",
                                  myisam_recover_names, NULL};

const char *myisam_stats_method_names[] = {"nulls_unequal", "nulls_equal",
                                           "nulls_ignored", NullS};
TYPELIB myisam_stats_method_typelib = {
    array_elements(myisam_stats_method_names) - 1, "",
    myisam_stats_method_names, NULL};

static MYSQL_SYSVAR_ULONG(block_size, opt_myisam_block_size,
                          PLUGIN_VAR_EXPERIMENTAL | PLUGIN_VAR_RQCMDARG,
                          "Block size to be used for MyISAM index pages", NULL,
                          NULL, MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH,
                          MI_MAX_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH);

static MYSQL_SYSVAR_ULONG(data_pointer_size, myisam_data_pointer_size,
                          PLUGIN_VAR_RQCMDARG,
                          "Default pointer size to be used for MyISAM tables",
                          NULL, NULL, 6, 2, 7, 1);

#define MB (1024 * 1024)
static MYSQL_SYSVAR_ULONGLONG(
    max_sort_file_size, myisam_max_temp_length, PLUGIN_VAR_RQCMDARG,
    "Don't use the fast sort index method to created "
    "index if the temporary file would get bigger than this",
    NULL, NULL, LONG_MAX / MB * MB, 0, MAX_FILE_SIZE, MB);

static MYSQL_SYSVAR_SET(
    recover_options, myisam_recover_options,
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
    "Syntax: myisam-recover-options[=option[,option...]], where option can be "
    "DEFAULT, BACKUP, FORCE, QUICK, or OFF",
    NULL, NULL, 0, &myisam_recover_typelib);

static MYSQL_THDVAR_ULONG(
    repair_threads, PLUGIN_VAR_RQCMDARG,
    "If larger than 1, when repairing a MyISAM table all indexes will be "
    "created in parallel, with one thread per index. The value of 1 "
    "disables parallel repair",
    NULL, NULL, 1, 1, ULONG_MAX, 1);

static MYSQL_THDVAR_ULONGLONG(
    sort_buffer_size, PLUGIN_VAR_RQCMDARG,
    "The buffer that is allocated when sorting the index when doing "
    "a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE",
    NULL, NULL, 8192 * 1024, (long)(MIN_SORT_BUFFER + MALLOC_OVERHEAD),
    SIZE_T_MAX, 1);

static MYSQL_SYSVAR_BOOL(
    use_mmap, opt_myisam_use_mmap, PLUGIN_VAR_NOCMDARG,
    "Use memory mapping for reading and writing MyISAM tables", NULL, NULL,
    false);

static MYSQL_SYSVAR_ULONGLONG(mmap_size, myisam_mmap_size,
                              PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                              "Restricts the total memory "
                              "used for memory mapping of MySQL tables",
                              NULL, NULL, SIZE_T_MAX, MEMMAP_EXTRA_MARGIN,
                              SIZE_T_MAX, 1);

static MYSQL_THDVAR_ENUM(
    stats_method, PLUGIN_VAR_RQCMDARG,
    "Specifies how MyISAM index statistics collection code should "
    "treat NULLs. Possible values of name are NULLS_UNEQUAL (default "
    "behavior for 4.1 and later), NULLS_EQUAL (emulate 4.0 behavior), "
    "and NULLS_IGNORED",
    NULL, NULL, MI_STATS_METHOD_NULLS_NOT_EQUAL, &myisam_stats_method_typelib);

#ifndef DBUG_OFF
/**
  Causes the thread to wait in a spin lock for a query kill signal.
  This function is used by the test frame work to identify race conditions.

  The signal is caught and ignored and the thread is not killed.
*/

static void debug_wait_for_kill(const char *info) {
  DBUG_ENTER("debug_wait_for_kill");
  const char *prev_info;
  THD *thd;
  thd = current_thd;
  prev_info = thd_proc_info(thd, info);
  while (!thd->killed) my_sleep(1000);
  DBUG_PRINT("info", ("Exit debug_wait_for_kill"));
  thd_proc_info(thd, prev_info);
  DBUG_VOID_RETURN;
}
#endif

/*****************************************************************************
** MyISAM tables
*****************************************************************************/

static handler *myisam_create_handler(handlerton *hton, TABLE_SHARE *table,
                                      bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_myisam(hton, table);
}

// collect errors printed by mi_check routines

static void mi_check_print_msg(MI_CHECK *param, const char *msg_type,
                               const char *fmt, va_list args)
    MY_ATTRIBUTE((format(printf, 3, 0)));

static void mi_check_print_msg(MI_CHECK *param, const char *msg_type,
                               const char *fmt, va_list args) {
  THD *thd = (THD *)param->thd;
  Protocol *protocol = thd->get_protocol();
  size_t length, msg_length;
  char msgbuf[MI_MAX_MSG_BUF];
  char name[NAME_LEN * 2 + 2];

  msg_length = vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0;  // healthy paranoia

  DBUG_PRINT(msg_type, ("message: %s", msgbuf));

  if (!thd->get_protocol()->connection_alive()) {
    LogErr(ERROR_LEVEL, ER_MYISAM_CHECK_METHOD_ERROR, msgbuf);
    return;
  }

  if (param->testflag &
      (T_CREATE_MISSING_KEYS | T_SAFE_REPAIR | T_AUTO_REPAIR)) {
    my_message(ER_NOT_KEYFILE, msgbuf, MYF(MY_WME));
    return;
  }
  length = (uint)(strxmov(name, param->db_name, ".", param->table_name, NullS) -
                  name);
  /*
    TODO: switch from protocol to push_warning here. The main reason we didn't
    it yet is parallel repair. Due to following trace:
    mi_check_print_msg/push_warning/sql_alloc/my_pthread_getspecific_ptr.

    Also we likely need to lock mutex here (in both cases with protocol and
    push_warning).
  */
  if (param->need_print_msg_lock) mysql_mutex_lock(&param->print_msg_mutex);

  protocol->start_row();
  protocol->store(name, length, system_charset_info);
  protocol->store(param->op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->end_row())
    LogErr(ERROR_LEVEL, ER_MY_NET_WRITE_FAILED_FALLING_BACK_ON_STDERR, msgbuf);

  if (param->need_print_msg_lock) mysql_mutex_unlock(&param->print_msg_mutex);

  return;
}

/*
  Convert TABLE object to MyISAM key and column definition

  SYNOPSIS
    table2myisam()
      table_arg   in     TABLE object.
      keydef_out  out    MyISAM key definition.
      recinfo_out out    MyISAM column definition.
      records_out out    Number of fields.

  DESCRIPTION
    This function will allocate and initialize MyISAM key and column
    definition for further use in mi_create or for a check for underlying
    table conformance in merge engine.

    The caller needs to free *recinfo_out after use. Since *recinfo_out
    and *keydef_out are allocated with a my_multi_malloc, *keydef_out
    is freed automatically when *recinfo_out is freed.

  RETURN VALUE
    0  OK
    !0 error code
*/

int table2myisam(TABLE *table_arg, MI_KEYDEF **keydef_out,
                 MI_COLUMNDEF **recinfo_out, uint *records_out) {
  uint i, j, recpos, minpos, fieldpos, temp_length, length;
  enum ha_base_keytype type = HA_KEYTYPE_BINARY;
  uchar *record;
  KEY *pos;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo, *recinfo_pos;
  HA_KEYSEG *keyseg;
  TABLE_SHARE *share = table_arg->s;
  uint options = share->db_options_in_use;
  DBUG_ENTER("table2myisam");
  if (!(my_multi_malloc(PSI_INSTRUMENT_ME, MYF(MY_WME), recinfo_out,
                        (share->fields * 2 + 2) * sizeof(MI_COLUMNDEF),
                        keydef_out, share->keys * sizeof(MI_KEYDEF), &keyseg,
                        (share->key_parts + share->keys) * sizeof(HA_KEYSEG),
                        NullS)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM); /* purecov: inspected */
  keydef = *keydef_out;
  recinfo = *recinfo_out;
  pos = table_arg->key_info;
  for (i = 0; i < share->keys; i++, pos++) {
    keydef[i].flag =
        ((uint16)pos->flags & (HA_NOSAME | HA_FULLTEXT | HA_SPATIAL));
    DBUG_ASSERT(pos->algorithm != HA_KEY_ALG_SE_SPECIFIC);
    DBUG_ASSERT(!(pos->flags & HA_SPATIAL) ||
                pos->algorithm == HA_KEY_ALG_RTREE);
    DBUG_ASSERT(!(pos->flags & HA_FULLTEXT) ||
                pos->algorithm == HA_KEY_ALG_FULLTEXT);
    keydef[i].key_alg = pos->algorithm;
    keydef[i].block_length = pos->block_size;
    keydef[i].seg = keyseg;
    keydef[i].keysegs = pos->user_defined_key_parts;
    for (j = 0; j < pos->user_defined_key_parts; j++) {
      Field *field = pos->key_part[j].field;
      type = field->key_type();
      keydef[i].seg[j].flag = pos->key_part[j].key_part_flag;

      if (options & HA_OPTION_PACK_KEYS ||
          (pos->flags &
           (HA_PACK_KEY | HA_BINARY_PACK_KEY | HA_SPACE_PACK_USED))) {
        if (pos->key_part[j].length > 8 &&
            (type == HA_KEYTYPE_TEXT || type == HA_KEYTYPE_NUM ||
             (type == HA_KEYTYPE_BINARY && !field->zero_pack()))) {
          /* No blobs here */
          if (j == 0) keydef[i].flag |= HA_PACK_KEY;
          if (!(field->flags & ZEROFILL_FLAG) &&
              (field->type() == MYSQL_TYPE_STRING ||
               field->type() == MYSQL_TYPE_VAR_STRING ||
               ((int)(pos->key_part[j].length - field->decimals())) >= 4))
            keydef[i].seg[j].flag |= HA_SPACE_PACK;
        } else if (j == 0 &&
                   (!(pos->flags & HA_NOSAME) || pos->key_length > 16))
          keydef[i].flag |= HA_BINARY_PACK_KEY;
      }
      keydef[i].seg[j].type = (int)type;
      keydef[i].seg[j].start = pos->key_part[j].offset;
      keydef[i].seg[j].length = pos->key_part[j].length;
      keydef[i].seg[j].bit_start = keydef[i].seg[j].bit_end =
          keydef[i].seg[j].bit_length = 0;
      keydef[i].seg[j].bit_pos = 0;
      keydef[i].seg[j].language = field->charset_for_protocol()->number;

      if (field->real_maybe_null()) {
        keydef[i].seg[j].null_bit = field->null_bit;
        keydef[i].seg[j].null_pos = field->null_offset();
      } else {
        keydef[i].seg[j].null_bit = 0;
        keydef[i].seg[j].null_pos = 0;
      }
      if (field->type() == MYSQL_TYPE_BLOB ||
          field->type() == MYSQL_TYPE_GEOMETRY) {
        keydef[i].seg[j].flag |= HA_BLOB_PART;
        /* save number of bytes used to pack length */
        keydef[i].seg[j].bit_start =
            (uint)(field->pack_length() - portable_sizeof_char_ptr);
      } else if (field->type() == MYSQL_TYPE_BIT) {
        keydef[i].seg[j].bit_length = ((Field_bit *)field)->bit_len;
        keydef[i].seg[j].bit_start = ((Field_bit *)field)->bit_ofs;
        keydef[i].seg[j].bit_pos = (uint)(((Field_bit *)field)->bit_ptr -
                                          (uchar *)table_arg->record[0]);
      }
    }
    keyseg += pos->user_defined_key_parts;
  }
  if (table_arg->found_next_number_field)
    keydef[share->next_number_index].flag |= HA_AUTO_KEY;
  record = table_arg->record[0];
  recpos = 0;
  recinfo_pos = recinfo;
  while (recpos < (uint)share->stored_rec_length) {
    Field **field, *found = 0;
    minpos = share->reclength;
    length = 0;

    for (field = table_arg->field; *field; field++) {
      if ((fieldpos = (*field)->offset(record)) >= recpos &&
          fieldpos <= minpos) {
        /* skip null fields */
        if (!(temp_length = (*field)->pack_length_in_rec()))
          continue; /* Skip null-fields */
        if (!found || fieldpos < minpos ||
            (fieldpos == minpos && temp_length < length)) {
          minpos = fieldpos;
          found = *field;
          length = temp_length;
        }
      }
    }
    DBUG_PRINT("loop", ("found: %p  recpos: %d  minpos: %d  length: %d", found,
                        recpos, minpos, length));
    if (recpos != minpos) {  // Reserved space (Null bits?)
      memset(recinfo_pos, 0, sizeof(*recinfo_pos));
      recinfo_pos->type = (int)FIELD_NORMAL;
      recinfo_pos++->length = (uint16)(minpos - recpos);
    }
    if (!found) break;

    if (found->flags & BLOB_FLAG)
      recinfo_pos->type = (int)FIELD_BLOB;
    else if (found->type() == MYSQL_TYPE_VARCHAR)
      recinfo_pos->type = FIELD_VARCHAR;
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->type = (int)FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->type = (int)FIELD_SKIP_ZERO;
    else
      recinfo_pos->type =
          (int)((length <= 3 || (found->flags & ZEROFILL_FLAG))
                    ? FIELD_NORMAL
                    : found->type() == MYSQL_TYPE_STRING ||
                              found->type() == MYSQL_TYPE_VAR_STRING
                          ? FIELD_SKIP_ENDSPACE
                          : FIELD_SKIP_PRESPACE);
    if (found->real_maybe_null()) {
      recinfo_pos->null_bit = found->null_bit;
      recinfo_pos->null_pos = found->null_offset();
    } else {
      recinfo_pos->null_bit = 0;
      recinfo_pos->null_pos = 0;
    }
    (recinfo_pos++)->length = (uint16)length;
    recpos = minpos + length;
    DBUG_PRINT("loop", ("length: %d  type: %d", recinfo_pos[-1].length,
                        recinfo_pos[-1].type));
  }
  *records_out = (uint)(recinfo_pos - recinfo);
  DBUG_RETURN(0);
}

/*
  Check for underlying table conformance

  SYNOPSIS
    check_definition()
      t1_keyinfo       in    First table key definition
      t1_recinfo       in    First table record definition
      t1_keys          in    Number of keys in first table
      t1_recs          in    Number of records in first table
      t2_keyinfo       in    Second table key definition
      t2_recinfo       in    Second table record definition
      t2_keys          in    Number of keys in second table
      t2_recs          in    Number of records in second table
      strict           in    Strict check switch

  DESCRIPTION
    This function compares two MyISAM definitions. By intention it was done
    to compare merge table definition against underlying table definition.
    It may also be used to compare dot-frm and MYI definitions of MyISAM
    table as well to compare different MyISAM table definitions.

    For merge table it is not required that number of keys in merge table
    must exactly match number of keys in underlying table. When calling this
    function for underlying table conformance check, 'strict' flag must be
    set to false, and converted merge definition must be passed as t1_*.

    Otherwise 'strict' flag must be set to 1 and it is not required to pass
    converted dot-frm definition as t1_*.

    For compatibility reasons we relax some checks, specifically:
    - 4.0 (and earlier versions) always set key_alg to 0.
    - 4.0 (and earlier versions) have the same language for all keysegs.

  RETURN VALUE
    0 - Equal definitions.
    1 - Different definitions.

  TODO
    - compare FULLTEXT keys;
    - compare SPATIAL keys;
    - compare FIELD_SKIP_ZERO which is converted to FIELD_NORMAL correctly
      (should be corretly detected in table2myisam).
*/

int check_definition(MI_KEYDEF *t1_keyinfo, MI_COLUMNDEF *t1_recinfo,
                     uint t1_keys, uint t1_recs, MI_KEYDEF *t2_keyinfo,
                     MI_COLUMNDEF *t2_recinfo, uint t2_keys, uint t2_recs,
                     bool strict) {
  uint i, j;
  DBUG_ENTER("check_definition");

  if ((strict ? t1_keys != t2_keys : t1_keys > t2_keys)) {
    DBUG_PRINT("error", ("Number of keys differs: t1_keys=%u, t2_keys=%u",
                         t1_keys, t2_keys));
    DBUG_RETURN(1);
  }
  if (t1_recs != t2_recs) {
    DBUG_PRINT("error", ("Number of recs differs: t1_recs=%u, t2_recs=%u",
                         t1_recs, t2_recs));
    DBUG_RETURN(1);
  }
  for (i = 0; i < t1_keys; i++) {
    HA_KEYSEG *t1_keysegs = t1_keyinfo[i].seg;
    HA_KEYSEG *t2_keysegs = t2_keyinfo[i].seg;
    if (t1_keyinfo[i].flag & HA_FULLTEXT && t2_keyinfo[i].flag & HA_FULLTEXT)
      continue;
    else if (t1_keyinfo[i].flag & HA_FULLTEXT ||
             t2_keyinfo[i].flag & HA_FULLTEXT) {
      DBUG_PRINT("error", ("Key %d has different definition", i));
      DBUG_PRINT("error",
                 ("t1_fulltext= %d, t2_fulltext=%d",
                  static_cast<bool>(t1_keyinfo[i].flag & HA_FULLTEXT),
                  static_cast<bool>(t2_keyinfo[i].flag & HA_FULLTEXT)));
      DBUG_RETURN(1);
    }
    if (t1_keyinfo[i].flag & HA_SPATIAL && t2_keyinfo[i].flag & HA_SPATIAL)
      continue;
    else if (t1_keyinfo[i].flag & HA_SPATIAL ||
             t2_keyinfo[i].flag & HA_SPATIAL) {
      DBUG_PRINT("error", ("Key %d has different definition", i));
      DBUG_PRINT("error", ("t1_spatial= %d, t2_spatial=%d",
                           static_cast<bool>(t1_keyinfo[i].flag & HA_SPATIAL),
                           static_cast<bool>(t2_keyinfo[i].flag & HA_SPATIAL)));
      DBUG_RETURN(1);
    }
    if (!(t1_keyinfo[i].key_alg == t2_keyinfo[i].key_alg ||
          /*
            Pre-8.0 server stored HA_KEY_ALG_HASH value for MyISAM tables
            but treated it as HA_KEY_ALG_BTREE. Starting from 8.0 we store
            correct algorithm value in the data-dictionary. So we have to
            relax our check in order to be able to open old tables in 8.0.
          */
          (t1_keyinfo[i].key_alg == HA_KEY_ALG_BTREE &&
           t2_keyinfo[i].key_alg == HA_KEY_ALG_HASH)) ||
        t1_keyinfo[i].keysegs != t2_keyinfo[i].keysegs) {
      DBUG_PRINT("error", ("Key %d has different definition", i));
      DBUG_PRINT("error", ("t1_keysegs=%d, t1_key_alg=%d",
                           t1_keyinfo[i].keysegs, t1_keyinfo[i].key_alg));
      DBUG_PRINT("error", ("t2_keysegs=%d, t2_key_alg=%d",
                           t2_keyinfo[i].keysegs, t2_keyinfo[i].key_alg));
      DBUG_RETURN(1);
    }
    for (j = t1_keyinfo[i].keysegs; j--;) {
      uint8 t1_keysegs_j__type = t1_keysegs[j].type;

      /*
        Table migration from 4.1 to 5.1. In 5.1 a *TEXT key part is
        always HA_KEYTYPE_VARTEXT2. In 4.1 we had only the equivalent of
        HA_KEYTYPE_VARTEXT1. Since we treat both the same on MyISAM
        level, we can ignore a mismatch between these types.
      */
      if ((t1_keysegs[j].flag & HA_BLOB_PART) &&
          (t2_keysegs[j].flag & HA_BLOB_PART)) {
        if ((t1_keysegs_j__type == HA_KEYTYPE_VARTEXT2) &&
            (t2_keysegs[j].type == HA_KEYTYPE_VARTEXT1))
          t1_keysegs_j__type = HA_KEYTYPE_VARTEXT1; /* purecov: tested */
        else if ((t1_keysegs_j__type == HA_KEYTYPE_VARBINARY2) &&
                 (t2_keysegs[j].type == HA_KEYTYPE_VARBINARY1))
          t1_keysegs_j__type = HA_KEYTYPE_VARBINARY1; /* purecov: inspected */
      }

      if (t1_keysegs[j].language != t2_keysegs[j].language ||
          t1_keysegs_j__type != t2_keysegs[j].type ||
          t1_keysegs[j].null_bit != t2_keysegs[j].null_bit ||
          t1_keysegs[j].length != t2_keysegs[j].length ||
          t1_keysegs[j].start != t2_keysegs[j].start) {
        DBUG_PRINT("error", ("Key segment %d (key %d) has different "
                             "definition",
                             j, i));
        DBUG_PRINT("error", ("t1_type=%d, t1_language=%d, t1_null_bit=%d, "
                             "t1_length=%d",
                             t1_keysegs[j].type, t1_keysegs[j].language,
                             t1_keysegs[j].null_bit, t1_keysegs[j].length));
        DBUG_PRINT("error", ("t2_type=%d, t2_language=%d, t2_null_bit=%d, "
                             "t2_length=%d",
                             t2_keysegs[j].type, t2_keysegs[j].language,
                             t2_keysegs[j].null_bit, t2_keysegs[j].length));

        DBUG_RETURN(1);
      }
    }
  }
  for (i = 0; i < t1_recs; i++) {
    MI_COLUMNDEF *t1_rec = &t1_recinfo[i];
    MI_COLUMNDEF *t2_rec = &t2_recinfo[i];
    /*
      FIELD_SKIP_ZERO can be changed to FIELD_NORMAL in mi_create,
      see NOTE1 in mi_create.c
    */
    if ((t1_rec->type != t2_rec->type &&
         !(t1_rec->type == (int)FIELD_SKIP_ZERO && t1_rec->length == 1 &&
           t2_rec->type == (int)FIELD_NORMAL)) ||
        t1_rec->length != t2_rec->length ||
        t1_rec->null_bit != t2_rec->null_bit) {
      DBUG_PRINT("error", ("Field %d has different definition", i));
      DBUG_PRINT("error", ("t1_type=%d, t1_length=%d, t1_null_bit=%d",
                           t1_rec->type, t1_rec->length, t1_rec->null_bit));
      DBUG_PRINT("error", ("t2_type=%d, t2_length=%d, t2_null_bit=%d",
                           t2_rec->type, t2_rec->length, t2_rec->null_bit));
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

extern "C" {

volatile int *killed_ptr(MI_CHECK *param) {
  /* In theory Unsafe conversion, but should be ok for now */
  return (int *)&(((THD *)(param->thd))->killed);
}

void mi_check_print_error(MI_CHECK *param, const char *fmt, ...) {
  param->error_printed |= 1;
  param->out_flag |= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "error", fmt, args);
  va_end(args);
}

void mi_check_print_info(MI_CHECK *param, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "info", fmt, args);
  va_end(args);
}

void mi_check_print_warning(MI_CHECK *param, const char *fmt, ...) {
  param->warning_printed = 1;
  param->out_flag |= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "warning", fmt, args);
  va_end(args);
}

/**
  Report list of threads (and queries) accessing a table, thread_id of a
  thread that detected corruption, ource file name and line number where
  this corruption was detected, optional extra information (string).

  This function is intended to be used when table corruption is detected.

  @param[in] file      MI_INFO object.
  @param[in] message   Optional error message.
  @param[in] sfile     Name of source file.
  @param[in] sline     Line number in source file.
*/

void _mi_report_crashed(MI_INFO *file, const char *message, const char *sfile,
                        uint sline) {
  THD *cur_thd;
  LIST *element;
  char buf[1024];
  mysql_mutex_lock(&file->s->intern_lock);

  if ((cur_thd = (THD *)file->in_use.data))
    LogErr(ERROR_LEVEL, ER_MYISAM_CRASHED_ERROR_IN_THREAD, cur_thd->thread_id(),
           sfile, sline);
  else
    LogErr(ERROR_LEVEL, ER_MYISAM_CRASHED_ERROR_IN, sfile, sline);

  if (message) LogErr(ERROR_LEVEL, ER_MYISAM_CRASHED_ERROR, message);

  for (element = file->s->in_use; element; element = list_rest(element)) {
    THD *thd = (THD *)element->data;
    LogErr(ERROR_LEVEL, ER_MYISAM_CRASHED_ERROR,
           thd ? thd_security_context(thd, buf, sizeof(buf), 0)
               : "Unknown thread accessing table");
  }
  mysql_mutex_unlock(&file->s->intern_lock);
}
}

ha_myisam::ha_myisam(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg),
      file(0),
      int_table_flags(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
                      HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
                      HA_DUPLICATE_POS | HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY |
                      HA_FILE_BASED | HA_CAN_GEOMETRY | HA_NO_TRANSACTIONS |
                      HA_CAN_BIT_FIELD | HA_CAN_RTREEKEYS | HA_HAS_RECORDS |
                      HA_STATS_RECORDS_IS_EXACT | HA_CAN_REPAIR |
                      HA_GENERATED_COLUMNS | HA_ATTACHABLE_TRX_COMPATIBLE),
      can_enable_indexes(1),
      ds_mrr(this) {}

handler *ha_myisam::clone(const char *name, MEM_ROOT *mem_root) {
  ha_myisam *new_handler =
      static_cast<ha_myisam *>(handler::clone(name, mem_root));
  if (new_handler) new_handler->file->state = file->state;
  return new_handler;
}

static const char *ha_myisam_exts[] = {".MYI", ".MYD", NullS};

/**
  @brief Check if the given db.tablename is a system table for this SE.

  @param table_name                 table name to check.
  @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                    layer system table.

  @note As for 5.7, mysql doesn't support MyISAM as an engine for the following
        system tables: columns_priv, db, procs_priv, proxies_priv, tables_priv,
        user.

  @note In case there is a need to define MYISAM specific system
        database, then please see reference implementation in
        ha_example.cc.

  @return
    @retval true   Given db.table_name is supported system table.
    @retval false  Given db.table_name is not a supported system table.
*/

static bool myisam_is_supported_system_table(const char *,
                                             const char *table_name,
                                             bool is_sql_layer_system_table) {
  THD *thd = current_thd;

  if (thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
      thd->lex->sql_command == SQLCOM_ALTER_TABLE) {
    /*
      We allow creation of ACL tables in MyISAM to allow upgrade from
      older versions through mysqldump and downgrade.
    */
    // Does MYISAM support "ALL" SQL layer system tables ?
    if (is_sql_layer_system_table) return true;

    /*
      Currently MYISAM does not support any other SE specific
      system tables. If in future it does, please see ha_example.cc
      for reference implementation
    */

    return false;
  } else {
    static const char *unsupported_system_tables[] = {
        "columns_priv", "db",   "procs_priv",      "proxies_priv",
        "tables_priv",  "user", (const char *)NULL};

    if (is_sql_layer_system_table) {
      for (unsigned i = 0; unsupported_system_tables[i] != NULL; ++i) {
        if (!strcmp(table_name, unsupported_system_tables[i]))
          // Doesn't support MYISAM for this table name
          return false;
      }
      // Support MYISAM for other system tables not listed explicitly
      return true;
    }

    /*
      Currently MYISAM does not support any other SE specific
      system tables. If in future it does, please see ha_example.cc
      for reference implementation
    */

    return false;
  }
}

/* Name is here without an extension */
int ha_myisam::open(const char *name, int mode, uint test_if_locked,
                    const dd::Table *) {
  MI_KEYDEF *keyinfo;
  MI_COLUMNDEF *recinfo = 0;
  Myisam_handler_share *my_handler_share;
  MYISAM_SHARE *share = NULL;
  uint recs;
  uint i;

  /*
     We are allocating the handler share only in case of normal MyISAM tables
  */
  if (table->s->tmp_table == NO_TMP_TABLE) {
    lock_shared_ha_data();
    my_handler_share = static_cast<Myisam_handler_share *>(get_ha_share_ptr());
    if (my_handler_share) share = my_handler_share->m_share;

    if (!(file = mi_open_share(name, share, mode,
                               test_if_locked | HA_OPEN_FROM_SQL_LAYER))) {
      unlock_shared_ha_data();
      return (my_errno() ? my_errno() : -1);
    }
    if (!my_handler_share) {
      my_handler_share = new (std::nothrow) Myisam_handler_share;
      if (my_handler_share) {
        my_handler_share->m_share = file->s;
        set_ha_share_ptr(static_cast<Handler_share *>(my_handler_share));
      } else {
        mi_close(file);
        unlock_shared_ha_data();
        return (my_errno() ? my_errno() : HA_ERR_OUT_OF_MEM);
      }
    }
    unlock_shared_ha_data();
  } else if (!(file = mi_open_share(name, share, mode,
                                    test_if_locked | HA_OPEN_FROM_SQL_LAYER)))
    return (my_errno() ? my_errno() : -1);

  if (!table->s->tmp_table) /* No need to perform a check for tmp table */
  {
    /*
      If the data dictionary and SQL-layer have outdated information
      about whether table is compressed or not, ask them to retrieve
      correct row format from the storage engine and store it in DD.
    */
    if ((table->s->real_row_type != ROW_TYPE_COMPRESSED) !=
        !(file->s->options & HA_OPTION_COMPRESS_RECORD)) {
      table->s->db_options_in_use = file->s->options;
      set_my_errno(HA_ERR_ROW_FORMAT_CHANGED);
      goto err;
    }
    set_my_errno(table2myisam(table, &keyinfo, &recinfo, &recs));
    if (my_errno()) {
      /* purecov: begin inspected */
      DBUG_PRINT("error", ("Failed to convert TABLE object to MyISAM "
                           "key and column definition"));
      goto err;
      /* purecov: end */
    }
    if (check_definition(keyinfo, recinfo, table->s->keys, recs,
                         file->s->keyinfo, file->s->rec, file->s->base.keys,
                         file->s->base.fields, true)) {
      /* purecov: begin inspected */
      set_my_errno(HA_ERR_CRASHED);
      goto err;
      /* purecov: end */
    }
  }

  if (test_if_locked & (HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_TMP_TABLE))
    (void)mi_extra(file, HA_EXTRA_NO_WAIT_LOCK, 0);

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    (void)mi_extra(file, HA_EXTRA_WAIT_LOCK, 0);
  if (file->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    int_table_flags |= HA_HAS_CHECKSUM;

  for (i = 0; i < table->s->keys; i++) {
    plugin_ref parser = table->key_info[i].parser;
    if (table->key_info[i].flags & HA_USES_PARSER)
      file->s->keyinfo[i].parser =
          (struct st_mysql_ftparser *)plugin_decl(parser)->info;
    table->key_info[i].block_size = file->s->keyinfo[i].block_length;
  }
  set_my_errno(0);
  goto end;
err:
  this->close();
end:
  /*
    Both recinfo and keydef are allocated by my_multi_malloc(), thus only
    recinfo must be freed.
  */
  if (recinfo) my_free(recinfo);
  return my_errno();
}

int ha_myisam::close(void) {
  bool closed_share = false;
  lock_shared_ha_data();
  int err = mi_close_share(file, &closed_share);
  file = 0;
  /*
    Since tmp tables will also come to the same flow. To distinguesh with them
    we need to check table_share->tmp_table.
  */
  if (closed_share && table_share->tmp_table == NO_TMP_TABLE) {
    Myisam_handler_share *my_handler_share =
        static_cast<Myisam_handler_share *>(get_ha_share_ptr());
    if (my_handler_share && my_handler_share->m_share)
      delete (my_handler_share);
    set_ha_share_ptr(NULL);
  }
  unlock_shared_ha_data();
  return err;
}

int ha_myisam::write_row(uchar *buf) {
  ha_statistic_increment(&System_status_var::ha_write_count);

  /*
    If we have an auto_increment column and we are writing a changed row
    or a new row, then update the auto_increment value in the record.
  */
  if (table->next_number_field && buf == table->record[0]) {
    int error;
    if ((error = update_auto_increment())) return error;
  }
  return mi_write(file, buf);
}

int ha_myisam::check(THD *thd, HA_CHECK_OPT *check_opt) {
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  int error;
  MI_CHECK param;
  MYISAM_SHARE *share = file->s;
  const char *old_proc_info = thd->proc_info;

  thd_proc_info(thd, "Checking table");
  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = "check";
  param.db_name = table->s->db.str;
  param.table_name = table->alias;
  param.testflag = check_opt->flags | T_CHECK | T_SILENT;
  param.stats_method = (enum_mi_stats_method)THDVAR(thd, stats_method);

  if (!(table->db_stat & HA_READ_ONLY)) param.testflag |= T_STATISTICS;
  param.using_global_keycache = 1;

  if (!mi_is_crashed(file) &&
      (((param.testflag & T_CHECK_ONLY_CHANGED) &&
        !(share->state.changed &
          (STATE_CHANGED | STATE_CRASHED | STATE_CRASHED_ON_REPAIR)) &&
        share->state.open_count == 0) ||
       ((param.testflag & T_FAST) &&
        (share->state.open_count == (uint)(share->global_changed ? 1 : 0)))))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_status(&param, file);  // Not fatal
  error = chk_size(&param, file);
  if (!error) error |= chk_del(&param, file, param.testflag);
  if (!error) error = chk_key(&param, file);
  if (!error) {
    if ((!(param.testflag & T_QUICK) &&
         ((share->options &
           (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
          (param.testflag & (T_EXTEND | T_MEDIUM)))) ||
        mi_is_crashed(file)) {
      uint old_testflag = param.testflag;
      param.testflag |= T_MEDIUM;
      if (!(error = init_io_cache(&param.read_cache, file->dfile,
                                  my_default_record_cache_size, READ_CACHE,
                                  share->pack.header_length, 1, MYF(MY_WME)))) {
        error = chk_data_link(&param, file, param.testflag & T_EXTEND);
        end_io_cache(&(param.read_cache));
      }
      param.testflag = old_testflag;
    }
  }
  if (!error) {
    if ((share->state.changed & (STATE_CHANGED | STATE_CRASHED_ON_REPAIR |
                                 STATE_CRASHED | STATE_NOT_ANALYZED)) ||
        (param.testflag & T_STATISTICS) || mi_is_crashed(file)) {
      file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      mysql_mutex_lock(&share->intern_lock);
      share->state.changed &=
          ~(STATE_CHANGED | STATE_CRASHED | STATE_CRASHED_ON_REPAIR);
      if (!(table->db_stat & HA_READ_ONLY))
        error = update_state_info(
            &param, file, UPDATE_TIME | UPDATE_OPEN_COUNT | UPDATE_STAT);
      mysql_mutex_unlock(&share->intern_lock);
      info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
           HA_STATUS_CONST);
    }
  } else if (!mi_is_crashed(file) && !thd->killed) {
    mi_mark_crashed(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
  }

  thd_proc_info(thd, old_proc_info);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}

/*
  analyze the key distribution in the table
  As the table may be only locked for read, we have to take into account that
  two threads may do an analyze at the same time!
*/

int ha_myisam::analyze(THD *thd, HA_CHECK_OPT *) {
  int error = 0;
  MI_CHECK param;
  MYISAM_SHARE *share = file->s;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = "analyze";
  param.db_name = table->s->db.str;
  param.table_name = table->alias;
  param.testflag =
      (T_FAST | T_CHECK | T_SILENT | T_STATISTICS | T_DONT_CHECK_CHECKSUM);
  param.using_global_keycache = 1;
  param.stats_method = (enum_mi_stats_method)THDVAR(thd, stats_method);

  if (!(share->state.changed & STATE_NOT_ANALYZED))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_key(&param, file);
  if (!error) {
    mysql_mutex_lock(&share->intern_lock);
    error = update_state_info(&param, file, UPDATE_STAT);
    mysql_mutex_unlock(&share->intern_lock);
  } else if (!mi_is_crashed(file) && !thd->killed)
    mi_mark_crashed(file);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}

int ha_myisam::repair(THD *thd, HA_CHECK_OPT *check_opt) {
  int error;
  MI_CHECK param;
  ha_rows start_records;

  if (!file) return HA_ADMIN_INTERNAL_ERROR;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = "repair";
  param.testflag =
      ((check_opt->flags & ~(T_EXTEND)) | T_SILENT | T_FORCE_CREATE |
       T_CALC_CHECKSUM | (check_opt->flags & T_EXTEND ? T_REP : T_REP_BY_SORT));
  param.sort_buffer_length = THDVAR(thd, sort_buffer_size);
  start_records = file->state->records;
  while ((error = repair(thd, param, 0)) && param.retry_repair) {
    param.retry_repair = 0;
    if (test_all_bits(param.testflag,
                      (uint)(T_RETRY_WITHOUT_QUICK | T_QUICK))) {
      param.testflag &= ~T_RETRY_WITHOUT_QUICK;
      LogErr(INFORMATION_LEVEL, ER_RETRYING_REPAIR_WITHOUT_QUICK,
             table->s->path.str);
      continue;
    }
    param.testflag &= ~T_QUICK;
    if ((param.testflag & T_REP_BY_SORT)) {
      param.testflag = (param.testflag & ~T_REP_BY_SORT) | T_REP;
      LogErr(INFORMATION_LEVEL, ER_RETRYING_REPAIR_WITH_KEYCACHE,
             table->s->path.str);
      continue;
    }
    break;
  }
  if (!error && start_records != file->state->records &&
      !(check_opt->flags & T_VERY_SILENT)) {
    char llbuff[22], llbuff2[22];
    LogErr(INFORMATION_LEVEL, ER_FOUND_ROWS_WHILE_REPAIRING,
           llstr(file->state->records, llbuff), llstr(start_records, llbuff2),
           table->s->path.str);
  }
  return error;
}

int ha_myisam::optimize(THD *thd, HA_CHECK_OPT *check_opt) {
  int error;
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = "optimize";
  param.testflag = (check_opt->flags | T_SILENT | T_FORCE_CREATE |
                    T_REP_BY_SORT | T_STATISTICS | T_SORT_INDEX);
  param.sort_buffer_length = THDVAR(thd, sort_buffer_size);
  if ((error = repair(thd, param, 1)) && param.retry_repair) {
    LogErr(WARNING_LEVEL, ER_ERROR_DURING_OPTIMIZE_TABLE, my_errno(),
           param.db_name, param.table_name);
    param.testflag &= ~T_REP_BY_SORT;
    error = repair(thd, param, 1);
  }
  return error;
}

int ha_myisam::repair(THD *thd, MI_CHECK &param, bool do_optimize) {
  int error = 0;
  uint local_testflag = param.testflag;
  bool optimize_done = !do_optimize, statistics_done = 0;
  bool has_old_locks = thd->locked_tables_mode || file->lock_type != F_UNLCK;
  const char *old_proc_info = thd->proc_info;
  char fixed_name[FN_REFLEN];
  MYISAM_SHARE *share = file->s;
  ha_rows rows = file->state->records;
  DBUG_ENTER("ha_myisam::repair");

  param.db_name = table->s->db.str;
  param.table_name = table->alias;
  param.using_global_keycache = 1;
  param.thd = thd;
  param.tmpdir = &mysql_tmpdir_list;
  param.out_flag = 0;
  my_stpcpy(fixed_name, file->filename);

  // Don't lock tables if we have used LOCK TABLE or already locked.
  if (!has_old_locks &&
      mi_lock_database(file, table->s->tmp_table ? F_EXTRA_LCK : F_WRLCK)) {
    char errbuf[MYSYS_STRERROR_SIZE];
    mi_check_print_error(&param, ER_THD(thd, ER_CANT_LOCK), my_errno(),
                         my_strerror(errbuf, sizeof(errbuf), my_errno()));
    DBUG_RETURN(HA_ADMIN_FAILED);
  }

  if (!do_optimize ||
      ((file->state->del || share->state.split != file->state->records) &&
       (!(param.testflag & T_QUICK) ||
        !(share->state.changed & STATE_NOT_OPTIMIZED_KEYS)))) {
    ulonglong key_map = ((local_testflag & T_CREATE_MISSING_KEYS)
                             ? mi_get_mask_all_keys_active(share->base.keys)
                             : share->state.key_map);
    uint testflag = param.testflag;
    bool remap = (share->file_map);
    /*
      mi_repair*() functions family use file I/O even if memory
      mapping is available.

      Since mixing mmap I/O and file I/O may cause various artifacts,
      memory mapping must be disabled.
    */
    if (remap) mi_munmap_file(file);
    if (mi_test_if_sort_rep(file, file->state->records, key_map, 0) &&
        (local_testflag & T_REP_BY_SORT)) {
      local_testflag |= T_STATISTICS;
      param.testflag |= T_STATISTICS;  // We get this for free
      statistics_done = 1;
      if (THDVAR(thd, repair_threads) > 1) {
        char buf[40];
        /* TODO: respect myisam_repair_threads variable */
        snprintf(buf, 40, "Repair with %d threads", my_count_bits(key_map));
        thd_proc_info(thd, buf);
        /*
          The new file is created with the right stats, so we can skip
          copying file stats from old to new.
        */
        error = mi_repair_parallel(&param, file, fixed_name,
                                   param.testflag & T_QUICK, true);
        thd_proc_info(thd, "Repair done");  // to reset proc_info, as
                                            // it was pointing to local buffer
      } else {
        thd_proc_info(thd, "Repair by sorting");
        /*
          The new file is created with the right stats, so we can skip
          copying file stats from old to new.
        */
        error = mi_repair_by_sort(&param, file, fixed_name,
                                  param.testflag & T_QUICK, true);
      }
    } else {
      thd_proc_info(thd, "Repair with keycache");
      param.testflag &= ~T_REP_BY_SORT;
      /*
        The new file is created with the right stats, so we can skip
        copying file stats from old to new.
      */
      error =
          mi_repair(&param, file, fixed_name, param.testflag & T_QUICK, true);
    }
    if (remap) mi_dynmap_file(file, file->state->data_file_length);
    param.testflag = testflag;
    optimize_done = 1;
  }
  if (!error) {
    if ((local_testflag & T_SORT_INDEX) &&
        (share->state.changed & STATE_NOT_SORTED_PAGES)) {
      optimize_done = 1;
      thd_proc_info(thd, "Sorting index");
      /*
        The new file is created with the right stats, so we can skip
        copying file stats from old to new.
      */
      error = mi_sort_index(&param, file, fixed_name, true);
    }
    if (!statistics_done && (local_testflag & T_STATISTICS)) {
      if (share->state.changed & STATE_NOT_ANALYZED) {
        optimize_done = 1;
        thd_proc_info(thd, "Analyzing");
        error = chk_key(&param, file);
      } else
        local_testflag &= ~T_STATISTICS;  // Don't update statistics
    }
  }
  thd_proc_info(thd, "Saving state");
  if (!error) {
    if ((share->state.changed & STATE_CHANGED) || mi_is_crashed(file)) {
      share->state.changed &=
          ~(STATE_CHANGED | STATE_CRASHED | STATE_CRASHED_ON_REPAIR);
      file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
    /*
      the following 'if', thought conceptually wrong,
      is a useful optimization nevertheless.
    */
    if (file->state != &file->s->state.state)
      file->s->state.state = *file->state;
    if (file->s->base.auto_key) update_auto_increment_key(&param, file, 1);
    if (optimize_done)
      error = update_state_info(
          &param, file,
          UPDATE_TIME | UPDATE_OPEN_COUNT |
              (local_testflag & T_STATISTICS ? UPDATE_STAT : 0));
    info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
         HA_STATUS_CONST);
    if (rows != file->state->records && !(param.testflag & T_VERY_SILENT)) {
      char llbuff[22], llbuff2[22];
      mi_check_print_warning(&param, "Number of rows changed from %s to %s",
                             llstr(rows, llbuff),
                             llstr(file->state->records, llbuff2));
    }
  } else {
    mi_mark_crashed_on_repair(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    update_state_info(&param, file, 0);
  }
  thd_proc_info(thd, old_proc_info);
  if (!has_old_locks) mi_lock_database(file, F_UNLCK);
  DBUG_RETURN(error ? HA_ADMIN_FAILED
                    : !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}

/*
  Assign table indexes to a specific key cache.
*/

int ha_myisam::assign_to_keycache(THD *thd, HA_CHECK_OPT *check_opt) {
  KEY_CACHE *new_key_cache = check_opt->key_cache;
  const char *errmsg = 0;
  int error = HA_ADMIN_OK;
  ulonglong map;
  TABLE_LIST *table_list = table->pos_in_table_list;
  DBUG_ENTER("ha_myisam::assign_to_keycache");

  table->keys_in_use_for_query.clear_all();

  if (table_list->process_index_hints(thd, table)) DBUG_RETURN(HA_ADMIN_FAILED);
  map = ~(ulonglong)0;
  if (!table->keys_in_use_for_query.is_clear_all())
    /* use all keys if there's no list specified by the user through hints */
    map = table->keys_in_use_for_query.to_ulonglong();

  if ((error = mi_assign_to_key_cache(file, map, new_key_cache))) {
    char buf[STRING_BUFFER_USUAL_SIZE];
    snprintf(buf, sizeof(buf), "Failed to flush to index file (errno: %d)",
             error);
    errmsg = buf;
    error = HA_ADMIN_CORRUPT;
  }

  if (error != HA_ADMIN_OK) {
    /* Send error to user */
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd = thd;
    param.op_name = "assign_to_keycache";
    param.db_name = table->s->db.str;
    param.table_name = table->s->table_name.str;
    param.testflag = 0;
    mi_check_print_error(&param, "%s", errmsg);
  }
  DBUG_RETURN(error);
}

/*
  Preload pages of the index file for a table into the key cache.
*/

int ha_myisam::preload_keys(THD *thd, HA_CHECK_OPT *) {
  int error;
  const char *errmsg;
  ulonglong map;
  TABLE_LIST *table_list = table->pos_in_table_list;
  bool ignore_leaves = table_list->ignore_leaves;
  char buf[MYSQL_ERRMSG_SIZE];

  DBUG_ENTER("ha_myisam::preload_keys");

  table->keys_in_use_for_query.clear_all();

  if (table_list->process_index_hints(thd, table)) DBUG_RETURN(HA_ADMIN_FAILED);

  map = ~(ulonglong)0;
  /* Check validity of the index references */
  if (!table->keys_in_use_for_query.is_clear_all())
    /* use all keys if there's no list specified by the user through hints */
    map = table->keys_in_use_for_query.to_ulonglong();

  mi_extra(file, HA_EXTRA_PRELOAD_BUFFER_SIZE,
           (void *)&thd->variables.preload_buff_size);

  if ((error = mi_preload(file, map, ignore_leaves))) {
    switch (error) {
      case HA_ERR_NON_UNIQUE_BLOCK_SIZE:
        errmsg = "Indexes use different block sizes";
        break;
      case HA_ERR_OUT_OF_MEM:
        errmsg = "Failed to allocate buffer";
        break;
      default:
        snprintf(buf, sizeof(buf), "Failed to read from index file (errno: %d)",
                 my_errno());
        errmsg = buf;
    }
    error = HA_ADMIN_FAILED;
    goto err;
  }

  DBUG_RETURN(HA_ADMIN_OK);

err : {
  MI_CHECK param;
  myisamchk_init(&param);
  param.thd = thd;
  param.op_name = "preload_keys";
  param.db_name = table->s->db.str;
  param.table_name = table->s->table_name.str;
  param.testflag = 0;
  mi_check_print_error(&param, "%s", errmsg);
  DBUG_RETURN(error);
}
}

/*
  Disable indexes, making it persistent if requested.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::disable_indexes(uint mode) {
  int error;

  if (mode == HA_KEY_SWITCH_ALL) {
    /* call a storage engine function to switch the key map */
    error = mi_disable_indexes(file);
  } else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE) {
    mi_extra(file, HA_EXTRA_NO_KEYS, 0);
    info(HA_STATUS_CONST);  // Read new key info
    error = 0;
  } else {
    /* mode not implemented */
    error = HA_ERR_WRONG_COMMAND;
  }
  return error;
}

/*
  Enable indexes, making it persistent if requested.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes, which might have been disabled by disable_index() before.
    The modes without _SAVE work only if both data and indexes are empty,
    since the MyISAM repair would enable them persistently.
    To be sure in these cases, call handler::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented.

  RETURN
    0  ok
    !=0  Error, among others:
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_myisam::enable_indexes(uint mode) {
  int error;

  DBUG_EXECUTE_IF("wait_in_enable_indexes",
                  debug_wait_for_kill("wait_in_enable_indexes"););

  if (mi_is_all_keys_active(file->s->state.key_map, file->s->base.keys)) {
    /* All indexes are enabled already. */
    return 0;
  }

  if (mode == HA_KEY_SWITCH_ALL) {
    error = mi_enable_indexes(file);
    /*
       Do not try to repair on error,
       as this could make the enabled state persistent,
       but mode==HA_KEY_SWITCH_ALL forbids it.
    */
  } else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE) {
    THD *thd = current_thd;
    MI_CHECK param;
    const char *save_proc_info = thd->proc_info;
    thd_proc_info(thd, "Creating index");
    myisamchk_init(&param);
    param.op_name = "recreating_index";
    param.testflag =
        (T_SILENT | T_REP_BY_SORT | T_QUICK | T_CREATE_MISSING_KEYS);
    param.myf_rw &= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length = THDVAR(thd, sort_buffer_size);
    param.stats_method = (enum_mi_stats_method)THDVAR(thd, stats_method);
    param.tmpdir = &mysql_tmpdir_list;
    if ((error = (repair(thd, param, 0) != HA_ADMIN_OK)) &&
        param.retry_repair) {
      LogErr(WARNING_LEVEL, ER_ERROR_ENABLING_KEYS, my_errno(), param.db_name,
             param.table_name);
      /*
        Repairing by sort failed. Now try standard repair method.
        Still we want to fix only index file. If data file corruption
        was detected (T_RETRY_WITHOUT_QUICK), we shouldn't do much here.
        Let implicit repair do this job.
      */
      if (!(param.testflag & T_RETRY_WITHOUT_QUICK)) {
        param.testflag &= ~T_REP_BY_SORT;
        error = (repair(thd, param, 0) != HA_ADMIN_OK);
      }
      /*
        If the standard repair succeeded, clear all error messages which
        might have been set by the first repair. They can still be seen
        with SHOW WARNINGS then.
      */
      if (!error) thd->clear_error();
    }
    info(HA_STATUS_CONST);
    thd_proc_info(thd, save_proc_info);
  } else {
    /* mode not implemented */
    error = HA_ERR_WRONG_COMMAND;
  }
  return error;
}

/*
  Test if indexes are disabled.


  SYNOPSIS
    indexes_are_disabled()
      no parameters


  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_myisam::indexes_are_disabled(void) {
  return mi_indexes_are_disabled(file);
}

/*
  prepare for a many-rows insert operation
  e.g. - disable indexes (if they can be recreated fast) or
  activate special bulk-insert optimizations

  SYNOPSIS
    start_bulk_insert(rows)
    rows        Rows to be inserted
                0 if we don't know

  NOTICE
    Do not forget to call end_bulk_insert() later!
*/

void ha_myisam::start_bulk_insert(ha_rows rows) {
  DBUG_ENTER("ha_myisam::start_bulk_insert");
  THD *thd = current_thd;

  can_enable_indexes =
      mi_is_all_keys_active(file->s->state.key_map, file->s->base.keys);

  /*
    Only disable old index if the table was empty and we are inserting
    a lot of rows.
    Note that in end_bulk_insert() we may truncate the table if
    enable_indexes() failed, thus it's essential that indexes are
    disabled ONLY for an empty table.
  */
  if (file->state->records == 0 && can_enable_indexes &&
      (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES))
    mi_disable_non_unique_index(file, rows);
  else if (!file->bulk_insert &&
           (!rows || rows >= MI_MIN_ROWS_TO_USE_BULK_INSERT)) {
    mi_init_bulk_insert(file, thd->variables.bulk_insert_buff_size, rows);
  }
  DBUG_VOID_RETURN;
}

/*
  end special bulk-insert optimizations,
  which have been activated by start_bulk_insert().

  SYNOPSIS
    end_bulk_insert()
    no arguments

  RETURN
    0     OK
    != 0  Error
*/

int ha_myisam::end_bulk_insert() {
  mi_end_bulk_insert(file);
  int err = 0;
  if (can_enable_indexes) {
    /*
      Truncate the table when enable index operation is killed.
      After truncating the table we don't need to enable the
      indexes, because the last repair operation is aborted after
      setting the indexes as active and  trying to recreate them.
   */

    if (((err = enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE)) != 0) &&
        current_thd->killed) {
      delete_all_rows();
      /* not crashed, despite being killed during repair */
      file->s->state.changed &= ~(STATE_CRASHED | STATE_CRASHED_ON_REPAIR);
    }
  }
  return err;
}

bool ha_myisam::check_and_repair(THD *thd) {
  int error = 0;
  int marked_crashed;
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_myisam::check_and_repair");

  check_opt.init();
  check_opt.flags = T_MEDIUM | T_AUTO_REPAIR;
  // Don't use quick if deleted rows
  if (!file->state->del && (myisam_recover_options & HA_RECOVER_QUICK))
    check_opt.flags |= T_QUICK;
  LogErr(WARNING_LEVEL, ER_CHECKING_TABLE, table->s->path.str);

  if ((marked_crashed = mi_is_crashed(file)) || check(thd, &check_opt)) {
    LogErr(WARNING_LEVEL, ER_RECOVERING_TABLE, table->s->path.str);
    check_opt.flags =
        ((myisam_recover_options & HA_RECOVER_BACKUP ? T_BACKUP_DATA : 0) |
         (marked_crashed ? 0 : T_QUICK) |
         (myisam_recover_options & HA_RECOVER_FORCE ? 0 : T_SAFE_REPAIR) |
         T_AUTO_REPAIR);
    if (repair(thd, &check_opt)) error = 1;
  }
  DBUG_RETURN(error);
}

bool ha_myisam::is_crashed() const {
  return (file->s->state.changed & STATE_CRASHED ||
          (my_disable_locking && file->s->state.open_count));
}

int ha_myisam::update_row(const uchar *old_data, uchar *new_data) {
  ha_statistic_increment(&System_status_var::ha_update_count);
  return mi_update(file, old_data, new_data);
}

int ha_myisam::delete_row(const uchar *buf) {
  ha_statistic_increment(&System_status_var::ha_delete_count);
  return mi_delete(file, buf);
}

C_MODE_START

ICP_RESULT index_cond_func_myisam(void *arg) {
  ha_myisam *h = (ha_myisam *)arg;

  if (h->end_range && h->compare_key_icp(h->end_range) > 0)
    return ICP_OUT_OF_RANGE; /* caller should return HA_ERR_END_OF_FILE already
                              */

  return (ICP_RESULT)MY_TEST(h->pushed_idx_cond->val_int());
}

C_MODE_END

int ha_myisam::index_init(uint idx, bool) {
  active_index = idx;
  if (pushed_idx_cond_keyno == idx)
    mi_set_index_cond_func(file, index_cond_func_myisam, this);
  return 0;
}

int ha_myisam::index_end() {
  active_index = MAX_KEY;
  // pushed_idx_cond_keyno= MAX_KEY;
  mi_set_index_cond_func(file, NULL, 0);
  in_range_check_pushed_down = false;
  ds_mrr.dsmrr_close();
  return 0;
}

int ha_myisam::rnd_end() {
  ds_mrr.dsmrr_close();
  return 0;
}

int ha_myisam::index_read_map(uchar *buf, const uchar *key,
                              key_part_map keypart_map,
                              enum ha_rkey_function find_flag) {
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = mi_rkey(file, buf, active_index, key, keypart_map, find_flag);
  return error;
}

int ha_myisam::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                  key_part_map keypart_map,
                                  enum ha_rkey_function find_flag) {
  DBUG_ASSERT(pushed_idx_cond == NULL);
  DBUG_ASSERT(pushed_idx_cond_keyno == MAX_KEY);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = mi_rkey(file, buf, index, key, keypart_map, find_flag);
  return error;
}

int ha_myisam::index_read_last_map(uchar *buf, const uchar *key,
                                   key_part_map keypart_map) {
  DBUG_ENTER("ha_myisam::index_read_last");
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error =
      mi_rkey(file, buf, active_index, key, keypart_map, HA_READ_PREFIX_LAST);
  DBUG_RETURN(error);
}

int ha_myisam::index_next(uchar *buf) {
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  int error = mi_rnext(file, buf, active_index);
  return error;
}

int ha_myisam::index_prev(uchar *buf) {
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_prev_count);
  int error = mi_rprev(file, buf, active_index);
  return error;
}

int ha_myisam::index_first(uchar *buf) {
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  int error = mi_rfirst(file, buf, active_index);
  return error;
}

int ha_myisam::index_last(uchar *buf) {
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_last_count);
  int error = mi_rlast(file, buf, active_index);
  return error;
}

int ha_myisam::index_next_same(uchar *buf,
                               const uchar *key MY_ATTRIBUTE((unused)),
                               uint length MY_ATTRIBUTE((unused))) {
  int error;
  DBUG_ASSERT(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  do {
    error = mi_rnext_same(file, buf);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}

int ha_myisam::rnd_init(bool scan) {
  if (scan) return mi_scan_init(file);
  return mi_reset(file);  // Free buffers
}

int ha_myisam::rnd_next(uchar *buf) {
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
  int error = mi_scan(file, buf);
  return error;
}

int ha_myisam::rnd_pos(uchar *buf, uchar *pos) {
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  int error = mi_rrnd(file, buf, my_get_ptr(pos, ref_length));
  return error;
}

void ha_myisam::position(const uchar *) {
  my_off_t row_position = mi_position(file);
  my_store_ptr(ref, ref_length, row_position);
}

int ha_myisam::info(uint flag) {
  MI_ISAMINFO misam_info;
  char name_buff[FN_REFLEN];

  (void)mi_status(file, &misam_info, flag);
  if (flag & HA_STATUS_VARIABLE) {
    stats.records = misam_info.records;
    stats.deleted = misam_info.deleted;
    stats.data_file_length = misam_info.data_file_length;
    stats.index_file_length = misam_info.index_file_length;
    stats.delete_length = misam_info.delete_length;
    stats.check_time = (ulong)misam_info.check_time;
    stats.mean_rec_length = misam_info.mean_reclength;
  }
  if (flag & HA_STATUS_CONST) {
    TABLE_SHARE *share = table->s;
    stats.max_data_file_length = misam_info.max_data_file_length;
    stats.max_index_file_length = misam_info.max_index_file_length;
    stats.create_time = misam_info.create_time;
    /*
      We want the value of stats.mrr_length_per_rec to be platform independent.
      The size of the chunk at the end of the join buffer used for MRR needs
      is calculated now basing on the values passed in the stats structure.
      The remaining part of the join buffer is used for records. A different
      number of records in the buffer results in a different number of buffer
      refills and in a different order of records in the result set.
    */
    stats.mrr_length_per_rec =
        misam_info.reflength + 8;  // 8=max(sizeof(void *))
    ref_length = misam_info.reflength;
    share->db_options_in_use = misam_info.options;
    stats.block_size = myisam_block_size; /* record block size */

    /*
      Update share.
      lock_shared_ha_data is slighly abused here, since there is no other
      way of locking the TABLE_SHARE.
    */
    lock_shared_ha_data();
    share->keys_in_use.set_prefix(share->keys);
    share->keys_in_use.intersect_extended(misam_info.key_map);
    share->keys_for_keyread.intersect(share->keys_in_use);
    unlock_shared_ha_data();
    if (share->key_parts)
      memcpy((char *)table->key_info[0].rec_per_key,
             (char *)misam_info.rec_per_key,
             sizeof(table->key_info[0].rec_per_key[0]) * share->key_parts);

    /*
      Set data_file_name and index_file_name to point at the symlink value
      if table is symlinked (Ie;  Real name is not same as generated name)
    */
    data_file_name = index_file_name = 0;
    fn_format(name_buff, file->filename, "", MI_NAME_DEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.data_file_name))
      data_file_name = misam_info.data_file_name;
    fn_format(name_buff, file->filename, "", MI_NAME_IEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.index_file_name))
      index_file_name = misam_info.index_file_name;
  }
  if (flag & HA_STATUS_ERRKEY) {
    errkey = misam_info.errkey;
    my_store_ptr(dup_ref, ref_length, misam_info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME) stats.update_time = (ulong)misam_info.update_time;
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value = misam_info.auto_increment;

  return 0;
}

int ha_myisam::extra(enum ha_extra_function operation) {
  return mi_extra(file, operation, 0);
}

int ha_myisam::reset(void) {
  /* Reset MyISAM specific part for index condition pushdown */
  DBUG_ASSERT(pushed_idx_cond == NULL);
  DBUG_ASSERT(pushed_idx_cond_keyno == MAX_KEY);
  mi_set_index_cond_func(file, NULL, 0);
  ds_mrr.reset();
  return mi_reset(file);
}

/* To be used with WRITE_CACHE and EXTRA_CACHE */

int ha_myisam::extra_opt(enum ha_extra_function operation, ulong cache_size) {
  return mi_extra(file, operation, (void *)&cache_size);
}

int ha_myisam::delete_all_rows() { return mi_delete_all_rows(file); }

int ha_myisam::delete_table(const char *name, const dd::Table *) {
  return mi_delete_table(name);
}

int ha_myisam::external_lock(THD *thd, int lock_type) {
  file->in_use.data = thd;
  return mi_lock_database(
      file, !table->s->tmp_table
                ? lock_type
                : ((lock_type == F_UNLCK) ? F_UNLCK : F_EXTRA_LCK));
}

THR_LOCK_DATA **ha_myisam::store_lock(THD *, THR_LOCK_DATA **to,
                                      enum thr_lock_type lock_type) {
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type = lock_type;
  *to++ = &file->lock;
  return to;
}

void ha_myisam::update_create_info(HA_CREATE_INFO *create_info) {
  ha_myisam::info(HA_STATUS_AUTO | HA_STATUS_CONST);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    create_info->auto_increment_value = stats.auto_increment_value;
  }
  create_info->data_file_name = data_file_name;
  create_info->index_file_name = index_file_name;
}

int ha_myisam::create(const char *name, TABLE *table_arg,
                      HA_CREATE_INFO *ha_create_info, dd::Table *) {
  int error;
  uint create_flags = 0, records, i;
  char buff[FN_REFLEN];
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo;
  MI_CREATE_INFO create_info;
  TABLE_SHARE *share = table_arg->s;
  uint options = share->db_options_in_use;
  DBUG_ENTER("ha_myisam::create");
  if (ha_create_info->encrypt_type.length > 0) {
    set_my_errno(HA_WRONG_CREATE_OPTION);
    DBUG_RETURN(HA_WRONG_CREATE_OPTION);
  }
  for (i = 0; i < share->keys; i++) {
    if (table_arg->key_info[i].flags & HA_USES_PARSER) {
      create_flags |= HA_CREATE_RELIES_ON_SQL_LAYER;
      break;
    }
  }
  if ((error = table2myisam(table_arg, &keydef, &recinfo, &records)))
    DBUG_RETURN(error); /* purecov: inspected */
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows = share->max_rows;
  create_info.reloc_rows = share->min_rows;
  create_info.with_auto_increment = share->next_number_key_offset == 0;
  create_info.auto_increment = (ha_create_info->auto_increment_value
                                    ? ha_create_info->auto_increment_value - 1
                                    : (ulonglong)0);
  create_info.data_file_length =
      ((ulonglong)share->max_rows * share->avg_row_length);
  create_info.language = share->table_charset->number;

#ifndef _WIN32
  if (my_enable_symlinks) {
    create_info.data_file_name = ha_create_info->data_file_name;
    create_info.index_file_name = ha_create_info->index_file_name;
  } else
#endif /* !_WIN32 */
  {
    if (ha_create_info->data_file_name)
      push_warning_printf(
          table_arg->in_use, Sql_condition::SL_WARNING, WARN_OPTION_IGNORED,
          ER_THD(table_arg->in_use, WARN_OPTION_IGNORED), "DATA DIRECTORY");
    if (ha_create_info->index_file_name)
      push_warning_printf(
          table_arg->in_use, Sql_condition::SL_WARNING, WARN_OPTION_IGNORED,
          ER_THD(table_arg->in_use, WARN_OPTION_IGNORED), "INDEX DIRECTORY");
  }

  if (ha_create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags |= HA_CREATE_TMP_TABLE;
  if (ha_create_info->options & HA_CREATE_KEEP_FILES)
    create_flags |= HA_CREATE_KEEP_FILES;
  if (options & HA_OPTION_PACK_RECORD) create_flags |= HA_PACK_RECORD;
  if (options & HA_OPTION_CHECKSUM) create_flags |= HA_CREATE_CHECKSUM;
  if (options & HA_OPTION_DELAY_KEY_WRITE)
    create_flags |= HA_CREATE_DELAY_KEY_WRITE;

  /* TODO: Check that the following fn_format is really needed */
  error = mi_create(
      fn_format(buff, name, "", "", MY_UNPACK_FILENAME | MY_APPEND_EXT),
      share->keys, keydef, records, recinfo, 0, (MI_UNIQUEDEF *)0, &create_info,
      create_flags);
  my_free(recinfo);
  DBUG_RETURN(error);
}

int ha_myisam::rename_table(const char *from, const char *to, const dd::Table *,
                            dd::Table *) {
  return mi_rename(from, to);
}

void ha_myisam::get_auto_increment(ulonglong, ulonglong, ulonglong,
                                   ulonglong *first_value,
                                   ulonglong *nb_reserved_values) {
  ulonglong nr;
  int error;
  uchar key[MI_MAX_KEY_LENGTH];

  if (!table->s->next_number_key_offset) {  // Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    *first_value = stats.auto_increment_value;
    /* MyISAM has only table-level lock, so reserves to +inf */
    *nb_reserved_values = ULLONG_MAX;
    return;
  }

  /* it's safe to call the following if bulk_insert isn't on */
  mi_flush_bulk_insert(file, table->s->next_number_index);

  (void)extra(HA_EXTRA_KEYREAD);
  key_copy(key, table->record[0], table->key_info + table->s->next_number_index,
           table->s->next_number_key_offset);
  error = mi_rkey(file, table->record[1], (int)table->s->next_number_index, key,
                  make_prev_keypart_map(table->s->next_number_keypart),
                  HA_READ_PREFIX_LAST);
  if (error)
    nr = 1;
  else {
    /* Get data from record[1] */
    nr = ((ulonglong)table->next_number_field->val_int_offset(
              table->s->rec_buff_length) +
          1);
  }
  extra(HA_EXTRA_NO_KEYREAD);
  *first_value = nr;
  /*
    MySQL needs to call us for next row: assume we are inserting ("a",null)
    here, we return 3, and next this statement will want to insert ("b",null):
    there is no reason why ("b",3+1) would be the good row to insert: maybe it
    already exists, maybe 3+1 is too large...
  */
  *nb_reserved_values = 1;
}

/*
  Find out how many rows there is in the given range

  SYNOPSIS
    records_in_range()
    inx			Index to use
    min_key		Start of range.  Null pointer if from first key
    max_key		End of range. Null pointer if to last key

  NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT		Include the key in the range
      HA_READ_AFTER_KEY		Don't include key in range

    max_key.flag can have one of the following values:
      HA_READ_BEFORE_KEY	Don't include key in range
      HA_READ_AFTER_KEY		Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR		Something is wrong with the index tree.
   0			There is no matching keys in the given range
   number > 0		There is approximately 'number' matching rows in
                        the range.
*/

ha_rows ha_myisam::records_in_range(uint inx, key_range *min_key,
                                    key_range *max_key) {
  return (ha_rows)mi_records_in_range(file, (int)inx, min_key, max_key);
}

int ha_myisam::ft_read(uchar *buf) {
  int error;

  if (!ft_handler) return -1;

  ha_statistic_increment(&System_status_var::ha_read_next_count);

  error = ft_handler->please->read_next(ft_handler, (char *)buf);

  return error;
}

uint ha_myisam::checksum() const { return (uint)file->state->checksum; }

bool ha_myisam::check_if_incompatible_data(HA_CREATE_INFO *info,
                                           uint table_changes) {
  uint options = table->s->db_options_in_use;

  if (info->auto_increment_value != stats.auto_increment_value ||
      info->data_file_name != data_file_name ||
      info->index_file_name != index_file_name ||
      table_changes == IS_EQUAL_NO ||
      table_changes & IS_EQUAL_PACK_LENGTH)  // Not implemented yet
    return COMPATIBLE_DATA_NO;

  if ((options & (HA_OPTION_PACK_RECORD | HA_OPTION_CHECKSUM |
                  HA_OPTION_DELAY_KEY_WRITE)) !=
      (info->table_options & (HA_OPTION_PACK_RECORD | HA_OPTION_CHECKSUM |
                              HA_OPTION_DELAY_KEY_WRITE)))
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

static int myisam_panic(handlerton *, ha_panic_function flag) {
  return mi_panic(flag);
}

st_keycache_thread_var *keycache_thread_var() {
  THD *thd = current_thd;
  if (thd == NULL) {
    /*
      This is not a thread belonging to a connection.
      It will then be the main thread during startup/shutdown or
      extra threads created for thr_find_all_keys().
    */
    return keycache_tls;
  }

  /*
    For connection threads keycache thread state is stored in Ha_data::ha_ptr.
    This pointer has lifetime for the connection duration and is not used
    for anything else by MyISAM.

    @see Ha_data (sql_class.h)
  */
  st_keycache_thread_var *keycache_thread_var =
      static_cast<st_keycache_thread_var *>(thd_get_ha_data(thd, myisam_hton));
  if (!keycache_thread_var) {
    /* Lazy initialization */
    keycache_thread_var = static_cast<st_keycache_thread_var *>(
        my_malloc(mi_key_memory_keycache_thread_var,
                  sizeof(st_keycache_thread_var), MYF(MY_ZEROFILL)));
    mysql_cond_init(mi_keycache_thread_var_suspend,
                    &keycache_thread_var->suspend);
    thd_set_ha_data(thd, myisam_hton, keycache_thread_var);
  }
  return keycache_thread_var;
}

static int myisam_close_connection(handlerton *hton, THD *thd) {
  st_keycache_thread_var *keycache_thread_var =
      static_cast<st_keycache_thread_var *>(thd_get_ha_data(thd, hton));

  if (keycache_thread_var) {
    thd_set_ha_data(thd, hton, NULL);
    mysql_cond_destroy(&keycache_thread_var->suspend);
    my_free(keycache_thread_var);
  }

  return 0;
}

static int myisam_init(void *p) {
  handlerton *myisam_hton;

#ifdef HAVE_PSI_INTERFACE
  init_myisam_psi_keys();
#endif

  /* Set global variables based on startup options */
  if (myisam_recover_options)
    ha_open_options |= HA_OPEN_ABORT_IF_CRASHED;
  else
    myisam_recover_options = HA_RECOVER_OFF;

  myisam_block_size = (uint)1 << my_bit_log2(opt_myisam_block_size);

  myisam_hton = (handlerton *)p;
  myisam_hton->state = SHOW_OPTION_YES;
  myisam_hton->db_type = DB_TYPE_MYISAM;
  myisam_hton->create = myisam_create_handler;
  myisam_hton->panic = myisam_panic;
  myisam_hton->close_connection = myisam_close_connection;
  myisam_hton->flags = HTON_CAN_RECREATE | HTON_SUPPORT_LOG_TABLES;
  myisam_hton->is_supported_system_table = myisam_is_supported_system_table;
  myisam_hton->file_extensions = ha_myisam_exts;
  myisam_hton->rm_tmp_tables = default_rm_tmp_tables;

  main_thread_keycache_var = st_keycache_thread_var();
  mysql_cond_init(mi_keycache_thread_var_suspend,
                  &main_thread_keycache_var.suspend);
  keycache_tls = &main_thread_keycache_var;
  return 0;
}

static int myisam_deinit(void *) {
  mysql_cond_destroy(&main_thread_keycache_var.suspend);
  keycache_tls = nullptr;
  return 0;
}

/****************************************************************************
 * MyISAM MRR implementation: use DS-MRR
 ***************************************************************************/

int ha_myisam::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                     uint n_ranges, uint mode,
                                     HANDLER_BUFFER *buf) {
  ds_mrr.init(table);
  return ds_mrr.dsmrr_init(seq, seq_init_param, n_ranges, mode, buf);
}

int ha_myisam::multi_range_read_next(char **range_info) {
  return ds_mrr.dsmrr_next(range_info);
}

ha_rows ha_myisam::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                               void *seq_init_param,
                                               uint n_ranges, uint *bufsz,
                                               uint *flags,
                                               Cost_estimate *cost) {
  /*
    This call is here because there is no location where this->table would
    already be known.
    TODO: consider moving it into some per-query initialization call.
  */
  ds_mrr.init(table);
  return ds_mrr.dsmrr_info_const(keyno, seq, seq_init_param, n_ranges, bufsz,
                                 flags, cost);
}

ha_rows ha_myisam::multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                         uint *bufsz, uint *flags,
                                         Cost_estimate *cost) {
  ds_mrr.init(table);
  return ds_mrr.dsmrr_info(keyno, n_ranges, keys, bufsz, flags, cost);
}

/* MyISAM MRR implementation ends */

/* Index condition pushdown implementation*/

Item *ha_myisam::idx_cond_push(uint keyno_arg, Item *idx_cond_arg) {
  /*
    Check if the key contains a blob field. If it does then MyISAM
    should not accept the pushed index condition since MyISAM will not
    read the blob field from the index entry during evaluation of the
    pushed index condition and the BLOB field might be part of the
    range evaluation done by the ICP code.
  */
  const KEY *key = &table_share->key_info[keyno_arg];

  for (uint k = 0; k < key->user_defined_key_parts; ++k) {
    const KEY_PART_INFO *key_part = &key->key_part[k];
    if (key_part->key_part_flag & HA_BLOB_PART) {
      /* Let the server handle the index condition */
      return idx_cond_arg;
    }
  }

  pushed_idx_cond_keyno = keyno_arg;
  pushed_idx_cond = idx_cond_arg;
  in_range_check_pushed_down = true;
  if (active_index == pushed_idx_cond_keyno)
    mi_set_index_cond_func(file, index_cond_func_myisam, this);
  return NULL;
}

static SYS_VAR *myisam_sysvars[] = {
    MYSQL_SYSVAR(block_size),         MYSQL_SYSVAR(data_pointer_size),
    MYSQL_SYSVAR(max_sort_file_size), MYSQL_SYSVAR(recover_options),
    MYSQL_SYSVAR(repair_threads),     MYSQL_SYSVAR(sort_buffer_size),
    MYSQL_SYSVAR(use_mmap),           MYSQL_SYSVAR(mmap_size),
    MYSQL_SYSVAR(stats_method),       0};

struct st_mysql_storage_engine myisam_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(myisam){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &myisam_storage_engine,
    "MyISAM",
    "MySQL AB",
    "MyISAM storage engine",
    PLUGIN_LICENSE_GPL,
    myisam_init,    /* Plugin Init */
    NULL,           /* Plugin Check uninstall */
    myisam_deinit,  /* Plugin Deinit */
    0x0100,         /* 1.0 */
    NULL,           /* status variables                */
    myisam_sysvars, /* system variables                */
    NULL,
    0,
} mysql_declare_plugin_end;
