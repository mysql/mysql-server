/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_INFO_DUMMY_H
#define RPL_INFO_DUMMY_H

#include <stddef.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "sql/rpl_info_handler.h"  // Rpl_info_handler

class Server_ids;

/**
  Defines a dummy handler that should only be internally accessed.
  This class is useful for debugging and performance tests.

  The flag abort indicates if the execution should abort if some
  methods are called. See the code for further details.
*/
class Rpl_info_dummy : public Rpl_info_handler {
 public:
  Rpl_info_dummy(const int nparam);
  ~Rpl_info_dummy() override = default;

 private:
  int do_init_info() override;
  int do_init_info(uint instance) override;
  enum_return_check do_check_info() override;
  enum_return_check do_check_info(uint instance) override;
  void do_end_info() override;
  int do_flush_info(const bool force) override;
  int do_remove_info() override;
  int do_clean_info() override;

  int do_prepare_info_for_read() override;
  int do_prepare_info_for_write() override;
  bool do_set_info(const int pos, const char *value) override;
  bool do_set_info(const int pos, const uchar *value,
                   const size_t size) override;
  bool do_set_info(const int pos, const int value) override;
  bool do_set_info(const int pos, const ulong value) override;
  bool do_set_info(const int pos, const float value) override;
  bool do_set_info(const int pos, const Server_ids *value) override;
  bool do_set_info(const int pos, const std::nullptr_t value) override;
  bool do_set_info(const int pos, const std::nullptr_t value,
                   const size_t size) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, char *value, const size_t size,
      const char *default_value) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, uchar *value, const size_t size,
      const uchar *default_value) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, int *value, const int default_value) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, ulong *value, const ulong default_value) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, float *value, const float default_value) override;
  Rpl_info_handler::enum_field_get_status do_get_info(
      const int pos, Server_ids *value,
      const Server_ids *default_value) override;
  char *do_get_description_info() override;
  bool do_is_transactional() override;
  bool do_update_is_transactional() override;
  uint do_get_rpl_info_type() override;

  static const bool abort = false;

  Rpl_info_dummy &operator=(const Rpl_info_dummy &info);
  Rpl_info_dummy(const Rpl_info_dummy &info);
};
#endif /* RPL_INFO_DUMMY_H */
