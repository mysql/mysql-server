/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_UNIQUEPTR_INCLUDED
#define MYSQL_HARNESS_UNIQUEPTR_INCLUDED

#include <assert.h>  // <cassert> is flawed: assert() lands in global namespace on Ubuntu 14.04, not std::
#include <functional>
#include <memory>

namespace mysql_harness {

/* @brief: improved std::unique_ptr super-class
 *
 * std::unique_ptr has one problem: it's tedious to use with custom deleters.
 * This class is a convenience super-class, which makes working with unique_ptr
 * easier. Advantages:
 *
 *   1. Improved constructor for easier type declaration (deleter type is
 *      automatically resolved):
 *
 *        UniquePtr<Foo> foo(new_foo(), foo_deleter);
 *        UniquePtr<Foo> bar = std::move(foo);
 *
 *      instead of:
 *
 *        std::unique_ptr<Foo, decltype(foo_deleter)> foo(new_foo(),
 * foo_deleter); std::unique_ptr<Foo, decltype(foo_deleter)> bar =
 * std::move(foo);
 *
 *
 *
 *   2. UniquePtr constructor ALWAYS defines a custom deleter, which is a must
 * if we're passing unique_ptr accross DLL boundaries (the idea is to release
 * the memory within the same DLL in which it was allocated). However, if all we
 * need is a default deleter, it will implicitly define it for us:
 *
 *        UniquePtr<Foo> foo(new_foo()); // std::default_delete is default 2nd
 * parameter
 *
 *      instead of:
 *
 *        std::unique_ptr<Foo, std::default_delete> foo(new_foo(),
 * std::default_delete());
 *
 *
 *
 *   3. In debug builds, it offers a safety feature to warn the developer, if
 * (s)he forgets about the custom deleter. See comments above release() method
 */
template <typename T>
class UniquePtr : public std::unique_ptr<T, std::function<void(T *)>> {
 public:
  UniquePtr() {}

  UniquePtr(T *ptr, std::function<void(T *)> deleter = std::default_delete<T>())
      : std::unique_ptr<T, std::function<void(T *)>>(ptr, deleter) {}

  UniquePtr(const UniquePtr<T> &) = delete;
  UniquePtr(UniquePtr<T> &&other)
      : std::unique_ptr<T, std::function<void(T *)>>::unique_ptr(
            std::move(other))
#ifndef NDEBUG
        ,
        get_deleter_called_(other.get_deleter_called_)
#endif
  {
  }

  UniquePtr &operator=(const UniquePtr<T> &) = delete;
  UniquePtr &operator=(UniquePtr<T> &&other) {
#ifndef NDEBUG
    get_deleter_called_ = other.get_deleter_called_;
#endif
    std::unique_ptr<T, std::function<void(T *)>>::operator=(std::move(other));
    return *this;
  }

// It is typical for developers never to worry about how they create and how
// they delete objects. Since we're using custom deleter, we want to make sure
// that the developer doesn't forget about that. We want to protect against a
// situation like so:
//
//   // some_place_1
//   UniquePtr<A> u_ptr(new A, my_custom_deleter(ptr); });
//
//   // some_place_2
//   A* raw_ptr = u_ptr.release();  // dev forgot that u_ptr also stores the
//   deleter to erase this object
//   ...
//   ...
//   delete raw_ptr;  // KABOOOM!!!
//
// So the least we can do, is try to remind the developer if we notice the dev
// didn't bother fetching the deleter.
//
#ifndef NDEBUG
  T *release() {
    assert(get_deleter_called_);  // do you know what you're doing? (how are you
                                  // going to delete this object?)
    return std::unique_ptr<T, std::function<void(T *)>>::release();
  }

  typename UniquePtr<T>::deleter_type get_deleter() {
    get_deleter_called_ = true;  // dev didn't forget about the deleter, good!
    return std::unique_ptr<T, std::function<void(T *)>>::get_deleter();
  }

 private:
  bool get_deleter_called_ = false;
#endif
};

}  // namespace mysql_harness
#endif  //#ifndef MYSQL_HARNESS_UNIQUEPTR_INCLUDED
