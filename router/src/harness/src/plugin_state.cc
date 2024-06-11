/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/plugin_state.h"

#include <algorithm>
#include <mutex>

namespace mysql_harness {

using PluginName = PluginState::PluginName;
using Plugins = PluginState::Plugins;
using ObserverId = PluginState::ObserverId;

namespace observers {

class RecordActivePluginsObserver : public PluginStateObserver {
 public:
  RecordActivePluginsObserver(Plugins &active_plugins, Plugins &stopped_plugins)
      : active_plugins_{active_plugins}, stopped_plugins_{stopped_plugins} {}

  void on_plugin_startup(const PluginState *,
                         const std::string &name) override {
    active_plugins_.push_back(name);
  }
  void on_plugin_shutdown(const PluginState *,
                          const std::string &name) override {
    active_plugins_.erase(
        std::remove(active_plugins_.begin(), active_plugins_.end(), name),
        active_plugins_.end());
    stopped_plugins_.push_back(name);
  }

  Plugins &active_plugins_;
  Plugins &stopped_plugins_;
};

}  // namespace observers

class PluginState::PluginStateOp {
 public:
  PluginStateOp(PluginState *parent) : parent_{parent} {}

  // template <void (PluginStateObserver::*f)(const PluginState *,
  //                                           const std::string &name)>
  template <typename Method, typename... Args>
  void dispatch(Method m, Args &&... args) {  // const PluginName &name) {
    auto lock = std::unique_lock<std::mutex>(parent_->mutex_guard_listeners_);
    for (auto &observer : parent_->listeners_) {
      auto obj = observer.second.lock();
      if (obj) {
        (*obj.*m)(parent_, args...);
      }
    }
  }

  PluginState *parent_;
};

PluginState *PluginState::get_instance() {
  static PluginState ps;

  return &ps;
}

PluginState::PluginState() {
  default_observer_ = std::make_shared<observers::RecordActivePluginsObserver>(
      running_plugins_, stopped_plugins_);
  push_back_observer(default_observer_);
}

ObserverId PluginState::push_back_observer(
    std::weak_ptr<PluginStateObserver> psl) {
  auto id = k_invalid_id_;

  while (k_invalid_id_ == id) {
    auto try_id = last_used_id_.fetch_add(1);

    std::unique_lock<std::mutex> lock(mutex_guard_listeners_);
    if (listeners_.count(try_id)) {
      continue;
    }
    id = try_id;
  }

  std::unique_lock<std::mutex> lock(mutex_guard_listeners_);
  listeners_[id] = psl;
  auto ptr = psl.lock();
  if (ptr) {
    ptr->on_begin_observation(running_plugins_, stopped_plugins_);
  }
  return id;
}

std::vector<ObserverId> PluginState::push_back_observers(
    const std::vector<std::weak_ptr<PluginStateObserver>> &array) {
  std::vector<ObserverId> result;
  for (auto &a : array) {
    result.push_back(push_back_observer(a));
  }

  return result;
}

void PluginState::remove_observer(ObserverId k) {
  std::unique_lock<std::mutex> lock(mutex_guard_listeners_);
  auto it = listeners_.find(k);
  if (listeners_.end() == it) return;

  auto ptr = it->second.lock();
  if (ptr) {
    ptr->on_end_observation();
  }
  listeners_.erase(it);
}

void PluginState::remove_observers(
    const std::vector<ObserverId> &observer_ids) {
  for (auto id : observer_ids) remove_observer(id);
}

void PluginState::dispatch_register_waitable(const PluginName &name) {
  PluginStateOp(this).dispatch(
      &PluginStateObserver::on_plugin_register_waitable, name);
}

void PluginState::dispatch_startup(const PluginName &name) {
  PluginStateOp(this).dispatch(&PluginStateObserver::on_plugin_startup, name);
}

void PluginState::dispatch_shutdown(const PluginName &name) {
  PluginStateOp(this).dispatch(&PluginStateObserver::on_plugin_shutdown, name);
}

Plugins PluginState::get_running_plugins() const {
  std::unique_lock<std::mutex> lock(mutex_guard_listeners_);
  return running_plugins_;
}

Plugins PluginState::get_loaded_plugins() const { return loaded_plugins_; }

}  // namespace mysql_harness
