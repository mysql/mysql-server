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

#ifndef MYSQL_CSA_CHANNEL_H
#define MYSQL_CSA_CHANNEL_H

#include "sql/rpl_replica_commit_order_manager.h"

namespace mysql::csa {

/// @class Channel
/// @brief Represents a channel in the Change Stream Applier (CSA).
class Channel {
 protected:
  /// The name of the channel.
  const std::string m_name;
  /// Unique id of the channel
  unsigned int m_instance_id{0};
  /// Pointer to the commit order manager.
  Commit_order_manager *m_commit_order_manager;

 public:
  /// Constructor
  /// @param name Name of the channel
  /// @param instance_id Unique id of the channel
  /// @param commit_order_manager Pointer to the commit order manager
  Channel(const std::string &name, unsigned int instance_id,
          Commit_order_manager *commit_order_manager);
  /// Destructor
  virtual ~Channel();

  /// Obtains the name of the channel
  /// @return Channel name
  const std::string &get_name() const;
  /// Commit order manager accessor
  /// @return Pointer to the commit order manager instance for this channel
  Commit_order_manager *get_commit_order_manager() const;
  unsigned int get_channel_id() const;
};

}  // namespace mysql::csa

#endif
