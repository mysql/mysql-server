/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
*/

#pragma once

#include "fil0tablespace_node_handle_interface.h"
#include "fil0tablespaces_nodes_interface.h"

namespace ib::fil {

class Tablespaces_nodes final : public Tablespaces_nodes_interface {
 public:
  Tablespaces_nodes() = default;

  ~Tablespaces_nodes() override = default;

  [[nodiscard]] Capabilities get_capabilities() override;

  [[nodiscard]] ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
                             Create_error>
  create(Tablespace_id space_id, size_t node_order,
         const Create_node_hints &hints, uint32_t flags,
         page_no_t size_in_pages) override;

  [[nodiscard]] ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
                             Open_error>
  open(Tablespace_id space_id, size_t node_order, const Node_hints &hints,
       size_t page_size, bool for_read_only) override;

  [[nodiscard]] Status remove(Tablespace_id space_id, size_t node_order,
                              const Node_hints &hints) override;

  [[nodiscard]] Status rename(Tablespace_id space_id, size_t node_order,
                              const std::string &old_path,
                              const std::string &new_path) override;
  [[nodiscard]] ut::Expected<Node_info, Node_error> get_node_info(
      Tablespace_id space_id, size_t node_order, const Node_hints &hints,
      size_t page_size) override;

 private:
  /** Opens an UNDO tablespace using the supplied path in the @p hints.
  @param[in]   space_id            Tablespace ID to open the node's storage for.
  @param[in]   hints               Additional information that may be useful for
                                   opening the node's storage, including path to
                                   file.
  @param[in]   page_size           Physical page size used in the tablespace,
                                   must match one specified when creating the
                                   tablespace.
  @param[in]   for_read_only       True if the storage should be opened for
                                   read-only access.
  @return Pointer to handle to be used for accessing the node's storage if the
  storage is opened successfully, error code otherwise. */
  [[nodiscard]] ut::Expected<ut::unique_ptr<Tablespace_node_handle_interface>,
                             Open_error>
  open_undo_tablespace(space_id_t space_id, const Node_hints &hints,
                       size_t page_size, bool for_read_only);
};

} /* namespace ib::fil */
