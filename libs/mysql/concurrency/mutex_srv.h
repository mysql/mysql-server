// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CONCURRENCY_MUTEX_SRV_H
#define MYSQL_CONCURRENCY_MUTEX_SRV_H

#ifdef MYSQL_CONCURRENCY_MUTEX_STL_H
#error Inclusion of both mutex_stl.h and mutex_srv.h is prohibited.
#endif

#include "mysql/concurrency/mutex_wrapper.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"

#define MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(key) key
#define MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(key) key

namespace mysql::concurrency {

using Mutex = Mutex_wrapper;
using Mutex_key = PSI_mutex_key;

}  // namespace mysql::concurrency

#endif  // MYSQL_CONCURRENCY_MUTEX_SRV_H
