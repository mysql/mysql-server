/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
#define MYSQL_HARNESS_DIMANAGER_INCLUDED

#include "harness_export.h"

#include "unique_ptr.h"

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace mysql_harness {

// forward declarations
class RandomGeneratorInterface;
namespace logging {
class Registry;
}  // namespace logging
class LoaderConfig;
class DynamicState;

class HARNESS_EXPORT DIM {  // DIM = Dependency Injection Manager

  // this class is a singleton
 protected:
  DIM();
  ~DIM();

 public:
  DIM(const DIM &) = delete;
  DIM &operator=(const DIM &) = delete;
  static DIM &instance();

  // Logging Registry
  //
  void set_static_LoggingRegistry(mysql_harness::logging::Registry *instance) {
    logging_registry_.set_static(instance);
  }

  void set_LoggingRegistry(
      mysql_harness::logging::Registry *instance,
      const std::function<void(mysql_harness::logging::Registry *)> &deleter) {
    logging_registry_.set(instance, deleter);
  }

  bool has_LoggingRegistry() const {
    return static_cast<bool>(logging_registry_);
  }

  mysql_harness::logging::Registry &get_LoggingRegistry() const {
    return logging_registry_.get();
  }

  // RandomGenerator
  void set_static_RandomGenerator(
      mysql_harness::RandomGeneratorInterface *inst) {
    random_generator_.set_static(inst);
  }

  void set_RandomGenerator(
      mysql_harness::RandomGeneratorInterface *inst,
      const std::function<void(mysql_harness::RandomGeneratorInterface *)>
          &deleter) {
    random_generator_.set(inst, deleter);
  }

  mysql_harness::RandomGeneratorInterface &get_RandomGenerator() const {
    return random_generator_.get();
  }

  // LoaderConfig

  void set_Config(
      mysql_harness::LoaderConfig *instance,
      const std::function<void(mysql_harness::LoaderConfig *)> &deleter) {
    loader_config_.set(instance, deleter);
  }

  bool has_Config() const { return static_cast<bool>(loader_config_); }

  mysql_harness::LoaderConfig &get_Config() const {
    return loader_config_.get();
  }

  // DynamicState

  void set_DynamicState(
      mysql_harness::DynamicState *instance,
      const std::function<void(mysql_harness::DynamicState *)> &deleter) {
    dynamic_state_.set(instance, deleter);
  }

  bool is_DynamicState() const { return static_cast<bool>(dynamic_state_); }

  mysql_harness::DynamicState &get_DynamicState() const {
    return dynamic_state_.get();
  }

 private:
  template <class T>
  class RWLockedUniquePtr {
   public:
    using value_type = T;

    void set_static(value_type *inst) {
      std::unique_lock lk(mtx_);

      inst_ = {inst, [](value_type *) {}};
    }

    void set(value_type *inst, std::function<void(value_type *)> deleter) {
      std::unique_lock lk(mtx_);

      inst_ = {inst, deleter};
    }

    value_type &get() const {
      std::shared_lock lk(mtx_);

      return *inst_;
    }

    void reset() {
      std::unique_lock lk(mtx_);

      inst_.reset();
    }

    explicit operator bool() const {
      std::shared_lock lk(mtx_);

      return static_cast<bool>(inst_);
    }

   private:
    UniquePtr<value_type> inst_;

    mutable std::shared_mutex mtx_;
  };

  RWLockedUniquePtr<mysql_harness::logging::Registry> logging_registry_;

  RWLockedUniquePtr<mysql_harness::RandomGeneratorInterface> random_generator_;

  RWLockedUniquePtr<mysql_harness::LoaderConfig> loader_config_;

  RWLockedUniquePtr<mysql_harness::DynamicState> dynamic_state_;

};  // class DIM

}  // namespace mysql_harness
#endif  // #ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
