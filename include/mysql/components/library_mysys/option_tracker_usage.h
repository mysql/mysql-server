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

#ifndef OPTION_TRACKER_USAGE_H
#define OPTION_TRACKER_USAGE_H

#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/services/registry.h"

/**
  @brief A helper function to read the current value of a option usage counter

  Fetches a reference to the option_tracker service.
  Retrieves the usage JSON for the option, parses it, fetches
  the usedCounter field and stores it into the pointer passed.
  Then disposes of the reference.

  Returns true if there's any error.

  @retval true failure
  @retval false success
  @param option_name the name of the option to retrieve data for
  @param[out] pCounter the buffer to return the value into
  @param registry A reference to the registry service to use
*/
extern bool option_usage_read_counter(const char *option_name,
                                      unsigned long long *pCounter,
                                      SERVICE_TYPE(registry) * registry);
/**
   @brief Registers a cache update callback

   Call this to subscribe to persisted data updates

   @param option_name the name of the option to register a callback for
   @param cb the callback function
   @param registry A reference to the registry service to use
   @retval true failure
   @retval false sucees
 */
extern bool option_usage_register_callback(
    const char *option_name,
    mysql_option_tracker_usage_cache_update_callback cb,
    SERVICE_TYPE(registry) * registry);

/**
   @brief Unregisters a cache update callback

   Call this to unsubscribe to persisted data updates

   @param option_name the name of the option to unregister a callback for
   @param cb the callback function
   @param registry A reference to the registry service to use
   @retval true failure
   @retval false sucees
 */
extern bool option_usage_unregister_callback(
    const char *option_name,
    mysql_option_tracker_usage_cache_update_callback cb,
    SERVICE_TYPE(registry) * registry);

/**
  @brief A helper function to parse a JSON string and extract the counter
  value

  Given the usage JSON for the option, parses it, fetches
  the usedCounter field and stores it into the pointer passed.

  Returns true if there's any error.

  @retval true failure
  @retval false success
  @param registry A reference to the registry service to use to put errors
  and warnings. can be null.
  @param option_name the name of the option that JSON usage is for
  @param usage_data a JSON string for the usage data
  @param[out] pCounter the buffer to return the value into
*/
extern bool option_usage_set_counter_from_json(SERVICE_TYPE(registry) *
                                                   registry,
                                               const char *option_name,
                                               char *usage_data,
                                               unsigned long long *pCounter);
#endif /* OPTION_TRACKER_USAGE_H */
