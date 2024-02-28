/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "static_files.h"

#include <memory>
#include <string>

#ifdef _WIN32
#include <io.h>  // close
#else
#include <unistd.h>  // close
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "http/base/uri.h"
#include "mysql/harness/stdx/io/file_handle.h"
#include "mysqlrouter/component/http_auth_realm_component.h"
#include "mysqlrouter/component/http_server_auth.h"
#include "mysqlrouter/component/http_server_component.h"

#include "content_type.h"

HttpStaticFolderHandler::HttpStaticFolderHandler(std::string static_basedir,
                                                 std::string require_realm)
    : static_basedir_(std::move(static_basedir)),
      require_realm_{std::move(require_realm)} {}

void HttpStaticFolderHandler::handle_request(http::base::Request &req) {
  auto &parsed_uri{req.get_uri()};

  if (req.get_method() != HttpMethod::Get &&
      req.get_method() != HttpMethod::Head) {
    req.send_error(HttpStatusCode::MethodNotAllowed);

    return;
  }

  if (!require_realm_.empty()) {
    if (auto realm =
            HttpAuthRealmComponent::get_instance().get(require_realm_)) {
      if (HttpAuth::require_auth(req, realm)) {
        // request is already handled, nothing to do
        return;
      }

      // access granted, fall through
    }
  }

  // guess mime-type

  std::string file_path{static_basedir_};

  file_path += "/";
  const auto unescaped =
      mysqlrouter::URIParser::decode(parsed_uri.get_path(), true);
  file_path += http::base::http_uri_path_canonicalize(unescaped);

  auto &out_hdrs = req.get_output_headers();

  struct stat st;
  if (-1 == stat(file_path.c_str(), &st)) {
    if (errno == ENOENT) {
      // file doesn't exist
      req.send_error(HttpStatusCode::NotFound);

      return;
    } else {
      req.send_error(HttpStatusCode::InternalError);

      return;
    }
  }

  // if we have a directory, check if it contains a index.html file
  if ((st.st_mode & S_IFMT) == S_IFDIR) {
    file_path += "/index.html";

    if (-1 == stat(file_path.c_str(), &st)) {
      if (errno == ENOENT) {
        // it was a directory, but there is no index-file
        req.send_error(HttpStatusCode::Forbidden);

        return;
      } else {
        req.send_error(HttpStatusCode::InternalError);

        return;
      }
    }
  }

  auto res = stdx::io::file_handle::file({}, file_path);
  if (!res) {
    if (res.error() ==
        make_error_condition(std::errc::no_such_file_or_directory)) {
      // stat() succeeded, but open() failed.
      //
      // either a race or apparmor
      req.send_error(HttpStatusCode::NotFound);

      return;
    } else {
      // if it was a directory
      req.send_error(HttpStatusCode::InternalError);

      return;
    }
  } else {
    auto fh = std::move(res.value());
    if (!req.is_modified_since(st.st_mtime)) {
      req.send_error(HttpStatusCode::NotModified);
      return;
    }

    req.add_last_modified(st.st_mtime);

    auto &chunk = req.get_output_buffer();
    chunk.get().resize(st.st_size);

    ssize_t offset = 0;

    while (offset != st.st_size) {
      auto result =
          ::read(fh.native_handle(), &chunk.get()[offset], st.st_size);

      if (result < 0) {
        if (errno == EINTR) continue;
        req.send_error(HttpStatusCode::InternalError);
        return;
      }

      offset += result;
    }

    // file exists
    auto n = file_path.rfind('.');
    if (n != std::string::npos) {
      out_hdrs.add("Content-Type",
                   ContentType::from_extension(file_path.substr(n + 1)));
    }

    req.send_reply(HttpStatusCode::Ok,
                   HttpStatusCode::get_default_status_text(HttpStatusCode::Ok),
                   chunk);

    return;
  }
}
