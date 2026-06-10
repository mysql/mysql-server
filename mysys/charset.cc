/* Copyright (c) 2000, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/charset.cc
*/

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>

#include "my_config.h"

#include "m_string.h"
#include "map_helpers.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/my_loglevel.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/collations.h"
#include "mysql/strings/int2str.h"
#include "mysql/strings/m_ctype.h"
#include "mysys/mysys_priv.h"
#include "mysys_err.h"
#include "nulls.h"
#include "strings/collations_internal.h"
#include "strmake.h"
#include "strxmov.h"

#ifdef MYSQL_SERVER
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#endif

using myf = int;

/*
  The code below implements this functionality:

    - Initializing charset related structures
    - Loading dynamic charsets
    - Searching for a proper CHARSET_INFO
      using charset name, collation name or collation ID
    - Setting server default character set
*/

namespace {

class Mysys_charset_loader : public MY_CHARSET_LOADER {
 public:
  Mysys_charset_loader(const Mysys_charset_loader &) = delete;
  Mysys_charset_loader(const Mysys_charset_loader &&) = delete;
  Mysys_charset_loader &operator=(const Mysys_charset_loader &) = delete;
  Mysys_charset_loader &operator=(const Mysys_charset_loader &&) = delete;

  Mysys_charset_loader() = default;
  ~Mysys_charset_loader() override = default;

  void reporter(loglevel level, uint errcode, ...) override {
    va_list args;
    va_start(args, errcode);
    my_charset_error_reporter(level, errcode, args);
    va_end(args);
  }

  void *once_alloc(size_t sz) override {
    return my_once_alloc(sz, MYF(MY_WME));
  }

  void *mem_malloc(size_t sz) override {
    return my_malloc(key_memory_charset_loader, sz, MYF(MY_WME));
  }

  void mem_free(void *ptr) override { my_free(ptr); }

  void *read_file(const char *path, size_t *size) override;
};

}  // namespace

static mysql::collation_internals::Collations *entry() {
  return mysql::collation_internals::entry;
}

/**
  Report character set initialization errors and warnings.
  Be silent by default: no warnings on the client side.
*/
static void default_reporter(loglevel /*unused*/, uint errcode [[maybe_unused]],
                             va_list /* unused */) {}
my_error_vreporter my_charset_error_reporter = default_reporter;

constexpr size_t MY_MAX_ALLOWED_BUF = static_cast<size_t>(1024) * 1024;

const char *charsets_dir = nullptr;

void *Mysys_charset_loader::read_file(const char *path, size_t *size) {
  MY_STAT stat_info{};
  if (!my_stat(path, &stat_info, 0)) {
    return nullptr;
  }

  size_t const len = stat_info.st_size;
  if (len > MY_MAX_ALLOWED_BUF) {
    return nullptr;
  }

  // NOLINTNEXTLINE we *do* use a smart pointer here.
  unique_ptr_free<uint8_t> buf(static_cast<uint8_t *>(malloc(len)));
  if (buf == nullptr) {
    return nullptr;
  }

  int const fd = mysql_file_open(key_file_charset, path, O_RDONLY, 0);
  if (fd < 0) {
    return nullptr;
  }

  size_t const tmp_len = mysql_file_read(fd, buf.get(), len, 0);
  mysql_file_close(fd, 0);
  if (tmp_len != len) {
    return nullptr;
  }

  *size = len;
  return buf.release();
}

char *get_charsets_dir(char *buf) {
  const char *sharedir = SHAREDIR;
  DBUG_TRACE;

  if (charsets_dir != nullptr)
    strmake(buf, charsets_dir, FN_REFLEN - 1);
  else {
    if (test_if_hard_path(sharedir) ||
        is_prefix(sharedir, DEFAULT_CHARSET_HOME))
      strxmov(buf, sharedir, "/", CHARSET_DIR, NullS);
    else
      strxmov(buf, DEFAULT_CHARSET_HOME, "/", sharedir, "/", CHARSET_DIR,
              NullS);
  }
  char *res = convert_dirname(buf, buf, NullS);
  DBUG_PRINT("info", ("charsets dir: '%s'", buf));
  return res;
}

const CHARSET_INFO *all_charsets[MY_ALL_CHARSETS_SIZE] = {nullptr};
CHARSET_INFO *default_charset_info = &my_charset_latin1;

static std::once_flag charsets_initialized;

static Mysys_charset_loader *loader = nullptr;

static void init_available_charsets() {
  assert(loader == nullptr);
  loader = new Mysys_charset_loader;

  char charset_dir[FN_REFLEN];
  get_charsets_dir(charset_dir);

  mysql::collation::initialize(charset_dir, loader);
  entry()->iterate(
      [](const CHARSET_INFO *cs) { all_charsets[cs->number] = cs; });
}

uint get_collation_number(const char *collation_name) {
  std::call_once(charsets_initialized, init_available_charsets);
  mysql::collation::Name const name{collation_name};
  return entry()->get_collation_id(name);
}

unsigned get_charset_number(const char *cs_name, uint cs_flags) {
  std::call_once(charsets_initialized, init_available_charsets);
  mysql::collation::Name const name{cs_name};
  if ((cs_flags & MY_CS_PRIMARY)) {
    return entry()->get_primary_collation_id(name);
  }
  if ((cs_flags & MY_CS_BINSORT)) {
    return entry()->get_default_binary_collation_id(name);
  }
  assert(false);
  return 0;
}

const char *get_collation_name(uint charset_number) {
  std::call_once(charsets_initialized, init_available_charsets);

  CHARSET_INFO *cs = entry()->find_by_id(charset_number);
  if (cs != nullptr) {
    assert(cs->number == charset_number);
    assert(cs->m_coll_name != nullptr);
    return cs->m_coll_name;
  }

  return "?"; /* this mimics find_type() */
}

CHARSET_INFO *get_charset(uint cs_number, myf flags) {
  std::call_once(charsets_initialized, init_available_charsets);

  if (cs_number == default_charset_info->number) return default_charset_info;

  if (cs_number == 0 || cs_number >= MY_ALL_CHARSETS_SIZE) {
    return nullptr;
  }

  CHARSET_INFO *cs = entry()->find_by_id(cs_number);
  if (!cs && (flags & MY_WME)) {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    constexpr int cs_string_size = 23;
    char cs_string[cs_string_size];
    my_stpcpy(get_charsets_dir(index_file), MY_CHARSET_INDEX);
    cs_string[0] = '#';
    longlong10_to_str(cs_number, cs_string + 1, 10);
    my_error(EE_UNKNOWN_CHARSET, MYF(0), cs_string, index_file);
  }
  return cs;
}

namespace {

bool starts_with_utf8(const char *name) {
  // See normalization of names in mysql::collation::Name::Name()
  const uint8_t *map = my_charset_latin1.to_lower;
  constexpr const char prefix[] = "UTF8_";
  if (strlen(name) < strlen(prefix)) return false;
  for (unsigned int ix = 0; ix < strlen(prefix); ++ix) {
    if (map[static_cast<uint8_t>(name[ix])] !=
        map[static_cast<uint8_t>(prefix[ix])]) {
      return false;
    }
  }
  return true;
}

/**
  What does "utf8" mean, "utf8mb3" or "utf8mb4"?

  We ask 'current_thd' whether MODE_INTERPRET_UTF8_AS_UTF8MB4 has been
  set in SQL_MODE.

  We return 3 or 4 to indicate what alias is active.  A special value
  42 is used during bootstrap: reject command line arguments, and
  abort. There are no lookups during shutdown.

  For client-side requests (e.g. mysql --default-character-set=utf8)
  we always return 3.

  @param [in] cname Charset/Collation name, used during bootstrap
              to give the desired error message.

  @return 3,4,42 to indicate what alias to use, or to abort.
 */
int utf8_alias_lookup(const char *cname [[maybe_unused]]) {
#ifdef MYSQL_SERVER
  enum_server_operational_state server_state = get_server_state();
  if (server_state == SERVER_BOOTING) {
    if (native_strcasecmp(cname, "utf8") == 0) {
      LogErr(ERROR_LEVEL, ER_INVALID_SERVER_OPTION_CHARSET);
      return 42;  // abort
    }
    if (native_strncasecmp(cname, "utf8_", 5) == 0) {
      LogErr(ERROR_LEVEL, ER_INVALID_SERVER_OPTION_COLLATION, cname);
      return 42;  // abort
    }
    assert(false);
    return 3;
  }
  if (server_state == SERVER_OPERATING) {
    assert(current_thd != nullptr);
    return (current_thd->interpret_utf8_as_utf8mb4() ? 4 : 3);
  }
  if (server_state == SERVER_SHUTTING_DOWN) {
    assert(false);
    return 42;
  }
  assert(false);
  return 42;
#else
  // We do not want to report an error
  //    Character set 'utf8' is not a compiled character set ...
  // whenever a client is invoked with --default-character-set=utf8
  return 3;
#endif  // MYSQL_SERVER
}

}  // namespace

/**
  Find collation by name: extended version of get_charset_by_name()
  to return error messages to the caller.
  @param [in]  collation_name Collation name
  @param [in]  flags   Flags
  @param [out] errmsg  Error message buffer (if any)

  @return          NULL on error, pointer to collation on success
*/

CHARSET_INFO *my_collation_get_by_name(const char *collation_name, myf flags,
                                       MY_CHARSET_ERRMSG *errmsg) {
  std::call_once(charsets_initialized, init_available_charsets);

  std::string collation_name_string(collation_name);
  if (starts_with_utf8(collation_name)) {
    int retval = utf8_alias_lookup(collation_name);
    // abort, assume that utf8_alias_lookup has logged something
    if (retval == 42) return nullptr;
    assert(retval == 3 || retval == 4);

    // Insert "mb3" to get "utf8mb3_xxxx" or
    //        "mb4" to get "utf8mb4_xxxx"
    // These have no utf8mb4 counterparts:
    //   utf8mb3_general_mysql500_ci
    //   utf8mb3_tolower_ci
    // So for retval 4 we return an error.
    const char *mb3_or_mb4 = nullptr;
    if (retval == 4) {
      // See normalization of names in mysql::collation::Name::Name()
      if (0 == my_strcasecmp(&my_charset_latin1, collation_name,
                             "utf8_general_mysql500_ci") ||
          0 == my_strcasecmp(&my_charset_latin1, collation_name,
                             "utf8_tolower_ci")) {
        errmsg->errcode = EE_COLLATION_ALIAS_ERROR;
        collation_name_string.insert(4, "mb3");
        snprintf(errmsg->errarg, sizeof(errmsg->errarg),
                 "Collation '%s' must be specified explicitly as '%s'",
                 collation_name, collation_name_string.c_str());
        return nullptr;
      }
      mb3_or_mb4 = "mb4";
    } else {
      mb3_or_mb4 = "mb3";
    }

    collation_name_string.insert(4, mb3_or_mb4);
    collation_name = collation_name_string.c_str();
  }

  mysql::collation::Name const name{collation_name};
  CHARSET_INFO *cs = entry()->find_by_name(name, flags, errmsg);
  if (cs == nullptr && (flags & MY_WME)) {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    my_stpcpy(get_charsets_dir(index_file), MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_COLLATION, MYF(0),
             std::string(name.to_string_view()).c_str(), index_file);
  }
  return cs;
}

CHARSET_INFO *get_charset_by_name(const char *collation_name, myf flags) {
  MY_CHARSET_ERRMSG dummy;
  return my_collation_get_by_name(collation_name, flags, &dummy);
}

/**
  Find character set by name: extended version of get_charset_by_csname()
  to return error messages to the caller.
  @param [in]  cs_name  Collation name
  @param [in]  cs_flags Character set flags (e.g. default or binary collation)
  @param [in]  flags    Flags
  @param [out] errmsg   Error message buffer (if any)

  @return           NULL on error, pointer to collation on success
*/
CHARSET_INFO *my_charset_get_by_name(const char *cs_name, uint cs_flags,
                                     myf flags, MY_CHARSET_ERRMSG *errmsg) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("name: '%s'", cs_name));

  std::call_once(charsets_initialized, init_available_charsets);

  mysql::collation::Name const name{cs_name};
  CHARSET_INFO *cs = nullptr;
  if (cs_flags & MY_CS_PRIMARY) {
    cs = entry()->find_primary(name, flags, errmsg);

    if (cs == nullptr && name.to_string_view() == "utf8") {
      int retval = utf8_alias_lookup("utf8");
      // abort, assume that utf8_alias_lookup has logged something
      if (retval == 42) return nullptr;
      assert(retval == 3 || retval == 4);

      // The parser does get_charset_by_csname().
      // Also needed for e.g. SET character_set_client= 'utf8'.
      // Also needed by the lexer for: "select _utf8 0xD0B0D0B1D0B2;"
      cs = entry()->find_primary(
          mysql::collation::Name(retval == 4 ? "utf8mb4" : "utf8mb3"), flags,
          errmsg);
    }
  } else if (cs_flags & MY_CS_BINSORT) {
    cs = entry()->find_default_binary(name, flags, errmsg);
    if (cs == nullptr && name.to_string_view() == "utf8") {
      int retval = utf8_alias_lookup("utf8");
      if (retval == 42) return nullptr;
      assert(retval == 3 || retval == 4);

      cs = entry()->find_default_binary(
          mysql::collation::Name(retval == 4 ? "utf8mb4" : "utf8mb3"), flags,
          errmsg);
    }
  }
  if (!cs && (flags & MY_WME)) {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    my_stpcpy(get_charsets_dir(index_file), MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(0), cs_name, index_file);
  }

  return cs;
}

CHARSET_INFO *get_charset_by_csname(const char *cs_name, uint cs_flags,
                                    myf flags) {
  MY_CHARSET_ERRMSG dummy;
  return my_charset_get_by_name(cs_name, cs_flags, flags, &dummy);
}

/**
  Resolve character set by the character set name (utf8, latin1, ...).

  The function tries to resolve character set by the specified name. If
  there is character set with the given name, it is assigned to the "cs"
  parameter and false is returned. If there is no such character set,
  "default_cs" is assigned to the "cs" and true is returned.

  @param[in] cs_name    Character set name.
  @param[in] default_cs Default character set.
  @param[out] cs        Variable to store character set.

  @return false if character set was resolved successfully; true if there
  is no character set with given name.
*/

bool resolve_charset(const char *cs_name, const CHARSET_INFO *default_cs,
                     const CHARSET_INFO **cs) {
  *cs = get_charset_by_csname(cs_name, MY_CS_PRIMARY, MYF(0));

  if (*cs == nullptr) {
    *cs = default_cs;
    return true;
  }

  return false;
}

/**
  Resolve collation by the collation name (utf8_general_ci, ...).

  The function tries to resolve collation by the specified name. If there
  is collation with the given name, it is assigned to the "cl" parameter
  and false is returned. If there is no such collation, "default_cl" is
  assigned to the "cl" and true is returned.

  @param[in] cl_name    Collation name.
  @param[in] default_cl Default collation.
  @param[out] cl        Variable to store collation.

  @return false if collation was resolved successfully; true if there is no
  collation with given name.
*/

bool resolve_collation(const char *cl_name, const CHARSET_INFO *default_cl,
                       const CHARSET_INFO **cl) {
  *cl = get_charset_by_name(cl_name, MYF(0));

  if (*cl == nullptr) {
    *cl = default_cl;
    return true;
  }

  return false;
}

#ifdef _WIN32
extern CHARSET_INFO my_charset_cp932_japanese_ci;

static CHARSET_INFO *fs_cset_cache = nullptr;

CHARSET_INFO *fs_character_set() {
  if (fs_cset_cache == nullptr) {
    char buf[10] = "cp";
    GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, buf + 2,
                  sizeof(buf) - 3);
    /*
      We cannot call get_charset_by_name here,
      we will end up in a deadlock (in std::call_once) because of recursion:
        init-avaliable-charsets ->
        get_charsets_dir ->
        convert_dirname ->
        fs_character_set
    */
    fs_cset_cache =
        !strcmp(buf, "cp932") ? &my_charset_cp932_japanese_ci : &my_charset_bin;
  }
  return fs_cset_cache;
}
#endif

void charset_uninit() {
  mysql::collation::shutdown();

  delete loader;
  loader = nullptr;

  new (&charsets_initialized) std::once_flag;
}
