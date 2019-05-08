/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_AGGREGATOR_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_AGGREGATOR_INTERFACE_H_

#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"

namespace ngs {

class Document_id_aggregator_interface {
 public:
  using Document_id_list = std::vector<std::string>;
  using Variables = Document_id_generator_interface::Variables;
  class Retention_guard {
   public:
    explicit Retention_guard(Document_id_aggregator_interface *agg)
        : m_agg(agg) {
      if (!m_agg) return;
      agg->clear_ids();
      agg->set_id_retention(true);
    }
    ~Retention_guard() {
      if (!m_agg) return;
      m_agg->clear_ids();
      m_agg->set_id_retention(false);
    }

   private:
    Retention_guard(const Retention_guard &) = delete;
    const Retention_guard &operator=(const Retention_guard &) = delete;
    Document_id_aggregator_interface *m_agg;
  };

  virtual ~Document_id_aggregator_interface() = default;

  virtual std::string generate_id() = 0;
  virtual std::string generate_id(const Variables &vars) = 0;
  virtual void clear_ids() = 0;
  virtual const Document_id_list &get_ids() const = 0;
  virtual Error_code configue(Sql_session_interface *data_context) = 0;
  virtual void set_id_retention(const bool state) = 0;
};  // namespace ngs

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_AGGREGATOR_INTERFACE_H_
