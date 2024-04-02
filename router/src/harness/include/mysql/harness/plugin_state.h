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

#ifndef ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_PLUGIN_STATE_H_
#define ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_PLUGIN_STATE_H_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "mysql/harness/plugin_state_observer.h"

#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT PluginState {
 public:
  using PluginName = std::string;
  using Plugins = std::vector<PluginName>;
  using ObserverId = uint32_t;
  using ObserverPtr = std::weak_ptr<PluginStateObserver>;
  using MapOfListeners = std::map<ObserverId, ObserverPtr>;

  constexpr static ObserverId k_invalid_id_ = 0;

 public:
  virtual ~PluginState() = default;

  static PluginState *get_instance();

  virtual ObserverId push_back_observer(ObserverPtr psl);
  virtual std::vector<ObserverId> push_back_observers(
      const std::vector<ObserverPtr> &array);
  virtual void remove_observer(ObserverId k);
  virtual void remove_observers(const std::vector<ObserverId> &k);

  virtual void dispatch_register_waitable(const PluginName &name);
  virtual void dispatch_startup(const PluginName &name);
  virtual void dispatch_shutdown(const PluginName &name);

  virtual Plugins get_running_plugins() const;
  virtual Plugins get_loaded_plugins() const;

 protected:
  class PluginStateOp;
  PluginState();

  std::atomic<ObserverId> last_used_id_{k_invalid_id_};
  mutable std::mutex mutex_guard_listeners_;
  MapOfListeners listeners_;
  Plugins running_plugins_;
  Plugins stopped_plugins_;
  Plugins loaded_plugins_;
  std::shared_ptr<PluginStateObserver> default_observer_;
};

}  // namespace mysql_harness

#endif  // ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_PLUGIN_STATE_H_
