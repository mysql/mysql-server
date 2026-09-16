/*****************************************************************************

Copyright (c) 2013, 2026, Oracle and/or its affiliates.

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

/** @file include/fsp0sysspace.h
 Multi file, shared, system tablespace implementation.

 Created 2013-7-26 by Kevin Lewis
 *******************************************************/

#ifndef fsp0sysspace_h
#define fsp0sysspace_h

#include "fil0tablespace_node_handle_interface.h"
#include "fil0tablespaces_nodes_interface.h"
#include "fsp0space.h"
#include "univ.i"
#include "ut0expected.h" /* ut::Expected */

#ifdef UNIV_HOTBACKUP
#include "srv0srv.h"
#endif

/** If the last data file is auto-extended, we add this many pages to it
at a time. We have to make this public because it is a config variable. */
extern ulong sys_tablespace_auto_extend_increment;

#ifdef UNIV_DEBUG
/** Control if extra debug checks need to be done for temporary tablespace.
Default = true that is disable such checks.
This variable is not exposed to end-user but still kept as variable for
developer to enable it during debug. */
extern bool srv_skip_temp_table_checks_debug;
#endif /* UNIV_DEBUG */

namespace ib::fsp {

/** Stores information about a single system tablespace node parsed out from the
param string. It represents a "const" config part of the node. */
struct SysTablespace_node_config : public Tablespace_node {
 public:
  /** Constructor with all information that could be parsed out of the param
  string. */
  SysTablespace_node_config(const std::string &name, page_no_t size,
                            size_t order, node_device_type_t device_type)
      : Tablespace_node(name, size, order), m_device_type(device_type) {}

  node_device_type_t device_type() const { return m_device_type; }

 private:
  /** Type of the node's device. */
  const node_device_type_t m_device_type;
};

/** Represents a tablespace node, including runtime info acquired and changed
after the node is parsed out of the param string. */
struct SysTablespace_node : public SysTablespace_node_config {
 public:
  SysTablespace_node(const std::string &name, page_no_t size, size_t order,
                     node_device_type_t device_type)
      : SysTablespace_node_config(name, size, order, device_type) {}

  void set_node_storage_exists() {
    ut_a(m_node_storage_exists == false);
    m_node_storage_exists = true;
  }

  [[nodiscard]] bool node_storage_exists() const {
    return m_node_storage_exists;
  }

 private:
  /** Specifies if the storage node exists. This is populated only after
  checking the actual nodes' storage. */
  bool m_node_storage_exists{};
};

/** Data structure that contains the information about shared tablespaces.
Currently this can be the system tablespace or a temporary table tablespace */
class SysTablespace : public Tablespace<SysTablespace_node> {
 private:
  class Opened_storage_node {
   public:
    Opened_storage_node(
        const SysTablespace_node &node,
        ut::unique_ptr<ib::fil::Tablespace_node_handle_interface>
            storage) noexcept
        : m_node(node), m_storage(std::move(storage)) {}

    const SysTablespace_node &m_node;
    ut::unique_ptr<ib::fil::Tablespace_node_handle_interface> m_storage;
  };

 public:
  SysTablespace(space_id_t space_id, fil_type_t space_type)
      : Tablespace(space_id, space_type) { /* No op */
  }

#ifdef UNIV_HOTBACKUP
  void reset() override {
    m_auto_extend_last_file = false;
    m_last_file_size_in_pages_max = 0;
    m_is_tablespace_full = false;
    m_sum_of_new_sizes_in_pages = 0;
    Tablespace::reset();
  }
#endif

  /** Set tablespace full status
  @param[in]    is_full         true if full */
  void set_tablespace_full_status(bool is_full) {
    m_is_tablespace_full = is_full;
  }

  /** Get tablespace full status
  @return true if table is full */
  [[nodiscard]] bool get_tablespace_full_status() {
    return m_is_tablespace_full;
  }

  /** Parse the input params and populate member variables.
  @param[in]    filepath_spec   path to data files
  @return true on success parse */
  [[nodiscard]] bool parse_params(const char *filepath_spec);

  /** Check the data file specification.
  @param[in]  create_new_tablespace True if a new tablespace is to be created
  @param[in]  min_expected_size     Minimum expected tablespace size in bytes
  @param[in]  supports_raw_devices  True if the tablespace supports raw devices.
  @return DB_SUCCESS if all OK else error code */
  [[nodiscard]] dberr_t check_file_spec(bool create_new_tablespace,
                                        uint64_t min_expected_size,
                                        bool supports_raw_devices);

  /**
  @return auto_extend value setting */
  [[nodiscard]] ulint can_auto_extend_last_file() const {
    return (m_auto_extend_last_file);
  }

  /**
  @return the autoextend increment in pages. */
  [[nodiscard]] static page_no_t get_autoextend_increment() {
    return sys_tablespace_auto_extend_increment *
           ((1024 * 1024) / UNIV_PAGE_SIZE);
  }

  /** Open or create the data files, register the tablespace and its nodes in
  the `fil`'s tablespace registry.
  @return DB_SUCCESS or error code */
  [[nodiscard]] dberr_t prepare_nodes();

  /** Check the tablespace header for this tablespace and read the flush LSN.
  @return value stored at offset FIL_PAGE_FILE_FLUSH_LSN or error code */
  [[nodiscard]] ut::Expected<lsn_t> read_lsn_and_check_flags();

  /** @return the sum of the node sizes that were created, in pages. */
  [[nodiscard]] page_no_t get_sum_of_new_sizes_in_pages() const {
    return m_sum_of_new_sizes_in_pages;
  }

 private:
  /** Create storage for the specified data node.
  @param[in,out]        node    data node object. May change value of the
                                m_node_storage_exists member.
  @return Opened storage node or error code */
  [[nodiscard]] ut::Expected<SysTablespace::Opened_storage_node> create_node(
      SysTablespace_node &node);

  /** Open storage of the specified data node.
  @param[in]        node    data node object
  @return Opened storage node or error code */
  [[nodiscard]] ut::Expected<SysTablespace::Opened_storage_node> open_node(
      const SysTablespace_node &node);

 private:
  /* Put the pointer to the next byte after a valid file name.
  Note that we must step over the ':' in a Windows filepath.
  A Windows path normally looks like C:\ibdata\ibdata1:1G, but
  a Windows raw partition may have a specification like
  \\.\C::1Gnewraw or \\.\PHYSICALDRIVE2:1Gnewraw.
  @param[in]    str             system tablespace file path spec
  @return next character in string after the file name */
  [[nodiscard]] static char *parse_file_name(char *ptr);

  /** Convert a numeric string representing a number of bytes optionally ending
  in upper or lower case G, M, or K, to a number of megabytes, rounding down to
  the nearest megabyte. Then return the number of pages in the file.
  @param[in,out]  ptr     Pointer to a numeric string
  @return the number of pages in the file. */
  [[nodiscard]] page_no_t parse_suffixed_page_count(char *&ptr);

  enum class file_status_t_ {
    FILE_STATUS_VOID = 0,              /** status not set */
    FILE_STATUS_RW_PERMISSION_ERROR,   /** permission error */
    FILE_STATUS_READ_WRITE_ERROR,      /** not readable/writable */
    FILE_STATUS_NOT_REGULAR_FILE_ERROR /** not a regular file */
  };

  /** Verify the size of the node storage
  @param[in]    node        data node object
  @param[in]    node_info   Information about the node storage specified.
  @return DB_SUCCESS if OK else error code. */
  [[nodiscard]] dberr_t check_size(
      SysTablespace_node &node,
      const ib::fil::Tablespaces_nodes_interface::Node_info &node_info);

  /* DATA MEMBERS */

  /** if true, then we auto-extend the last data file */
  bool m_auto_extend_last_file{};

  /** if != 0, this tells the max size auto-extending may increase the
  last data file size */
  page_no_t m_last_file_size_in_pages_max{};

  /** Tablespace full status */
  bool m_is_tablespace_full{};

  /** Sum of sizes of all nodes, in pages, that had the storage created during
  this instance lifetime. */
  page_no_t m_sum_of_new_sizes_in_pages{};
};

}  // namespace ib::fsp

/** The control info of the system tablespace. */
extern ib::fsp::SysTablespace srv_sys_space;

/** The control info of a temporary table shared tablespace. */
extern ib::fsp::SysTablespace srv_tmp_space;
#endif /* fsp0sysspace_h */
