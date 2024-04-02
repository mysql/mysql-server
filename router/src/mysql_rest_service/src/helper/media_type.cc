/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "helper/media_type.h"

namespace helper {

const char *get_mime_name(MediaType mt) {
  switch (mt) {
    case MediaType::typeXieee754ClientJson:
      return "application/x.ieee754.client+json";
    case MediaType::typeJson:
      return "application/json";
    case MediaType::typeUnknownBinary:
      return "application/octet-stream";
    case MediaType::typeUnknownText:
      return "text/plain";
    case MediaType::typePlain:
      return "text/plain";
    case MediaType::typeHtml:
      return "text/html";
    case MediaType::typeJs:
      return "text/javascript";
    case MediaType::typeCss:
      return "text/css";
    case MediaType::typePng:
      return "image/png";
    case MediaType::typeJpg:
      return "image/jpeg";
    case MediaType::typeGif:
      return "image/gif";
    case MediaType::typeBmp:
      return "image/bmp";
    case MediaType::typeAvi:
      return "image/avi";
    case MediaType::typeWav:
      return "image/wav";
    case MediaType::typeSvg:
      return "image/svg+xml";
    case MediaType::typeIco:
      return "image/x-icon";
  }

  return "";
}

std::string to_string(MediaType mt) { return get_mime_name(mt); }

}  // namespace helper
