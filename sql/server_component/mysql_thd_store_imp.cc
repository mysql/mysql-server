/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mysql_thd_store_imp.h"
#include "mysql_current_thread_reader_imp.h"

#include "mysql/components/services/log_builtins.h" /* LogErr */
#include "mysqld_error.h"
#include "rwlock_scoped_lock.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"

#include <string>
#include <vector>

mysql_rwlock_t LOCK_thd_store_data;
/** PSI key for @sa LOCK_thd_store_data */
static PSI_rwlock_key key_LOCK_thd_store_data;
/** PSI info for @sa LOCK_thd_store_data */
static PSI_rwlock_info info_LOCK_thd_store_data = {
    &key_LOCK_thd_store_data, "LOCK_thd_store_data", PSI_FLAG_SINGLETON, 0,
    "RW Lock protecting structure required for THD store service"};

namespace {
class Thd_store_data_service final {
 public:
  Thd_store_data_service() {
    mysql_rwlock_register("sql", &info_LOCK_thd_store_data, 1);
    mysql_rwlock_init(key_LOCK_thd_store_data, &LOCK_thd_store_data);
  }

  ~Thd_store_data_service() {
    mysql_rwlock_destroy(&LOCK_thd_store_data);
    vector_.clear();
  }

  unsigned int assign(const std::string &name, free_resource_fn free_fn) {
    const rwlock_scoped_lock lock(&LOCK_thd_store_data, true, __FILE__,
                                  __LINE__);
    auto value = std::make_pair(name, free_fn);
    vector_.push_back(value);
    auto index = vector_.size();
    LogErr(INFORMATION_LEVEL, ER_NOTE_COMPONENT_SLOT_REGISTRATION_SUCCESS,
           index - 1, name.c_str());
    return index - 1;
  }

  void unassign(unsigned int slot) {
    const rwlock_scoped_lock lock(&LOCK_thd_store_data, true, __FILE__,
                                  __LINE__);
    if (slot >= vector_.size() || !vector_[slot].first.length()) return;
    LogErr(INFORMATION_LEVEL, ER_NOTE_COMPONENT_SLOT_DEREGISTRATION_SUCCESS,
           slot, vector_[slot].first.c_str());
    vector_[slot].first.clear();
    vector_[slot].second = nullptr;
  }

  bool free_resource(THD *thd, std::unordered_map<unsigned, void *> &data) {
    bool retval = false;
    const rwlock_scoped_lock lock(&LOCK_thd_store_data, false, __FILE__,
                                  __LINE__);
    for (auto &element : data) {
      if (element.second != nullptr) {
        auto i = element.first;
        if (!vector_[i].first.length()) {
          retval = true;
        } else if (vector_[i].second && vector_[i].second(element.second)) {
          LogErr(WARNING_LEVEL,
                 ER_WARN_CANNOT_FREE_COMPONENT_DATA_DEALLOCATION_FAILED,
                 vector_[i].first.c_str(), thd->thread_id());
          retval = true;
        }
      }
    }
    data.clear();
    return retval;
  }

 private:
  std::vector<std::pair<std::string, free_resource_fn>> vector_;
};

Thd_store_data_service *g_thd_store_data_service{nullptr};

}  // namespace

void init_thd_store_service() {
  if (g_thd_store_data_service) return;
  g_thd_store_data_service = new Thd_store_data_service();
}

void deinit_thd_store_service() {
  if (!g_thd_store_data_service) return;
  delete g_thd_store_data_service;
  g_thd_store_data_service = nullptr;
}

bool free_thd_store_resource(THD *thd,
                             std::unordered_map<unsigned int, void *> &data) {
  if (!g_thd_store_data_service) return 1;
  return g_thd_store_data_service->free_resource(thd, data);
}

DEFINE_BOOL_METHOD(Mysql_thd_store_service_imp::register_slot,
                   (const char *name, free_resource_fn free_fn,
                    mysql_thd_store_slot *slot)) {
  try {
    if (g_thd_store_data_service == nullptr || slot == nullptr ||
        free_fn == nullptr)
      return true;
    unsigned int index = g_thd_store_data_service->assign(name, free_fn);
    unsigned int *slot_ptr = new unsigned int(index);
    if (slot_ptr == nullptr) return true;
    *slot = reinterpret_cast<mysql_thd_store_slot>(slot_ptr);
    return false;
  } catch (...) {
    return true;
  }
}

DEFINE_BOOL_METHOD(Mysql_thd_store_service_imp::unregister_slot,
                   (mysql_thd_store_slot slot)) {
  try {
    if (g_thd_store_data_service == nullptr || slot == nullptr) return true;
    unsigned int *slot_ptr = reinterpret_cast<unsigned int *>(slot);
    g_thd_store_data_service->unassign(*slot_ptr);
    delete slot_ptr;
    return false;
  } catch (...) {
    return true;
  }
}

DEFINE_BOOL_METHOD(Mysql_thd_store_service_imp::set,
                   (MYSQL_THD o_thd, mysql_thd_store_slot slot, void *object)) {
  try {
    if (slot == nullptr) return true;
    unsigned int *slot_ptr = reinterpret_cast<unsigned int *>(slot);
    auto thd = o_thd ? reinterpret_cast<THD *>(o_thd) : current_thd;
    return thd ? thd->add_external(*slot_ptr, object) : true;
  } catch (...) {
    return true;
  }
}

DEFINE_METHOD(void *, Mysql_thd_store_service_imp::get,
              (MYSQL_THD o_thd, mysql_thd_store_slot slot)) {
  try {
    if (slot == nullptr) return nullptr;
    unsigned int *slot_ptr = reinterpret_cast<unsigned int *>(slot);
    auto thd = o_thd ? reinterpret_cast<THD *>(o_thd) : current_thd;
    return thd ? thd->fetch_external(*slot_ptr) : nullptr;
  } catch (...) {
    return nullptr;
  }
}
