/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_QUERY_RETRY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_QUERY_RETRY_H_

#include "collector/mysql_cache_manager.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/query_retry.h"

#include "mysqlrouter/mysql_session.h"

namespace mrs {
namespace database {

class QueryRetryOnRW : public mrs::interface::QueryRetry {
 public:
  using MysqlCacheManager = collector::MysqlCacheManager;
  using CachedSession = MysqlCacheManager::CachedObject;

 public:
  QueryRetryOnRW(collector::MysqlCacheManager *cache, CachedSession &session,
                 GtidManager *gtid_manager, FilterObjectGenerator &fog,
                 uint64_t wait_gtid_timeout, bool query_has_gtid_check);

  void before_query() override;
  mysqlrouter::MySQLSession *get_session() override;
  const FilterObjectGenerator &get_fog() override;
  bool should_retry(const uint64_t affected) const override;

 private:
  bool check_gtid(const std::string &gtid);
  CachedSession &session_;
  GtidManager *gtid_manager_;
  collector::MysqlCacheManager *cache_;
  FilterObjectGenerator &fog_;
  mutable bool is_retry_{false};
  mysqlrouter::sqlstring gtid_;
  uint64_t wait_gtid_timeout_;
  bool query_has_gtid_check_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_QUERY_RETRY_H_
