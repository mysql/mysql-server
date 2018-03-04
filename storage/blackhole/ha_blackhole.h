/* Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>

#include "my_inttypes.h"
#include "sql/handler.h"                        /* handler */
#include "sql/table.h"                          /* TABLE_SHARE */
#include "thr_lock.h"                           /* THR_LOCK */

/*
  Shared structure for correct LOCK operation
*/
struct st_blackhole_share {
  THR_LOCK lock;
  uint use_count;
  uint table_name_length;
  char table_name[1];
};


/*
  Class definition for the blackhole storage engine
  "Dumbest named feature ever"
*/
class ha_blackhole: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  st_blackhole_share *share;

public:
  ha_blackhole(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_blackhole()
  {
  }
  /* The name that will be used for display purposes */
  const char *table_type() const { return "BLACKHOLE"; }
  virtual enum ha_key_alg get_default_index_algorithm() const
  { return HA_KEY_ALG_BTREE; }
  virtual bool is_index_algorithm_supported(enum ha_key_alg key_alg) const
  { return key_alg == HA_KEY_ALG_BTREE || key_alg == HA_KEY_ALG_RTREE; }
  ulonglong table_flags() const
  {
    return(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
           HA_BINLOG_STMT_CAPABLE | HA_BINLOG_ROW_CAPABLE |
           HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY | HA_READ_OUT_OF_SYNC |
           HA_FILE_BASED | HA_CAN_GEOMETRY);
  }
  ulong index_flags(uint inx, uint, bool) const
  {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
            0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
            HA_READ_ORDER | HA_KEYREAD_ONLY);
  }
  /* The following defines can be increased if necessary */
#define BLACKHOLE_MAX_KEY	64		/* Max allowed keys */
#define BLACKHOLE_MAX_KEY_SEG	16		/* Max segments for key */
#define BLACKHOLE_MAX_KEY_LENGTH 1000
  uint max_supported_keys()          const { return BLACKHOLE_MAX_KEY; }
  uint max_supported_key_length()    const { return BLACKHOLE_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length() const { return BLACKHOLE_MAX_KEY_LENGTH; }
  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def);
  int close(void);
  int rnd_init(bool scan);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar * buf, uchar *pos);
  int index_read_map(uchar * buf, const uchar * key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(uchar * buf, uint idx, const uchar * key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(uchar * buf, const uchar * key, key_part_map keypart_map);
  int index_next(uchar * buf);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  void position(const uchar *record);
  int info(uint flag);
  int external_lock(THD *thd, int lock_type);
  int create(const char *name, TABLE *table_arg,
             HA_CREATE_INFO *create_info,
             dd::Table *table_def);
  THR_LOCK_DATA **store_lock(THD *thd,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
private:
  virtual int write_row(uchar *buf);
  virtual int update_row(const uchar *old_data, uchar *new_data);
  virtual int delete_row(const uchar *buf);
};
