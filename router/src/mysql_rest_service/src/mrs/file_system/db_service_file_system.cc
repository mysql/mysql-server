/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mrs/file_system/db_service_file_system.h"
#include "mrs/database/entry/entry.h"
#include "mrs/database/query_entry_content_file.h"
#include "mrs/endpoint/content_file_endpoint.h"
#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mysqlrouter/file_system_exceptions.h"
#include "router/src/http/include/http/client/request.h"

#include <iostream>
#include <vector>

namespace mrs {
namespace file_system {

namespace {
class File_byte_channel : public shcore::polyglot::ISeekable_channel {
 public:
  File_byte_channel(std::string &&content) : m_data{std::move(content)} {}

  ~File_byte_channel() override = default;

  bool is_open() override { return m_data.good(); }

  void close() override { m_data.clear(); }

  int64_t read(void *buffer, size_t size) override {
    m_data.read(static_cast<char *>(buffer), size);
    return m_data.gcount();
  }

  int64_t write(const char * /*buffer*/, size_t /*size*/) override { return 0; }

  int64_t position() override { return m_data.tellg(); }

  ISeekable_channel &set_position(int64_t new_position) override {
    m_data.seekg(new_position);
    return *this;
  }

  int64_t size() override {
    // auto cur_pos = m_data.tellg();
    m_data.seekg(0, m_data.end);
    auto size = m_data.tellg();
    m_data.seekg(0, m_data.beg);
    return size;
  }

  ISeekable_channel &truncate(int64_t /*size*/) override { return *this; }

 private:
  std::stringstream m_data;
};

#ifdef WIN32
std::string str_replace(std::string_view s, std::string_view from,
                        std::string_view to) {
  std::string str;

  if (from.empty()) {
    str.reserve(to.length() * (s.size() + 1));

    str.append(to);
    for (char c : s) {
      str.push_back(c);
      str.append(to);
    }
  } else {
    str.reserve(s.length());

    int offs = from.length();
    std::string::size_type start = 0, p = s.find(from);
    while (p != std::string::npos) {
      if (p > start) str.append(s, start, p - start);
      str.append(to);
      start = p + offs;
      p = s.find(from, start);
    }
    if (start < s.length()) str.append(s, start, s.length() - start);
  }
  return str;
}
#endif

}  // namespace

DbServiceFileSystem::DbServiceFileSystem(endpoint::DbServiceEndpoint *endpoint)
    : m_service_endpoint{endpoint} {}

void DbServiceFileSystem::traverse_files(
    std::function<bool(const ContentFilePtr &)> callback) {
  for (const auto &child : m_service_endpoint->get_children()) {
    const auto &content_set_ep =
        std::dynamic_pointer_cast<mrs::endpoint::ContentSetEndpoint>(child);

    if (content_set_ep) {
      for (const auto &grand_child : content_set_ep->get_children()) {
        const auto &content_file_ep =
            std::dynamic_pointer_cast<mrs::endpoint::ContentFileEndpoint>(
                grand_child);

        if (content_file_ep) {
          // Traversing the files will finish when the callback returns false..
          if (!callback(content_file_ep)) {
            break;
          }
        }
      }
    }
  }
}

ContentFilePtr DbServiceFileSystem::lookup_file(const std::string &path) {
  using EnabledType = mrs::database::entry::EnabledType;
  ContentFilePtr content_file_ep;
  traverse_files([&content_file_ep, &path](const ContentFilePtr &file_ep) {
    if (file_ep->get_enabled_level() != EnabledType::EnabledType_none &&
        file_ep->get()->request_path == path) {
      if (file_ep->get_persistent_data()) {
        content_file_ep = file_ep;
      }
      return false;
    }
    return true;
  });

  return content_file_ep;
}

std::string DbServiceFileSystem::parse_uri_path(const std::string &uri) {
  return uri;
}

std::string DbServiceFileSystem::parse_string_path(const std::string &path) {
  return path;
}

void DbServiceFileSystem::check_access(const std::string &path,
                                       int64_t /*flags*/) {
  auto content_file_ep = lookup_file(path);

#ifdef WIN32
  // In Windows, graal sends the path using windows path separator even if the
  // linux path separator was used in the code, so we normalize and give it a
  // chance to look for the file
  if (!content_file_ep && path.find('\\') != std::string::npos) {
    auto normalized_path = str_replace(path, "\\", "/");
    content_file_ep = lookup_file(normalized_path);
  }
#endif

  if (!content_file_ep) {
    throw shcore::polyglot::No_such_file_exception(path.c_str());
  }
}

void DbServiceFileSystem::create_directory(const std::string & /*path*/) {
  throw shcore::polyglot::Unsupported_operation_exception(
      "The DBServiceFileSystem does not support directory creation");
}

void DbServiceFileSystem::remove(const std::string & /*path*/) {
  throw shcore::polyglot::Unsupported_operation_exception(
      "The DBServiceFileSystem does not support removing files");
}

std::shared_ptr<shcore::polyglot::ISeekable_channel>
DbServiceFileSystem::new_byte_channel(const std::string &path) {
  auto file_ep = lookup_file(path);

#ifdef WIN32
  // In Windows, graal sends the path using windows path separator even if the
  // linux path separator was used in the code, so we normalize and give it a
  // chance to look for the file
  if (!file_ep && path.find('\\') != std::string::npos) {
    auto normalized_path = str_replace(path, "\\", "/");
    file_ep = lookup_file(normalized_path);
  }
#endif

  if (!file_ep) {
    throw shcore::polyglot::No_such_file_exception(path.c_str());
  }

  mrs::endpoint::handler::PersistentDataContentFile::FetchedFile file;
  try {
    file = file_ep->get_persistent_data()->fetch_file(nullptr);
  } catch (const std::exception &error) {
    std::string str_error = "Unable to read the file '";
    str_error += path;
    str_error += "': ";
    str_error += error.what();
    throw shcore::polyglot::IO_exception(str_error.c_str());
  }

  return std::make_shared<File_byte_channel>(std::move(file.content));
}

std::shared_ptr<shcore::polyglot::IDirectory_stream>
DbServiceFileSystem::new_directory_stream(const std::string & /*path*/) {
  throw shcore::polyglot::Unsupported_operation_exception(
      "The DBServiceFileSystem does not support directory listing.");
}

std::string DbServiceFileSystem::to_absolute_path(const std::string &path) {
  return path;
}

std::string DbServiceFileSystem::to_real_path(const std::string &path) {
  return path;
}

}  // namespace file_system
}  // namespace mrs
