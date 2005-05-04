/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

/* class for the the myisam merge handler */

#include <merge.h>

class ha_isammrg: public handler
{
  MRG_INFO *file;

 public:
  ha_isammrg(TABLE *table): handler(table), file(0) {}
  ~ha_isammrg() {}
  const char *table_type() const { return "MRG_ISAM"; }
  const char **bas_ext() const;
  ulong table_flags() const { return (HA_READ_RND_SAME |
				      HA_REC_NOT_IN_SEQ | HA_FILE_BASED); }
  ulong index_flags(uint idx, uint part, bool all_parts) const
  { DBUG_ASSERT(0); return 0; }

  uint max_supported_keys()          const { return 0; }
  bool low_byte_first()	   const { return 0; }
  uint min_record_length(uint options) const;

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int write_row(byte * buf);
  int update_row(const byte * old_data, byte * new_data);
  int delete_row(const byte * buf);
  int index_read(byte * buf, const byte * key,
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(byte * buf, uint indx, const byte * key,
		     uint key_len, enum ha_rkey_function find_flag);
  int index_next(byte * buf);
  int index_prev(byte * buf);
  int index_first(byte * buf);
  int index_last(byte * buf);
  int rnd_init(bool scan);
  int rnd_next(byte *buf);
  int rnd_pos(byte * buf, byte *pos);
  void position(const byte *record);
  void info(uint);
  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);
  uint lock_count(void) const;
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }
};
