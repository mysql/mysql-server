/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_LOCK_FREE_HASH_H
#define MYSQL_LOCK_FREE_HASH_H

#include <lf_types.h>
#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

/**
  Hash colleciton handle.
*/
DEFINE_SERVICE_HANDLE(LF_HASH_h);

/**
  Hash collection access handle.
*/
DEFINE_SERVICE_HANDLE(LF_PINS_h);

/**
  @ingroup group_components_services_inventory

  Lock free hashing collection.
*/
BEGIN_SERVICE_DEFINITION(mysql_lock_free_hash)

/**
  Initialize lock free hash.

  @return Initialized hash handle.
*/
DECLARE_METHOD(LF_HASH_h, init,
               (uint element_size, uint flags, uint key_offset, uint key_length,
                hash_get_key_function get_key, CHARSET_INFO_h charset,
                lf_allocator_func *ctor, lf_allocator_func *dtor,
                lf_hash_init_func *init));
/**
  Destroy lock free hash.

  @param hash Hash handle.
*/
DECLARE_BOOL_METHOD(destroy, (LF_HASH_h hash));

/**
  Get pins for search.

  @param hash Hash handle.

  @return Search pins handle.
*/
DECLARE_METHOD(LF_PINS_h, get_pins, (LF_HASH_h hash));

/**
  Find hash element corresponding to the key.

  @param hash    The hash to search element in.
  @param pins    Pins for the calling thread which were earlier
                 obtained from this hash using lf_hash_get_pins().
  @param dat     Key
  @param length  Key length

  @retval A pointer to an element with the given key (if a hash is not unique
          and there're many elements with this key - the "first" matching
          element).
  @retval NULL         - if nothing is found
  @retval MY_LF_ERRPTR - if OOM

  @note Uses pins[0..2]. On return pins[0..1] are removed and pins[2]
        is used to pin object found. It is also not removed in case when
        object is not found/error occurs but pin value is undefined in
        this case.
        So calling search_unpin() is mandatory after call to this function
        in case of both success and failure.
*/
DECLARE_METHOD(void *, search,
               (LF_HASH_h hash, LF_PINS_h pins, const void *data,
                ulong length));

/**
  Remove data from hash.

  @param hash   Hash handle.
  @param pins   Search pins.
  @param data   Search data.
  @param length Data length.

  @return Zero value on success.
*/
DECLARE_METHOD(int, remove,
               (LF_HASH_h hash, LF_PINS_h pins, const void *data, uint length));

/**
  Find random hash element which satisfies condition specified by
  match function.

  @param hash      Hash to search element in.
  @param pins      Pins for calling thread to be used during search
                   and for pinning its result.
  @param match     Pointer to match function. This function takes
                   pointer to object stored in hash as parameter
                   and returns 0 if object doesn't satisfy its
                   condition (and non-0 value if it does).
  @param rand_val  Random value to be used for selecting hash
                   bucket from which search in sort-ordered
                   list needs to be started.
  @param match_arg Argument passed to match function.

  @retval A pointer to a random element matching condition.
  @retval NULL         - if nothing is found
  @retval MY_LF_ERRPTR - OOM.

  @note This function follows the same pinning protocol as lf_hash_search(),
        i.e. uses pins[0..2]. On return pins[0..1] are removed and pins[2]
        is used to pin object found. It is also not removed in case when
        object is not found/error occurs but its value is undefined in
        this case.
        So calling lf_hash_unpin() is mandatory after call to this function
        in case of both success and failure.
*/
DECLARE_METHOD(void *, random_match,
               (LF_HASH_h hash, LF_PINS_h pins, lf_hash_match_func *match,
                uint rand_val, void *match_arg));

/**
  Unpin search pins.

  @param pins Pins handle.
*/
DECLARE_METHOD(void, search_unpin, (LF_PINS_h pins));

/**
  Put pins into a hash.

  @param pins Pins handle.
*/
DECLARE_METHOD(void, put_pins, (LF_PINS_h pins));

/**
  Insert data into a hash.

  @param hash Hash handle.
  @param pins Pins handle.
  @param data Data pointer.

  @retval 0  Inserted.
  @retval 1  Failed. Unique key conflict.
  @retval -1 Failed. Out of memory.
*/
DECLARE_METHOD(int, insert, (LF_HASH_h hash, LF_PINS_h pins, const void *data));

/**
  Hash entry header size.

  @return Overhead value.
*/
DECLARE_METHOD(int, overhead, ());

END_SERVICE_DEFINITION(mysql_lock_free_hash)

#endif /* MYSQL_LOCK_FREE_HASH_H */
