/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "plugin.h"

#include <iterator>
#include <sstream>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

Plugin_info::Plugin_info(const Plugin_v1 &plugin)
    : abi_version(plugin.abi_version),
      arch_descriptor(plugin.arch_descriptor ? plugin.arch_descriptor : ""),
      brief(plugin.brief ? plugin.brief : ""),
      plugin_version(plugin.plugin_version) {
  copy_to_list(requires_plugins, plugin.requires_plugins,
               plugin.requires_length);
  copy_to_list(conflicts, plugin.conflicts, plugin.conflicts_length);
}

std::string Plugin_info::get_abi_version_str(uint32_t ver) {
  return std::to_string(ABI_VERSION_MAJOR(ver)) + "." +
         std::to_string(ABI_VERSION_MINOR(ver));
}

std::string Plugin_info::get_plugin_version_str(uint32_t ver) {
  return std::to_string(VERSION_MAJOR(ver)) + "." +
         std::to_string(VERSION_MINOR(ver)) + "." +
         std::to_string(VERSION_PATCH(ver));
}

void Plugin_info::copy_to_list(std::list<std::string> &out_list,
                               const char **in_list, size_t in_list_size) {
  std::copy(in_list, in_list + in_list_size, std::back_inserter(out_list));
}

void Plugin_info::print_as_json(std::ostream &out_stream) const {
  rapidjson::StringBuffer buff;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buff);
  writer.StartObject();

  writer.Key("abi-version");
  std::string abi_version_str = get_abi_version_str(abi_version);
  writer.String(abi_version_str.c_str());

  writer.Key("arch-descriptor");
  writer.String(arch_descriptor.c_str());

  writer.Key("brief");
  writer.String(brief.c_str());

  writer.Key("plugin-version");
  std::string plugin_version_str = get_plugin_version_str(plugin_version);
  writer.String(plugin_version_str.c_str());

  writer.Key("requires");
  writer.StartArray();
  for (const auto &i : requires_plugins) writer.String(i.c_str());
  writer.EndArray();

  writer.Key("conflicts");
  writer.StartArray();
  for (const auto &i : conflicts) writer.String(i.c_str());
  writer.EndArray();

  writer.EndObject();

  out_stream << buff.GetString();
}

std::ostream &operator<<(std::ostream &stream, const Plugin_info &plugin_info) {
  plugin_info.print_as_json(stream);
  return stream;
}
