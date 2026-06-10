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

#include "mysql/components/library_mysys/option_tracker_usage.h"
#include <atomic>
#include <ctime>
#include <string>
#include "my_rapidjson_size_t.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/services/mysql_simple_error_log.h"
#include "mysql/components/services/registry.h"
#include "mysqld_error.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

const size_t small_usage_data_size = 8192;
const size_t max_usage_data_size = 65536;

static bool report_warning_func(SERVICE_TYPE(registry) * registry,
                                const char *option_name, const char *reason,
                                const char *file, unsigned long line) {
  if (registry != nullptr) {
    my_service<SERVICE_TYPE(mysql_simple_error_log)> const errlog(
        "mysql_simple_error_log", registry);
    if (errlog.is_valid()) {
      errlog->emit("option_usage_read_counter", file, line,
                   MYSQL_ERROR_LOG_SEVERITY_INFORMATION,
                   ER_WARN_OPTION_USAGE_COUNTER_READ_FAILED, option_name,
                   reason);
    }
  }
  return true;
}

#define report_warning(registry, option_name, reason) \
  report_warning_func(registry, option_name, reason, __FILE__, __LINE__)

bool option_usage_set_counter_from_json(SERVICE_TYPE(registry) * registry,
                                        const char *option_name,
                                        char *usage_data,
                                        unsigned long long *pCounter) {
  assert(option_name);
  assert(*option_name);
  assert(pCounter);
  assert(usage_data);
  try {
    if (*usage_data == 0)
      return report_warning(registry, option_name,
                            "Option usage persisted data is empty");
    rapidjson::Document doc;
    if (doc.ParseInsitu(usage_data).HasParseError()) {
      report_warning(registry, option_name, usage_data);
      return report_warning(registry, option_name,
                            "Option usage persisted data are not valid JSON");
    }

    // make sure it's an object
    if (!doc.IsObject()) {
      report_warning(registry, option_name, usage_data);
      return report_warning(
          registry, option_name,
          "Option usage persisted data are not a JSON object");
    }

    auto it = doc.FindMember("usedCounter");
    if (it == doc.MemberEnd() || !it->value.IsUint64()) {
      /*
        If we don't find "usedCounter", we look for "used" in case it's an old
        format. We treat used=true as 1.
      */
      auto it2 = doc.FindMember("used");
      if (it2 != doc.MemberEnd() && it2->value.IsBool()) {
        *pCounter = it2->value.GetBool() ? 1 : 0;
        return false;
      }

      report_warning(registry, option_name, usage_data);
      return report_warning(
          registry, option_name,
          "Option usage persisted data do not contain usedCounter or used");
    }

    *pCounter = it->value.GetUint64();
  } catch (...) {
    return report_warning(
        registry, option_name,
        "Exception ocurred handling option usage persisted data");
  }
  return false;
}

bool option_usage_register_callback(
    const char *option_name,
    mysql_option_tracker_usage_cache_update_callback cb,
    SERVICE_TYPE(registry) * registry) {
  my_service<SERVICE_TYPE(mysql_option_tracker_usage_cache_callbacks)> const
      cbsvc("mysql_option_tracker_usage_cache_callbacks", registry);
  if (!cbsvc.is_valid()) {
    return report_warning(registry, option_name,
                          "No mysql_option_tracker_usage_cache_callbacks "
                          "service defined at register");
  }
  return cbsvc->add(option_name, cb) != 0;
}

bool option_usage_unregister_callback(
    const char *option_name,
    mysql_option_tracker_usage_cache_update_callback cb,
    SERVICE_TYPE(registry) * registry) {
  my_service<SERVICE_TYPE(mysql_option_tracker_usage_cache_callbacks)> const
      cbsvc("mysql_option_tracker_usage_cache_callbacks", registry);
  if (!cbsvc.is_valid()) {
    return report_warning(registry, option_name,
                          "No mysql_option_tracker_usage_cache_callbacks "
                          "service defined at unregister");
  }
  return cbsvc->remove(option_name, cb) != 0;
}

bool option_usage_read_counter(const char *option_name,
                               unsigned long long *pCounter,
                               SERVICE_TYPE(registry) * registry) {
  assert(registry);

  my_service<SERVICE_TYPE(mysql_option_tracker_usage)> const usage(
      "mysql_option_tracker_usage", registry);
  if (!usage.is_valid()) {
    return report_warning(registry, option_name,
                          "No option_tracker_usage service defined");
  }

  // read the data
  char usage_data[small_usage_data_size], *p_usage_data = &usage_data[0];
  std::unique_ptr<char[]> buffer(nullptr);
  if (0 != usage->get(option_name, usage_data, sizeof(usage_data))) {
    buffer.reset(new (std::nothrow) char[max_usage_data_size]);
    if (0 != usage->get(option_name, buffer.get(), max_usage_data_size)) {
      report_warning(registry, option_name, "Can't read the option usage data");
      return false;
    }
    p_usage_data = buffer.get();
  }

  option_usage_set_counter_from_json(registry, option_name, p_usage_data,
                                     pCounter);
  return false;
}
