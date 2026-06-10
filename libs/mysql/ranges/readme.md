\page PageLibsMysqlRanges Library: Ranges

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
MySQL Library: Ranges
=====================
-->

Code documentation: @ref GroupLibsMysqlRanges.

## Overview

This library provides general-purpose C++ range definitions.  Currently it is
the following:

- buffer_interface.h: the class `Buffer_interface` is a CRTP base class/mixin,
  which given implementations of `data` and `size`, provides the members
  `begin`, `end`, `operator bool`, `empty`, `string_view`; subscript operator;
  comparison operators; `std::hash`; variants of all member functions that use
  `unsigned char` or `std::byte` instead of `char`; and `ssize` that returns
  `ptrdiff_t` (signed) instead of `size_t`. The resulting class satisfies
  `std::ranges::range`.

  User-defined classes that behave like character buffers can inherit from
  this class to provide a richer and more easily used API.

  While all member functions have default implementations, for example `empty`
  returns true if the `size()` member of the user-defined class returns 0, the
  user-defined class can override functions for which it can provide faster
  implementations.

- collection_interface.h: the class `Collection_interface` is an enhanced
  version of `std::ranges::view_interface`: this is a CRTP base class/mixin,
  which given implementations of `begin()` and `end()`, provides the members
  `[c][r]begin()`, `[c][r]end()`, `[s]size()`, `empty()`, `operator!()`,
  `operator bool()`, `front()`, `back()`, `data()`, `operator[]`. We need this
  because not all compilers we build on (as of 2025) have implemented
  `std::ranges::view_interface` yet, and also because we want the C++23
  extensions and some non-standard extensions.

  User-defined classes that behave like collections of objects can inherit from
  this class to provide a richer and more easily used API. The interface will
  provide only those member functions that match the iterator type, so
  `[c]rbegin`, `[c]rend`, and `back` are available only for bidirectional
  iterators; `operator[]` only for random access iterators, and `data` only for
  contiguous iterators.

  While all member functions have default implementations, for example `size`
  uses `std::ranges::distance` with the begin and end iterators, the
  user-defined class can override functions for which is can provide faster
  implementations.

- disjoint_pairs_view.h: The classes `Disjoint_pairs_interface`,
  `Disjoint_pairs_view`, and `Disjoint_pairs_interface` are a CRTP base
  class/mixin, a view, and an iterator. Given an even-length sequence, they
  provides a sequence of half the length consisting of pairs from the original
  sequence.

  Algorithms that need to inspect elements of a range pair-by-pair can use this
  as a convenience.

  This is optimized for minimizing the number of iterator advancements: it
  always caches an iterator to the first element of the pair, and lazily caches
  an iterator to the second element of the pair whenever that is needed.

- iterator_with_range.h: The class `Iterator_with_range` is a wrapper around an
  iterator, which provides members to get a reference to the source range. In
  particular, unlike iterators in general, this iterator knows whether it is
  positioned at the end.

  Algorithms that operate on iterators, and sometimes need to either access the
  underlying range or just test if an iterator has reached the end, can use this
  class to bundle all they need in one object, rather than having to use two
  objects.

  This is designed to work even in cases where the end iterator may sometimes be
  invalidated; in particular, it does not cache the end iterator, but gets it
  from the container each time the user queries if the iterator has reached the
  end.

- flat_view.h: Given a nested data structure, such as `set<vector<map<float,
  list<int>>>>`, the class `Flat_view<set<vector<map<float, list<int>>>>>`
  provides a linear range view over a sequence over the innermost elements, the
  `int`s. If any of the containers are empty, they are omitted.

  This is usable whenever an algorithm needs to process only the innermost
  elements of a nested data structure. The algorithm only needs to be
  implemented for flat ranges; users can pass the flat view of the nested data
  structures to it.

  It is designed so that:
  - It skips empty data structures, so the output range only yields valid
    elements of the innermost type.
  - It works with arbitrarily deeply nested data structures. Each level must
    be homogenic, but different levels can be different data structures.
  - Users can provide custom functions to extract the range of inner data
    structures from a given type. Thus, for sequence structures such as `vector`
    or `list`, the `begin` and `end` iterators can be used directly; for
    key-value data structures such as `map` or `unordered_map`, you can instruct
    the view to use only the second component (which it may in turn flatten).
    See also `Mapped_view` below.

  The first property may require iterating over multiple empty data structures.
  The skipping of empty data structures occurs when constructing the begin
  iterator and when advancing the iterator; those operations are therefore not
  worst-case constant-time. Comparison and dereference require only a constant
  number of evaluations of the underlying iterators.

- meta.h: metaprogramming features (type aliases and concepts) to determine if a
  class is a collection, and extract iterator and value types from it.

  This is usable in class and function templates that need to restrict the types
  they operate on, or in code that needs to deduce the iterator or value type
  from a container or from an iterator.

- projection_views.h: Defines the views `Key_view<R>` and `Mapped_view<R>`. For
  a given range `R` over pairs, they provide views over only the first component
  and only the second component, respectively. For example, the iterator
  `Key_view<std::map<K, M>>` yields values of type `K`, whereas
  `Mapped_view<std::map<K, M>>` yields values of type `M`. Also
  `Projection_view<N, R>` provides a view over the Nth component of tuple-like
  objects. And for an iterator `I` over pairs/tuple-like objects,
  `Key_iterator<I>`, `Mapped_iterator<I>` and `Projection_iterator<N, I>` are
  the iterators.

  These are usable in algorithms that need to iterate over just the keys or
  values of a view. In particular, `Mapped_view` is usable with `Flat_view` to
  unfold the second component of the pairs from a `map`.

- transform_view.h: Defines the view `Transform_view<T, R>`. For a given range
  `R` and transformation `T`, it provides iterators over transformed elements.

  This is usable in algorithms that need to apply a function to each element of
  a range.

- view_sources.h: Support types to be used in view classes and their iterators,
  for members holding (references to) the view's source(s). The types intend to
  prevent dangling references by using the following rule:

    "Views and their iterators shall represent sources that are views by-value,
    and sources that are not views by-reference."

  The concept `std::ranges::view<T>` identifies that a source `T` is a view. See
  below for details about the dangling reference problem. This header provides
  the following types:

  - `View_source<T>`: Class for view sources. The internal representation is
    `T` if `std::ranges::view<T>`, and `const T *` otherwise.

  - `Optional_view_source<T>`: Class for view sources that are optional. The
    internal representation of the source is `std::optional<T>` if
    `std::ranges::view<T>`, and`const T *` otherwise. The API is similar to
    `std::optional<T>`. When this does not hold a source object, it behaves as
    if the source object is an empty range.

  - `Raw_view_source<T>`: Type alias for view sources, equal to `T` if
    `std::ranges::view<T>` and `T &` otherwise. This is a more simplistic
    variant of `View_source`. However, `View_source` is default-constructible if
    `T` is, whereas `T&` is not default-constructible. Therefore, `View_source`
    is preferred for views that need to be default-constructible.

  This is usable in any user-defined view or user-defined iterator that needs
  to hold references to an underlying source.

## View_source and the dangling reference problem

In the following discussion, we use the following terminology (part of which is
a special case of standard terminology such as std::ranges::view).

- A *container* owns elements, and provides iterators that yield references to
  the elements.

- A *view* provides iterators that yield elements that the view does not own.
  The elements are either produced by the iterators, or retrieved from a source;
  for the remainder of the discussion we restrict attention to the latter case.
  The source may be a container or a view. Iterators provided by views may yield
  either references (to elements owned by a source container) or values
  (generated by the view).

- A *nested container* is a container whose elements are containers. The nested
  container is also called an outer container and the iterators it provides are
  called outer iterators. Its elements are called inner containers and the
  iterators that the inner containers provide are called inner iterators. An
  example of a nested container is a vector of vectors.

- A *nested view* is a view whose elements are views (not to be confused with a
  view whose source is a view). The nested view is also called an outer view and
  its iterators are called outer iterators. Its elements are called inner views
  and the iterators that the inner views provide are called inner iterators.

- A *primary view* is a view whose source is a container. A *secondary view* is
  a view whose source is a view. It is possible to have *chains* of views, so
  the source of a secondary view may itself be either a primary view or a
  secondary view.

The potential issues with dangling references occur when a view references its
source, or when a view's iterators reference the view's source or the view's
source's iterators. The summary is that, while user must ensure that *primary*
views outlive their source, a *secondary view* must hold copies of its source to
prevent dangling references. This prevents dangling references in chains. More
precisely:

- When views and their iterators need to represent their sources in member
  variables, it can be by-value or by-reference.

- Primary views and their iterators must represent their sources by-reference.
  (Otherwise, they would copy the entire container, which is expensive, and
  would make the view out-of-date in case the container is altered.) The user
  must ensure that the storage duration of the source is longer than the storage
  duration of the primary view. (Otherwise the primary view's references will
  dangle.)

- Secondary views and their iterators must represent their sources by-value.
  This has two benefits:
  - Users do not have to worry whether secondary views outlive their sources,
    because the secondary view does not depend on its source. (Only on the
    most "upstream" container source.) This makes user code less error-prone.
  - A nested secondary view that (wrongly) represented its source by reference
    would have dangling references, since the source is a temporary object
    returned from an iterator. The temporary object would be deleted after the
    next function return, whereas the reference held in the member variable
    would live longer.
