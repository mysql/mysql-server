/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_AGGREGATOR_H_
#define PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_AGGREGATOR_H_

#include <string>
#include <vector>

#include "plugin/x/src/interface/document_id_generator.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"

namespace xpl {
namespace iface {

class Document_id_aggregator {
 public:
  using Document_id_list = std::vector<std::string>;
  using Variables = iface::Document_id_generator::Variables;
  class Retention_guard {
   public:
    explicit Retention_guard(Document_id_aggregator *agg) : m_agg(agg) {
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
    Document_id_aggregator *m_agg;
  };

  virtual ~Document_id_aggregator() = default;

  virtual std::string generate_id() = 0;
  virtual std::string generate_id(const Variables &vars) = 0;
  virtual void clear_ids() = 0;
  virtual const Document_id_list &get_ids() const = 0;
  virtual ngs::Error_code configue(Sql_session *data_context) = 0;
  virtual void set_id_retention(const bool state) = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_AGGREGATOR_H_
