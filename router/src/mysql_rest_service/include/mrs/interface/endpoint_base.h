/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_ENDPOINT_BASE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_ENDPOINT_BASE_H_

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "http/base/uri.h"
#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/universal_id.h"
#include "mrs/interface/endpoint_configuration.h"
#include "mrs/interface/rest_handler.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace interface {

/*
 * Base class to build the Endpoint hierarchy
 *
 * Following step should be done manually by
 * the instance user.
 *     change_parent({});
 * It's important because parent holds the pointer to this instance in
 * endpoints_ /shared_ptr/.
 */
class EndpointBase : public std::enable_shared_from_this<EndpointBase> {
 public:
  using EnabledType = mrs::database::entry::EnabledType;
  using EndpointBasePtr = std::shared_ptr<EndpointBase>;
  using UniversalId = mrs::database::entry::UniversalId;
  using Children = std::vector<EndpointBasePtr>;
  using EndpointConfiguration = mrs::interface::EndpointConfiguration;
  using EndpointConfigurationPtr = std::shared_ptr<EndpointConfiguration>;
  using Handler = mrs::interface::RestHandler;
  using HandlerPtr = std::shared_ptr<Handler>;
  using Uri = ::http::base::Uri;
  using OptionalIndexNames = std::optional<std::vector<std::string>>;

 public:
  EndpointBase(EndpointConfigurationPtr configuration)
      : configuration_{configuration} {}
  virtual ~EndpointBase() = default;

  /*
   * Virtual methods, to overwrite in subclass.
   */
 public:
  virtual UniversalId get_id() const = 0;
  virtual UniversalId get_parent_id() const = 0;
  virtual OptionalIndexNames get_index_files() = 0;
  virtual std::optional<std::string> get_options() const = 0;
  virtual std::string get_my_url_path_part() const = 0;

 protected:
  virtual EnabledType get_this_node_enabled_level() const = 0;
  virtual std::string get_my_url_part() const = 0;

  virtual bool does_this_node_require_authentication() const = 0;

  /*
   * Predefined behaviors.
   */
 public:
  virtual bool required_authentication() const {
    bool parents_required_authnetication = false;
    auto parent = get_parent_ptr();

    if (parent) {
      parents_required_authnetication = parent->required_authentication();
    }

    return parents_required_authnetication ||
           does_this_node_require_authentication();
  }

  virtual std::string get_url_path() const {
    auto parent = get_parent_ptr();

    return (parent ? parent->get_url_path() : std::string()) +
           get_my_url_path_part();
  }

  virtual Uri get_url() const {
    auto parent = get_parent_ptr();
    Uri uri{parent ? parent->get_url() : Uri{}};
    uri.set_path(uri.get_path() + get_my_url_part());

    return uri;
  }

  /*
   * enable-public
   *
   * The 'override' methods may expose handlers (http endpoints) that
   * should be visible/available through HTTP interface.
   */
  virtual void activate_public() {}

  /*
   * enable-private
   *
   * The 'override' methods may initialize some per endpoint
   * data that may be used internally in dependencies from other endpoints.
   */
  virtual void activate_private() {}

  /*
   * disable / deactivate
   *
   * The endpoint is still in memory, be should no
   * expose any handlers (http endpoint) nor should be
   * used internally.
   */
  virtual void deactivate() {}

  virtual EnabledType get_enabled_level() const {
    auto parent = get_parent_ptr();

    if (!parent) return EnabledType::EnabledType_none;

    auto this_enabled_level = get_this_node_enabled_level();

    if (this_enabled_level == EnabledType::EnabledType_none)
      return EnabledType::EnabledType_none;

    auto parent_enabled_level = parent->get_enabled_level();

    if (this_enabled_level == EnabledType::EnabledType_public)
      return parent_enabled_level;

    return parent_enabled_level == EnabledType::EnabledType_public
               ? EnabledType::EnabledType_private
               : parent_enabled_level;
  }

  void set_parent(EndpointBasePtr parent) {
    auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);
    change_parent(parent);
    changed();
  }

  void change_parent(EndpointBasePtr parent_new) {
    auto parent_old = get_parent_ptr();

    if (parent_old == parent_new) return;
    if (parent_old) parent_old->remove_child_endpoint(get_id());
    if (parent_new) parent_new->add_child_endpoint(shared_from_this());

    parent_ = parent_new;
  }

  EndpointConfigurationPtr get_configuration() const { return configuration_; }

  const EndpointBasePtr get_parent_ptr() const { return parent_.lock(); }
  EndpointBasePtr get_parent_ptr() { return parent_.lock(); }

  void add_child_endpoint(EndpointBasePtr child_ptr) {
    auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);

    endpoints_.insert_or_assign(child_ptr->get_id(), child_ptr);
  }

  void remove_child_endpoint(const UniversalId &child_id) {
    auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);
    endpoints_.erase(child_id);
  }

  Children get_children() {
    Children result;
    result.reserve(endpoints_.size());
    for (auto [_, child] : endpoints_) {
      result.push_back(child);
    }

    return result;
  }

  /*
   * Generic methods
   */
 protected:
  std::optional<EnabledType> last_state_;
  static const char *get_state_name(EnabledType et) {
    switch (et) {
      case EnabledType::EnabledType_none:
        return "disabled";
      case EnabledType::EnabledType_private:
        return "private";
      case EnabledType::EnabledType_public:
        return "public";
    }
    assert(false && "Missing 'enums' value inside the switch.");
    return "unknown";
  }
  void log_update() {
    auto current = get_enabled_level();
    if (last_state_ != current) {
      last_state_ = current;

      log_info("Endpoint(id=%s, path='%s'%s) changed state to '%s'",
               get_id().to_string().c_str(), get_url().join().c_str(),
               get_extra_update_data().c_str(), get_state_name(current));
    }
  }

  virtual std::string get_extra_update_data() { return {}; }

  virtual void update() {
    log_update();
    switch (get_enabled_level()) {
      case EnabledType::EnabledType_public:
        activate_public();
        break;
      case EnabledType::EnabledType_private:
        activate_private();
        break;
      case EnabledType::EnabledType_none:
        deactivate();
        break;
    }
  }

  EndpointBasePtr get_child_by_id(const UniversalId &id) const {
    auto it = endpoints_.find(id);
    if (it == endpoints_.end()) return {};

    return it->second;
  }

  /*
   * Update current endpoint and all its children
   *
   * Until now the method was called from other methods like:
   * `change_parent`, `set`. This was changed because too many
   * updates were generated. Currently, the user of the class
   * is responsible to call `changed` in right moment.
   */
  void changed() {
    update();
    // Make a copy of shared-pointers that hold our children.
    // This way we can operate in thread safe way.
    auto children = get_children();

    for (auto &child : children) child->changed();
  }

 protected:
  std::shared_mutex endpoints_access_;
  std::map<UniversalId, EndpointBasePtr> endpoints_;
  std::weak_ptr<EndpointBase> parent_;
  EndpointConfigurationPtr configuration_;
};

}  // namespace interface
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_INTERFACE_ENDPOINT_BASE_H_ \
        */
