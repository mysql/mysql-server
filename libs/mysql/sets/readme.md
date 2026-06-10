\page PageLibsMysqlSets Library: Sets

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
MySQL Library: Sets
===================
-->

Code documentation: @ref GroupLibsMysqlSets.

## Overview

This library provides classes to represent several forms of sets:

- Range-compressed sets of intervals. For example, large sets of integers such
  as 1-123456,125000-300200100 can be stored compactly.

- Nested sets composed of keys mapped to and any other set type. For example,
  maps like {"ABC": intervals(20-30,40-80), "FOO": intervals(1-31)} can be
  stored. Nested sets can be used to compose any other set type into more
  complex set types.

- The API is open-ended, allowing users to define other representations of sets,
  including other symbolic representations where not each element is stored
  explicitly.

The library defines the set operations
union/subtraction/intersection/complement; tests such as
equality/membership/subset/disjointness; size computation; and string
conversion. These are implemented for interval sets and nested sets, and are
optional to implement for user-defined set types.

Interval sets are parameterized by the element type, so they can store intervals
of any discrete type such as integers and dates, as well as continuous types
such as floating-point numbers or any totally ordered data type.

Nested sets are parameterized by the key type, which can be any totally ordered
type, and the value type, which can be any other set type. Thus, any existing or
future set types can be composed into more complex types.

Sets can be containers, which support modifications such as in-place
union/subtraction/intersection, assignment, clear, etc. Or they can be views,
which do not support modifications. The framework provides views over the
union/subtraction/intersection of two sets of the same type, views over the
complement of one set, and views over the empty set.

In more detail, the following APIs summarize the features that the library
provides:

- Interval Sets: All interval sets have member functions `[c]begin`, `[c]end`,
  `front`, `size`, `ssize`, `empty`, `operator bool`, which, respectively,
  provide iterators over intervals, determine the number of intervals
  (unsigned/signed), and determine emptiness. Any interval set is based on a
  Boundary Set. Boundary Sets are more convenient for implementing set
  operations. See details below. A reference to the Boundary Set can be obtained
  using the member function `boundaries`.

- `Interval_container`: This is an Interval Set as described above, with
  additional member modifiers `insert`, `remove`, `inplace_union`,
  `inplace_intersection`, `inplace_subtract`, `assign`, `clear`.
  `Interval_container` is based on `Boundary_container`, which is a Boundary Set
  that can be obtained from the `boundaries` member (see below).

- Nested Sets: All nested sets have member functions `[c]begin`, `[c]end`,
  `front`, `size`, `ssize`, `empty`, `operator bool`, `find`, `operator[]`,
  which provide iterators over key-value pairs, determine the number of
  key-value pairs, determine emptiness, and lookup keys, respectively.

- `Nested_container`: This is a Nested Set as described above, with additional
  member modifiers `insert`, `remove`, `inplace_union`, `inplace_intersection`,
  `inplace_subtract`, `assign`, `clear`. Any `Nested_container` is based on a
  Nested Storage, which is a lower-level container defining more primitive
  container modifiers. See below. A reference to the Nested Storage can be
  obtained from the member function `storage`.

- Predicates: `operator==`, `is_equal`, `contains_element`, `is_subset`,
  `is_superset`, `is_intersecting`, and `is_disjoint` return a truth value
  indicating how two intervals, two interval sets, an interval and an interval
  set, or two nested sets relate to each other.

- Set volume: `volume(set)` returns the number of elements in a set, as a
  floating-point number. For Interval sets and Boundary sets, this is the total
  length of all intervals (contrast with `set.size()`, which gives the number of
  intervals and number of boundaries, respectively). For nested sets, gives the
  sum of volumes of all contained Interval sets or Boundary sets.
  `volume_difference(set1, set2)` computes `volume(set1) - volume(set2)`, but
  without losing precision when the sets are large and their difference is
  small.

- Views over set operations: `Union_view`, `Intersection_view`,
  `Subtraction_view`, `Complement_view`. The views are Sets (as described
  above). They do not store the full result explicitly; rather, the iterators
  compute the result on-the-fly. The views are defined both over two Interval
  Sets of compatible types, and over two Nested Sets of compatible types. Users
  that define other sets types can specialize the views to the new types.

- String conversion: the free functions in the `mysql::strconv` library,
  `mysql::strconv::encode`, `mysql::strconv::decode`, and related
  functions, convert between object model and strings. There is support for both
  human-readable text format and space-efficient binary format.

- The default API is non-throwing. The containers and string conversion
  functions also exist in an alternative, throwing API, in the namespace
  `mysql::sets::throwing`.

- Boundary Sets: These represent sets of intervals as linear sequences of
  alternating start boundares and end boundaries. While Interval Sets have
  iterators that point to entire intervals, Boundary Sets have iterators that
  point to either the start or the end of an interval. Just like Interval Sets,
  Boundary Sets have member functions `[c]begin`, `[c]end`, `front`, `size`,
  `ssize`, `empty`, `operator bool`; the size members return twice the number
  that would be returned from an Interval Set. In addition, Boundary Sets have
  member functions `upper_bound` and `lower_bound`, which compute the iterator
  at or nearest following a given value, analogous to `std::upper_bound` and
  `std::lower_bound`. They are building blocks of most of the higher level set
  operations. The reason we need boundary sets in addition to interval sets is
  that `upper_bound` and `lower_bound` are expressed more naturally for boundary
  sets.

- `Boundary_container`: This is a Boundary Set as described above. Analogously
  to `Interval_container`, it has the additional member modifiers `insert`,
  `remove`, `inplace_union`, `inplace_intersection`, `inplace_subtract`,
  `assign`, and `clear`. Any `Boundary_container` is based on a Boundary
  Storage; see below. A reference to the Boundary Storage can be obtained from
  the member function `storage`.

- Boundary Storages: Lower-level container defining more primitive container
  modifiers, compared to `Boundary_container`. The storages implement the same
  member functions as Boundary Sets. In addition, they have the member modifiers
  `clear`, `insert`, `erase`, and `update_point`, which are low-level operations
  that do not generally preserve the invariants required by
  `Boundary_container`, which require boundaries to be in order. There are
  currently two implemented Boundary Storage classes: `Vector_boundary_storage`
  based on `std::vector` and `Map_boundary_storage` based on `std::map`. They
  provide different performance trade-offs (see below). Users should not invoke
  the API of a Boundary Storage directly, but need to choose which Boundary
  Storage implementation their containers should use.

- Nested Storages: These implement the same member functions as Nested Sets. In
  addition, they have the member modifiers `clear`, `emplace`, and `erase`.
  There is currently one nested storage class: Map_nested_storage, which is
  based on `std::map`.

## Intervals vs Boundaries

Sets of intervals are expressed in two equivalent ways:

- *Interval set*: Each element is an interval, and an interval has a start point
  and an end point.

- *Boundary set*: Each element is a boundary of an interval - either the start
  or the end. The number of elements is twice the number of intervals. Each
  boundary carries the information whether it is a start boundary or an end
  boundary. Boundary sets have two extra member functions compared to interval
  sets: `upper_bound` and `lower_bound`, which are building blocks for many of
  the algorithms.

In our object model, we assume that users usually access Interval Sets. Only
Interval Sets, not Boundary Sets, can be used as the mapped type in a Nested
Set. It is the Boundary Set that implements all logic; the Interval Set consists
of wrappers that relay all logic to the Boundary Set.

## Sorted and Range-Compressed; Inclusive vs Exclusive Boundary

Our Boundary Sets and Interval Sets are always sorted and range-compressed, and
never contain empty intervals. In the object model, start boundaries are
inclusive and end boundaries are exclusive. It follows that boundary sets are
strictly increasing. Human-readable string representations, on the other hand,
have both inclusive start boundaries and inclusive end boundaries.

Example:
  Consider the following set of integers:
    1, 2, 4, 5, 6, 7, 10
  This set is represented in the object model using the following boundary
  set:
    1, 3, 4, 8, 10, 11
  The corresponding interval set is:
    {1, 3}, {4, 8}, {10, 11}
  The human-readable text form is:
    1-2,4-7,10

Exclusive end boundaries are the best form to use in computations. They also
align with C++ standard idioms, such as begin/end iterators where begin is
inclusive and end is exclusive. Inclusive end boundaries are not practical to
use in computations, but are the de facto standard in natural language: for
example, in a Printer dialog box where the user can select page ranges, "1-3"
repesents page 1, 2, and 3; and a person that is on vacation "from the 23rd to
the 26th" will be expected back on the 27th only. We do not use inclusive end
boundaries anywhere else than in text representations.

## Storage

Our `Boundary_container` and `Nested_container` classes store boundaries in
low-level Storage classes. The Storage classes are specified as template
arguments. As described above, we provide both contiguous storage based on
`std::vector`, and associative storage based on `std::map`. These have different
performance trade-offs: generally, vector is more efficient for small
containers, but map has stronger asymptotic upper bounds and is more efficient
for large containers.

Vector-based storages keep values contiguously in order, so they have good cache
locality, few allocations, logarithmic upper_bound/lower_bound operations (using
binary search), but worst-case linear-time modifications. Map-based storages
keep values in nodes of a balanced tree, so there is an allocation per value,
which results in worse cache locality, but guarantees worst-case logarithmic
upper bounds on both upper_bound/lower_bound and modifying operations.

The worst-case time to perform N modifying operations in O(N*log(N)) for
map-based storages and O(N^2) for vector-based operations. Therefore, the
functions to convert from text format requires map-based storage, in order to
prevent that maliciously crafted input strings result in excessive CPU usage.

## Set Categories and Set Traits

We use *Set Categories* to distinguish different kinds of sets. The Set Category
is a type tag, and every set must provide its Category as the member type
`Set_category_t`. The Set Category is used to identify which implementation of
an algorithm to use, for example, when constructing
union/subtraction/intersection views, or when computing
membership/equality/subset/disjointness predicates. Each of these operations has
one implementation for Interval Sets, one of Boundary Sets, and one for Nested
Sets. The type tags `Interval_set_category_tag`, `Boundary_set_category_tag`,
and `Nested_set_category_tag` identify the correct function to call in each
case.

We use *Set Traits* to define parameters of the algorithms. Each Set type must
provide its Traits as the member type `Set_traits_t`. For Interval Sets and
Boundary Sets, the Set Traits define the element type, the comparison function
for the total order among elements, the minimum and maximum value, etc. For
Nested Sets, the Set Traits define the key type and the Set Traits for the
mapped type. Set Traits are used for two purposes:
- Algorithms on sets depend on the Set Traits. For example, an algorithm to
  compute unions of intervals has the element type as a template argument.
- Two Sets are only compatible if they have the same Traits. For example, it
  is possible to compoute the union of two sets of integers, but not the union
  of a set of integers and a set of strings.

We define a hierarchy of set trait types:

- Interval algorithms need to know an element type. We call traits that have a
  member that defines the element type *element traits*.

- Most interval algorithms also need Traits that can compare boundaries - the
  traits must be *ordered*.

- Many algorithms (for example, complement operations) also need to know the
  minimum and maximum. Ordered traits providing a minimum and maximum value are
  called *bounded*.

- If it is possible to compute successor and predecessor operations, the Set
  Traits are called *discrete*. This is used to convert between inclusive and
  exclusive end boundaries while converting to text.

- If it is possible to compute the distance between elements, to add and
  subtract distances, and to add distances to elements, the Set Traits are
  called *metric*. Metric set traits are necessary to compute interval lengths.

For example, integers are ordered, bounded, metric, and discrete; floating point
numbers are ordered, bounded and metric, strings are just ordered.

## Terminology and Concepts

- A *Set* is any class that has appropriate `Set_traits_t`/`Set_category_t`
  member types. The concept `Is_set` identifies sets.

- A *Boundary Iterator* is an iterator that provides a strictly increasing
  sequence of boundaries, and has a member function `bool is_endpoint()` that
  returns true/false for every second element. The concept is
  `Is_boundary_iterator`.

- A *Boundary Set* is a Set with appropriate category and traits, member
  functions `begin`, `end`, `size`, `ssize`, `empty`, `operator bool`,
  `lower_bound`, `upper_bound`, where the iterator type is a Boundary Iterator,
  and has an even number of boundaries of which the `end` is a start boundary.
  The concept is `Is_boundary_set`.

- A *Boundary Container* is a boundary set that can be modified through `clear`,
  `assign`, `insert`, `remove`, `inplace_update`, `inplace_intersect`, and
  `inplace_subtract`. The concept is `Is_boundary_container`.

- An *Interval* is a class template with members `start` and `exclusive_end`,
  representing a consecutive interval of some value type.

- An *Interval Iterator* is an iterator that provides a sequence of Intervals
  whose boundaries are strictly increasing. The concept is `Interval_iterator`.

- An *Interval Set* is a Set with appropriate category and traits, whose element
  type is `Interval`, and has a member function `boundaries` that provides the
  underlying boundary set. Contrary to boundary sets, interval sets don't have
  `upper_bound` and `lower_bound`. The concept is `Is_interval_set`.

- An *Interval Container* is an Interval Set that can be modified through
  `clear`, `assign`, `insert`, `remove`, `inplace_union`, `inplace_subtract`,
  and `inplace_intersect`. The concept is `Is_interval_container`.

- A *Nested Set* is a Set with appropriate category and traits, member functions
  `begin`, `end`, `size`, `ssize`, `empty`, `operator bool`, `find`, and
  `operator[]`. The concept is `Is_nested_set`.

- A *Nested Container* is a Nested Set that can be modified through
  `clear`, `assign`, `insert`, `remove`, `inplace_union`, `inplace_subtract`,
  and `inplace_intersect`. The concept is `Is_nested_container`.

- A *Set category* is a type that derives from `Base_set_category_tag`. The
  concept is `Is_set_category`.

- A *Set traits* is a type that derives from `Base_set_traits`. The concept is
  `Is_set_traits`.

## Non-throwing and throwing APIs for out-of-memory conditions

Containers may fail with out-of-memory conditions while inserting new values.
The default APIs for all Containers are non-throwing, returning
`mysql::utils::Return_status`. For Interval Containers and Boundary Containers,
we also provide alternative APIs which throw `bad_alloc`, in the
`mysql::sets::throwing` namespace.

The throwing APIs have copy constructors and copy assignment operators. The
default/non-throwing don't. However, the member function `assign` can be used to
copy containers and is defined for all types. Move constructors and move
assignment operators are non-throwing and defined for all containers.

The actual implementations for Intervals and Boundaries is
`mysql::sets::throwing`; the default API is wrappers that catch the exceptions
and convert them to `mysql::utils::Return_status` values. Interval Storages and
Boundary Storages are throwing; Nested Storages are non-throwing (but users do
not need to know that since the APIs are used only internally).

## String conversion

Any Set can be converted to a human-readable string using the
`mysql::strconv::encode` family of functions, and back from string using the
 `mysql::strconv::decode` family of functions.

For intervals/boundaries, three formats are supported: Text_format,
Binary_format, and Fixint_binary_format. The formats are as follows:

- `Text_format` is intended to be human-readable. It is enabled for Interval
  Sets/Boundary Sets with discrete Set Traits, for which there are string
  formatters/string parsers defined for the Element type. It stores an interval
  by writing the start boundary, a separator, and the *inclusive* end boundary,
  in text format. If the inclusive end boundary equals the start boundary, the
  separator and end boundary are omitted. It stores interval sets by
  representing all the boundaries using a separator to delimit adjacent pairs of
  boundaries. `encode` guarantees that intervals are disjoint and strictly
  increasing. `decode` allows intervals to be out of order and overlapping.
  It also allows that the inclusive end boundary is less than the start
  boundary: such intervals are treated as empty and hence ignored. For
  `decode`, the container type to store intervals must be
  `Map_boundary_storage` (identified using the member constant
  `has_fast_insertion`).

- `Binary_format` is intended to be space-efficient. It is enabled for discrete,
  metric Set traits for which there is string formatters/string parsers defined
  for the element type. It stores an integer length followed by the distances
  between adjacent boundaries, minus 1. If the first boundary is equal to the
  minimum for the Set Traits, the first boundary is omitted. (The decoder can
  determine if this is the case by checking if the length is odd.)

- `Fixint_binary_format` is intended to be easy to machine-parse. It is enabled
  for all Set traits for which there is string formatters/string parsers defined
  for the element type. It consists of the number of intervals, as an integer in
  64 bit little-endian, followed by all the boundaries.

For nested sets, the following formats are supported:

- `Text_format` is intended to be human-readable. It stores both keys and
  mapped objects by delegating the encoding to `encode`/`decode`.
