\page PageLibsMysqlMeta Library: Meta

<!---
Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.
//
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->


<!--
MySQL Library: Meta
===================
-->

Code documentation: @ref GroupLibsMysqlMeta.

## Overview

This library contains generic features related to meta-programming, i.e.,
algorithms executed at compile-time, including concepts, type predicates, and
metafunctions (@see Concepts, type predicates, and metafunctions).

Other libraries may contain meta-programming utilities related to functions or
types defined in that library; this library is for "pure" meta-programming
utilities.

Currently, it contains the following:

- all_same.h: vararg version of the std::same_as concept, true if N types are
  equal.

- is_charlike.h: concepts to identify that a type is char, unsigned char, or
  std::byte.

- is_const_ref.h: concept that is true for types that are const ref.

- is_either.h: concept that is true if first argument equals any of the rest.

- is_pointer.h: concepts to identify that a type is a pointer.

- is_same_ignore_const.h: metaprogramming utility to determine if two types are
  equal, ignoring const-ness.

- is_specialization.h: utility to determine if a template is a specialization
  of another.

- not_decayed.h: concept used to protect a constructor taking forwarding
  reference arguments from being used as copy constructor.

  This is intended to be used in constructors taking (variadic) forwarding
  references, like:

  template <class... Args_t>
    requires Not_decayed<Type, Args...>
  Type::Type(Args_t &&...);

  When a `Type` object is *copied*, and this constructor would *not* have the
  constraint, this constructor could be a better match than the copy constructor
  `Type::Type(const Type &)`, according to the compiler's overload resolution
  rules. The reason is that the copy constructor requires a const argument,
  whereas the forwarding constructor accepts non-const arguments; thus if you
  copy a non-const object the forwarding constructor is preferred. The
  constraint prevents that this constructor is invoked with a single argument of
  type `Type`, `Type &`, `const Type &`, or `Type &&`.

- optional_is_same.h: concept taking one or two arguments; true if the second is
  omitted or void, or if the two types are the same.

## Concepts, type predicates, and metafunctions

We use the following terminology.

- metafunctions. A metafunction is like a "function" (in a broad sense) that
  takes a type as argument and returns a type. In C++ they are typically defined
  with `using` directives taking template arguments. Example:

  ```
  /// Value_type<X> equals X::value_type.
  template <class Container>
  using Value_type = typename Container::value_type;
  ```

- concepts, the C++20 feature. A concept is like a "function" that takes a type
  as argument and returns a boolean value. Example:

  ```
  /// Has_foo<X> is true if X has a nonstatic member foo that can be invoked.
  template <class Test>
  concept Has_foo = requires (Test test) { test.foo(); };
  ```

- type predicates. A type predicate is a struct that derives from
  std::integral_constant, and usually even std::bool_constant. Its purpose is
  the same as that of a concept: to take a type as argument and return a boolean
  value. See below for a comparison of the two tools. Example:

  ```
  /// Has_foo_predicate<X>::value is true if Has_foo<X> is true.
  template <class Test>
  struct Has_foo_predicate : public std::bool_constant<false> {};
  template <class Test>
    requires requires (Test test) { test.foo(); }
  struct Has_foo_predicate<Test> : public std::bool_constant<true> {};
  ```

### Choosing between concepts and type predicates

Although concepts and type predicates have exactly the same purpose, both these
tools are needed, because C++ puts different limitations on type predicates and
concepts:

- concepts, but not type predicates, are allowed in the following syntactic
  contexts:

  ```
  // constrained function parameter
  void f1(conceptname auto param);

  // constrained template arguments
  template <conceptname type>
  void f2();
  template <conceptname auto constant>
  void f3();

  // constraints on result types in requires expressions
  template <class T>
      requires (T obj) {
          { obj.f() } -> conceptname;
      }
  void f5();
  ```

- type predicates, but not concepts, can be passed as template arguments.
  Therefore, if a generic metafunction executes an algorithm, and a parameter of
  that algorithm is a function on types, then that parameter can be expressed as
  a type constraint, but not as a concept:

  ```
  // template parameter
  template <class first, class second, template <class> class predicate>
  concept both_match = predicate<first>::value && predicate<second>::value;
  ```
