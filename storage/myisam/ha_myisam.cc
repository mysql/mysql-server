/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "sql_priv.h"
#include "probes_mysql.h"
#include "key.h"                                // key_copy
#include "sql_plugin.h"
#include <m_ctype.h>
#include <my_bit.h>
#include <myisampack.h>
#include "ha_myisam.h"
#include <stdarg.h>
#include "myisamdef.h"
#include "rt_index.h"
#include "sql_table.h"                          // tablename_to_filename
#include "sql_class.h"                          // THD

ulonglong myisam_recover_options;
static ulong opt_myisam_block_size;

/* Interface to mysqld, to check system tables supported by SE */
static bool myisam_is_supported_system_table(const char *db,
                                             const char *table_name,
                                             bool is_sql_layer_system_table);

/* bits in myisam_recover_options */
const char *myisam_recover_names[] =
{ "DEFAULT", "BACKUP", "FORCE", "QUICK", "OFF", NullS};
TYPELIB myisam_recover_typelib= {array_elements(myisam_recover_names)-1,"",
				 myisam_recover_names, NULL};

const char *myisam_stats_method_names[] = {"nulls_unequal", "nulls_equal",
                                           "nulls_ignored", NullS};
TYPELIB myisam_stats_method_typelib= {
  array_elements(myisam_stats_method_names) - 1, "",
  myisam_stats_method_names, NULL};

static MYSQL_SYSVAR_ULONG(block_size, opt_myisam_block_size,
  PLUGIN_VAR_NOSYSVAR | PLUGIN_VAR_RQCMDARG,
  "Block size to be used for MyISAM index pages", NULL, NULL,
  MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, MI_MAX_KEY_BLOCK_LENGTH,
  MI_MIN_KEY_BLOCK_LENGTH);

static MYSQL_SYSVAR_ULONG(data_pointer_size, myisam_data_pointer_size,
  PLUGIN_VAR_RQCMDARG, "Default pointer size to be used for MyISAM tables",
  NULL, NULL, 6, 2, 7, 1);

#define MB (1024*1024)
static MYSQL_SYSVAR_ULONGLONG(max_sort_file_size, myisam_max_temp_length,
  PLUGIN_VAR_RQCMDARG, "Don't use the fast sort index method to created "
  "index if the temporary file would get bigger than this", NULL, NULL,
  LONG_MAX/MB*MB, 0, MAX_FILE_SIZE, MB);

static MYSQL_SYSVAR_SET(recover_options, myisam_recover_options,
  PLUGIN_VAR_OPCMDARG|PLUGIN_VAR_READONLY,
  "Syntax: myisam-recover-options[=option[,option...]], where option can be "
  "DEFAULT, BACKUP, FORCE, QUICK, or OFF",
  NULL, NULL, 0, &myisam_recover_typelib);

static MYSQL_THDVAR_ULONG(repair_threads, PLUGIN_VAR_RQCMDARG,
  "If larger than 1, when repairing a MyISAM table all indexes will be "
  "created in parallel, with one thread per index. The value of 1 "
  "disables parallel repair", NULL, NULL,
  1, 1, ULONG_MAX, 1);

static MYSQL_THDVAR_ULONGLONG(sort_buffer_size, PLUGIN_VAR_RQCMDARG,
  "The buffer that is allocated when sorting the index when doing "
  "a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE", NULL, NULL,
  8192 * 1024, (long) (MIN_SORT_BUFFER + MALLOC_OVERHEAD), SIZE_T_MAX, 1);

static MYSQL_SYSVAR_BOOL(use_mmap, opt_myisam_use_mmap, PLUGIN_VAR_NOCMDARG,
  "Use memory mapping for reading and writing MyISAM tables", NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONGLONG(mmap_size, myisam_mmap_size,
  PLUGIN_VAR_RQCMDARG|PLUGIN_VAR_READONLY, "Restricts the total memory "
  "used for memory mapping of MySQL tables", NULL, NULL,
  SIZE_T_MAX, MEMMAP_EXTRA_MARGIN, SIZE_T_MAX, 1);

static MYSQL_THDVAR_ENUM(stats_method, PLUGIN_VAR_RQCMDARG,
  "Specifies how MyISAM index statistics collection code should "
  "treat NULLs. Possible values of name are NULLS_UNEQUAL (default "
  "behavior for 4.1 and later), NULLS_EQUAL (emulate 4.0 behavior), "
  "and NULLS_IGNORED", NULL, NULL,
  MI_STATS_METHOD_NULLS_NOT_EQUAL, &myisam_stats_method_typelib);

#ifndef DBUG_OFF
/**
  Causes the thread to wait in a spin lock for a query kill signal.
  This function is used by the test frame work to identify race conditions.

  The signal is caught and ignored and the thread is not killed.
*/

static void debug_wait_for_kill(const char *info)
{
  DBUG_ENTER("debug_wait_for_kill");
  const char *prev_info;
  THD *thd;
  thd= current_thd;
  prev_info= thd_proc_info(thd, info);
  while(!thd->killed)
    my_sleep(1000);
  DBUG_PRINT("info", ("Exit debug_wait_for_kill"));
  thd_proc_info(thd, prev_info);
  DBUG_VOID_RETURN;
}
#endif

/*****************************************************************************
** MyISAM tables
*****************************************************************************/

static handler *myisam_create_handler(handlerton *hton,
                                      TABLE_SHARE *table, 
                                      MEM_ROOT *mem_root)
{
  return new (mem_root) ha_myisam(hton, table);
}

// collect errors printed by mi_check routines

static void mi_check_print_msg(MI_CHECK *param,	const char* msg_type,
			       const char *fmt, va_list args)
{
  THD* thd = (THD*)param->thd;
  Protocol *protocol= thd->protocol;
  size_t length, msg_length;
  char msgbuf[MI_MAX_MSG_BUF];
  char name[NAME_LEN*2+2];

  msg_length= my_vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
  msgbuf[sizeof(msgbuf) - 1] = 0; // healthy paranoia

  DBUG_PRINT(msg_type,("message: %s",msgbuf));

  if (!thd->vio_ok())
  {
    sql_print_error("%s", msgbuf);
    return;
  }

  if (param->testflag & (T_CREATE_MISSING_KEYS | T_SAFE_REPAIR |
			 T_AUTO_REPAIR))
  {
    my_message(ER_NOT_KEYFILE,msgbuf,MYF(MY_WME));
    return;
  }
  length=(uint) (strxmov(name, param->db_name,".",param->table_name,NullS) -
		 name);
  /*
    TODO: switch from protocol to push_warning here. The main reason we didn't
    it yet is parallel repair. Due to following trace:
    mi_check_print_msg/push_warning/sql_alloc/my_pthread_getspecific_ptr.

    Also we likely need to lock mutex here (in both cases with protocol and
    push_warning).
  */
  if (param->need_print_msg_lock)
    mysql_mutex_lock(&param->print_msg_mutex);

  protocol->prepare_for_resend();
  protocol->store(name, length, system_charset_info);
  protocol->store(param->op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->write())
    sql_print_error("Failed on my_net_write, writing to stderr instead: %s\n",
		    msgbuf);

  if (param->need_print_msg_lock)
    mysql_mutex_unlock(&param->print_msg_mutex);

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
                 MI_COLUMNDEF **recinfo_out, uint *records_out)
{
  uint i, j, recpos, minpos, fieldpos, temp_length, length;
  enum ha_base_keytype type= HA_KEYTYPE_BINARY;
  uchar *record;
  KEY *pos;
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo, *recinfo_pos;
  HA_KEYSEG *keyseg;
  TABLE_SHARE *share= table_arg->s;
  uint options= share->db_options_in_use;
  DBUG_ENTER("table2myisam");
  if (!(my_multi_malloc(MYF(MY_WME),
          recinfo_out, (share->fields * 2 + 2) * sizeof(MI_COLUMNDEF),
          keydef_out, share->keys * sizeof(MI_KEYDEF),
          &keyseg,
          (share->key_parts + share->keys) * sizeof(HA_KEYSEG),
          NullS)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM); /* purecov: inspected */
  keydef= *keydef_out;
  recinfo= *recinfo_out;
  pos= table_arg->key_info;
  for (i= 0; i < share->keys; i++, pos++)
  {
    keydef[i].flag= ((uint16) pos->flags & (HA_NOSAME | HA_FULLTEXT | HA_SPATIAL));
    keydef[i].key_alg= pos->algorithm == HA_KEY_ALG_UNDEF ?
      (pos->flags & HA_SPATIAL ? HA_KEY_ALG_RTREE : HA_KEY_ALG_BTREE) :
      pos->algorithm;
    keydef[i].block_length= pos->block_size;
    keydef[i].seg= keyseg;
    keydef[i].keysegs= pos->key_parts;
    for (j= 0; j < pos->key_parts; j++)
    {
      Field *field= pos->key_part[j].field;
      type= field->key_type();
      keydef[i].seg[j].flag= pos->key_part[j].key_part_flag;

      if (options & HA_OPTION_PACK_KEYS ||
          (pos->flags & (HA_PACK_KEY | HA_BINARY_PACK_KEY |
                         HA_SPACE_PACK_USED)))
      {
        if (pos->key_part[j].length > 8 &&
            (type == HA_KEYTYPE_TEXT ||
             type == HA_KEYTYPE_NUM ||
             (type == HA_KEYTYPE_BINARY && !field->zero_pack())))
        {
          /* No blobs here */
          if (j == 0)
            keydef[i].flag|= HA_PACK_KEY;
          if (!(field->flags & ZEROFILL_FLAG) &&
              (field->type() == MYSQL_TYPE_STRING ||
               field->type() == MYSQL_TYPE_VAR_STRING ||
               ((int) (pos->key_part[j].length - field->decimals())) >= 4))
            keydef[i].seg[j].flag|= HA_SPACE_PACK;
        }
        else if (j == 0 && (!(pos->flags & HA_NOSAME) || pos->key_length > 16))
          keydef[i].flag|= HA_BINARY_PACK_KEY;
      }
      keydef[i].seg[j].type= (int) type;
      keydef[i].seg[j].start= pos->key_part[j].offset;
      keydef[i].seg[j].length= pos->key_part[j].length;
      keydef[i].seg[j].bit_start= keydef[i].seg[j].bit_end=
        keydef[i].seg[j].bit_length= 0;
      keydef[i].seg[j].bit_pos= 0;
      keydef[i].seg[j].language= field->charset_for_protocol()->number;

      if (field->null_ptr)
      {
        keydef[i].seg[j].null_bit= field->null_bit;
        keydef[i].seg[j].null_pos= (uint) (field->null_ptr-
                                           (uchar*) table_arg->record[0]);
      }
      else
      {
        keydef[i].seg[j].null_bit= 0;
        keydef[i].seg[j].null_pos= 0;
      }
      if (field->type() == MYSQL_TYPE_BLOB ||
          field->type() == MYSQL_TYPE_GEOMETRY)
      {
        keydef[i].seg[j].flag|= HA_BLOB_PART;
        /* save number of bytes used to pack length */
        keydef[i].seg[j].bit_start= (uint) (field->pack_length() -
                                            share->blob_ptr_size);
      }
      else if (field->type() == MYSQL_TYPE_BIT)
      {
        keydef[i].seg[j].bit_length= ((Field_bit *) field)->bit_len;
        keydef[i].seg[j].bit_start= ((Field_bit *) field)->bit_ofs;
        keydef[i].seg[j].bit_pos= (uint) (((Field_bit *) field)->bit_ptr -
                                          (uchar*) table_arg->record[0]);
      }
    }
    keyseg+= pos->key_parts;
  }
  if (table_arg->found_next_number_field)
    keydef[share->next_number_index].flag|= HA_AUTO_KEY;
  record= table_arg->record[0];
  recpos= 0;
  recinfo_pos= recinfo;
  while (recpos < (uint) share->reclength)
  {
    Field **field, *found= 0;
    minpos= share->reclength;
    length= 0;

    for (field= table_arg->field; *field; field++)
    {
      if ((fieldpos= (*field)->offset(record)) >= recpos &&
          fieldpos <= minpos)
      {
        /* skip null fields */
        if (!(temp_length= (*field)->pack_length_in_rec()))
          continue; /* Skip null-fields */
        if (! found || fieldpos < minpos ||
            (fieldpos == minpos && temp_length < length))
        {
          minpos= fieldpos;
          found= *field;
          length= temp_length;
        }
      }
    }
    DBUG_PRINT("loop", ("found: 0x%lx  recpos: %d  minpos: %d  length: %d",
                        (long) found, recpos, minpos, length));
    if (recpos != minpos)
    { // Reserved space (Null bits?)
      bzero((char*) recinfo_pos, sizeof(*recinfo_pos));
      recinfo_pos->type= (int) FIELD_NORMAL;
      recinfo_pos++->length= (uint16) (minpos - recpos);
    }
    if (!found)
      break;

    if (found->flags & BLOB_FLAG)
      recinfo_pos->type= (int) FIELD_BLOB;
    else if (found->type() == MYSQL_TYPE_VARCHAR)
      recinfo_pos->type= FIELD_VARCHAR;
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->type= (int) FIELD_SKIP_ZERO;
    else
      recinfo_pos->type= (int) ((length <= 3 ||
                                 (found->flags & ZEROFILL_FLAG)) ?
                                  FIELD_NORMAL :
                                  found->type() == MYSQL_TYPE_STRING ||
                                  found->type() == MYSQL_TYPE_VAR_STRING ?
                                  FIELD_SKIP_ENDSPACE :
                                  FIELD_SKIP_PRESPACE);
    if (found->null_ptr)
    {
      recinfo_pos->null_bit= found->null_bit;
      recinfo_pos->null_pos= (uint) (found->null_ptr -
                                     (uchar*) table_arg->record[0]);
    }
    else
    {
      recinfo_pos->null_bit= 0;
      recinfo_pos->null_pos= 0;
    }
    (recinfo_pos++)->length= (uint16) length;
    recpos= minpos + length;
    DBUG_PRINT("loop", ("length: %d  type: %d",
                        recinfo_pos[-1].length,recinfo_pos[-1].type));
  }
  *records_out= (uint) (recinfo_pos - recinfo);
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
      table            in    handle to the table object

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
                     uint t1_keys, uint t1_recs,
                     MI_KEYDEF *t2_keyinfo, MI_COLUMNDEF *t2_recinfo,
                     uint t2_keys, uint t2_recs, bool strict, TABLE *table_arg)
{
  uint i, j;
  DBUG_ENTER("check_definition");
  my_bool mysql_40_compat= table_arg && table_arg->s->frm_version < FRM_VER_TRUE_VARCHAR;
  if ((strict ? t1_keys != t2_keys : t1_keys > t2_keys))
  {
    DBUG_PRINT("error", ("Number of keys differs: t1_keys=%u, t2_keys=%u",
                         t1_keys, t2_keys));
    DBUG_RETURN(1);
  }
  if (t1_recs != t2_recs)
  {
    DBUG_PRINT("error", ("Number of recs differs: t1_recs=%u, t2_recs=%u",
                         t1_recs, t2_recs));
    DBUG_RETURN(1);
  }
  for (i= 0; i < t1_keys; i++)
  {
    HA_KEYSEG *t1_keysegs= t1_keyinfo[i].seg;
    HA_KEYSEG *t2_keysegs= t2_keyinfo[i].seg;
    if (t1_keyinfo[i].flag & HA_FULLTEXT && t2_keyinfo[i].flag & HA_FULLTEXT)
      continue;
    else if (t1_keyinfo[i].flag & HA_FULLTEXT ||
             t2_keyinfo[i].flag & HA_FULLTEXT)
    {
       DBUG_PRINT("error", ("Key %d has different definition", i));
       DBUG_PRINT("error", ("t1_fulltext= %d, t2_fulltext=%d",
                            test(t1_keyinfo[i].flag & HA_FULLTEXT),
                            test(t2_keyinfo[i].flag & HA_FULLTEXT)));
       DBUG_RETURN(1);
    }
    if (t1_keyinfo[i].flag & HA_SPATIAL && t2_keyinfo[i].flag & HA_SPATIAL)
      continue;
    else if (t1_keyinfo[i].flag & HA_SPATIAL ||
             t2_keyinfo[i].flag & HA_SPATIAL)
    {
       DBUG_PRINT("error", ("Key %d has different definition", i));
       DBUG_PRINT("error", ("t1_spatial= %d, t2_spatial=%d",
                            test(t1_keyinfo[i].flag & HA_SPATIAL),
                            test(t2_keyinfo[i].flag & HA_SPATIAL)));
       DBUG_RETURN(1);
    }
    if ((!mysql_40_compat &&
        t1_keyinfo[i].key_alg != t2_keyinfo[i].key_alg) ||
        t1_keyinfo[i].keysegs != t2_keyinfo[i].keysegs)
    {
      DBUG_PRINT("error", ("Key %d has different definition", i));
      DBUG_PRINT("error", ("t1_keysegs=%d, t1_key_alg=%d",
                           t1_keyinfo[i].keysegs, t1_keyinfo[i].key_alg));
      DBUG_PRINT("error", ("t2_keysegs=%d, t2_key_alg=%d",
                           t2_keyinfo[i].keysegs, t2_keyinfo[i].key_alg));
      DBUG_RETURN(1);
    }
    for (j=  t1_keyinfo[i].keysegs; j--;)
    {
      uint8 t1_keysegs_j__type= t1_keysegs[j].type;

      /*
        Table migration from 4.1 to 5.1. In 5.1 a *TEXT key part is
        always HA_KEYTYPE_VARTEXT2. In 4.1 we had only the equivalent of
        HA_KEYTYPE_VARTEXT1. Since we treat both the same on MyISAM
        level, we can ignore a mismatch between these types.
      */
      if ((t1_keysegs[j].flag & HA_BLOB_PART) &&
          (t2_keysegs[j].flag & HA_BLOB_PART))
      {
        if ((t1_keysegs_j__type == HA_KEYTYPE_VARTEXT2) &&
            (t2_keysegs[j].type == HA_KEYTYPE_VARTEXT1))
          t1_keysegs_j__type= HA_KEYTYPE_VARTEXT1; /* purecov: tested */
        else if ((t1_keysegs_j__type == HA_KEYTYPE_VARBINARY2) &&
                 (t2_keysegs[j].type == HA_KEYTYPE_VARBINARY1))
          t1_keysegs_j__type= HA_KEYTYPE_VARBINARY1; /* purecov: inspected */
      }

      if ((!mysql_40_compat &&
          t1_keysegs[j].language != t2_keysegs[j].language) ||
          t1_keysegs_j__type != t2_keysegs[j].type ||
          t1_keysegs[j].null_bit != t2_keysegs[j].null_bit ||
          t1_keysegs[j].length != t2_keysegs[j].length)
      {
        DBUG_PRINT("error", ("Key segment %d (key %d) has different "
                             "definition", j, i));
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
  for (i= 0; i < t1_recs; i++)
  {
    MI_COLUMNDEF *t1_rec= &t1_recinfo[i];
    MI_COLUMNDEF *t2_rec= &t2_recinfo[i];
    /*
      FIELD_SKIP_ZERO can be changed to FIELD_NORMAL in mi_create,
      see NOTE1 in mi_create.c
    */
    if ((t1_rec->type != t2_rec->type &&
         !(t1_rec->type == (int) FIELD_SKIP_ZERO &&
           t1_rec->length == 1 &&
           t2_rec->type == (int) FIELD_NORMAL)) ||
        t1_rec->length != t2_rec->length ||
        t1_rec->null_bit != t2_rec->null_bit)
    {
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

volatile int *killed_ptr(MI_CHECK *param)
{
  /* In theory Unsafe conversion, but should be ok for now */
  return (int*) &(((THD *)(param->thd))->killed);
}

void mi_check_print_error(MI_CHECK *param, const char *fmt,...)
{
  param->error_printed|=1;
  param->out_flag|= O_DATA_LOST;
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "error", fmt, args);
  va_end(args);
}

void mi_check_print_info(MI_CHECK *param, const char *fmt,...)
{
  va_list args;
  va_start(args, fmt);
  mi_check_print_msg(param, "info", fmt, args);
  va_end(args);
}

void mi_check_print_warning(MI_CHECK *param, const char *fmt,...)
{
  param->warning_printed=1;
  param->out_flag|= O_DATA_LOST;
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

  @return void
*/

void _mi_report_crashed(MI_INFO *file, const char *message,
                        const char *sfile, uint sline)
{
  THD *cur_thd;
  LIST *element;
  char buf[1024];
  mysql_mutex_lock(&file->s->intern_lock);
  if ((cur_thd= (THD*) file->in_use.data))
    sql_print_error("Got an error from thread_id=%lu, %s:%d", cur_thd->thread_id,
                    sfile, sline);
  else
    sql_print_error("Got an error from unknown thread, %s:%d", sfile, sline);
  if (message)
    sql_print_error("%s", message);
  for (element= file->s->in_use; element; element= list_rest(element))
  {
    THD *thd= (THD*) element->data;
    sql_print_error("%s", thd ? thd_security_context(thd, buf, sizeof(buf), 0)
                              : "Unknown thread accessing table");
  }
  mysql_mutex_unlock(&file->s->intern_lock);
}

}


ha_myisam::ha_myisam(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg), file(0),
  int_table_flags(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
                  HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
                  HA_DUPLICATE_POS | HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY |
                  HA_FILE_BASED | HA_CAN_GEOMETRY | HA_NO_TRANSACTIONS |
                  HA_CAN_INSERT_DELAYED | HA_CAN_BIT_FIELD | HA_CAN_RTREEKEYS |
                  HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT | HA_CAN_REPAIR),
   can_enable_indexes(1)
{}

handler *ha_myisam::clone(const char *name, MEM_ROOT *mem_root)
{
  ha_myisam *new_handler= static_cast <ha_myisam *>(handler::clone(name,
                                                                   mem_root));
  if (new_handler)
    new_handler->file->state= file->state;
  return new_handler;
}


static const char *ha_myisam_exts[] = {
  ".MYI",
  ".MYD",
  NullS
};

const char **ha_myisam::bas_ext() const
{
  return ha_myisam_exts;
}

/**
  @brief Check if the given db.tablename is a system table for this SE.

  @param db                         Database name to check.
  @param table_name                 table name to check.
  @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                    layer system table.

  @note Currently, only MYISAM engine supports all the SQL layer
        system tables, and hence it returns true, when
        is_sql_layer_system_table is set.

  @note In case there is a need to define MYISAM specific system
        database, then please see reference implementation in
        ha_example.cc.

  @return
    @retval TRUE   Given db.table_name is supported system table.
    @retval FALSE  Given db.table_name is not a supported system table.
*/
static bool myisam_is_supported_system_table(const char *db,
                                             const char *table_name,
                                             bool is_sql_layer_system_table)
{
  // Does MYISAM support "ALL" SQL layer system tables ?
  if (is_sql_layer_system_table)
    return true;

  /*
    Currently MYISAM does not support any other SE specific
    system tables. If in future it does, please see ha_example.cc
    for reference implementation.
  */

  return false;
}

const char *ha_myisam::index_type(uint key_number)
{
  return ((table->key_info[key_number].flags & HA_FULLTEXT) ? 
	  "FULLTEXT" :
	  (table->key_info[key_number].flags & HA_SPATIAL) ?
	  "SPATIAL" :
	  (table->key_info[key_number].algorithm == HA_KEY_ALG_RTREE) ?
	  "RTREE" :
	  "BTREE");
}


/* Name is here without an extension */
int ha_myisam::open(const char *name, int mode, uint test_if_locked)
{
  MI_KEYDEF *keyinfo;
  MI_COLUMNDEF *recinfo= 0;
  uint recs;
  uint i;

  /*
    If the user wants to have memory mapped data files, add an
    open_flag. Do not memory map temporary tables because they are
    expected to be inserted and thus extended a lot. Memory mapping is
    efficient for files that keep their size, but very inefficient for
    growing files. Using an open_flag instead of calling mi_extra(...
    HA_EXTRA_MMAP ...) after mi_open() has the advantage that the
    mapping is not repeated for every open, but just done on the initial
    open, when the MyISAM share is created. Everytime the server
    requires to open a new instance of a table it calls this method. We
    will always supply HA_OPEN_MMAP for a permanent table. However, the
    MyISAM storage engine will ignore this flag if this is a secondary
    open of a table that is in use by other threads already (if the
    MyISAM share exists already).
  */
  if (!(test_if_locked & HA_OPEN_TMP_TABLE) && opt_myisam_use_mmap)
    test_if_locked|= HA_OPEN_MMAP;

  if (!(file=mi_open(name, mode, test_if_locked | HA_OPEN_FROM_SQL_LAYER)))
    return (my_errno ? my_errno : -1);
  if (!table->s->tmp_table) /* No need to perform a check for tmp table */
  {
    if ((my_errno= table2myisam(table, &keyinfo, &recinfo, &recs)))
    {
      /* purecov: begin inspected */
      DBUG_PRINT("error", ("Failed to convert TABLE object to MyISAM "
                           "key and column definition"));
      goto err;
      /* purecov: end */
    }
    if (check_definition(keyinfo, recinfo, table->s->keys, recs,
                         file->s->keyinfo, file->s->rec,
                         file->s->base.keys, file->s->base.fields,
                         true, table))
    {
      /* purecov: begin inspected */
      my_errno= HA_ERR_CRASHED;
      goto err;
      /* purecov: end */
    }
  }
  
  if (test_if_locked & (HA_OPEN_IGNORE_IF_LOCKED | HA_OPEN_TMP_TABLE))
    (void) mi_extra(file, HA_EXTRA_NO_WAIT_LOCK, 0);

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    (void) mi_extra(file, HA_EXTRA_WAIT_LOCK, 0);
  if (!table->s->db_record_offset)
    int_table_flags|=HA_REC_NOT_IN_SEQ;
  if (file->s->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
    int_table_flags|=HA_HAS_CHECKSUM;

  for (i= 0; i < table->s->keys; i++)
  {
    plugin_ref parser= table->key_info[i].parser;
    if (table->key_info[i].flags & HA_USES_PARSER)
      file->s->keyinfo[i].parser=
        (struct st_mysql_ftparser *)plugin_decl(parser)->info;
    table->key_info[i].block_size= file->s->keyinfo[i].block_length;
  }
  my_errno= 0;
  goto end;
 err:
  this->close();
 end:
  /*
    Both recinfo and keydef are allocated by my_multi_malloc(), thus only
    recinfo must be freed.
  */
  if (recinfo)
    my_free(recinfo);
  return my_errno;
}

int ha_myisam::close(void)
{
  MI_INFO *tmp=file;
  file=0;
  return mi_close(tmp);
}

int ha_myisam::write_row(uchar *buf)
{
  ha_statistic_increment(&SSV::ha_write_count);

  /* If we have a timestamp column, update it to the current time */
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();

  /*
    If we have an auto_increment column and we are writing a changed row
    or a new row, then update the auto_increment value in the record.
  */
  if (table->next_number_field && buf == table->record[0])
  {
    int error;
    if ((error= update_auto_increment()))
      return error;
  }
  return mi_write(file,buf);
}

int ha_myisam::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  int error;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;
  const char *old_proc_info=thd->proc_info;

  thd_proc_info(thd, "Checking table");
  myisamchk_init(&param);
  param.thd = thd;
  param.op_name =   "check";
  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.testflag = check_opt->flags | T_CHECK | T_SILENT;
  param.stats_method= (enum_mi_stats_method)THDVAR(thd, stats_method);

  if (!(table->db_stat & HA_READ_ONLY))
    param.testflag|= T_STATISTICS;
  param.using_global_keycache = 1;

  if (!mi_is_crashed(file) &&
      (((param.testflag & T_CHECK_ONLY_CHANGED) &&
	!(share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				  STATE_CRASHED_ON_REPAIR)) &&
	share->state.open_count == 0) ||
       ((param.testflag & T_FAST) && (share->state.open_count ==
				      (uint) (share->global_changed ? 1 : 0)))))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_status(&param, file);		// Not fatal
  error = chk_size(&param, file);
  if (!error)
    error |= chk_del(&param, file, param.testflag);
  if (!error)
    error = chk_key(&param, file);
  if (!error)
  {
    if ((!(param.testflag & T_QUICK) &&
	 ((share->options &
	   (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
	  (param.testflag & (T_EXTEND | T_MEDIUM)))) ||
	mi_is_crashed(file))
    {
      uint old_testflag=param.testflag;
      param.testflag|=T_MEDIUM;
      if (!(error= init_io_cache(&param.read_cache, file->dfile,
                                 my_default_record_cache_size, READ_CACHE,
                                 share->pack.header_length, 1, MYF(MY_WME))))
      {
        error= chk_data_link(&param, file, param.testflag & T_EXTEND);
        end_io_cache(&(param.read_cache));
      }
      param.testflag= old_testflag;
    }
  }
  if (!error)
  {
    if ((share->state.changed & (STATE_CHANGED |
				 STATE_CRASHED_ON_REPAIR |
				 STATE_CRASHED | STATE_NOT_ANALYZED)) ||
	(param.testflag & T_STATISTICS) ||
	mi_is_crashed(file))
    {
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      mysql_mutex_lock(&share->intern_lock);
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      if (!(table->db_stat & HA_READ_ONLY))
	error=update_state_info(&param,file,UPDATE_TIME | UPDATE_OPEN_COUNT |
				UPDATE_STAT);
      mysql_mutex_unlock(&share->intern_lock);
      info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	   HA_STATUS_CONST);
    }
  }
  else if (!mi_is_crashed(file) && !thd->killed)
  {
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

int ha_myisam::analyze(THD *thd, HA_CHECK_OPT* check_opt)
{
  int error=0;
  MI_CHECK param;
  MYISAM_SHARE* share = file->s;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name=    "analyze";
  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.testflag= (T_FAST | T_CHECK | T_SILENT | T_STATISTICS |
                   T_DONT_CHECK_CHECKSUM);
  param.using_global_keycache = 1;
  param.stats_method= (enum_mi_stats_method)THDVAR(thd, stats_method);

  if (!(share->state.changed & STATE_NOT_ANALYZED))
    return HA_ADMIN_ALREADY_DONE;

  error = chk_key(&param, file);
  if (!error)
  {
    mysql_mutex_lock(&share->intern_lock);
    error=update_state_info(&param,file,UPDATE_STAT);
    mysql_mutex_unlock(&share->intern_lock);
  }
  else if (!mi_is_crashed(file) && !thd->killed)
    mi_mark_crashed(file);
  return error ? HA_ADMIN_CORRUPT : HA_ADMIN_OK;
}


int ha_myisam::repair(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  MI_CHECK param;
  ha_rows start_records;

  if (!file) return HA_ADMIN_INTERNAL_ERROR;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name=  "repair";
  param.testflag= ((check_opt->flags & ~(T_EXTEND)) |
                   T_SILENT | T_FORCE_CREATE | T_CALC_CHECKSUM |
                   (check_opt->flags & T_EXTEND ? T_REP : T_REP_BY_SORT));
  param.sort_buffer_length=  THDVAR(thd, sort_buffer_size);
  start_records=file->state->records;
  while ((error=repair(thd,param,0)) && param.retry_repair)
  {
    param.retry_repair=0;
    if (test_all_bits(param.testflag,
		      (uint) (T_RETRY_WITHOUT_QUICK | T_QUICK)))
    {
      param.testflag&= ~T_RETRY_WITHOUT_QUICK;
      sql_print_information("Retrying repair of: '%s' without quick",
                            table->s->path.str);
      continue;
    }
    param.testflag&= ~T_QUICK;
    if ((param.testflag & T_REP_BY_SORT))
    {
      param.testflag= (param.testflag & ~T_REP_BY_SORT) | T_REP;
      sql_print_information("Retrying repair of: '%s' with keycache",
                            table->s->path.str);
      continue;
    }
    break;
  }
  if (!error && start_records != file->state->records &&
      !(check_opt->flags & T_VERY_SILENT))
  {
    char llbuff[22],llbuff2[22];
    sql_print_information("Found %s of %s rows when repairing '%s'",
                          llstr(file->state->records, llbuff),
                          llstr(start_records, llbuff2),
                          table->s->path.str);
  }
  return error;
}

int ha_myisam::optimize(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  if (!file) return HA_ADMIN_INTERNAL_ERROR;
  MI_CHECK param;

  myisamchk_init(&param);
  param.thd = thd;
  param.op_name= "optimize";
  param.testflag= (check_opt->flags | T_SILENT | T_FORCE_CREATE |
                   T_REP_BY_SORT | T_STATISTICS | T_SORT_INDEX);
  param.sort_buffer_length=  THDVAR(thd, sort_buffer_size);
  if ((error= repair(thd,param,1)) && param.retry_repair)
  {
    sql_print_warning("Warning: Optimize table got errno %d on %s.%s, retrying",
                      my_errno, param.db_name, param.table_name);
    param.testflag&= ~T_REP_BY_SORT;
    error= repair(thd,param,1);
  }
  return error;
}


int ha_myisam::repair(THD *thd, MI_CHECK &param, bool do_optimize)
{
  int error=0;
  uint local_testflag=param.testflag;
  bool optimize_done= !do_optimize, statistics_done=0;
  const char *old_proc_info=thd->proc_info;
  char fixed_name[FN_REFLEN];
  MYISAM_SHARE* share = file->s;
  ha_rows rows= file->state->records;
  DBUG_ENTER("ha_myisam::repair");

  param.db_name=    table->s->db.str;
  param.table_name= table->alias;
  param.tmpfile_createflag = O_RDWR | O_TRUNC;
  param.using_global_keycache = 1;
  param.thd= thd;
  param.tmpdir= &mysql_tmpdir_list;
  param.out_flag= 0;
  strmov(fixed_name,file->filename);

  // Release latches since this can take a long time
  ha_release_temporary_latches(thd);

  // Don't lock tables if we have used LOCK TABLE
  if (! thd->locked_tables_mode &&
      mi_lock_database(file, table->s->tmp_table ? F_EXTRA_LCK : F_WRLCK))
  {
    mi_check_print_error(&param,ER(ER_CANT_LOCK),my_errno);
    DBUG_RETURN(HA_ADMIN_FAILED);
  }

  if (!do_optimize ||
      ((file->state->del || share->state.split != file->state->records) &&
       (!(param.testflag & T_QUICK) ||
	!(share->state.changed & STATE_NOT_OPTIMIZED_KEYS))))
  {
    ulonglong key_map= ((local_testflag & T_CREATE_MISSING_KEYS) ?
			mi_get_mask_all_keys_active(share->base.keys) :
			share->state.key_map);
    uint testflag=param.testflag;
#ifdef HAVE_MMAP
    bool remap= test(share->file_map);
    /*
      mi_repair*() functions family use file I/O even if memory
      mapping is available.

      Since mixing mmap I/O and file I/O may cause various artifacts,
      memory mapping must be disabled.
    */
    if (remap)
      mi_munmap_file(file);
#endif
    if (mi_test_if_sort_rep(file,file->state->records,key_map,0) &&
	(local_testflag & T_REP_BY_SORT))
    {
      local_testflag|= T_STATISTICS;
      param.testflag|= T_STATISTICS;		// We get this for free
      statistics_done=1;
      if (THDVAR(thd, repair_threads)>1)
      {
        char buf[40];
        /* TODO: respect myisam_repair_threads variable */
        my_snprintf(buf, 40, "Repair with %d threads", my_count_bits(key_map));
        thd_proc_info(thd, buf);
        error = mi_repair_parallel(&param, file, fixed_name,
            param.testflag & T_QUICK);
        thd_proc_info(thd, "Repair done"); // to reset proc_info, as
                                      // it was pointing to local buffer
      }
      else
      {
        thd_proc_info(thd, "Repair by sorting");
        error = mi_repair_by_sort(&param, file, fixed_name,
            param.testflag & T_QUICK);
      }
    }
    else
    {
      thd_proc_info(thd, "Repair with keycache");
      param.testflag &= ~T_REP_BY_SORT;
      error=  mi_repair(&param, file, fixed_name,
			param.testflag & T_QUICK);
    }
#ifdef HAVE_MMAP
    if (remap)
      mi_dynmap_file(file, file->state->data_file_length);
#endif
    param.testflag=testflag;
    optimize_done=1;
  }
  if (!error)
  {
    if ((local_testflag & T_SORT_INDEX) &&
	(share->state.changed & STATE_NOT_SORTED_PAGES))
    {
      optimize_done=1;
      thd_proc_info(thd, "Sorting index");
      error=mi_sort_index(&param,file,fixed_name);
    }
    if (!statistics_done && (local_testflag & T_STATISTICS))
    {
      if (share->state.changed & STATE_NOT_ANALYZED)
      {
	optimize_done=1;
	thd_proc_info(thd, "Analyzing");
	error = chk_key(&param, file);
      }
      else
	local_testflag&= ~T_STATISTICS;		// Don't update statistics
    }
  }
  thd_proc_info(thd, "Saving state");
  if (!error)
  {
    if ((share->state.changed & STATE_CHANGED) || mi_is_crashed(file))
    {
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
			       STATE_CRASHED_ON_REPAIR);
      file->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
    /*
      the following 'if', thought conceptually wrong,
      is a useful optimization nevertheless.
    */
    if (file->state != &file->s->state.state)
      file->s->state.state = *file->state;
    if (file->s->base.auto_key)
      update_auto_increment_key(&param, file, 1);
    if (optimize_done)
      error = update_state_info(&param, file,
				UPDATE_TIME | UPDATE_OPEN_COUNT |
				(local_testflag &
				 T_STATISTICS ? UPDATE_STAT : 0));
    info(HA_STATUS_NO_LOCK | HA_STATUS_TIME | HA_STATUS_VARIABLE |
	 HA_STATUS_CONST);
    if (rows != file->state->records && ! (param.testflag & T_VERY_SILENT))
    {
      char llbuff[22],llbuff2[22];
      mi_check_print_warning(&param,"Number of rows changed from %s to %s",
			     llstr(rows,llbuff),
			     llstr(file->state->records,llbuff2));
    }
  }
  else
  {
    mi_mark_crashed_on_repair(file);
    file->update |= HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    update_state_info(&param, file, 0);
  }
  thd_proc_info(thd, old_proc_info);
  if (! thd->locked_tables_mode)
    mi_lock_database(file,F_UNLCK);
  DBUG_RETURN(error ? HA_ADMIN_FAILED :
	      !optimize_done ? HA_ADMIN_ALREADY_DONE : HA_ADMIN_OK);
}


/*
  Assign table indexes to a specific key cache.
*/

int ha_myisam::assign_to_keycache(THD* thd, HA_CHECK_OPT *check_opt)
{
  KEY_CACHE *new_key_cache= check_opt->key_cache;
  const char *errmsg= 0;
  int error= HA_ADMIN_OK;
  ulonglong map;
  TABLE_LIST *table_list= table->pos_in_table_list;
  DBUG_ENTER("ha_myisam::assign_to_keycache");

  table->keys_in_use_for_query.clear_all();

  if (table_list->process_index_hints(table))
    DBUG_RETURN(HA_ADMIN_FAILED);
  map= ~(ulonglong) 0;
  if (!table->keys_in_use_for_query.is_clear_all())
    /* use all keys if there's no list specified by the user through hints */
    map= table->keys_in_use_for_query.to_ulonglong();

  if ((error= mi_assign_to_key_cache(file, map, new_key_cache)))
  { 
    char buf[STRING_BUFFER_USUAL_SIZE];
    my_snprintf(buf, sizeof(buf),
		"Failed to flush to index file (errno: %d)", error);
    errmsg= buf;
    error= HA_ADMIN_CORRUPT;
  }

  if (error != HA_ADMIN_OK)
  {
    /* Send error to user */
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd= thd;
    param.op_name=    "assign_to_keycache";
    param.db_name=    table->s->db.str;
    param.table_name= table->s->table_name.str;
    param.testflag= 0;
    mi_check_print_error(&param, errmsg);
  }
  DBUG_RETURN(error);
}


/*
  Preload pages of the index file for a table into the key cache.
*/

int ha_myisam::preload_keys(THD* thd, HA_CHECK_OPT *check_opt)
{
  int error;
  const char *errmsg;
  ulonglong map;
  TABLE_LIST *table_list= table->pos_in_table_list;
  my_bool ignore_leaves= table_list->ignore_leaves;
  char buf[MYSQL_ERRMSG_SIZE];

  DBUG_ENTER("ha_myisam::preload_keys");

  table->keys_in_use_for_query.clear_all();

  if (table_list->process_index_hints(table))
    DBUG_RETURN(HA_ADMIN_FAILED);

  map= ~(ulonglong) 0;
  /* Check validity of the index references */
  if (!table->keys_in_use_for_query.is_clear_all())
    /* use all keys if there's no list specified by the user through hints */
    map= table->keys_in_use_for_query.to_ulonglong();

  mi_extra(file, HA_EXTRA_PRELOAD_BUFFER_SIZE,
           (void *) &thd->variables.preload_buff_size);

  if ((error= mi_preload(file, map, ignore_leaves)))
  {
    switch (error) {
    case HA_ERR_NON_UNIQUE_BLOCK_SIZE:
      errmsg= "Indexes use different block sizes";
      break;
    case HA_ERR_OUT_OF_MEM:
      errmsg= "Failed to allocate buffer";
      break;
    default:
      my_snprintf(buf, sizeof(buf),
                  "Failed to read from index file (errno: %d)", my_errno);
      errmsg= buf;
    }
    error= HA_ADMIN_FAILED;
    goto err;
  }

  DBUG_RETURN(HA_ADMIN_OK);

 err:
  {
    MI_CHECK param;
    myisamchk_init(&param);
    param.thd= thd;
    param.op_name=    "preload_keys";
    param.db_name=    table->s->db.str;
    param.table_name= table->s->table_name.str;
    param.testflag=   0;
    mi_check_print_error(&param, errmsg);
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

int ha_myisam::disable_indexes(uint mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    /* call a storage engine function to switch the key map */
    error= mi_disable_indexes(file);
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    mi_extra(file, HA_EXTRA_NO_KEYS, 0);
    info(HA_STATUS_CONST);                        // Read new key info
    error= 0;
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
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

int ha_myisam::enable_indexes(uint mode)
{
  int error;

  DBUG_EXECUTE_IF("wait_in_enable_indexes",
                  debug_wait_for_kill("wait_in_enable_indexes"); );

  if (mi_is_all_keys_active(file->s->state.key_map, file->s->base.keys))
  {
    /* All indexes are enabled already. */
    return 0;
  }

  if (mode == HA_KEY_SWITCH_ALL)
  {
    error= mi_enable_indexes(file);
    /*
       Do not try to repair on error,
       as this could make the enabled state persistent,
       but mode==HA_KEY_SWITCH_ALL forbids it.
    */
  }
  else if (mode == HA_KEY_SWITCH_NONUNIQ_SAVE)
  {
    THD *thd=current_thd;
    MI_CHECK param;
    const char *save_proc_info=thd->proc_info;
    thd_proc_info(thd, "Creating index");
    myisamchk_init(&param);
    param.op_name= "recreating_index";
    param.testflag= (T_SILENT | T_REP_BY_SORT | T_QUICK |
                     T_CREATE_MISSING_KEYS);
    param.myf_rw&= ~MY_WAIT_IF_FULL;
    param.sort_buffer_length=  THDVAR(thd, sort_buffer_size);
    param.stats_method= (enum_mi_stats_method)THDVAR(thd, stats_method);
    param.tmpdir=&mysql_tmpdir_list;
    if ((error= (repair(thd,param,0) != HA_ADMIN_OK)) && param.retry_repair)
    {
      sql_print_warning("Warning: Enabling keys got errno %d on %s.%s, retrying",
                        my_errno, param.db_name, param.table_name);
      /*
        Repairing by sort failed. Now try standard repair method.
        Still we want to fix only index file. If data file corruption
        was detected (T_RETRY_WITHOUT_QUICK), we shouldn't do much here.
        Let implicit repair do this job.
      */
      if (!(param.testflag & T_RETRY_WITHOUT_QUICK))
      {
        param.testflag&= ~T_REP_BY_SORT;
        error= (repair(thd,param,0) != HA_ADMIN_OK);
      }
      /*
        If the standard repair succeeded, clear all error messages which
        might have been set by the first repair. They can still be seen
        with SHOW WARNINGS then.
      */
      if (! error)
        thd->clear_error();
    }
    info(HA_STATUS_CONST);
    thd_proc_info(thd, save_proc_info);
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
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

int ha_myisam::indexes_are_disabled(void)
{
  
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

void ha_myisam::start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("ha_myisam::start_bulk_insert");
  THD *thd= current_thd;
  ulong size= min(thd->variables.read_buff_size,
                  (ulong) (table->s->avg_row_length*rows));
  DBUG_PRINT("info",("start_bulk_insert: rows %lu size %lu",
                     (ulong) rows, size));

  /* don't enable row cache if too few rows */
  if (! rows || (rows > MI_MIN_ROWS_TO_USE_WRITE_CACHE))
    mi_extra(file, HA_EXTRA_WRITE_CACHE, (void*) &size);

  can_enable_indexes= mi_is_all_keys_active(file->s->state.key_map,
                                            file->s->base.keys);

  if (!(specialflag & SPECIAL_SAFE_MODE))
  {
    /*
      Only disable old index if the table was empty and we are inserting
      a lot of rows.
      Note that in end_bulk_insert() we may truncate the table if
      enable_indexes() failed, thus it's essential that indexes are
      disabled ONLY for an empty table.
    */
    if (file->state->records == 0 && can_enable_indexes &&
        (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES))
      mi_disable_non_unique_index(file,rows);
    else
    if (!file->bulk_insert &&
        (!rows || rows >= MI_MIN_ROWS_TO_USE_BULK_INSERT))
    {
      mi_init_bulk_insert(file, thd->variables.bulk_insert_buff_size, rows);
    }
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

int ha_myisam::end_bulk_insert()
{
  mi_end_bulk_insert(file);
  int err=mi_extra(file, HA_EXTRA_NO_CACHE, 0);
  if (!err)
  {
    if (can_enable_indexes)
    {
      /* 
        Truncate the table when enable index operation is killed. 
        After truncating the table we don't need to enable the 
        indexes, because the last repair operation is aborted after 
        setting the indexes as active and  trying to recreate them. 
     */
   
      if (((err= enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE)) != 0) && 
                                                  current_thd->killed)
      {
        delete_all_rows();
        /* not crashed, despite being killed during repair */
        file->s->state.changed&= ~(STATE_CRASHED|STATE_CRASHED_ON_REPAIR);
      }
    } 
  }
  return err;
}


bool ha_myisam::check_and_repair(THD *thd)
{
  int error=0;
  int marked_crashed;
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_myisam::check_and_repair");

  check_opt.init();
  check_opt.flags= T_MEDIUM | T_AUTO_REPAIR;
  // Don't use quick if deleted rows
  if (!file->state->del && (myisam_recover_options & HA_RECOVER_QUICK))
    check_opt.flags|=T_QUICK;
  sql_print_warning("Checking table:   '%s'",table->s->path.str);

  const CSET_STRING query_backup= thd->query_string;
  thd->set_query(table->s->table_name.str,
                 (uint) table->s->table_name.length, system_charset_info);

  if ((marked_crashed= mi_is_crashed(file)) || check(thd, &check_opt))
  {
    sql_print_warning("Recovering table: '%s'",table->s->path.str);
    check_opt.flags=
      ((myisam_recover_options & HA_RECOVER_BACKUP ? T_BACKUP_DATA : 0) |
       (marked_crashed                             ? 0 : T_QUICK) |
       (myisam_recover_options & HA_RECOVER_FORCE  ? 0 : T_SAFE_REPAIR) |
       T_AUTO_REPAIR);
    if (repair(thd, &check_opt))
      error=1;
  }
  thd->set_query(query_backup);
  DBUG_RETURN(error);
}

bool ha_myisam::is_crashed() const
{
  return (file->s->state.changed & STATE_CRASHED ||
	  (my_disable_locking && file->s->state.open_count));
}

int ha_myisam::update_row(const uchar *old_data, uchar *new_data)
{
  ha_statistic_increment(&SSV::ha_update_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return mi_update(file,old_data,new_data);
}

int ha_myisam::delete_row(const uchar *buf)
{
  ha_statistic_increment(&SSV::ha_delete_count);
  return mi_delete(file,buf);
}

int ha_myisam::index_read_map(uchar *buf, const uchar *key,
                              key_part_map keypart_map,
                              enum ha_rkey_function find_flag)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                  key_part_map keypart_map,
                                  enum ha_rkey_function find_flag)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_read_last_map(uchar *buf, const uchar *key,
                                   key_part_map keypart_map)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ENTER("ha_myisam::index_read_last");
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=mi_rkey(file, buf, active_index, key, keypart_map,
                    HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  DBUG_RETURN(error);
}

int ha_myisam::index_next(uchar *buf)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_next_count);
  int error=mi_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_prev(uchar *buf)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_prev_count);
  int error=mi_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_first(uchar *buf)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_first_count);
  int error=mi_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_last(uchar *buf)
{
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  DBUG_ASSERT(inited==INDEX);
  ha_statistic_increment(&SSV::ha_read_last_count);
  int error=mi_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::index_next_same(uchar *buf,
			       const uchar *key __attribute__((unused)),
			       uint length __attribute__((unused)))
{
  int error;
  DBUG_ASSERT(inited==INDEX);
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  ha_statistic_increment(&SSV::ha_read_next_count);
  do
  {
    error= mi_rnext_same(file,buf);
  } while (error == HA_ERR_RECORD_DELETED);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_INDEX_READ_ROW_DONE(error);
  return error;
}


int ha_myisam::rnd_init(bool scan)
{
  if (scan)
    return mi_scan_init(file);
  return mi_reset(file);                        // Free buffers
}

int ha_myisam::rnd_next(uchar *buf)
{
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       TRUE);
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  int error=mi_scan(file, buf);
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_READ_ROW_DONE(error);
  return error;
}

int ha_myisam::restart_rnd_next(uchar *buf, uchar *pos)
{
  return rnd_pos(buf,pos);
}

int ha_myisam::rnd_pos(uchar *buf, uchar *pos)
{
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       FALSE);
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  int error=mi_rrnd(file, buf, my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  MYSQL_READ_ROW_DONE(error);
  return error;
}

void ha_myisam::position(const uchar *record)
{
  my_off_t row_position= mi_position(file);
  my_store_ptr(ref, ref_length, row_position);
}

int ha_myisam::info(uint flag)
{
  MI_ISAMINFO misam_info;
  char name_buff[FN_REFLEN];

  (void) mi_status(file,&misam_info,flag);
  if (flag & HA_STATUS_VARIABLE)
  {
    stats.records=           misam_info.records;
    stats.deleted=           misam_info.deleted;
    stats.data_file_length=  misam_info.data_file_length;
    stats.index_file_length= misam_info.index_file_length;
    stats.delete_length=     misam_info.delete_length;
    stats.check_time=        (ulong) misam_info.check_time;
    stats.mean_rec_length=   misam_info.mean_reclength;
  }
  if (flag & HA_STATUS_CONST)
  {
    TABLE_SHARE *share= table->s;
    stats.max_data_file_length=  misam_info.max_data_file_length;
    stats.max_index_file_length= misam_info.max_index_file_length;
    stats.create_time= (ulong) misam_info.create_time;
    ref_length= misam_info.reflength;
    share->db_options_in_use= misam_info.options;
    stats.block_size= myisam_block_size;        /* record block size */

    /* Update share */
    if (share->tmp_table == NO_TMP_TABLE)
      mysql_mutex_lock(&share->LOCK_ha_data);
    share->keys_in_use.set_prefix(share->keys);
    share->keys_in_use.intersect_extended(misam_info.key_map);
    share->keys_for_keyread.intersect(share->keys_in_use);
    share->db_record_offset= misam_info.record_offset;
    if (share->key_parts)
      memcpy((char*) table->key_info[0].rec_per_key,
	     (char*) misam_info.rec_per_key,
             sizeof(table->key_info[0].rec_per_key[0])*share->key_parts);
    if (share->tmp_table == NO_TMP_TABLE)
      mysql_mutex_unlock(&share->LOCK_ha_data);

   /*
     Set data_file_name and index_file_name to point at the symlink value
     if table is symlinked (Ie;  Real name is not same as generated name)
   */
    data_file_name= index_file_name= 0;
    fn_format(name_buff, file->filename, "", MI_NAME_DEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.data_file_name))
      data_file_name=misam_info.data_file_name;
    fn_format(name_buff, file->filename, "", MI_NAME_IEXT,
              MY_APPEND_EXT | MY_UNPACK_FILENAME);
    if (strcmp(name_buff, misam_info.index_file_name))
      index_file_name=misam_info.index_file_name;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey  = misam_info.errkey;
    my_store_ptr(dup_ref, ref_length, misam_info.dupp_key_pos);
  }
  if (flag & HA_STATUS_TIME)
    stats.update_time = (ulong) misam_info.update_time;
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= misam_info.auto_increment;

  return 0;
}


int ha_myisam::extra(enum ha_extra_function operation)
{
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_KEYREAD)
    return 0;
  if (operation == HA_EXTRA_MMAP && !opt_myisam_use_mmap)
    return 0;
  return mi_extra(file, operation, 0);
}

int ha_myisam::reset(void)
{
  return mi_reset(file);
}

/* To be used with WRITE_CACHE and EXTRA_CACHE */

int ha_myisam::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_WRITE_CACHE)
    return 0;
  return mi_extra(file, operation, (void*) &cache_size);
}

int ha_myisam::delete_all_rows()
{
  return mi_delete_all_rows(file);
}


/*
  Intended to support partitioning.
  Allows a particular partition to be truncated.
*/

int ha_myisam::truncate()
{
  int error= delete_all_rows();
  return error ? error : reset_auto_increment(0);
}

int ha_myisam::reset_auto_increment(ulonglong value)
{
  file->s->state.auto_increment= value;
  return 0;
}

int ha_myisam::delete_table(const char *name)
{
  return mi_delete_table(name);
}


int ha_myisam::external_lock(THD *thd, int lock_type)
{
  file->in_use.data= thd;
  return mi_lock_database(file, !table->s->tmp_table ?
			  lock_type : ((lock_type == F_UNLCK) ?
				       F_UNLCK : F_EXTRA_LCK));
}

THR_LOCK_DATA **ha_myisam::store_lock(THD *thd,
				      THR_LOCK_DATA **to,
				      enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type=lock_type;
  *to++= &file->lock;
  return to;
}

void ha_myisam::update_create_info(HA_CREATE_INFO *create_info)
{
  ha_myisam::info(HA_STATUS_AUTO | HA_STATUS_CONST);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  {
    create_info->auto_increment_value= stats.auto_increment_value;
  }
  create_info->data_file_name=data_file_name;
  create_info->index_file_name=index_file_name;
}


int ha_myisam::create(const char *name, register TABLE *table_arg,
		      HA_CREATE_INFO *ha_create_info)
{
  int error;
  uint create_flags= 0, records, i;
  char buff[FN_REFLEN];
  MI_KEYDEF *keydef;
  MI_COLUMNDEF *recinfo;
  MI_CREATE_INFO create_info;
  TABLE_SHARE *share= table_arg->s;
  uint options= share->db_options_in_use;
  DBUG_ENTER("ha_myisam::create");
  for (i= 0; i < share->keys; i++)
  {
    if (table_arg->key_info[i].flags & HA_USES_PARSER)
    {
      create_flags|= HA_CREATE_RELIES_ON_SQL_LAYER;
      break;
    }
  }
  if ((error= table2myisam(table_arg, &keydef, &recinfo, &records)))
    DBUG_RETURN(error); /* purecov: inspected */
  bzero((char*) &create_info, sizeof(create_info));
  create_info.max_rows= share->max_rows;
  create_info.reloc_rows= share->min_rows;
  create_info.with_auto_increment= share->next_number_key_offset == 0;
  create_info.auto_increment= (ha_create_info->auto_increment_value ?
                               ha_create_info->auto_increment_value -1 :
                               (ulonglong) 0);
  create_info.data_file_length= ((ulonglong) share->max_rows *
                                 share->avg_row_length);
  create_info.data_file_name= ha_create_info->data_file_name;
  create_info.index_file_name= ha_create_info->index_file_name;
  create_info.language= share->table_charset->number;

  if (ha_create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= HA_CREATE_TMP_TABLE;
  if (ha_create_info->options & HA_CREATE_KEEP_FILES)
    create_flags|= HA_CREATE_KEEP_FILES;
  if (options & HA_OPTION_PACK_RECORD)
    create_flags|= HA_PACK_RECORD;
  if (options & HA_OPTION_CHECKSUM)
    create_flags|= HA_CREATE_CHECKSUM;
  if (options & HA_OPTION_DELAY_KEY_WRITE)
    create_flags|= HA_CREATE_DELAY_KEY_WRITE;

  /* TODO: Check that the following fn_format is really needed */
  error= mi_create(fn_format(buff, name, "", "",
                             MY_UNPACK_FILENAME|MY_APPEND_EXT),
                   share->keys, keydef,
                   records, recinfo,
                   0, (MI_UNIQUEDEF*) 0,
                   &create_info, create_flags);
  my_free(recinfo);
  DBUG_RETURN(error);
}


int ha_myisam::rename_table(const char * from, const char * to)
{
  return mi_rename(from,to);
}


void ha_myisam::get_auto_increment(ulonglong offset, ulonglong increment,
                                   ulonglong nb_desired_values,
                                   ulonglong *first_value,
                                   ulonglong *nb_reserved_values)
{
  ulonglong nr;
  int error;
  uchar key[MI_MAX_KEY_LENGTH];

  if (!table->s->next_number_key_offset)
  {						// Autoincrement at key-start
    ha_myisam::info(HA_STATUS_AUTO);
    *first_value= stats.auto_increment_value;
    /* MyISAM has only table-level lock, so reserves to +inf */
    *nb_reserved_values= ULONGLONG_MAX;
    return;
  }

  /* it's safe to call the following if bulk_insert isn't on */
  mi_flush_bulk_insert(file, table->s->next_number_index);

  (void) extra(HA_EXTRA_KEYREAD);
  key_copy(key, table->record[0],
           table->key_info + table->s->next_number_index,
           table->s->next_number_key_offset);
  error= mi_rkey(file, table->record[1], (int) table->s->next_number_index,
                 key, make_prev_keypart_map(table->s->next_number_keypart),
                 HA_READ_PREFIX_LAST);
  if (error)
    nr= 1;
  else
  {
    /* Get data from record[1] */
    nr= ((ulonglong) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
  }
  extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
  /*
    MySQL needs to call us for next row: assume we are inserting ("a",null)
    here, we return 3, and next this statement will want to insert ("b",null):
    there is no reason why ("b",3+1) would be the good row to insert: maybe it
    already exists, maybe 3+1 is too large...
  */
  *nb_reserved_values= 1;
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
                                    key_range *max_key)
{
  return (ha_rows) mi_records_in_range(file, (int) inx, min_key, max_key);
}


int ha_myisam::ft_read(uchar *buf)
{
  int error;

  if (!ft_handler)
    return -1;

  thread_safe_increment(table->in_use->status_var.ha_read_next_count,
			&LOCK_status); // why ?

  error=ft_handler->please->read_next(ft_handler,(char*) buf);

  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

uint ha_myisam::checksum() const
{
  return (uint)file->state->checksum;
}


bool ha_myisam::check_if_incompatible_data(HA_CREATE_INFO *info,
					   uint table_changes)
{
  uint options= table->s->db_options_in_use;

  if (info->auto_increment_value != stats.auto_increment_value ||
      info->data_file_name != data_file_name ||
      info->index_file_name != index_file_name ||
      table_changes == IS_EQUAL_NO ||
      table_changes & IS_EQUAL_PACK_LENGTH) // Not implemented yet
    return COMPATIBLE_DATA_NO;

  if ((options & (HA_OPTION_PACK_RECORD | HA_OPTION_CHECKSUM |
		  HA_OPTION_DELAY_KEY_WRITE)) !=
      (info->table_options & (HA_OPTION_PACK_RECORD | HA_OPTION_CHECKSUM |
			      HA_OPTION_DELAY_KEY_WRITE)))
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

extern int mi_panic(enum ha_panic_function flag);
int myisam_panic(handlerton *hton, ha_panic_function flag)
{
  return mi_panic(flag);
}

static int myisam_init(void *p)
{
  handlerton *myisam_hton;

#ifdef HAVE_PSI_INTERFACE
  init_myisam_psi_keys();
#endif

  /* Set global variables based on startup options */
  if (myisam_recover_options)
    ha_open_options|=HA_OPEN_ABORT_IF_CRASHED;
  else
    myisam_recover_options= HA_RECOVER_OFF;

  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);

  myisam_hton= (handlerton *)p;
  myisam_hton->state= SHOW_OPTION_YES;
  myisam_hton->db_type= DB_TYPE_MYISAM;
  myisam_hton->create= myisam_create_handler;
  myisam_hton->panic= myisam_panic;
  myisam_hton->flags= HTON_CAN_RECREATE | HTON_SUPPORT_LOG_TABLES;
  myisam_hton->is_supported_system_table= myisam_is_supported_system_table;

  return 0;
}

static struct st_mysql_sys_var* myisam_sysvars[]= {
  MYSQL_SYSVAR(block_size),
  MYSQL_SYSVAR(data_pointer_size),
  MYSQL_SYSVAR(max_sort_file_size),
  MYSQL_SYSVAR(recover_options),
  MYSQL_SYSVAR(repair_threads),
  MYSQL_SYSVAR(sort_buffer_size),
  MYSQL_SYSVAR(use_mmap),
  MYSQL_SYSVAR(mmap_size),
  MYSQL_SYSVAR(stats_method),
  0
};

struct st_mysql_storage_engine myisam_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(myisam)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &myisam_storage_engine,
  "MyISAM",
  "MySQL AB",
  "MyISAM storage engine",
  PLUGIN_LICENSE_GPL,
  myisam_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100, /* 1.0 */
  NULL,                       /* status variables                */
  myisam_sysvars,             /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;


#ifdef HAVE_QUERY_CACHE
/**
  @brief Register a named table with a call back function to the query cache.

  @param thd The thread handle
  @param table_key A pointer to the table name in the table cache
  @param key_length The length of the table name
  @param[out] engine_callback The pointer to the storage engine call back
    function, currently 0
  @param[out] engine_data Engine data will be set to 0.

  @note Despite the name of this function, it is used to check each statement
    before it is cached and not to register a table or callback function.

  @see handler::register_query_cache_table

  @return The error code. The engine_data and engine_callback will be set to 0.
    @retval TRUE Success
    @retval FALSE An error occured
*/

my_bool ha_myisam::register_query_cache_table(THD *thd, char *table_name,
                                              uint table_name_len,
                                              qc_engine_callback
                                              *engine_callback,
                                              ulonglong *engine_data)
{
  DBUG_ENTER("ha_myisam::register_query_cache_table");
  /*
    No call back function is needed to determine if a cached statement
    is valid or not.
  */
  *engine_callback= 0;

  /*
    No engine data is needed.
  */
  *engine_data= 0;

  if (file->s->concurrent_insert)
  {
    /*
      If a concurrent INSERT has happened just before the currently
      processed SELECT statement, the total size of the table is
      unknown.

      To determine if the table size is known, the current thread's snap
      shot of the table size with the actual table size are compared.

      If the table size is unknown the SELECT statement can't be cached.

      When concurrent inserts are disabled at table open, mi_open()
      does not assign a get_status() function. In this case the local
      ("current") status is never updated. We would wrongly think that
      we cannot cache the statement.
    */
    ulonglong actual_data_file_length;
    ulonglong current_data_file_length;

    /*
      POSIX visibility rules specify that "2. Whatever memory values a
      thread can see when it unlocks a mutex <...> can also be seen by any
      thread that later locks the same mutex". In this particular case,
      concurrent insert thread had modified the data_file_length in
      MYISAM_SHARE before it has unlocked (or even locked)
      structure_guard_mutex. So, here we're guaranteed to see at least that
      value after we've locked the same mutex. We can see a later value
      (modified by some other thread) though, but it's ok, as we only want
      to know if the variable was changed, the actual new value doesn't matter
    */
    actual_data_file_length= file->s->state.state.data_file_length;
    current_data_file_length= file->save_state.data_file_length;

    if (current_data_file_length != actual_data_file_length)
    {
      /* Don't cache current statement. */
      DBUG_RETURN(FALSE);
    }
  }

  /*
    This query execution might have started after the query cache was flushed
    by a concurrent INSERT. In this case, don't cache this statement as the
    data file length difference might not be visible yet if the tables haven't
    been unlocked by the concurrent insert thread.
  */
  if (file->state->uncacheable)
    DBUG_RETURN(FALSE);

  /* It is ok to try to cache current statement. */
  DBUG_RETURN(TRUE);
}
#endif
