/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HA_NDBINFO_H
#define HA_NDBINFO_H

#include "sql/handler.h"

class ha_ndbinfo: public handler
{
public:
  ha_ndbinfo(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_ndbinfo();

  const char *table_type() const { return "NDBINFO"; }
  ulonglong table_flags() const {
    return HA_REC_NOT_IN_SEQ | HA_NO_TRANSACTIONS |
           HA_NO_BLOBS | HA_NO_AUTO_INCREMENT;
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const {
    return 0;
  }

  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info,
             dd::Table *table_def);

  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def);
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

  virtual ha_rows estimate_rows_upper_bound() {
    // Estimate "many" rows to be returned so that filesort
    // allocates buffers properly.
    // Default impl. for this function is otherwise 10 rows
    // in case handler hasn't filled in stats.records
    return HA_POS_ERROR;
  }

private:
  void unpack_record(uchar *dst_row);

  bool is_open(void) const;
  bool is_closed(void) const { return ! is_open(); };

  bool is_offline(void) const;

  struct ha_ndbinfo_impl& m_impl;

};

#endif
