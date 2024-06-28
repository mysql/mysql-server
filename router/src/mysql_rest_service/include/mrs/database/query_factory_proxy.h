/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_FACTORY_PROXY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_FACTORY_PROXY_H_

#include <memory>

#include "mrs/database/query_factory.h"
#include "mrs/interface/query_monitor_factory.h"

namespace mrs {
namespace database {

class QueryFactoryProxy : public mrs::interface::QueryFactory {
 public:
  using QueryFactoryPtr = std::shared_ptr<mrs::interface::QueryFactory>;
  using QueryMonitorFactory = mrs::interface::QueryMonitorFactory;

 public:
  QueryFactoryProxy(QueryFactoryPtr subject) : subject_{subject} {}

  void change_subject(QueryFactoryPtr subject) { subject_ = subject; }

  std::shared_ptr<QueryAuditLogEntries> create_query_audit_log() override {
    return subject_->create_query_audit_log();
  }

  std::shared_ptr<QueryEntriesAuthPrivileges> create_query_auth_privileges()
      override {
    return subject_->create_query_auth_privileges();
  }

  std::shared_ptr<QueryEntryContentFile> create_query_content_file() override {
    return subject_->create_query_content_file();
  }

  std::shared_ptr<QueryRestSPMedia> create_query_sp_media() override {
    return subject_->create_query_sp_media();
  }

  std::shared_ptr<QueryEntryGroupRowSecurity> create_query_group_row_security()
      override {
    return subject_->create_query_group_row_security();
  }
  std::shared_ptr<QueryEntryAuthUser> create_query_auth_user() override {
    return subject_->create_query_auth_user();
  }

  std::shared_ptr<QueryEntryObject> create_query_object() override {
    return subject_->create_query_object();
  }

  std::shared_ptr<QueryUserGroups> create_query_user_groups() override {
    return subject_->create_query_user_groups();
  }

  std::shared_ptr<QueryRestTable> create_query_table() override {
    return subject_->create_query_table();
  }

  std::shared_ptr<QueryRestTableSingleRow> create_query_table_single_row(
      bool encode_bigints_as_string) override {
    return subject_->create_query_table_single_row(encode_bigints_as_string);
  }

  std::shared_ptr<QueryRestSP> create_query_sp() override {
    return subject_->create_query_sp();
  }

  std::shared_ptr<database::QueryEntryFields> create_query_fields() override {
    return subject_->create_query_fields();
  }

 private:
  QueryFactoryPtr subject_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_FACTORY_PROXY_H_
