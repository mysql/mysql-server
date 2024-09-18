/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "mysql/components/library_mysys/option_usage_data.h"
#include <atomic>
#include <ctime>
#include <string>
#include "my_rapidjson_size_t.h"
#include "mysql/components/my_service.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/services/registry.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

const size_t small_usage_data_size = 4096;
const size_t max_usage_data_size = 65536;

bool Option_usage_data::set(bool is_used) {
  my_service<SERVICE_TYPE(mysql_option_tracker_usage)> usage(
      "mysql_option_tracker_usage", m_registry);
  if (!usage.is_valid()) return false;

  // parse incoming data, if any
  char usage_data[small_usage_data_size];
  std::unique_ptr<char[]> buffer(nullptr);
  try {
    rapidjson::Document doc;
    if (!usage->get(m_option_name, usage_data, sizeof(usage_data))) {
      if (doc.ParseInsitu(usage_data).HasParseError()) return true;
    } else {  // assume the data buffer is too small and try one more time
      buffer.reset(new (std::nothrow) char[max_usage_data_size]);
      if (!usage->get(m_option_name, buffer.get(), max_usage_data_size)) {
        if (doc.ParseInsitu(buffer.get()).HasParseError()) return true;
      }
    }

    // make sure it's an object
    if (!doc.IsObject()) doc.SetObject();

    auto it = doc.FindMember("used");
    // set the new values
    if (it == doc.MemberEnd())
      doc.AddMember("used", is_used, doc.GetAllocator());
    else
      it->value.SetBool(is_used);

    time_t now;
    time(&now);
    char curent_iso8601_datetime[sizeof("2011-10-08T07:07:09Z")];
    strftime(curent_iso8601_datetime, sizeof curent_iso8601_datetime, "%FT%TZ",
             gmtime(&now));

    it = doc.FindMember("usedDate");
    if (it == doc.MemberEnd())
      doc.AddMember("usedDate", rapidjson::StringRef(curent_iso8601_datetime),
                    doc.GetAllocator());
    else
      it->value.SetString(curent_iso8601_datetime, doc.GetAllocator());

    // serialize and store
    rapidjson::StringBuffer upated_json;
    upated_json.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> string_writer(upated_json);
    doc.Accept(string_writer);
    buffer.reset(nullptr);
    return usage->set(m_option_name, upated_json.GetString());
  } catch (...) {
    return true;
  }
}

bool Option_usage_data::set_sampled(bool is_used,
                                    unsigned long log_usage_every_nth_time) {
  // guard to do it only once
  if ((m_counter.fetch_add(1, std::memory_order_relaxed) %
       log_usage_every_nth_time) != 0)
    return false;
  return set(is_used);
}
