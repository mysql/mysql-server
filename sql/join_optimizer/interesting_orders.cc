/* Copyright (c) 2021, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/join_optimizer/interesting_orders.h"
#include <algorithm>
#include <type_traits>
#include "sql/item.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/sql_class.h"

using std::distance;
using std::equal;
using std::fill;
using std::lower_bound;
using std::make_pair;
using std::max;
using std::move;
using std::pair;
using std::sort;
using std::string;
using std::swap;
using std::unique;
using std::upper_bound;

namespace {

template <class T>
Bounds_checked_array<T> DuplicateArray(THD *thd,
                                       Bounds_checked_array<T> array) {
  static_assert(std::is_pod<T>::value, "");
  T *items = thd->mem_root->ArrayAlloc<T>(array.size());
  if (!array.empty()) {
    memcpy(items, &array[0], sizeof(*items) * array.size());
  }
  return {items, array.size()};
}

bool IsGrouping(Ordering ordering) {
  return !ordering.empty() && ordering[0].direction == ORDER_NOT_RELEVANT;
}

bool OrderingsAreEqual(Ordering a, Ordering b) {
  return equal(a.begin(), a.end(), b.begin(), b.end());
}

}  // namespace

LogicalOrderings::LogicalOrderings(THD *thd)
    : m_items(thd->mem_root),
      m_orderings(thd->mem_root),
      m_fds(thd->mem_root),
      m_states(thd->mem_root),
      m_edges(thd->mem_root),
      m_dfsm_states(thd->mem_root),
      m_dfsm_edges(thd->mem_root) {
  GetHandle(nullptr);  // Always has the zero handle.

  // Add the empty ordering/grouping.
  m_orderings.push_back(
      OrderingWithInfo{Ordering{nullptr, 0}, OrderingWithInfo::UNINTERESTING});

  FunctionalDependency decay_fd;
  decay_fd.type = FunctionalDependency::DECAY;
  decay_fd.tail = 0;
  decay_fd.always_active = true;
  m_fds.push_back(decay_fd);
}

int LogicalOrderings::AddOrderingInternal(THD *thd, Ordering order,
                                          OrderingWithInfo::Type type) {
  assert(!m_built);

  if (type != OrderingWithInfo::UNINTERESTING) {
    for (OrderElement element : order) {
      if (element.direction == ORDER_ASC) {
        m_items[element.item].used_asc = true;
      }
      if (element.direction == ORDER_DESC) {
        m_items[element.item].used_desc = true;
      }
    }
  }

  // Deduplicate against all the existing ones.
  for (size_t i = 0; i < m_orderings.size(); ++i) {
    if (OrderingsAreEqual(m_orderings[i].ordering, order)) {
      // Potentially promote the existing one.
      m_orderings[i].type = std::max(m_orderings[i].type, type);
      return i;
    }
  }
  m_orderings.push_back(OrderingWithInfo{DuplicateArray(thd, order), type});
  m_longest_ordering = std::max<int>(m_longest_ordering, order.size());

  return m_orderings.size() - 1;
}

int LogicalOrderings::AddFunctionalDependency(THD *thd,
                                              FunctionalDependency fd) {
  assert(!m_built);

  // Deduplicate against all the existing ones.
  for (size_t i = 0; i < m_fds.size(); ++i) {
    if (m_fds[i].type != fd.type) {
      continue;
    }
    if (fd.type == FunctionalDependency::EQUIVALENCE) {
      // Equivalences are symmetric.
      if (m_fds[i].head[0] == fd.head[0] && m_fds[i].tail == fd.tail) {
        return i;
      }
      if (m_fds[i].tail == fd.head[0] && m_fds[i].head[0] == fd.tail) {
        return i;
      }
    } else {
      if (m_fds[i].tail == fd.tail &&
          equal(m_fds[i].head.begin(), m_fds[i].head.end(), fd.head.begin(),
                fd.head.end())) {
        return i;
      }
    }
  }
  fd.head = DuplicateArray(thd, fd.head);
  m_fds.push_back(fd);
  return m_fds.size() - 1;
}

void LogicalOrderings::Build(THD *thd, string *trace) {
  BuildEquivalenceClasses();
  CreateHomogenizedOrderings(thd);
  PruneFDs(thd);
  if (trace != nullptr) {
    PrintFunctionalDependencies(trace);
  }
  FindElementsThatCanBeAddedByFDs();
  PruneUninterestingOrders(thd);
  if (trace != nullptr) {
    PrintInterestingOrders(trace);
  }
  BuildNFSM(thd);
  if (trace != nullptr) {
    *trace += "NFSM for interesting orders, before pruning:\n";
    PrintNFSMDottyGraph(trace);
  }
  PruneNFSM(thd);
  if (trace != nullptr) {
    *trace += "\nNFSM for interesting orders, after pruning:\n";
    PrintNFSMDottyGraph(trace);
  }
  ConvertNFSMToDFSM(thd);
  if (trace != nullptr) {
    *trace += "\nDFSM for interesting orders:\n";
    PrintDFSMDottyGraph(trace);
  }
  FindInitialStatesForOrdering();
  m_built = true;
}

LogicalOrderings::StateIndex LogicalOrderings::ApplyFDs(
    LogicalOrderings::StateIndex state_idx, FunctionalDependencySet fds) const {
  for (;;) {  // Termination condition within loop.
    FunctionalDependencySet relevant_fds =
        m_dfsm_states[state_idx].can_use_fd & fds;
    if (relevant_fds.none()) {
      return state_idx;
    }

    // Pick an arbitrary one and follow it. Note that this part assumes
    // kMaxSupportedFDs <= 64.
    static_assert(kMaxSupportedFDs <= sizeof(unsigned long long) * CHAR_BIT,
                  "");
    int fd_idx = FindLowestBitSet(relevant_fds.to_ullong()) + 1;
    state_idx = m_dfsm_states[state_idx].next_state[fd_idx];

    // Now continue for as long as we have anything to follow;
    // we'll converge on the right answer eventually. Typically,
    // there will be one or two edges to follow, but in extreme cases,
    // there could be O(k²) in the number of FDs.
  }
}

/**
  Try to get rid of uninteresting orders, possibly by discarding irrelevant
  suffixes and merging them with others. In a typical query, this removes a
  large amount of index-created orderings that will never get to something
  interesting, reducing the end FSM size (and thus, reducing the number of
  different access paths we have to keep around).

  This step is the only one that can move orderings around, and thus also
  populates m_optimized_ordering_mapping.
 */
void LogicalOrderings::PruneUninterestingOrders(THD *thd) {
  m_optimized_ordering_mapping =
      Bounds_checked_array<int>::Alloc(thd->mem_root, m_orderings.size());
  int new_length = 0;
  for (size_t ordering_idx = 0; ordering_idx < m_orderings.size();
       ++ordering_idx) {
    if (m_orderings[ordering_idx].type == OrderingWithInfo::UNINTERESTING) {
      // Shorten this ordering one by one element, until it can (heuristically)
      // become an interesting ordering with the FDs we have. Note that it might
      // become the empty ordering, and if so, it will be deleted entirely
      // in the step below.
      Ordering &ordering = m_orderings[ordering_idx].ordering;
      while (!ordering.empty() && !CouldBecomeInterestingOrdering(ordering)) {
        ordering.resize(ordering.size() - 1);
      }
    }

    // Since some orderings may have changed, we need to re-deduplicate.
    m_optimized_ordering_mapping[ordering_idx] = new_length;
    for (int i = 0; i < new_length; ++i) {
      if (OrderingsAreEqual(m_orderings[i].ordering,
                            m_orderings[ordering_idx].ordering)) {
        m_optimized_ordering_mapping[ordering_idx] = i;
        m_orderings[i].type =
            std::max(m_orderings[i].type, m_orderings[ordering_idx].type);
        break;
      }
    }
    if (m_optimized_ordering_mapping[ordering_idx] == new_length) {
      // Not a duplicate of anything earlier, so keep it.
      m_orderings[new_length++] = m_orderings[ordering_idx];
    }
  }
  m_orderings.resize(new_length);
}

void LogicalOrderings::PruneFDs(THD *thd) {
  // The definition of prunable FDs in the papers seems to be very abstract
  // and not practically realizable, so we use a simple heuristic instead:
  // A FD is useful iff it produces an item that is part of some ordering.
  // Discard all non-useful FDs. (Items not part of some ordering will cause
  // the new proposed ordering to immediately be pruned away, so this is
  // safe. See also the comment in the .h file about transitive dependencies.)
  //
  // Note that this will sometimes leave useless FDs; if we have e.g. a → b
  // and b is useful, we will mark the FD as useful even if nothing can
  // produce a. However, such FDs don't induce more NFSM states (which is
  // the main point of the pruning), it just slows the NFSM down slightly,
  // and by far the dominant FDs to prune in our cases are the ones
  // induced by keys, e.g. S → k where S is always the same and k
  // is non-useful. These are caught by this heuristic.

  m_optimized_fd_mapping =
      Bounds_checked_array<int>::Alloc(thd->mem_root, m_fds.size());
  size_t old_length = m_fds.size();

  // We always need to keep the decay FD, so start at 1.
  m_optimized_fd_mapping[0] = 0;
  int new_length = 1;

  for (size_t fd_idx = 1; fd_idx < old_length; ++fd_idx) {
    const FunctionalDependency &fd = m_fds[fd_idx];

    // See if we this FDs is useful, ie., can produce an item used in an
    // ordering.
    bool used_fd = false;
    ItemHandle tail = m_items[fd.tail].canonical_item;
    if (m_items[tail].used_asc || m_items[tail].used_desc) {
      used_fd = true;
    } else if (fd.type == FunctionalDependency::EQUIVALENCE) {
      ItemHandle head = m_items[fd.head[0]].canonical_item;
      if (m_items[head].used_asc || m_items[head].used_desc) {
        used_fd = true;
      }
    }

    if (!used_fd) {
      m_optimized_fd_mapping[fd_idx] = -1;
      continue;
    }

    if (m_fds[fd_idx].always_active) {
      // Defer these for now, by moving them to the end. We will need to keep
      // them in the array so that we can apply them under FSM construction,
      // but they should not get a FD bitmap, and thus also not priority for
      // the lowest index. We could have used a separate array, but the m_fds
      // array probably already has the memory.
      m_optimized_fd_mapping[fd_idx] = -1;
      m_fds.push_back(m_fds[fd_idx]);
    } else {
      m_optimized_fd_mapping[fd_idx] = new_length;
      m_fds[new_length++] = m_fds[fd_idx];
    }
  }

  // Now include the always-on FDs we deferred earlier.
  for (size_t fd_idx = old_length; fd_idx < m_fds.size(); ++fd_idx) {
    m_fds[new_length++] = m_fds[fd_idx];
  }

  m_fds.resize(new_length);
}

void LogicalOrderings::BuildEquivalenceClasses() {
  for (size_t i = 0; i < m_items.size(); ++i) {
    m_items[i].canonical_item = i;
  }

  // In the worst case, for n items, all equal, m FDs ordered optimally bad,
  // this algorithm is O(nm) (all items shifted one step down each loop).
  // In practice, it should be much better.
  bool done_anything;
  do {
    done_anything = false;
    for (const FunctionalDependency &fd : m_fds) {
      if (fd.type != FunctionalDependency::EQUIVALENCE) {
        continue;
      }
      ItemHandle left_item = fd.head[0];
      ItemHandle right_item = fd.tail;

      if (m_items[left_item].canonical_item ==
          m_items[right_item].canonical_item) {
        // Already fully applied.
        continue;
      }

      // Merge the classes so that the lowest index always is the canonical one
      // of its equivalence class.
      ItemHandle canonical_item, duplicate_item;
      if (m_items[right_item].canonical_item <
          m_items[left_item].canonical_item) {
        canonical_item = m_items[right_item].canonical_item;
        duplicate_item = left_item;
      } else {
        canonical_item = m_items[left_item].canonical_item;
        duplicate_item = right_item;
      }
      m_items[duplicate_item].canonical_item = canonical_item;
      m_items[canonical_item].used_asc |= m_items[duplicate_item].used_asc;
      m_items[canonical_item].used_desc |= m_items[duplicate_item].used_desc;
      done_anything = true;
    }
  } while (done_anything);
}

void LogicalOrderings::FindElementsThatCanBeAddedByFDs() {
  for (const FunctionalDependency &fd : m_fds) {
    m_items[m_items[fd.tail].canonical_item].can_be_added_by_fd = true;
    if (fd.type == FunctionalDependency::EQUIVALENCE) {
      m_items[m_items[fd.head[0]].canonical_item].can_be_added_by_fd = true;
    }
  }
}

/**
  Does the element already exist in given ordering? Unlike
  ImpliedByEarlierElements, only counts literal item duplicates, not items that
  are redundant due to functional dependencies.
 */
static bool Contains(Ordering prefix, ItemHandle item) {
  for (OrderElement elem : prefix) {
    if (elem.item == item) {
      // ASC/DESC doesn't matter, the second item is redundant even on a
      // mismatch.
      return true;
    }
  }
  return false;
}

/**
  Checks whether the given item is redundant given previous elements in
  the ordering; ie., whether adding it will never change the ordering.
  This could either be because it's a duplicate, or because it is implied
  by functional dependencies. When this is applied to all elements in turn,
  it is called “reducing” the ordering. [Neu04] claims that this operation
  is not confluent, which is erroneous (their example is faulty, ignoring
  that Simmen reduces from the back). [Neu04b] has modified the claim to
  be that it is not confluent for _groupings_, which is correct.
  We make no attempt at optimality.

  We consider all functional dependencies here, including those that may
  not always be active; e.g. a FD a=b may come from a join, and thus does
  not hold before the join is actually done, but we assume it holds anyway.
  This is OK because we are only called during order homogenization,
  which is concerned with making orderings that will turn into the desired
  interesting ordering (e.g. for ORDER BY) only after all joins have been
  done. It would not be OK if we were to use it for merge joins somehow.
 */
bool LogicalOrderings::ImpliedByEarlierElements(ItemHandle item,
                                                Ordering prefix) const {
  // First, search for straight-up duplicates (ignoring ASC/DESC).
  if (Contains(prefix, item)) {
    return true;
  }

  // Check if this item is implied by any of the functional dependencies.
  for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
    const FunctionalDependency &fd = m_fds[fd_idx];
    if (fd.type == FunctionalDependency::FD) {
      if (fd.tail != item) {
        continue;
      }

      // Check if we have all the required head items.
      bool all_found = true;
      for (ItemHandle other_item : fd.head) {
        if (!Contains(prefix, other_item)) {
          all_found = false;
          break;
        }
      }
      if (all_found) {
        return true;
      }
    } else {
      // a = b implies that a → b and b → a, so we check for both of those.
      assert(fd.type == FunctionalDependency::EQUIVALENCE);
      assert(fd.head.size() == 1);
      if (fd.tail == item && Contains(prefix, fd.head[0])) {
        return true;
      }
      if (fd.head[0] == item && Contains(prefix, fd.tail)) {
        return true;
      }
    }
  }
  return false;
}

/**
  For each interesting ordering, see if we can homogenize it onto each table.
  A homogenized ordering is one that refers to fewer tables than the original
  one -- in our case, a single table. (If we wanted to, we could homogenize down
  to sets of tables instead of single tables only. However, that would open up
  for O(2^n) orderings, so we restrict to single-table.)

  The idea is to enable sort-ahead; find an ordering we can sort a single table
  in that, after later applying functional dependencies, eventually gives the
  desired ordering. This is just a heuristic (in particular, we only consider
  equivalences, not other functional dependencies), but in most cases will give
  us an ordering if any exist.

  Neumann et al do not talk much about this, so this comes from the Simmen
  paper, where it is called “Homogenize Order”.
 */
void LogicalOrderings::CreateHomogenizedOrderings(THD *thd) {
  // Collect all tables we have seen referred to in items. (Actually, we could
  // limit ourselves to the ones we've seen in functional dependencies, but this
  // is simpler.)
  table_map seen_tables = 0;
  for (const ItemInfo &item : m_items) {
    if (item.item != nullptr) {
      seen_tables |= item.item->used_tables();
    }
  }
  seen_tables &= ~PSEUDO_TABLE_BITS;

  // Build a reverse table of canonical items to items,
  // and sort it, so that we can fairly efficiently make lookups into it.
  auto reverse_canonical =
      Bounds_checked_array<pair<ItemHandle, ItemHandle>>::Alloc(thd->mem_root,
                                                                m_items.size());
  for (size_t item_idx = 0; item_idx < m_items.size(); ++item_idx) {
    reverse_canonical[item_idx].first = m_items[item_idx].canonical_item;
    reverse_canonical[item_idx].second = item_idx;
  }
  sort(reverse_canonical.begin(), reverse_canonical.end());

  OrderElement *tmpbuf =
      thd->mem_root->ArrayAlloc<OrderElement>(m_longest_ordering);
  OrderElement *tmpbuf2 =
      thd->mem_root->ArrayAlloc<OrderElement>(m_longest_ordering);

  // Now, for each table, try to see if we can rewrite an ordering
  // to something only referring to that table, by swapping out non-conforming
  // items for others.
  int num_original_orderings = m_orderings.size();
  for (int ordering_idx = 1; ordering_idx < num_original_orderings;
       ++ordering_idx) {
    if (m_orderings[ordering_idx].type != OrderingWithInfo::INTERESTING) {
      continue;
    }
    Ordering reduced_ordering =
        ReduceOrdering(m_orderings[ordering_idx].ordering, tmpbuf);
    if (reduced_ordering.empty()) {
      continue;
    }

    // Now try to homogenize it onto all tables in turn.
    for (int table_idx : BitsSetIn(seen_tables)) {
      AddHomogenizedOrderingIfPossible(thd, reduced_ordering, table_idx,
                                       reverse_canonical, tmpbuf2);
    }
  }
}

/**
  Remove redundant elements using the functional dependencies that we have,
  to give a more canonical form before homogenization. Note that we assume
  here that every functional dependency holds, so this is not applicable
  generally throughout the tree, only at the end (e.g. final ORDER BY).
  This is called “Reduce Order” in the Simmen paper.

  tmpbuf is used as the memory store for the new ordering.
 */
Ordering LogicalOrderings::ReduceOrdering(Ordering ordering,
                                          OrderElement *tmpbuf) const {
  size_t reduced_length = 0;
  for (size_t part_idx = 0; part_idx < ordering.size(); ++part_idx) {
    if (ImpliedByEarlierElements(ordering[part_idx].item,
                                 ordering.prefix(part_idx))) {
      // Delete this element.
    } else {
      tmpbuf[reduced_length++] = ordering[part_idx];
    }
  }
  return {tmpbuf, reduced_length};
}

/// Helper function for CreateHomogenizedOrderings().
void LogicalOrderings::AddHomogenizedOrderingIfPossible(
    THD *thd, Ordering reduced_ordering, int table_idx,
    Bounds_checked_array<pair<ItemHandle, ItemHandle>> reverse_canonical,
    OrderElement *tmpbuf) {
  const table_map available_tables = table_map{1} << table_idx;
  int length = 0;

  for (OrderElement element : reduced_ordering) {
    if (IsSubset(m_items[element.item].item->used_tables(), available_tables)) {
      // Already OK.
      if (!ImpliedByEarlierElements(element.item, Ordering(tmpbuf, length))) {
        tmpbuf[length++] = element;
      }
      continue;
    }

    // Find all equivalent items.
    ItemHandle canonical_item = m_items[element.item].canonical_item;
    auto first = lower_bound(reverse_canonical.begin(), reverse_canonical.end(),
                             canonical_item,
                             [](const pair<ItemHandle, ItemHandle> &a,
                                ItemHandle b) { return a.first < b; });
    auto last =
        upper_bound(first, reverse_canonical.end(), canonical_item,
                    [](ItemHandle a, const pair<ItemHandle, ItemHandle> &b) {
                      return a < b.first;
                    });
    assert(last - first >= 1);

    bool found = false;
    for (auto it = first; it != last; ++it) {
      if (IsSubset(m_items[it->second].item->used_tables(), available_tables)) {
        if (ImpliedByEarlierElements(it->second, Ordering(tmpbuf, length))) {
          // Unneeded in the new order, so delete it.
          // Similar to the reduction process above.
        } else {
          tmpbuf[length].item = it->second;
          tmpbuf[length].direction = element.direction;
          ++length;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      // Not possible to homogenize this ordering.
      return;
    }
  }
  AddOrderingInternal(thd, Ordering(tmpbuf, length),
                      OrderingWithInfo::HOMOGENIZED);
}

ItemHandle LogicalOrderings::GetHandle(Item *item) {
  for (size_t i = 1; i < m_items.size(); ++i) {
    if (item == m_items[i].item ||
        item->eq(m_items[i].item, /*binary_cmp=*/true)) {
      return i;
    }
  }
  m_items.push_back(ItemInfo{item, /*canonical_item=*/0});
  return m_items.size() - 1;
}

/**
  For a given ordering, check whether it ever has the hope of becoming an
  interesting ordering. In its base form, this is a prefix check; if we
  have an ordering (a,b) and an interesting order (a,b,c), it passes.
  However, we add some slightly more lax heuristics in order to make the
  graph a bit wider at build time (and thus require fewer FD applications at
  runtime); namely, if there's a prefix mismatch but the item could be added
  by some FD later (without the ordering becoming too long), we let it slide
  and just skip that item.

  E.g.: If we have an ordering (a,b) and an interesting order (a,x,b),
  we first match a. x does not match b, but we check whether x is ever on the
  right side of any FD (for instance because there might be an FD a → x).
  If it is, we skip it and match b with b. There's an example of this in the
  DoesNotStrictlyPruneOnPrefixes unit test.

  Obviously, this leads to false positives, but that is fine; this is just
  to prune down the amount of states in the NFSM. [Neu04] points out that
  such pruning is pretty much essential for performance, and our experience
  is the same.

  There is one extra quirk; the prefix check needs to take equivalences into
  account, or we would prune away orderings that could become interesting
  after equivalences. We solve this by always mapping to an equivalence class
  when doing the prefix comparison. There's an example of this in the
  TwoEquivalences unit test.
 */
bool LogicalOrderings::CouldBecomeInterestingOrdering(Ordering ordering) const {
  for (OrderingWithInfo other_ordering : m_orderings) {
    const Ordering interesting_ordering = other_ordering.ordering;
    if (other_ordering.type != OrderingWithInfo::INTERESTING ||
        interesting_ordering.size() < ordering.size()) {
      continue;
    }

    bool match = true;
    for (size_t i = 0, j = 0;
         i < ordering.size() || j < interesting_ordering.size();) {
      if (ordering.size() - i > interesting_ordering.size() - j) {
        // We have excess items at the end, so give up.
        match = false;
        break;
      }

      const ItemHandle needed_item =
          m_items[interesting_ordering[j].item].canonical_item;
      if (i < ordering.size() &&
          m_items[ordering[i].item].canonical_item == needed_item &&
          ordering[i].direction == interesting_ordering[j].direction) {
        // We have a matching item, so move both iterators along.
        ++i, ++j;
        continue;
      }

      if (m_items[needed_item].can_be_added_by_fd) {
        // We don't have this item, but it could be generated, so skip it.
        ++j;
        continue;
      }

      // We don't have this item, and it can not be generated by any FD,
      // so give up.
      match = false;
      break;
    }
    if (match) {
      return true;
    }
  }
  return false;
}

int LogicalOrderings::AddArtificialState(THD *thd, Ordering ordering) {
  for (size_t i = 0; i < m_states.size(); ++i) {
    if (OrderingsAreEqual(m_states[i].satisfied_ordering, ordering)) {
      return i;
    }
  }

  NFSMState state;
  state.satisfied_ordering = DuplicateArray(thd, ordering);
  state.satisfied_ordering_idx = -1;  // Irrelevant, but placate the compiler.
  state.outgoing_edges.init(thd->mem_root);
  state.type = NFSMState::ARTIFICIAL;
  m_states.push_back(move(state));
  return m_states.size() - 1;
}

void LogicalOrderings::AddEdge(THD *thd, int state_idx, int required_fd_idx,
                               Ordering ordering) {
  NFSMEdge edge;
  edge.required_fd_idx = required_fd_idx;
  edge.state_idx = AddArtificialState(thd, ordering);

  if (edge.state_idx == state_idx) {
    // Don't add self-edges; they are already implicit.
    return;
  }

  m_edges.push_back(edge);
  m_states[state_idx].outgoing_edges.push_back(m_edges.size() - 1);
}

bool LogicalOrderings::FunctionalDependencyApplies(
    const FunctionalDependency &fd, const Ordering ordering,
    int *start_point) const {
  assert(fd.type != FunctionalDependency::DECAY);
  *start_point = -1;
  for (ItemHandle head_item : fd.head) {
    bool matched = false;
    for (size_t i = 0; i < ordering.size(); ++i) {
      if (ordering[i].item == head_item ||
          (fd.type == FunctionalDependency::EQUIVALENCE &&
           ordering[i].item == fd.tail)) {
        *start_point = max<int>(*start_point, i);
        matched = true;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }
  return true;
}

/**
  Remove duplicate entries from an ordering, in-place.
 */
static void DeduplicateOrdering(Ordering *ordering) {
  size_t length = 0;
  for (size_t i = 0; i < ordering->size(); ++i) {
    if (!Contains(ordering->prefix(length), (*ordering)[i].item)) {
      (*ordering)[length++] = (*ordering)[i];
    }
  }
  ordering->resize(length);
}

void LogicalOrderings::BuildNFSM(THD *thd) {
  // Add a state for each producable ordering.
  for (size_t i = 0; i < m_orderings.size(); ++i) {
    NFSMState state;
    state.satisfied_ordering = m_orderings[i].ordering;
    state.satisfied_ordering_idx = i;
    state.outgoing_edges.init(thd->mem_root);
    state.type = m_orderings[i].type == OrderingWithInfo::INTERESTING
                     ? NFSMState::INTERESTING
                     : NFSMState::ARTIFICIAL;
    m_states.push_back(move(state));
  }

  // Add an edge from the initial state to each producable ordering.
  for (size_t i = 1; i < m_orderings.size(); ++i) {
    NFSMEdge edge;
    edge.required_fd_idx = INT_MIN + i;
    edge.state_idx = i;
    m_edges.push_back(edge);
    m_states[0].outgoing_edges.push_back(m_edges.size() - 1);
  }

  // Add edges from functional dependencies, in a breadth-first search
  // (the array of m_states will expand as we go).
  OrderElement *tmpbuf =
      thd->mem_root->ArrayAlloc<OrderElement>(m_longest_ordering);
  OrderElement *tmpbuf2 =
      thd->mem_root->ArrayAlloc<OrderElement>(m_longest_ordering);
  for (size_t state_idx = 0; state_idx < m_states.size(); ++state_idx) {
    // Apply the special decay FD.
    if (m_states[state_idx].satisfied_ordering.size() > 1) {
      AddEdge(thd, state_idx, /*required_fd_idx=*/0,
              m_states[state_idx].satisfied_ordering.without_back());
    }

    for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
      const FunctionalDependency &fd = m_fds[fd_idx];
      Ordering old_ordering = m_states[state_idx].satisfied_ordering;

      int start_point;
      if (!FunctionalDependencyApplies(fd, old_ordering, &start_point)) {
        continue;
      }

      ItemHandle item_to_add = fd.tail;

      // On a = b, try to replace a with b or b with a.
      if (fd.type == FunctionalDependency::EQUIVALENCE) {
        Ordering new_ordering{tmpbuf, old_ordering.size()};
        memcpy(tmpbuf, &old_ordering[0],
               sizeof(old_ordering[0]) * old_ordering.size());
        ItemHandle other_item = fd.head[0];
        if (new_ordering[start_point].item == item_to_add) {
          // b already existed, so it's a we must add.
          swap(item_to_add, other_item);
        }
        new_ordering[start_point].item = item_to_add;  // Keep the direction.
        DeduplicateOrdering(&new_ordering);
        if (CouldBecomeInterestingOrdering(new_ordering)) {
          AddEdge(thd, state_idx, fd_idx, new_ordering);
        }

        // Now we can add back the item we just replaced,
        // at any point after this. E.g., if we had an order abc
        // and applied b=d to get adc, we can add back b to get
        // adbc or adcb. Also, we'll fall through afterwards
        // to _not_ replacing but just adding d, e.g. abdc and abcd.
        // So fall through.
        old_ordering = new_ordering;
        item_to_add = other_item;
      }

      // On S -> b, try to add b everywhere after the last element of S.
      bool add_asc = m_items[m_items[item_to_add].canonical_item].used_asc;
      bool add_desc = m_items[m_items[item_to_add].canonical_item].used_desc;
      assert(add_asc || add_desc);  // Enforced by PruneFDs().

      if (add_asc) {
        TryAddingOrderWithElementInserted(thd, state_idx, fd_idx, old_ordering,
                                          start_point + 1, item_to_add,
                                          ORDER_ASC, tmpbuf2);
      }
      if (add_desc) {
        TryAddingOrderWithElementInserted(thd, state_idx, fd_idx, old_ordering,
                                          start_point + 1, item_to_add,
                                          ORDER_DESC, tmpbuf2);
      }
    }
  }
}

void LogicalOrderings::TryAddingOrderWithElementInserted(
    THD *thd, int state_idx, int fd_idx, Ordering old_ordering,
    size_t start_point, ItemHandle item_to_add, enum_order direction,
    OrderElement *tmpbuf) {
  if (static_cast<int>(old_ordering.size()) >= m_longest_ordering) {
    return;
  }

  for (size_t add_pos = start_point; add_pos <= old_ordering.size();
       ++add_pos) {
    if (add_pos > 0) {
      memcpy(tmpbuf, &old_ordering[0], sizeof(*tmpbuf) * (add_pos));
    }
    tmpbuf[add_pos].item = item_to_add;
    tmpbuf[add_pos].direction = direction;
    if (old_ordering.size() > add_pos) {
      memcpy(tmpbuf + add_pos + 1, &old_ordering[add_pos],
             sizeof(*tmpbuf) * (old_ordering.size() - add_pos));
    }
    Ordering new_ordering{tmpbuf, old_ordering.size() + 1};
    DeduplicateOrdering(&new_ordering);

    if (CouldBecomeInterestingOrdering(new_ordering)) {
      AddEdge(thd, state_idx, fd_idx, new_ordering);
    }
  }
}

/**
  Try to prune away irrelevant nodes from the NFSM; it is worth spending some
  time on this, since the number of NFSM states can explode the size of the
  DFSM. Like with PruneFDs(), we don't do any of the pruning described in
  [Neu04]; it is unclear exactly what is meant, but it would seem the state
  removal/merging there is either underdefined or simply does not do anything
  except remove trivially bad nodes (those that cannot reach anything).

  This also sets the can_reach_interesting_order bitmap on each NFSM node.
 */
void LogicalOrderings::PruneNFSM(THD *thd) {
  // Find the transitive closure of the NFSM; ie., whether state A can reach
  // state B, either directly or through some other state (possibly many).
  // We use the standard Floyd-Warshall algorithm, which is O(n³); if n gets
  // to be very large, we can flip the direction of all edges and use
  // Dijkstra from each interesting order instead (since we're only interested
  // in reachability to interesting orders, and our graph is quite sparse),
  // but Floyd-Warshall is simple and has a low constant factor.
  const int N = m_states.size();
  bool *reachable = thd->mem_root->ArrayAlloc<bool>(N * N);

  // We have multiple pruning techniques, all heuristic in nature.
  // If one removes something, it may help to run the others again,
  // so keep running until we've stabilized.
  bool pruned_anything;
  do {
    pruned_anything = false;
    memset(reachable, 0, sizeof(*reachable) * N * N);

    for (int i = 0; i < N; ++i) {
      if (m_states[i].type == NFSMState::DELETED) {
        continue;
      }

      // There's always an implicit self-edge.
      reachable[i * N + i] = true;

      for (size_t edge_idx : m_states[i].outgoing_edges) {
        reachable[i * N + m_edges[edge_idx].state_idx] = true;
      }
    }

    for (int k = 0; k < N; ++k) {
      for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
          // If there are edges i -> k -> j, add an edge i -> j.
          reachable[i * N + j] |= reachable[i * N + k] & reachable[k * N + j];
        }
      }
    }

    // Now prune away artificial m_states that cannot reach any
    // interesting orders, and m_states that are not reachable from
    // the initial node (the latter can only happen as the result
    // of other prunings).
    for (int i = 1; i < N; ++i) {
      if (m_states[i].type != NFSMState::ARTIFICIAL) {
        continue;
      }

      if (!reachable[0 * N + i]) {
        m_states[i].type = NFSMState::DELETED;
        pruned_anything = true;
        continue;
      }

      bool can_reach_interesting = false;
      for (int j = 1; j < static_cast<int>(m_orderings.size()); ++j) {
        if (reachable[i * N + j] &&
            m_states[j].type == NFSMState::INTERESTING) {
          can_reach_interesting = true;
          break;
        }
      }
      if (!can_reach_interesting) {
        m_states[i].type = NFSMState::DELETED;
        pruned_anything = true;
      }
    }

    // For each producing order, remove edges to m_states that cannot
    // reach any _other_ interesting orders. This often helps dislodging
    // such m_states from the graph as a whole, removing them in some later
    // step. This supersedes the same-destination merging step from [Neu04].
    for (size_t i = 1; i < m_orderings.size(); ++i) {
      NFSMState &state = m_states[i];
      for (size_t j = 0; j < state.outgoing_edges.size(); ++j) {
        const int next_state_idx = m_edges[state.outgoing_edges[j]].state_idx;
        bool can_reach_other_interesting = false;
        for (size_t k = 1; k < m_orderings.size(); ++k) {
          if (k != i && m_states[k].type == NFSMState::INTERESTING &&
              reachable[next_state_idx * N + k]) {
            can_reach_other_interesting = true;
            break;
          }
        }
        if (!can_reach_other_interesting) {
          // Remove this edge.
          state.outgoing_edges[j] =
              state.outgoing_edges[state.outgoing_edges.size() - 1];
          state.outgoing_edges.resize(state.outgoing_edges.size() - 1);
          pruned_anything = true;
        }
      }
    }

    // Remove any edges to deleted m_states.
    for (int i = 0; i < N; ++i) {
      NFSMState &state = m_states[i];
      if (state.type == NFSMState::DELETED) {
        continue;
      }
      int num_kept = 0;
      for (size_t j = 0; j < state.outgoing_edges.size(); ++j) {
        const NFSMEdge &edge = m_edges[state.outgoing_edges[j]];
        if (edge.state(this)->type != NFSMState::DELETED) {
          state.outgoing_edges[num_kept++] = state.outgoing_edges[j];
        }
      }
      state.outgoing_edges.resize(num_kept);
    }
  } while (pruned_anything);

  // Set the bitmask of what each node can reach.
  for (size_t order_idx = 0; order_idx < m_orderings.size(); ++order_idx) {
    if (m_orderings[order_idx].type != OrderingWithInfo::INTERESTING ||
        order_idx >= kMaxSupportedOrderings) {
      continue;
    }
    for (int i = 0; i < N; ++i) {
      if (m_states[i].type == NFSMState::DELETED) {
        continue;
      }
      if (reachable[i * N + order_idx]) {
        m_states[i].can_reach_interesting_order.set(order_idx);
      }
    }
  }
}

bool LogicalOrderings::AlwaysActiveFD(int fd_idx) {
  // Note: Includes ϵ-edges.
  return fd_idx >= 0 && m_fds[fd_idx].always_active;
}

void LogicalOrderings::FinalizeDFSMState(THD *thd, int state_idx) {
  LogicalOrderings::DFSMState &state = m_dfsm_states[state_idx];
  for (int nfsm_state_idx : state.nfsm_states) {
    int ordering_idx = m_states[nfsm_state_idx].satisfied_ordering_idx;
    if (m_states[nfsm_state_idx].type == NFSMState::INTERESTING &&
        ordering_idx < kMaxSupportedOrderings &&
        m_orderings[ordering_idx].type == OrderingWithInfo::INTERESTING) {
      state.follows_interesting_order.set(ordering_idx);
    }
    state.can_reach_interesting_order |=
        m_states[nfsm_state_idx].can_reach_interesting_order;
  }
  state.next_state =
      Bounds_checked_array<int>::Alloc(thd->mem_root, m_fds.size());
  fill(state.next_state.begin(), state.next_state.end(), state_idx);
}

void LogicalOrderings::ExpandThroughAlwaysActiveFDs(
    Mem_root_array<int> *nfsm_states, int *generation,
    int extra_allowed_fd_idx) {
  ++*generation;  // Effectively clear the “seen” flag in all NFSM states.
  for (size_t i = 0; i < nfsm_states->size(); ++i) {
    const NFSMState &state = m_states[(*nfsm_states)[i]];
    for (int outgoing_edge_idx : state.outgoing_edges) {
      const NFSMEdge &edge = m_edges[outgoing_edge_idx];
      if ((AlwaysActiveFD(edge.required_fd_idx) ||
           edge.required_fd_idx == extra_allowed_fd_idx) &&
          m_states[edge.state_idx].seen != *generation) {
        nfsm_states->push_back(edge.state_idx);
        m_states[edge.state_idx].seen = *generation;
      }
    }
  }
}

/**
  From the NFSM, convert an equivalent DFSM.

  This is by means of the so-called powerset conversion, which is more commonly
  used to convert NFAs to DFAs. (The only real difference is that FAs have
  accepting states, while our FSM instead needs to store information about
  constituent interesting order states.)

  The powerset algorithm works by creating DFSM states that represent sets of
  NFSM states we could be in. E.g., if we have a state (a) and an FD {} → x can
  lead to new states () (ϵ-edge), (a) (implicit self-edge), (x), (ax), (xa),
  then we create a single new DFSM state that represent all those five states,
  and an {} → x edge from {(a)} to that new state. When creating edges from
  such superstates, we need to follow that FD from _all_ of them, so the list
  of constituent states can be fairly large.

  In theory, we could get 2^n DFSM states from n NFSM states, but in practice,
  we get fewer since our orderings generally only increase, not decrease.
  We only generate DFSM states by following FDs from the initial NFSM state;
  we don't create states eagerly for all 2^n possibilities.

  When creating DFSM states, we always include states that can be reached by
  means of always-active FDs. The ϵ edge (drop the last element from the
  ordering) is always active, and the client can also mark others as such.
  This means we get fewer DFSM states and fewer FDs to follow. See
  FunctionalDependency::always_active.
 */
void LogicalOrderings::ConvertNFSMToDFSM(THD *thd) {
  // See NFSMState::seen.
  int generation = 0;

  // Create the initial DFSM state. It consists of everything in the initial
  // NFSM state, and everything reachable from it with only always-active FDs.
  DFSMState initial;
  initial.nfsm_states.init(thd->mem_root);
  initial.nfsm_states.push_back(0);
  ExpandThroughAlwaysActiveFDs(&initial.nfsm_states, &generation,
                               /*extra_allowed_fd_idx=*/0);
  m_dfsm_states.push_back(move(initial));
  FinalizeDFSMState(thd, /*state_idx=*/0);

  // Reachability information set by FinalizeDFSMState() will include those
  // that can be reached through SetOrder() nodes, so it's misleading.
  // Clear it; this isn't 100% active if interesting orderings can be reached
  // through FDs only, but it will ever cause too little pruning, not too much.
  m_dfsm_states[0].can_reach_interesting_order.reset();

  // Used in iteration below.
  Mem_root_array<int> nfsm_states(thd->mem_root);
  Mem_root_array<NFSMEdge> nfsm_edges(thd->mem_root);

  for (size_t dfsm_state_idx = 0; dfsm_state_idx < m_dfsm_states.size();
       ++dfsm_state_idx) {
    // Take the union of all outgoing edges from the constituent NFSM m_states,
    // ignoring ϵ-edges and always active FDs, since we have special handling of
    // them below.
    nfsm_edges.clear();
    for (int nfsm_state_idx : m_dfsm_states[dfsm_state_idx].nfsm_states) {
      for (const int edge_idx : m_states[nfsm_state_idx].outgoing_edges) {
        const NFSMEdge &edge = m_edges[edge_idx];
        if (!AlwaysActiveFD(edge.required_fd_idx)) {
          nfsm_edges.push_back(edge);
        }
      }
    }

    {
      // Sort and deduplicate the edges. Note that we sort on FD first,
      // since we'll be grouping on that when creating new m_states.
      sort(nfsm_edges.begin(), nfsm_edges.end(),
           [](const NFSMEdge &a, const NFSMEdge &b) {
             return make_pair(a.required_fd_idx, a.state_idx) <
                    make_pair(b.required_fd_idx, b.state_idx);
           });
      auto new_end =
          unique(nfsm_edges.begin(), nfsm_edges.end(),
                 [](const NFSMEdge &a, const NFSMEdge &b) {
                   return make_pair(a.required_fd_idx, a.state_idx) ==
                          make_pair(b.required_fd_idx, b.state_idx);
                 });
      nfsm_edges.resize(distance(nfsm_edges.begin(), new_end));
    }

    // For each relevant FD, find out which set of m_states we could reach.
    m_dfsm_states[dfsm_state_idx].outgoing_edges.init(thd->mem_root);
    nfsm_states.clear();
    for (size_t edge_idx = 0; edge_idx < nfsm_edges.size(); ++edge_idx) {
      nfsm_states.push_back(nfsm_edges[edge_idx].state_idx);

      // Is this the last state in the group? If not, keep iterating.
      if (edge_idx != nfsm_edges.size() - 1 &&
          nfsm_edges[edge_idx].required_fd_idx ==
              nfsm_edges[edge_idx + 1].required_fd_idx) {
        continue;
      }

      // Add the implicit self-edges.
      for (int nfsm_state_idx : m_dfsm_states[dfsm_state_idx].nfsm_states) {
        if (nfsm_state_idx != 0) {
          nfsm_states.push_back(nfsm_state_idx);
        }
      }

      // Expand the set to contain any ϵ-edges and always active FDs,
      // in a breadth-first manner. Note that now, we might see new
      // edges for the same FD, so we should follow those as well.
      ExpandThroughAlwaysActiveFDs(&nfsm_states, &generation,
                                   nfsm_edges[edge_idx].required_fd_idx);

      // Canonicalize: Sort and deduplicate.
      sort(nfsm_states.begin(), nfsm_states.end());
      auto new_end = unique(nfsm_states.begin(), nfsm_states.end());
      nfsm_states.resize(distance(nfsm_states.begin(), new_end));

      // See if there is an existing DFSM state that matches the set of
      // NFSM m_states we've collected.
      int target_dfsm_state_idx = -1;
      for (size_t i = 0; i < m_dfsm_states.size(); ++i) {
        if (equal(nfsm_states.begin(), nfsm_states.end(),
                  m_dfsm_states[i].nfsm_states.begin(),
                  m_dfsm_states[i].nfsm_states.end())) {
          target_dfsm_state_idx = i;
          break;
        }
      }
      if (target_dfsm_state_idx == -1) {
        // There's none, so create a new one. The type doesn't really matter,
        // except for printing out the graph.
        DFSMState state;
        state.nfsm_states = move(nfsm_states);
        m_dfsm_states.push_back(move(state));
        FinalizeDFSMState(thd, m_dfsm_states.size() - 1);
        target_dfsm_state_idx = m_dfsm_states.size() - 1;
      }

      // Finally, add an edge in the DFSM. Ignore self-edges; they are implicit.
      if (static_cast<size_t>(target_dfsm_state_idx) != dfsm_state_idx) {
        DFSMEdge edge;
        edge.required_fd_idx = nfsm_edges[edge_idx].required_fd_idx;
        edge.state_idx = target_dfsm_state_idx;
        m_dfsm_edges.push_back(edge);

        DFSMState &dfsm_state = m_dfsm_states[dfsm_state_idx];
        dfsm_state.outgoing_edges.push_back(m_dfsm_edges.size() - 1);
        if (edge.required_fd_idx >= 0) {
          dfsm_state.next_state[edge.required_fd_idx] = target_dfsm_state_idx;
          if (edge.required_fd_idx >= 1 &&
              edge.required_fd_idx <= kMaxSupportedFDs) {
            dfsm_state.can_use_fd.set(edge.required_fd_idx - 1);
          }
        }
      }

      // Prepare for the next group.
      nfsm_states.clear();
    }
  }
}

void LogicalOrderings::FindInitialStatesForOrdering() {
  // Find all constructor edges from the initial state, and use them
  // to populate the table.
  for (int outgoing_edge_idx : m_dfsm_states[0].outgoing_edges) {
    const DFSMEdge &edge = m_dfsm_edges[outgoing_edge_idx];
    if (edge.required_fd_idx < 0) {
      const int ordering_idx = edge.required_fd_idx - INT_MIN;
      m_orderings[ordering_idx].state_idx = edge.state_idx;
    }
  }
}

string LogicalOrderings::PrintOrdering(Ordering ordering) const {
  const bool is_grouping = IsGrouping(ordering);
  string ret = is_grouping ? "{" : "(";
  for (size_t i = 0; i < ordering.size(); ++i) {
    if (i != 0) ret += ", ";
    ret += ItemToString(m_items[ordering[i].item].item);
    if (ordering[i].direction == ORDER_DESC) {
      ret += " DESC";
    }
  }
  ret += is_grouping ? '}' : ')';
  return ret;
}

string LogicalOrderings::PrintFunctionalDependency(
    const FunctionalDependency &fd, bool html) const {
  switch (fd.type) {
    case FunctionalDependency::DECAY:
      if (html) {
        return "&epsilon;";
      } else {
        return "eps";
      }
    case FunctionalDependency::EQUIVALENCE:
      return ItemToString(m_items[fd.head[0]].item) + "=" +
             ItemToString(m_items[fd.tail].item);
    case FunctionalDependency::FD: {
      string ret = "{";
      for (size_t i = 0; i < fd.head.size(); ++i) {
        if (i != 0) {
          ret += ", ";
        }
        ret += ItemToString(m_items[fd.head[i]].item);
      }
      if (html) {
        ret += "} &rarr; ";
      } else {
        ret += "} -> ";
      }
      ret += ItemToString(m_items[fd.tail].item);
      return ret;
    }
  }
  assert(false);
  return "";
}

void LogicalOrderings::PrintFunctionalDependencies(string *trace) {
  if (m_fds.size() <= 1) {
    *trace += "\nNo functional dependencies (after pruning).\n\n";
  } else {
    *trace += "\nFunctional dependencies (after pruning):\n";
    for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
      *trace +=
          " - " + PrintFunctionalDependency(m_fds[fd_idx], /*html=*/false);
      if (m_fds[fd_idx].always_active) {
        *trace += " [always active]";
      }
      *trace += "\n";
    }
    *trace += "\n";
  }
}

void LogicalOrderings::PrintInterestingOrders(string *trace) {
  *trace += "Interesting orders:\n";
  for (size_t order_idx = 1; order_idx < m_orderings.size(); ++order_idx) {
    const OrderingWithInfo &ordering = m_orderings[order_idx];
    *trace += StringPrintf(" - %zu: ", order_idx);
    bool first = true;
    for (OrderElement element : ordering.ordering) {
      if (!first) {
        *trace += ", ";
      }
      first = false;
      *trace += ItemToString(m_items[element.item].item);
      if (element.direction == ORDER_DESC) {
        *trace += " DESC";
      }
    }
    if (ordering.type == OrderingWithInfo::HOMOGENIZED) {
      *trace += " [homogenized from other ordering]";
    } else if (ordering.type == OrderingWithInfo::UNINTERESTING) {
      *trace += " [support order]";
    }
    *trace += "\n";
  }
  *trace += "\n";
}

void LogicalOrderings::PrintNFSMDottyGraph(string *trace) const {
  *trace += "digraph G {\n";
  for (size_t state_idx = 0; state_idx < m_states.size(); ++state_idx) {
    const NFSMState &state = m_states[state_idx];
    if (state.type == NFSMState::DELETED) {
      continue;
    }

    // We're printing the NFSM.
    *trace += StringPrintf("  s%zu [label=\"%s\"", state_idx,
                           PrintOrdering(state.satisfied_ordering).c_str());
    if (state.type == NFSMState::INTERESTING) {
      *trace += ", peripheries=2";
    }
    *trace += "]\n";

    for (size_t edge_idx : state.outgoing_edges) {
      const NFSMEdge &edge = m_edges[edge_idx];
      if (edge.required_fd_idx < 0) {
        // Pseudo-edge without a FD (from initial state only).
        *trace +=
            StringPrintf("  s%zu -> s%d [label=\"ordering %d\"]\n", state_idx,
                         edge.state_idx, edge.required_fd_idx - INT_MIN);
      } else {
        const FunctionalDependency *fd = edge.required_fd(this);
        *trace += StringPrintf(
            "  s%zu -> s%d [label=\"%s\"]\n", state_idx, edge.state_idx,
            PrintFunctionalDependency(*fd, /*html=*/true).c_str());
      }
    }
  }

  *trace += "}\n";
}

void LogicalOrderings::PrintDFSMDottyGraph(string *trace) const {
  *trace += "digraph G {\n";
  for (size_t state_idx = 0; state_idx < m_dfsm_states.size(); ++state_idx) {
    const DFSMState &state = m_dfsm_states[state_idx];
    *trace += StringPrintf("  s%zu [label=< ", state_idx);

    bool any_interesting = false;
    for (size_t i = 0; i < state.nfsm_states.size(); ++i) {
      const NFSMState &nsfm_state = m_states[state.nfsm_states[i]];
      if (i != 0) {
        *trace += ", ";
      }
      if (nsfm_state.type == NFSMState::INTERESTING) {
        any_interesting = true;
        *trace += "<b>";
      }
      *trace += PrintOrdering(nsfm_state.satisfied_ordering);
      if (nsfm_state.type == NFSMState::INTERESTING) {
        *trace += "</b>";
      }
    }
    *trace += " >";
    if (any_interesting) {
      *trace += ", peripheries=2";
    }
    *trace += "]\n";

    for (size_t edge_idx : state.outgoing_edges) {
      const DFSMEdge &edge = m_dfsm_edges[edge_idx];
      if (edge.required_fd_idx < 0) {
        // Pseudo-edge without a FD (from initial state only).
        *trace +=
            StringPrintf("  s%zu -> s%d [label=\"ordering %d\"]\n", state_idx,
                         edge.state_idx, edge.required_fd_idx - INT_MIN);
      } else {
        const FunctionalDependency *fd = edge.required_fd(this);
        *trace += StringPrintf(
            "  s%zu -> s%d [label=\"%s\"]\n", state_idx, edge.state_idx,
            PrintFunctionalDependency(*fd, /*html=*/true).c_str());
      }
    }
  }

  *trace += "}\n";
}
