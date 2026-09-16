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

/** @file include/fsp0space.h
 General shared tablespace implementation.

 Created 2013-7-26 by Kevin Lewis
 *******************************************************/

#ifndef fsp0space_h
#define fsp0space_h

#include "fil0tablespaces_nodes_interface.h"
#include "fsp0file.h"
#include "fsp0fsp.h"
#include "fsp0types.h"
#include "univ.i"
#include "ut0log.h"
#include "ut0new.h"

#include <vector>

namespace ib::fsp {

/** Stores information about a single tablespace node. */
struct Tablespace_node {
 public:
  Tablespace_node(const std::string &name, page_no_t expected_size_in_pages,
                  size_t order)
      : m_name(name),
        m_expected_size_in_pages(expected_size_in_pages),
        m_order(order) {}

  [[nodiscard]] page_no_t expected_size_in_pages() const {
    return m_expected_size_in_pages;
  }

  [[nodiscard]] const std::string &name() const { return m_name; }

  [[nodiscard]] size_t order() const { return m_order; }

 private:
  /** It can be an absolute path to the node storage.
  It can be a relative path (using `./` or `../`) to the process working
  directory. Otherwise it is a name, that is to be concatenated with
  `Tablespace::path`. */
  const std::string m_name;

  /** Expected size of the node in pages. */
  const page_no_t m_expected_size_in_pages;

  /** Node's position in the list of nodes of the tablespace it resides in. */
  const size_t m_order;
};

/** Data structure that contains the information about an InnoDB tablespace. */
template <typename TNode = Tablespace_node>
class Tablespace {
 protected:
  /** Data nodes information used in this tablespace, in the correct order. */
  ut::vector<TNode> m_nodes;

 public:
  Tablespace(space_id_t space_id, fil_type_t space_type)
      : m_space_id(space_id), m_space_type(space_type) {}

  virtual ~Tablespace() {
    if (m_name != nullptr) {
      ut::free(m_name);
      m_name = nullptr;
    }
  }

  // Disable copying
  Tablespace(const Tablespace &) = delete;

  Tablespace &operator=(const Tablespace &) = delete;

#ifdef UNIV_HOTBACKUP
  virtual void reset() {
    if (m_name != nullptr) {
      ut::free(m_name);
      m_name = nullptr;
    }
    m_nodes.clear();
    m_path.clear();
    m_flags = 0;
    m_autoextend_size = 0;
    /* We are leaving space_id and space_type fields untouched. */
  }
#endif

  const TNode &node(size_t order) const {
    ut_a(order < m_nodes.size());
    const auto &res = m_nodes[order];
    ut_a(res.order() == order);
    return res;
  }

  /** Get tablespace type
  @return tablespace type */
  fil_type_t space_type() const { return m_space_type; }

  /** Set tablespace name
  @param[in]    name    tablespace name */
  void set_name(const char *name) {
    ut_ad(m_name == nullptr);
    m_name = mem_strdup(name);
    ut_ad(m_name != nullptr);
  }

  /** Get tablespace name
  @return tablespace name */
  const char *name() const { return (m_name); }

  /** Set tablespace path.
  @param[in]    path    where tablespace nodes reside. */
  void set_path(const std::string &path) {
    ut_ad(m_path.empty());
    m_path = path;
    Fil_path::normalize(m_path);
  }

  /** Get tablespace path
  @return tablespace path */
  const std::string &path() const { return m_path; }

  /** Set the space id of the tablespace
  @param[in]    space_id         tablespace ID to set */
  void set_space_id(space_id_t space_id) {
    ut_ad(m_space_id == SPACE_UNKNOWN);
    m_space_id = space_id;
  }

  /** Get the space id of the tablespace
  @return m_space_id space id of the tablespace */
  space_id_t space_id() const { return (m_space_id); }

  /** Set the tablespace flags
  @param[in]    fsp_flags       tablespace flags */
  void set_flags(uint32_t fsp_flags) {
    ut_ad(fsp_flags_is_valid(fsp_flags));
    m_flags = fsp_flags;
  }

  /** Get the tablespace flags
  @return m_flags tablespace flags */
  uint32_t flags() const { return (m_flags); }

  /** @return the sum of all the node sizes, in pages. */
  page_no_t get_sum_of_expected_sizes_in_pages() const {
    page_no_t sum = 0;

    for (const auto &node : m_nodes) {
      sum += node.expected_size_in_pages();
    }

    return sum;
  }

  /** Delete all the data files.
  @return Order of all files for which deletion succeeded. */
  std::vector<size_t> delete_files();

  /** Check if two tablespaces have common data file names.
  @param[in]    other_space     Tablespace to check against this.
  @return true if they have the same data filenames and paths */
  bool intersects(const Tablespace &other_space);

  /** Use the ADD DATAFILE path to create a node object and add
  it to the front of m_nodes. Parse the node path into a path
  and a basename with extension 'ibd'. This datafile_path provided
  may be an absolute or relative path, but it must end with the
  extension .ibd and have a basename of at least 1 byte.

  Set tablespace m_path member and add the first node with the path and filename
  to be extracted from node_path specified.
  @param[in]    node_path  full path of the tablespace node. */
  void add_datafile(const char *node_path);

  /** Determines if the current tablespace should be opened for read-only. */
  bool is_read_only() const {
    return srv_read_only_mode && !fsp_is_system_temporary(space_id());
  }

  /** Returns the number of nodes the tablespace contains. */
  size_t get_nodes_count() const { return m_nodes.size(); }

  /** Returns a full path to the node.
  @param[in] node_order Number of the node on the list of nodes in the
  tablespace to query */
  std::string get_node_full_path(size_t node_order = 0) const {
    return get_node_full_path(node(node_order));
  }

  /** Returns a full path to the node.
  @param[in] node Node from the tablespace to query */
  std::string get_node_full_path(const Tablespace_node &node) const {
    const auto node_path = Fil_path::make(path(), node.name(), NO_EXT);
    const std::string res{node_path};
    ut::free(node_path);
    return res;
  }

  /* Set the autoextend size for the tablespace */
  void set_autoextend_size(uint64_t size) { m_autoextend_size = size; }

  /* Get the autoextend size for the tablespace */
  uint64_t get_autoextend_size() const { return m_autoextend_size; }

 private:
  /**
  @param[in]    node_name        Name to lookup in the data nodes.
  @return true if the node name exists in the data nodes */
  bool find(std::string_view node_name);

  /** Name of the tablespace. */
  char *m_name = nullptr;

  /** Tablespace ID */
  space_id_t m_space_id{SPACE_UNKNOWN};

  /** Path where tablespace files will reside, not including a filename.*/
  std::string m_path;

  /** Tablespace flags */
  uint32_t m_flags;

  /** Autoextend size */
  uint64_t m_autoextend_size;

  /** Type of the tablespace */
  const fil_type_t m_space_type;
};

template <typename TNode>
bool Tablespace<TNode>::intersects(const Tablespace &other_space) {
  for (const auto &node : other_space.m_nodes) {
    if (find(node.name())) {
      return (true);
    }
  }

  return (false);
}

template <typename TNode>
bool Tablespace<TNode>::find(std::string_view node_name) {
  for (const auto &node : m_nodes) {
    if (innobase_strcasecmp(node_name.data(), node.name().c_str()) == 0) {
      return (true);
    }
  }

  return (false);
}

template <typename TNode>
std::vector<size_t> Tablespace<TNode>::delete_files() {
  std::vector<size_t> res;
  for (const auto &node : m_nodes) {
    const ib::fil::Tablespaces_nodes_interface::Node_hints hints{
        .m_path = get_node_full_path(node)};
    const auto node_info =
        tablespaces_nodes->get_node_info(space_id(), node.order(), hints, 0);
    if (node_info) {
      const auto delete_status =
          tablespaces_nodes->remove(space_id(), node.order(), hints);

      if (delete_status ==
          ib::fil::Tablespaces_nodes_interface::Status::SUCCESS) {
        res.push_back(node.order());
      }
    }
  }
  return res;
}

template <typename TNode>
void Tablespace<TNode>::add_datafile(const char *node_path) {
  /* This method works only for adding first and only node. */
  ut_a(m_nodes.size() == 0);
  /* The path provided ends in ".ibd".  This was assured by
  validate_create_tablespace_info() */
  ut_d(const char *dot = strrchr(node_path, '.'));
  ut_ad(dot != nullptr && Fil_path::has_suffix(IBD, dot));

  std::string filepath{node_path};

  Fil_path::normalize(filepath);

  /* If the path is an absolute path, separate it into m_path and a basename.
  For relative paths, make the whole thing a basename so that
  it can be appended to the datadir. */
  const auto dirlen = Fil_path::is_absolute_path(filepath)
                          ? dirname_length(filepath.c_str())
                          : 0;

  /* If the pathname contains a directory separator, fill the
  m_path member which is the default directory for files in this
  tablespace. Leave it null otherwise. */
  if (dirlen > 0) {
    set_path(filepath.substr(0, dirlen));
  }

  /* Now add a new node and set the file name removing the prefix path that we
  extracted and have set above. */
  m_nodes.push_back({filepath.substr(dirlen), FIL_IBD_FILE_INITIAL_SIZE, 0});
}

}  // namespace ib::fsp

#endif /* fsp0space_h */
