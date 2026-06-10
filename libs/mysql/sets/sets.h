// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_SETS_SETS_H
#define MYSQL_SETS_SETS_H

/// @file
/// Experimental API header
///
/// This is a high-level header, intended for users to include in order to
/// import all functionality in the sets library.

// Files are listed in order from more primitive to more complex. A file is not
// supposed to include a file appearing after it in the list.

// ==== Set categories ====

// Set categories are type tags used by the member type Set_category_t of any
// set class. Set categories are used to dispatch to a particular implementation
// of an algorithm. For example, each category has its own implementation of
// `is_subset`, and the type tags of the operands are used to determine which
// implementation to use. Currently we have the categories Boundary sets,
// Interval sets, and Nested sets. Users may define their own set categories.

// Type tags to identify categories of sets.
#include "mysql/sets/set_categories.h"

// Set category definitions for boundary sets.
#include "mysql/sets/boundary_set_category.h"

// Set category definitions for interval sets.
#include "mysql/sets/interval_set_category.h"

// Set category definitions for interval sets.
#include "mysql/sets/nested_set_category.h"

// ==== Set traits ====

// Set traits are classes that bundle compile-time parameters of a set. Each set
// has a member type Set_traits_t bound to a set traits class. Algorithms use
// set traits to determine how to perform basic operations. For example, for an
// interval set, the set traits determine the type of elements, the predicate to
// compare two elements, and the minimum and maximum value. Users may define
// their own set traits.

// Traits to characterize the data stored in a set.
#include "mysql/sets/set_traits.h"

// Concepts that depend on both category and traits.
#include "mysql/sets/set_categories_and_traits.h"

// Other generic metaprogramming functionality.
#include "mysql/sets/meta.h"

// CRTP base for ordered Set traits.
#include "mysql/sets/ordered_set_traits_interface.h"

// Int_set_traits, the Set traits class template for sets holding integers.
#include "mysql/sets/int_set_traits.h"

// Nested_set_traits, the class template for Set traits for Nested sets.
#include "mysql/sets/nested_set_traits.h"

// ==== Metaprogramming (concepts) for each specific set type ====

// Concepts to test set traits for boundary sets.
#include "mysql/sets/boundary_set_meta.h"

// Concepts to test set traits for interval sets.
#include "mysql/sets/interval_set_meta.h"

// Concepts to test set traits for nested sets.
#include "mysql/sets/nested_set_meta.h"

// ==== Upper and lower bounds ====

// CRTP base for classes with upper_bound/lower_bound member functions.
#include "mysql/sets/upper_lower_bound_interface.h"

// ==== Interfaces for Boundary sets and Interval sets ====

// CRTP base for boundary sets
#include "mysql/sets/boundary_set_interface.h"

// Classes representing single intervals.
#include "mysql/sets/interval.h"

// CRTP base for interval sets.
#include "mysql/sets/interval_set_interface.h"

// ==== Free function predicates ====

// Free function Boolean set predicates common to most set categories, e.g.,
// is_equal.
#include "mysql/sets/common_predicates.h"

// Free function Boolean set predicates such as is_subset, for intervals.
#include "mysql/sets/interval_predicates.h"

// Free function Boolean set predicates such as is_subset, for boundary sets.
#include "mysql/sets/boundary_set_predicates.h"

// Free function Boolean set predicates such as is_subset, for interval sets.
#include "mysql/sets/interval_set_predicates.h"

// Free function Boolean set predicates such as is_subset.
#include "mysql/sets/nested_set_predicates.h"

// ==== Constant views, e.g., Empty_set_view ====

// Helpers used when defining views below.
#include "mysql/sets/optional_view_source_set.h"

// Primary templates and factory functions.
#include "mysql/sets/base_const_views.h"

// Views over empty and full Boundary sets, and views over arbitrary
// compile-time-defined boundaries.
#include "mysql/sets/boundary_set_const_views.h"

// View over empty and full Interval set.
#include "mysql/sets/interval_const_views.h"

// View over empty nested sets.
#include "mysql/sets/nested_set_const_views.h"

// ==== Views over binary operations, e.g. Union_view ====

// enum Binary_operation { op_union, op_intersection, op_subtraction }
#include "libs/mysql/sets/binary_operation.h"

// Primary templates and factory functions.
#include "mysql/sets/base_binary_operation_views.h"

// ---- Boundary set binary operation binary operations ----

// Common class template for iterators over binary operations on boundary sets.
#include "mysql/sets/boundary_set_binary_operation_iterator.h"

// Base class for views over binary operations on boundary sets.
#include "mysql/sets/boundary_set_binary_operation_view_base.h"

// View over union of boundary sets.
#include "mysql/sets/boundary_set_union_view.h"

// View over intersection of boundary sets.
#include "mysql/sets/boundary_set_intersection_view.h"

// View over subtraction of boundary sets.
#include "mysql/sets/boundary_set_subtraction_view.h"

// ---- Interval set binary operations ----

// Base class for views of binary operations on interval sets.
#include "mysql/sets/interval_set_binary_operation_view_base.h"

// View over union of interval sets.
#include "mysql/sets/interval_set_union_view.h"

// View over intersection of interval sets.
#include "mysql/sets/interval_set_intersection_view.h"

// View over subtraction of interval sets.
#include "mysql/sets/interval_set_subtraction_view.h"

// ---- Nested set binary operations ----

// Common base class for iterators over binary operations on nested sets.
#include "mysql/sets/nested_set_binary_operation_iterator_base.h"

// Iterators over union of nested sets.
#include "mysql/sets/nested_set_union_iterator.h"

// Iterators over intersection of nested sets.
#include "mysql/sets/nested_set_intersection_iterator.h"

// Iterators over subtraction of nested sets.
#include "mysql/sets/nested_set_subtraction_iterator.h"

// Base class for views over binary operations on nested sets.
#include "mysql/sets/nested_set_binary_operation_view_interface.h"

// View over union of nested sets.
#include "mysql/sets/nested_set_union_view.h"

// View over intersection of nested sets.
#include "mysql/sets/nested_set_intersection_view.h"

// View over subtraction of nested sets.
#include "mysql/sets/nested_set_subtraction_view.h"

// ==== Views over complements: Complement_view ====

// Primary template and factory function.
#include "mysql/sets/base_complement_view.h"

// Complement view over boundary sets.
#include "mysql/sets/boundary_set_complement_view.h"

// Complement view over interval sets.
#include "mysql/sets/interval_set_complement_view.h"

// ==== Volume ====

// Generic implementation of volume_difference.
#include "mysql/sets/base_volume.h"

// Compute the volume of a single interval.
#include "mysql/sets/interval_volume.h"

// Compute the volume of an interval set.
#include "mysql/sets/interval_set_volume.h"

// Compute the volume of a boundary set.
#include "mysql/sets/boundary_set_volume.h"

// Compute the volume of a nested set.
#include "mysql/sets/nested_set_volume.h"

// ==== Boundary containers and storage ====

// Base class for containers which are wrappers around other containers.
#include "libs/mysql/sets/basic_set_container_wrapper.h"

// Helper function used when defining inplace set operations.
#include "libs/mysql/sets/set_container_helpers.h"

// Associative boundary storage, based on std::map. May throw bad_alloc.
#include "mysql/sets/throwing/map_boundary_storage.h"

// Contiguous boundary storage, based on std::vector. May throw bad_alloc.
#include "mysql/sets/throwing/vector_boundary_storage.h"

// Boundary container, based on one of the storage classes. May throw bad_alloc.
#include "mysql/sets/throwing/boundary_container.h"

// Adaptor for boundary containers, that "converts" bad_alloc to a return value.
#include "mysql/sets/nonthrowing_boundary_container_adaptor.h"

// ==== Interval containers ====

// Adaptor that provides an interval container from a boundary container.
#include "mysql/sets/interval_container.h"

// ==== Nested containers and storage ====

// Associative nested set storage, based on std::map.
#include "mysql/sets/map_nested_storage.h"

// Nested set container.
#include "mysql/sets/nested_container.h"

// ==== Type aliases for containers ====

// Using-definitions for containers based on map/vector storage.
#include "mysql/sets/aliases.h"

// ==== String conversion ====

// String format definition for boundary sets and interval sets.
#include "mysql/sets/strconv/boundary_set_text_format.h"

// String format definition for nested sets.
#include "mysql/sets/strconv/nested_set_text_format.h"

// Implementation enabling mysql::strconv::encode for interval/boundary
// sets.
#include "mysql/sets/strconv/encode.h"

// Implementation enabling mysql::strconv::decode for interval/boundary
// sets.
#include "mysql/sets/strconv/decode.h"

#endif  // ifndef MYSQL_SETS_SETS_H
