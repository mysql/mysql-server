/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_SYNODE_H
#define GCS_XCOM_SYNODE_H

#include <sstream>
#include <unordered_set>
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"

/**
 Defines a message identifier so that joining members can fetch the associated
 packet from a remote node.

 See Gcs_xcom_communication_interface::recover_packets.

 It wraps synode_no so we can use it in hash-based containers.
 */
class Gcs_xcom_synode {
 public:
  Gcs_xcom_synode() noexcept;
  explicit Gcs_xcom_synode(synode_no synod) noexcept;

  ~Gcs_xcom_synode();

  Gcs_xcom_synode(Gcs_xcom_synode const &other) = default;
  Gcs_xcom_synode &operator=(Gcs_xcom_synode const &other) = default;

  Gcs_xcom_synode(Gcs_xcom_synode &&other) noexcept;
  Gcs_xcom_synode &operator=(Gcs_xcom_synode &&other) noexcept;

  bool operator==(const Gcs_xcom_synode &other) const;

  synode_no const &get_synod() const;

 private:
  synode_no synode_;
};

/*
 Specialization of std::hash<Gcs_xcom_synode> so we can use it in hash-based
 containers.
 */
namespace std {
template <>
struct hash<Gcs_xcom_synode> {
  std::size_t operator()(Gcs_xcom_synode const &s) const noexcept {
    /*
     Represent the synode as a string of the format g<gid>m<msg_nr>n<node_nr> to
     serve as the hash function argument.
     */
    std::hash<std::string> hash_function;

    std::ostringstream formatter;
    formatter << "g" << s.get_synod().group_id << "m" << s.get_synod().msgno
              << "n" << s.get_synod().node;
    auto representation = formatter.str();

    return hash_function(representation);
  }
};
}  // namespace std

using Gcs_xcom_synode_set = std::unordered_set<Gcs_xcom_synode>;

#endif  // GCS_XCOM_SYNODE_H
