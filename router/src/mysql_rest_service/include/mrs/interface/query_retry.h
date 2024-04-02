/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_QUERY_RETRY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_QUERY_RETRY_H_

#include "mrs/database/filter_object_generator.h"
#include "mysqlrouter/mysql_session.h"

namespace mrs {
namespace interface {

class QueryRetry {
 public:
  using FilterObjectGenerator = mrs::database::FilterObjectGenerator;

 public:
  virtual ~QueryRetry() = default;

  virtual void before_query() = 0;
  virtual mysqlrouter::MySQLSession *get_session() = 0;
  virtual const FilterObjectGenerator &get_fog() = 0;

  virtual bool should_retry(const uint64_t affected) const = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_QUERY_RETRY_H_
