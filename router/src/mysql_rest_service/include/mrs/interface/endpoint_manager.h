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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_MANAGER_H_

#include <vector>

#include "mrs/database/entry/content_file.h"
#include "mrs/database/entry/content_set.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/database/entry/db_schema.h"
#include "mrs/database/entry/db_service.h"
#include "mrs/database/entry/db_state.h"
#include "mrs/rest/entry/app_url_host.h"

namespace mrs {
namespace interface {

class EndpointManager {
 public:
  using DbSchema = database::entry::DbSchema;
  using DbService = database::entry::DbService;
  using UrlHost = rest::entry::AppUrlHost;
  using ContentSet = database::entry::ContentSet;
  using ContentFile = database::entry::ContentFile;
  using DbObject = database::entry::DbObject;

 public:
  virtual ~EndpointManager() = default;

  virtual void configure(const std::optional<std::string> &options) = 0;

  virtual void update(const std::vector<UrlHost> &paths) = 0;
  virtual void update(const std::vector<DbService> &paths) = 0;

  virtual void update(const std::vector<DbSchema> &paths) = 0;
  virtual void update(const std::vector<DbObject> &paths) = 0;

  virtual void update(const std::vector<ContentSet> &set) = 0;
  virtual void update(const std::vector<ContentFile> &files) = 0;

  virtual void clear() = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_MANAGER_H_
