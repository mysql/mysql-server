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

/**
@file clone/include/clone_hton.h
Clone Plugin: Interface with SE handlerton

*/

#ifndef CLONE_HTON_H
#define CLONE_HTON_H

#include <vector>

#include "my_byteorder.h"
#include "mysql/plugin.h"
#include "sql/handler.h"
#include "sql/sql_plugin.h"

/* Namespace for all clone data types */
namespace myclone {

struct Locator {
  /** Get buffer length for serilized locator.
  @return serialized length */
  size_t serlialized_length() const {
    /* Add one byte for SE type */
    return (1 + sizeof(m_loc_len) + m_loc_len);
  }

  /** Serialize the structure
  @param[in,out]	buffer	allocated buffer for serialized data
  @return serialized length */
  size_t serialize(uchar *buffer) {
    *buffer = static_cast<uchar>(m_hton->db_type);
    ++buffer;

    int4store(buffer, m_loc_len);
    buffer += 4;

    memcpy(buffer, m_loc, m_loc_len);

    return (serlialized_length());
  }

  /** Deserialize the buffer to structure. The structure elements
  point within the buffer and the buffer should not be freed while
  using the structure.
  @param[in,out]	buffer	serialized locator buffer
  @return serialized length */
  size_t deserialize(THD *thd, const uchar *buffer) {
    auto db_type = static_cast<enum legacy_db_type>(*buffer);
    ++buffer;

    if (m_hton == nullptr) {
      /* Should not lock plugin for auxiliary threads */
      assert(thd != nullptr);

      m_hton = ha_resolve_by_legacy_type(thd, db_type);

    } else {
      assert(m_hton->db_type == db_type);
    }

    m_loc_len = uint4korr(buffer);
    buffer += 4;

    m_loc = (m_loc_len == 0) ? nullptr : buffer;

    return (serlialized_length());
  }

  /** SE handlerton for the locator */
  handlerton *m_hton;

  /** Locator for the clone operation */
  const uchar *m_loc;

  /** Locator length */
  uint32 m_loc_len;
};

using Storage_Vector = std::vector<Locator>;

using Task_Vector = std::vector<uint32_t>;

} /* namespace myclone */

using myclone::Storage_Vector;
using myclone::Task_Vector;

/** Begin clone operation for all storage engines supporting clone
@param[in,out]	thd			server thread handle
@param[in,out]	clone_loc_vec		vector of locators from SEs
@param[out]	task_vec		vector of task identifiers
@param[in]	clone_type		clone type
@param[in]	clone_mode		clone begin mode
@return error code */
int hton_clone_begin(THD *thd, Storage_Vector &clone_loc_vec,
                     Task_Vector &task_vec, Ha_clone_type clone_type,
                     Ha_clone_mode clone_mode);

/** Clone copy for all storage engines supporting clone
@param[in,out]	thd		server thread handle
@param[in]	clone_loc_vec	vector of locators for SEs
@param[in]	task_vec	vector of task identifiers
@param[in]	clone_cbk	clone callback
@return error code */
int hton_clone_copy(THD *thd, Storage_Vector &clone_loc_vec,
                    Task_Vector &task_vec, Ha_clone_cbk *clone_cbk);

/** Clone end for all storage engines supporting clone
@param[in,out]	thd		server thread handle
@param[in]	clone_loc_vec	vector of locators for SEs
@param[in]	task_vec	vector of task identifiers
@param[in]	in_err		error code when ending after error
@return error code */
int hton_clone_end(THD *thd, Storage_Vector &clone_loc_vec,
                   Task_Vector &task_vec, int in_err);

/** Begin Clone apply operation for all storage engines supporting clone
@param[in,out]	thd			server thread handle
@param[in]	clone_data_dir		target data directory
@param[in,out]	clone_loc_vec		vector of locators from SEs
@param[out]	task_vec		vector of task identifiers
@param[in]	clone_mode		clone begin mode
@return error code */
int hton_clone_apply_begin(THD *thd, const char *clone_data_dir,
                           Storage_Vector &clone_loc_vec, Task_Vector &task_vec,
                           Ha_clone_mode clone_mode);

/** Clone apply error for all storage engines supporting clone
@param[in,out]	thd		server thread handle
@param[in]	clone_loc_vec	vector of locators for SEs
@param[in]	task_vec	vector of task identifiers
@param[in]	in_err		error code when ending after error
@return error code */
int hton_clone_apply_error(THD *thd, Storage_Vector &clone_loc_vec,
                           Task_Vector &task_vec, int in_err);

/** Clone apply end for all storage engines supporting clone
@param[in,out]	thd		server thread handle
@param[in]	clone_loc_vec	vector of locators for SEs
@param[in]	task_vec	vector of task identifiers
@param[in]	in_err		error code when ending after error
@return error code */
int hton_clone_apply_end(THD *thd, Storage_Vector &clone_loc_vec,
                         Task_Vector &task_vec, int in_err);

#endif /* CLONE_HTON_H */
