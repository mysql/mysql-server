/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include <cstdlib> /* size_t */
#include <string>
#include "fil0tablespace_node_handle_interface.h"
#include "page0size.h"
#include "ut0expected.h" /* ut::Expected */
#include "ut0new.h"      /* ut::unique_ptr */

namespace ib::fil {

/** Manages low-level storage for Tablespaces. */
class Tablespaces_nodes_interface {
 public:
  /** Type used for giving the tablespaces unique number. */
  using Tablespace_id = uint32_t;

  /** Performs any clean up. */
  virtual ~Tablespaces_nodes_interface() = default;

  /*==========================================================
  Tablespace Service Implementation's Capabilities
  ==========================================================*/
  struct Capabilities {
    bool supports_dblwr;
    bool supports_import_tablespace;
    bool supports_discard_tablespace;
    bool supports_bulk_load;
    bool supports_clone;
    bool supports_transparent_data_encryption;
    bool supports_transparent_page_compression;
    bool supports_raw_devices;
  };

  /** Query the capabilities of the Page Service implementation.
  This function can be called even before start().
  @return the capabilities of this Page Service implementation */
  [[nodiscard]] virtual Capabilities get_capabilities() = 0;

  enum class Status { SUCCESS = 0, IO_ERROR };

  enum class Create_error {
    NODE_EXISTS = 1,
    FILE_NAME_TOO_LONG,
    OUT_OF_DISK_SPACE,
    IO_ERROR,
    UNSUPPORTED
  };

  /* If an API requires an additional hint then evaluate if you need to
  extend the Node_hints for that API to keep the APIs contract intact. */
  struct Node_hints {
    /** Path and filename of the node to process. */
    const std::string m_path;
    /** Specifies if the @p m_path is a raw device or disk partition. */
    const bool m_is_raw_disk = false;
    /** Specifies if get_node_info() should probe current access permissions.
    This may open the node storage and can be sharing-sensitive on Windows. */
    const bool m_check_permissions = false;
  };

  struct Create_node_hints {
    Node_hints m_base_hints;
    /** Specifies if the @p m_path is for an explicit undo tablespace. */
    const bool m_is_explicit_undo = false;
  };

  /** Creates a low-level storage for a new tablespace node.
  if storage 'path' already exists, it returns error TABLESPACE_EXISTS.
  To be used in fil_create_tablespace().
  @param[in]   space_id            Tablespace ID to create the node's storage
                                   for.
  @param[in]   node_order          Number of the node in the tablespace.
  @param[in]   hints               Additional information that may be useful for
                                   creating the node's storage.
  @param[in]   flags               Tablespace flags
  @param[in]   size_in_pages       Initial size of the tablespace node in pages,
                                   must be >= FIL_IBD_FILE_INITIAL_SIZE
  @return Pointer to handle to be used for accessing the node's storage if the
  storage is created successfully, error code otherwise. */
  [[nodiscard]] virtual ut::Expected<
      ut::unique_ptr<Tablespace_node_handle_interface>, Create_error>
  create(Tablespace_id space_id, size_t node_order,
         const Create_node_hints &hints, uint32_t flags,
         page_no_t size_in_pages) = 0;

  enum class Open_error {
    NODE_DOES_NOT_EXIST = 1,
    NO_ACCESS_PERMISSIONS,
    IO_ERROR
  };

  /** Opens an existing tablespace's node storage by @p path, so it is ready to
  read and write pages, flush and extend the tablespace node. The tablespace
  must not be opened at the time of this call.
  To be used in fil_node_t::open().
  @param[in]   space_id            Tablespace ID to open the node's storage for.
  @param[in]   node_order          Number of the node in the tablespace.
  @param[in]   hints               Additional information that may be useful for
                                   opening the node's storage.
  @param[in]   page_size           Physical page size used in the tablespace,
                                   must match one specified when creating the
                                   tablespace.
  @param[in]   for_read_only       True if the storage should be opened for
                                   read-only access.
  @return Pointer to handle to be used for accessing the node's storage if the
  storage is opened successfully, error code otherwise. */
  [[nodiscard]] virtual ut::Expected<
      ut::unique_ptr<Tablespace_node_handle_interface>, Open_error>
  open(Tablespace_id space_id, size_t node_order, const Node_hints &hints,
       size_t page_size, bool for_read_only) = 0;

  /** Rename a tablespace file.
  @param[in]  space_id  tablespace_id
  @param[in]  node_order Number of the node in the tablespace.
  @param[in]  old_path  old name of the file
  @param[in]  new_path  new name of the file
  @return SUCCESS if renames successfully, error code otherwise */
  [[nodiscard]] virtual Status rename(space_id_t space_id, size_t node_order,
                                      const std::string &old_path,
                                      const std::string &new_path) = 0;

  /** Deletes a specified tablespace's node storage by @p path.
  To be used in Fil_shard::space_delete().
  @param[in]      space_id        Tablespace ID
  @param[in]      node_order      Number of the node in the tablespace.
  @param[in]      hints           Additional information that may be useful for
                                  removing the node's storage.
  @return Status::SUCCESS if tablespace storage is removed successfully, error
  code otherwise. */
  [[nodiscard]] virtual Status remove(Tablespace_id space_id, size_t node_order,
                                      const Node_hints &hints) = 0;

  /** Errors that can be returned while getting information about a node. */
  enum class Node_error {
    /** The specified node does not exist and the hinted path is free to be
    reserved by any new node. */
    NODE_DOES_NOT_EXIST,
    /** The specified node does not exist, but the hinted path is used by
    something that is not a node and can't be used to create a new node. */
    NOT_A_NODE,
    /** A generic error occurred during the operation, the existence of the node
    could not be determined. */
    IO_ERROR
  };

  /** Stored general information about the storage node at the time of calling
  the get_node_info(). */
  struct Node_info {
    /** Size of the node in pages. */
    page_no_t size;
    /** Block size to use for IO in bytes. If transparent page compression is
    used, the buffer size for write_page()/write_page_async() must be multiple
    of this value. */
    uint32_t block_size;
    /** Actual file size that is, the amount of space allocated on the file
    system. */
    uint64_t alloc_size;
    /** Determines if caller would succeed to open the path if they tried now,
    with specified access modes. This value is reliable only when
    Node_hints::m_check_permissions was true. When a `false` permission is
    reported after an explicit permission check, it may mean that it is a
    temporary problem which may go away if another thread/process unlocks the
    file. This temporary problem is more common on Windows as the default
    setting is to lock the files when opening them. */
    Access_permissions access_permissions;
  };

  /** Gets basic information about the node specified, this includes its size
  (first page offset that is not available for reading and writing) and minimum
  block size for usage in transparent page compression. If
  Node_hints::m_check_permissions is true, currently available access modes are
  also checked; otherwise Node_info::access_permissions is not reliable.
  The node may not exist, or the path specified may lead to a non-file object or
  the operation may encounter any other problem in which case a suitable
  `Node_error` will be returned.
  @param[in]      space_id        Tablespace ID. It can be `SPACE_UNKNOWN` to
                                  check if the path specified in hints exists
                                  and get all info but the size of node in
                                  pages. In such case @p page_size should be set
                                  to 0 and @p hints.m_check_permissions must be
                                  set to false.
  @param[in]      node_order      Number of the node in the tablespace.
  @param[in]      hints           Additional information that may be useful for
                                  querying the node's storage. If space_id is
                                  `SPACE_UNKNOWN` then @p m_check_permissions
                                  must be set to false.
  @param[in]      page_size       Physical page size used in the tablespace,
                                  must match one specified when creating the
                                  tablespace. However, if the @p space_id is
                                  `SPACE_UNKNOWN` or we are not interested in
                                  the node size, then @p page_size should be 0.
                                  In such case, the returned Node_info::size
                                  will be 0. */
  [[nodiscard]] virtual ut::Expected<Node_info, Node_error> get_node_info(
      Tablespace_id space_id, size_t node_order, const Node_hints &hints,
      size_t page_size) = 0;
};

constexpr const char *to_string(
    ib::fil::Tablespaces_nodes_interface::Node_error error) {
  switch (error) {
#define CASE_XXX(x)                                         \
  case ib::fil::Tablespaces_nodes_interface::Node_error::x: \
    return #x;
    CASE_XXX(NODE_DOES_NOT_EXIST)
    CASE_XXX(NOT_A_NODE)
    CASE_XXX(IO_ERROR)
#undef CASE_XXX
  }
  return "Unknown";
}

/* Sets the implementation of the Tablespaces_nodes_interface. It can be called
only once, that is, once set, it is impossible to set other implementation.
@param[in] new_tablepsaces_nodes New implementation to use. */
void set_tablespaces_nodes(
    ut::unique_ptr<Tablespaces_nodes_interface> new_tablepsaces_nodes);

} /* namespace ib::fil */

/** The low-level tablespaces' nodes' storage implementation. */
extern ut::unique_ptr<ib::fil::Tablespaces_nodes_interface> tablespaces_nodes;
