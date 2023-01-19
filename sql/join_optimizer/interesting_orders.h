/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_H
#define SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_H

/**
  @file

  Tracks which tuple streams follow which orders, and in particular whether
  they follow interesting orders.

  An interesting order (and/or grouping) is one that we might need to sort by
  at some point during query execution (e.g. to satisfy an ORDER BY predicate);
  if the rows already are produced in that order, for instance because we
  scanned along the right index, we can skip the sort and get a lower cost.

  We generally follow these papers:

    [Neu04] Neumann and Moerkotte: “An efficient framework for order
      optimization”
    [Neu04b] Neumann and Moerkotte: “A Combined Framework for
      Grouping and Order Optimization”

  [Neu04b] is an updated version of [Neu04] that also deals with interesting
  groupings but omits some details to make more space, so both are needed.
  A combined and updated version of the same material is available in
  Moerkotte's “Query compilers” PDF.

  Some further details, like order homogenization, come from

    [Sim96] Simmen et al: “Fundamental Techniques for Order Optimization”

  All three papers deal with the issue of _logical_ orderings, where any
  row stream may follow more than one order simultaneously, as inferred
  through functional dependencies (FDs). For instance, if we have an ordering
  (ab) but also an active FD {a} → c (c is uniquely determined by a,
  for instance because a is a primary key in the same table as c), this means
  we also implicitly follow the orders (acb) and (abc). In addition,
  we trivially follow the orders (a), (ac) and (ab). However, note that we
  do _not_ necessarily follow the order (cab).

  Similarly, equivalences, such as WHERE conditions and joins, give rise
  to a stronger form of FDs. If we have an ordering (ab) and the FD b = c,
  we can be said to follow (ac), (acb) or (abc). The former would not be
  inferable from {b} → c and {c} → b alone. Equivalences with constants
  are perhaps even stronger, e.g. WHERE x=3 would give rise to {} → x,
  which could extend (a) to (xa), (ax) or (x).

  Neumann et al solve this by modelling which ordering we're following as a
  state in a non-deterministic finite state machine (NFSM). By repeatedly
  applying FDs (which become edges in the NFSM), we can build up all possible
  orderings from a base (which can be either the empty ordering, ordering from
  scanning along an index, or one produced by an explicit sort) and then
  checking whether we are in a state matching the ordering we are interested
  in. (There can be quite a bit of states, so we need a fair amount of pruning
  to keep the count manageable, or else performance will suffer.) Of course,
  since NFSMs are nondeterministic, a base ordering and a set of FDs can
  necessarily put us in a number of states, so we need to convert the NFSM
  to a DFSM (using the standard powerset construction for NFAs; see
  ConvertNFSMToDFSM()). This means that the ordering state for an access path
  is only a single integer, the DFSM state number. When we activate more FDs,
  for instance because we apply joins, we will move throughout the DFSM into
  more attractive states. By checking simple precomputed lookup tables,
  we can quickly find whether a given DFSM state follows a given ordering.

  The other kind of edges we follow are from the artificial starting state;
  they represent setting a specific ordering (e.g. because we sort by that
  ordering). This is set up in the NFSM and preserved in the DFSM.

  The actual collection of FDs and interesting orders happen outside this
  class, in the caller.

  A weakness in the approach is that transitive FDs are not always followed
  correctly. E.g., if we have an ordering (a), and FDs {a} → b and {b} → c,
  we will create (ab) and (abc), but _not_ (ac). This is not a problem for
  equivalences, though, and most of the FDs we collect are equivalences.
  We do have some heuristics to produce a new FD {a} → c where it is relevant,
  but they are not always effective.

  Neumann and Moerkotte distinguish between “tested-for” (O_T) and
  “producing” (O_P) orderings, where all orders are interesting but only
  some can be produced by explicit operators, such as sorts. Our implementation
  is exactly opposite; we allow every ordering to be produced (by means of
  sort-ahead), but there are orders that can be produced (e.g. when scanning
  an index) that are not interesting in themselves. Such orders can be
  pruned away early if we can show they do not produce anything interesting.


  The operations related to interesting orders, in particular the concept
  of functional dependencies, are related to the ones we are doing when
  checking ONLY_FULL_GROUP_BY legality in sql/aggregate_check.h. However,
  there are some key differences as well:

   - Orderings are lexical, while groupings are just a bag of attributes.
     This increases the state space for orderings significantly; groupings
     can add elements at will and just grow the set freely, while orderings
     need more care. In particular, this means that groupings only need
     FDs on the form S → x (where S is a set), while orderings also benefit
     from those of the type x = y, which replace an element instead of
     adding a new one.

   - ONLY_FULL_GROUP_BY is for correctness of rejecting or accepting the
     query, while interesting orders is just an optimization, so not
     recognizing rare cases is more acceptable.

   - ONLY_FULL_GROUP_BY testing only cares about the set of FDs that hold
     at one specific point (GROUP BY testing, naturally), while interesting
     orders must be tracked throughout the entire operator tree. In particular,
     if using interesting orders for merge join, the status at nearly every
     join is relevant. Also, performance matters much more.

  Together, these mean that the code ends up being fairly different,
  and some cases are recognized by ONLY_FULL_GROUP_BY but not by interesting
  orders. (The actual FD collection happens in BuildInterestingOrders in
  join_optimizer.cc; see the comment there for FD differences.)

  A note about nomenclature: Like Neumann et al, we use the term “ordering”
  (and “grouping”) instead of “order”, with the special exception of the
  already-established term “interesting order”.
 */

#include "my_table_map.h"
#include "sql/join_optimizer/interesting_orders_defs.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/sql_array.h"

#include <bitset>
#include <string>

class LogicalOrderings;
class Window;

/**
   Represents a (potentially interesting) ordering, rollup or (non-rollup)
   grouping.
*/
class Ordering final {
  friend bool operator==(const Ordering &a, const Ordering &b);

 public:
  /// This type hold the individual elements of the ordering.
  using Elements = Bounds_checked_array<OrderElement>;

  /// The kind of ordering that an Ordering instance may represent.
  enum class Kind : char {
    /// An ordering with no elements. Such an ordering is not useful in itself,
    /// but may appear as an intermediate result.
    kEmpty,

    /// Specific sequence of m_elements, and specific direction of each element.
    /// Needed for e.g. ORDER BY.
    kOrder,

    ///  Specific sequence of m_elements, but each element may be ordered in any
    /// direction. Needed for ROLLUP:
    kRollup,

    /// Elements may appear in any sequence and may be ordered in any direction.
    /// Needed for GROUP BY (with out ROLLUP), DISCTINCT, semi-join etc.
    kGroup
  };

  Ordering() : m_kind{Kind::kEmpty} {}

  Ordering(Elements elements, Kind kind) : m_elements{elements}, m_kind{kind} {
    assert(Valid());
  }

  /// Copy constructor. Only defined explicitly to check Valid().
  Ordering(const Ordering &other)
      : m_elements{other.m_elements}, m_kind{other.m_kind} {
    assert(Valid());
  }

  /// Assignment operator. Only defined explicitly to check Valid().
  Ordering &operator=(const Ordering &other) {
    assert(Valid());
    m_kind = other.m_kind;
    m_elements = other.m_elements;
    return *this;
  }

  /// Make a copy of *this. Allocate new memory for m_elements from mem_root.
  Ordering Clone(MEM_ROOT *mem_root) const {
    assert(Valid());
    return Ordering(m_elements.Clone(mem_root), GetKind());
  }

  Kind GetKind() const {
    assert(Valid());
    return m_kind;
  }

  const Elements &GetElements() const {
    assert(Valid());
    return m_elements;
  }

  Elements &GetElements() {
    assert(Valid());
    return m_elements;
  }

  size_t size() const { return m_elements.size(); }

  /**
     Remove duplicate entries, in-place.
  */
  void Deduplicate();

 private:
  /// The ordering terms.
  Elements m_elements;

  /// The kind of this ordering.
  Kind m_kind;

  /// @returns true iff *this passes a consistency check.
  bool Valid() const;
};

/// Check if 'a' and 'b' has the same kind and contains the same elements.
inline bool operator==(const Ordering &a, const Ordering &b) {
  assert(a.Valid());
  assert(b.Valid());
  return a.m_kind == b.m_kind &&
         std::equal(a.m_elements.cbegin(), a.m_elements.cend(),
                    b.m_elements.cbegin(), b.m_elements.cend());
}

inline bool operator!=(const Ordering &a, const Ordering &b) {
  return !(a == b);
}

struct FunctionalDependency {
  enum {
    // A special “empty” kind of edge in the FSM that signifies
    // adding no functional dependency, ie., a state we can reach
    // with no further effort. This can happen in two ways:
    //
    //  1. An ordering can drop its last element, ie.,
    //     if a tuple stream is ordered on (a,b,c), it is also
    //     ordered on (a,b).
    //  2. An ordering can be converted to a grouping, i.e,
    //     if a tuple stream is ordered on (a,b,c), it is also
    //     grouped on {a,b,c}.
    //
    // head must be empty, tail must be 0. Often called ϵ.
    // Must be the first in the edge list.
    DECAY,

    // A standard functional dependency {a} → b; if a row tuple
    // is ordered on all elements of a and this FD is applied,
    // it is also ordered on b. A typical example is if {a}
    // is an unique key in a table, and b is a column of the
    // same table. head can be empty.
    FD,

    // An equivalence a = b; implies a → b and b → a, but is
    // stronger (e.g. if ordered on (a,c), there is also an
    // ordering on (b,c), which wouldn't be true just from
    // applying FDs individually). head must be a single element.
    EQUIVALENCE
  } type;

  Bounds_checked_array<ItemHandle> head;
  ItemHandle tail;

  // Whether this functional dependency can always be applied, ie.,
  // there is never a point during query processing where it does not hold.
  //
  // Examples of not-always-active FDs include join conditions;
  // e.g. for t1.x = t2.x, it is not true before the join has actually
  // happened (and t1.x won't be the same order as t2.x before that,
  // and thus cannot be used in e.g. a merge join).
  //
  // However, FDs that stem from unique indexes are always true; e.g. if
  // t1.x is a primary key, {t1.x} → t1.y will always be true, and we can
  // always reduce t1.y from an order if t1.x is present earlier.
  // Similarly, WHERE conditions that are applied on the base table
  // (ie., it is not delayed due to outer joins) will always be true,
  // if t1.x = 3, we can safely assume {} → t1.x holds even before
  // joining in t1, so a sort on (t1.x, t2.y) can be satisfied just by
  // sorting t2 on y.
  //
  // Always-active FDs are baked into the DFSM, so that we need to follow
  // fewer arcs during query processing. They can also be used for reducing
  // the final order (to get more efficient sorting), but we don't do it yet.
  bool always_active = false;
};

class LogicalOrderings {
  friend class OrderingElementsGuard;

 public:
  explicit LogicalOrderings(THD *thd);

  // Maps the Item to an opaque integer handle. Deduplicates items as we go,
  // inserting new ones if needed.
  ItemHandle GetHandle(Item *item);

  Item *item(ItemHandle item) const { return m_items[item].item; }

  // These are only available before Build() has been called.

  // Mark an interesting ordering (or grouping) as interesting,
  // returning an index that can be given to SetOrder() later.
  // Will deduplicate against previous entries; if not deduplicated
  // away, a copy will be taken.
  //
  // Uninteresting orderings are those that can be produced by some
  // operator (for instance, index scan) but are not interesting to
  // test for. Orderings may be merged, pruned (if uninteresting)
  // and moved around after Build(); see RemapOrderingIndex().
  //
  // If used_at_end is true, the ordering is assumed to be used only
  // after all joins have happened, so all FDs are assumed to be
  // active. This enables reducing the ordering more (which can in
  // some cases help with better sortahead or the likes), but is not
  // correct if the ordering wants to be used earlier on, e.g.
  // in merge join or for semijoin duplicate removal. If it is false,
  // then it is also only attempted homogenized onto the given set
  // of tables (otherwise, it is ignored, and homogenization is over
  // all tables).
  //
  // The empty ordering/grouping is always index 0.
  int AddOrdering(THD *thd, Ordering order, bool interesting, bool used_at_end,
                  table_map homogenize_tables) {
    return AddOrderingInternal(thd, order,
                               interesting ? OrderingWithInfo::INTERESTING
                                           : OrderingWithInfo::UNINTERESTING,
                               used_at_end, homogenize_tables);
  }

  // NOTE: Will include the empty ordering.
  int num_orderings() const { return m_orderings.size(); }

  const Ordering &ordering(int ordering_idx) const {
    return m_orderings[ordering_idx].ordering;
  }

  bool ordering_is_relevant_for_sortahead(int ordering_idx) const {
    return !m_orderings[ordering_idx].ordering.GetElements().empty() &&
           m_orderings[ordering_idx].type != OrderingWithInfo::UNINTERESTING;
  }

  // Add a functional dependency that may be applied at some point
  // during the query planning. Same guarantees as AddOrdering().
  // The special “decay” FD is always index 0.
  int AddFunctionalDependency(THD *thd, FunctionalDependency fd);

  // NOTE: Will include the decay (epsilon) FD.
  int num_fds() const { return m_fds.size(); }

  // Set the list of GROUP BY expressions, if any. This is used as the
  // head of the functional dependencies for all aggregate functions
  // (which by definition are functionally dependent on the GROUP BY
  // expressions, unless ROLLUP is active -- see below), and must be
  // valid (ie., not freed or modified) until Build() has run.
  //
  // If none is set, and there are aggregates present in orderings,
  // implicit aggregation is assumed (ie., all aggregate functions
  // are constant).
  void SetHeadForAggregates(Bounds_checked_array<ItemHandle> head) {
    m_aggregate_head = head;
  }

  // Set whether ROLLUP is active; if so, we can no longer assume that
  // aggregate functions are functionally dependent on (nullable)
  // GROUP BY expressions, as two NULLs may be for different reasons.
  void SetRollup(bool rollup) { m_rollup = rollup; }

  // Builds the actual FSMs; all information about orderings and FDs is locked,
  // optimized and then the state machine is built. After this, you can no
  // longer add new orderings or FDs, ie., you are moving into the actual
  // planning phase.
  //
  // Build() may prune away orderings and FDs, and it may also add homogenized
  // orderings, ie., orderings derived from given interesting orders but
  // modified so that they only require a single table (but will become an
  // actual interesting order later, after the FDs have been applied). These are
  // usually at the end, but may also be deduplicated against uninteresting
  // orders, which will then be marked as interesting.
  //
  // trace can be nullptr; if not, it get human-readable optimizer trace
  // appended to it.
  void Build(THD *thd, std::string *trace);

  // These are only available after Build() has been called.
  // They are stateless and used in the actual planning phase.

  // Converts an index returned by AddOrdering() to one that can be given
  // to SetOrder() or DoesFollowOrder(). They don't convert themselves
  // since it's completely legitimate to iterate over all orderings using
  // num_orderings() and orderings(), and those indexes should _not_ be
  // remapped.
  //
  // If an ordering has been pruned away, will return zero (the empty ordering),
  // which is a valid input to SetOrder().
  int RemapOrderingIndex(int ordering_idx) const {
    assert(m_built);
    return m_optimized_ordering_mapping[ordering_idx];
  }

  using StateIndex = int;

  StateIndex SetOrder(int ordering_idx) const {
    assert(m_built);
    return m_orderings[ordering_idx].state_idx;
  }

  // Get a bitmap representing the given functional dependency. The bitmap
  // can be all-zero if the given FD is optimized away, or outside the range
  // of the representable bits. The bitmaps may be ORed together, but are
  // otherwise to be treated as opaque to the client.
  FunctionalDependencySet GetFDSet(int fd_idx) const {
    FunctionalDependencySet fd_set;
    int new_fd_idx = m_optimized_fd_mapping[fd_idx];
    if (new_fd_idx >= 1 && new_fd_idx <= kMaxSupportedFDs) {
      fd_set.set(new_fd_idx - 1);
    }
    return fd_set;
  }

  // For a given state, see what other (better) state we can move to given a
  // set of active functional dependencies, e.g. if we are in state ((),a) and
  // the FD a=b becomes active, we can set its bit (see GetFDSet()) in the FD
  // mask and use that to move to the state ((),a,b,ab,ba). Note that “fds”
  // should contain the entire set of active FDs, not just newly-applied ones.
  // This is because “old” FDs can suddenly become relevant when new logical
  // orderings are possible, and the DFSM is not always able to bake this in.
  StateIndex ApplyFDs(StateIndex state_idx, FunctionalDependencySet fds) const;

  bool DoesFollowOrder(StateIndex state_idx, int ordering_idx) const {
    assert(m_built);
    if (ordering_idx == 0) {
      return true;
    }
    if (ordering_idx >= kMaxSupportedOrderings) {
      return false;
    }
    return m_dfsm_states[state_idx].follows_interesting_order.test(
        ordering_idx);
  }

  // Whether "a" follows any interesting orders than "b" does not, or could
  // do so in the future. If !MoreOrderedThan(a, b) && !MoreOrderedThan(b, a)
  // the states are equal (they follow the same interesting orders, and could
  // lead to the same interesting orders given the same FDs -- see below).
  // It is possible to have MoreOrderedThan(a, b) && MoreOrderedThan(b, a), e.g.
  // if they simply follow disjunct orders.
  //
  // This is used in the planner, when pruning access paths -- an AP A can be
  // kept even if it has higher cost than AP B, if it follows orders that B does
  // not. Why is it enough to check interesting orders -- must we also not check
  // uninteresting orders, since they could lead to new interesting orders
  // later? This is because in the planner, two states will only ever be
  // compared if the same functional dependencies have been applied to both
  // sides:
  //
  // The set of logical orders, and thus the state, is uniquely determined
  // by the initial ordering and applied FDs. Thus, if A has _uninteresting_
  // orders that B does not, the initial ordering must have differed -- but the
  // initial states only contain (and thus differ in) interesting orders.
  // Thus, the additional uninteresting orders must have been caused by
  // additional interesting orders (that do not go away), so testing the
  // interesting ones really suffices in planner context.
  //
  // Note that this also means that in planner context, !MoreOrderedThan(a, b)
  // && !MoreOrderedThan(b, a) implies that a == b.
  bool MoreOrderedThan(
      StateIndex a_idx, StateIndex b_idx,
      std::bitset<kMaxSupportedOrderings> ignored_orderings) const {
    assert(m_built);
    std::bitset<kMaxSupportedOrderings> a =
        m_dfsm_states[a_idx].follows_interesting_order & ~ignored_orderings;
    std::bitset<kMaxSupportedOrderings> b =
        m_dfsm_states[b_idx].follows_interesting_order & ~ignored_orderings;
    std::bitset<kMaxSupportedOrderings> future_a =
        m_dfsm_states[a_idx].can_reach_interesting_order & ~ignored_orderings;
    std::bitset<kMaxSupportedOrderings> future_b =
        m_dfsm_states[b_idx].can_reach_interesting_order & ~ignored_orderings;
    return (a & b) != a || (future_a & future_b) != future_a;
  }

  // See comment in .cc file.
  Ordering ReduceOrdering(Ordering ordering, bool all_fds,
                          Ordering::Elements tmp) const;

 private:
  struct NFSMState;
  class OrderWithElementInserted;

  bool m_built = false;

  struct ItemInfo {
    // Used to translate Item * to ItemHandle and back.
    Item *item;

    // Points to the head of this item's equivalence class. (If the item
    // is not equivalent to anything, points to itself.) The equivalence class
    // is defined by EQUIVALENCE FDs, transitively, and the head is the one with
    // the lowest index. So if we have FDs a = b and b = c, all three {a,b,c}
    // will point to a here. This is useful for pruning and homogenization;
    // if two elements have the same equivalence class (ie., the same canonical
    // item), they could become equivalent after applying FDs. See also
    // m_can_be_added_by_fd, which deals with non-EQUIVALENCE FDs.
    //
    // Set by BuildEquivalenceClasses().
    ItemHandle canonical_item;

    // Whether the given item (after canonicalization by means of
    // m_canonical_item[]) shows up as the tail of any functional dependency.
    //
    // Set by FindElementsThatCanBeAddedByFDs();
    bool can_be_added_by_fd = false;

    // Whether the given item ever shows up in orderings as ASC or DESC,
    // respectively. Used to see whether adding the item in that direction
    // is worthwhile or not. Note that this is propagated through equivalences,
    // so if a = b and any ordering contains b DESC and a is the head of that
    // equivalence class, then a is also marked as used_desc = true.
    bool used_asc = false;
    bool used_desc = false;
    bool used_in_grouping = false;
  };
  // All items we have seen in use (in orderings or FDs), deduplicated
  // and indexed by ItemHandle.
  Mem_root_array<ItemInfo> m_items;

  // Head for all FDs generated for aggregate functions.
  // See SetHeadForAggregates().
  Bounds_checked_array<ItemHandle> m_aggregate_head;

  // Whether rollup is active; if so, we need to take care not to create
  // FDs for aggregates in some cases. See SetHeadForAggregates() and
  // SetRollup().
  bool m_rollup = false;

  struct NFSMEdge {
    // Which FD is required to follow this edge. Index into m_fd, with one
    // exception; from the initial state (0), we have constructor edges for
    // setting a specific order without following an FD. Such edges have
    // required_fd_idx = INT_MIN + order_idx, ie., they are negative.
    int required_fd_idx;

    // Destination state (index into m_states).
    int state_idx;

    const FunctionalDependency *required_fd(
        const LogicalOrderings *orderings) const {
      return &orderings->m_fds[required_fd_idx];
    }
    const NFSMState *state(const LogicalOrderings *orderings) const {
      return &orderings->m_states[state_idx];
    }
  };

  friend bool operator==(const NFSMEdge &a, const NFSMEdge &b);
  friend bool operator!=(const NFSMEdge &a, const NFSMEdge &b);

  struct NFSMState {
    enum { INTERESTING, ARTIFICIAL, DELETED } type;
    Mem_root_array<NFSMEdge> outgoing_edges;
    Ordering satisfied_ordering;
    int satisfied_ordering_idx;  // Only for type == INTERESTING.

    // Indexed by ordering.
    std::bitset<kMaxSupportedOrderings> can_reach_interesting_order{0};

    // Used during traversal, to keep track of which states we have
    // already seen (for fast deduplication). We use the standard trick
    // of using a generational counter instead of a bool, so that we don't
    // have to clear it every time; we can just increase the generation
    // and treat everything with lower/different “seen” as unseen.
    int seen = 0;
  };
  struct DFSMState {
    Mem_root_array<int> outgoing_edges;  // Index into dfsm_edges.
    Mem_root_array<int> nfsm_states;     // Index into states.

    // Structures derived from the above, but in forms for faster access.

    // Indexed by FD.
    Bounds_checked_array<int> next_state;

    // Indexed by ordering.
    std::bitset<kMaxSupportedOrderings> follows_interesting_order{0};

    // Interesting orders that this state can eventually reach,
    // given that all FDs are applied (a superset of follows_interesting_order).
    // We track this instead of the producing orders (e.g. which homogenized
    // order are we following), because it allows for more access paths to
    // compare equal. See also OrderingWithInfo::Type::HOMOGENIZED.
    std::bitset<kMaxSupportedOrderings> can_reach_interesting_order{0};

    // Whether applying the given functional dependency will take us to a
    // different state from this one. Used to quickly intersect with the
    // available FDs to find out what we can apply.
    FunctionalDependencySet can_use_fd{0};
  };

  struct DFSMEdge {
    int required_fd_idx;
    int state_idx;

    const FunctionalDependency *required_fd(
        const LogicalOrderings *orderings) const {
      return &orderings->m_fds[required_fd_idx];
    }
    const DFSMState *state(const LogicalOrderings *orderings) const {
      return &orderings->m_dfsm_states[state_idx];
    }
  };

  struct OrderingWithInfo {
    Ordering ordering;

    // Status of the ordering. Note that types with higher indexes dominate
    // lower, ie., two orderings can be collapsed into the one with the higher
    // type index if they are otherwise equal.
    enum Type {
      // An ordering that is interesting in its own right,
      // e.g. because it is given to ORDER BY.
      INTERESTING = 2,

      // An ordering that is derived from an interesting order, but refers to
      // one table only (or conceptually, a subset of tables -- but we don't
      // support that in this implementation). Guaranteed to reach some
      // interesting order at some point, but we don't track it as interesting
      // in the FSM states. This means that these orderings don't get state bits
      // in follows_interesting_order for themselves, but they will always have
      // one or more interesting orders in can_reach_interesting_order.
      // This helps us collapse access paths more efficiently; if we have
      // an interesting order t3.x and create homogenized orderings t1.x
      // and t2.x (due to some equality with t3.x), an access path following one
      // isn't better than an access path following the other. They will lead
      // to the same given the same FDs anyway (see MoreOrderedThan()), and
      // thus are equally good.
      HOMOGENIZED = 1,

      // An ordering that is just added because it is easy to produce;
      // e.g. because it is produced by scanning along an index. Such orderings
      // can be shortened or pruned away entirely (in
      // PruneUninterestingOrders())
      // unless we find that they may lead to an interesting order.
      UNINTERESTING = 0
    } type;

    bool used_at_end;

    // Only used if used_at_end = false (see AddOrdering()).
    table_map homogenize_tables = 0;

    // Which initial state to use for this ordering (in SetOrder()).
    StateIndex state_idx = 0;
  };

  Mem_root_array<OrderingWithInfo> m_orderings;

  // The longest ordering in m_orderings.
  int m_longest_ordering = 0;

  Mem_root_array<FunctionalDependency> m_fds;

  // NFSM. 0 is the initial state, all others are found by following edges.
  Mem_root_array<NFSMState> m_states;

  // DFSM. 0 is the initial state, all others are found by following edges.
  Mem_root_array<DFSMState> m_dfsm_states;
  Mem_root_array<DFSMEdge> m_dfsm_edges;

  // After PruneUninterestingOrders has run, maps from the old indexes to the
  // new indexes.
  Bounds_checked_array<int> m_optimized_ordering_mapping;

  // After PruneFDs() has run, maps from the old indexes to the new indexes.
  Bounds_checked_array<int> m_optimized_fd_mapping;

  /// We may potentially use a lot of Ordering::Elements objects, with short and
  /// non-overlapping life times. Therefore we have a pool
  /// to allow reuse and avoid allocating from MEM_ROOT each time.
  Mem_root_array<OrderElement *> m_elements_pool;

  // The canonical order for two items in a grouping
  // (after BuildEquivalenceClasses() has run; enforced by
  // RecanonicalizeGroupings()). The reason why we sort by
  // canonical_item first is so that switching out one element
  // with an equivalent one (ie., applying an EQUIVALENCE
  // functional dependency) does not change the order of the
  // elements in the grouing, which could give false negatives
  // in CouldBecomeInterestingOrdering().
  inline bool ItemHandleBeforeInGroup(ItemHandle a, ItemHandle b) const {
    if (m_items[a].canonical_item != m_items[b].canonical_item)
      return m_items[a].canonical_item < m_items[b].canonical_item;
    return a < b;
  }

  inline bool ItemBeforeInGroup(const OrderElement &a,
                                const OrderElement &b) const {
    return ItemHandleBeforeInGroup(a.item, b.item);
  }

  // Helper for AddOrdering().
  int AddOrderingInternal(THD *thd, Ordering order, OrderingWithInfo::Type type,
                          bool used_at_end, table_map homogenize_tables);

  // See comment in .cc file.
  void PruneUninterestingOrders(THD *thd);

  // See comment in .cc file.
  void PruneFDs(THD *thd);

  // See comment in .cc file.
  bool ImpliedByEarlierElements(ItemHandle item, Ordering::Elements prefix,
                                bool all_fds) const;

  // Populates ItemInfo::canonical_item.
  void BuildEquivalenceClasses();

  // See comment in .cc file.
  void RecanonicalizeGroupings();

  // See comment in .cc file.
  void AddFDsFromComputedItems(THD *thd);

  // See comment in .cc file.
  void AddFDsFromAggregateItems(THD *thd);

  // See comment in .cc file.
  Bounds_checked_array<ItemHandle> CollectHeadForStaticWindowFunction(
      THD *thd, ItemHandle argument_item, Window *window);

  // See comment in .cc file.
  void AddFDsFromConstItems(THD *thd);

  // Populates ItemInfo::can_be_added_by_fd.
  void FindElementsThatCanBeAddedByFDs();

  void PreReduceOrderings(THD *thd);
  void CreateOrderingsFromGroupings(THD *thd);
  void CreateOrderingsFromRollups(THD *thd);
  void CreateHomogenizedOrderings(THD *thd);
  void AddHomogenizedOrderingIfPossible(
      THD *thd, const Ordering &reduced_ordering, bool used_at_end,
      int table_idx,
      Bounds_checked_array<std::pair<ItemHandle, ItemHandle>>
          reverse_canonical);

  /// Sort the elements so that a will appear before b if
  /// ItemBeforeInGroup(a,b)==true.
  void SortElements(Ordering::Elements elements) const;

  // See comment in .cc file.
  bool CouldBecomeInterestingOrdering(const Ordering &ordering) const;

  void BuildNFSM(THD *thd);
  void AddRollupFromOrder(THD *thd, int state_idx, const Ordering &ordering);
  void AddGroupingFromOrder(THD *thd, int state_idx, const Ordering &ordering);
  void AddGroupingFromRollup(THD *thd, int state_idx, const Ordering &ordering);
  void TryAddingOrderWithElementInserted(THD *thd, int state_idx, int fd_idx,
                                         Ordering old_ordering,
                                         size_t start_point,
                                         ItemHandle item_to_add,
                                         enum_order direction);
  void PruneNFSM(THD *thd);
  bool AlwaysActiveFD(int fd_idx);
  void FinalizeDFSMState(THD *thd, int state_idx);
  void ExpandThroughAlwaysActiveFDs(Mem_root_array<int> *nfsm_states,
                                    int *generation, int extra_allowed_fd_idx);
  void ConvertNFSMToDFSM(THD *thd);

  // Populates state_idx for every ordering in m_ordering.
  void FindInitialStatesForOrdering();

  // If a state with the given ordering already exists (artificial or not),
  // returns its index. Otherwise, adds an artificial state with the given
  // order and returns its index.
  int AddArtificialState(THD *thd, const Ordering &ordering);

  // Add an edge from state_idx to an state with the given ordering; if there is
  // no such state, adds an artificial state with it (taking a copy, so does not
  // need to take ownership).
  void AddEdge(THD *thd, int state_idx, int required_fd_idx,
               const Ordering &ordering);

  // Returns true if the given (non-DECAY) functional dependency applies to the
  // given ordering, and the index of the element from which the FD is active
  // (ie., the last element that was part of the head). One can start inserting
  // the tail element at any point _after_ this index; if it is an EQUIVALENCE
  // FD, one can instead choose to replace the element at start_point entirely.
  bool FunctionalDependencyApplies(const FunctionalDependency &fd,
                                   const Ordering &ordering,
                                   int *start_point) const;

  /**
     Fetch an Ordering::Elements object with size()==m_longest_ordering.
     Get it from m_elements_pool if that is non-empty, otherwise allocate
     from mem_root.
   */
  Ordering::Elements RetrieveElements(MEM_ROOT *mem_root) {
    if (m_elements_pool.empty()) {
      return Ordering::Elements::Alloc(mem_root, m_longest_ordering);
    } else {
      OrderElement *buffer = m_elements_pool.back();
      m_elements_pool.pop_back();
      return Ordering::Elements(buffer, m_longest_ordering);
    }
  }

  /**
     Return an Ordering::Elements object with size()==m_longest_ordering
     to m_elements_pool.
   */
  void ReturnElements(Ordering::Elements elements) {
    // Overwrite the array with garbage, so that we have a better chance
    // of detecting it if we by mistake access it afterwards.
    TRASH(elements.data(), m_longest_ordering * sizeof(elements.data()[0]));
    m_elements_pool.push_back(elements.data());
  }
  // Used for optimizer trace.

  std::string PrintOrdering(const Ordering &ordering) const;
  std::string PrintFunctionalDependency(const FunctionalDependency &fd,
                                        bool html) const;
  void PrintFunctionalDependencies(std::string *trace);
  void PrintInterestingOrders(std::string *trace);
  void PrintNFSMDottyGraph(std::string *trace) const;
  void PrintDFSMDottyGraph(std::string *trace) const;
};

inline bool operator==(const LogicalOrderings::NFSMEdge &a,
                       const LogicalOrderings::NFSMEdge &b) {
  return a.required_fd_idx == b.required_fd_idx && a.state_idx == b.state_idx;
}

inline bool operator!=(const LogicalOrderings::NFSMEdge &a,
                       const LogicalOrderings::NFSMEdge &b) {
  return !(a == b);
}

#endif  // SQL_JOIN_OPTIMIZER_INTERESTING_ORDERS_H
