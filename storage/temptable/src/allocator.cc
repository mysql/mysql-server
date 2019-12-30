/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/allocator.cc
TempTable custom allocator implementation. */

#include <atomic>  /* std::atomic */
#include <cstddef> /* size_t */
#include <cstdint> /* uint8_t */

#include "storage/temptable/include/temptable/allocator.h"

namespace temptable {

/** RAII-managed Allocator thread-resources cleanup class */
struct End_thread {
  ~End_thread() {
    if (!shared_block.is_empty()) {
      shared_block.destroy();
    }
  }
};

/** Thread-local variable whose destruction will make sure that
 *  shared memory-block will be destroyed. */
static thread_local End_thread end_thread;

/* Global shared memory-block. */
thread_local Block shared_block;

/* Initialization of MemoryMonitor static variables. */
std::atomic<size_t> MemoryMonitor::ram(0);

} /* namespace temptable */
