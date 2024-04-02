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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_PLUGIN_MONITOR_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_PLUGIN_MONITOR_H_

#include <algorithm>
#include <set>
#include <string>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin_state.h"
#include "mysql/harness/stdx/monitor.h"

#include "helper/container/to_string.h"

IMPORT_LOG_FUNCTIONS()

namespace helper {

class PluginMonitor {
 public:
  using ServiceName = std::string;
  using Services = std::set<ServiceName>;
  using PluginState = mysql_harness::PluginState;
  using PluginStateObserver = mysql_harness::PluginStateObserver;

  PluginMonitor(PluginState *ps = PluginState::get_instance())
      : ps_{ps}, observer_{new ServiceObserver(this)} {
    observer_id_ = ps_->push_back_observer(observer_);
  }

  ~PluginMonitor() {
    if (PluginState::k_invalid_id_ != observer_id_) {
      ps_->remove_observer(observer_id_);
    }

    observer_->reset();
  }

  bool wait_for_services(const Services &services) {
    log_debug("wait_for_services: '%s'",
              helper::container::to_string(services).c_str());
    bool result{false};
    observer_->wait_for_services_.wait([&services,
                                        &result](PluginMonitor *state) {
      if (nullptr == state) return true;

      result = std::all_of(services.begin(), services.end(),
                           [state](const ServiceName &name) {
                             return 0 != state->active_services_.count(name);
                           });

      if (result) return result;

      if (std::all_of(services.begin(), services.end(),
                      [state](const ServiceName &name) {
                        return 0 !=
                               state->active_and_stopped_services_.count(name);
                      })) {
        // result is false, and we are breaking the "wait" call.
        return true;
      }

      return result;
    });
    log_debug("wait_for_services ended with '%s'",
              (result ? "success" : "failure"));

    return result;
  }

  void abort() {
    if (observer_) observer_->reset();
  }

  Services get_active_services() { return active_services_; }

 private:
  class ServiceObserver : public PluginStateObserver {
   public:
    ServiceObserver(PluginMonitor *parent) : wait_for_services_{parent} {}

    void on_begin_observation(
        const std::vector<std::string> &active_plugins,
        const std::vector<std::string> &stopped_plugins) override {
      wait_for_services_.serialize_with_cv(
          [&active_plugins, &stopped_plugins](PluginMonitor *ptr,
                                              [[maybe_unused]] auto &cv) {
            if (!ptr) return;

            ptr->active_services_.clear();
            for (auto &p : active_plugins) {
              ptr->active_services_.insert(p);
              ptr->active_and_stopped_services_.insert(p);
            }
            for (auto &p : stopped_plugins) {
              ptr->active_and_stopped_services_.insert(p);
            }
          });
    }

    void on_plugin_startup([[maybe_unused]] const PluginState *state,
                           const std::string &name) override {
      log_debug("on_plugin_startup %s", name.c_str());
      wait_for_services_.serialize_with_cv(
          [&name](PluginMonitor *ptr, auto &cv) {
            if (!ptr) return;

            ptr->active_services_.insert(name);
            ptr->active_and_stopped_services_.insert(name);
            cv.notify_all();
          });
    }
    void on_plugin_shutdown([[maybe_unused]] const PluginState *state,
                            const std::string &name) override {
      log_debug("on_plugin_shutdown %s", name.c_str());
      wait_for_services_.serialize_with_cv(
          [&name](PluginMonitor *ptr, auto &cv) {
            if (!ptr) return;

            ptr->active_services_.erase(name);
            ptr->active_and_stopped_services_.insert(name);
            cv.notify_all();
          });
    }

    void reset() {
      wait_for_services_.serialize_with_cv([](PluginMonitor *&ptr, auto &cv) {
        ptr = nullptr;
        cv.notify_all();
      });
    }

    WaitableMonitor<PluginMonitor *> wait_for_services_;
  };

  PluginState *ps_;
  PluginState::ObserverId observer_id_{PluginState::k_invalid_id_};
  std::shared_ptr<ServiceObserver> observer_;
  Services active_services_;
  Services active_and_stopped_services_;
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_PLUGIN_MONITOR_H_
