/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_DOCUMENT_ID_AGGREGATOR_H_
#define PLUGIN_X_SRC_DOCUMENT_ID_AGGREGATOR_H_

#include <string>

#include "plugin/x/src/interface/document_id_aggregator.h"

namespace xpl {

class Document_id_aggregator : public iface::Document_id_aggregator {
 public:
  explicit Document_id_aggregator(iface::Document_id_generator *gen)
      : m_id_generator(gen) {}
  std::string generate_id() override { return generate_id(m_variables); }
  std::string generate_id(const Variables &vars) override;
  void clear_ids() override { m_document_ids.clear(); }
  const Document_id_list &get_ids() const override { return m_document_ids; }
  ngs::Error_code configue(iface::Sql_session *data_context) override;
  void set_id_retention(const bool state) override {
    m_id_retention_state = state;
  }

 private:
  iface::Document_id_generator *m_id_generator;
  iface::Document_id_generator::Variables m_variables;
  Document_id_list m_document_ids;
  bool m_id_retention_state{false};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_DOCUMENT_ID_AGGREGATOR_H_
