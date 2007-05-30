/* Copyright (C) 2005 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

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
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  const char *index_type(uint key_number);
  const char **bas_ext() const;
  ulonglong table_flags() const
  {
    return(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
           HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY |
           HA_FILE_BASED | HA_CAN_GEOMETRY | HA_CAN_INSERT_DELAYED);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
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
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int rnd_init(bool scan);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  int index_read(byte * buf, const byte * key, key_part_map keypart_map,
                 enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint idx, const byte * key,
                   key_part_map keypart_map, enum ha_rkey_function find_flag);
  int index_read_last(byte * buf, const byte * key, key_part_map keypart_map);
  int index_next(byte * buf);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  void position(const byte *record);
  int info(uint flag);
  int external_lock(THD *thd, int lock_type);
  int create(const char *name, TABLE *table_arg,
             HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
};
