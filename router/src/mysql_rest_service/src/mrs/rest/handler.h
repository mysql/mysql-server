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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "helper/media_type.h"
#include "http/base/uri.h"
#include "mrs/endpoint/handler/helper/protocol.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/rest_handler.h"

namespace mrs {
namespace rest {

mrs::interface::Options parse_json_options(
    const std::optional<std::string> &options);

class Handler : public interface::RestHandler {
 public:
  using HttpUri = ::http::base::Uri;
  using Protocol = ::mrs::endpoint::handler::Protocol;
  using UriPathMatcher = ::http::base::UriPathMatcher;

 public:
  Handler(const Protocol protocol, const std::string &url_host,
          const std::vector<UriPathMatcher> &rest_path_matcher,
          const std::optional<std::string> &options,
          interface::AuthorizeManager *auth_manager);

  ~Handler() override;

  void initialize(const Configuration &configuration) override;

  const std::string &get_url_host() const override;
  const std::string &get_protocol() const override;
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

  uint32_t check_privileges(
      const std::vector<database::entry::AuthPrivilege> &privileges,
      const UniversalId &service_id, const std::string &service_path,
      const UniversalId &schema_id, const std::string &schema_path,
      const UniversalId &db_object_id, const std::string &db_object_path);

 protected:
  interface::Options options_;
  const std::string url_host_;
  std::vector<UriPathMatcher> rest_path_matcher_;
  std::vector<void *> handler_id_;
  interface::AuthorizeManager *authorization_manager_;
  const std::string protocol_;
  std::shared_ptr<Configuration> configuration_;

  bool log_level_is_debug_;
  bool log_level_is_info_;

  const std::string &empty_path() const;
};

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_HANDLER_H_
