/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_STATUS_CODE_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_STATUS_CODE_H_

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {
namespace status_code {

using name_type = const char *;
using key_type = int;

// https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
constexpr key_type Continue = 100;            // RFC 7231
constexpr key_type SwitchingProtocols = 101;  // RFC 7231
constexpr key_type Processing = 102;          // RFC 2518
constexpr key_type EarlyHints = 103;          // RFC 8297

constexpr key_type Ok = 200;                         // RFC 7231
constexpr key_type Created = 201;                    // RFC 7231
constexpr key_type Accepted = 202;                   // RFC 7231
constexpr key_type NonAuthoritiveInformation = 203;  // RFC 7231
constexpr key_type NoContent = 204;                  // RFC 7231
constexpr key_type ResetContent = 205;               // RFC 7231
constexpr key_type PartialContent = 206;             // RFC 7233
constexpr key_type MultiStatus = 207;                // RFC 4918
constexpr key_type AlreadyReported = 208;            // RFC 5842
constexpr key_type InstanceManipulationUsed = 226;   // RFC 3229

constexpr key_type MultipleChoices = 300;    // RFC 7231
constexpr key_type MovedPermanently = 301;   // RFC 7231
constexpr key_type Found = 302;              // RFC 7231
constexpr key_type SeeOther = 303;           // RFC 7231
constexpr key_type NotModified = 304;        // RFC 7232
constexpr key_type UseProxy = 305;           // RFC 7231
constexpr key_type TemporaryRedirect = 307;  // RFC 7231
constexpr key_type PermanentRedirect = 308;  // RFC 7538

constexpr key_type BadRequest = 400;                   // RFC 7231
constexpr key_type Unauthorized = 401;                 // RFC 7235
constexpr key_type PaymentRequired = 402;              // RFC 7231
constexpr key_type Forbidden = 403;                    // RFC 7231
constexpr key_type NotFound = 404;                     // RFC 7231
constexpr key_type MethodNotAllowed = 405;             // RFC 7231
constexpr key_type NotAcceptable = 406;                // RFC 7231
constexpr key_type ProxyAuthenticationRequired = 407;  // RFC 7235
constexpr key_type RequestTimeout = 408;               // RFC 7231
constexpr key_type Conflicts = 409;                    // RFC 7231
constexpr key_type Gone = 410;                         // RFC 7231
constexpr key_type LengthRequired = 411;               // RFC 7231
constexpr key_type PreconditionFailed = 412;           // RFC 7232
constexpr key_type PayloadTooLarge = 413;              // RFC 7231
constexpr key_type URITooLarge = 414;                  // RFC 7231
constexpr key_type UnsupportedMediaType = 415;         // RFC 7231
constexpr key_type RangeNotSatisfiable = 416;          // RFC 7233
constexpr key_type ExpectationFailed = 417;            // RFC 7231
constexpr key_type IamaTeapot = 418;                   // RFC 7168
constexpr key_type MisdirectedRequest = 421;           // RFC 7540
constexpr key_type UnprocessableEntity = 422;          // RFC 4918
constexpr key_type Locked = 423;                       // RFC 4918
constexpr key_type FailedDependency = 424;             // RFC 4918
constexpr key_type UpgradeRequired = 426;              // RFC 7231
constexpr key_type PreconditionRequired = 428;         // RFC 6585
constexpr key_type TooManyRequests = 429;              // RFC 6585
constexpr key_type RequestHeaderFieldsTooLarge = 431;  // RFC 6585
constexpr key_type UnavailableForLegalReasons = 451;   // RFC 7725

constexpr key_type InternalError = 500;                 // RFC 7231
constexpr key_type NotImplemented = 501;                // RFC 7231
constexpr key_type BadGateway = 502;                    // RFC 7231
constexpr key_type ServiceUnavailable = 503;            // RFC 7231
constexpr key_type GatewayTimeout = 504;                // RFC 7231
constexpr key_type HTTPVersionNotSupported = 505;       // RFC 7231
constexpr key_type VariantAlsoNegotiates = 506;         // RFC 2295
constexpr key_type InsufficientStorage = 507;           // RFC 4918
constexpr key_type LoopDetected = 508;                  // RFC 5842
constexpr key_type NotExtended = 510;                   // RFC 2774
constexpr key_type NetworkAuthorizationRequired = 511;  // RFC 6585

HTTP_COMMON_EXPORT name_type to_string(key_type key);

}  // namespace status_code
}  // namespace base
}  // namespace http

namespace HttpStatusCode {
using namespace http::base::status_code;

inline auto get_default_status_text(key_type status) {
  return ::http::base::status_code::to_string(status);
}

}  // namespace HttpStatusCode

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_STATUS_CODE_H_
