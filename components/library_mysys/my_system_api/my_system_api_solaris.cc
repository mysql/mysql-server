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

#include "my_config.h"  // HAVE_UNISTD_H

#include <sys/pset.h>
#include <sys/types.h>
#include <cstdint>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_system_api.h"

/**
  @file components/library_mysys/my_system_api/my_system_api_solaris.cc
  Functions to fetch the number of VCPUs from the system. APIs retrieve this
  information using the affinity between the process and the VCPU or by reading
  the system configuration
*/

uint32_t num_vcpus_using_affinity() {
  uint32_t num_vcpus = 0;
  pid_t pid = getpid();
  psetid_t pset = PS_NONE;

  if (pset_bind(PS_QUERY, P_PID, pid, &pset) == 0 && pset != PS_NONE) {
    pset_info(pset, nullptr, &num_vcpus, nullptr);
  }
  return num_vcpus;
}

uint32_t num_vcpus_using_config() {
  uint32_t num_vcpus = 0;

  pset_info(P_MYID, nullptr, &num_vcpus, nullptr);
  return num_vcpus;
}
