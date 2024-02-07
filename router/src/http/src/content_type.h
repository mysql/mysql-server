/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_HTTP_CONTENT_TYPE_INCLUDED
#define MYSQLROUTER_HTTP_CONTENT_TYPE_INCLUDED

#include "mysqlrouter/http_server_export.h"

#include <algorithm>
#include <array>
#include <string>

class HTTP_SERVER_EXPORT MimeType {
 public:
  // RFC4329 deprecated text/javascript for application/javascript
  static constexpr const char ApplicationJavascript[] =
      "application/javascript";
  static constexpr const char ApplicationJson[] = "application/json";
  static constexpr const char ApplicationOctetStream[] =
      "application/octet-stream";
  static constexpr const char TextCss[] = "text/css";
  static constexpr const char TextHtml[] = "text/html";
  static constexpr const char ImageJpeg[] = "image/jpeg";
  static constexpr const char ImagePng[] = "image/png";
  static constexpr const char ImageSvgXML[] = "image/svg+xml";
};

class HTTP_SERVER_EXPORT ContentType {
 public:
  /**
   * get a mimetype for a file-extension.
   *
   * file-extension is matched case-insensitive
   *
   * returns 'application/octet-stream' in case no mapping is found
   *
   * @returns a mimetype for the extension
   * @retval 'application/octet-stream' if no mapping is found
   */
  static const char *from_extension(std::string extension) {
    // sorted list of extensions and their mapping to their mimetype
    const std::array<std::pair<std::string, const char *>, 9> mimetypes{{
        std::make_pair("css", MimeType::TextCss),
        std::make_pair("htm", MimeType::TextHtml),
        std::make_pair("html", MimeType::TextHtml),
        std::make_pair("jpeg", MimeType::ImageJpeg),
        std::make_pair("jpg", MimeType::ImageJpeg),
        std::make_pair("js", MimeType::ApplicationJavascript),
        std::make_pair("json", MimeType::ApplicationJson),
        std::make_pair("png", MimeType::ImagePng),
        std::make_pair("svg", MimeType::ImageSvgXML),
    }};

    // lower-case file-extensions.
    //
    // Use ASCII only conversion as our map is ASCII too
    //
    // not using std::lolower() and friends as they are locale-aware which
    // isn't wanted in this case.
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('z' - 'Z') : c; });

    auto low_bound_it = std::lower_bound(
        mimetypes.begin(), mimetypes.end(), extension,
        [](const auto &a, const auto &_ext) { return a.first < _ext; });

    // std::lower_bound() returns it to first element that's >= value
    return (low_bound_it != mimetypes.end() && low_bound_it->first == extension)
               ? low_bound_it->second
               : MimeType::ApplicationOctetStream;
  }
};

#endif
