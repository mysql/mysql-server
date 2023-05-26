/*****************************************************************************

Copyright (c) 2014, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0new.h
    Dynamic memory allocation routines and custom allocators specifically
    crafted to support memory instrumentation through performance schema memory
    engine (PFS).
 */

/** This file contains a set of libraries providing overloads for regular
    dynamic allocation routines which allow for opt-in memory instrumentation
    through performance schema memory engine (PFS).

    In particular, _no_ regular dynamic allocation routines shall be used given
    that the end goal of instrumentation through PFS is system observability
    and resource control. In practice this means that we are off the chances to
    use _any_ standard means of allocating the memory and that we have to
    provide and re-implement our own PFS-aware variants ourselves.

    This does not only apply to direct memory allocation through malloc or new
    but also to data structures that may allocate dynamic memory under the hood,
    like the ones from STL. For that reason, STL data structures shall always
    be used with PFS-enabled custom memory allocator. STL algorithms OTOH
    also _may_ allocate dynamic memory but they do not provide customization
    point for user-code to provide custom memory allocation mechanism so there's
    nothing that we can do about it.

    Furthermore, facilities that allow safer memory management such as
    std::unique_ptr, std::shared_ptr and their respective std::make_unique and
    std::make_shared functions also have to be re-implemented as such so that
    they become PFS-aware.

    Following is the list of currently implemented PFS-enabled dynamic
    allocation overloads and associated facilities:
      * Primitive allocation functions:
          * ut::malloc
          * ut::zalloc
          * ut::realloc
          * ut::free
          * ut::{malloc | zalloc | realloc}_withkey
      * Primitive allocation functions for types with extended alignment:
          * ut::aligned_alloc
          * ut::aligned_zalloc
          * ut::aligned_free
          * ut::{aligned_alloc | aligned_zalloc}_withkey
      * Primitive allocation functions for page-aligned allocations:
          * ut::malloc_page
          * ut::malloc_page_withkey
          * ut::free_page
      * Primitive allocation functions for large (huge) page aligned
        allocations:
          * ut::malloc_large_page
          * ut::malloc_large_page_withkey
          * ut::free_large_page
      * Primitive allocation functions for large (huge) aligned allocations with
        fallback to page-aligned allocations:
          * ut::malloc_large_page(fallback_to_normal_page_t)
          * ut::malloc_large_page_withkey(fallback_to_normal_page_t)
          * ut::free_large_page(fallback_to_normal_page_t)
      * Overloads for C++ new and delete syntax:
          * ut::new_
          * ut::new_arr
          * ut::{new_ | new_arr_}_withkey
          * ut::delete_
          * ut::delete_arr
      * Overloads for C++ new and delete syntax for types with extended
        alignment:
          * ut::aligned_new
          * ut::aligned_new_arr
          * ut::{aligned_new_ | aligned_new_arr_}_withkey
          * ut::aligned_delete
          * ut::aligned_delete_arr
      * Custom memory allocators:
          * ut::allocator
      * Overloads for std::unique_ptr and std::shared_ptr factory functions
          * ut::make_unique
          * ut::make_unique_aligned
          * ut::make_shared
          * ut::make_shared_aligned
    _withkey variants from above are the PFS-enabled dynamic allocation
    overloads.

    Usages of PFS-enabled library functions are trying to resemble already
    familiar syntax as close as possible. For concrete examples please see
    particular function documentation but in general it applies that ::foo(x)
    becomes ut::foo(x) or ut::foo_withkey(key, x) where foo is some allocation
    function listed above and key is PFS key to instrument the allocation with.
*/

#ifndef ut0new_h
#define ut0new_h

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <type_traits> /* std::is_trivially_default_constructible */
#include <unordered_map>
#include <unordered_set>

#include "my_basename.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/psi_memory.h"

namespace ut {
/** Can be used to extract pointer and size of the allocation provided by the
OS. It is a low level information, and is needed only to call low level
memory-related OS functions. */
struct allocation_low_level_info {
  /** A pointer returned by the OS allocator. */
  void *base_ptr;
  /** The size of allocation that OS performed. */
  size_t allocation_size;
};
}  // namespace ut

#include "detail/ut0new.h"
#include "os0proc.h"
#include "os0thread.h"
#include "univ.i"
#include "ut0byte.h" /* ut_align */
#include "ut0cpu_cache.h"
#include "ut0dbg.h"
#include "ut0ut.h"

namespace ut {

/** Light-weight and type-safe wrapper around the PSI_memory_key
    that eliminates the possibility of introducing silent bugs
    through the course of implicit conversions and makes them
    show up as compile-time errors.

    Without this wrapper it was possible to say:
      aligned_alloc_withkey(10*sizeof(int), key, 64))
    Which would unfortunately compile just fine but it would silently
    introduce a bug because it confuses the order of 10*sizeof(int) and
    key input arguments. Both of them are unsigned types.

    With the wrapper, aligned_alloc_withkey(10*sizeof(int), key, 64)) now
    results with a compile-time error and the only proper way to accomplish
    the original intent is to use PSI_memory_key_t wrapper like so:
    aligned_alloc_withkey(PSI_memory_key_t{key}, 10*sizeof(int), 64))

    Or by making use of the convenience function to create one:
      aligned_alloc_withkey(make_psi_memory_key(key), 10*sizeof(int), 64))
*/
struct PSI_memory_key_t {
  explicit PSI_memory_key_t(PSI_memory_key key) : m_key(key) {}
  PSI_memory_key operator()() const { return m_key; }
  PSI_memory_key m_key;
};

/** Convenience helper function to create type-safe representation of
    PSI_memory_key.

    @param[in] key PSI memory key to be held in type-safe PSI_memory_key_t.
    @return PSI_memory_key_t which wraps the given PSI_memory_key
 */
inline PSI_memory_key_t make_psi_memory_key(PSI_memory_key key) {
  return PSI_memory_key_t(key);
}

}  // namespace ut

/** Maximum number of retries to allocate memory. */
extern const size_t alloc_max_retries;

/** Keys for registering allocations with performance schema.
Pointers to these variables are supplied to PFS code via the pfs_info[]
array and the PFS code initializes them via PSI_MEMORY_CALL(register_memory)().
mem_key_other and mem_key_std are special in the following way.
* If the caller has not provided a key and the file name of the caller is
  unknown, then mem_key_std will be used. This happens only when called from
  within std::* containers.
* If the caller has not provided a key and the file name of the caller is
  known, but is not amongst the predefined names (see ut_new_boot()) then
  mem_key_other will be used. Generally this should not happen and if it
  happens then that means that the list of predefined names must be extended.
Keep this list alphabetically sorted. */
extern PSI_memory_key mem_key_ahi;
extern PSI_memory_key mem_key_archive;
extern PSI_memory_key mem_key_buf_buf_pool;
extern PSI_memory_key mem_key_buf_stat_per_index_t;
/** Memory key for clone */
extern PSI_memory_key mem_key_clone;
extern PSI_memory_key mem_key_dict_stats_bg_recalc_pool_t;
extern PSI_memory_key mem_key_dict_stats_index_map_t;
extern PSI_memory_key mem_key_dict_stats_n_diff_on_level;
extern PSI_memory_key mem_key_fil_space_t;
extern PSI_memory_key mem_key_redo_log_archive_queue_element;
extern PSI_memory_key mem_key_other;
extern PSI_memory_key mem_key_partitioning;
extern PSI_memory_key mem_key_row_log_buf;
extern PSI_memory_key mem_key_ddl;
extern PSI_memory_key mem_key_std;
extern PSI_memory_key mem_key_trx_sys_t_rw_trx_ids;
extern PSI_memory_key mem_key_undo_spaces;
extern PSI_memory_key mem_key_ut_lock_free_hash_t;
/* Please obey alphabetical order in the definitions above. */

/** Setup the internal objects needed for ut::*_withkey() to operate.
This must be called before the first call to ut::*_withkey(). */
void ut_new_boot();

/** Setup the internal objects needed for ut::*_withkey() to operate.
This must be called before the first call to ut::*_withkey(). This
version of function might be called several times and it will
simply skip all calls except the first one, during which the
initialization will happen. */
void ut_new_boot_safe();

#ifdef UNIV_PFS_MEMORY

/** List of filenames that allocate memory and are instrumented via PFS. */
static constexpr const char *auto_event_names[] = {
    /* Keep this list alphabetically sorted. */
    "api0api",
    "api0misc",
    "btr0btr",
    "btr0cur",
    "btr0load",
    "btr0pcur",
    "btr0sea",
    "btr0types",
    "buf",
    "buf0buddy",
    "buf0buf",
    "buf0checksum",
    "buf0dblwr",
    "buf0dump",
    "buf0flu",
    "buf0lru",
    "buf0rea",
    "buf0stats",
    "buf0types",
    "checksum",
    "crc32",
    "create",
    "data0data",
    "data0type",
    "data0types",
    "db0err",
    "ddl0buffer",
    "ddl0builder",
    "ddl0ctx",
    "ddl0ddl",
    "ddl0file-reader",
    "ddl0loader",
    "ddl0merge",
    "ddl0rtree",
    "ddl0par-scan",
    "dict",
    "dict0boot",
    "dict0crea",
    "dict0dd",
    "dict0dict",
    "dict0load",
    "dict0mem",
    "dict0priv",
    "dict0sdi",
    "dict0stats",
    "dict0stats_bg",
    "dict0types",
    "dyn0buf",
    "dyn0types",
    "eval0eval",
    "eval0proc",
    "fil0fil",
    "fil0types",
    "file",
    "fsp0file",
    "fsp0fsp",
    "fsp0space",
    "fsp0sysspace",
    "fsp0types",
    "fts0ast",
    "fts0blex",
    "fts0config",
    "fts0fts",
    "fts0opt",
    "fts0pars",
    "fts0plugin",
    "fts0priv",
    "fts0que",
    "fts0sql",
    "fts0tlex",
    "fts0tokenize",
    "fts0types",
    "fts0vlc",
    "fut0fut",
    "fut0lst",
    "gis0geo",
    "gis0rtree",
    "gis0sea",
    "gis0type",
    "ha0ha",
    "ha0storage",
    "ha_innodb",
    "ha_innopart",
    "ha_prototypes",
    "handler0alter",
    "hash0hash",
    "i_s",
    "ib0mutex",
    "ibuf0ibuf",
    "ibuf0types",
    "lexyy",
    "lob0lob",
    "lock0iter",
    "lock0lock",
    "lock0prdt",
    "lock0priv",
    "lock0types",
    "lock0wait",
    "log0log",
    "log0recv",
    "log0write",
    "mach0data",
    "mem",
    "mem0mem",
    "memory",
    "mtr0log",
    "mtr0mtr",
    "mtr0types",
    "os0atomic",
    "os0event",
    "os0file",
    "os0numa",
    "os0once",
    "os0proc",
    "os0thread",
    "page",
    "page0cur",
    "page0page",
    "page0size",
    "page0types",
    "page0zip",
    "pars0grm",
    "pars0lex",
    "pars0opt",
    "pars0pars",
    "pars0sym",
    "pars0types",
    "que0que",
    "que0types",
    "read0read",
    "read0types",
    "rec",
    "rem0cmp",
    "rem0rec",
    "rem0types",
    "row0ext",
    "row0ft",
    "row0import",
    "row0ins",
    "row0log",
    "row0mysql",
    "row0purge",
    "row0quiesce",
    "row0row",
    "row0sel",
    "row0types",
    "row0uins",
    "row0umod",
    "row0undo",
    "row0upd",
    "row0vers",
    "sess0sess",
    "srv0conc",
    "srv0mon",
    "srv0srv",
    "srv0start",
    "srv0tmp",
    "sync0arr",
    "sync0debug",
    "sync0policy",
    "sync0sharded_rw",
    "sync0rw",
    "sync0sync",
    "sync0types",
    "trx0i_s",
    "trx0purge",
    "trx0rec",
    "trx0roll",
    "trx0rseg",
    "trx0sys",
    "trx0trx",
    "trx0types",
    "trx0undo",
    "trx0xa",
    "usr0sess",
    "usr0types",
    "ut",
    "ut0byte",
    "ut0counter",
    "ut0crc32",
    "ut0dbg",
    "ut0link_buf",
    "ut0list",
    "ut0lock_free_hash",
    "ut0lst",
    "ut0mem",
    "ut0mutex",
    "ut0new",
    "ut0pool",
    "ut0rbt",
    "ut0rnd",
    "ut0sort",
    "ut0stage",
    "ut0ut",
    "ut0vec",
    "ut0wqueue",
    "zipdecompress",
};

static constexpr size_t n_auto = UT_ARR_SIZE(auto_event_names);
extern PSI_memory_key auto_event_keys[n_auto];
extern PSI_memory_info pfs_info_auto[n_auto];

/** gcc 5 fails to evaluate costexprs at compile time. */
#if defined(__GNUG__) && (__GNUG__ == 5)

/** Compute whether a string begins with a given prefix, compile-time.
@param[in]      a       first string, taken to be zero-terminated
@param[in]      b       second string (prefix to search for)
@param[in]      b_len   length in bytes of second string
@param[in]      index   character index to start comparing at
@return whether b is a prefix of a */
constexpr bool ut_string_begins_with(const char *a, const char *b, size_t b_len,
                                     size_t index = 0) {
  return (index == b_len || (a[index] == b[index] &&
                             ut_string_begins_with(a, b, b_len, index + 1)));
}

/** Find the length of the filename without its file extension.
@param[in]      file    filename, with extension but without directory
@param[in]      index   character index to start scanning for extension
                        separator at
@return length, in bytes */
constexpr size_t ut_len_without_extension(const char *file, size_t index = 0) {
  return ((file[index] == '\0' || file[index] == '.')
              ? index
              : ut_len_without_extension(file, index + 1));
}

/** Retrieve a memory key (registered with PFS), given the file name of the
caller.
@param[in]      file    portion of the filename - basename, with extension
@param[in]      len     length of the filename to check for
@param[in]      index   index of first PSI key to check
@return registered memory key or PSI_NOT_INSTRUMENTED if not found */
constexpr PSI_memory_key ut_new_get_key_by_base_file(const char *file,
                                                     size_t len,
                                                     size_t index = 0) {
  return ((index == n_auto)
              ? PSI_NOT_INSTRUMENTED
              : (ut_string_begins_with(auto_event_names[index], file, len)
                     ? auto_event_keys[index]
                     : ut_new_get_key_by_base_file(file, len, index + 1)));
}

/** Retrieve a memory key (registered with PFS), given the file name of
the caller.
@param[in]      file    portion of the filename - basename, with extension
@return registered memory key or PSI_NOT_INSTRUMENTED if not found */
constexpr PSI_memory_key ut_new_get_key_by_file(const char *file) {
  return (ut_new_get_key_by_base_file(file, ut_len_without_extension(file)));
}

#define UT_NEW_THIS_FILE_PSI_KEY ut_new_get_key_by_file(MY_BASENAME)

#else /* __GNUG__ == 5 */

/** Compute whether a string begins with a given prefix, compile-time.
@param[in]      a       first string, taken to be zero-terminated
@param[in]      b       second string (prefix to search for)
@param[in]      b_len   length in bytes of second string
@return whether b is a prefix of a */
constexpr bool ut_string_begins_with(const char *a, const char *b,
                                     size_t b_len) {
  for (size_t i = 0; i < b_len; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

/** Find the length of the filename without its file extension.
@param[in]      file    filename, with extension but without directory
@return length, in bytes */
constexpr size_t ut_len_without_extension(const char *file) {
  for (size_t i = 0;; ++i) {
    if (file[i] == '\0' || file[i] == '.') {
      return i;
    }
  }
}

/** Retrieve a memory key (registered with PFS), given the file name of the
caller.
@param[in]      file    portion of the filename - basename, with extension
@param[in]      len     length of the filename to check for
@return index to registered memory key or -1 if not found */
constexpr int ut_new_get_key_by_base_file(const char *file, size_t len) {
  for (size_t i = 0; i < n_auto; ++i) {
    if (ut_string_begins_with(auto_event_names[i], file, len)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

/** Retrieve a memory key (registered with PFS), given the file name of
the caller.
@param[in]      file    portion of the filename - basename, with extension
@return index to memory key or -1 if not found */
constexpr int ut_new_get_key_by_file(const char *file) {
  return ut_new_get_key_by_base_file(file, ut_len_without_extension(file));
}

// Sending an expression through a template variable forces the compiler to
// evaluate the expression at compile time (constexpr in itself has no such
// guarantee, only that the compiler is allowed).
template <int Value>
struct force_constexpr {
  static constexpr int value = Value;
};

#define UT_NEW_THIS_FILE_PSI_INDEX \
  (force_constexpr<ut_new_get_key_by_file(MY_BASENAME)>::value)

#define UT_NEW_THIS_FILE_PSI_KEY                       \
  (UT_NEW_THIS_FILE_PSI_INDEX == -1                    \
       ? ut::make_psi_memory_key(PSI_NOT_INSTRUMENTED) \
       : ut::make_psi_memory_key(auto_event_keys[UT_NEW_THIS_FILE_PSI_INDEX]))

#endif /* __GNUG__ == 5 */

#else

#define UT_NEW_THIS_FILE_PSI_KEY ut::make_psi_memory_key(PSI_NOT_INSTRUMENTED)

#endif /* UNIV_PFS_MEMORY */

namespace ut {

#ifdef HAVE_PSI_MEMORY_INTERFACE
constexpr bool WITH_PFS_MEMORY = true;
#else
constexpr bool WITH_PFS_MEMORY = false;
#endif

/** Dynamically allocates storage of given size. Instruments the memory with
    given PSI memory key in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_withkey(key, 10*sizeof(int)));
 */
inline void *malloc_withkey(PSI_memory_key_t key, std::size_t size) noexcept {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, false>;
  using malloc_impl = detail::Alloc_<impl>;
  return malloc_impl::alloc<false>(size, key());
}

/** Dynamically allocates storage of given size.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
   10*sizeof(int)));
 */
inline void *malloc(std::size_t size) noexcept {
  return ut::malloc_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED), size);
}

/** Dynamically allocates zero-initialized storage of given size. Instruments
    the memory with given PSI memory key in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the zero-initialized allocated storage. nullptr if
    dynamic storage allocation failed.

    Example:
     int *x = static_cast<int*>(ut::zalloc_withkey(key, 10*sizeof(int)));
 */
inline void *zalloc_withkey(PSI_memory_key_t key, std::size_t size) noexcept {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, false>;
  using malloc_impl = detail::Alloc_<impl>;
  return malloc_impl::alloc<true>(size, key());
}

/** Dynamically allocates zero-initialized storage of given size.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the zero-initialized allocated storage. nullptr if
    dynamic storage allocation failed.

    Example:
     int *x = static_cast<int*>(ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
   10*sizeof(int)));
 */
inline void *zalloc(std::size_t size) noexcept {
  return ut::zalloc_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED), size);
}

/** Upsizes or downsizes already dynamically allocated storage to the new size.
    Instruments the memory with given PSI memory key in case PFS memory support
    is enabled.

    It also supports standard realloc() semantics by:
      * allocating size bytes of memory when passed ptr is nullptr
      * freeing the memory pointed by ptr if passed size is 0

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] ptr Pointer to the memory area to be reallocated.
    @param[in] size New size of storage (in bytes) requested to be reallocated.
    @return Pointer to the reallocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_withkey(key, 10*sizeof(int));
     x = static_cast<int*>(ut::realloc_withkey(key, ptr, 100*sizeof(int)));
 */
inline void *realloc_withkey(PSI_memory_key_t key, void *ptr,
                             std::size_t size) noexcept {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, false>;
  using malloc_impl = detail::Alloc_<impl>;
  return malloc_impl::realloc(ptr, size, key());
}

/** Upsizes or downsizes already dynamically allocated storage to the new size.

    It also supports standard realloc() semantics by:
      * allocating size bytes of memory when passed ptr is nullptr
      * freeing the memory pointed by ptr if passed size is 0

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] ptr Pointer to the memory area to be reallocated.
    @param[in] size New size of storage (in bytes) requested to be reallocated.
    @return Pointer to the reallocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
   10*sizeof(int)); x = static_cast<int*>(ut::realloc(key, ptr,
   100*sizeof(int)));
 */
inline void *realloc(void *ptr, std::size_t size) noexcept {
  return ut::realloc_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED), ptr,
                             size);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::malloc*(), ut::realloc* or ut::zalloc*() variants.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc*(), ut::realloc* or ut::zalloc*() variants.

    Example:
     ut::free(ptr);
 */
inline void free(void *ptr) noexcept {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, false>;
  using malloc_impl = detail::Alloc_<impl>;
  malloc_impl::free(ptr);
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Instruments the memory with given PSI memory
    key in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] args Arguments one wishes to pass over to T constructor(s)
    @return Pointer to the allocated storage. Throws std::bad_alloc exception
    if dynamic storage allocation could not be fulfilled. Re-throws whatever
    exception that may have occurred during the construction of T, in which case
    it automatically cleans up the raw memory allocated for it.

    Example 1:
     int *ptr = ut::new_withkey<int>(key);

    Example 2:
     int *ptr = ut::new_withkey<int>(key, 10);
     assert(*ptr == 10);

    Example 3:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_withkey<A>(key, 1, 2);
     assert(ptr->_x == 1);
     assert(ptr->_y == 2);
 */
template <typename T, typename... Args>
inline T *new_withkey(PSI_memory_key_t key, Args &&... args) {
  auto mem = ut::malloc_withkey(key, sizeof(T));
  if (unlikely(!mem)) throw std::bad_alloc();
  try {
    new (mem) T(std::forward<Args>(args)...);
  } catch (...) {
    ut::free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] args Arguments one wishes to pass over to T constructor(s)
    @return Pointer to the allocated storage. Throws std::bad_alloc exception
    if dynamic storage allocation could not be fulfilled. Re-throws whatever
    exception that may have occurred during the construction of T, in which case
    it automatically cleans up the raw memory allocated for it.

    Example 1:
     int *ptr = ut::new_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY);

    Example 2:
     int *ptr = ut::new_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY, 10);
     assert(*ptr == 10);

    Example 3:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_withkey<A>(UT_NEW_THIS_FILE_PSI_KEY, 1, 2);
     assert(ptr->_x == 1);
     assert(ptr->_y == 2);
 */
template <typename T, typename... Args>
inline T *new_(Args &&... args) {
  return ut::new_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                            std::forward<Args>(args)...);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::new*() variants. Destructs the object of type T.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::new*() variants

    Example:
     ut::delete_(ptr);
 */
template <typename T>
inline void delete_(T *ptr) noexcept {
  if (unlikely(!ptr)) return;
  ptr->~T();
  ut::free(ptr);
}

/** Dynamically allocates storage for an array of T's. Constructs objects of
    type T with provided Args. Arguments that are to be used to construct some
    respective instance of T shall be wrapped into a std::tuple. See examples
    down below. Instruments the memory with given PSI memory key in case PFS
    memory support is enabled.

    To create an array of default-intialized T's, one can use this function
    template but for convenience purposes one can achieve the same by using
    the ut::new_arr_withkey with ut::Count overload.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] args Tuples of arguments one wishes to pass over to T
    constructor(s).
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::new_arr_withkey<int>(key,
                    std::forward_as_tuple(1),
                    std::forward_as_tuple(2));
     assert(ptr[0] == 1);
     assert(ptr[1] == 2);

    Example 2:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(key,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
                std::forward_as_tuple(8, 9));
     assert(ptr[0]->_x == 0 && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2 && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 4 && ptr[2]->_y == 5);
     assert(ptr[3]->_x == 6 && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 8 && ptr[4]->_y == 9);

    Example 3:
     struct A {
       A() : _x(10), _y(100) {}
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(key,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(), std::forward_as_tuple(6, 7),
                std::forward_as_tuple());
     assert(ptr[0]->_x == 0  && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2  && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 6  && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);
 */
template <typename T, typename... Args>
inline T *new_arr_withkey(PSI_memory_key_t key, Args &&... args) {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, true>;
  using malloc_impl = detail::Alloc_<impl>;
  auto mem = malloc_impl::alloc<false>(sizeof(T) * sizeof...(args), key());
  if (unlikely(!mem)) throw std::bad_alloc();

  size_t idx = 0;
  try {
    (...,
     detail::construct<T>(mem, sizeof(T) * idx++, std::forward<Args>(args)));
  } catch (...) {
    for (size_t offset = (idx - 1) * sizeof(T); offset != 0;
         offset -= sizeof(T)) {
      reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(mem) + offset -
                            sizeof(T))
          ->~T();
    }
    malloc_impl::free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an array of T's. Constructs objects of
    type T with provided Args. Arguments that are to be used to construct some
    respective instance of T shall be wrapped into a std::tuple. See examples
    down below.

    To create an array of default-intialized T's, one can use this function
    template but for convenience purposes one can achieve the same by using
    the ut::new_arr_withkey with ut::Count overload.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] args Tuples of arguments one wishes to pass over to T
    constructor(s).
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::new_arr_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY,
                    std::forward_as_tuple(1),
                    std::forward_as_tuple(2));
     assert(ptr[0] == 1);
     assert(ptr[1] == 2);

    Example 2:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(UT_NEW_THIS_FILE_PSI_KEY,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
                std::forward_as_tuple(8, 9));
     assert(ptr[0]->_x == 0 && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2 && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 4 && ptr[2]->_y == 5);
     assert(ptr[3]->_x == 6 && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 8 && ptr[4]->_y == 9);

    Example 3:
     struct A {
       A() : _x(10), _y(100) {}
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(UT_NEW_THIS_FILE_PSI_KEY,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(), std::forward_as_tuple(6, 7),
                std::forward_as_tuple());
     assert(ptr[0]->_x == 0  && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2  && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 6  && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);
 */
template <typename T, typename... Args>
inline T *new_arr(Args &&... args) {
  return ut::new_arr_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                std::forward<Args>(args)...);
}

/** Light-weight and type-safe wrapper which serves a purpose of
    being able to select proper ut::new_arr* overload.

    Without having a separate overload with this type, creating an array of
    default-initialized instances of T through the ut::new_arr*(Args &&... args)
    overload would have been impossible because:
      int *ptr = ut::new_arr_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY, 5);
    wouldn't even compile and
      int *ptr = ut::new_arr_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY,
   std::forward_as_tuple(5)); would compile but would not have intended effect.
   It would create an array holding 1 integer element that is initialized to 5.

    Given that function templates cannot be specialized, having an overload
    crafted specifically for given case solves the problem:
      int *ptr = ut::new_arr_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY,
   ut::Count{5});
*/
struct Count {
  explicit Count(size_t count) : m_count(count) {}
  size_t operator()() const { return m_count; }
  size_t m_count;
};

/** Dynamically allocates storage for an array of T's. Constructs objects of
    type T using default constructor. If T cannot be default-initialized (e.g.
    default constructor does not exist), then this interface cannot be used for
    constructing such an array. ut::new_arr_withkey overload with user-provided
    initialization must be used then. Instruments the memory with given PSI
    memory key in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] count Number of T elements in an array.
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::new_arr_withkey<int>(key, ut::Count{2});

    Example 2:
     struct A {
       A() : _x(10), _y(100) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(key, ut::Count{5});
     assert(ptr[0]->_x == 10 && ptr[0]->_y == 100);
     assert(ptr[1]->_x == 10 && ptr[1]->_y == 100);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 10 && ptr[3]->_y == 100);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);

    Example 3:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     // Following cannot compile because A is not default-constructible
     A *ptr = ut::new_arr_withkey<A>(key, ut::Count{5});
 */
template <typename T>
inline T *new_arr_withkey(PSI_memory_key_t key, Count count) {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, true>;
  using malloc_impl = detail::Alloc_<impl>;
  auto mem = malloc_impl::alloc<false>(sizeof(T) * count(), key());
  if (unlikely(!mem)) throw std::bad_alloc();

  size_t offset = 0;
  try {
    for (; offset < sizeof(T) * count(); offset += sizeof(T)) {
      new (reinterpret_cast<uint8_t *>(mem) + offset) T{};
    }
  } catch (...) {
    for (; offset != 0; offset -= sizeof(T)) {
      reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(mem) + offset -
                            sizeof(T))
          ->~T();
    }
    malloc_impl::free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an array of T's. Constructs objects of
    type T using default constructor. If T cannot be default-initialized (e.g.
    default constructor does not exist), then this interface cannot be used for
    constructing such an array. ut::new_arr overload with user-provided
    initialization must be used then.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] count Number of T elements in an array.
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::new_arr_withkey<int>(UT_NEW_THIS_FILE_PSI_KEY,
   ut::Count{2});

    Example 2:
     struct A {
       A() : _x(10), _y(100) {}
       int _x, _y;
     };
     A *ptr = ut::new_arr_withkey<A>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{5});
     assert(ptr[0]->_x == 10 && ptr[0]->_y == 100);
     assert(ptr[1]->_x == 10 && ptr[1]->_y == 100);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 10 && ptr[3]->_y == 100);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);

    Example 3:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     // Following cannot compile because A is not default-constructible
     A *ptr = ut::new_arr_withkey<A>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{5});
 */
template <typename T>
inline T *new_arr(Count count) {
  return ut::new_arr_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                count);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::new_arr*() variants. Destructs all objects of type T.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::new_arr*() variants

    Example:
     ut::delete_arr(ptr);
 */
template <typename T>
inline void delete_arr(T *ptr) noexcept {
  if (unlikely(!ptr)) return;
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, true>;
  using malloc_impl = detail::Alloc_<impl>;
  const auto data_len = malloc_impl::datalen(ptr);
  for (size_t offset = 0; offset < data_len; offset += sizeof(T)) {
    reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(ptr) + offset)->~T();
  }
  malloc_impl::free(ptr);
}

/** Returns number of bytes that ut::malloc_*, ut::zalloc_*, ut::realloc_* and
    ut::new_* variants will be using to store the necessary metadata for PFS.

    @return Size of the PFS metadata.
*/
inline size_t pfs_overhead() noexcept {
  using impl = detail::select_malloc_impl_t<WITH_PFS_MEMORY, false>;
  using malloc_impl = detail::Alloc_<impl>;
  return malloc_impl::pfs_overhead();
}

/** Dynamically allocates system page-aligned storage of given size. Instruments
    the memory with given PSI memory key in case PFS memory support is enabled.

    Actual page-alignment, and thus page-size, will depend on CPU architecture
    but in general page is traditionally mostly 4K large. In contrast to Unices,
    Windows do make an exception here and implement 64K granularity on top of
    regular page-size for some legacy reasons. For more details see:
      https://devblogs.microsoft.com/oldnewthing/20031008-00/?p=42223

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_page_withkey(key, 10*sizeof(int)));
 */
inline void *malloc_page_withkey(PSI_memory_key_t key,
                                 std::size_t size) noexcept {
  using impl = detail::select_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using page_alloc_impl = detail::Page_alloc_<impl>;
  return page_alloc_impl::alloc(size, key());
}

/** Dynamically allocates system page-aligned storage of given size.

    Actual page-alignment, and thus page-size, will depend on CPU architecture
    but in general page is traditionally mostly 4K large. In contrast to Unices,
    Windows do make an exception here and implement 64K granularity on top of
    regular page-size for some legacy reasons. For more details see:
      https://devblogs.microsoft.com/oldnewthing/20031008-00/?p=42223

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_page(10*sizeof(int)));
 */
inline void *malloc_page(std::size_t size) noexcept {
  return ut::malloc_page_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                 size);
}

/** Retrieves the total amount of bytes that are available for application code
    to use.

    Amount of bytes returned does _not_ have to match bytes requested
    through ut::malloc_page*(). This is so because bytes requested will always
    be implicitly rounded up to the next regular page size (e.g. 4K).

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_page*() variants.
    @return Number of bytes available.

    Example:
     int *x = static_cast<int*>(ut::malloc_page(10*sizeof(int)));
     assert(page_allocation_size(x) == CPU_PAGE_SIZE);
 */
inline size_t page_allocation_size(void *ptr) noexcept {
  using impl = detail::select_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using page_alloc_impl = detail::Page_alloc_<impl>;
  return page_alloc_impl::datalen(ptr);
}

/** Retrieves the pointer and size of the allocation provided by the OS. It is a
    low level information, and is needed only to call low level memory-related
    OS functions.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_page*() variants.
    @return Low level OS allocation info.
 */
inline allocation_low_level_info page_low_level_info(void *ptr) noexcept {
  using impl = detail::select_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using page_alloc_impl = detail::Page_alloc_<impl>;
  return page_alloc_impl::low_level_info(ptr);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::malloc_page*() variants.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_page*() variants.
    @return True if releasing the page-aligned memory was successful.

    Example:
     ut::free_page(ptr);
 */
inline bool free_page(void *ptr) noexcept {
  using impl = detail::select_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using page_alloc_impl = detail::Page_alloc_<impl>;
  return page_alloc_impl::free(ptr);
}

/** Dynamically allocates memory backed up by large (huge) pages. Instruments
    the memory with given PSI memory key in case PFS memory support is enabled.

    For large (huge) pages to be functional, usually some steps in system admin
    preparation is required. Exact steps vary from system to system.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(
                ut::malloc_large_page_withkey(key, 10*sizeof(int))
              );
 */
inline void *malloc_large_page_withkey(PSI_memory_key_t key,
                                       std::size_t size) noexcept {
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  return large_page_alloc_impl::alloc(size, key());
}

/** Dynamically allocates memory backed up by large (huge) pages.

    For large (huge) pages to be functional, usually some steps in system admin
    preparation is required. Exact steps vary from system to system.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(ut::malloc_large_page(10*sizeof(int)));
 */
inline void *malloc_large_page(std::size_t size) noexcept {
  return ut::malloc_large_page_withkey(
      make_psi_memory_key(PSI_NOT_INSTRUMENTED), size);
}

/** Retrieves the total amount of bytes that are available for application code
    to use.

    Amount of bytes returned does _not_ have to match bytes requested
    through ut::malloc_large_page*(). This is so because bytes requested will
    always be implicitly rounded up to the next multiple of huge-page size (e.g.
    2MiB). Exact huge-page size value that is going to be used will be stored
    in large_page_default_size.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*() variants.
    @return Number of bytes available.

    Example:
     int *x = static_cast<int*>(ut::malloc_large_page(10*sizeof(int)));
     assert(large_page_allocation_size(x) == HUGE_PAGE_SIZE);
 */
inline size_t large_page_allocation_size(void *ptr) noexcept {
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  return large_page_alloc_impl::datalen(ptr);
}

/** Retrieves the pointer and size of the allocation provided by the OS. It is a
    low level information, and is needed only to call low level memory-related
    OS functions.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*() variants.
    @return Low level OS allocation info.
 */
inline allocation_low_level_info large_page_low_level_info(void *ptr) noexcept {
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  return large_page_alloc_impl::low_level_info(ptr);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::malloc_large_page*() variants.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*() variants.
    @return True if releasing the large (huge) page-aligned memory was
    successful.

    Example:
     ut::free_large_page(ptr);
 */
inline bool free_large_page(void *ptr) noexcept {
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  return large_page_alloc_impl::free(ptr);
}

/* Helper type for tag-dispatch */
struct fallback_to_normal_page_t {};

/** Dynamically allocates memory backed up by large (huge) pages. In the event
    that large (huge) pages are unavailable or disabled explicitly through
    os_use_large_pages, it will fallback to dynamic allocation backed by
    page-aligned memory. Instruments the memory with given PSI memory key in
    case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] large_pages_enabled If true, the large pages will be tried to be
    used.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(
                ut::malloc_large_page_withkey(
                  key,
                  10*sizeof(int),
                  fallback_to_normal_page_t{}
                )
              );
 */
inline void *malloc_large_page_withkey(
    PSI_memory_key_t key, std::size_t size, fallback_to_normal_page_t,
    bool large_pages_enabled = os_use_large_pages) noexcept {
  void *large_page_mem = nullptr;
  if (large_pages_enabled) {
    large_page_mem = malloc_large_page_withkey(key, size);
  }
  return large_page_mem ? large_page_mem : malloc_page_withkey(key, size);
}

/** Dynamically allocates memory backed up by large (huge) pages. In the event
    that large (huge) pages are unavailable or disabled explicitly through
    os_use_large_pages, it will fallback to dynamic allocation backed by
    page-aligned memory.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] large_pages_enabled If true, the large pages will be tried to be
    used.
    @return Pointer to the page-aligned storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int *x = static_cast<int*>(
                ut::malloc_large_page(
                  10*sizeof(int),
                  fallback_to_normal_page_t{}
                )
              );
 */
inline void *malloc_large_page(
    std::size_t size, fallback_to_normal_page_t,
    bool large_pages_enabled = os_use_large_pages) noexcept {
  return ut::malloc_large_page_withkey(
      make_psi_memory_key(PSI_NOT_INSTRUMENTED), size,
      fallback_to_normal_page_t{}, large_pages_enabled);
}

/** Retrieves the total amount of bytes that are available for application code
    to use.

    Amount of bytes returned does _not_ have to match bytes requested
    through ut::malloc_large_page*(fallback_to_normal_page_t). This is so
    because bytes requested will always be implicitly rounded up to the next
    multiple of either huge-page size (e.g. 2MiB) or regular page size (e.g.
    4K).

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*(fallback_to_normal_page_t) variants.
    @return Number of bytes available for use.
 */
inline size_t large_page_allocation_size(void *ptr,
                                         fallback_to_normal_page_t) noexcept {
  assert(ptr);
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  if (large_page_alloc_impl::page_type(ptr) == detail::Page_type::system_page)
    return ut::page_allocation_size(ptr);
  ut_a(large_page_alloc_impl::page_type(ptr) == detail::Page_type::large_page);
  return ut::large_page_allocation_size(ptr);
}

/** Retrieves the pointer and size of the allocation provided by the OS. It is a
    low level information, and is needed only to call low level memory-related
    OS functions.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*(fallback_to_normal_page_t) variants.
    @return Low level OS allocation info.
 */
inline allocation_low_level_info large_page_low_level_info(
    void *ptr, fallback_to_normal_page_t) noexcept {
  assert(ptr);
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;
  if (large_page_alloc_impl::page_type(ptr) == detail::Page_type::system_page)
    return ut::page_low_level_info(ptr);
  ut_a(large_page_alloc_impl::page_type(ptr) == detail::Page_type::large_page);
  return ut::large_page_low_level_info(ptr);
}

/** Releases storage which has been dynamically allocated through any of
    the ut::malloc_large_page*(fallback_to_normal_page_t) variants.

    Whether the pointer is representing area backed up by regular or huge-pages,
    this function will know the difference and therefore act accordingly.

    @param[in] ptr Pointer which has been obtained through any of the
    ut::malloc_large_page*(fallback_to_normal_page_t) variants.
    @return True if releasing the memory was successful.

    Example:
     ut::free_large_page(ptr);
 */
inline bool free_large_page(void *ptr, fallback_to_normal_page_t) noexcept {
  using impl = detail::select_large_page_alloc_impl_t<WITH_PFS_MEMORY>;
  using large_page_alloc_impl = detail::Large_alloc_<impl>;

  if (!ptr) return false;

  bool success;
  if (large_page_alloc_impl::page_type(ptr) == detail::Page_type::system_page) {
    success = free_page(ptr);
  } else {
    ut_a(large_page_alloc_impl::page_type(ptr) ==
         detail::Page_type::large_page);
    success = free_large_page(ptr);
  }
  assert(success);
  return success;
}

/** Dynamically allocates storage of given size and at the address aligned to
    the requested alignment. Instruments the memory with given PSI memory key
    in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @return Pointer to the allocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int* x = static_cast<int*>(aligned_alloc_withkey(key, 10*sizeof(int), 64));
 */
inline void *aligned_alloc_withkey(PSI_memory_key_t key, std::size_t size,
                                   std::size_t alignment) noexcept {
  using impl = detail::select_alloc_impl_t<WITH_PFS_MEMORY>;
  using aligned_alloc_impl = detail::Aligned_alloc_<impl>;
  return aligned_alloc_impl::alloc<false>(size, alignment, key());
}

/** Dynamically allocates storage of given size and at the address aligned to
    the requested alignment.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @return Pointer to the allocated storage. nullptr if dynamic storage
    allocation failed.

    Example:
     int* x = static_cast<int*>(aligned_alloc(10*sizeof(int), 64));
 */
inline void *aligned_alloc(std::size_t size, std::size_t alignment) noexcept {
  return aligned_alloc_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED), size,
                               alignment);
}

/** Dynamically allocates zero-initialized storage of given size and at the
    address aligned to the requested alignment. Instruments the memory with
    given PSI memory key in case PFS memory support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @return Pointer to the zero-initialized allocated storage. nullptr if
    dynamic storage allocation failed.

    Example:
     int* x =
       static_cast<int*>(aligned_zalloc_withkey(key, 10*sizeof(int), 64));
 */
inline void *aligned_zalloc_withkey(PSI_memory_key_t key, std::size_t size,
                                    std::size_t alignment) noexcept {
  using impl = detail::select_alloc_impl_t<WITH_PFS_MEMORY>;
  using aligned_alloc_impl = detail::Aligned_alloc_<impl>;
  return aligned_alloc_impl::alloc<true>(size, alignment, key());
}

/** Dynamically allocates zero-initialized storage of given size and at the
    address aligned to the requested alignment.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @return Pointer to the zero-initialized allocated storage. nullptr if
    dynamic storage allocation failed.

    Example:
     int* x = static_cast<int*>(aligned_zalloc(10*sizeof(int), 64));
 */
inline void *aligned_zalloc(std::size_t size, std::size_t alignment) noexcept {
  return aligned_zalloc_withkey(make_psi_memory_key(PSI_NOT_INSTRUMENTED), size,
                                alignment);
}

/** Releases storage which has been dynamically allocated through any of
    the aligned_alloc_*() or aligned_zalloc_*() variants.

    @param[in] ptr Pointer which has been obtained through any of the
    aligned_alloc_*() or aligned_zalloc_*() variants.

    Example:
     aligned_free(ptr);
 */
inline void aligned_free(void *ptr) noexcept {
  using impl = detail::select_alloc_impl_t<WITH_PFS_MEMORY>;
  using aligned_alloc_impl = detail::Aligned_alloc_<impl>;
  aligned_alloc_impl::free(ptr);
}

/** Dynamically allocates storage for an object of type T at address aligned
    to the requested alignment. Constructs the object of type T with provided
    Args. Instruments the memory with given PSI memory key in case PFS memory
    support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s)
    @return Pointer to the allocated storage. Throws std::bad_alloc exception
    if dynamic storage allocation could not be fulfilled.

    Example 1:
     int *ptr = aligned_new_withkey<int>(key, 2);

    Example 2:
     int *ptr = aligned_new_withkey<int>(key, 2, 10);
     assert(*ptr == 10);

    Example 3:
     struct A { A(int x, int y) : _x(x), _y(y) {} int x, y; }
     A *ptr = aligned_new_withkey<A>(key, 2, 1, 2);
     assert(ptr->x == 1);
     assert(ptr->y == 2);
 */
template <typename T, typename... Args>
inline T *aligned_new_withkey(PSI_memory_key_t key, std::size_t alignment,
                              Args &&... args) {
  auto mem = aligned_alloc_withkey(key, sizeof(T), alignment);
  if (unlikely(!mem)) throw std::bad_alloc();
  try {
    new (mem) T(std::forward<Args>(args)...);
  } catch (...) {
    ut::aligned_free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an object of type T at address aligned
    to the requested alignment. Constructs the object of type T with provided
    Args.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s)
    @return Pointer to the allocated storage. Throws std::bad_alloc exception
    if dynamic storage allocation could not be fulfilled.

    Example 1:
     int *ptr = aligned_new<int>(2);

    Example 2:
     int *ptr = aligned_new<int>(2, 10);
     assert(*ptr == 10);

    Example 3:
     struct A { A(int x, int y) : _x(x), _y(y) {} int x, y; }
     A *ptr = aligned_new<A>(2, 1, 2);
     assert(ptr->x == 1);
     assert(ptr->y == 2);
 */
template <typename T, typename... Args>
inline T *aligned_new(std::size_t alignment, Args &&... args) {
  return aligned_new_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                alignment, std::forward<Args>(args)...);
}

/** Releases storage which has been dynamically allocated through any of
    the aligned_new_*() variants. Destructs the object of type T.

    @param[in] ptr Pointer which has been obtained through any of the
    aligned_new_*() variants

    Example:
     aligned_delete(ptr);
 */
template <typename T>
inline void aligned_delete(T *ptr) noexcept {
  ptr->~T();
  aligned_free(ptr);
}

/** Dynamically allocates storage for an array of T's at address aligned to the
    requested alignment. Constructs objects of type T with provided Args.
    Arguments that are to be used to construct some respective instance of T
    shall be wrapped into a std::tuple. See examples down below. Instruments the
    memory with given PSI memory key in case PFS memory support is enabled.

    To create an array of default-initialized T's, one can use this function
    template but for convenience purposes one can achieve the same by using
    the ut::aligned_new_arr_withkey with ut::Count overload.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Tuples of arguments one wishes to pass over to T
    constructor(s).
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::aligned_new_arr_withkey<int>(key, 32,
                    std::forward_as_tuple(1),
                    std::forward_as_tuple(2));
     assert(ptr[0] == 1);
     assert(ptr[1] == 2);

    Example 2:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::aligned_new_arr_withkey<A>(key, 32,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
                std::forward_as_tuple(8, 9));
     assert(ptr[0]->_x == 0 && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2 && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 4 && ptr[2]->_y == 5);
     assert(ptr[3]->_x == 6 && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 8 && ptr[4]->_y == 9);

    Example 3:
     struct A {
       A() : _x(10), _y(100) {}
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     A *ptr = ut::aligned_new_arr_withkey<A>(key, 32,
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(), std::forward_as_tuple(6, 7),
                std::forward_as_tuple());
     assert(ptr[0]->_x == 0  && ptr[0]->_y == 1);
     assert(ptr[1]->_x == 2  && ptr[1]->_y == 3);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 6  && ptr[3]->_y == 7);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);
 */
template <typename T, typename... Args>
inline T *aligned_new_arr_withkey(PSI_memory_key_t key, std::size_t alignment,
                                  Args &&... args) {
  auto mem = aligned_alloc_withkey(key, sizeof(T) * sizeof...(args), alignment);
  if (unlikely(!mem)) throw std::bad_alloc();

  size_t idx = 0;
  try {
    (...,
     detail::construct<T>(mem, sizeof(T) * idx++, std::forward<Args>(args)));
  } catch (...) {
    for (size_t offset = (idx - 1) * sizeof(T); offset != 0;
         offset -= sizeof(T)) {
      reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(mem) + offset -
                            sizeof(T))
          ->~T();
    }
    aligned_free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an array of T's at address aligned to the
    requested alignment. Constructs objects of type T using default constructor.
    If T cannot be default-initialized (e.g. default constructor does not
    exist), then this interface cannot be used for constructing such an array.
    ut::new_arr_withkey overload with user-provided initialization must be used
    then. Instruments the memory with given PSI memory key in case PFS memory
    support is enabled.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] count Number of T elements in an array.
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled. Re-throws whatever exception that may have occurred during the
    construction of any instance of T, in which case it automatically destroys
    successfully constructed objects till that moment (if any), and finally
    cleans up the raw memory allocated for T instances.

    Example 1:
     int *ptr = ut::aligned_new_arr_withkey<int>(key, 32, ut::Count{2});

    Example 2:
     struct A {
       A() : _x(10), _y(100) {}
       int _x, _y;
     };
     A *ptr = ut::aligned_new_arr_withkey<A>(key, 32, ut::Count{5});
     assert(ptr[0]->_x == 10 && ptr[0]->_y == 100);
     assert(ptr[1]->_x == 10 && ptr[1]->_y == 100);
     assert(ptr[2]->_x == 10 && ptr[2]->_y == 100);
     assert(ptr[3]->_x == 10 && ptr[3]->_y == 100);
     assert(ptr[4]->_x == 10 && ptr[4]->_y == 100);

    Example 3:
     struct A {
       A(int x, int y) : _x(x), _y(y) {}
       int _x, _y;
     };
     // Following cannot compile because A is not default-constructible
     A *ptr = ut::aligned_new_arr_withkey<A>(key, 32, ut::Count{5});
 */
template <typename T>
inline T *aligned_new_arr_withkey(PSI_memory_key_t key, std::size_t alignment,
                                  Count count) {
  auto mem = aligned_alloc_withkey(key, sizeof(T) * count(), alignment);
  if (unlikely(!mem)) throw std::bad_alloc();

  size_t offset = 0;
  try {
    for (; offset < sizeof(T) * count(); offset += sizeof(T)) {
      new (reinterpret_cast<uint8_t *>(mem) + offset) T{};
    }
  } catch (...) {
    for (; offset != 0; offset -= sizeof(T)) {
      reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(mem) + offset -
                            sizeof(T))
          ->~T();
    }
    aligned_free(mem);
    throw;
  }
  return static_cast<T *>(mem);
}

/** Dynamically allocates storage for an array of T's at address aligned to
    the requested alignment. Constructs objects of type T with provided Args.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s)
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled.

    Example 1:
     int *ptr = aligned_new_arr<int, 5>(2);
     ptr[0] ... ptr[4]

    Example 2:
     int *ptr = aligned_new_arr<int, 5>(2, 1, 2, 3, 4, 5);
     assert(*ptr[0] == 1);
     assert(*ptr[1] == 2);
     ...
     assert(*ptr[4] == 5);

    Example 3:
     struct A { A(int x, int y) : _x(x), _y(y) {} int x, y; }
     A *ptr = aligned_new_arr<A, 5>(2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
     assert(ptr[0]->x == 1);
     assert(ptr[0]->y == 2);
     assert(ptr[1]->x == 3);
     assert(ptr[1]->y == 4);
     ...
     assert(ptr[4]->x == 9);
     assert(ptr[4]->y == 10);
 */
template <typename T, typename... Args>
inline T *aligned_new_arr(std::size_t alignment, Args &&... args) {
  return aligned_new_arr_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                    alignment, std::forward<Args>(args)...);
}

/** Dynamically allocates storage for an array of T's at address aligned to
    the requested alignment. Constructs objects of type T using default
    constructor.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] count Number of T elements in an array.
    @return Pointer to the first element of allocated storage. Throws
    std::bad_alloc exception if dynamic storage allocation could not be
    fulfilled.

    Example 1:
     int *ptr = aligned_new_arr<int>(2, 5);
     assert(*ptr[0] == 0);
     assert(*ptr[1] == 0);
     ...
     assert(*ptr[4] == 0);

    Example 2:
     struct A { A) : x(1), y(2) {} int x, y; }
     A *ptr = aligned_new_arr<A>(2, 5);
     assert(ptr[0].x == 1);
     assert(ptr[0].y == 2);
     ...
     assert(ptr[4].x == 1);
     assert(ptr[4].y == 2);

    Example 3:
     struct A { A(int x, int y) : _x(x), _y(y) {} int x, y; }
     A *ptr = aligned_new_arr<A>(2, 5);
     // will not compile, no default constructor
 */
template <typename T>
inline T *aligned_new_arr(std::size_t alignment, Count count) {
  return aligned_new_arr_withkey<T>(make_psi_memory_key(PSI_NOT_INSTRUMENTED),
                                    alignment, count);
}

/** Releases storage which has been dynamically allocated through any of the
    aligned_new_arr_*() variants. Destructs all objects of type T.

    @param[in] ptr Pointer which has been obtained through any of the
    aligned_new_arr_*() variants.

    Example:
     aligned_delete_arr(ptr);
 */
template <typename T>
inline void aligned_delete_arr(T *ptr) noexcept {
  using impl = detail::select_alloc_impl_t<WITH_PFS_MEMORY>;
  using aligned_alloc_impl = detail::Aligned_alloc_<impl>;
  const auto data_len = aligned_alloc_impl::datalen(ptr);
  for (size_t offset = 0; offset < data_len; offset += sizeof(T)) {
    reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(ptr) + offset)->~T();
  }
  aligned_free(ptr);
}

/** Lightweight convenience wrapper which manages dynamically allocated
    over-aligned type. Wrapper makes use of RAII to do the resource cleanup.

    Example usage:
      struct My_fancy_type {
        My_fancy_type(int x, int y) : _x(x), _y(y) {}
        int _x, _y;
      };

      aligned_pointer<My_fancy_type, 32> ptr;
      ptr.alloc(10, 5);
      My_fancy_type *p = ptr;
      assert(p->_x == 10 && p->_y == 5);

    @tparam T Type to be managed.
    @tparam Alignment Number of bytes to align the type T to.
 */
template <typename T, size_t Alignment>
class aligned_pointer {
  T *ptr = nullptr;

 public:
  /** Destructor. Invokes destructor of the underlying instance of
      type T. Releases dynamically allocated resources, if there had been
      left any.
   */
  ~aligned_pointer() {
    if (ptr) dealloc();
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the instance of type T at the address which is aligned to Alignment bytes.
      Constructs the instance of type T with given Args.

      Underlying instance of type T is accessed through the conversion operator.

      @param[in] args Any number and type of arguments that type T can be
      constructed with.
    */
  template <typename... Args>
  void alloc(Args &&... args) {
    ut_ad(ptr == nullptr);
    ptr = ut::aligned_new<T>(Alignment, args...);
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the instance of type T at the address which is aligned to Alignment bytes.
      Constructs the instance of type T with given Args. Instruments the memory
      with given PSI memory key in case PFS memory support is enabled.

      Underlying instance of type T is accessed through the conversion operator.

      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @param[in] args Any number and type of arguments that type T can be
      constructed with.
    */
  template <typename... Args>
  void alloc_withkey(PSI_memory_key_t key, Args &&... args) {
    ut_ad(ptr == nullptr);
    ptr =
        ut::aligned_new_withkey<T>(key, Alignment, std::forward<Args>(args)...);
  }

  /** Invokes the destructor of instance of type T, if applicable.
      Releases the resources previously allocated with alloc().
    */
  void dealloc() {
    ut_ad(ptr != nullptr);
    ut::aligned_delete(ptr);
    ptr = nullptr;
  }

  /** Conversion operator. Used for accessing the underlying instance of
      type T.
    */
  operator T *() const {
    ut_ad(ptr != nullptr);
    return ptr;
  }
};

/** Lightweight convenience wrapper which manages a dynamically
    allocated array of over-aligned types. Only the first element of an array is
    guaranteed to be aligned to the requested Alignment. Wrapper makes use of
    RAII to do the resource cleanup.

    Example usage 1:
      struct My_fancy_type {
        My_fancy_type() : _x(0), _y(0) {}
        My_fancy_type(int x, int y) : _x(x), _y(y) {}
        int _x, _y;
      };

      aligned_array_pointer<My_fancy_type, 32> ptr;
      ptr.alloc(3);
      My_fancy_type *p = ptr;
      assert(p[0]._x == 0 && p[0]._y == 0);
      assert(p[1]._x == 0 && p[1]._y == 0);
      assert(p[2]._x == 0 && p[2]._y == 0);

    Example usage 2:
      aligned_array_pointer<My_fancy_type, 32> ptr;
      ptr.alloc<3>(1, 2, 3, 4, 5, 6);
      My_fancy_type *p = ptr;
      assert(p[0]._x == 1 && p[0]._y == 2);
      assert(p[1]._x == 3 && p[1]._y == 4);
      assert(p[2]._x == 5 && p[2]._y == 6);

    @tparam T Type to be managed.
    @tparam Alignment Number of bytes to align the first element of array to.
 */
template <typename T, size_t Alignment>
class aligned_array_pointer {
  T *ptr = nullptr;

 public:
  /** Destructor. Invokes destructors of the underlying instances of
      type T. Releases dynamically allocated resources, if there had been
      left any.
   */
  ~aligned_array_pointer() {
    if (ptr) dealloc();
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the array of size number of elements of type T at the address which is
      aligned to Alignment bytes. Constructs the size number of instances of
      type T, each being initialized through the means of default constructor.

      Underlying instances of type T are accessed through the conversion
      operator.

      @param[in] count Number of T elements in an array.
    */
  void alloc(Count count) {
    ut_ad(ptr == nullptr);
    ptr = ut::aligned_new_arr<T>(Alignment, count);
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the array of size number of elements of type T at the address which is
      aligned to Alignment bytes. Constructs the size number of instances of
      type T, each being initialized through the means of provided Args and
      corresponding constructors.

      Underlying instances of type T are accessed through the conversion
      operator.

      @param[in] args Any number and type of arguments that type T can be
      constructed with.
    */
  template <typename... Args>
  void alloc(Args &&... args) {
    ut_ad(ptr == nullptr);
    ptr = ut::aligned_new_arr<T>(Alignment, std::forward<Args>(args)...);
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the array of size number of elements of type T at the address which is
      aligned to Alignment bytes. Constructs the size number of instances of
      type T, each being initialized through the means of default constructor.
      Instruments the memory with given PSI memory key in case PFS memory
      support is enabled.

      Underlying instances of type T are accessed through the conversion
      operator.

      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @param[in] count Number of T elements in an array.
    */
  void alloc_withkey(PSI_memory_key_t key, Count count) {
    ut_ad(ptr == nullptr);
    ptr = ut::aligned_new_arr_withkey<T>(key, Alignment, count);
  }

  /** Allocates sufficiently large memory of dynamic storage duration to fit
      the array of size number of elements of type T at the address which is
      aligned to Alignment bytes. Constructs the size number of instances of
      type T, each being initialized through the means of provided Args and
      corresponding constructors. Instruments the memory with given PSI memory
      key in case PFS memory support is enabled.

      Underlying instances of type T are accessed through the conversion
      operator.

      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @param[in] args Any number and type of arguments that type T can be
      constructed with.
    */
  template <typename... Args>
  void alloc_withkey(PSI_memory_key_t key, Args &&... args) {
    ut_ad(ptr == nullptr);
    ptr = ut::aligned_new_arr_withkey<T>(key, Alignment,
                                         std::forward<Args>(args)...);
  }

  /** Invokes destructors of instances of type T, if applicable.
      Releases the resources previously allocated with any variant of
      alloc().
    */
  void dealloc() {
    ut::aligned_delete_arr(ptr);
    ptr = nullptr;
  }

  /** Conversion operator. Used for accessing the underlying instances of
      type T.
    */
  operator T *() const {
    ut_ad(ptr != nullptr);
    return ptr;
  }
};

namespace detail {
template <typename T>
struct allocator_base {
  explicit allocator_base(PSI_memory_key /*key*/) {}

  template <typename U>
  allocator_base(const allocator_base<U> &other) {}

  void *allocate_impl(size_t n_bytes) { return ut::malloc(n_bytes); }
};

template <typename T>
struct allocator_base_pfs {
  explicit allocator_base_pfs(PSI_memory_key key) : m_key(key) {}

  template <typename U>
  allocator_base_pfs(const allocator_base_pfs<U> &other)
      : allocator_base_pfs(other.get_mem_key()) {}

  PSI_memory_key get_mem_key() const { return m_key; }

  void *allocate_impl(size_t n_bytes) {
    return ut::malloc_withkey(ut::make_psi_memory_key(m_key), n_bytes);
  }

 private:
  const PSI_memory_key m_key;
};
}  // namespace detail

/** Allocator that allows std::* containers to manage their memory through
    ut::malloc* and ut::free library functions.

    Main purpose of this custom allocator is to instrument all of the memory
    allocations and deallocations that are being done by std::* containers under
    the hood, and have them recorded through the PFS (memory) engine.

    Other than std::* containers, this allocator is of course also suitable for
    use in any other allocator-aware containers and/or code.

    Given that ut::malloc* and ut::free library functions already handle all
    the PFS and non-PFS implementation bits and pieces, this allocator is a mere
    wrapper around them.

    Example which uses default PFS key (mem_key_std) to trace all std::vector
    allocations and deallocations:
      std::vector<int, ut::allocator<int>> vec;
      vec.push_back(...);
      ...
      vec.push_back(...);

    Example which uses user-provided PFS key to trace std::vector allocations
    and deallocations:
      ut::allocator<int> allocator(some_other_psi_key);
      std::vector<int, ut::allocator<int>> vec(allocator);
      vec.push_back(...);
      ...
      vec.push_back(...);
 */
template <typename T, typename Allocator_base = std::conditional_t<
                          ut::WITH_PFS_MEMORY, detail::allocator_base_pfs<T>,
                          detail::allocator_base<T>>>
class allocator : public Allocator_base {
 public:
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static_assert(alignof(T) <= alignof(std::max_align_t),
                "ut::allocator does not support over-aligned types. Use "
                "ut::aligned_* API to handle such types.");

  /** Default constructor.
      @param[in] key  performance schema key.
    */
  explicit allocator(PSI_memory_key key = mem_key_std) : Allocator_base(key) {}

  /* Rule-of-five */
  allocator(const allocator<T, Allocator_base> &) = default;
  allocator<T, Allocator_base> &operator=(
      const allocator<T, Allocator_base> &) = default;
  allocator(allocator<T, Allocator_base> &&) = default;
  allocator<T, Allocator_base> &operator=(allocator<T, Allocator_base> &&) =
      default;
  ~allocator() = default;

  /** Copy-construct a new instance of allocator with type T by using existing
      instance of allocator constructed with a different type U.
      @param[in] other  the allocator to copy from.
    */
  template <typename U>
  allocator(const allocator<U, Allocator_base> &other)
      : Allocator_base(other) {}

  /* NOTE: rebind is deprecated in C++17 and to be removed in C++20 but one of
     our toolchains, when used in 32-bit setting, still does not support custom
     allocators that do not provide rebind support explicitly. In future, this
     part will become redundant and can be removed.
   */
  template <typename U>
  struct rebind {
    using other = ut::allocator<U, Allocator_base>;
  };

  /** Equality of allocators instantiated with same types T. */
  inline bool operator==(const ut::allocator<T, Allocator_base> &) const {
    return true;
  }
  /** Non-equality of allocators instantiated with same types T. */
  inline bool operator!=(const ut::allocator<T, Allocator_base> &other) const {
    return !(*this == other);
  }

  /** Return the maximum number of objects that can be allocated by
      this allocator. This number is somewhat lower for PFS-enabled
      builds because of extra few bytes needed for PFS.
    */
  size_type max_size() const {
    const size_type s_max = std::numeric_limits<size_type>::max();
    return (s_max - ut::pfs_overhead()) / sizeof(T);
  }

  /** Allocates chunk of memory that can hold n_elements objects of
      type T. Returned pointer is always valid. In case underlying
      allocation function was not able to fulfill the allocation request,
      this function will throw std::bad_alloc exception. After successful
      allocation, returned pointer must be passed back to
      ut::allocator<T>::deallocate() when no longer needed.

      @param[in]  n_elements  number of elements
      @param[in]  hint        pointer to a nearby memory location,
                              not used by this implementation
      @return pointer to the allocated memory
    */
  pointer allocate(size_type n_elements,
                   const_pointer hint [[maybe_unused]] = nullptr) {
    if (unlikely(n_elements > max_size())) {
      throw std::bad_array_new_length();
    }

    auto ptr = Allocator_base::allocate_impl(n_elements * sizeof(T));

    if (unlikely(!ptr)) {
      throw std::bad_alloc();
    }

    return static_cast<pointer>(ptr);
  }

  /** Releases the memory allocated through ut::allocator<T>::allocate().

      @param[in,out]  ptr         pointer to memory to free
      @param[in]      n_elements  number of elements allocated (unused)
   */
  void deallocate(pointer ptr, size_type n_elements [[maybe_unused]] = 0) {
    ut::free(ptr);
  }
};

namespace detail {
template <typename>
constexpr bool is_unbounded_array_v = false;
template <typename T>
constexpr bool is_unbounded_array_v<T[]> = true;

template <typename>
constexpr bool is_bounded_array_v = false;
template <typename T, std::size_t N>
constexpr bool is_bounded_array_v<T[N]> = true;

template <typename>
constexpr size_t bounded_array_size_v = 0;
template <typename T, std::size_t N>
constexpr size_t bounded_array_size_v<T[N]> = N;

template <typename T>
struct Deleter {
  void operator()(T *ptr) { ut::delete_(ptr); }
};

template <typename T>
struct Array_deleter {
  void operator()(T *ptr) { ut::delete_arr(ptr); }
};

template <typename T>
struct Aligned_deleter {
  void operator()(T *ptr) { ut::aligned_delete(ptr); }
};

template <typename T>
struct Aligned_array_deleter {
  void operator()(T *ptr) { ut::aligned_delete_arr(ptr); }
};

}  // namespace detail

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to T instance into the
    std::unique_ptr.

    This overload participates in overload resolution only if T
    is not an array type.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::unique_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Deleter<T>, typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, Deleter>>
make_unique(Args &&... args) {
  return std::unique_ptr<T, Deleter>(ut::new_<T>(std::forward<Args>(args)...));
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to T instance into the
    std::unique_ptr with custom deleter which knows how to handle PFS-enabled
    dynamic memory allocations. Instruments the memory with given PSI memory key
    in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is not an array type.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::unique_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Deleter<T>, typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, Deleter>>
make_unique(PSI_memory_key_t key, Args &&... args) {
  return std::unique_ptr<T, Deleter>(
      ut::new_withkey<T>(key, std::forward<Args>(args)...));
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instance
    into the std::unique_ptr.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @return std::unique_ptr holding a pointer to an array of size instances of
   T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::unique_ptr<T, Deleter>>
make_unique(size_t size) {
  return std::unique_ptr<T, Deleter>(
      ut::new_arr<std::remove_extent_t<T>>(ut::Count{size}));
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instances
    into the std::unique_ptr with custom deleter which knows how to handle
    PFS-enabled dynamic memory allocations. Instruments the memory with given
    PSI memory key in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    @return std::unique_ptr holding a pointer to an array of size instances of
   T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::unique_ptr<T, Deleter>>
make_unique(PSI_memory_key_t key, size_t size) {
  return std::unique_ptr<T, Deleter>(
      ut::new_arr_withkey<std::remove_extent_t<T>>(key, ut::Count{size}));
}

/** std::unique_ptr for arrays of known compile-time bound are disallowed.

    For more details see 4.3 paragraph from
    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3588.txt
 */
template <typename T, typename... Args>
std::enable_if_t<detail::is_bounded_array_v<T>> make_unique(Args &&...) =
    delete;

/** std::unique_ptr in PFS-enabled builds for arrays of known compile-time bound
    are disallowed.

    For more details see 4.3 paragraph from
    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3588.txt
 */
template <typename T, typename... Args>
std::enable_if_t<detail::is_bounded_array_v<T>> make_unique(
    PSI_memory_key_t key, Args &&...) = delete;

/** The following is a common type that is returned by all the ut::make_unique
    (non-aligned) specializations listed above. This is effectively a if-ladder
    for the following list of conditions on the input type:
    !std::is_array<T>::value -> std::unique_ptr<T, detail::Deleter<T>>
    detail::is_unbounded_array_v<T> ->
      std::unique_ptr<T,detail::Array_deleter<std::remove_extent_t<T>>> else (or
    else if detail::is_bounded_array_v<T>) -> void (we do not support bounded
     array ut::make_unique)
 */
template <typename T>
using unique_ptr = std::conditional_t<
    !std::is_array<T>::value, std::unique_ptr<T, detail::Deleter<T>>,
    std::conditional_t<
        detail::is_unbounded_array_v<T>,
        std::unique_ptr<T, detail::Array_deleter<std::remove_extent_t<T>>>,
        void>>;

/** Dynamically allocates storage for an object of type T at address aligned to
    the requested alignment. Constructs the object of type T with provided Args.
    Wraps the pointer to T instance into the std::unique_ptr.

    This overload participates in overload resolution only if T
    is not an array type.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::unique_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Aligned_deleter<T>,
          typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, Deleter>>
make_unique_aligned(size_t alignment, Args &&... args) {
  return std::unique_ptr<T, Deleter>(
      ut::aligned_new<T>(alignment, std::forward<Args>(args)...));
}

/** Dynamically allocates storage for an array of objects of type T at address
    aligned to the requested alignment. Constructs the object of type T with
    provided Args. Wraps the pointer to T instance into the std::unique_ptr with
    custom deleter which knows how to handle PFS-enabled dynamic memory
    allocations. Instruments the memory with given PSI memory key in case PFS
    memory support is enabled.

    This overload participates in overload resolution only if T is not an array
    type.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::unique_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Aligned_deleter<T>,
          typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::unique_ptr<T, Deleter>>
make_unique_aligned(PSI_memory_key_t key, size_t alignment, Args &&... args) {
  return std::unique_ptr<T, Deleter>(
      ut::aligned_new_withkey<T>(key, alignment, std::forward<Args>(args)...));
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T at address aligned to the requested alignment. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instance
    into the std::unique_ptr.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] size Size of the array of objects T to allocate.
    @return std::unique_ptr holding a pointer to an array of size instances of
   T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::unique_ptr<T, Deleter>>
make_unique_aligned(size_t alignment, size_t size) {
  return std::unique_ptr<T, Deleter>(
      ut::aligned_new_arr<std::remove_extent_t<T>>(alignment, ut::Count{size}));
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T at address aligned to the requested alignment. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instances
    into the std::unique_ptr with custom deleter which knows how to handle
    PFS-enabled dynamic memory allocations. Instruments the memory with given
    PSI memory key in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] size Size of the array of objects T to allocate.
    @return std::unique_ptr holding a pointer to an array of size instances of
   T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::unique_ptr<T, Deleter>>
make_unique_aligned(PSI_memory_key_t key, size_t alignment, size_t size) {
  return std::unique_ptr<T, Deleter>(
      ut::aligned_new_arr_withkey<std::remove_extent_t<T>>(key, alignment,
                                                           ut::Count{size}));
}

/** std::unique_ptr for arrays of known compile-time bound are disallowed.

    For more details see 4.3 paragraph from
    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3588.txt
 */
template <typename T, typename... Args>
std::enable_if_t<detail::is_bounded_array_v<T>> make_unique_aligned(
    Args &&...) = delete;

/** std::unique_ptr in PFS-enabled builds for arrays of known compile-time bound
    are disallowed.

    For more details see 4.3 paragraph from
    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3588.txt
 */
template <typename T, typename... Args>
std::enable_if_t<detail::is_bounded_array_v<T>> make_unique_aligned(
    PSI_memory_key_t key, Args &&...) = delete;

/** The following is a common type that is returned by all the
    ut::make_unique_aligned (non-aligned) specializations listed above. This is
    effectively a if-ladder for the following list of conditions on the input
    type: !std::is_array<T>::value -> std::unique_ptr<T,
   detail::Aligned_deleter<T>> detail::is_unbounded_array_v<T> ->
      std::unique_ptr<T,detail::Aligned_array_deleter<std::remove_extent_t<T>>>
   else (or else if detail::is_bounded_array_v<T>) -> void (we do not support
   bounded array ut::make_unique)
 */
template <typename T>
using unique_ptr_aligned = std::conditional_t<
    !std::is_array<T>::value, std::unique_ptr<T, detail::Aligned_deleter<T>>,
    std::conditional_t<detail::is_unbounded_array_v<T>,
                       std::unique_ptr<T, detail::Aligned_array_deleter<
                                              std::remove_extent_t<T>>>,
                       void>>;

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to T instance into the
    std::shared_ptr.

    This overload participates in overload resolution only if T
    is not an array type.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::shared_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Deleter<T>, typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::shared_ptr<T>> make_shared(
    Args &&... args) {
  return std::shared_ptr<T>(ut::new_<T>(std::forward<Args>(args)...),
                            Deleter{});
}

/** Dynamically allocates storage for an object of type T. Constructs the object
    of type T with provided Args. Wraps the pointer to T instance into the
    std::shared_ptr with custom deleter which knows how to handle PFS-enabled
    dynamic memory allocations. Instruments the memory with given PSI memory key
    in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is not an array type.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::shared_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Deleter<T>, typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::shared_ptr<T>> make_shared(
    PSI_memory_key_t key, Args &&... args) {
  return std::shared_ptr<T>(
      ut::new_withkey<T>(key, std::forward<Args>(args)...), Deleter{});
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T. Constructs the object of type T with provided Args. Wraps the
    pointer to an array of T instance into the std::shared_ptr.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] size Size of the array of objects T to allocate.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::shared_ptr<T>>
make_shared(size_t size) {
  return std::shared_ptr<T>(
      ut::new_arr<std::remove_extent_t<T>>(ut::Count{size}), Deleter{});
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T. Constructs the object of type T with provided Args. Wraps the
    pointer to an array of T instances into the std::shared_ptr with custom
    deleter which knows how to handle PFS-enabled dynamic memory allocations.
    Instruments the memory with given PSI memory key in case PFS memory support
    is enabled.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] size Size of the array of objects T to allocate.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::shared_ptr<T>>
make_shared(PSI_memory_key_t key, size_t size) {
  return std::shared_ptr<T>(
      ut::new_arr_withkey<std::remove_extent_t<T>>(key, ut::Count{size}),
      Deleter{});
}

/** Dynamically allocates storage for an array of objects of type T. Constructs
    the object of type T with provided Args. Wraps the pointer to an array of T
    instance into the std::shared_ptr.

    This overload participates in overload resolution only if T
    is an array type with known compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_bounded_array_v<T>, std::shared_ptr<T>>
make_shared() {
  return std::shared_ptr<T>(ut::new_arr<std::remove_extent_t<T>>(
                                ut::Count{detail::bounded_array_size_v<T>}),
                            Deleter{});
}

/** Dynamically allocates storage for an array of objects of type T. Constructs
    the object of type T with provided Args. Wraps the pointer to an array of T
    instances into the std::shared_ptr with custom deleter which knows how to
    handle PFS-enabled dynamic memory allocations. Instruments the memory with
    given PSI memory key in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is an array type with known compile-time bound.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T,
          typename Deleter = detail::Array_deleter<std::remove_extent_t<T>>>
std::enable_if_t<detail::is_bounded_array_v<T>, std::shared_ptr<T>> make_shared(
    PSI_memory_key_t key) {
  return std::shared_ptr<T>(
      ut::new_arr_withkey<std::remove_extent_t<T>>(
          key, ut::Count{detail::bounded_array_size_v<T>}),
      Deleter{});
}

/** Dynamically allocates storage for an object of type T at address aligned to
    the requested alignment. Constructs the object of type T with provided Args.
    Wraps the pointer to T instance into the std::shared_ptr.

    This overload participates in overload resolution only if T
    is not an array type.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::shared_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Aligned_deleter<T>,
          typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::shared_ptr<T>>
make_shared_aligned(size_t alignment, Args &&... args) {
  return std::shared_ptr<T>(
      ut::aligned_new<T>(alignment, std::forward<Args>(args)...), Deleter{});
}

/** Dynamically allocates storage for an object of type T at address aligned to
    the requested alignment. Constructs the object of type T with provided Args.
    Wraps the pointer to T instance into the std::shared_ptr with custom deleter
    which knows how to handle PFS-enabled dynamic memory allocations.
    Instruments the memory with given PSI memory key in case PFS memory support
    is enabled.

    This overload participates in overload resolution only if T
    is not an array type.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] args Arguments one wishes to pass over to T constructor(s) .
    @return std::shared_ptr holding a pointer to instance of T.
 */
template <typename T, typename Deleter = detail::Aligned_deleter<T>,
          typename... Args>
std::enable_if_t<!std::is_array<T>::value, std::shared_ptr<T>>
make_shared_aligned(PSI_memory_key_t key, size_t alignment, Args &&... args) {
  return std::shared_ptr<T>(
      ut::aligned_new_withkey<T>(key, alignment, std::forward<Args>(args)...),
      Deleter{});
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T at address aligned to the requested alignment. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instance
    into the std::shared_ptr.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] size Size of the array of objects T to allocate.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::shared_ptr<T>>
make_shared_aligned(size_t alignment, size_t size) {
  return std::shared_ptr<T>(
      ut::aligned_new_arr<std::remove_extent_t<T>>(alignment, ut::Count{size}),
      Deleter{});
}

/** Dynamically allocates storage for an array of requested size of objects of
    type T at address aligned to the requested alignment. Constructs the object
    of type T with provided Args. Wraps the pointer to an array of T instances
    into the std::shared_ptr with custom deleter which knows how to handle
    PFS-enabled dynamic memory allocations. Instruments the memory with given
    PSI memory key in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is an array type with unknown compile-time bound.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @param[in] size Size of the array of objects T to allocate.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_unbounded_array_v<T>, std::shared_ptr<T>>
make_shared_aligned(PSI_memory_key_t key, size_t alignment, size_t size) {
  return std::shared_ptr<T>(
      ut::aligned_new_arr_withkey<std::remove_extent_t<T>>(key, alignment,
                                                           ut::Count{size}),
      Deleter{});
}

/** Dynamically allocates storage for an array of objects of type T at address
    aligned to the requested alignment. Constructs the object of type T with
    provided Args. Wraps the pointer to an array of T instance into the
    std::shared_ptr.

    This overload participates in overload resolution only if T
    is an array type with known compile-time bound.

    NOTE: Given that this function will _NOT_ be instrumenting the allocation
    through PFS, observability for particular parts of the system which want to
    use it will be lost or in best case inaccurate. Please have a strong reason
    to do so.

    @param[in] alignment Alignment requirement for storage to be allocated.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_bounded_array_v<T>, std::shared_ptr<T>>
make_shared_aligned(size_t alignment) {
  return std::shared_ptr<T>(
      ut::aligned_new_arr<std::remove_extent_t<T>>(
          alignment, ut::Count{detail::bounded_array_size_v<T>}),
      Deleter{});
}

/** Dynamically allocates storage for an array of objects of type T at address
    aligned to the requested alignment. Constructs the object of type T with
    provided Args. Wraps the pointer to an array of T instances into the
    std::shared_ptr with custom deleter which knows how to handle PFS-enabled
    dynamic memory allocations. Instruments the memory with given PSI memory key
    in case PFS memory support is enabled.

    This overload participates in overload resolution only if T
    is an array type with known compile-time bound.

    @param[in] key PSI memory key to be used for PFS memory instrumentation.
    @param[in] alignment Alignment requirement for storage to be allocated.
    @return std::shared_ptr holding a pointer to an array of size instances of
    T.
 */
template <typename T, typename Deleter = detail::Aligned_array_deleter<
                          std::remove_extent_t<T>>>
std::enable_if_t<detail::is_bounded_array_v<T>, std::shared_ptr<T>>
make_shared_aligned(PSI_memory_key_t key, size_t alignment) {
  return std::shared_ptr<T>(
      ut::aligned_new_arr_withkey<std::remove_extent_t<T>>(
          key, alignment, ut::Count{detail::bounded_array_size_v<T>}),
      Deleter{});
}

/** Specialization of basic_ostringstream which uses ut::allocator. Please note
    that it's .str() method returns std::basic_string which is not std::string,
    so it has similar API (in particular .c_str()), but you can't assign it to
    regular, std::string.
 */
using ostringstream =
    std::basic_ostringstream<char, std::char_traits<char>, ut::allocator<char>>;

/** Specialization of vector which uses allocator. */
template <typename T>
using vector = std::vector<T, ut::allocator<T>>;

/** Specialization of list which uses ut_allocator. */
template <typename T>
using list = std::list<T, ut::allocator<T>>;

/** Specialization of set which uses ut_allocator. */
template <typename Key, typename Compare = std::less<Key>>
using set = std::set<Key, Compare, ut::allocator<Key>>;

template <typename Key>
using unordered_set =
    std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>,
                       ut::allocator<Key>>;

/** Specialization of map which uses ut_allocator. */
template <typename Key, typename Value, typename Compare = std::less<Key>>
using map =
    std::map<Key, Value, Compare, ut::allocator<std::pair<const Key, Value>>>;

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename Key_equal = std::equal_to<Key>>
using unordered_map =
    std::unordered_map<Key, Value, Hash, Key_equal,
                       ut::allocator<std::pair<const Key, Value>>>;

}  // namespace ut

#endif /* ut0new_h */
