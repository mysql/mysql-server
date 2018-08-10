/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_HTTP_COMMON_INCLUDED
#define MYSQLROUTER_HTTP_COMMON_INCLUDED

#include "mysqlrouter/http_common_export.h"

#include <bitset>
#include <ctime>
#include <functional>  // std::function
#include <memory>
#include <vector>

struct evhttp_uri;
struct evhttp_request;
struct evkeyvalq;
struct evkeyval;
struct evbuffer;

class HttpServer;

// http_common.cc

// https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
namespace HttpStatusCode {
using name_type = const char *;
using key_type = unsigned int;

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

inline name_type get_default_status_text(key_type key) {
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
}  // namespace HttpStatusCode

/**
 * representation of HTTP URI.
 *
 * wraps evhttp_uri and exposes a subset of the libevent function-set
 */
class HTTP_COMMON_EXPORT HttpUri {
 public:
  HttpUri(std::unique_ptr<evhttp_uri, std::function<void(evhttp_uri *)>> uri);
  HttpUri(HttpUri &&);
  ~HttpUri();

  /**
   * create HttpUri from string.
   */
  static HttpUri parse(const std::string &uri_str);

  /**
   * get path part of the URI.
   */
  std::string get_path() const;

 private:
  struct impl;

  std::unique_ptr<impl> pImpl_;
};

/**
 * a Buffer to send/read from network.
 *
 * - memory buffer
 * - file
 *
 * wraps evbuffer
 */
class HTTP_COMMON_EXPORT HttpBuffer {
 public:
  HttpBuffer(std::unique_ptr<evbuffer, std::function<void(evbuffer *)>> buffer);

  HttpBuffer(HttpBuffer &&);

  ~HttpBuffer();

  /**
   * add a memory buffer.
   */
  void add(const char *data, size_t data_size);

  /**
   * add a file.
   */
  void add_file(int file_fd, off_t offset, off_t size);

  /**
   * get length of buffer.
   */
  size_t length() const;

  /**
   * move a subset out from the front of the buffer.
   */
  std::vector<uint8_t> pop_front(size_t length);

 private:
  struct impl;

  std::unique_ptr<impl> pImpl_;

  friend class HttpRequest;
};

/**
 * headers of a HTTP response/request.
 */
class HTTP_COMMON_EXPORT HttpHeaders {
 public:
  class HTTP_COMMON_EXPORT Iterator {
    evkeyval *node_;

   public:
    Iterator(evkeyval *node) : node_{node} {}
    std::pair<std::string, std::string> operator*();
    Iterator &operator++();
    bool operator!=(const Iterator &it) const;
  };
  HttpHeaders(
      std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>> hdrs);
  HttpHeaders(HttpHeaders &&);

  ~HttpHeaders();

  /**
   * add a header.
   */
  int add(const char *key, const char *value);

  /**
   * get a header.
   */
  const char *get(const char *key) const;

  Iterator begin();
  Iterator end();

 private:
  struct impl;

  std::unique_ptr<impl> pImpl_;
};

namespace HttpMethod {
using type = int;
using pos_type = unsigned;
namespace Pos {
constexpr pos_type Get = 0;
constexpr pos_type Post = 1;
constexpr pos_type Head = 2;
constexpr pos_type Put = 3;
constexpr pos_type Delete = 4;
constexpr pos_type Options = 5;
constexpr pos_type Trace = 6;
constexpr pos_type Connect = 7;
constexpr pos_type Patch = 8;

constexpr pos_type _LAST = Patch;
}  // namespace Pos
using Bitset = std::bitset<Pos::_LAST>;

constexpr type Get{1 << Pos::Get};
constexpr type Post{1 << Pos::Post};
constexpr type Head{1 << Pos::Head};
constexpr type Put{1 << Pos::Put};
constexpr type Delete{1 << Pos::Delete};
constexpr type Options{1 << Pos::Options};
constexpr type Trace{1 << Pos::Trace};
constexpr type Connect{1 << Pos::Connect};
constexpr type Patch{1 << Pos::Patch};
}  // namespace HttpMethod

/**
 * IO Context for network operations.
 *
 * wraps libevent's base
 */
class HTTP_COMMON_EXPORT IOContext {
 public:
  IOContext();
  ~IOContext();

  /**
   * wait for events to fire and calls handlers.
   *
   * exits if no more pending events
   *
   * @returns false if no events were pending nor active, true otherwise
   * @throws  std::runtime_error on internal, unexpected error
   */
  bool dispatch();

 private:
  class impl;

  std::unique_ptr<impl> pImpl_;
  friend class HttpClient;
};

/**
 * a HTTP request and response.
 *
 * wraps evhttp_request
 */
class HTTP_COMMON_EXPORT HttpRequest {
 public:
  using RequestHandler = void (*)(HttpRequest *, void *);

  HttpRequest(RequestHandler cb, void *arg = nullptr);
  HttpRequest(
      std::unique_ptr<evhttp_request, std::function<void(evhttp_request *)>>
          req);
  HttpRequest(HttpRequest &&);
  ~HttpRequest();

  HttpHeaders get_output_headers();
  HttpHeaders get_input_headers() const;
  HttpBuffer get_output_buffer();
  HttpBuffer get_input_buffer() const;

  unsigned get_response_code() const;
  std::string get_response_code_line() const;

  HttpMethod::type get_method() const;

  std::string get_uri() const;

  void send_reply(int status_code) {
    send_reply(status_code,
               HttpStatusCode::get_default_status_text(status_code));
  }
  void send_reply(int status_code, std::string status_text);
  void send_reply(int status_code, std::string status_text, HttpBuffer &buffer);

  void send_error(int status_code) {
    send_error(status_code,
               HttpStatusCode::get_default_status_text(status_code));
  }
  void send_error(int status_code, std::string status_text);

  static void sync_callback(HttpRequest *, void *);

  operator bool() const;

  int error_code();
  void error_code(int);
  std::string error_msg();

  /**
   * is request modified since 'last_modified'.
   *
   * @return true, if local content is newer than the clients last known date,
   * false otherwise
   */
  bool is_modified_since(time_t last_modified);

  /**
   * add a Last-Modified-Since header to the response headers.
   */
  bool add_last_modified(time_t last_modified);

 private:
  class impl;

  std::unique_ptr<impl> pImpl_;

  friend class HttpClient;
};

// http_time.cc

/**
 * convert a Date: header into a time_t.
 *
 * @return a time_t representation of Date: header value
 * @throws std::out_of_range on invalid formats
 */
HTTP_COMMON_EXPORT time_t time_from_rfc5322_fixdate(const char *date_buf);

/**
 * convert time_t into a Date: header value.
 *
 */
HTTP_COMMON_EXPORT int time_to_rfc5322_fixdate(time_t ts, char *date_buf,
                                               size_t date_buf_len);

#endif
