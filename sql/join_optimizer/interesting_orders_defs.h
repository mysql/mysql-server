/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_DEFS_H
#define SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_DEFS_H

// Definitions related to interesting_orders.h that can be pulled in
// without additional dependencies on MySQL headers.

#include <bitset>

enum enum_order : int;
class Item;
class THD;

// All Item * are normalized to opaque handles, where handle(x) == handle(y)
// iff x->eq(y, /*binary_cmp=*/true). This makes them faster to compare,
// and as an added bonus, they also take up slightly less memory.
using ItemHandle = int;

// Like ORDER, but smaller and easier to handle for our purposes (in particular,
// no double-pointer for item). Designed for planning, not execution, so you
// will need to make a Filesort element out of it eventually.
struct OrderElement {
  ItemHandle item;

  // ORDER_NOT_RELEVANT for a group specification. Groupings are by convention
  // sorted by item.
  enum_order direction;
};

inline bool operator==(const OrderElement &a, const OrderElement &b) {
  return a.item == b.item && a.direction == b.direction;
}

// We represent sets of functional dependencies by bitsets, and for simplicity,
// we only allow a fixed number of them; if you have more of them, they will not
// get their own bitmask and will be silently ignored (imposisble to follow in
// the state machine). Note that this does not include “always-on” FDs and FDs
// that will be pruned away, as these are either removed or silently removed to
// the highest indexes.
static constexpr int kMaxSupportedFDs = 64;
using FunctionalDependencySet = std::bitset<kMaxSupportedFDs>;

static constexpr int kMaxSupportedOrderings = 64;
using OrderingSet = std::bitset<kMaxSupportedOrderings>;

#endif  // SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_H
