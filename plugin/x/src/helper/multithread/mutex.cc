/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/helper/multithread/mutex.h"

#include "my_compiler.h"

namespace xpl {

Mutex::Mutex(PSI_mutex_key key) { mysql_mutex_init(key, &m_mutex, nullptr); }
Mutex::~Mutex() { mysql_mutex_destroy(&m_mutex); }

Mutex::operator mysql_mutex_t *() { return &m_mutex; }

void Mutex::lock() { mysql_mutex_lock(&m_mutex); }

bool Mutex::try_lock() { return mysql_mutex_trylock(&m_mutex); }

void Mutex::unlock() { mysql_mutex_unlock(&m_mutex); }

}  // namespace xpl
