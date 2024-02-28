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

#include "http/base/status_code.h"

#include <stdexcept>
#include <string>

namespace http {
namespace base {
namespace status_code {

name_type to_string(key_type key) {
  switch (key) {
    case Continue:
      return "Continue";
    case SwitchingProtocols:
      return "Switching Protocols";
    case Processing:
      return "Processing";
    case EarlyHints:
      return "Early Hints";

    case Ok:
      return "Ok";
    case Created:
      return "Created";
    case Accepted:
      return "Accepted";
    case NonAuthoritiveInformation:
      return "Non Authoritive Information";
    case NoContent:
      return "No Content";
    case ResetContent:
      return "Reset Content";
    case PartialContent:
      return "Partial Content";
    case MultiStatus:
      return "Multi Status";
    case AlreadyReported:
      return "Already Reported";
    case InstanceManipulationUsed:
      return "IMUsed";

    case MultipleChoices:
      return "Multiple Choices";
    case MovedPermanently:
      return "Moved Permanently";
    case Found:
      return "Found";
    case SeeOther:
      return "See Other";
    case NotModified:
      return "Not Modified";
    case UseProxy:
      return "Use Proxy";
    case TemporaryRedirect:
      return "Temporary Redirect";
    case PermanentRedirect:
      return "Permanent Redirect";

    case BadRequest:
      return "Bad Request";
    case Unauthorized:
      return "Unauthorized";
    case PaymentRequired:
      return "Payment Required";
    case Forbidden:
      return "Forbidden";
    case NotFound:
      return "Not Found";
    case MethodNotAllowed:
      return "Method Not Allowed";
    case NotAcceptable:
      return "Not NotAcceptable";
    case ProxyAuthenticationRequired:
      return "Proxy Authentication Required";
    case RequestTimeout:
      return "Request Timeout";
    case Conflicts:
      return "Conflicts";
    case Gone:
      return "Gone";
    case LengthRequired:
      return "Length Required";
    case PreconditionFailed:
      return "Precondition Failed";
    case PayloadTooLarge:
      return "Payload Too Large";
    case URITooLarge:
      return "URITooLarge";
    case UnsupportedMediaType:
      return "Unsupported MediaType";
    case RangeNotSatisfiable:
      return "Range Not Satisfiable";
    case ExpectationFailed:
      return "Expectation Failed";
    case IamaTeapot:
      return "I am a Teapot";
    case MisdirectedRequest:
      return "Misdirected Request";
    case UnprocessableEntity:
      return "Unprocessable Entity";
    case Locked:
      return "Locked";
    case FailedDependency:
      return "Failed Dependency";
    case UpgradeRequired:
      return "Upgrade Required";
    case PreconditionRequired:
      return "Precondition Required";
    case TooManyRequests:
      return "Too Many Requests";
    case RequestHeaderFieldsTooLarge:
      return "Request Header Fields Too Large";
    case UnavailableForLegalReasons:
      return "Unavailable For Legal Reasons";

    case InternalError:
      return "Internal Error";
    case NotImplemented:
      return "Not Implemented";
    case BadGateway:
      return "Bad Gateway";
    case ServiceUnavailable:
      return "Service Unavailable";
    case GatewayTimeout:
      return "Gateway Timeout";
    case HTTPVersionNotSupported:
      return "HTTP Version Not Supported";
    case VariantAlsoNegotiates:
      return "Variant Also Negotiates";
    case InsufficientStorage:
      return "Insufficient Storage";
    case LoopDetected:
      return "Loop Detected";
    case NotExtended:
      return "Not Extended";
    case NetworkAuthorizationRequired:
      return "Network Authorization Required";
    default:
      throw std::logic_error("no text for HTTP Status " + std::to_string(key));
  }
}

}  // namespace status_code
}  // namespace base
}  // namespace http
