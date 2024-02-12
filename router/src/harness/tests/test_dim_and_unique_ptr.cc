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

////////////////////////////////////////
// Standard include files
#include <memory>

////////////////////////////////////////
// Third-party include files

#include <gmock/gmock.h>

////////////////////////////////////////
// Test system include files
#include "dim.h"
#include "test/helpers.h"

using mysql_harness::UniquePtr;
using ::testing::_;

class Notifier {
 public:
  MOCK_METHOD(void, called_ctor, (const std::string &));
  MOCK_METHOD(void, called_dtor, (const std::string &));
  MOCK_METHOD(void, called_deleter, (const std::string &));
  MOCK_METHOD(void, called_do_something, (const std::string &));
};

// GMock objects cannot be global, because EXPECT_CALL()s are evaluated in their
// destructors. The simplest workaround is to set a ptr to such a local object,
// and make that globally-accessible to the things that need it.
::testing::StrictMock<Notifier> *g_notifier = NULL;
void set_notifier(::testing::StrictMock<Notifier> *notifier) {
  g_notifier = notifier;
}

class A {
 public:
  A() {
    if (call_notifier_ && g_notifier) g_notifier->called_ctor("A");
  }

  explicit A(int)
      : call_notifier_(false) {
  }  // special ctor used by class B, that doesn't call notifier methods

  virtual ~A() {
    if (call_notifier_ && g_notifier) g_notifier->called_dtor("A");
  }

  A(const A &) { should_never_call_this("COPY CONSTRUCTOR"); }
  A(A &&) { should_never_call_this("MOVE CONSTRUCTOR"); }
  A &operator=(const A &) {
    should_never_call_this("COPY ASSIGNMENT");
    return *this;
  }
  A &operator=(A &&) {
    should_never_call_this("MOVE ASSIGNMENT");
    return *this;
  }
  static int &should_never_call_this_ignore_cnt() {
    return should_never_call_this_ignore_cnt_;
  }

  virtual void do_something() {}

 private:
  void should_never_call_this(const std::string &origin) {
    if (should_never_call_this_ignore_cnt_)
      should_never_call_this_ignore_cnt_--;
    else
      FAIL() << origin << " should never be called";
  }

  static int should_never_call_this_ignore_cnt_;
  bool call_notifier_ = true;
};
/*static*/ int A::should_never_call_this_ignore_cnt_;

class Deleter {
 public:
  void operator()(A *ptr) {
    if (g_notifier) g_notifier->called_deleter("-");
    delete ptr;
  }
};

void deleter(A *ptr) { Deleter()(ptr); }

////////////////////////////////////////////////////////////////////////////////
//
// UniquePtr tests
//
// Note: many of the tests found below seem like they don't do much. Keep in
// mind
//       that ability to build this test, is also a test! (templates are tricky)
//
////////////////////////////////////////////////////////////////////////////////

class UniquePtrTest : public ::testing::Test {
 protected:
  void SetUp() override { set_notifier(notifier_); }

  Notifier &get_notifier() { return notifier_; }

 private:
  ::testing::StrictMock<Notifier> notifier_;
};

TEST_F(UniquePtrTest, test_illegal_operations_warning) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(1);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(3);

  A::should_never_call_this_ignore_cnt() = 4;
  A a1;
  A a2(a1);
  a2 = a1;
  A a3(std::move(a2));
  a2 = std::move(a1);
  EXPECT_EQ(0, A::should_never_call_this_ignore_cnt());
}

TEST_F(UniquePtrTest, direct_creation) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(1);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(1);
  A a1;
}

TEST_F(UniquePtrTest, null_pointer) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(0);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(0);
  UniquePtr<A> p01;
  UniquePtr<A> p11(NULL);
  UniquePtr<A> p12(nullptr);
  UniquePtr<A> p21(NULL, NULL);
  UniquePtr<A> p22(nullptr, nullptr);
  UniquePtr<A> p23(NULL, nullptr);
  UniquePtr<A> p24(nullptr, NULL);
}

TEST_F(UniquePtrTest, null_pointer_with_custom_deleter) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(0);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(0);
  EXPECT_CALL(get_notifier(), called_deleter("-")).Times(0);
  UniquePtr<A> p11(nullptr, [](A *a) { delete a; });
  UniquePtr<A> p12(nullptr, deleter);
  UniquePtr<A> p13(nullptr, Deleter());
  UniquePtr<A> p14(nullptr, std::default_delete<A>());
  UniquePtr<A> p21(NULL, [](A *a) { delete a; });
  UniquePtr<A> p22(NULL, deleter);
  UniquePtr<A> p23(NULL, Deleter());
  UniquePtr<A> p24(NULL, std::default_delete<A>());
}

TEST_F(UniquePtrTest, deleter) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(5);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(5);
  EXPECT_CALL(get_notifier(), called_deleter("-")).Times(2);
  UniquePtr<A> p10(new A);
  UniquePtr<A> p11(new A, [](A *a) { delete a; });
  UniquePtr<A> p12(new A, deleter);
  UniquePtr<A> p13(new A, Deleter());
  UniquePtr<A> p14(new A, std::default_delete<A>());
}

TEST_F(UniquePtrTest, moving_stuff) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(1);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(1);

  UniquePtr<A> p0(new A);
  EXPECT_TRUE(!!p0);
  EXPECT_NE(p0.get(), nullptr);

  UniquePtr<A> p1(std::move(p0));
  EXPECT_TRUE(!!p1);
  EXPECT_FALSE(p0);

  UniquePtr<A> p2 = std::move(p1);
  EXPECT_TRUE(!!p2);
  EXPECT_FALSE(p1);

  UniquePtr<A> p3;
  p3 = std::move(p2);
  EXPECT_TRUE(!!p3);
  EXPECT_FALSE(p2);

  UniquePtr<A> p4;
  p4 = std::move(p3);
  EXPECT_TRUE(!!p4);
  EXPECT_FALSE(p3);

#ifndef __clang__
  // Clang generates -Wself-move warning, because self-move is undefined per
  // C++11 standard. However, a lot of people feel it should be a no-op. Our
  // UniquePtr adheres to that tighter specification.
  p4 = std::move(p4);
  EXPECT_TRUE(!!p4);
#endif
}

TEST_F(UniquePtrTest, shared_ptr_conversion) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(2);
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(2);
  EXPECT_CALL(get_notifier(), called_deleter("-")).Times(2);

  UniquePtr<A> p0(new A, deleter);
  std::shared_ptr<A> sp0 = std::move(p0);
  EXPECT_TRUE(!!sp0);
  EXPECT_FALSE(p0);

  UniquePtr<A> p1(new A, deleter);
  std::shared_ptr<A> sp1(std::move(p1));
  EXPECT_TRUE(!!sp1);
  EXPECT_FALSE(p1);
}

// Disabled this test because it behaves differently on different platforms.
// The culprit is the EXPECT_DEBUG_DEATH(), which does some voodoo magic that
// isn't very portable.  For example, it can fail on same compiler (VS2015u3)
// but on different Windows versions (7 vs 10 for example).  Swapping #ifdef
// blocks can fix this test on one machine, but will make it fail on another.
// Hopefully one day we can re-enable this test, if EXPECT_DEBUG_DEATH() becomes
// more portable.
#if !defined(__FreeBSD__)  // EXPECT_DEBUG_DEATH() doesn't build on BSD
TEST_F(UniquePtrTest, DISABLED_release_assertion) {
  EXPECT_CALL(get_notifier(), called_ctor("A")).Times(1);
#ifdef _WIN32
  EXPECT_CALL(get_notifier(), called_dtor("A")).Times(0);
  EXPECT_CALL(get_notifier(), called_deleter("-")).Times(0);
#else
  EXPECT_CALL(get_notifier(), called_dtor("A"))
      .Times(1);  // \_ the process would die
  EXPECT_CALL(get_notifier(), called_deleter("-")).Times(1);  // /  in real life
#endif
  UniquePtr<A> p1(new A, deleter);

  // switch the death test to thread-safe mode. More info:
  // https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#death-tests-and-threads
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  // NOTE: this line triggers plenty of valgrind warnings, disable when testing
  // with valgrind
#if 1
  EXPECT_DEBUG_DEATH(p1.release(), "");
#endif
}
#endif

TEST_F(UniquePtrTest, release_and_get_deleter) {
  UniquePtr<A>::deleter_type del;
  A *raw_ptr;
  {
    EXPECT_CALL(get_notifier(), called_ctor("A")).Times(1);
    raw_ptr = new A;
  }
  {
    EXPECT_CALL(get_notifier(), called_dtor("A")).Times(0);
    EXPECT_CALL(get_notifier(), called_deleter("-")).Times(0);
    UniquePtr<A> p1(raw_ptr, deleter);
    del = p1.get_deleter();
    p1.release();
  }
  {
    EXPECT_CALL(get_notifier(), called_dtor("A")).Times(1);
    EXPECT_CALL(get_notifier(), called_deleter("-")).Times(1);
    del(raw_ptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Dependency Injectino Manager (DIM) tests
//
////////////////////////////////////////////////////////////////////////////////

class Foo;
class Bar;
class Baz;
class Ext;

class TestDIM : public mysql_harness::DIM {
  // this class is a singleton
 private:
  TestDIM() {}

 public:
  TestDIM(const TestDIM &) = delete;
  TestDIM &operator=(const TestDIM &) = delete;

  static TestDIM &instance() {
    static TestDIM manager;
    return manager;
  }

 public:
  // factory and deleter setters [step 2]
  void set_A(
      const std::function<A *(void)> &factory,
      const std::function<void(A *)> &deleter = std::default_delete<A>()) {
    factory_A_ = factory;
    deleter_A_ = deleter;
  }
  void set_A(
      const std::function<A *(const char *)> &factory,
      const std::function<void(A *)> &deleter = std::default_delete<A>()) {
    factory_A1_ = factory;
    deleter_A_ = deleter;
  }
  void set_A(
      const std::function<A *(const char *, int)> &factory,
      const std::function<void(A *)> &deleter = std::default_delete<A>()) {
    factory_A2_ = factory;
    deleter_A_ = deleter;
  }
  void set_Foo(
      const std::function<Foo *(void)> &factory,
      const std::function<void(Foo *)> &deleter = std::default_delete<Foo>()) {
    factory_Foo_ = factory;
    deleter_Foo_ = deleter;
  }
  void set_Bar(
      const std::function<Bar *(void)> &factory,
      const std::function<void(Bar *)> &deleter = std::default_delete<Bar>()) {
    factory_Bar_ = factory;
    deleter_Bar_ = deleter;
  }
  void set_Baz(
      const std::function<Baz *(void)> &factory,
      const std::function<void(Baz *)> &deleter = std::default_delete<Baz>()) {
    factory_Baz_ = factory;
    deleter_Baz_ = deleter;
  }
  void set_Ext(
      const std::function<Ext *(void)> &factory,
      const std::function<void(Ext *)> &deleter = std::default_delete<Ext>()) {
    factory_Ext_ = factory;
    deleter_Ext_ = deleter;
  }
  void reset_Ext() { reset_generic(instance_Ext_); }

  // NOTE: for convenience of not writing two separate test classes, we have
  // both new_A() and get_A() here,
  //       but normally only one of those two methods would be implemented (we
  //       either want DIM to manage particular class as a singleton class or
  //       not). Ditto Foo, Bar and Baz.

  // object getters [step 3]
  A &get_A() const { return get_generic(factory_A_, deleter_A_); }
  Foo &get_Foo() const { return get_generic<Foo>(factory_Foo_, deleter_Foo_); }
  Bar &get_Bar() const { return get_generic<Bar>(factory_Bar_, deleter_Bar_); }
  Baz &get_Baz() const { return get_generic<Baz>(factory_Baz_, deleter_Baz_); }
  Ext &get_Ext() {
    return get_external_generic(instance_Ext_, factory_Ext_, deleter_Ext_);
  }

  // object creators [step 3]
  UniquePtr<A> new_A() const { return new_generic(factory_A_, deleter_A_); }
  UniquePtr<A> new_A(const char *arg1) const {
    return new_generic1(factory_A1_, deleter_A_, arg1);
  }
  UniquePtr<A> new_A(const char *arg1, int arg2) const {
    return new_generic2(factory_A2_, deleter_A_, arg1, arg2);
  }
  UniquePtr<Foo> new_Foo() const {
    return new_generic(factory_Foo_, deleter_Foo_);
  }
  UniquePtr<Bar> new_Bar() const {
    return new_generic(factory_Bar_, deleter_Bar_);
  }
  UniquePtr<Baz> new_Baz() const {
    return new_generic(factory_Baz_, deleter_Baz_);
  }

 private:
  // factory and deleter functions [step 4]
  std::function<A *(void)> factory_A_;
  std::function<A *(const char *)> factory_A1_;
  std::function<A *(const char *, int)> factory_A2_;
  std::function<void(A *)> deleter_A_;
  std::function<Foo *(void)> factory_Foo_;
  std::function<void(Foo *)> deleter_Foo_;
  std::function<Bar *(void)> factory_Bar_;
  std::function<void(Bar *)> deleter_Bar_;
  std::function<Baz *(void)> factory_Baz_;
  std::function<void(Baz *)> deleter_Baz_;
  std::function<Ext *(void)> factory_Ext_;
  std::function<void(Ext *)> deleter_Ext_;
  UniquePtr<Ext> instance_Ext_;
};

class DIMTest : public ::testing::Test {
 public:
  TestDIM &dim = TestDIM::instance();
};

class B : public A {
 public:
  B() : A(0) {
    if (g_notifier) g_notifier->called_ctor("B");
  }
  B(const char *arg1) : A(0) {
    if (g_notifier) {
      std::string s = std::string("B(") + arg1 + ")";
      g_notifier->called_ctor(s.c_str());
    }
  }
  B(const char *arg1, int arg2) : A(0) {
    if (g_notifier) {
      std::string s =
          std::string("B(") + arg1 + "," + std::to_string(arg2) + ")";
      g_notifier->called_ctor(s.c_str());
    }
  }

  ~B() override {
    if (g_notifier) g_notifier->called_dtor("B");
  }

  void do_something() override {
    if (g_notifier) g_notifier->called_do_something("B");
  }
};

TEST_F(DIMTest, singleton_simple) {
  // 1st get_A() call should create a new instance
  {
    ::testing::StrictMock<Notifier> notifier;
    set_notifier(notifier);
    EXPECT_CALL(notifier, called_ctor("B")).Times(1);
    EXPECT_CALL(notifier, called_dtor("B"))
        .Times(0);  // \_ singleton should outlive
    EXPECT_CALL(notifier, called_deleter("-")).Times(0);  // /  everything
    EXPECT_CALL(notifier, called_do_something("B")).Times(1);

    // multiple set_A() are ok (to allow overriding defaults) - only last one
    // matters (also use this opportunity to test passing various deleters - if
    // they'll trigger build failure)
    dim.set_A([]() { return nullptr; });
    dim.set_A([]() { return nullptr; }, [](A *) {});
    dim.set_A([]() { return nullptr; }, Deleter());
    dim.set_A([]() { return new A; });
    dim.set_A([]() { return new A; }, nullptr);
    dim.set_A([]() { return new B; }, deleter);  // only this one matters

    A &a = dim.get_A();
    a.do_something();
  }

  // subsequent get_A() calls should not create new instances
  {
    ::testing::StrictMock<Notifier> notifier;
    set_notifier(notifier);
    EXPECT_CALL(notifier, called_ctor("B"))
        .Times(0);  // no new instance should be created
    EXPECT_CALL(notifier, called_dtor("B")).Times(0);
    EXPECT_CALL(notifier, called_deleter("-")).Times(0);
    EXPECT_CALL(notifier, called_do_something("B")).Times(1);

    A &a = dim.get_A();
    a.do_something();
  }

  // calling set_A() should have no effect if singleton has already been created
  {
    ::testing::StrictMock<Notifier> notifier;
    set_notifier(notifier);
    EXPECT_CALL(notifier, called_ctor("B")).Times(0);
    EXPECT_CALL(notifier, called_dtor("B")).Times(0);
    EXPECT_CALL(notifier, called_deleter("-")).Times(0);
    EXPECT_CALL(notifier, called_do_something("B")).Times(1);

    // this brutal creator and deleter should never get called
    dim.set_A(
        []() {
          assert(0);
          return nullptr;
        },
        [](A *) { assert(0); });

    A &a = dim.get_A();
    a.do_something();
  }
}

// Foo depends on Bar and Baz,
// Bar depends on Baz and some int,
// Baz depends on nothing
class Baz {
 public:
  Baz() {
    if (g_notifier) g_notifier->called_ctor("Baz");
  }
};
class Bar {
 public:
  Bar(Baz, int) {
    if (g_notifier) g_notifier->called_ctor("Bar");
  }
};
class Foo {
 public:
  Foo(Bar, Baz) {
    if (g_notifier) g_notifier->called_ctor("Foo");
  }
  void do_something() {
    if (g_notifier) g_notifier->called_do_something("Foo");
  }
};

TEST_F(DIMTest, singleton_dependency_cascade) {
  // init factories
  {
    int n = 42;
    dim.set_Foo([this]() { return new Foo(dim.get_Bar(), dim.get_Baz()); });
    dim.set_Bar([this, n]() { return new Bar(dim.get_Baz(), n); });
    dim.set_Baz([]() { return new Baz; });
  }

  // should trigger creation of Foo, Bar and Baz
  {
    ::testing::StrictMock<Notifier> notifier;
    set_notifier(notifier);
    EXPECT_CALL(notifier, called_ctor("Foo")).Times(1);
    EXPECT_CALL(notifier, called_ctor("Bar")).Times(1);
    EXPECT_CALL(notifier, called_ctor("Baz")).Times(1);
    EXPECT_CALL(notifier, called_do_something("Foo")).Times(1);
    EXPECT_CALL(notifier, called_dtor(_))
        .Times(0);  // \_ singletons should outlive
    EXPECT_CALL(notifier, called_deleter(_)).Times(0);  // /  everything

    dim.get_Foo().do_something();
  }
}

void deleter0(A *ptr) {
  if (g_notifier) g_notifier->called_deleter("B0");
  delete ptr;
}

void deleter1(A *ptr) {
  if (g_notifier) g_notifier->called_deleter("B1");
  delete ptr;
}

void deleter2(A *ptr) {
  if (g_notifier) g_notifier->called_deleter("B2");
  delete ptr;
}

void deleterX(A *) { FAIL() << "This deleter should never be called"; }

TEST_F(DIMTest, factory_simple) {
  ::testing::StrictMock<Notifier> notifier;
  set_notifier(notifier);
  EXPECT_CALL(notifier, called_ctor("B")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1)")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1,2)")).Times(1);
  EXPECT_CALL(notifier, called_dtor("B")).Times(3);
  EXPECT_CALL(notifier, called_deleter("-")).Times(3);  // last deleter matters
  EXPECT_CALL(notifier, called_do_something("B")).Times(3);

  // note that different variants of set_A() share the deleter - only last one
  // set matters
  dim.set_A([]() { return new B; });
  dim.set_A([](const char *arg1) { return new B(arg1); },
            [](A *p) { delete p; });
  dim.set_A([](const char *arg1, int arg2) { return new B(arg1, arg2); },
            deleter);
  UniquePtr<A> a0 = dim.new_A();
  UniquePtr<A> a1 = dim.new_A("arg1");
  UniquePtr<A> a2 = dim.new_A("arg1", 2);
  a0->do_something();
  a1->do_something();
  a2->do_something();
}

TEST_F(DIMTest, factory_object_should_remember_its_deleter) {
  ::testing::StrictMock<Notifier> notifier;
  set_notifier(notifier);
  EXPECT_CALL(notifier, called_ctor("B")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1)")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1,2)")).Times(1);
  EXPECT_CALL(notifier, called_dtor("B")).Times(3);
  EXPECT_CALL(notifier, called_deleter("B0")).Times(1);
  EXPECT_CALL(notifier, called_deleter("B1")).Times(1);
  EXPECT_CALL(notifier, called_deleter("B2")).Times(1);

  // changing deleter should not affect objects already instantiated
  // (instantiated objects should "remember their deleter" - they should be
  // deleted with the deleter current at the time of their instantiation)
  {
    dim.set_A([]() { return new B; }, deleter0);
    UniquePtr<A> a0 = dim.new_A();
    dim.set_A([]() { return new B; }, deleterX);
  }
  {
    dim.set_A([](const char *arg1) { return new B(arg1); }, deleter1);
    UniquePtr<A> a1 = dim.new_A("arg1");
    dim.set_A([](const char *arg1) { return new B(arg1); }, deleterX);
  }
  {
    dim.set_A([](const char *arg1, int arg2) { return new B(arg1, arg2); },
              deleter2);
    UniquePtr<A> a2 = dim.new_A("arg1", 2);
    dim.set_A([](const char *arg1, int arg2) { return new B(arg1, arg2); },
              deleterX);
  }
}

TEST_F(DIMTest, factory_object_should_remember_its_deleter2) {
  ::testing::StrictMock<Notifier> notifier;
  set_notifier(notifier);
  EXPECT_CALL(notifier, called_ctor("B")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1)")).Times(1);
  EXPECT_CALL(notifier, called_ctor("B(arg1,2)")).Times(1);
  EXPECT_CALL(notifier, called_dtor("B")).Times(3);
  EXPECT_CALL(notifier, called_deleter("B0")).Times(1);
  EXPECT_CALL(notifier, called_deleter("B1")).Times(1);
  EXPECT_CALL(notifier, called_deleter("B2")).Times(1);

  // same idea as previous test, but with different variants overwriting the
  // deleter (all versions of get_A() share the same deleter)
  {
    dim.set_A([]() { return new B; }, deleter0);
    UniquePtr<A> a0 = dim.new_A();
    dim.set_A([](const char *arg1) { return new B(arg1); }, deleter1);
    UniquePtr<A> a1 = dim.new_A("arg1");
    dim.set_A([](const char *arg1, int arg2) { return new B(arg1, arg2); },
              deleter2);
    UniquePtr<A> a2 = dim.new_A("arg1", 2);
  }
}

class Ext {
 public:
  Ext(int xx) : x(xx) {}
  int x;
};

TEST_F(DIMTest, object_reset) {
  dim.set_Ext([]() { return new Ext(42); });  // set new factory
  EXPECT_EQ(dim.get_Ext().x, 42);             // the factory gets called here

  dim.set_Ext([]() { return new Ext(555); });  // set new factory again, ...
  EXPECT_EQ(dim.get_Ext().x, 42);  // but it will not be called yet, ...

  dim.reset_Ext();                  // until we reset the object.
  EXPECT_EQ(dim.get_Ext().x, 555);  // now it gets called!
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  int res = RUN_ALL_TESTS();

  // All the singletons will start running their destructors soon, and some of
  // them call g_notifier methods. But g_notifier is now a dangling pointer,
  // so here we set it to NULL so it's properly handled.
  g_notifier = nullptr;

  return res;
}
