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

#include <sched.h>
#include <unistd.h>
#include <cstdint>

#include "my_system_api.h"

/**
  @file components/library_mysys/my_system_api/my_system_api_linux.cc
  Functions to fetch the number of VCPUs from the system. APIs retrieve this
  information using the affinity between the process and the VCPU or by reading
  the system configuration
*/

uint32_t num_vcpus_using_affinity() {
  cpu_set_t cs;
  CPU_ZERO(&cs);

  if (sched_getaffinity(getpid(), sizeof(cs), &cs) != 0) {
    return 0;
  }

  return CPU_COUNT(&cs);
}

uint32_t num_vcpus_using_config() {
  uint32_t num_vcpus = 0;

#ifdef _SC_NPROCESSORS_ONLN
  num_vcpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROCESSORS_CONF)
  num_vcpus = sysconf(_SC_NPROCESSORS_CONF);
#endif

  return num_vcpus;
}
