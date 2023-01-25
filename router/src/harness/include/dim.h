/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
#define MYSQL_HARNESS_DIMANAGER_INCLUDED

#include "harness_export.h"
#include "unique_ptr.h"

#include <functional>
#include <mutex>  // using fwd declaration + ptr-to-implementation gives build errors on BSD-based systems
#include <string>  // unfortunately, std::string is a typedef and therefore not easy to forward-declare

/** @file
 *  @brief Provides simple, yet useful dependency injection mechanism
 *
 * # Introduction
 *
 * Let's start with showing usage, for example class Foo:
 *
 * @code
 *     class Foo {
 *      public:
 *       Foo();
 *       void do_something();
 *     };
 * @endcode
 *
 * We want DIM to make instance(s) of this class available throughout our
 * application.
 *
 * ## Scenario 1: when Foo is a singleton
 *
 * @code
 *     void init_code() {
 *       DIM::instance().set_Foo([](){ return new Foo; });
 *     }
 *
 *     void use_code() {
 *       Foo& foo = DIM::instance().get_Foo();
 *
 *       // each call works on the same object
 *       foo.do_something();
 *       foo.do_something();
 *       foo.do_something();
 *     }
 * @endcode
 *
 * ## Scenario 2: when Foo is not a singleton
 *
 * @code
 *     void init_code() {
 *       DIM::instance().set_Foo([](){ return new Foo; });
 *     }
 *
 *     void use_code() {
 *       // each call generates a new object
 *       UniquePtr<Foo> foo1 = DIM::instance().new_Foo();
 *       foo1->do_something();
 *
 *       UniquePtr<Foo> foo2 = DIM::instance().new_Foo();
 *       foo2->do_something();
 *
 *       UniquePtr<Foo> foo3 = DIM::instance().new_Foo();
 *       foo3->do_something();
 *     }
 * @endcode
 *
 * ## Scenario 3: when Foo already exists (typically used in unit tests)
 *
 * @code
 *     Foo foo_that_lives_forever;
 *
 *     void init_code() {
 *       DIM::instance().set_Foo(
 *         [](){
 *           return &foo_that_lives_forever;
 *         },
 *         [](Foo*) {}); // so that DIM does not try to delete it
 *     }
 *
 *     void use_code() {
 *       Foo& foo = DIM::instance().get_Foo();
 *       foo.do_something();
 *     }
 * @endcode
 *
 * Convenient, isn't it?  But to make all this happen, class Foo (boilerplate
 * code) has to be added to DIM class.
 *
 * # Usage
 *
 * Adding a new managed object is done in 4 steps:
 *
 * 1. add class forward declaration
 * 2. add object factory + deleter setter
 * 3. add singleton object getter or object creator. Adding both usually makes
 * no sense
 * 4. add factory and deleter function objects
 *
 * Here is the (relevant part of) class DIM for class Foo:
 *
 *
 * @code
 *     // [step 1]
 *     // forward declarations
 *     class Foo;
 *
 *     class DIM {
 *       // ... constructors, instance(), other support methods ...
 *
 *      public:
 *       // [step 2]
 *       // factory + deleter setter
 *       void set_Foo(const std::function<Foo*(void)>& factory,
 *         const std::function<void(Foo*)>& deleter =
 *             std::default_delete<Foo>()) {
 *           factory_Foo_ = factory; deleter_Foo_ = deleter;
 *       }
 *
 *       // [step 3]
 *       // singleton object getter
 *       // (shown here, but normally mutually-exclusive with next method)
 *       Foo& get_Foo() const {
 *         return get_generic<Foo>(factory_Foo_, deleter_Foo_);
 *       }
 *
 *       // object creator
 *       // (shown here, but normally mutually-exclusive with previous method)
 *       UniquePtr<Foo> new_Foo() const {
 *         return new_generic(factory_Foo_, deleter_Foo_);
 *       }
 *
 *      private:
 *       // factory and deleter function objects [step 4]
 *       std::function<Foo*(void)> factory_Foo_;
 *       std::function<void(Foo*)> deleter_Foo_;
 *     };
 * @endcode
 *
 *
 * ## Example
 *
 * @code
 *     // forward declarations [step 1]
 *     class Foo;
 *     class Bar;
 *     class Baz;
 *
 *     class DIM {
 *       // ... constructors, instance(), other support methods ...
 *
 *       // Example: Foo depends on Bar and Baz,
 *       //          Bar depends on Baz and some int,
 *       //          Baz depends on nothing
 *
 *      public:
 *       // factory + deleter setters [step 2]
 *       void set_Foo(const std::function<Foo*(void)>& factory,
 *         const std::function<void(Foo*)>& deleter =
 *             std::default_delete<Foo>()) {
 *           factory_Foo_ = factory; deleter_Foo_ = deleter;
 *       }
 *
 *       void set_Bar(const std::function<Bar*(void)>& factory,
 *         const std::function<void(Bar*)>& deleter =
 *             std::default_delete<Bar>()) {
 *           factory_Bar_ = factory; deleter_Bar_ = deleter;
 *       }
 *
 *       void set_Baz(const std::function<Baz*(void)>& factory,
 *         const std::function<void(Baz*)>& deleter =
 *             std::default_delete<Baz>()) {
 *           factory_Baz_ = factory; deleter_Baz_ = deleter;
 *       }
 *
 *       // singleton object getters
 *       // (all are shown, but normally mutually-exclusive
 *       // with next group) [step 3]
 *       Foo& get_Foo() const {
 *         return get_generic<Foo>(factory_Foo_, deleter_Foo_);
 *       }
 *       Bar& get_Bar() const {
 *         return get_generic<Bar>(factory_Bar_, deleter_Bar_);
 *       }
 *       Baz& get_Baz() const {
 *         return get_generic<Baz>(factory_Baz_, deleter_Baz_);
 *       }
 *
 *       // object creators
 *       // (all are shown, but normally mutually-exclusive
 *       // with previous group) [step 3]
 *       UniquePtr<Foo> new_Foo() const {
 *         return new_generic(factory_Foo_, deleter_Foo_);
 *       }
 *       UniquePtr<Bar> new_Bar() const {
 *         return new_generic(factory_Bar_, deleter_Bar_);
 *       }
 *       UniquePtr<Baz> new_Baz() const {
 *         return new_generic(factory_Baz_, deleter_Baz_);
 *       }
 *
 *      private:
 *       // factory and deleter function objects [step 4]
 *       std::function<Foo*(void)> factory_Foo_;
 *       std::function<void(Foo*)> deleter_Foo_;
 *       std::function<Bar*(void)> factory_Bar_;
 *       std::function<void(Bar*)> deleter_Bar_;
 *       std::function<Baz*(void)> factory_Baz_;
 *       std::function<void(Baz*)> deleter_Baz_;
 *     };
 *
 *
 *
 *     // actual classes
 *     struct Baz {
 *       Baz() {}
 *     };
 *     struct Bar {
 *       Bar(Baz, int) {}
 *     };
 *     struct Foo {
 *       Foo(Bar, Baz) {}
 *       void do_something() {}
 *     };
 *
 *
 *
 *     // usage
 *     int main() {
 *       int n = 3306;
 *
 *       // init code
 *       DIM& dim = DIM::instance();
 *       dim.set_Foo([&dim]()    {
 *           return new Foo(dim.get_Bar(), dim.get_Baz()); });
 *       dim.set_Bar([&dim, n]() {
 *           return new Bar(dim.get_Baz(), n);             });
 *       dim.set_Baz([]()        {
 *           return new Baz;                               });
 *
 *       // use code (as singleton)
 *       //
 *       // will automatically instantiate Bar and Baz as well
 *       dim.get_Foo().do_something();
 *
 *       // use code (as new object)
 *       UniquePtr<Foo> foo = dim.new_Foo();
 *       foo->do_something();
 *     }
 * @endcode
 *
 * # Object Reset
 *
 * There's also an option to reset an object managed by DIM, should you need it.
 * Normally, on the first call to get_Foo(), it will call the factory_Foo_() to
 * create the object before returning it. On subsequent calls, it will just
 * return that Foo object previously created. But what if you needed to reset
 * that object? And perhaps to create it via another Foo factory method, or with
 * different parameters?
 *
 * For such case, we can define reset_Foo() method, which will reset the Foo
 * object back to nullptr. The Foo object can no longer be kept inside of
 * get_Foo(), because it has to be modifiable via reset_Foo(). Here's the code:
 *
 *
 * @code
 *     // Foo-related members.
 *     //
 *     // instance_Foo_ is new here, it now stores the Foo object
 *     //
 *     // (previously, this object was stored as a static variable
 *     // inside of get_Foo()
 *     std::function<Foo*(void)> factory_Foo_;
 *     std::function<void(Foo*)> deleter_Foo_;
 *     UniquePtr<Foo>            instance_Foo_; // <---- new member
 *
 *     // getter now relies on get_external_generic() to manage the Foo object
 *     Foo& get_Foo() {
 *       return get_external_generic(instance_Foo_,
 *                                   factory_Foo_,
 *                                   deleter_Foo_);
 *     }
 *
 *     // this is our new function.
 *     //
 *     // After calling it, set_Foo() can be used again
 *     // to set the factory method, which will be
 *     // triggered on subsequent call to get_Foo() to
 *     // create the new Foo object
 *     void reset_Foo() { reset_generic(instance_Foo_); }
 *
 *     // set_Foo remains unaltered
 *     void set_Foo(const std::function<Foo*(void)>& factory,
 *         const std::function<void(Foo*)>& deleter =
 *           std::default_delete<Foo>()) {
 *       factory_Foo_ = factory;
 *       deleter_Foo_ = deleter;
 *     }
 * @endcode
 *
 * ## Example
 *
 * @code
 *     // init code
 *     DIM& dim = DIM::instance();
 *     dim.set_Foo([]() { return new Foo(42); });
 *
 *     // use code
 *
 *     // automatically calls set_Foo() which returns new Foo(42)
 *     dim.get_Foo().do_something();
 *
 *     // does not call set_Foo() anymore
 *     dim.get_Foo().do_something();
 *
 *     // does not call set_Foo() anymore
 *     dim.get_Foo().do_something();
 *
 *     // sets new creating function
 *     dim.set_Foo([]() {
 *         return new Foo(555);
 *     });
 *     // but the new set_Foo() is still not called
 *     dim.get_Foo().do_something();
 *
 *     dim.reset_Foo();
 *
 *     // automatically calls (new) set_Foo(), which returns new Foo(555)
 *     dim.get_Foo().do_something();
 * @endcode
 *
 */

// forward declarations [step 1]
namespace mysqlrouter {
class Ofstream;
}
namespace mysql_harness {
class RandomGeneratorInterface;
}
namespace mysql_harness {
namespace logging {
class Registry;
}
}  // namespace mysql_harness
namespace mysql_harness {
class LoaderConfig;
}
namespace mysql_harness {
class DynamicState;
}

namespace mysql_harness {

class HARNESS_EXPORT DIM {  // DIM = Dependency Injection Manager

  // this class is a singleton
 protected:
  DIM();
  ~DIM();

 public:
  DIM(const DIM &) = delete;
  DIM &operator=(const DIM &) = delete;
  static DIM &instance();

  // NOTE: once we gain confidence in this DIM and we can treat it as black box,
  //       all the boilerplate stuff (steps 2-4) for each class can be generated
  //       by a macro)

 public:
  ////////////////////////////////////////////////////////////////////////////////
  // factory and deleter setters [step 2]
  ////////////////////////////////////////////////////////////////////////////////

  // Logging Registry
  void reset_LoggingRegistry() { reset_generic(instance_LoggingRegistry_); }
  void set_LoggingRegistry(
      const std::function<mysql_harness::logging::Registry *(void)> &factory,
      const std::function<void(mysql_harness::logging::Registry *)> &deleter) {
    factory_LoggingRegistry_ = factory;
    deleter_LoggingRegistry_ = deleter;
  }

  // RandomGenerator
  void set_RandomGenerator(
      const std::function<mysql_harness::RandomGeneratorInterface *(void)>
          &factory,
      const std::function<void(mysql_harness::RandomGeneratorInterface *)>
          &deleter) {
    factory_RandomGenerator_ = factory;
    deleter_RandomGenerator_ = deleter;
  }

  // LoaderConfig
  void reset_Config() { reset_generic(instance_Config_); }
  void set_Config(
      const std::function<mysql_harness::LoaderConfig *(void)> &factory,
      const std::function<void(mysql_harness::LoaderConfig *)> &deleter) {
    factory_Config_ = factory;
    deleter_Config_ = deleter;
  }

  // DynamicState
  void reset_DynamicState() { reset_generic(instance_DynamicState_); }
  void set_DynamicState(
      const std::function<mysql_harness::DynamicState *(void)> &factory,
      const std::function<void(mysql_harness::DynamicState *)> &deleter) {
    factory_DynamicState_ = factory;
    deleter_DynamicState_ = deleter;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // object getters [step 3] (used for singleton objects)
  ////////////////////////////////////////////////////////////////////////////////

  // Logging Registry
  mysql_harness::logging::Registry &get_LoggingRegistry() {
    return get_external_generic(instance_LoggingRegistry_,
                                factory_LoggingRegistry_,
                                deleter_LoggingRegistry_);
  }

  // RandomGenerator
  mysql_harness::RandomGeneratorInterface &get_RandomGenerator() const {
    return get_generic(factory_RandomGenerator_, deleter_RandomGenerator_);
  }

  // LoaderConfig
  mysql_harness::LoaderConfig &get_Config() {
    return get_external_generic(instance_Config_, factory_Config_,
                                deleter_Config_);
  }

  // DynamicState
  bool is_DynamicState() { return (bool)instance_DynamicState_; }
  mysql_harness::DynamicState &get_DynamicState() {
    return get_external_generic(instance_DynamicState_, factory_DynamicState_,
                                deleter_DynamicState_);
  }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  // factory and deleter functions [step 4]
  ////////////////////////////////////////////////////////////////////////////////

  // Logging Registry
  std::function<mysql_harness::logging::Registry *(void)>
      factory_LoggingRegistry_;
  std::function<void(mysql_harness::logging::Registry *)>
      deleter_LoggingRegistry_;
  UniquePtr<mysql_harness::logging::Registry> instance_LoggingRegistry_;

  // RandomGenerator
  std::function<mysql_harness::RandomGeneratorInterface *(void)>
      factory_RandomGenerator_;
  std::function<void(mysql_harness::RandomGeneratorInterface *)>
      deleter_RandomGenerator_;

  // LoaderConfig
  std::function<mysql_harness::LoaderConfig *(void)> factory_Config_;
  std::function<void(mysql_harness::LoaderConfig *)> deleter_Config_;
  UniquePtr<mysql_harness::LoaderConfig> instance_Config_;

  // DynamicState
  std::function<mysql_harness::DynamicState *(void)> factory_DynamicState_;
  std::function<void(mysql_harness::DynamicState *)> deleter_DynamicState_;
  UniquePtr<mysql_harness::DynamicState> instance_DynamicState_;

  ////////////////////////////////////////////////////////////////////////////////
  // utility functions
  ////////////////////////////////////////////////////////////////////////////////

 protected:
  template <typename T>
  static T &get_generic(const std::function<T *(void)> &factory,
                        const std::function<void(T *)> &deleter) {
    static UniquePtr<T> obj = new_generic(factory, deleter);
    return *obj;
  }

  // new_generic*() (add more variants if needed, or convert into varargs
  // template)
  template <typename T>
  static UniquePtr<T> new_generic(const std::function<T *(void)> &factory,
                                  const std::function<void(T *)> &deleter) {
    return UniquePtr<T>(factory(),
                        [deleter](T *p) {
                          deleter(p);
                        }  // [&deleter] would be unsafe if set_T() was called
                           // before this object got erased
    );
  }
  template <typename T, typename A1>
  static UniquePtr<T> new_generic1(const std::function<T *(A1)> &factory,
                                   const std::function<void(T *)> &deleter,
                                   const A1 &a1) {
    return UniquePtr<T>(factory(a1),
                        [deleter](T *p) {
                          deleter(p);
                        }  // [&deleter] would be unsafe if set_T() was called
                           // before this object got erased
    );
  }
  template <typename T, typename A1, typename A2>
  static UniquePtr<T> new_generic2(const std::function<T *(A1, A2)> &factory,
                                   const std::function<void(T *)> &deleter,
                                   const A1 &a1, const A2 &a2) {
    return UniquePtr<T>(factory(a1, a2),
                        [deleter](T *p) {
                          deleter(p);
                        }  // [&deleter] would be unsafe if set_T() was called
                           // before this object got erased
    );
  }

  template <typename T>
  T &get_external_generic(UniquePtr<T> &object,
                          const std::function<T *()> &factory,
                          const std::function<void(T *)> &deleter) {
    mtx_.lock();
    std::shared_ptr<void> exit_trigger(nullptr, [&](void *) { mtx_.unlock(); });

    if (!object) object = new_generic(factory, deleter);

    return *object;
  }

  template <typename T>
  void reset_generic(UniquePtr<T> &object) {
    mtx_.lock();
    std::shared_ptr<void> exit_trigger(nullptr, [&](void *) { mtx_.unlock(); });

    object.reset();
  }

  mutable std::recursive_mutex mtx_;

};  // class DIM

}  // namespace mysql_harness
#endif  //#ifndef MYSQL_HARNESS_DIMANAGER_INCLUDED
