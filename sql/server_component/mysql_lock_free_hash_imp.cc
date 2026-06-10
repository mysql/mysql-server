/* Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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

#include "sql/server_component/mysql_lock_free_hash_imp.h"

#include <lf.h>
#include <mysql/components/service_implementation.h>

DEFINE_METHOD(LF_HASH_h, mysql_component_mysql_lock_free_hash_imp::init,
              (uint element_size, uint flags, uint key_offset, uint key_length,
               hash_get_key_function get_key, CHARSET_INFO_h charset,
               lf_allocator_func *ctor, lf_allocator_func *dtor,
               lf_hash_init_func *init)) {
  CHARSET_INFO *ch = reinterpret_cast<CHARSET_INFO *>(charset);

  LF_HASH *h = new (std::nothrow) LF_HASH;

  if (h)
    lf_hash_init2(h, element_size, flags, key_offset, key_length, get_key, ch,
                  nullptr, ctor, dtor, init);

  return (LF_HASH_h)h;
}

DEFINE_BOOL_METHOD(mysql_component_mysql_lock_free_hash_imp::destroy,
                   (LF_HASH_h hash)) {
  LF_HASH *h = reinterpret_cast<LF_HASH *>(hash);

  lf_hash_destroy(h);

  delete h;

  return 0;
}

DEFINE_METHOD(LF_PINS_h, mysql_component_mysql_lock_free_hash_imp::get_pins,
              (LF_HASH_h hash)) {
  return (LF_PINS_h)lf_hash_get_pins(reinterpret_cast<LF_HASH *>(hash));
}

DEFINE_METHOD(void *, mysql_component_mysql_lock_free_hash_imp::search,
              (LF_HASH_h hash, LF_PINS_h pins, const void *data,
               ulong length)) {
  return lf_hash_search(reinterpret_cast<LF_HASH *>(hash), (LF_PINS *)pins,
                        data, length);
}

DEFINE_METHOD(int, mysql_component_mysql_lock_free_hash_imp::remove,
              (LF_HASH_h hash, LF_PINS_h pins, const void *data, uint length)) {
  return lf_hash_delete(reinterpret_cast<LF_HASH *>(hash), (LF_PINS *)pins,
                        data, length);
}

DEFINE_METHOD(void *, mysql_component_mysql_lock_free_hash_imp::random_match,
              (LF_HASH_h hash, LF_PINS_h pins, lf_hash_match_func *match,
               uint rand_val, void *match_arg)) {
  return lf_hash_random_match(reinterpret_cast<LF_HASH *>(hash),
                              reinterpret_cast<LF_PINS *>(pins), match,
                              rand_val, match_arg);
}

DEFINE_METHOD(void, mysql_component_mysql_lock_free_hash_imp::search_unpin,
              (LF_PINS_h pins)) {
  lf_hash_search_unpin(reinterpret_cast<LF_PINS *>(pins));
}

DEFINE_METHOD(void, mysql_component_mysql_lock_free_hash_imp::put_pins,
              (LF_PINS_h pins)) {
  lf_hash_put_pins(reinterpret_cast<LF_PINS *>(pins));
}

DEFINE_METHOD(int, mysql_component_mysql_lock_free_hash_imp::insert,
              (LF_HASH_h hash, LF_PINS_h pins, const void *data)) {
  return lf_hash_insert(reinterpret_cast<LF_HASH *>(hash),
                        reinterpret_cast<LF_PINS *>(pins), data);
}

DEFINE_METHOD(int, mysql_component_mysql_lock_free_hash_imp::overhead, ()) {
  return LF_HASH_OVERHEAD;
}
