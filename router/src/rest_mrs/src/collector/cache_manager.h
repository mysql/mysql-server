/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_CACHE_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_CACHE_MANAGER_H_

#include <list>
#include <mutex>
#include <utility>

namespace collector {

template <typename Obj>
class CacheManager {
 public:
  using Object = Obj;
  class CachedObject {
   public:
    CachedObject(CacheManager *parent = nullptr)
        : parent_{parent}, object_{nullptr} {}

    CachedObject(CachedObject &&other)
        : parent_{other.parent_}, object_{std::move(other.object_)} {
      other.parent_ = nullptr;
      other.object_ = nullptr;
    }

    template <typename... Args>
    CachedObject(CacheManager *parent, Args &&... args)
        : parent_{parent}, object_{std::forward<Args>(args)...} {}

    ~CachedObject() {
      if (parent_ && object_) parent_->return_instance(*this);
    }

    CachedObject &operator=(CachedObject &&other) {
      parent_ = other.parent_;
      object_ = other.object_;
      other.parent_ = nullptr;
      other.object_ = {};

      return *this;
    }

    bool operator==(const Object &obj) const { return obj == object_; }

    Object operator->() { return object_; }
    Object get() {
      if (object_ == nullptr && parent_) *this = parent_->get_instance();

      return object_;
    }

    /**
     * Mark that the object is dirty.
     *
     * Dirty object means that it is release by the manager
     * without trying to cache it. This functionality is useful,
     * when the user code, can't rollback changed done at instance
     * of `Obj` thus releasing is the  best option.
     */
    void set_dirty() { dirty_ = true; }

    /**
     * Mark the the object is clean.
     *
     * After marking object dirty, this method removes the dirty flag.
     * It is useful after successful processing `Obj`, and there is
     * no need of rollback its state.
     */
    void set_clean() { dirty_ = false; }

    // TODO(lkotula): Make those fields private (Shouldn't be in review)
    //   private:
    CacheManager *parent_;
    Object object_;
    bool dirty_{false};
  };

  class Callbacks {
   public:
    virtual ~Callbacks() = default;

    virtual bool object_before_cache(Object) = 0;
    virtual bool object_retrived_from_cache(Object) = 0;
    virtual void object_remove(Object) = 0;
    virtual Object object_allocate() = 0;
  };

 public:
  CacheManager(Callbacks *callbacks) : callbacks_{callbacks} {}

  virtual ~CacheManager() {
    while (!objects_.empty()) {
      callbacks_->object_remove(objects_.front());
      objects_.pop_front();
    }
  }

  CachedObject get_instance() {
    auto result = pop();

    return CachedObject{this, result};
  }

  void return_instance(CachedObject &object) {
    object.parent_ = nullptr;
    {
      std::unique_lock<std::mutex> lock(object_container_mutex_);
      if (objects_.size() < objects_limit_ && !object.dirty_) {
        if (callbacks_->object_before_cache(object.object_)) {
          objects_.push_back(std::move(object.object_));
          return;
        }
      }
    }
    callbacks_->object_remove(object.object_);
  }

  void change_cache_object_limit(uint32_t limit) {
    // Just set the new limit, still in the cache
    // there might be more objects than new limit,
    // lest leave. Those object will be removed at runtime.
    objects_limit_ = limit;
  }

  Callbacks *get_callbacks() const { return callbacks_; }

 private:
  Object pop() {
    {
      std::unique_lock<std::mutex> lock(object_container_mutex_);
      while (objects_.size()) {
        auto result = std::move(objects_.front());
        objects_.pop_front();

        if (callbacks_->object_retrived_from_cache(result)) {
          return result;
        }
        callbacks_->object_remove(result);
      }
    }
    return callbacks_->object_allocate();
  }

  uint32_t objects_limit_{20};
  std::mutex object_container_mutex_;
  std::list<Object> objects_;
  Callbacks *callbacks_{nullptr};
};

}  // namespace collector

#endif  // ROUTER_SRC_REST_MRS_SRC_CACHE_MANAGER_H_
