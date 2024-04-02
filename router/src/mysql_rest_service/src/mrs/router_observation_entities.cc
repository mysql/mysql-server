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

#include "mrs/router_observation_entities.h"

#include <cassert>
#include <memory>
#include <set>

namespace mrs {

using EntitiesManager = observability::EntitiesManager;
using Entity = observability::Entity;

class RegisterEntites {
 public:
  RegisterEntites(observability::EntitiesManager *manager)
      : manager_{manager} {}
  virtual ~RegisterEntites() {}

  template <uint32_t id>
  void register_entity_with_id(std::string &&name) {
    register_entity(
        std::make_unique<observability::EntityCounter<id>>(std::move(name)));
  }

  template <uint32_t id>
  void register_entity_with_id_not_resetable(std::string &&name) {
    register_entity(
        std::make_unique<observability::EntityCounterNotResetable<id>>(
            std::move(name)));
  }

  virtual void register_entity(std::unique_ptr<Entity> &&entity) {
    manager_->record_entity(std::move(entity));
  }

  virtual void last_is(uint32_t last_entity_id [[maybe_unused]]) {}

 private:
  std::set<uint32_t> used_entities_;
  observability::EntitiesManager *manager_;
};

class RegisterEntitesAndCheckIfAllAdded : public RegisterEntites {
 public:
  using RegisterEntites::RegisterEntites;

#ifndef NDEBUG
  void register_entity(std::unique_ptr<Entity> &&entity) override {
    auto id = entity->get_id();

    assert(!used_entities_.count(id) &&
           "Entity with given ID was already registred.");
    used_entities_.insert(id);
    RegisterEntites::register_entity(std::move(entity));
  }

  void last_is(uint32_t last_entity_id) override {
    RegisterEntites::last_is(last_entity_id);
    assert((used_entities_.size() == last_entity_id - 1) &&
           "Wrong number of entities registred");

    for (uint32_t id = 1; id < last_entity_id; ++id) {
      assert(used_entities_.count(id) && "Entity was not registered.");
    }
  }

  std::set<uint32_t> used_entities_;
#endif
};

void initialize_entities(observability::EntitiesManager *manager) {
  RegisterEntitesAndCheckIfAllAdded reg(manager);

  reg.register_entity_with_id<kEntityCounterHttpRequestGet>("httpRequestGet");
  reg.register_entity_with_id<kEntityCounterHttpRequestPost>("httpRequestPost");
  reg.register_entity_with_id<kEntityCounterHttpRequestPut>("httpRequestPut");
  reg.register_entity_with_id<kEntityCounterHttpRequestDelete>(
      "httpRequestDelete");
  reg.register_entity_with_id<kEntityCounterHttpRequestOptions>(
      "kEntityCounterHttpRequestOptions");
  reg.register_entity_with_id<kEntityCounterHttpConnectionsReused>(
      "httpConnectionsReused");
  reg.register_entity_with_id<kEntityCounterHttpConnectionsCreated>(
      "httpConnectionsCreated");
  reg.register_entity_with_id<kEntityCounterHttpConnectionsClosed>(
      "httpConnectionsClosed");
  reg.register_entity_with_id<kEntityCounterMySQLConnectionsReused>(
      "mysqlConnectionsReused");
  reg.register_entity_with_id<kEntityCounterMySQLConnectionsCreated>(
      "mysqlConnectionsCreated");
  reg.register_entity_with_id<kEntityCounterMySQLConnectionsClosed>(
      "mysqlConnectionsClosed");
  reg.register_entity_with_id_not_resetable<
      kEntityCounterMySQLConnectionsActive>("mysqlConnectionsActive");
  reg.register_entity_with_id<kEntityCounterMySQLQueries>("mysqlQueries");

  reg.register_entity_with_id<kEntityCounterMySQLChangeUser>("mysqlChangeUser");
  reg.register_entity_with_id<kEntityCounterMySQLPrepare>("mysqlPrepareStmt");
  reg.register_entity_with_id<kEntityCounterMySQLPrepareExecute>(
      "mysqlExecuteStmt");
  reg.register_entity_with_id<kEntityCounterMySQLPrepareRemove>(
      "mysqlRemoveStmt");

  reg.register_entity_with_id<kEntityCounterRestReturnedItems>(
      "restReturnedItems");
  reg.register_entity_with_id<kEntityCounterRestAffectedItems>(
      "restAffectedItems");

  reg.register_entity_with_id_not_resetable<kEntityCounterUpdatesObjects>(
      "changesObjects");
  reg.register_entity_with_id_not_resetable<kEntityCounterUpdatesFiles>(
      "changesFiles");
  reg.register_entity_with_id_not_resetable<
      kEntityCounterUpdatesAuthentications>("changesAuthentications");
  reg.last_is(kEntityCounterLast);
}

}  // namespace mrs
