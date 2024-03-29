/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_REST_API_UTILS_INCLUDED
#define MYSQLROUTER_REST_API_UTILS_INCLUDED

#include <chrono>
#include <map>
#include <string>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "mysql/harness/utility/string.h"  // string_format()
#include "mysqlrouter/http_request.h"

/**
 * send a JsonProblem HTTP response.
 *
 * RFC 7807 defines `application/problem+json`:
 *
 * - title
 * - description
 * - instance
 * - detail
 *
 * @param req HttpRequest object to send error-msg
 * @param status_code HTTP Status code of the problem message
 * @param fields fields on the problem message
 */
void send_rfc7807_error(HttpRequest &req, HttpStatusCode::key_type status_code,
                        const std::map<std::string, std::string> &fields);

/**
 * ensure HTTP method is allowed.
 *
 * sends HTTP-response with status 405 if method is not allowed and sets `Allow`
 * HTTP header.
 *
 * @returns success
 * @retval true if HTTP method is allowed
 * @retval false if HTTP is not allowed and HTTP response has been sent
 */
bool ensure_http_method(HttpRequest &req, HttpMethod::Bitset allowed_methods);

/**
 * ensure request is authenticated.
 *
 * sends HTTP-response with status 401 if authentication was not successful.
 *
 * @returns success
 * @retval true if request is authenticaticated
 * @retval false if authentication was not successful and HTTP response has been
 * sent
 */
bool ensure_auth(HttpRequest &req, const std::string require_realm);

/**
 * ensure request has no parameters.
 *
 * sends HTTP-response with status 400 if request contained a query string.
 *
 * @returns success
 * @retval true if request did not contain a query-string
 * @retval false if request contained a query-string and HTTP response has
 * been sent
 */
bool ensure_no_params(HttpRequest &req);

/**
 * send a JsonProblem "Not Found" error.
 *
 * @param req HttpRequest object to send error-msg
 */
void send_rfc7807_not_found_error(HttpRequest &req);

/**
 * ensure resource has modified since client received it.
 *
 * sends HTTP-response with status 304 if client has a newer version that
 *
 * @returns success
 * @retval true if resource is modified since client received it.
 * @retval false if client has the same resource and HTTP response has been
 * sent
 */
bool ensure_modified_since(HttpRequest &req, time_t last_modified);

/**
 * send json document as HTTP response.
 *
 * Content-Type must be sent before the function is called.
 *
 * @param req HttpRequest object to send error-msg
 * @param status_code HTTP Status code of the problem message
 * @param json_doc json document to send as response
 */
void send_json_document(HttpRequest &req, HttpStatusCode::key_type status_code,
                        const rapidjson::Document &json_doc);

/**
 * format a timepoint as json-value (date-time format).
 */
template <class Encoding, class AllocatorType>
rapidjson::GenericValue<Encoding, AllocatorType> json_value_from_timepoint(
    std::chrono::time_point<std::chrono::system_clock> tp,
    AllocatorType &allocator) {
  time_t cur = std::chrono::system_clock::to_time_t(tp);
  struct tm cur_gmtime;
#ifdef _WIN32
  gmtime_s(&cur_gmtime, &cur);
#else
  gmtime_r(&cur, &cur_gmtime);
#endif
  auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
      tp - std::chrono::system_clock::from_time_t(cur));

  std::string iso8601_datetime{mysql_harness::utility::string_format(
      "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ", cur_gmtime.tm_year + 1900,
      cur_gmtime.tm_mon + 1, cur_gmtime.tm_mday, cur_gmtime.tm_hour,
      cur_gmtime.tm_min, cur_gmtime.tm_sec,
      // cast to long int as it is "longlong" on 32bit, and "long" on
      // 64bit platforms, but we only have a range of 0-999
      static_cast<long int>(usec.count()))};

  return {iso8601_datetime.c_str(), iso8601_datetime.size(), allocator};
}

#endif
