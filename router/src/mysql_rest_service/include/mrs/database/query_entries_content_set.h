/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_

#include <vector>

#include "mrs/database/entry/content_set.h"
#include "mrs/database/helper/query.h"
#include "mrs/interface/supported_mrs_schema_version.h"

namespace mrs {
namespace database {

class QueryEntriesContentSet : protected Query {
 public:
  using VectorOfContentSets = std::vector<database::entry::ContentSet>;
  using Version = mrs::interface::SupportedMrsMetadataVersion;

 public:
  QueryEntriesContentSet(const Version version);

  virtual uint64_t get_last_update();
  virtual void query_entries(MySQLSession *session);

  VectorOfContentSets entries;

 protected:
  void on_row(const ResultRow &row) override;

  uint64_t audit_log_id_;
  Version version_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_CONTENT_SET_H_
