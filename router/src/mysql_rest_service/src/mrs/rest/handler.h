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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "helper/media_type.h"
#include "http/base/uri.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/rest_handler.h"

namespace mrs {
namespace rest {

using HttpUri = ::http::base::Uri;

class Handler : public interface::RestHandler {
 public:
  Handler(const std::string &url,
          const std::vector<std::string> &rest_path_matcher,
          const std::string &options,
          interface::AuthorizeManager *auth_manager);
  ~Handler() override;

  bool may_check_access() const override;
  void authorization(RequestContext *ctxt) override;
  bool request_begin(RequestContext *ctxt) override;
  void request_end(RequestContext *ctxt) override;
  /**
   * Error handler.
   *
   * Method that allows customization of error handling.
   *
   * @returns true in case when the handler send response to the client
   * @returns false in case when the default handler should send a response to
   * the client
   */
  bool request_error(RequestContext *ctxt, const http::Error &e) override;

  const interface::Options &get_options() const override;

  void throw_unauthorize_when_check_auth_fails(RequestContext *);

 protected:
  interface::Options options_;
  const std::string url_;
  const std::vector<std::string> rest_path_matcher_;
  std::vector<void *> handler_id_;
  interface::AuthorizeManager *authorization_manager_;
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_
