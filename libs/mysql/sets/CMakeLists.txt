# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET(TARGET_HEADERS
  aliases.h
  base_binary_operation_views.h
  base_complement_view.h
  base_const_views.h
  base_volume.h
  basic_set_container_wrapper.h
  binary_operation.h
  boundary_set_binary_operation_iterator.h
  boundary_set_binary_operation_view_base.h
  boundary_set_category.h
  boundary_set_complement_view.h
  boundary_set_const_views.h
  boundary_set_interface.h
  boundary_set_intersection_view.h
  boundary_set_meta.h
  boundary_set_predicates.h
  boundary_set_subtraction_view.h
  boundary_set_union_view.h
  boundary_set_volume.h
  boundary_storage.h
  common_predicates.h
  complement_view.h
  int_set_traits.h
  interval.h
  interval_const_views.h
  interval_predicates.h
  interval_set_binary_operation_view_base.h
  interval_set_category.h
  interval_set_complement_view.h
  interval_set_interface.h
  interval_container.h
  interval_operation_view.h
  interval_set_intersection_view.h
  interval_set_meta.h
  interval_set_predicates.h
  interval_set_subtraction_view.h
  interval_set_union_view.h
  interval_set_volume.h
  interval_volume.h
  map_nested_storage.h
  meta.h
  nested_container.h
  nested_set.h
  nested_set_binary_operation_iterator_base.h
  nested_set_binary_operation_view_interface.h
  nested_set_category.h
  nested_set_const_views.h
  nested_set_interface.h
  nested_set_intersection_iterator.h
  nested_set_intersection_view.h
  nested_set_meta.h
  nested_set_predicates.h
  nested_set_subtraction_iterator.h
  nested_set_subtraction_view.h
  nested_set_traits.h
  nested_set_union_iterator.h
  nested_set_union_view.h
  nested_set_volume.h
  nonthrowing_boundary_container_adaptor.h
  optional_view_source_set.h
  ordered_set_traits_interface.h
  set_categories.h
  set_categories_and_traits.h
  set_container_helpers.h
  set_traits.h
  sets.h
  strconv/boundary_set_text_format.h
  strconv/decode.h
  strconv/encode.h
  strconv/nested_set_text_format.h
  throwing/boundary_container.h
  throwing/map_boundary_storage.h
  throwing/vector_boundary_storage.h
  upper_lower_bound_interface.h
  upper_lower_bound_interface_for_map.h
)

SET(TARGET_SRCS
)
