/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTRITIES_MANAGER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTRITIES_MANAGER_H_

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "mrs/observability/entity.h"

namespace mrs {
namespace observability {

class EntitiesManager : private Common {
 public:
  using Snapshot = std::vector<std::pair<std::string, uint64_t>>;

  void record_entity(std::unique_ptr<Entity> &&entity) {
    auto id = entity->get_id();

    if (!(id < entities_.size())) {
      entities_.resize(id + 1);
    }

    entities_[id] = std::move(entity);
  }

  const Snapshot &fetch_counters() {
    if (snapshoot_.size() != entities_.size()) {
      snapshoot_.resize(entities_.size());
      for (size_t i = 0; i < snapshoot_.size(); ++i) {
        snapshoot_[i].first =
            entities_[i] ? entities_[i]->get_name() : std::string();
      }
    }

    auto l = std::unique_lock(mutex);

    for (size_t i = 0; i < snapshoot_.size(); ++i) {
      if (!entities_[i]) {
        snapshoot_[i].second = 0;
        continue;
      }
      snapshoot_[i].second = entities_[i]->get_value_and_reset();
    }

    return snapshoot_;
  }

 private:
  Snapshot snapshoot_;
  std::vector<std::unique_ptr<Entity>> entities_;
};

}  // namespace observability
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_OBSERVABILITY_ENTRITIES_MANAGER_H_
