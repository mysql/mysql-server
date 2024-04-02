/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/authentication/helper/http_result.h"

#include "helper/json/to_string.h"
#include "mrs/interface/http_result.h"

namespace mrs {
namespace authentication {

using HttpResult = mrs::interface::HttpResult;

HttpResult get_problem_description(::http::base::status_code::key_type status,
                                   const std::string &msg, MapObject o) {
  helper::json::MapObject object{{"status", std::to_string(status)},
                                 {"message", msg}};

  object.merge(o);

  return HttpResult(status, helper::json::to_string(object),
                    helper::MediaType::typeJson);
}

HttpResult get_problem_description(::http::base::status_code::key_type status) {
  return get_problem_description(
      status, HttpStatusCode::get_default_status_text(status));
}

}  // namespace authentication

namespace interface {

HttpResult::HttpResult() : status{::http::base::status_code::Ok} {}

HttpResult::HttpResult(std::string &&r)
    : response{r}, status{::http::base::status_code::Ok} {}

HttpResult::HttpResult(std::string &&r, Type t, std::string e)
    : response{r},
      status{::http::base::status_code::Ok},
      type{t},
      etag{std::move(e)} {}

HttpResult::HttpResult(::http::base::status_code::key_type s, std::string &&r,
                       Type t, std::string e)
    : response{r}, status{s}, type{t}, etag{std::move(e)} {}

HttpResult::HttpResult(const std::string &r, Type t, std::string e)
    : response{r},
      status{::http::base::status_code::Ok},
      type{t},
      etag{std::move(e)} {}

HttpResult::HttpResult(std::string &&r, std::string t, std::string e)
    : response{r},
      status{::http::base::status_code::Ok},
      type_text{t},
      etag{std::move(e)} {}

}  // namespace interface
}  // namespace mrs
