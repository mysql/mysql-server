/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/* class for the the myisam merge handler */

#include <myisammrg.h>

/** 
  Represents one name of a MERGE child.

  @todo: Add MYRG_SHARE and store chlidren names in the
  share.
*/

class Mrg_child_def: public Sql_alloc
{
  /* Remembered MERGE child def version.  See top comment in ha_myisammrg.cc */
  enum_table_ref_type m_child_table_ref_type;
  ulong m_child_def_version;
public:
  LEX_STRING db;
  LEX_STRING name;

  /* Access MERGE child def version.  See top comment in ha_myisammrg.cc */
  inline enum_table_ref_type get_child_table_ref_type()
  {
    return m_child_table_ref_type;
  }
  inline ulong get_child_def_version()
  {
    return m_child_def_version;
  }
  inline void set_child_def_version(enum_table_ref_type child_table_ref_type,
                                    ulong version)
  {
    m_child_table_ref_type= child_table_ref_type;
    m_child_def_version= version;
  }

  Mrg_child_def(char *db_arg, size_t db_len_arg,
                char *table_name_arg, size_t table_name_len_arg)
  {
    db.str= db_arg;
    db.length= db_len_arg;
    name.str= table_name_arg;
    name.length= table_name_len_arg;
    m_child_def_version= ~0UL;
    m_child_table_ref_type= TABLE_REF_NULL;
  }
};


class ha_myisammrg: public handler
{
  MYRG_INFO *file;
  my_bool is_cloned;                    /* This instance has been cloned */

public:
  MEM_ROOT      children_mem_root;      /* mem root for children list */
  List<Mrg_child_def> child_def_list;
  TABLE_LIST    *children_l;            /* children list */
  TABLE_LIST    **children_last_l;      /* children list end */
  uint          test_if_locked;         /* flags from ::open() */

  ha_myisammrg(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_myisammrg();
  const char *table_type() const { return "MRG_MyISAM"; }
  const char **bas_ext() const;
  const char *index_type(uint key_number);
  ulonglong table_flags() const
  {
    return (HA_REC_NOT_IN_SEQ | HA_AUTO_PART_KEY | HA_NO_TRANSACTIONS |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
	    HA_NULL_IN_KEY | HA_CAN_INDEX_BLOBS | HA_FILE_BASED |
            HA_ANY_INDEX_MAY_BE_UNIQUE | HA_CAN_BIT_FIELD |
            HA_HAS_RECORDS |
            HA_NO_COPY_ON_ALTER |
            HA_DUPLICATE_POS);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
            0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
            HA_READ_ORDER | HA_KEYREAD_ONLY);
  }
  uint max_supported_keys()          const { return MI_MAX_KEY; }
  uint max_supported_key_length()    const { return MI_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length() const { return MI_MAX_KEY_LENGTH; }
  double scan_time()
  { return ulonglong2double(stats.data_file_length) / IO_SIZE + file->tables; }

  int open(const char *name, int mode, uint test_if_locked);
  int add_children_list(void);
  int attach_children(void);
  int detach_children(void);
  virtual handler *clone(const char *name, MEM_ROOT *mem_root);
  int close(void);
  int write_row(uchar * buf);
  int update_row(const uchar * old_data, uchar * new_data);
  int delete_row(const uchar * buf);
  int index_read_map(uchar *buf, const uchar *key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(uchar *buf, uint index, const uchar *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(uchar *buf, const uchar *key, key_part_map keypart_map);
  int index_next(uchar * buf);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  int index_next_same(uchar *buf, const uchar *key, uint keylen);
  int rnd_init(bool scan);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar * buf, uchar *pos);
  void position(const uchar *record);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int truncate();
  int info(uint);
  int reset(void);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int external_lock(THD *thd, int lock_type);
  uint lock_count(void) const;
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  void update_create_info(HA_CREATE_INFO *create_info);
  void append_create_info(String *packet);
  MYRG_INFO *myrg_info() { return file; }
  TABLE *table_ptr() { return table; }
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);
  int check(THD* thd, HA_CHECK_OPT* check_opt);
  ha_rows records();
};
