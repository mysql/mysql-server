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

#include <optional>

/**
  Determines if the current process is running in a container.
  @return true if running in either cgroup v1 or cgroup v2, false otherwise
*/
bool is_running_in_cgroup();

/**
  Determines if cgroup restricts resources
  @return true if cgroup restricts resources like memory
*/
bool does_cgroup_limit_resources();

/**
  Read the memory limit set by the container. Try cgroup v2, and then cgroup v1
  @return memory limit set by cgroup v2 or cgroup v1; or 0
  @note Return value of 0 implies either no limits are set or server is not
  running in a container
*/
uint64_t my_cgroup_mem_limit();

/**
  Find number of VCPUs as seen by the current process based on the
  affinity between each process and VCPU.
*/
uint32_t num_vcpus_using_affinity();

/**
  Get the number of VCPUS based on system configuration.
*/
uint32_t num_vcpus_using_config();

/**
  Get the number of VCPU.
*/
uint32_t my_system_num_vcpus();
