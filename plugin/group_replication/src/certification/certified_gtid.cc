
// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

#include "plugin/group_replication/include/certification/certified_gtid.h"

namespace gr {

Certified_gtid::Certified_gtid(const Gtid &server_gtid, const Gtid &group_gtid,
                               bool is_gtid_specified, bool is_local,
                               const Certification_result &certificate)
    : m_server_gtid(server_gtid),
      m_group_gtid(group_gtid),
      m_is_local(is_local),
      m_is_gtid_specified(is_gtid_specified),
      m_cert(certificate) {}

Certified_gtid::Certified_gtid(bool is_gtid_specified, bool is_local)
    : m_is_local(is_local), m_is_gtid_specified(is_gtid_specified) {
  m_server_gtid.clear();
  m_group_gtid.clear();
}

const Gtid &Certified_gtid::get_server_gtid() const { return m_server_gtid; }

const Gtid &Certified_gtid::get_group_gtid() const { return m_group_gtid; }

const Certification_result &Certified_gtid::get_cert_result() const {
  return m_cert;
}

bool Certified_gtid::is_specified_gtid() const { return m_is_gtid_specified; }

bool Certified_gtid::is_local() const { return m_is_local; }

}  // namespace gr
