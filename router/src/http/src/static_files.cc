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

#include <map>
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

#include "http_auth.h"
#include "http_server_plugin.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/http_auth_realm_component.h"
#include "mysqlrouter/http_server_component.h"

void HttpStaticFolderHandler::handle_request(HttpRequest &req) {
  HttpUri parsed_uri{req.get_uri()};

  // failed to parse the URI
  if (!parsed_uri) {
    req.send_error(HttpStatusCode::BadRequest);
    return;
  }

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
  std::unique_ptr<char, decltype(&free)> unescaped{
      evhttp_uridecode(parsed_uri.get_path().c_str(), 1, nullptr), &free};
  file_path += http_uri_path_canonicalize(unescaped.get());

  auto out_hdrs = req.get_output_headers();

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

  int file_fd = open(file_path.c_str(), O_RDONLY);

  if (file_fd < 0) {
    if (errno == ENOENT) {
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
    if (!req.is_modified_since(st.st_mtime)) {
      close(file_fd);
      req.send_error(HttpStatusCode::NotModified);
      return;
    }

    req.add_last_modified(st.st_mtime);

    auto chunk = req.get_output_buffer();
    // if the file-size is 0, there is nothing to send ... and it triggers a
    // mmap() error
    if (st.st_size > 0) {
      // only use sendfile if packet is large enough as it will be sent in sep
      // TCP packet/syscall
      //
      // using TCP_CORK would help here
      // if (st.st_size > 64 * 1024) {
      //    evbuffer_set_flags(chunk, EVBUFFER_FLAG_DRAINS_TO_FD);
      // }
      chunk.add_file(file_fd, 0, st.st_size);
      // file_fd is owned by evbuffer_add_file(), don't close it
    } else {
      close(file_fd);
    }

    // file exists
    auto n = file_path.rfind('.');
    if (n != std::string::npos) {
      const std::map<std::string, std::string> mimetypes{
          {"css", "text/css"},          {"js", "text/javascript"},
          {"json", "application/json"}, {"html", "text/html"},
          {"png", "image/png"},         {"svg", "image/svg+xml"},
      };
      std::string extension = file_path.substr(n + 1);
      auto it = mimetypes.find(extension);

      if (it != mimetypes.end()) {
        // found
        out_hdrs.add("Content-Type", it->second.c_str());
      } else {
        out_hdrs.add("Content-Type", "application/octet-stream");
      }
    }

    req.send_reply(HttpStatusCode::Ok,
                   HttpStatusCode::get_default_status_text(HttpStatusCode::Ok),
                   chunk);

    return;
  }
}
