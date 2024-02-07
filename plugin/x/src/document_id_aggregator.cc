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

#include "plugin/x/src/document_id_aggregator.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

std::string Document_id_aggregator::generate_id(const Variables &vars) {
  std::string id = m_id_generator->generate(vars);
  if (m_id_retention_state) m_document_ids.push_back(id);
  return id;
}

ngs::Error_code Document_id_aggregator::configue(
    iface::Sql_session *data_context) {
  Sql_data_result result(data_context);
  try {
    result.query(
        "SELECT @@mysqlx_document_id_unique_prefix,"
        "@@auto_increment_offset,@@auto_increment_increment");
    if (result.size() != 1) {
      log_error(ER_XPLUGIN_FAILED_TO_GET_SYS_VAR,
                "mysqlx_document_id_unique_prefix', "
                "'auto_increment_offset', 'auto_increment_increment");
      return ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
    }
    uint16_t prefix = 0, offset = 0, increment = 0;
    result.get(&prefix, &offset, &increment);
    m_variables = Variables{prefix, offset, increment};
  } catch (const ngs::Error_code &e) {
    log_debug("Unable to get document id variables; exception message: '%s'",
              e.message.c_str());
    return e;
  }
  return ngs::Success();
}

}  // namespace xpl
