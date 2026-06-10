\page PageLibsMysqlIterators Library: Iterators

<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
MySQL Library: Iterators
========================
-->

Code documentation: @ref GroupLibsMysqlIterators.

## Overview

This library provides helper functionality for defining custom iterators:

- iterator_interface.h: The CRTP base class (mixin) `Iterator_interface` makes
  it easy to define iterator types that meet the C++ standard library iterator
  named requirements and concepts. See below for details.

- empty_sequence_iterator.h: The class template `Empty_sequence_iterator` is an
  iterator over an empty sequence of a given (default-constructible) type.

- meta.h: Defines metaprogramming utilities related to iterators:

  - `Iterator_concept_tag` gives the type tag for the strongest iterator
    concept satisfied by an iterator.

  - `Is_declared_legacy_input_iterator` and similar concepts determine if  an
    iterator, through `std::iterator_traits<It>::iterator_category`, is declared
    to satisfy each of the legacy iterator named requirements

- null_iterator.h: The function template `null_iterator` returns an iterator
  object for a given source type, created without passing a source object, while
  guaranteeing that two such iterators for the same type compare as equal.

  The use case is a view which does not have a source: then the view can return
  null iterators its their begin and end member functions, making it behave as a
  view over an empty range.

  Note that default-constructed iterators do not generally work in this case,
  because comparison for default-constructed standard library iterators is
  undefined behavior.

## Iterator_interface

### Comparison of iterator definitions with and without the library

To define, for example, a random access iterator, it suffices to inherit from
Iterator_interface and define 3 functions. This interface is already unit-tested
and carefully designed to satisfy both the semantic and the syntactic iterator
requirements. It works the same way whether the iterator returns by-value or
by-reference.

In contrast, to define a random access iterator without inheriting from
Iterator_interface, requires:
- implementing around 15-20 member functions, some of which have non-obvious
  return types;
- defining a handful of member types or specializing std::iterator_traits
- if the iterator returns by-value, requires defining a new class and
  understanding deep details about iterator requirements, pointer traits, and
  operator->.

### Supported iterator categories

`Iterator_interface` can be used to define input iterators, forward iterators,
bidirectional iterators, random access iterators, or contiguous iterators. Given
appropriate functions implemented by the user, the library deduces both the
correct standard named requirement (LegacyInputIterator, LegacyForwardIterator,
LegacyBidirectionalIterator, or LegacyRandomAccessIterator) and makes it satisfy
the standard C++20 concept (`std::input_iterator`, `std::forward_iterator`,
`std::bidirectional_iterator`, `std::random_access_iterator`, or
`std::contiguous_iterator`).

The benefits of the library are greater for the more advanced iterators. For
all categories, the library saves repetitive and error-prone work, and
requires less knowledge about standard library details, compared to an
iterator implementation without the library.

### Defining iterators of a particular category

To make `It` an iterator that iterates over values of type `V`, inherit like:

```
  class It : public Iterator_interface<It> {
    public:
    /* define member functions as described below */
  };
```

`It` must be default-constructible, copy/move-constructible, and
copy/move-assignable. In addition, define a subset of the following member
functions:

```
  // Exactly one of the following, to read the current value:
  V get() const;
  V &get() const;
  V *get_pointer() const;

  // At least one of next and advance to move the position; prev is optional.
  void next();
  void prev();
  void advance(std::ptrdiff_t);

  // Optionally one of the following, to compare iterators:
  bool is_equal(const It &) const;
  std::ptrdiff_t distance_from(const It &) const;
  bool is_sentinel() const;
  std::ptrdiff_t distance_from_sentinel() const;
```

C++ defines two hierarchies of iterators, which are confusingly similar and
subtly different:

- Pre-C++20 named requirements such as `LegacyInputIterator`, which can be
  queried at compile-time using `std::iterator_traits<It>::iterator_category`.

- C++20 concepts such as `std::input_iterator`, which can be queried at
  compile-time using these concepts.

This class deduces both the iterator category and the iterator concept, as
follows:

- `LegacyInputIterator` requries any `get` or `get_pointer` function, either
  `next` or `advance`, and either `is_equal` or `distance_from`.

- `LegacyForwardIterator` requries `LegacyInputIterator`, and that `get` returns
  by reference, and the class must be copyable.

- `LegacyBidirectionalIterator` requires `LegacyForwardIterator`, and either
  `prev` or `advance`.

- `LegacyRandomAccessIterator` requries `LegacyBidirectionalIterator`, and
  `advance`, and `distance_from`.

- `LegacyContiguousIterator` requries `LegacyRandomAccessIterator`, and requires
  `get_pointer`.

- `std::input_iterator` requries any `get` or `get_pointer` function, and either
  `next` or `advance`. (It is weaker than LegacyInputIterator because it does
  not require that iterators can be compared.)

- `std::forward_iterator` requries `std::input_iterator`, and either `is_equal`
  or `distance_from`, and the class must be copyable. (It is weaker than
  LegacyForwardIterator because it does not require that `get` returns by
  reference.)

- `std::bidirectional_iterator` requries `std::forward_iterator`, and either
  `prev` or `advance`. (It is weaker than LegacyBidirectionalIterator because it
  does not require that `get` returns by reference.)

- `std::random_access_iterator` requires `std::bidirectional_iterator`, and
  `advance` and `distance_from`. (It is weaker than LegacyRandomAccessIterator
  because it does not require that `get` returns by reference.)

- `std::contiguous_iterator` requires `std::random_access_iterator`, and
  requires `get_pointer`. This coincides with the requirements for
 `LegacyContiguousIterator`.

### Diagram of concept/category strengths

To summarize the previous section, the following diagram illustrates the
relative strengths between deduced iterator concepts and categories for types
derived from this class. The notation A-->B indicates that B is stronger that A,
i.e., requires everything that A requires and more. The abbreviations
I/F/B/R/C/ct/cy mean
input/forward/bidirectional/random_access/contiguous/concept/category,
respectively.

```
           1          2          4          5
    I_ct ----> I_cy ----> F_ct ----> B_ct ----> R_ct
                           |          |          |
                          3|         3|         3|
                           V     4    V     5    V     6
                          F_cy ----> B_cy ----> R_cy ----> C_ct==C_cy
```

Each arrow is annotated by a number that refers to the following list,
indicating what you need to implement to "follow the arrow":
(1) Implement `is_equal` to make iterators equality-comparable.
(2) Implement the copy constructor (actually, just don't delete it).
(3) Make `get` return by reference.
(4) Implement `prev` to enable moving backwards.
(5) Implement `advance` and `distance_from` instead of `get`/`next`/`is_equal`,
    to make it possible to take long steps.
(6) Implement `get_pointer` and ensure that returned objects are adjacent in
    memory.

### Iterators returning values, not references

Iterators that return value rather than reference can't meet the
`LegacyForwardIterator` requirement. This determines the behavior of standard
algorithms like `std::prev`, `std::advance`, and `std::distance`. Thus, just
because the iterator returns by value, both `std::advance(it, negative_number)`
and `std::prev` produce undefined behavior (typically an infinite loop), and
`std::distance` is linear-time (even if the iterator satisfies
`std::random_access_iterator`). Use `std::ranges::prev`, `std::ranges::advance`,
and `std::ranges::distance` instead.

### Sentinel types

If your iterator needs a sentinel type, this class limits it to be the
`mysql::iterators::Default_sentinel` type. Define one or both of the member
functions `is_sentinel` or `distance_from_sentinel`.
