/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <cstdint>
#include <string>

using ulonglong = unsigned long long;

/**
  Initialize the my_physical_memory function using server_memory option
  @param[in]  memory  Value of the server_memory startup option

  @note The input value of 0 indicates no limits, and underlying container/host
  configuration must be used
  @return true on success, false if input memory value is invalid
*/
bool init_my_physical_memory(ulonglong memory);

/**
  Overloaded function for easy use.
  @param[in]  memory  string containing value of server_memory

  @return true on success, false if input memory value is invalid
*/
bool init_my_physical_memory(const std::string &memory);

/**
  Determine the total physical memory available in bytes.

  If process is running within a container, then memory available is the maximum
  limit set for the container. If the process is not running in a container then
  it uses the appropriate system APIs to determine the available memory.

  If the API is unable to determine the available memory, then it returns 0.

   @return physical memory in bytes or 0
*/
uint64_t my_physical_memory() noexcept;

/**
  Determine the total number of logical CPUs available to be used by the mysql
  server process.

  This API uses the process affinity to calculate the number of logical CPUs. If
  this method fails, then the API calls the corresponding system API to retrieve
  the number of logical CPUs. If this method fails too, then the API calls the
  C++ standard API hardware_concurrency.

  If the API is unable to determine the number of logical CPUs, then it returns
  0.

  @return number of logical CPUs or 0
*/
uint32_t my_num_vcpus() noexcept;

/**
  Determine if resource limits set by container must be respected and checks the
  correctness of the configurations. Initializes internal state with input

  @param[in]  is_container_aware  true if container config must be respected
  @return true if container configurations are correct, false otherwise
*/
bool init_container_aware(const bool is_container_aware) noexcept;

/**
  Overloaded function for easy use.

  @param[in]  is_container_aware  string considered to be true if "ON", false
  otherwise
  @return true if container configurations are correct, false otherwise
*/
bool init_container_aware(const std::string &is_container_aware) noexcept;

/**
  Release internal state and deinitialize container awareness
*/
void deinit_container_aware() noexcept;

/**
  Determines if container configurations has set resource limits
  @return true if container configuration resource limits
*/
bool has_container_resource_limits();
