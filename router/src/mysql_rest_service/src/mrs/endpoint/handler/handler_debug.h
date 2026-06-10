/*
 Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DEBUG_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DEBUG_H_

#include "mrs/interface/authorize_manager.h"
#include "mrs/rest/handler.h"

namespace mrs {
namespace endpoint {
namespace handler {

class HandlerCallback {
 public:
  virtual ~HandlerCallback() = default;

  virtual void handler_start() = 0;
  virtual void handler_stop() = 0;
};

class HandlerDebug : public mrs::rest::Handler {
 public:
  HandlerDebug(HandlerCallback *cb);

  UniversalId get_service_id() const override { return {}; }
  UniversalId get_db_object_id() const override { return {}; }
  UniversalId get_schema_id() const override { return {}; }
  const std::string &get_service_path() const override;
  const std::string &get_schema_path() const override;
  const std::string &get_db_object_path() const override;

  uint32_t get_access_rights() const override {
    return mrs::database::entry::Operation::valueRead;
  }

  Authorization requires_authentication() const override {
    return Authorization::kNotNeeded;
  }

  HttpResult handle_delete(rest::RequestContext *) override { return {}; }
  HttpResult handle_put(rest::RequestContext *) override { return {}; }
  HttpResult handle_post(rest::RequestContext *,
                         const std::vector<uint8_t> &) override {
    return {};
  }
  HttpResult handle_get(rest::RequestContext *ctxt) override;

 private:
  HandlerCallback *cb_;
};

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HANDLER_DEBUG_H_
