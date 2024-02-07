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

#ifndef SQL_REFERENCE_CACHING_SETUP_H
#define SQL_REFERENCE_CACHING_SETUP_H

#include <atomic>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mysql/components/services/reference_caching.h"
#include "mysql/plugin_audit.h"

#include "sql/sql_event_tracking_to_audit_event_mapping.h"

using Event_map = std::unordered_map<std::string, size_t>;
using Channel_vector = std::vector<reference_caching_channel>;
using Cache_vector = std::vector<reference_caching_cache>;

class Event_reference_caching_channels final {
 public:
  class Event_mapping final {
   public:
    Event_mapping();
    ~Event_mapping() {}
    bool map(const std::string &event, std::pair<size_t, size_t> &index);
    bool map(const std::string &event, size_t &index);

   private:
    Event_map event_map_;
  };

  static Event_reference_caching_channels *create();
  ~Event_reference_caching_channels();

  bool map(const std::string &event, std::pair<size_t, size_t> &index) {
    return event_mapping_.map(event, index);
  }

  bool map(const std::string &event, size_t &index) {
    return event_mapping_.map(event, index);
  }

  bool service_notification(const char *name, bool load);

  SERVICE_TYPE(reference_caching_cache) * get_reference_caching_cache_handle() {
    return reference_caching_cache_service_;
  }

  bool valid() const { return valid_; }

  bool create_cache(Cache_vector &cache_vector);

  bool service_exists(Event_tracking_class event_tracking_class) {
    return (
        service_counters_[static_cast<size_t>(event_tracking_class)].load() >
        0);
  }

 private:
  Event_reference_caching_channels() = default;
  bool init();
  void deinit();

  /** Validity */
  bool valid_{false};
  /** Handle to @sa reference_caching_channel service */
  SERVICE_TYPE_NO_CONST(reference_caching_channel)
  *reference_caching_channel_service_{nullptr};
  /** Handle to @sa reference_caching_cache service */
  SERVICE_TYPE_NO_CONST(reference_caching_cache)
  *reference_caching_cache_service_{nullptr};
  /** Handle to @sa reference_caching_channel_ignore_list */
  SERVICE_TYPE_NO_CONST(reference_caching_channel_ignore_list)
  *reference_caching_channel_ignore_list_{nullptr};

  /** Event map */
  Event_mapping event_mapping_;
  /** Reference caching channels */
  Channel_vector channels_;
  /** Service counters */
  std::atomic_int
      service_counters_[static_cast<size_t>(Event_tracking_class::LAST)];
};

extern Event_reference_caching_channels *g_event_channels;

class Event_reference_caching_cache final {
 public:
  Event_reference_caching_cache();
  ~Event_reference_caching_cache();
  bool get(Event_tracking_class event_tracking_class,
           const my_h_service **services);
  bool valid() const { return valid_; }
  void refresh_all();

 private:
  void deinit();
  bool valid_{false};
  Cache_vector local_cache_vector_;
  SERVICE_TYPE(reference_caching_cache) * reference_caching_cache_service_{
                                              nullptr};
};

#endif  // !SQL_REFERENCE_CACHING_SETUP_H
