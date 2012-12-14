/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_INFO_DUMMY_H
#define RPL_INFO_DUMMY_H

#include <my_global.h>
#include <sql_priv.h>
#include "rpl_info_handler.h"

/**
  Defines a dummy handler that should only be internally accessed.
  This class is useful for debugging and performance tests.

  The flag abort indicates if the execution should abort if some
  methods are called. See the code for further details.
*/
class Rpl_info_dummy : public Rpl_info_handler
{
public:
  Rpl_info_dummy(const int nparam);
  virtual ~Rpl_info_dummy() { };

private:
  int do_init_info();
  int do_init_info(uint instance);
  enum_return_check do_check_info();
  enum_return_check do_check_info(uint instance,
                                  const bool ignore_error);
  void do_end_info();
  int do_flush_info(const bool force);
  int do_remove_info();
  int do_clean_info();
  static int do_reset_info(const int nparam);

  int do_prepare_info_for_read();
  int do_prepare_info_for_write();
  bool do_set_info(const int pos, const char *value);
  bool do_set_info(const int pos, const uchar *value,
                   const size_t size);
  bool do_set_info(const int pos, const int value);
  bool do_set_info(const int pos, const ulong value);
  bool do_set_info(const int pos, const float value);
  bool do_set_info(const int pos, const Dynamic_ids *value);
  bool do_get_info(const int pos, char *value, const size_t size,
                   const char *default_value);
  bool do_get_info(const int pos, uchar *value, const size_t size,
                   const uchar *default_value);
  bool do_get_info(const int pos, int *value,
                   const int default_value);
  bool do_get_info(const int pos, ulong *value,
                   const ulong default_value);
  bool do_get_info(const int pos, float *value,
                   const float default_value);
  bool do_get_info(const int pos, Dynamic_ids *value,
                   const Dynamic_ids *default_value);
  char* do_get_description_info();
  bool do_is_transactional();
  bool do_update_is_transactional();
  uint do_get_rpl_info_type();

  static const bool abort= FALSE;

  Rpl_info_dummy& operator=(const Rpl_info_dummy& info);
  Rpl_info_dummy(const Rpl_info_dummy& info);
};
#endif /* RPL_INFO_DUMMY_H */
