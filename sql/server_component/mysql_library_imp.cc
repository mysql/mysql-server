/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mysql_library_imp.h"
#include <string_view>
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/types/library.h"
#include "sql/sp.h"
#include "sql/sp_head.h"  // sp_head

constexpr auto MYSQL_SUCCESS = 0;
constexpr auto MYSQL_FAILURE = 1;

static auto acquire_library(THD *thd, std::string_view schema,
                            std::string_view name) -> const dd::Library * {
  if (schema.empty() || name.empty()) return {};  // Error.

  // Ensure null-terminated strings.
  assert(schema.data()[schema.length()] == '\0');
  assert(name.data()[name.length()] == '\0');

  auto access_error = check_routine_access(
      thd, EXECUTE_ACL, schema.data(), name.data(), Acl_type::LIBRARY, true);
  if (access_error) return {};

  auto result = static_cast<const dd::Library *>(nullptr);
  if (thd->dd_client()->acquire<dd::Library>(schema.data(), name.data(),
                                             &result) == MYSQL_FAILURE)
    return {};
  return result;
}

class Mysql_library {
  THD *m_thd;
  bool m_exists{false};
  bool m_is_binary{false};
  std::string_view m_body;
  std::string_view m_language;
  MDL_ticket *m_lock{nullptr};

 public:
  Mysql_library(THD *thd, std::string_view schema,
                std::string_view library_name,
                [[maybe_unused]] std::string_view version)
      : m_thd{thd} {
    // Lock the library.
    auto mdl_key = MDL_key{};
    dd::Library::create_mdl_key(schema.data(), library_name.data(), &mdl_key);

    // MDL Lock request on the routine.
    auto library_request = MDL_request{};
    MDL_REQUEST_INIT_BY_KEY(&library_request, &mdl_key, MDL_SHARED_READ,
                            MDL_EXPLICIT);

    // Acquire an MDL lock.
    if (thd->mdl_context.acquire_lock(&library_request,
                                      thd->variables.lock_wait_timeout) ==
        MYSQL_FAILURE)
      return;
    m_lock = library_request.ticket;
    if (!m_lock) return;

    // Get the Library from the data dictionary.
    const auto releaser =
        dd::cache::Dictionary_client::Auto_releaser{thd->dd_client()};
    const auto library = acquire_library(thd, schema, library_name);
    if (!library) return;  // The library does not exist.

    m_body = library->definition();
    m_language = library->external_language();
    m_exists = true;
    dd::Object_id collation = library->client_collation_id();
    m_is_binary = collation == my_charset_bin.number;
    // Existent libraries should be locked.
    assert(m_lock);
  }

  ~Mysql_library() {
    // The libraries that do not exist, are not locked.
    assert(!m_exists || m_lock);
    if (m_lock) m_thd->mdl_context.release_lock(m_lock);
    m_lock = nullptr;
  }

  [[nodiscard]] operator bool() const {
    if (!m_exists) return false;
    assert(m_lock);
    if (!m_lock) return false;
    if (m_body.empty()) return false;
    if (m_language.empty()) return false;
    return true;
  }

  [[nodiscard]] auto get_body() const -> std::string_view {
    assert(*this);
    return m_body;
  }
  [[nodiscard]] auto get_language() const -> std::string_view {
    assert(*this);
    return m_language;
  }
  [[nodiscard]] auto is_binary() const -> bool {
    assert(*this);
    return m_is_binary;
  }
};

/// Component service interface implementation.

DEFINE_BOOL_METHOD(mysql_library_imp::exists,
                   (MYSQL_THD thd, mysql_cstring_with_length schema_name,
                    mysql_cstring_with_length library_name,
                    mysql_cstring_with_length version, bool *result)) {
  assert(result);
  if (!result) return MYSQL_FAILURE;
  *result = false;
  try {
    if (!thd) thd = current_thd;
    auto library = Mysql_library{thd,
                                 {schema_name.str, schema_name.length},
                                 {library_name.str, library_name.length},
                                 {version.str, version.length}};
    if (!library) return MYSQL_FAILURE;
    *result = bool{library};
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_library_imp::init,
                   (MYSQL_THD thd, mysql_cstring_with_length schema_name,
                    mysql_cstring_with_length library_name,
                    mysql_cstring_with_length version,
                    my_h_library *library_handle)) {
  assert(library_handle);
  if (!library_handle) return MYSQL_FAILURE;
  *library_handle = nullptr;
  try {
    if (!thd) thd = current_thd;
    auto library = new (thd->mem_root) Mysql_library(
        thd, std::string_view{schema_name.str, schema_name.length},
        std::string_view{library_name.str, library_name.length},
        std::string_view{version.str, version.length});
    if (!library) return MYSQL_FAILURE;
    *library_handle = reinterpret_cast<my_h_library>(library);
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_library_imp::deinit, (my_h_library library_handle)) {
  assert(library_handle);
  if (!library_handle) return MYSQL_FAILURE;
  try {
    auto library = reinterpret_cast<Mysql_library *>(library_handle);
    // The Library handle has been allocated in the THD's mem_root
    // and will be automatically deallocated. Just destroy the object,
    // to release the lock.
    library->~Mysql_library();
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_library_ext_imp::get_body,
                   (my_h_library library_handle,
                    mysql_cstring_with_length *body, bool *is_binary)) {
  assert(body);
  if (!body) return MYSQL_FAILURE;
  body->str = nullptr;
  body->length = 0;
  if (!library_handle) return MYSQL_FAILURE;

  try {
    auto library = reinterpret_cast<Mysql_library *>(library_handle);
    if (!*library) return MYSQL_FAILURE;  // Non-existent library.
    auto library_body = library->get_body();
    if (library_body.empty()) return MYSQL_FAILURE;
    body->str = library_body.data();
    body->length = library_body.length();
    if (is_binary) *is_binary = library->is_binary();
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_library_imp::get_body,
                   (my_h_library library_handle,
                    mysql_cstring_with_length *body)) {
  return mysql_library_ext_imp::get_body(library_handle, body, nullptr);
}

DEFINE_BOOL_METHOD(mysql_library_imp::get_language,
                   (my_h_library library_handle,
                    mysql_cstring_with_length *language)) {
  assert(language);
  if (!language) return MYSQL_FAILURE;
  language->str = nullptr;
  language->length = 0;
  if (!library_handle) return MYSQL_FAILURE;

  try {
    auto library = reinterpret_cast<Mysql_library *>(library_handle);
    if (!*library) return MYSQL_FAILURE;  // Non-existent library.
    auto library_language = library->get_language();
    if (library_language.empty()) return MYSQL_FAILURE;
    language->str = library_language.data();
    language->length = library_language.length();
  } catch (...) {
    return MYSQL_FAILURE;
  }
  return MYSQL_SUCCESS;
}
