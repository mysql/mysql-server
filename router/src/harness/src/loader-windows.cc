/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "loader.h"

#include "exception.h"
#include "filesystem.h"

#include <Windows.h>

#include <cassert>
#include <sstream>

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Loader

void Loader::platform_specific_init() {
  std::string plugin_path = config_.get_default("plugin_folder");
  SetDllDirectory(plugin_path.c_str());
}

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo::Impl

class Loader::PluginInfo::Impl {
 public:
  // throws bad_plugin
  Impl(const std::string &plugin_folder, const std::string &library_name);

  ~Impl();

  Path path;
  HMODULE handle;
};

Loader::PluginInfo::Impl::Impl(const std::string &plugin_folder,
                               const std::string &library_name)
    : path(Path::make_path(plugin_folder, library_name, "dll")) {
  handle = LoadLibrary(path.real_path().c_str());

  if (handle == nullptr) {
    char buffer[512];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                  nullptr, GetLastError(), LANG_NEUTRAL, buffer, sizeof(buffer),
                  nullptr);
    throw bad_plugin(path.str() + ": " + buffer);
  }
}

Loader::PluginInfo::Impl::~Impl() { FreeLibrary(handle); }

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo

Loader::PluginInfo::~PluginInfo() { delete impl_; }

Loader::PluginInfo::PluginInfo(Loader::PluginInfo &&p) {
  if (&p != this) {
    this->impl_ = p.impl_;
    p.impl_ = NULL;
    this->plugin = p.plugin;
    p.plugin = NULL;
    this->handle = p.handle;
    p.handle = NULL;
  }
}

Loader::PluginInfo::PluginInfo(const std::string &plugin_folder,
                               const std::string &library_name)
    : impl_(new Impl(plugin_folder, library_name)) {}

void Loader::PluginInfo::load_plugin(const std::string &name) {
  assert(impl_->handle);

  SetLastError(0);

  std::string symbol = "harness_plugin_" + name;
  Plugin *plugin =
      reinterpret_cast<Plugin *>(GetProcAddress(impl_->handle, symbol.c_str()));

  DWORD error = GetLastError();
  if (error) {
    char err_msg[512];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                  nullptr, error, LANG_NEUTRAL, err_msg, sizeof(err_msg),
                  nullptr);

    std::ostringstream buffer;
    buffer << "Loading plugin '" << name << "' failed: " << err_msg;
    throw bad_plugin(buffer.str());
  }

  this->plugin = plugin;
}

}  // namespace mysql_harness
