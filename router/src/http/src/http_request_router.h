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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_

#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "http/base/request.h"
#include "http/base/request_handler.h"
#include "http/base/uri_path_matcher.h"
#include "http/server/request_handler_interface.h"
#include "mysql/harness/regex_matcher.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/http_server_lib_export.h"

class HTTP_SERVER_LIB_EXPORT HttpRequestRouter
    : public http::server::RequestHandlerInterface {
 public:
  using RequestHandler = http::base::RequestHandler;
  using BaseRequestHandlerPtr = std::shared_ptr<http::base::RequestHandler>;

  void register_regex_handler(const std::string &url_host,
                              const std::string &url_regex_str,
                              std::unique_ptr<RequestHandler> cb);
  void register_direct_match_handler(
      const std::string &url_host,
      const ::http::base::UriPathMatcher &uri_path_matcher,
      std::unique_ptr<RequestHandler> cb);
  void unregister_handler(const void *handler_id);

  void set_default_route(std::unique_ptr<RequestHandler> cb);
  void clear_default_route();
  void route(http::base::Request &req) override;

  void require_realm(const std::string &realm) { require_realm_ = realm; }

 private:
  class RouteRegexMatcher {
   public:
    RouteRegexMatcher(std::string url_pattern, BaseRequestHandlerPtr handler)
        : matcher_(std::make_unique<mysql_harness::RegexMatcher>(url_pattern)),
          url_pattern_(std::move(url_pattern)),
          handler_(std::move(handler)) {}

    bool matches(std::string_view input) const;

    const std::string &url_pattern() const { return url_pattern_; }

    BaseRequestHandlerPtr handler() const { return handler_; }

   private:
    std::unique_ptr<mysql_harness::RegexMatcher> matcher_;
    std::string url_pattern_;
    BaseRequestHandlerPtr handler_;
  };

  class HTTP_SERVER_LIB_EXPORT RouteDirectMatcher {
   public:
    struct HTTP_SERVER_LIB_EXPORT UrlPathKey {
      // nullopt means the element is optional and matches any string, it only
      // makes sense as a last element
      using UrlPathElem = std::optional<std::string>;
      std::vector<UrlPathElem> path_elements;
      bool allow_trailing_slash;

      bool operator<(const UrlPathKey &other) const;

      auto str() const {
        std::string result;
        for (const auto &el : path_elements) {
          if (el)
            result += "/" + *el;
          else
            result += "/*";
        }

        if (allow_trailing_slash) result += "[/]";

        return result;
      }
    };

    static UrlPathKey path_key_from_matcher(
        const ::http::base::UriPathMatcher &url_path_matcher);

    struct PathHandler {
      ::http::base::UriPathMatcher path_matcher;
      BaseRequestHandlerPtr handler;
    };

    RouteDirectMatcher(const PathHandler &path_handler) {
      add_handler(path_handler);
    }

    BaseRequestHandlerPtr handler(const std::string_view path) const;
    std::vector<PathHandler> &handlers() { return handlers_; }
    void add_handler(const PathHandler &path_handler);
    bool has_handler(const void *handler_id) const;
    std::string get_handler_path(const void *handler_id) const;
    size_t remove_handler(const void *handler_id);

   private:
    std::vector<PathHandler> handlers_;
  };

  // if no routes are specified, return 404
  void handler_not_found(http::base::Request &req);

  BaseRequestHandlerPtr find_route_handler(std::string_view url_host,
                                           std::string_view path);

  BaseRequestHandlerPtr find_direct_match_route_handler(
      std::string_view url_host, std::string_view path);

  BaseRequestHandlerPtr find_regex_route_handler(std::string_view url_host,
                                                 std::string_view path);

  void unregister_regex_handler(const void *handler_id);
  void unregister_direct_match_handler(const void *handler_id);

  using UrlPathKey = RouteDirectMatcher::UrlPathKey;
  std::map<std::string, std::map<UrlPathKey, RouteDirectMatcher>, std::less<>>
      request_direct_handlers_;

  std::map<std::string, std::vector<RouteRegexMatcher>, std::less<>>
      request_regex_handlers_;

  BaseRequestHandlerPtr default_route_;
  std::string require_realm_;

  std::shared_mutex route_mtx_;

  friend class HttpRequestRouterDirectMatchTest;
};

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_REQUEST_ROUTER_H_
