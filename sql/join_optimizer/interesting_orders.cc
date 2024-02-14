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

#include "sql/join_optimizer/interesting_orders.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>

#include "map_helpers.h"
#include "my_hash_combine.h"
#include "my_pointer_arithmetic.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/mem_root_array.h"
#include "sql/parse_tree_nodes.h"
#include "sql/sql_array.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"

using std::all_of;
using std::distance;
using std::equal;
using std::fill;
using std::lower_bound;
using std::make_pair;
using std::max;
using std::none_of;
using std::ostream;
using std::pair;
using std::sort;
using std::string;
using std::swap;
using std::unique;
using std::upper_bound;

/**
   A scope-guard class for allocating an Ordering::Elements instance
   which is automatically returned to the pool when we exit the scope of
   the OrderingElementsGuard instance.
*/
class OrderingElementsGuard final {
 public:
  /**
     @param context The object containing the pool.
     @param mem_root For allocating additional Ordering::Elements instances if
     needed.
   */
  OrderingElementsGuard(LogicalOrderings *context, MEM_ROOT *mem_root)
      : m_context{context} {
    m_elements = context->RetrieveElements(mem_root);
  }

  // No copying of this class.
  OrderingElementsGuard(const OrderingElementsGuard &) = delete;
  OrderingElementsGuard &operator=(const OrderingElementsGuard &) = delete;

  ~OrderingElementsGuard() { m_context->ReturnElements(m_elements); }

  Ordering::Elements &Get() { return m_elements; }

 private:
  /// The object containing the pool.
  LogicalOrderings *m_context;

  /// The instance fetched from the pool.
  Ordering::Elements m_elements;
};

namespace {

// Set some maximum limits on the size of the FSMs, in order to prevent runaway
// computation on pathological queries. As rough reference: As of 8.0.26,
// there is a single query in the test suite hitting these limits (it wants 8821
// NFSM states and an estimated 2^50 DFSM states). Excluding that query, the
// test suite contains the following largest FSMs:
//
//  - Largest NFSM: 63 NFSM states => 2 DFSM states
//  - Largest DFSM: 37 NFSM states => 152 DFSM states
//
// And for DBT-3:
//
//  - Largest NFSM: 43 NFSM states => 3 DFSM states
//  - Largest DFSM: 8 NFSM states => 8 DFSM states
//
// We could make system variables out of these if needed, but they would
// probably have to be settable by superusers only, in order to prevent runaway
// unabortable queries from taking down the server. Having them as fixed limits
// is good enough for now.
constexpr int kMaxNFSMStates = 200;
constexpr int kMaxDFSMStates = 2000;

/**
   Check if 'elements' contains 'item'.
*/
bool Contains(Ordering::Elements elements, int item) {
  return std::any_of(elements.cbegin(), elements.cend(),
                     [item](OrderElement elem) { return elem.item == item; });
}

// Calculates the hash for a DFSM state given by an index into
// LogicalOrderings::m_dfsm_states. The hash is based on the set of NFSM states
// the DFSM state corresponds to.
template <typename DFSMState>
struct DFSMStateHash {
  const Mem_root_array<DFSMState> *dfsm_states;
  size_t operator()(int idx) const {
    size_t hash = 0;
    for (int nfsm_state : (*dfsm_states)[idx].nfsm_states) {
      my_hash_combine<size_t>(hash, nfsm_state);
    }
    return hash;
  }
};

// Checks if two DFSM states represent the same set of NFSM states.
template <typename DFSMState>
struct DFSMStateEqual {
  const Mem_root_array<DFSMState> *dfsm_states;
  bool operator()(int idx1, int idx2) const {
    return equal((*dfsm_states)[idx1].nfsm_states.begin(),
                 (*dfsm_states)[idx1].nfsm_states.end(),
                 (*dfsm_states)[idx2].nfsm_states.begin(),
                 (*dfsm_states)[idx2].nfsm_states.end());
  }
};

}  // namespace

void Ordering::Deduplicate() {
  assert(Valid());
  size_t length = 0;
  for (size_t i = 0; i < m_elements.size(); ++i) {
    if (!Contains(m_elements.prefix(length), m_elements[i].item)) {
      m_elements[length++] = m_elements[i];
    }
  }
  m_elements.resize(length);
}

bool Ordering::Valid() const {
  switch (m_kind) {
    case Kind::kEmpty:
      return m_elements.empty();

    case Kind::kOrder:
      return !m_elements.empty() &&
             std::all_of(m_elements.cbegin(), m_elements.cend(),
                         [](OrderElement e) {
                           return e.direction != ORDER_NOT_RELEVANT;
                         });

    case Kind::kRollup:
    case Kind::kGroup:
      return !m_elements.empty() &&
             std::all_of(m_elements.cbegin(), m_elements.cend(),
                         [](OrderElement e) {
                           return e.direction == ORDER_NOT_RELEVANT;
                         });
  }

  assert(false);
  return false;
}

LogicalOrderings::LogicalOrderings(THD *thd)
    : m_items(thd->mem_root),
      m_orderings(thd->mem_root),
      m_fds(thd->mem_root),
      m_states(thd->mem_root),
      m_dfsm_states(thd->mem_root),
      m_dfsm_edges(thd->mem_root),
      m_elements_pool(thd->mem_root) {
  GetHandle(nullptr);  // Always has the zero handle.

  // Add the empty ordering/grouping.
  m_orderings.push_back(OrderingWithInfo{Ordering(),
                                         OrderingWithInfo::UNINTERESTING,
                                         /*used_at_end=*/true});

  FunctionalDependency decay_fd;
  decay_fd.type = FunctionalDependency::DECAY;
  decay_fd.tail = 0;
  decay_fd.always_active = true;
  m_fds.push_back(decay_fd);
}

int LogicalOrderings::AddOrderingInternal(THD *thd, Ordering order,
                                          OrderingWithInfo::Type type,
                                          bool used_at_end,
                                          table_map homogenize_tables) {
  assert(!m_built);

#ifndef NDEBUG
  if (order.GetKind() == Ordering::Kind::kGroup) {
    Ordering::Elements elements = order.GetElements();
    // Verify that the grouping is sorted and deduplicated.
    for (size_t i = 1; i < elements.size(); ++i) {
      assert(elements[i].item > elements[i - 1].item);
      assert(elements[i].direction == ORDER_NOT_RELEVANT);
    }

    // Verify that none of the items are of ROW_RESULT,
    // as RemoveDuplicatesIterator cannot handle them.
    // (They would theoretically be fine for orderings.)
    for (size_t i = 0; i < elements.size(); ++i) {
      assert(m_items[elements[i].item].item->result_type() != ROW_RESULT);
    }
  }
#endif

  if (type != OrderingWithInfo::UNINTERESTING) {
    for (OrderElement element : order.GetElements()) {
      if (element.direction == ORDER_ASC) {
        m_items[element.item].used_asc = true;
      }
      if (element.direction == ORDER_DESC) {
        m_items[element.item].used_desc = true;
      }
      if (element.direction == ORDER_NOT_RELEVANT) {
        m_items[element.item].used_in_grouping = true;
      }
    }
  }

  // Deduplicate against all the existing ones.
  for (size_t i = 0; i < m_orderings.size(); ++i) {
    if (m_orderings[i].ordering == order) {
      // Potentially promote the existing one.
      m_orderings[i].type = std::max(m_orderings[i].type, type);
      m_orderings[i].homogenize_tables |= homogenize_tables;
      return i;
    }
  }

  m_orderings.push_back(OrderingWithInfo{order.Clone(thd->mem_root), type,
                                         used_at_end, homogenize_tables});
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

  fd.head = fd.head.Clone(thd->mem_root);
  m_fds.push_back(fd);
  return m_fds.size() - 1;
}

void LogicalOrderings::Build(THD *thd) {
  // If we have no interesting orderings or groupings, just create a DFSM
  // directly with a single state for the empty ordering.
  if (m_orderings.size() == 1) {
    m_dfsm_states.reserve(1);
    m_dfsm_states.emplace_back();
    DFSMState &initial = m_dfsm_states.back();
    initial.nfsm_states.init(thd->mem_root);
    initial.nfsm_states.reserve(1);
    initial.nfsm_states.push_back(0);
    initial.next_state =
        Bounds_checked_array<int>::Alloc(thd->mem_root, m_fds.size());
    m_optimized_ordering_mapping =
        Bounds_checked_array<int>::Alloc(thd->mem_root, 1);
    m_built = true;
    return;
  }

  BuildEquivalenceClasses();
  RecanonicalizeGroupings();
  AddFDsFromComputedItems(thd);
  AddFDsFromConstItems(thd);
  AddFDsFromAggregateItems(thd);
  PreReduceOrderings(thd);
  CreateOrderingsFromGroupings(thd);
  CreateHomogenizedOrderings(thd);
  PruneFDs(thd);
  if (TraceStarted(thd)) {
    PrintFunctionalDependencies(&Trace(thd));
  }
  FindElementsThatCanBeAddedByFDs();
  PruneUninterestingOrders(thd);
  if (TraceStarted(thd)) {
    PrintInterestingOrders(&Trace(thd));
  }
  BuildNFSM(thd);
  if (TraceStarted(thd)) {
    Trace(thd) << "NFSM for interesting orders, before pruning:\n";
    PrintNFSMDottyGraph(&Trace(thd));
    if (m_states.size() >= kMaxNFSMStates) {
      Trace(thd) << "NOTE: NFSM is incomplete, because it became too big.\n";
    }
  }
  PruneNFSM(thd);
  if (TraceStarted(thd)) {
    Trace(thd) << "\nNFSM for interesting orders, after pruning:\n";
    PrintNFSMDottyGraph(&Trace(thd));
  }
  ConvertNFSMToDFSM(thd);
  if (TraceStarted(thd)) {
    Trace(thd) << "\nDFSM for interesting orders:\n";
    PrintDFSMDottyGraph(&Trace(thd));
    if (m_dfsm_states.size() >= kMaxDFSMStates) {
      Trace(thd) << "NOTE: DFSM does not contain all NFSM states, because it "
                    "became too "
                    "big.\n";
    }
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
    static_assert(kMaxSupportedFDs <= sizeof(unsigned long long) * CHAR_BIT);
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
      Ordering &ordering = m_orderings[ordering_idx].ordering;

      // We are not prepared for uninteresting groupings yet.
      assert(ordering.GetKind() != Ordering::Kind::kGroup);

      // Find the longest prefix that contains only elements that are used in
      // interesting groupings. We will never shorten the uninteresting ordering
      // below this; it is overconservative in some cases, but it makes sure
      // we never miss a path to an interesting grouping.
      size_t minimum_prefix_len = 0;
      const Ordering::Elements &elements = ordering.GetElements();
      while (elements.size() > minimum_prefix_len &&
             m_items[m_items[elements[minimum_prefix_len].item].canonical_item]
                 .used_in_grouping) {
        ++minimum_prefix_len;
      }

      // Shorten this ordering one by one element, until it can (heuristically)
      // become an interesting ordering with the FDs we have. Note that it might
      // become the empty ordering, and if so, it will be deleted entirely
      // in the step below.
      while (elements.size() > minimum_prefix_len &&
             !CouldBecomeInterestingOrdering(ordering)) {
        if (elements.size() > 1) {
          ordering = Ordering(elements.without_back(), ordering.GetKind());
        } else {
          ordering = Ordering();
        }
      }
    }

    // Since some orderings may have changed, we need to re-deduplicate.
    // Note that at this point, we no longer care about used_at_end;
    // it was only used for reducing orderings in homogenization.
    m_optimized_ordering_mapping[ordering_idx] = new_length;
    for (int i = 0; i < new_length; ++i) {
      if (m_orderings[i].ordering == m_orderings[ordering_idx].ordering) {
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
  // Discard all useless FDs. (Items not part of some ordering will cause
  // the new proposed ordering to immediately be pruned away, so this is
  // safe. See also the comment in the .h file about transitive dependencies.)
  //
  // Note that this will sometimes leave useless FDs; if we have e.g. a → b
  // and b is useful, we will mark the FD as useful even if nothing can
  // produce a. However, such FDs don't induce more NFSM states (which is
  // the main point of the pruning), it just slows the NFSM down slightly,
  // and by far the dominant FDs to prune in our cases are the ones
  // induced by keys, e.g. S → k where S is always the same and k
  // is useless. These are caught by this heuristic.

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
    if (m_items[tail].used_asc || m_items[tail].used_desc ||
        m_items[tail].used_in_grouping) {
      used_fd = true;
    } else if (fd.type == FunctionalDependency::EQUIVALENCE) {
      ItemHandle head = m_items[fd.head[0]].canonical_item;
      if (m_items[head].used_asc || m_items[head].used_desc ||
          m_items[head].used_in_grouping) {
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
      m_items[canonical_item].used_in_grouping |=
          m_items[duplicate_item].used_in_grouping;
      done_anything = true;
    }
  } while (done_anything);
}

// Put all groupings into a canonical form that we can compare them
// as orderings without further logic. (It needs to be on a form that
// does not change markedly after applying equivalences, and it needs
// to be deterministic, but apart from that, the order is pretty arbitrary.)
// We can only do this after BuildEquivalenceClasses().
void LogicalOrderings::RecanonicalizeGroupings() {
  for (OrderingWithInfo &ordering : m_orderings) {
    if (ordering.ordering.GetKind() == Ordering::Kind::kGroup) {
      SortElements(ordering.ordering.GetElements());
    }
  }
}

// Window functions depend on both the function argument and on the PARTITION BY
// clause, so we need to add both to the functional dependency's head.
// The order of elements is arbitrary.
Bounds_checked_array<ItemHandle>
LogicalOrderings::CollectHeadForStaticWindowFunction(THD *thd,
                                                     ItemHandle argument_item,
                                                     Window *window) {
  const PT_order_list *partition_by = window->effective_partition_by();
  int partition_len = 0;
  if (partition_by != nullptr) {
    for (ORDER *order = partition_by->value.first; order != nullptr;
         order = order->next) {
      ++partition_len;
    }
  }
  auto head =
      Bounds_checked_array<ItemHandle>::Alloc(thd->mem_root, partition_len + 1);
  if (partition_by != nullptr) {
    for (ORDER *order = partition_by->value.first; order != nullptr;
         order = order->next) {
      head[partition_len--] = GetHandle(*order->item);
    }
  }
  head[0] = argument_item;
  return head;
}

/**
  Try to add new FDs from items that are not base items; e.g., if we have
  an item (a + 1), we add {a} → (a + 1) (since addition is deterministic).
  This can help reducing orderings that are on such derived items.
  For simplicity, we only bother doing this for items that derive from a
  single base field; i.e., from (a + b), we don't add {a,b} → (a + b)
  even though we could. Also note that these are functional dependencies,
  not equivalences; even though ORDER BY (a + 1) could be satisfied by an
  ordering on (a) (barring overflow issues), this does not hold in general,
  e.g. ORDER BY (-a) is _not_ satisfied by an ordering on (a), not to mention
  ORDER BY (a*a). We do not have the framework in Item to understand which
  functions are monotonous, so we do not attempt to create equivalences.

  This is really the only the case where we can get transitive FDs that are not
  equivalences. Since our approach does not apply FDs transitively without
  adding the intermediate item (e.g., for {a} → b and {b} → c, we won't extend
  (a) to (ac), only to (abc)), we extend any existing FDs here when needed.
 */
void LogicalOrderings::AddFDsFromComputedItems(THD *thd) {
  int num_original_items = m_items.size();
  int num_original_fds = m_fds.size();
  for (int item_idx = 0; item_idx < num_original_items; ++item_idx) {
    // We only care about items that are used in some ordering,
    // not any used as base in FDs or the likes.
    const ItemHandle canonical_idx = m_items[item_idx].canonical_item;
    if (!m_items[canonical_idx].used_asc && !m_items[canonical_idx].used_desc &&
        !m_items[canonical_idx].used_in_grouping) {
      continue;
    }

    // We only want to look at items that are not already Item_field
    // or aggregate functions (the latter are handled in
    // AddFDsFromAggregateItems()), and that are generated from a single field.
    // Some quick heuristics will eliminate most of these for us.
    Item *item = m_items[item_idx].item;
    const table_map used_tables = item->used_tables();
    if (item->type() == Item::FIELD_ITEM || item->has_aggregation() ||
        Overlaps(used_tables, PSEUDO_TABLE_BITS) ||
        !std::has_single_bit(used_tables)) {
      continue;
    }

    // Window functions have much more state than just the parameter,
    // so we cannot say that e.g. {a} → SUM(a) OVER (...), unless we
    // know that the function is over the entire frame (unbounded).
    //
    // TODO(sgunders): We could also add FDs for window functions
    // where could guarantee that the partition is only one row.
    bool is_static_wf = false;
    if (item->has_wf()) {
      if (item->m_is_window_function &&
          down_cast<Item_sum *>(item)->framing() &&
          down_cast<Item_sum *>(item)->window()->static_aggregates()) {
        is_static_wf = true;
      } else {
        continue;
      }
    }

    Item_field *base_field = nullptr;
    bool error =
        WalkItem(item, enum_walk::POSTFIX, [&base_field](Item *sub_item) {
          if (sub_item->type() == Item::FUNC_ITEM &&
              down_cast<Item_func *>(sub_item)->functype() ==
                  Item_func::ROLLUP_GROUP_ITEM_FUNC) {
            // Rollup items are nondeterministic, yet don't always set
            // RAND_TABLE_BIT.
            return true;
          }
          if (sub_item->type() == Item::FIELD_ITEM) {
            if (base_field != nullptr && !base_field->eq(sub_item)) {
              // More than one field in use.
              return true;
            }
            base_field = down_cast<Item_field *>(sub_item);
          }
          return false;
        });
    if (error || base_field == nullptr) {
      // More than one field in use, or no fields in use
      // (can happen even when used_tables is set, e.g. for
      // an Item_view_ref to a constant).
      continue;
    }

    if (!base_field->field->binary()) {
      // Fields with collations can have equality (with no tiebreaker)
      // even with fields that contain differing binary data.
      // Thus, functions do not always preserve equality; a == b
      // does not mean f(a) == f(b), and thus, the FD does not
      // hold either.
      continue;
    }

    ItemHandle head_item = GetHandle(base_field);
    FunctionalDependency fd;
    fd.type = FunctionalDependency::FD;
    if (is_static_wf) {
      fd.head = CollectHeadForStaticWindowFunction(
          thd, head_item, down_cast<Item_sum *>(item)->window());
    } else {
      fd.head = Bounds_checked_array<ItemHandle>(&head_item, 1);
    }
    fd.tail = item_idx;
    fd.always_active = true;
    AddFunctionalDependency(thd, fd);

    if (fd.head.size() == 1) {
      // Extend existing FDs transitively (see function comment).
      // E.g. if we have S → base, also add S → item.
      for (int fd_idx = 0; fd_idx < num_original_fds; ++fd_idx) {
        if (m_fds[fd_idx].type == FunctionalDependency::FD &&
            m_fds[fd_idx].tail == head_item && m_fds[fd_idx].always_active) {
          fd = m_fds[fd_idx];
          fd.tail = item_idx;
          AddFunctionalDependency(thd, fd);
        }
      }
    }
  }
}

/**
  Try to add FDs from items that are constant by themselves, e.g. if someone
  does ORDER BY 'x', add a new FD {} → 'x' so that the ORDER BY can be elided.

  TODO(sgunders): This can potentially remove subqueries or other functions
  that would throw errors if actually executed, potentially modifying
  semantics. See if that is illegal, and thus, if we need to test-execute them
  at least once somehow (ideally not during optimization).
 */
void LogicalOrderings::AddFDsFromConstItems(THD *thd) {
  int num_original_items = m_items.size();
  for (int item_idx = 0; item_idx < num_original_items; ++item_idx) {
    // We only care about items that are used in some ordering,
    // not any used as base in FDs or the likes.
    const ItemHandle canonical_idx = m_items[item_idx].canonical_item;
    if (!m_items[canonical_idx].used_asc && !m_items[canonical_idx].used_desc &&
        !m_items[canonical_idx].used_in_grouping) {
      continue;
    }

    if (m_items[item_idx].item->const_for_execution()) {
      // Add {} → item.
      FunctionalDependency fd;
      fd.type = FunctionalDependency::FD;
      fd.head = Bounds_checked_array<ItemHandle>();
      fd.tail = item_idx;
      fd.always_active = true;
      AddFunctionalDependency(thd, fd);
    }
  }
}

void LogicalOrderings::AddFDsFromAggregateItems(THD *thd) {
  // If ROLLUP is active, and we have nullable GROUP BY expressions, we could
  // get two different NULL groups with different aggregates; one for the actual
  // NULL value, and one for the rollup group. If so, these FDs no longer hold,
  // and we cannot add them.
  if (m_rollup) {
    for (ItemHandle item : m_aggregate_head) {
      if (m_items[item].item->is_nullable()) {
        return;
      }
    }
  }

  int num_original_items = m_items.size();
  for (int item_idx = 0; item_idx < num_original_items; ++item_idx) {
    // We only care about items that are used in some ordering,
    // not any used as base in FDs or the likes.
    const ItemHandle canonical_idx = m_items[item_idx].canonical_item;
    if (!m_items[canonical_idx].used_asc && !m_items[canonical_idx].used_desc &&
        !m_items[canonical_idx].used_in_grouping) {
      continue;
    }

    if (m_items[item_idx].item->has_aggregation() &&
        !m_items[item_idx].item->has_wf()) {
      // Add {all GROUP BY items} → item.
      // Note that the head might be empty, for implicit grouping,
      // which means all aggregate items are constant (there is only one row).
      FunctionalDependency fd;
      fd.type = FunctionalDependency::FD;
      fd.head = m_aggregate_head;
      fd.tail = item_idx;
      fd.always_active = true;
      AddFunctionalDependency(thd, fd);
    }
  }
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
  Checks whether the given item is redundant given previous elements in
  the ordering; ie., whether adding it will never change the ordering.
  This could either be because it's a duplicate, or because it is implied
  by functional dependencies. When this is applied to all elements in turn,
  it is called “reducing” the ordering. [Neu04] claims that this operation
  is not confluent, which is erroneous (their example is faulty, ignoring
  that Simmen reduces from the back). [Neu04b] has modified the claim to
  be that it is not confluent for _groupings_, which is correct.
  We make no attempt at optimality.

  If all_fds is true, we consider all functional dependencies, including those
  that may not always be active; e.g. a FD a=b may come from a join, and thus
  does not hold before the join is actually done, but we assume it holds anyway.
  This is OK from order homogenization, which is concerned with making orderings
  that will turn into the desired interesting ordering (e.g. for ORDER BY) only
  after all joins have been done. It would not be OK if we were to use it for
  merge joins somehow.
 */
bool LogicalOrderings::ImpliedByEarlierElements(ItemHandle item,
                                                Ordering::Elements prefix,
                                                bool all_fds) const {
  // First, search for straight-up duplicates (ignoring ASC/DESC).
  if (Contains(prefix, item)) {
    return true;
  }

  // Check if this item is implied by any of the functional dependencies.
  for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
    const FunctionalDependency &fd = m_fds[fd_idx];
    if (!all_fds && !fd.always_active) {
      continue;
    }
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
  Do safe reduction on all orderings (some of them may get merged by
  PruneUninterestingOrders() later), ie., remove all items that may be removed
  using only FDs that always are active.

  There's a problem in [Neu04] that is never adequately addressed; orderings are
  only ever expanded, and then eventually compared against interesting orders.
  But the interesting order itself is not necessarily extended, due to pruning.
  For instance, if an index could yield (x,y) and we have {} → x, there's no way
  we could get it to match the interesting order (y) even though they are
  logically equivalent. For an even trickier case, imagine an index (x,y) and
  an interesting order (y,z), with {} → x and y → z. For this to match, we'd
  need to have a “super-order” (x,y,z) and infer that from both orderings.

  Instead, we do a pre-step related to Simmen's “Test Ordering” procedure;
  we reduce the orderings. In the example above, both will be reduced to (y),
  and then match. This is mostly a band-aid around the problem; for instance,
  it cannot deal with FDs that are not always active, and it does not deal
  adequately with groupings (since reduction does not).

  Note that this could make the empty ordering interesting after merging.
 */
void LogicalOrderings::PreReduceOrderings(THD *thd) {
  for (OrderingWithInfo &ordering : m_orderings) {
    OrderingElementsGuard tmp_guard(this, thd->mem_root);
    Ordering reduced_ordering =
        ReduceOrdering(ordering.ordering,
                       /*all_fds=*/false, tmp_guard.Get());
    if (reduced_ordering.size() < ordering.ordering.size()) {
      ordering.ordering = reduced_ordering.Clone(thd->mem_root);
    }
  }
}

/**
  We don't currently have any operators that only group and do not sort
  (e.g. hash grouping), so we always implement grouping by sorting.
  This function makes that representation explicit -- for each grouping,
  it will make sure there is at least one ordering representing that
  grouping. This means we never need to “sort by a grouping”, which
  would destroy ordering information that could be useful later.

  As an example, take SELECT ... GROUP BY a, b ORDER BY a. This needs to
  group first by {a,b} (assume we're using filesort, not an index),
  then sort by (a). If we just represent the sort we're doing as going
  directly to {a,b}, we can't elide the sort on (a). Instead, we create
  a sort (a,b) (implicitly convertible to {a,b}), which makes the FSM
  understand that we're _both_ sorted on (a,b) and grouped on {a,b},
  and then also sorted on (a).

  Any given grouping would be satisfied by lots of different orderings:
  {a,b} could be (a,b), (b,a), (a DESC, b) etc.. We look through all
  interesting orders that are a subset of our grouping, and if they are,
  we extend them arbitrarily to complete the grouping. E.g., if our
  grouping is {a,b,c,d} and the ordering (c DESC, b) is interesting,
  we make a homogenized ordering (c DESC, b, a, d). This is roughly
  equivalent to Simmen's “Cover Order” procedure. If we cannot make
  such a cover, we simply make a new last-resort ordering (a,b,c,d).

  We don't consider equivalences here; perhaps we should, at least
  for at-end groupings.
 */
void LogicalOrderings::CreateOrderingsFromGroupings(THD *thd) {
  OrderingElementsGuard tmp_guard(this, thd->mem_root);
  Ordering::Elements &tmp = tmp_guard.Get();
  int num_original_orderings = m_orderings.size();
  for (int grouping_idx = 1; grouping_idx < num_original_orderings;
       ++grouping_idx) {
    const Ordering &grouping = m_orderings[grouping_idx].ordering;
    if (grouping.GetKind() != Ordering::Kind::kGroup ||
        m_orderings[grouping_idx].type != OrderingWithInfo::INTERESTING) {
      continue;
    }

    bool has_cover = false;
    for (int ordering_idx = 1; ordering_idx < num_original_orderings;
         ++ordering_idx) {
      const Ordering &ordering = m_orderings[ordering_idx].ordering;
      if (ordering.GetKind() != Ordering::Kind::kOrder ||
          m_orderings[ordering_idx].type != OrderingWithInfo::INTERESTING ||
          ordering.size() > grouping.size()) {
        continue;
      }
      bool can_cover =
          all_of(ordering.GetElements().begin(), ordering.GetElements().end(),
                 [&grouping](const OrderElement &element) {
                   return Contains(grouping.GetElements(), element.item);
                 });
      if (!can_cover) {
        continue;
      }

      has_cover = true;

      // On a full match, just note that we have a cover, don't make a new
      // ordering. We assume both are free of duplicates.
      if (ordering.size() == grouping.size()) {
        continue;
      }

      for (size_t i = 0; i < ordering.size(); ++i) {
        tmp[i] = ordering.GetElements()[i];
      }
      int len = ordering.size();
      for (const OrderElement &element : grouping.GetElements()) {
        if (!Contains(ordering.GetElements(), element.item)) {
          tmp[len].item = element.item;
          tmp[len].direction = ORDER_ASC;  // Arbitrary.
          ++len;
        }
      }
      assert(len == static_cast<int>(grouping.size()));

      AddOrderingInternal(
          thd, Ordering(tmp.prefix(len), Ordering::Kind::kOrder),
          OrderingWithInfo::HOMOGENIZED, m_orderings[grouping_idx].used_at_end,
          /*homogenize_tables=*/0);
    }

    // Make a fallback ordering if no cover was found.
    if (!has_cover) {
      for (size_t i = 0; i < grouping.size(); ++i) {
        tmp[i].item = grouping.GetElements()[i].item;
        tmp[i].direction = ORDER_ASC;  // Arbitrary.
      }

      AddOrderingInternal(
          thd, Ordering(tmp.prefix(grouping.size()), Ordering::Kind::kOrder),
          OrderingWithInfo::HOMOGENIZED, m_orderings[grouping_idx].used_at_end,
          /*homogenize_tables=*/0);
    }
  }
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

  // Now, for each table, try to see if we can rewrite an ordering
  // to something only referring to that table, by swapping out non-conforming
  // items for others.
  int num_original_orderings = m_orderings.size();
  for (int ordering_idx = 1; ordering_idx < num_original_orderings;
       ++ordering_idx) {
    if (m_orderings[ordering_idx].type == OrderingWithInfo::UNINTERESTING) {
      continue;
    }
    if (m_orderings[ordering_idx].ordering.GetKind() ==
        Ordering::Kind::kGroup) {
      // We've already made orderings out of these, which will be
      // homogenized, so we don't need to homogenize the grouping itself,
      // too.
      continue;
    }

    OrderingElementsGuard tmp_guard(this, thd->mem_root);
    const Ordering &reduced_ordering = ReduceOrdering(
        m_orderings[ordering_idx].ordering,
        /*all_fds=*/m_orderings[ordering_idx].used_at_end, tmp_guard.Get());
    if (reduced_ordering.GetElements().empty()) {
      continue;
    }

    // Now try to homogenize it onto all tables in turn.
    table_map homogenize_tables;
    if (m_orderings[ordering_idx].used_at_end) {
      // Try all tables.
      homogenize_tables = seen_tables;
    } else {
      // Try only the ones we were asked to (because it's not relevant
      // for later tables anyway).
      homogenize_tables = m_orderings[ordering_idx].homogenize_tables;
    }
    for (int table_idx : BitsSetIn(homogenize_tables)) {
      AddHomogenizedOrderingIfPossible(thd, reduced_ordering,
                                       m_orderings[ordering_idx].used_at_end,
                                       table_idx, reverse_canonical);
    }
  }
}

/**
  Remove redundant elements using the functional dependencies that we have,
  to give a more canonical form before homogenization. Note that we assume
  here that every functional dependency holds, so this is not applicable
  generally throughout the tree, only at the end (e.g. final ORDER BY).
  This is called “Reduce Order” in the Simmen paper.
 */
Ordering LogicalOrderings::ReduceOrdering(Ordering ordering, bool all_fds,
                                          Ordering::Elements tmp) const {
  size_t reduced_length = 0;
  for (size_t part_idx = 0; part_idx < ordering.size(); ++part_idx) {
    if (ImpliedByEarlierElements(ordering.GetElements()[part_idx].item,
                                 ordering.GetElements().prefix(part_idx),
                                 all_fds)) {
      // Delete this element.
    } else {
      tmp[reduced_length++] = ordering.GetElements()[part_idx];
    }
  }
  return {tmp.prefix(reduced_length),
          reduced_length > 0 ? ordering.GetKind() : Ordering::Kind::kEmpty};
}

/// Helper function for CreateHomogenizedOrderings().
void LogicalOrderings::AddHomogenizedOrderingIfPossible(
    THD *thd, const Ordering &reduced_ordering, bool used_at_end, int table_idx,
    Bounds_checked_array<pair<ItemHandle, ItemHandle>> reverse_canonical) {
  OrderingElementsGuard tmp_guard(this, thd->mem_root);
  Ordering::Elements &tmp = tmp_guard.Get();
  const table_map available_tables = table_map{1} << table_idx;
  int length = 0;

  for (OrderElement element : reduced_ordering.GetElements()) {
    if (IsSubset(m_items[element.item].item->used_tables(), available_tables)) {
      // Already OK.
      if (!ImpliedByEarlierElements(element.item, tmp.prefix(length),
                                    /*all_fds=*/used_at_end)) {
        tmp[length++] = element;
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
        if (ImpliedByEarlierElements(it->second, tmp.prefix(length),
                                     /*all_fds=*/used_at_end)) {
          // Unneeded in the new order, so delete it.
          // Similar to the reduction process above.
        } else {
          tmp[length].item = it->second;
          tmp[length].direction = element.direction;
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

  if (length > 0) {
    if (reduced_ordering.GetKind() == Ordering::Kind::kGroup) {
      // We've replaced some items, so we need to re-sort.
      SortElements(tmp.prefix(length));
    }

    AddOrderingInternal(
        thd, Ordering(tmp.prefix(length), reduced_ordering.GetKind()),
        OrderingWithInfo::HOMOGENIZED, used_at_end,
        /*homogenize_tables=*/0);
  }
}

void LogicalOrderings::SortElements(Ordering::Elements elements) const {
  assert(std::all_of(elements.cbegin(), elements.cend(), [](OrderElement e) {
    return e.direction == ORDER_NOT_RELEVANT;
  }));

  sort(elements.begin(), elements.end(),
       [this](const OrderElement &a, const OrderElement &b) {
         return this->ItemBeforeInGroup(a, b);
       });
}

ItemHandle LogicalOrderings::GetHandle(Item *item) {
  for (size_t i = 1; i < m_items.size(); ++i) {
    if (item == m_items[i].item || item->eq(m_items[i].item)) {
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
bool LogicalOrderings::CouldBecomeInterestingOrdering(
    const Ordering &ordering) const {
  for (OrderingWithInfo other_ordering : m_orderings) {
    const Ordering interesting_ordering = other_ordering.ordering;
    if (other_ordering.type != OrderingWithInfo::INTERESTING ||
        interesting_ordering.size() < ordering.size()) {
      continue;
    }

    // Groupings can never become orderings. Orderings can become groupings,
    // but for simplicity, we require them to immediately become groupings then,
    // or else be pruned away.
    if (ordering.GetKind() != interesting_ordering.GetKind()) {
      continue;
    }

    // Since groupings are ordered by item (actually canonical item;
    // see RecanonicalizeGroupings(), ItemBeforeInGroup() and
    // the GroupReordering unit test), we can use the same comparison
    // for ordering-ordering and grouping-grouping comparisons.
    bool match = true;
    for (size_t i = 0, j = 0;
         i < ordering.size() || j < interesting_ordering.size();) {
      if (ordering.size() - i > interesting_ordering.size() - j) {
        // We have excess items at the end, so give up.
        match = false;
        break;
      }

      const ItemHandle needed_item =
          m_items[interesting_ordering.GetElements()[j].item].canonical_item;
      if (i < ordering.size() &&
          m_items[ordering.GetElements()[i].item].canonical_item ==
              needed_item &&
          ordering.GetElements()[i].direction ==
              interesting_ordering.GetElements()[j].direction) {
        // We have a matching item, so move both iterators along.
        ++i, ++j;
        continue;
      }

      if (m_items[needed_item].can_be_added_by_fd) {
        // We don't have this item, but it could be generated, so skip it.
        ++j;
        continue;
      }

      // We don't have this item, and it can not be added later, so give up.
      match = false;
      break;
    }
    if (match) {
      return true;
    }
  }
  return false;
}

int LogicalOrderings::AddArtificialState(THD *thd, const Ordering &ordering) {
  for (size_t i = 0; i < m_states.size(); ++i) {
    if (m_states[i].satisfied_ordering == ordering) {
      return i;
    }
  }

  NFSMState state;
  state.satisfied_ordering = ordering.Clone(thd->mem_root);
  state.satisfied_ordering_idx = -1;  // Irrelevant, but placate the compiler.
  state.outgoing_edges.init(thd->mem_root);
  state.type = NFSMState::ARTIFICIAL;
  m_states.push_back(std::move(state));
  return m_states.size() - 1;
}

void LogicalOrderings::AddEdge(THD *thd, int state_idx, int required_fd_idx,
                               const Ordering &ordering) {
  NFSMEdge edge;
  edge.required_fd_idx = required_fd_idx;
  edge.state_idx = AddArtificialState(thd, ordering);

  if (edge.state_idx == state_idx) {
    // Don't add self-edges; they are already implicit.
    return;
  }

  assert(std::find(m_states[state_idx].outgoing_edges.cbegin(),
                   m_states[state_idx].outgoing_edges.cend(),
                   edge) == m_states[state_idx].outgoing_edges.cend());

  m_states[state_idx].outgoing_edges.push_back(edge);
}

bool LogicalOrderings::FunctionalDependencyApplies(
    const FunctionalDependency &fd, const Ordering &ordering,
    int *start_point) const {
  assert(fd.type != FunctionalDependency::DECAY);
  *start_point = -1;
  for (ItemHandle head_item : fd.head) {
    bool matched = false;
    for (size_t i = 0; i < ordering.size(); ++i) {
      if (ordering.GetElements()[i].item == head_item ||
          (fd.type == FunctionalDependency::EQUIVALENCE &&
           ordering.GetElements()[i].item == fd.tail)) {
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
   Given an order O and a functional dependency FD: S → x where S
   is a subset of O, create new orderings by inserting x into O at
   different positions, and add those to the set of orderings if they
   could become interesting (@see
   LogicalOrderings::CouldBecomeInterestingOrdering(Ordering ordering)).

   This operation is implemented as a class to avoid an excessively
   long parameter list.
*/
class LogicalOrderings::OrderWithElementInserted final {
 public:
  OrderWithElementInserted &SetContext(LogicalOrderings *context) {
    m_context = context;
    return *this;
  }

  OrderWithElementInserted &SetStateIdx(int state_idx) {
    m_state_idx = state_idx;
    return *this;
  }

  OrderWithElementInserted &SetFdIdx(int fd_idx) {
    m_fd_idx = fd_idx;
    return *this;
  }

  OrderWithElementInserted &SetOldOrdering(Ordering old_ordering) {
    m_old_ordering = old_ordering;
    return *this;
  }

  OrderWithElementInserted &SetStartPoint(size_t start_point) {
    m_start_point = start_point;
    return *this;
  }

  OrderWithElementInserted &SetItemToAdd(ItemHandle item_to_add) {
    m_item_to_add = item_to_add;
    return *this;
  }

  OrderWithElementInserted &SetDirection(enum_order direction) {
    m_direction = direction;
    return *this;
  }

  /// Add any potentially interesting orders.
  void AddPotentiallyInterestingOrders(THD *thd);

 private:
  /// The enclosing  LogicalOrderings instance.
  LogicalOrderings *m_context;

  /// The originator state.
  int m_state_idx;

  /// The functional dependency with which we will extend m_old_ordering.
  int m_fd_idx;

  /// The ordering to be extended.
  Ordering m_old_ordering;

  /// The first position at which m_item_to_add. If ordering is needed,
  /// this must be behind the last element of the FD head.
  size_t m_start_point;

  /// The item to add to the ordering.
  ItemHandle m_item_to_add;

  /// The desired direction of the extended ordering.
  enum_order m_direction;
};

void LogicalOrderings::OrderWithElementInserted::
    AddPotentiallyInterestingOrders(THD *thd) {
  assert(m_direction == ORDER_NOT_RELEVANT ||
         m_old_ordering.GetKind() != Ordering::Kind::kGroup);

  if (static_cast<int>(m_old_ordering.size()) >=
      m_context->m_longest_ordering) {
    return;
  }

  for (size_t add_pos = m_start_point; add_pos <= m_old_ordering.size();
       ++add_pos) {
    if (m_direction == ORDER_NOT_RELEVANT) {
      // For groupings, only insert in the sorted sequence.
      // (If we have found the right insertion spot, we immediately
      // exit after this at the end of the loop.)
      if (add_pos < m_old_ordering.size() &&
          m_context->ItemHandleBeforeInGroup(
              m_old_ordering.GetElements()[add_pos].item, m_item_to_add)) {
        continue;
      }

      // For groupings, we just deduplicate right away.
      // TODO(sgunders): When we get C++20, use operator<=> so that we
      // can use a == b here instead of !(a < b) && !(b < a) as we do now.
      if (add_pos < m_old_ordering.size() &&
          !m_context->ItemHandleBeforeInGroup(
              m_item_to_add, m_old_ordering.GetElements()[add_pos].item)) {
        break;
      }
    }

    OrderingElementsGuard tmp_guard(m_context, thd->mem_root);
    Ordering::Elements &tmp = tmp_guard.Get();
    const Ordering::Kind kind =
        m_old_ordering.GetKind() == Ordering::Kind::kEmpty
            ? Ordering::Kind::kOrder
            : m_old_ordering.GetKind();

    std::copy(m_old_ordering.GetElements().cbegin(),
              m_old_ordering.GetElements().cbegin() + add_pos, tmp.begin());
    tmp[add_pos].item = m_item_to_add;
    tmp[add_pos].direction =
        kind == Ordering::Kind::kOrder ? m_direction : ORDER_NOT_RELEVANT;

    std::copy(m_old_ordering.GetElements().cbegin() + add_pos,
              m_old_ordering.GetElements().cend(), tmp.begin() + add_pos + 1);
    Ordering new_ordering{tmp.prefix(m_old_ordering.size() + 1), kind};

    new_ordering.Deduplicate();

    if (m_context->CouldBecomeInterestingOrdering(new_ordering)) {
      // AddEdge() makes a deep copy of new_ordering, so reusing tmp is ok.
      m_context->AddEdge(thd, m_state_idx, m_fd_idx, new_ordering);
    }

    if (m_direction == ORDER_NOT_RELEVANT) {
      break;
    }
  }
}

void LogicalOrderings::BuildNFSM(THD *thd) {
  // Add a state for each producible ordering.
  for (size_t i = 0; i < m_orderings.size(); ++i) {
    NFSMState state;
    state.satisfied_ordering = m_orderings[i].ordering;
    state.satisfied_ordering_idx = i;
    state.outgoing_edges.init(thd->mem_root);
    state.type = m_orderings[i].type == OrderingWithInfo::INTERESTING
                     ? NFSMState::INTERESTING
                     : NFSMState::ARTIFICIAL;
    m_states.push_back(std::move(state));
  }

  // Add an edge from the initial state to each producible ordering/grouping.
  for (size_t i = 1; i < m_orderings.size(); ++i) {
    if (m_orderings[i].ordering.GetKind() == Ordering::Kind::kGroup) {
      // Not directly producible, but we've made an ordering out of it earlier.
      continue;
    }
    NFSMEdge edge;
    edge.required_fd_idx = INT_MIN + i;
    edge.state_idx = i;
    m_states[0].outgoing_edges.push_back(edge);
  }

  // Add edges from functional dependencies, in a breadth-first search
  // (the array of m_states will expand as we go).
  for (size_t state_idx = 0; state_idx < m_states.size(); ++state_idx) {
    // Refuse to apply FDs for nondeterministic orderings other than possibly
    // ordering -> grouping; ie., (a) can _not_ be satisfied by (a, rand()).
    // This is to avoid evaluating such a nondeterministic function unexpectedly
    // early, e.g. in GROUP BY when the user didn't expect it to be used in
    // ORDER BY. (We still allow it on exact matches, though.See also comments
    // on RAND_TABLE_BIT in SortAheadOrdering.)
    const Ordering old_ordering = m_states[state_idx].satisfied_ordering;
    const bool deterministic =
        none_of(old_ordering.GetElements().begin(),
                old_ordering.GetElements().end(), [this](OrderElement element) {
                  return Overlaps(m_items[element.item].item->used_tables(),
                                  RAND_TABLE_BIT);
                });

    // Apply the special decay FD; first to convert it into a grouping or rollup
    // (which we always allow, even for nondeterministic items),
    // then to shorten the ordering.
    switch (old_ordering.GetKind()) {
      case Ordering::Kind::kOrder:
        if (m_rollup) {
          AddRollupFromOrder(thd, state_idx, old_ordering);
        } else {
          // We do not add rollups if the query block does not do a grouping
          // with rollup.
          AddGroupingFromOrder(thd, state_idx, old_ordering);
        }
        break;

      case Ordering::Kind::kRollup:
        assert(m_rollup);
        AddGroupingFromRollup(thd, state_idx, old_ordering);
        break;

      default:
        break;
    }
    if (!deterministic) {
      continue;
    }
    if (old_ordering.GetKind() != Ordering::Kind::kGroup &&
        old_ordering.size() > 1) {
      AddEdge(thd, state_idx, /*required_fd_idx=*/0,
              Ordering(old_ordering.GetElements().without_back(),
                       old_ordering.GetKind()));
    }

    if (m_states.size() >= kMaxNFSMStates) {
      // Stop adding more states. We won't necessarily find the optimal query,
      // but we'll keep all essential information, and not throw away any of the
      // information we have already gathered (unless the DFSM gets too large,
      // too; see ConvertNFSMToDFSM()).
      break;
    }

    for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
      const FunctionalDependency &fd = m_fds[fd_idx];

      int start_point;
      if (!FunctionalDependencyApplies(fd, old_ordering, &start_point)) {
        continue;
      }

      ItemHandle item_to_add = fd.tail;

      // On a = b, try to replace a with b or b with a.
      OrderingElementsGuard tmp_guard(this, thd->mem_root);
      Ordering::Elements &tmp = tmp_guard.Get();
      Ordering base_ordering;

      if (fd.type == FunctionalDependency::EQUIVALENCE) {
        std::copy(old_ordering.GetElements().cbegin(),
                  old_ordering.GetElements().cend(), tmp.begin());

        ItemHandle other_item = fd.head[0];
        if (tmp[start_point].item == item_to_add) {
          // b already existed, so it's a we must add.
          swap(item_to_add, other_item);
        }
        tmp[start_point].item = item_to_add;  // Keep the direction.

        Ordering new_ordering{tmp.prefix(old_ordering.size()),
                              old_ordering.GetKind()};

        new_ordering.Deduplicate();
        if (CouldBecomeInterestingOrdering(new_ordering)) {
          AddEdge(thd, state_idx, fd_idx, new_ordering);
        }

        // Now we can add back the item we just replaced,
        // at any point after this. E.g., if we had an order abc
        // and applied b=d to get adc, we can add back b to get
        // adbc or adcb. Also, we'll fall through afterwards
        // to _not_ replacing but just adding d, e.g. abdc and abcd.
        // So fall through.
        base_ordering = new_ordering;
        item_to_add = other_item;
      } else {
        base_ordering = old_ordering;
      }

      auto extended_order = [&]() {
        return OrderWithElementInserted()
            .SetContext(this)
            .SetStateIdx(state_idx)
            .SetFdIdx(fd_idx)
            .SetOldOrdering(base_ordering)
            .SetItemToAdd(item_to_add);
      };

      // On S -> b, try to add b everywhere after the last element of S.
      switch (base_ordering.GetKind()) {
        case Ordering::Kind::kGroup:
        case Ordering::Kind::kRollup:
          if (m_items[m_items[item_to_add].canonical_item].used_in_grouping) {
            extended_order()
                // For GROUP BY without ROLLUP, any ordering on the
                // grouping terms T1..TN will work, as it ensures that all
                // rows with the same values for those grouping terms will
                // appear consecutively.  But the mechanism for generating
                // the ROLLUP rows also requires the rows to be sorted on
                // T1..TN. Therefore we cannot reorder the terms in
                // 'ordering' according to the GROUP BY sequence if we
                // have ROLLUP.  (See also bug #34670701.)
                .SetStartPoint(base_ordering.GetKind() ==
                                       Ordering::Kind::kRollup
                                   ? start_point + 1
                                   : 0)
                .SetDirection(ORDER_NOT_RELEVANT)
                .AddPotentiallyInterestingOrders(thd);
          }
          break;

        default:
          // NOTE: We could have neither add_asc nor add_desc, if the item is
          // used only in groupings. If so, we don't add it at all, before we
          // convert it to a grouping.
          bool add_asc = m_items[m_items[item_to_add].canonical_item].used_asc;
          bool add_desc =
              m_items[m_items[item_to_add].canonical_item].used_desc;
          if (add_asc) {
            extended_order()
                .SetStartPoint(start_point + 1)
                .SetDirection(ORDER_ASC)
                .AddPotentiallyInterestingOrders(thd);
          }
          if (add_desc) {
            extended_order()
                .SetStartPoint(start_point + 1)
                .SetDirection(ORDER_DESC)
                .AddPotentiallyInterestingOrders(thd);
          }
      }
    }
  }
}

void LogicalOrderings::AddGroupingFromOrder(THD *thd, int state_idx,
                                            const Ordering &ordering) {
  assert(ordering.GetKind() == Ordering::Kind::kOrder);
  OrderingElementsGuard tmp_guard(this, thd->mem_root);
  Ordering::Elements &tmp = tmp_guard.Get();

  std::copy(ordering.GetElements().cbegin(), ordering.GetElements().cend(),
            tmp.begin());

  for (size_t i = 0; i < ordering.size(); ++i) {
    tmp[i].direction = ORDER_NOT_RELEVANT;
    if (!m_items[m_items[tmp[i].item].canonical_item].used_in_grouping) {
      // Pruned away.
      return;
    }
  }

  SortElements(tmp.prefix(ordering.size()));

  AddEdge(thd, state_idx, /*required_fd_idx=*/0,
          Ordering(tmp.prefix(ordering.size()), Ordering::Kind::kGroup));
}

void LogicalOrderings::AddGroupingFromRollup(THD *thd, int state_idx,
                                             const Ordering &ordering) {
  assert(ordering.GetKind() == Ordering::Kind::kRollup);
  assert(std::all_of(
      ordering.GetElements().cbegin(), ordering.GetElements().cend(),
      [this](const OrderElement &elem) {
        // Not pruned away.
        return m_items[m_items[elem.item].canonical_item].used_in_grouping;
      }));

  OrderingElementsGuard tmp_guard(this, thd->mem_root);
  Ordering::Elements &tmp = tmp_guard.Get();
  std::copy(ordering.GetElements().cbegin(), ordering.GetElements().cend(),
            tmp.begin());
  SortElements(tmp.prefix(ordering.size()));

  AddEdge(thd, state_idx, /*required_fd_idx=*/0,
          Ordering(tmp.prefix(ordering.size()), Ordering::Kind::kGroup));
}

void LogicalOrderings::AddRollupFromOrder(THD *thd, int state_idx,
                                          const Ordering &ordering) {
  assert(m_rollup);
  assert(ordering.GetKind() == Ordering::Kind::kOrder);
  OrderingElementsGuard tmp_guard(this, thd->mem_root);
  Ordering::Elements &tmp = tmp_guard.Get();
  std::copy(ordering.GetElements().cbegin(), ordering.GetElements().cend(),
            tmp.begin());

  for (size_t i = 0; i < ordering.size(); ++i) {
    tmp[i].direction = ORDER_NOT_RELEVANT;
    if (!m_items[m_items[tmp[i].item].canonical_item].used_in_grouping) {
      // Pruned away.
      return;
    }
  }

  Ordering rollup =
      Ordering(tmp.prefix(ordering.size()), Ordering::Kind::kRollup);

  AddEdge(thd, state_idx, /*required_fd_idx=*/0, rollup);
}

// Clang vectorizes the inner loop below with -O2, but GCC does not. Enable
// vectorization with GCC too, since this loop is a bottleneck when there are
// many NFSM states.
#if defined(NDEBUG) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("tree-loop-vectorize")
#endif

// Calculates the transitive closure of the reachability graph.
static void FindAllReachable(Bounds_checked_array<bool *> reachable) {
  const int N = reachable.size();
  for (int k = 0; k < N; ++k) {
    for (int i = 0; i < N; ++i) {
      if (reachable[i][k]) {
        for (int j = 0; j < N; ++j) {
          // If there are edges i -> k -> j, add an edge i -> j.
          reachable[i][j] |= reachable[k][j];
        }
      }
    }
  }
}

#if defined(NDEBUG) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

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
  // Create a two-dimensional array with N elements in each dimension. Each line
  // starts at an eight byte word boundary, as that seems to improve the
  // performance of the inner loop in Floyd-Warshall. reachable[i][j] == true
  // means that state j is reachable from state i.
  const size_t N_aligned = ALIGN_SIZE(m_states.size());
  auto reachable = Bounds_checked_array<bool *>::Alloc(thd->mem_root, N);
  auto reachable_buffer =
      Bounds_checked_array<bool>::Alloc(thd->mem_root, N * N_aligned);
  for (int i = 0; i < N; ++i) {
    reachable[i] = reachable_buffer.data() + i * N_aligned;
  }

  // We have multiple pruning techniques, all heuristic in nature.
  // If one removes something, it may help to run the others again,
  // so keep running until we've stabilized.
  bool pruned_anything;
  do {
    pruned_anything = false;
    fill(reachable_buffer.begin(), reachable_buffer.end(), false);

    for (int i = 0; i < N; ++i) {
      if (m_states[i].type == NFSMState::DELETED) {
        continue;
      }

      // There's always an implicit self-edge.
      reachable[i][i] = true;

      for (const NFSMEdge &edge : m_states[i].outgoing_edges) {
        reachable[i][edge.state_idx] = true;
      }
    }

    FindAllReachable(reachable);

    // Now prune away artificial m_states that cannot reach any
    // interesting orders, and m_states that are not reachable from
    // the initial node (the latter can only happen as the result
    // of other prunings).
    for (int i = 1; i < N; ++i) {
      if (m_states[i].type != NFSMState::ARTIFICIAL) {
        continue;
      }

      if (!reachable[0][i]) {
        m_states[i].type = NFSMState::DELETED;
        pruned_anything = true;
        continue;
      }

      bool can_reach_interesting = false;
      for (int j = 1; j < static_cast<int>(m_orderings.size()); ++j) {
        if (reachable[i][j] && m_states[j].type == NFSMState::INTERESTING) {
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
        const int next_state_idx = state.outgoing_edges[j].state_idx;
        bool can_reach_other_interesting = false;
        for (size_t k = 1; k < m_orderings.size(); ++k) {
          if (k != i && m_states[k].type == NFSMState::INTERESTING &&
              reachable[next_state_idx][k]) {
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
      for (const NFSMEdge &edge : state.outgoing_edges) {
        if (edge.state(this)->type != NFSMState::DELETED) {
          state.outgoing_edges[num_kept++] = edge;
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
      if (reachable[i][order_idx]) {
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
    for (const NFSMEdge &edge : state.outgoing_edges) {
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

  // Keep track of which sets of NFSM states we've already seen, and which DFSM
  // state we created for that set.
  mem_root_unordered_set<int, DFSMStateHash<DFSMState>,
                         DFSMStateEqual<DFSMState>>
      constructed_states(thd->mem_root,
                         DFSMStateHash<DFSMState>{&m_dfsm_states},
                         DFSMStateEqual<DFSMState>{&m_dfsm_states});

  // Create the initial DFSM state. It consists of everything in the initial
  // NFSM state, and everything reachable from it with only always-active FDs.
  DFSMState initial;
  initial.nfsm_states.init(thd->mem_root);
  initial.nfsm_states.push_back(0);
  ExpandThroughAlwaysActiveFDs(&initial.nfsm_states, &generation,
                               /*extra_allowed_fd_idx=*/0);
  m_dfsm_states.push_back(std::move(initial));
  constructed_states.insert(0);
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
      assert(m_states[nfsm_state_idx].satisfied_ordering.GetKind() !=
                 Ordering::Kind::kRollup ||
             m_rollup);

      for (const NFSMEdge &edge : m_states[nfsm_state_idx].outgoing_edges) {
        if (!AlwaysActiveFD(edge.required_fd_idx)) {
          nfsm_edges.push_back(edge);
        }
      }
    }

    if (m_dfsm_states.size() >= kMaxDFSMStates) {
      // Stop creating new states, causing us to end fairly soon. Note that
      // since the paths representing explicit sorts are put first, they will
      // never be lost unless kMaxDFSMStates is set extremely low.
      continue;
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

      // Add a new DFSM state for the NFSM states we've collected.
      int target_dfsm_state_idx = m_dfsm_states.size();
      m_dfsm_states.push_back(DFSMState{});
      m_dfsm_states.back().nfsm_states = std::move(nfsm_states);

      // See if there is an existing DFSM state that matches the set of
      // NFSM states we've collected.
      if (auto [place, inserted] =
              constructed_states.insert(target_dfsm_state_idx);
          inserted) {
        // There's none, so create a new one. The type doesn't really matter,
        // except for printing out the graph.
        FinalizeDFSMState(thd, target_dfsm_state_idx);
      } else {
        // Already had a DFSM state for this set of NFSM states. Remove the
        // newly added duplicate and use the original one.
        target_dfsm_state_idx = *place;
        // Allow reuse of the memory in the next iteration.
        nfsm_states = std::move(m_dfsm_states.back().nfsm_states);
        m_dfsm_states.pop_back();
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

string LogicalOrderings::PrintOrdering(const Ordering &ordering) const {
  const bool is_grouping = ordering.GetKind() == Ordering::Kind::kGroup;
  string ret = is_grouping ? "{" : "(";
  if (ordering.GetKind() == Ordering::Kind::kRollup) {
    ret += "rollup: ";
  }
  for (size_t i = 0; i < ordering.size(); ++i) {
    if (i != 0) ret += ", ";
    ret += ItemToString(m_items[ordering.GetElements()[i].item].item);

    if (ordering.GetElements()[i].direction == ORDER_ASC) {
      ret += " ASC";
    } else if (ordering.GetElements()[i].direction == ORDER_DESC) {
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

void LogicalOrderings::PrintFunctionalDependencies(ostream *trace) {
  if (m_fds.size() <= 1) {
    *trace << "\nNo functional dependencies (after pruning).\n\n";
  } else {
    *trace << "\nFunctional dependencies (after pruning):\n";
    for (size_t fd_idx = 1; fd_idx < m_fds.size(); ++fd_idx) {
      *trace << " - " +
                    PrintFunctionalDependency(m_fds[fd_idx], /*html=*/false);
      if (m_fds[fd_idx].always_active) {
        *trace << " [always active]";
      }
      *trace << "\n";
    }
    *trace << "\n";
  }
}

void LogicalOrderings::PrintInterestingOrders(ostream *trace) {
  *trace << "Interesting orders:\n";
  for (size_t order_idx = 0; order_idx < m_orderings.size(); ++order_idx) {
    const OrderingWithInfo &ordering = m_orderings[order_idx];
    if (order_idx == 0 && ordering.type == OrderingWithInfo::UNINTERESTING) {
      continue;
    }

    *trace << StringPrintf(" - %zu: ", order_idx);
    bool first = true;
    switch (ordering.ordering.GetKind()) {
      case Ordering::Kind::kRollup:
        *trace << "rollup ";
        break;

      case Ordering::Kind::kGroup:
        *trace << "group ";
        break;

      default:
        break;
    }
    for (OrderElement element : ordering.ordering.GetElements()) {
      if (!first) {
        *trace << ", ";
      }
      first = false;
      *trace << ItemToString(m_items[element.item].item);
      if (element.direction == ORDER_ASC) {
        *trace << " ASC";
      } else if (element.direction == ORDER_DESC) {
        *trace << " DESC";
      }
    }
    if (ordering.ordering.GetElements().empty()) {
      *trace << "()";
    }
    if (ordering.type == OrderingWithInfo::HOMOGENIZED) {
      *trace << " [homogenized from other ordering]";
    } else if (ordering.type == OrderingWithInfo::UNINTERESTING) {
      *trace << " [support order]";
    }
    *trace << "\n";
  }
  *trace << "\n";
}

void LogicalOrderings::PrintNFSMDottyGraph(ostream *trace) const {
  *trace << "digraph G {\n";
  for (size_t state_idx = 0; state_idx < m_states.size(); ++state_idx) {
    const NFSMState &state = m_states[state_idx];
    if (state.type == NFSMState::DELETED) {
      continue;
    }

    // We're printing the NFSM.
    *trace << StringPrintf("  s%zu [label=\"%s\"", state_idx,
                           PrintOrdering(state.satisfied_ordering).c_str());
    if (state.type == NFSMState::INTERESTING) {
      *trace << ", peripheries=2";
    }
    *trace << "]\n";

    for (const NFSMEdge &edge : state.outgoing_edges) {
      if (edge.required_fd_idx < 0) {
        // Pseudo-edge without a FD (from initial state only).
        *trace << StringPrintf("  s%zu -> s%d [label=\"ordering %d\"]\n",
                               state_idx, edge.state_idx,
                               edge.required_fd_idx - INT_MIN);
      } else {
        const FunctionalDependency *fd = edge.required_fd(this);
        *trace << StringPrintf(
            "  s%zu -> s%d [label=\"%s\"]\n", state_idx, edge.state_idx,
            PrintFunctionalDependency(*fd, /*html=*/true).c_str());
      }
    }
  }

  *trace << "}\n";
}

void LogicalOrderings::PrintDFSMDottyGraph(ostream *trace) const {
  *trace << "digraph G {\n";
  for (size_t state_idx = 0; state_idx < m_dfsm_states.size(); ++state_idx) {
    const DFSMState &state = m_dfsm_states[state_idx];
    *trace << StringPrintf("  s%zu [label=< ", state_idx);

    bool any_interesting = false;
    for (size_t i = 0; i < state.nfsm_states.size(); ++i) {
      const NFSMState &nsfm_state = m_states[state.nfsm_states[i]];
      if (i != 0) {
        *trace << ", ";
      }
      if (nsfm_state.type == NFSMState::INTERESTING) {
        any_interesting = true;
        *trace << "<b>";
      }
      *trace << PrintOrdering(nsfm_state.satisfied_ordering);
      if (nsfm_state.type == NFSMState::INTERESTING) {
        *trace << "</b>";
      }
    }
    *trace << " >";
    if (any_interesting) {
      *trace << ", peripheries=2";
    }
    *trace << "]\n";

    for (size_t edge_idx : state.outgoing_edges) {
      const DFSMEdge &edge = m_dfsm_edges[edge_idx];
      if (edge.required_fd_idx < 0) {
        // Pseudo-edge without a FD (from initial state only).
        *trace << StringPrintf("  s%zu -> s%d [label=\"ordering %d\"]\n",
                               state_idx, edge.state_idx,
                               edge.required_fd_idx - INT_MIN);
      } else {
        const FunctionalDependency *fd = edge.required_fd(this);
        *trace << StringPrintf(
            "  s%zu -> s%d [label=\"%s\"]\n", state_idx, edge.state_idx,
            PrintFunctionalDependency(*fd, /*html=*/true).c_str());
      }
    }
  }

  *trace << "}\n";
}
