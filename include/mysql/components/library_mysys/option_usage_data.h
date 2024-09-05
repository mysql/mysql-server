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

#ifndef OPTION_USAGE_DATA_H
#define OPTION_USAGE_DATA_H

#include <atomic>
#include "mysql/components/service.h"
#include "mysql/components/services/registry.h"

/**
  @brief A helper class to handle option usage population

  Adds a top level JSON object (if missing), then adds (if missing, otherwise
  updates) the following two JSON elements to the top level JSON object:
    * used (boolean)
    * usedDate (ISO 8601 string)


  Create an instance of the class at component/plugin init time and
  dispose of it at deinit time as follows:

  @code
  ...
  #include "mysql/components/library_mysys/option_usage_data.h"
  ...
  Option_usage_data *handlerton{nullptr};
  ...
  init() {
    ...
    usage_data = new Option_usage_data("feature name",
  SERVICE_PLACEHOLDER(registry)); if (!usage_data) return 1;
    ...
  }
  ...
  deinit() {
    ..
    delete usage_data;
    usage_data = nullptr;
    ..
  }
  @endcode

  Now, when the functionality is used (careful, an expensive call), do:

  @code
  usage_data->set_sampled(true);
  @endcode

  Do not register usage at the time the plugin component is initilized.
  Try to register it at the time it is actually being used.
  But do not do it too often: it parses JSON and writes to an InnoDB table!
*/
class Option_usage_data {
 public:
  /**
    @brief Use this constructor at init time
    @param option_name The name of the option to register usage for
    @param registry a reference to the registry service
  */
  Option_usage_data(const char *option_name, SERVICE_TYPE(registry) * registry)
      : m_option_name(option_name), m_registry(registry), m_counter(0) {}
  Option_usage_data(Option_usage_data &) = delete;
  ~Option_usage_data() {}

  /**
    @brief Records usage.

    @param is_used True if the feature is used.
    @retval true Error
    @retval false Success
  */
  bool set(bool is_used);
  /**
    @brief Records usage (calls @ref Option_usage_data::set()) every Nth call

    Very useful for high volume of calls to the usage function.

    @param is_used True if the feature is used.
    @param log_usage_every_nth_time Log usage for every Nth call to this
    function.
    @retval true Error
    @retval false Success
  */
  bool set_sampled(bool is_used, unsigned long log_usage_every_nth_time);

 protected:
  const char *m_option_name;
  SERVICE_TYPE(registry) * m_registry;
  std::atomic<unsigned> m_counter;
};

#endif /* OPTION_USAGE_DATA_H */
