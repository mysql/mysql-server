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

#include "sql/reference_caching_setup.h"

#include "sql/mysqld.h" /* srv_registry */
#include "sql/sql_audit.h"

/**
  All reference caching channels maintained by server
  to handle event tracking broadcasts.
*/
Event_reference_caching_channels *g_event_channels{nullptr};

const size_t MAX_EVENT_PER_CHANNEL = 6;

/**
  Services handled by each channel.
  We group multiple services so that in case of
  cache miss, all required references are refreshed
  in one go.
*/
const char *channel_set[][MAX_EVENT_PER_CHANNEL] = {
    /* set 0 */
    {event_tracking_names[static_cast<size_t>(Event_tracking_class::COMMAND)],
     event_tracking_names[static_cast<size_t>(
         Event_tracking_class::CONNECTION)],
     event_tracking_names[static_cast<size_t>(Event_tracking_class::GENERAL)],
     event_tracking_names[static_cast<size_t>(Event_tracking_class::QUERY)],
     event_tracking_names[static_cast<size_t>(
         Event_tracking_class::TABLE_ACCESS)],
     nullptr},
    /* set 1 */
    {event_tracking_names[static_cast<size_t>(
         Event_tracking_class::GLOBAL_VARIABLE)],
     nullptr},
    /* set 2 */
    {event_tracking_names[static_cast<size_t>(
         Event_tracking_class::AUTHENTICATION)],
     nullptr},
    /* set 3 */
    {event_tracking_names[static_cast<size_t>(Event_tracking_class::MESSAGE)],
     nullptr},
    /* set 4 */
    {event_tracking_names[static_cast<size_t>(Event_tracking_class::PARSE)],
     nullptr},
    /* set 5 */
    {event_tracking_names[static_cast<size_t>(Event_tracking_class::STARTUP)],
     nullptr},
    /* set 6 */
    {event_tracking_names[static_cast<size_t>(
         Event_tracking_class::STORED_PROGRAM)],
     nullptr},
    /* Always Last */
    {nullptr}};

/**
  A Mapping to identify channel number and offset within
  the channel for a given event tracking service.
*/
std::pair<size_t, size_t> channel_set_mapping[] = {
    {2, 0} /* Event_tracking_class::AUTHENTICATION */,
    {0, 0} /* Event_tracking_class::COMMAND */,
    {0, 1} /* Event_tracking_class::CONNECTION */,
    {0, 2} /* Event_tracking_class::GENERAL */,
    {1, 0} /* Event_tracking_class::GLOBAL_VARIABLE */,
    {3, 0} /* Event_tracking_class::MESSAGE */,
    {4, 0} /* Event_tracking_class::PARSE */,
    {0, 3} /* Event_tracking_class::QUERY */,
    {5, 0} /* Event_tracking_class::SHUTDOWN */,
    {5, 0} /* Event_tracking_class::STARTUP */,
    {6, 0} /* Event_tracking_class::STORED_PROGRAM */,
    {0, 4} /* Event_tracking_class::TABLE_ACCESS */,
};

Event_reference_caching_channels::Event_mapping::Event_mapping() {
  for (size_t index = 0;
       index < static_cast<size_t>(Event_tracking_class::LAST); ++index) {
    event_map_[event_tracking_names[index]] = index;
  }
}

bool Event_reference_caching_channels::Event_mapping::map(
    const std::string &event, std::pair<size_t, size_t> &index) {
  try {
    size_t at = event_map_.at(event);
    index = channel_set_mapping[at];
    return false;
  } catch (...) {
    return true;
  }
}

bool Event_reference_caching_channels::Event_mapping::map(
    const std::string &event, size_t &index) {
  try {
    index = event_map_.at(event);
    return false;
  } catch (...) {
    return true;
  }
}

Event_reference_caching_channels::~Event_reference_caching_channels() {
  deinit();
  if (srv_registry != nullptr) {
    (void)srv_registry->release(
        reinterpret_cast<my_h_service>(reference_caching_channel_service_));
    (void)srv_registry->release(
        reinterpret_cast<my_h_service>(reference_caching_cache_service_));
    (void)srv_registry->release(
        reinterpret_cast<my_h_service>(reference_caching_channel_ignore_list_));
  }
}

bool Event_reference_caching_channels::init() {
  assert(srv_registry);
  if (srv_registry->acquire("reference_caching_channel",
                            reinterpret_cast<my_h_service *>(
                                &reference_caching_channel_service_)) ||
      srv_registry->acquire("reference_caching_cache",
                            reinterpret_cast<my_h_service *>(
                                &reference_caching_cache_service_)) ||
      srv_registry->acquire("reference_caching_channel_ignore_list",
                            reinterpret_cast<my_h_service *>(
                                &reference_caching_channel_ignore_list_))) {
    return true;
  }
  reference_caching_channel one_channel{nullptr};
  for (size_t index = 0; *channel_set[index] != nullptr; ++index) {
    if (reference_caching_channel_service_->create(channel_set[index],
                                                   &one_channel) ||
        reference_caching_channel_ignore_list_->add(one_channel,
                                                    "mysql_server")) {
      return true;
    }
    channels_.push_back(one_channel);
  }

  valid_ = true;
  return false;
}

void Event_reference_caching_channels::deinit() {
  if (reference_caching_channel_service_ == nullptr) return;
  for (auto &one_channel : channels_) {
    reference_caching_channel_service_->destroy(one_channel);
    one_channel = nullptr;
  }
  channels_.clear();
}

Event_reference_caching_channels *Event_reference_caching_channels::create() {
  auto obj = new (std::nothrow) Event_reference_caching_channels();
  if (obj) obj->init();
  return obj;
}

bool Event_reference_caching_channels::create_cache(
    Cache_vector &cache_vector) {
  if (!valid_) return true;

  for (auto one_channel : channels_) {
    reference_caching_cache one_cache;
    if (reference_caching_cache_service_->create(one_channel, srv_registry,
                                                 &one_cache)) {
      return true;
    }
    cache_vector.push_back(one_cache);
  }
  return false;
}

bool Event_reference_caching_channels::service_notification(const char *service,
                                                            bool load) {
  const char *dot = strchr(service, '.');
  std::string service_name(service, static_cast<size_t>(dot - service));
  size_t index;
  if (!map(service_name, index)) {
    if (load) {
      ++service_counters_[index];
    } else {
      /*
        Following is thread safe because persistent dynamic loader takes
        a mutex as a part of each UNINSTALL COMPONENT statement.
      */
      if (service_counters_[index].load() > 0) --service_counters_[index];
    }
    return false;
  }
  return true;
}

Event_reference_caching_cache::Event_reference_caching_cache() {
  if (g_event_channels == nullptr || !g_event_channels->valid()) return;
  reference_caching_cache_service_ =
      g_event_channels->get_reference_caching_cache_handle();
  valid_ = !g_event_channels->create_cache(local_cache_vector_);
  if (!valid_) deinit();
}

Event_reference_caching_cache::~Event_reference_caching_cache() { deinit(); }

void Event_reference_caching_cache::deinit() {
  if (local_cache_vector_.empty()) return;

  for (auto one_cache : local_cache_vector_) {
    reference_caching_cache_service_->destroy(one_cache);
  }
  local_cache_vector_.clear();
  valid_ = false;
}

bool Event_reference_caching_cache::get(
    Event_tracking_class event_tracking_class, const my_h_service **services) {
  if (!valid_ || event_tracking_class == Event_tracking_class::LAST)
    return true;

  if (!g_event_channels->service_exists(event_tracking_class)) return true;

  auto index = channel_set_mapping[static_cast<size_t>(event_tracking_class)];

  return reference_caching_cache_service_->get(local_cache_vector_[index.first],
                                               index.second, services);
}

void Event_reference_caching_cache::refresh_all() {
  if (valid_) {
    for (auto &one : local_cache_vector_) {
      const my_h_service *services{nullptr};
      (void)reference_caching_cache_service_->get(one, 0, &services);
    }
  }
}
