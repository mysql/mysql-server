/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "helper/media_type.h"

#include <map>
#include <string>

namespace helper {

const char *get_mime_name(MediaType mt) {
  switch (mt) {
    case typeXieee754ClientJson:
      return "application/x.ieee754.client+json";
    case typeJson:
      return "application/json";
    case typeUnknownBinary:
      return "application/octet-stream";
    case typeUnknownText:
      return "text/plain";
    case typePlain:
      return "text/plain";
    case typeHtml:
      return "text/html";
    case typeJs:
      return "text/javascript";
    case typeCss:
      return "text/css";
    case typePng:
      return "image/png";
    case typeJpg:
      return "image/jpeg";
    case typeGif:
      return "image/gif";
    case typeBmp:
      return "image/bmp";
    case typeAvi:
      return "image/avi";
    case typeWav:
      return "image/wav";
    case typeSvg:
      return "image/svg+xml";
    case typeIco:
      return "image/x-icon";
  }

  return "";
}

const char *get_mime_name_from_ext(const std::string &ext) {
  return get_mime_name(get_media_type_from_extension(ext));
}

std::string to_string(MediaType mt) { return get_mime_name(mt); }

MediaType get_media_type_from_extension(const std::string &extenstion) {
  const static std::map<std::string, MediaType> map{
      {".gif", typeGif},  {".jpg", typeJpg}, {".png", typePng},
      {".js", typeJs},    {".mjs", typeJs},  {".html", typeHtml},
      {".htm", typeHtml}, {".css", typeCss}, {".svg", typeSvg},
      {".map", typePlain}};

  auto i = map.find(extenstion);

  if (i == map.end()) return typePlain;

  return i->second;
}

bool is_text_type(const MediaType mt) {
  const static std::map<MediaType, bool> map{
      {typeGif, false}, {typeJpg, false},  {typePng, false}, {typeJs, true},
      {typeJs, true},   {typeHtml, true},  {typeHtml, true}, {typeCss, true},
      {typeSvg, true},  {typePlain, true}, {typeIco, false}};

  auto i = map.find(mt);

  if (i == map.end()) return false;

  return i->second;
}

}  // namespace helper
