/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_WFG_H
#define TOKU_WFG_H

#include <ft/fttypes.h>

#include <util/omt.h>

#include "txnid_set.h"

namespace toku {

// A wfg is a 'wait-for' graph. A directed edge in represents one
// txn waiting for another to finish before it can acquire a lock.

class wfg {
public:
    // Create a lock request graph
    void create(void);

    // Destroy the internals of the lock request graph
    void destroy(void);

    // Add an edge (a_id, b_id) to the graph
    void add_edge(TXNID a_txnid, TXNID b_txnid);

    // Return true if a node with the given transaction id exists in the graph.
    // Return false otherwise.
    bool node_exists(TXNID txnid);

    // Return true if there exists a cycle from a given transaction id in the graph.
    // Return false otherwise.
    bool cycle_exists_from_txnid(TXNID txnid);

    // Apply a given function f to all of the nodes in the graph.  The apply function
    // returns when the function f is called for all of the nodes in the graph, or the 
    // function f returns non-zero.
    void apply_nodes(int (*fn)(TXNID txnid, void *extra), void *extra);

    // Apply a given function f to all of the edges whose origin is a given node id. 
    // The apply function returns when the function f is called for all edges in the
    // graph rooted at node id, or the function f returns non-zero.
    void apply_edges(TXNID txnid,
            int (*fn)(TXNID txnid, TXNID edge_txnid, void *extra), void *extra);

private:
    struct node {
        // txnid for this node and the associated set of edges
        TXNID txnid;
        txnid_set edges;
        bool visited;

        static node *alloc(TXNID txnid);

        static void free(node *n);
    };
    ENSURE_POD(node);

    toku::omt<node *> m_nodes;

    node *find_node(TXNID txnid);

    node *find_create_node(TXNID txnid);

    bool cycle_exists_from_node(node *target, node *head);

    static int find_by_txnid(node *const &node_a, const TXNID &txnid_b);
};
ENSURE_POD(wfg);

} /* namespace toku */

#endif /* TOKU_WFG_H */
