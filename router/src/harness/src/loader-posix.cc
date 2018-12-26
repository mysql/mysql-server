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

#include "mysql/harness/loader.h"

#include "mysql/harness/filesystem.h"

#include "exception.h"

#include <dlfcn.h>
#include <unistd.h>

#include <cassert>
#include <sstream>

#define USE_DLCLOSE 1

// disable dlclose() when built with lsan
//
// clang has __has_feature(address_sanitizer)
// gcc has __SANITIZE_ADDRESS__
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif
#endif

#if defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ == 1
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif

namespace mysql_harness {

////////////////////////////////////////////////////////////////
// class Loader

void Loader::platform_specific_init() {}

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo::Impl

class Loader::PluginInfo::Impl {
 public:
  // throws bad_plugin
  Impl(const std::string &plugin_folder, const std::string &library_name);

  ~Impl();

  Path path;
  void *handle;
};

Loader::PluginInfo::Impl::Impl(const std::string &plugin_folder,
                               const std::string &library_name)
    : path(Path::make_path(plugin_folder, library_name, "so")),
      handle(dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW)) {
  if (handle == nullptr) throw bad_plugin(dlerror());
}

Loader::PluginInfo::Impl::~Impl() {
#if USE_DLCLOSE
  dlclose(handle);
#endif
}

////////////////////////////////////////////////////////////////
// class Loader::PluginInfo

Loader::PluginInfo::~PluginInfo() { delete impl_; }

Loader::PluginInfo::PluginInfo(PluginInfo &&p) {
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

  dlerror();  // clear any previous errors

  std::string symbol = "harness_plugin_" + name;
  Plugin *p = reinterpret_cast<Plugin *>(dlsym(impl_->handle, symbol.c_str()));

  const char *error = dlerror();
  if (error) {
    std::ostringstream buffer;
    buffer << "Loading plugin '" << name << "' failed: " << error;
    throw bad_plugin(buffer.str());
  }

  this->plugin = p;
}

}  // namespace mysql_harness
