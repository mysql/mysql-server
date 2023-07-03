/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0stateful_latching_rules.h
The Stateful_latching_rules class template which can be used to describe
possible states, latches required to transition between them, and then validate
if transitions performed by application take required latches, and that queries
for the state are performed when holding enough latches to prevent state from
changing. */

#ifndef ut0stateful_latching_rules
#define ut0stateful_latching_rules

#include <bitset>

namespace ut {
/** This is a generic mechanism for verifying correctness of latching rules for
state transitions and querying for state of a system. It was created for io_fix
field of buf_page_t, but can be configured via template instantiation for other
usages as long as they fit into the following model:

1. The object can be in one of fixed possible states. In case of io_fix, these
states were io_fix values, but in general the state can be a tuple of several
important, perhaps abstract, properties of the object, for example it could be
a std::pair<io_fix, is_in_page_hash>. As long you can pass the list of possible
states to constructor, you're good to go.

2. There is a fixed set of latches which are relevant to protecting the state.
In case of io_fix there were 3: each page has a latch, and its buffer pool has
a latch, and there is also a more abstract property of "being the thread which
is responsible for IO operation for this page", which is fine, as long as you
can quickly determine which of the three the current thread has, and you can
prove that no two threads can hold a given "latch" at the same time (which is
trivial for regular mutexes, but puts a burden of proof on you in case of
abstract concepts like "IO responsibility")

3. For each pair of states you can specify what latches a thread must hold to
perform it. For example a thread can change io_fix from BUF_IO_NONE to
BUF_IO_WRITE only while holding latches #0, #1 and #2. But the rule could be
more complex, for example "you either must hold 1 and 2, or just 0" - as long as
you can express it as an alternative of conjunctions (without negation) it's OK.

In other words, we model the situation as a graph, with states as nodes, and
edges being possible transitions, where each edge is labeled with a subset of
latches (and there might be zero or more edges between any pair of states).

For example:
+---+             +---+           +---+
| a | ---{0,2}--> | b | <--{2}--> | c |
|   | <---{0,2}-- |   | ---{2}--> |   |
|   | <---{1}---- |   |           |   |
+---+             +---+           +---+

If we know that current state *MUST* be 'a' or 'c', and want to determine which
of the two it is, then it is sufficient to hold just latch #2, because edges
going out of 'a' and edges going out of 'c' all require latch #2.
OTOH if we don't know what the current state is, but want to determine if the
state belongs to the set {b,c}, then it is sufficient to hold just latches
#0 and #1, because there are only three edges going in or out of {b,c}, namely
a-{0,2}->b, a<-{0,2}-b, a<-{1}-b, and each of them requires #0 or #1.
I hope this example shows that verifying correctness case by case like that is
possible to do manually, but seems error prone, subject to code rot, and would
benefit from automation.

You specify the type of nodes and number of latches by template instatiation,
and shape of the graph in the constructor.

This class then can then offer you two things:
1. If you call on_transition(from, to, owned_latches) it will verify for you
that such transition is allowed as at least one of the edges requires a subset
of owned_latches. This way you can verify that the model you described matches
reality. For example we call it from buf_page_set_io_fix(page,state).

2. If you call assert_latches_let_distinguish(owned_latches,A,B) where A and B
are two disjoint sets of states, it will tell you if the current thread holds
enough latches to make it possible to determine if state belongs to A or to B in
a meaningful way - that is the answer to such question can not change while
being asked, because each edge going out from A and out from B has at least one
of the owned_latches.
For example if a thread knows that io_fix is BUF_IO_READ or BUF_IO_WRITE but is
unsure which one of them it is, it might check io_fix state, but only if it has
latches which prevent a change from BUF_IO_READ to BUF_IO_PIN or BUF_IO_NONE or
BUF_IO_WRITE, and from BUF_IO_WRITE to BUF_IO_PIN or BUF_IO_NONE or BUF_IO_READ
- otherwise the answer could change during query.
Note that this might be a smaller set of latches than needed to prevent any
activity at all. For example we don't care about changes from BUF_IO_PIN to
BUF_IO_FIX. Also, in this example A and B are both singletons, but the mechanism
works for arbitrarily large sets. */
template <typename Node, size_t LATCHES_COUNT>
class Stateful_latching_rules {
 public:
  /** The type for a state in our automata */
  using node_t = Node;

  /** The type for a set of states */
  using nodes_set_t = std::set<node_t>;

  /** The type for a set of latches - there are LATCHES_COUNT distinct
  latches, each one corresponds to a fixed position in a bitset. The mapping is
  up to the user of this class - just be consistent. */
  using latches_set_t = std::bitset<LATCHES_COUNT>;

  /** The type for a possible transition from one state to another while holding
  at least a given set of latches. */
  struct edge_t {
    /** The old state from which the transition starts. */
    node_t m_from;

    /** The required subset of latches for this particular transition. Note that
    there might be several edges for the same from->to pair, which means that a
    thread can pick any of them for which it has the required latches. */
    latches_set_t m_latches;

    /** The new state from to which the transition leads. */
    node_t m_to;

    /** Creates a description of one of allowed state transitions from given
    state to another while holding given latches. For example
    {BUF_IO_READ, {0, 2}, BUF_IO_NONE}
    says a if a thread holds latches 0 and 2 then it can transition from
    BUF_IO_READ to BUF_IO_NONE.
    @param[in]  from  The old state from which the transition starts
    @param[in]  idxs  The list of integers in range <0,LATCHES_COUNT) used to
                      construct set of latches for this edge.
    @param[in]  to    The new state from to which the transition leads. */
    edge_t(node_t from, std::initializer_list<int> &&idxs, node_t to)
        : m_from(from), m_latches(), m_to(to) {
      for (auto id : idxs) m_latches[id] = true;
    }
  };

 private:
  /** The set of all possible states. Useful for computing the complement of
  any given set */
  const nodes_set_t m_states;

  /** The list of allowed state transitions. */
  const std::vector<edge_t> m_edges;

  /** A helper function which prints the ids of latches from the set of latches
  to an ostream-like object (such as our ib::logger).
  @param[in]  sout      the ostream-like object to print latches to
  @param[in]  latches   a set of latches to print to sout
  @*/
  template <typename T>
  void print(T &sout, const latches_set_t &latches) const {
    sout << '{';
    bool first = true;
    for (size_t i = 0; i < latches.size(); ++i) {
      if (latches[i]) {
        if (!first) {
          sout << ", ";
        }
        sout << i;
        first = false;
      }
    }
    sout << '}';
  }

  /** Checks if another thread could change the state even though we hold some
  latches. In other words, checks if the rules allow a thread which ISN'T
  holding any of the `forbiden_latches` to transition from at least one state
  inside  `source` to at least one state inside `destination`, where `source`
  and `destination` are sets of states.
  It prints an error if transition is possible.
  @param[in]  forbiden_latches  The set of latches which the hypothetical
                                transitioning thread can't hold (because we are
                                holding them).
  @param[in]  source            The set of source states
  @param[in]  destination       The set of destination states
  @return true iff there is at least one edge from a state inside `source` to
  a state inside `destination` labeled with a set of latches disjoint with
  `forbiden_latches`. */
  bool is_transition_possible(const latches_set_t &forbiden_latches,
                              const nodes_set_t &source,
                              const nodes_set_t &destination) const {
    for (const edge_t &edge : m_edges) {
      if (source.count(edge.m_from) && destination.count(edge.m_to) &&
          (edge.m_latches & forbiden_latches) == 0) {
        {
          ib::error the_err{};
          the_err << "It is possible to transition from " << edge.m_from
                  << " to " << edge.m_to << " holding just ";
          print(the_err, edge.m_latches);
          the_err << " even when we hold ";
          print(the_err, forbiden_latches);
        }

        return true;
      }
    }
    return false;
  }

  /** Computes the complement of the given set of states, that is a set of all
  of the other states.
  @param[in]  states  The set of states
  @return the complement of the set of states */
  nodes_set_t complement(const nodes_set_t &states) const {
    nodes_set_t other_states;
    std::set_difference(m_states.begin(), m_states.end(), states.begin(),
                        states.end(),
                        std::inserter(other_states, other_states.end()));
    return other_states;
  }

  /** A convenience function for the special case of
  @see is_transition_possible(forbiden_latches, A,B) where B=complement(A).
  In other words, it checks if another thread can cause the state to leave the
  given set of states even though we hold a set of `forbiden_latches`.
  @param[in]  forbiden_latches  The set of latches which the hypothetical
                                transitioning thread can't hold (because we are
                                holding them).
  @param[in]  source            The set of source states
  @return true iff there is at least one edge from a state inside `source` to
  a state outside of `source` labeled with a set of latches disjoint with
  `forbiden_latches`.*/
  bool can_leave(const latches_set_t &forbiden_latches,
                 const nodes_set_t &source) const {
    return is_transition_possible(forbiden_latches, source, complement(source));
  }

 public:
  /** Creates a set of rules for `allowed_transitions` between `all_states`.
  @param[in]  all_states          The set of all possible states of the system
  @param[in]  allowed_transitions The set of allowed transitions. There might be
                                  multiple edges between the same pare of states
                                  which is interpreted as an alternative:
                                  {{x, {0,2}, y}, {x, {1}, y}} would mean, that
                                  a thread which holds just latch #2 can't
                                  transition from x to y, but a thread which
                                  holds #0 and #2 can, and so does a thread
                                  which holds just #1 or #0 and #1 or #1 and #2
                                  or all three or even more. */
  Stateful_latching_rules(nodes_set_t all_states,
                          std::vector<edge_t> allowed_transitions)
      : m_states(std::move(all_states)),
        m_edges(std::move(allowed_transitions)) {}

  /** Checks if `owned_latches` (presumably held by the current thread) are
  enough to meaningfully ask a question if current state belongs to set A as
  opposed to set B. In other words it checks if `owned_latches` prevent other
  threads from performing state transition from the inside of set A outside,
  and from the inside of set B outside. Otherwise it prints debug information
  as fatal error.
  @param[in]  owned_latches   The latches the current thread owns
  @param[in]  A               The first set of latches
  @param[in]  B               The second set of latches */
  void assert_latches_let_distinguish(const latches_set_t &owned_latches,
                                      const nodes_set_t &A,
                                      const nodes_set_t &B) const {
    const bool can_leave_A = can_leave(owned_latches, A);
    const bool can_leave_B = can_leave(owned_latches, B);

    if (can_leave_A || can_leave_B) {
      ib::fatal the_err{UT_LOCATION_HERE};
      the_err << "We can leave "
              << (can_leave_A && can_leave_B ? "both A and B"
                                             : (can_leave_A ? "A" : "B"))
              << " as we only hold: ";
      print(the_err, owned_latches);
    }
  }

  /** A convenience function for the special case of
  @see assert_latches_let_distinguish(owned_latches, A, complement(A)).
  In other words, it checks if the current thread's owned_latches prevent state
  transitions to and from the set A.
  @param[in]  owned_latches   The latches the current thread owns
  @param[in]  A               The set of latches*/
  void assert_latches_let_distinguish(const latches_set_t &owned_latches,
                                      const nodes_set_t &A) const {
    assert_latches_let_distinguish(owned_latches, A, complement(A));
  }

  /** Checks if the transition between given states holding specified latches is
  allowed by the rules. In other words it checks if there is at least one edge
  between `from` and `to` nodes labeled with a subset of `ownwed_latches`.
  It prints a debug information as fatal error if rules are violated. */
  void on_transition(const node_t &from, const node_t &to,
                     const latches_set_t &owned_latches) const {
    if (from == to) {
      return;
    }
    const auto missing_latches = ~owned_latches;

    if (std::any_of(std::begin(m_edges), std::end(m_edges), [&](auto edge) {
          return (edge.m_from == from && edge.m_to == to) &&
                 (edge.m_latches & missing_latches) == 0;
        })) {
      return;
    }
    ib::fatal the_err{UT_LOCATION_HERE};
    the_err << "Disallowed transition FROM " << from << " TO " << to
            << " WITH ";
    print(the_err, owned_latches);
  }
};
}  // namespace ut
#endif /* ut0stateful_latching_rules */
