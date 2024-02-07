/*****************************************************************************

Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file handler/p_s.h
 InnoDB performance_schema tables interface to MySQL.

 *******************************************************/

#ifndef p_s_h
#define p_s_h

#include "mysql/psi/psi_data_lock.h"

/** Inspect data locks in innodb.
This class is used by the performance schema to extract lock data.
*/
class Innodb_data_lock_inspector : public PSI_engine_data_lock_inspector {
 public:
  Innodb_data_lock_inspector() = default;
  ~Innodb_data_lock_inspector() override = default;

  PSI_engine_data_lock_iterator *create_data_lock_iterator() override;
  PSI_engine_data_lock_wait_iterator *create_data_lock_wait_iterator() override;
  void destroy_data_lock_iterator(PSI_engine_data_lock_iterator *it) override;
  void destroy_data_lock_wait_iterator(
      PSI_engine_data_lock_wait_iterator *it) override;
};

#endif /* p_s_h */
