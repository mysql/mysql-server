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

#include <algorithm>
#include <array>
#include <string>

class MimeType {
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

class ContentType {
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
    constexpr auto mimetypes =
        std::to_array<std::pair<std::string_view, const char *>>({
            {"css", MimeType::TextCss},
            {"htm", MimeType::TextHtml},
            {"html", MimeType::TextHtml},
            {"jpeg", MimeType::ImageJpeg},
            {"jpg", MimeType::ImageJpeg},
            {"js", MimeType::ApplicationJavascript},
            {"json", MimeType::ApplicationJson},
            {"png", MimeType::ImagePng},
            {"svg", MimeType::ImageSvgXML},
        });

    // lower-case file-extensions.
    //
    // Use ASCII only conversion as our map is ASCII too
    //
    // not using std::lolower() and friends as they are locale-aware which
    // isn't wanted in this case.
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('z' - 'Z') : c; });

    const auto low_bound_it = std::lower_bound(
        mimetypes.begin(), mimetypes.end(), extension,
        [](const auto &a, const auto &_ext) { return a.first < _ext; });

    // std::lower_bound() returns it to first element that's >= value
    return (low_bound_it != mimetypes.end() && low_bound_it->first == extension)
               ? low_bound_it->second
               : MimeType::ApplicationOctetStream;
  }
};

#endif
