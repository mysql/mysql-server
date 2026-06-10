/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_LOCK_FREE_HASH_IMP_H
#define MYSQL_LOCK_FREE_HASH_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_lock_free_hash.h>

/**
  An implementation of "mysql_lock_free_hash" component service using the
  server's lock free hash implementation.
*/
class mysql_component_mysql_lock_free_hash_imp {
 public:
  /**
    Wrapper around lf_hash_init2 function.

    @see lf_hash_init2
  */
  static DEFINE_METHOD(LF_HASH_h, init,
                       (uint element_size, uint flags, uint key_offset,
                        uint key_length, hash_get_key_function get_key,
                        CHARSET_INFO_h charset, lf_allocator_func *ctor,
                        lf_allocator_func *dtor, lf_hash_init_func *init));

  /**
    Wrapper around lf_hash_destroy function.

    @see lf_hash_destroy
  */
  static DEFINE_BOOL_METHOD(destroy, (LF_HASH_h hash));

  /**
    Wrapper around lf_hash_get_pins function.

    @see lf_hash_get_pins
  */
  static DEFINE_METHOD(LF_PINS_h, get_pins, (LF_HASH_h hash));

  /**
    Wrapper around lf_hash_search function.

    @see lf_hash_search
  */
  static DEFINE_METHOD(void *, search,
                       (LF_HASH_h hash, LF_PINS_h pins, const void *data,
                        ulong length));

  /**
    Wrapper around lf_hash_delete function.

    @see lf_hash_delete
  */
  static DEFINE_METHOD(int, remove,
                       (LF_HASH_h hash, LF_PINS_h pins, const void *data,
                        uint length));

  /**
    Wrapper around lf_hash_random_match function.

    @see lf_hash_random_match
  */
  static DEFINE_METHOD(void *, random_match,
                       (LF_HASH_h hash, LF_PINS_h pins,
                        lf_hash_match_func *match, uint rand_val,
                        void *match_arg));

  /**
    Wrapper around lf_hash_search_unpin function.

    @see lf_hash_search_unpin
  */
  static DEFINE_METHOD(void, search_unpin, (LF_PINS_h pins));

  /**
    Wrapper around lf_hash_put_pins function.

    @see lf_hash_put_pins
  */
  static DEFINE_METHOD(void, put_pins, (LF_PINS_h pins));

  /**
    Wrapper around lf_hash_insert function.

    @see lf_hash_insert
  */
  static DEFINE_METHOD(int, insert,
                       (LF_HASH_h hash, LF_PINS_h pins, const void *data));

  /**
    LF_HASH_OVERHEAD value.

    @see LF_HASH_OVERHEAD
  */
  static DEFINE_METHOD(int, overhead, ());
};

#endif /* MYSQL_LOCK_FREE_HASH_IMP_H */
