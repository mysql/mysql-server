/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_synode.h"

Gcs_xcom_synode::Gcs_xcom_synode() noexcept : synode_(null_synode) {}

Gcs_xcom_synode::Gcs_xcom_synode(synode_no synod) noexcept : synode_(synod) {}

Gcs_xcom_synode::~Gcs_xcom_synode() = default;

Gcs_xcom_synode::Gcs_xcom_synode(Gcs_xcom_synode &&other) noexcept
    : synode_(other.synode_) {
  other.synode_ = null_synode;
}
Gcs_xcom_synode &Gcs_xcom_synode::operator=(Gcs_xcom_synode &&other) noexcept {
  synode_ = other.synode_;
  other.synode_ = null_synode;

  return *this;
}

bool Gcs_xcom_synode::operator==(const Gcs_xcom_synode &other) const {
  return (synode_eq(synode_, other.synode_) == 1);
}

synode_no const &Gcs_xcom_synode::get_synod() const { return synode_; }
