/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef HA_NDBINFO_H
#define HA_NDBINFO_H

#include <mysql/plugin.h>

int ndbinfo_init(void *plugin);
int ndbinfo_deinit(void *plugin);

class ha_ndbinfo: public handler
{
public:
  ha_ndbinfo(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_ndbinfo();

  const char *table_type() const { return "NDBINFO"; }
  const char **bas_ext() const {
    static const char *null[] = { NullS };
    return null;
  }
  ulonglong table_flags() const {
    return HA_REC_NOT_IN_SEQ | HA_NO_TRANSACTIONS |
           HA_NO_BLOBS | HA_NO_AUTO_INCREMENT;
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const {
    return 0;
  }

  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);
  void position(const uchar *record);
  int info(uint);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) {
    return to;
  }

  bool low_byte_first() const {
    // Data will be returned in machine format
#ifdef WORDS_BIGENDIAN
    return false;
#else
    return true;
#endif
  }

  bool get_error_message(int error, String *buf);

  uint8 table_cache_type() {
    // Don't put ndbinfo results in query cache
    return HA_CACHE_TBL_NOCACHE;
  }

private:
  void unpack_record(uchar *dst_row);

  bool is_open(void) const;
  bool is_closed(void) const { return ! is_open(); };

  bool is_offline(void) const;

  struct ha_ndbinfo_impl& m_impl;

};

#endif
