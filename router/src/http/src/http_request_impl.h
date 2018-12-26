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

#include <event2/http.h>
#include "mysqlrouter/http_common.h"

class HttpRequest::impl {
 public:
  using evhttp_req_type =
      std::unique_ptr<evhttp_request, std::function<void(evhttp_request *)>>;

  int error_code{0};

  evhttp_req_type req;

  impl(evhttp_req_type request) : req{std::move(request)} {}

  void own() { owns_http_request = true; }

  void disown() { owns_http_request = false; }

  ~impl() {
    // there are two ways to drop ownership of the wrapped
    // evhttp_request:
    //
    // - before evhttp_make_request(), HttpRequest owns the evhttp_request
    // - after evhttp_make_request(), ownership moves to the event-loop
    // - after the eventloop is done, it free()s the evhttp_request if no one
    // called "evhttp_request_own"
    // - ... in which case HttpRequest stays the owner and has to free it
    if (req && !evhttp_request_is_owned(req.get()) && !owns_http_request) {
      req.release();
    }
  }

 private:
  bool owns_http_request{true};
};
