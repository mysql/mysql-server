/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <locktree/wfg.h>

namespace toku {

enum {
    WFG_TEST_MAX_TXNID = 10
};

struct visit_extra {
    bool nodes_visited[WFG_TEST_MAX_TXNID];
    bool edges_visited[WFG_TEST_MAX_TXNID][WFG_TEST_MAX_TXNID];

    void clear(void) {
        memset(nodes_visited, 0, sizeof(nodes_visited));
        memset(edges_visited, 0, sizeof(edges_visited));
    }
};

// wfg node visit callback
static int visit_nodes(TXNID txnid, void *extra) {
    visit_extra *ve = static_cast<visit_extra *>(extra);
    invariant(txnid < WFG_TEST_MAX_TXNID);
    invariant(!ve->nodes_visited[txnid]);
    ve->nodes_visited[txnid] = true;
    return 0;
}

// wfg edge visit callback
static int visit_edges(TXNID txnid, TXNID edge_txnid, void *extra) {
    visit_extra *ve = static_cast<visit_extra *>(extra);
    invariant(txnid < WFG_TEST_MAX_TXNID);
    invariant(edge_txnid < WFG_TEST_MAX_TXNID);
    invariant(!ve->edges_visited[txnid][edge_txnid]);
    ve->edges_visited[txnid][edge_txnid] = true;
    return 0;
}

// the graph should only have 3 nodes labelled 0 1 and 2
static void verify_only_nodes_012_exist(wfg *g) {
    visit_extra ve;
    ve.clear();
    g->apply_nodes(visit_nodes, &ve);
    for (int i = 0; i < WFG_TEST_MAX_TXNID; i++) {
        if (i == 0 || i == 1 || i == 2) {
            invariant(ve.nodes_visited[i]);
        } else {
            invariant(!ve.nodes_visited[i]);
        }
    }
}

// the graph should only have edges 0->1 and 1->2
static void verify_only_edges_01_12_exist(wfg *g) {
    visit_extra ve;
    ve.clear();
    g->apply_edges(0, visit_edges, &ve);
    g->apply_edges(1, visit_edges, &ve);
    g->apply_edges(2, visit_edges, &ve);
    for (int i = 0; i < WFG_TEST_MAX_TXNID; i++) {
        for (int j = 0; j < WFG_TEST_MAX_TXNID; j++) {
            if ((i == 0 && j == 1) || (i == 1 && j == 2)) {
                invariant(ve.edges_visited[i][j]);
            } else {
                invariant(!ve.edges_visited[i][j]);
            }
        }
    }
}

static void test_add_cycle_exists() {
    wfg g;
    g.create();

    // test that adding edges works and is idempotent

    g.add_edge(0, 1);
    invariant(g.node_exists(0));
    invariant(g.node_exists(1));
    g.add_edge(1, 2);
    invariant(g.node_exists(0));
    invariant(g.node_exists(1));
    invariant(g.node_exists(2));

    // verify that adding edges with the same nodes
    // does not store multiple nodes with the same txnid.
    verify_only_nodes_012_exist(&g);
    verify_only_edges_01_12_exist(&g);
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    verify_only_nodes_012_exist(&g);
    verify_only_edges_01_12_exist(&g);

    // confirm that no cycle exists from txnid 0 1 or 2
    invariant(!g.cycle_exists_from_txnid(0));
    invariant(!g.cycle_exists_from_txnid(1));
    invariant(!g.cycle_exists_from_txnid(2));

    // add 2,3 and 3,1. now there should be a cycle
    // from 1 2 and 3 but not 0.
    //
    // 0 --> 1 -->  2
    //       ^    /
    //       ^ 3 <
    g.add_edge(2, 3);
    g.add_edge(3, 1);
    invariant(!g.cycle_exists_from_txnid(0));
    invariant(g.cycle_exists_from_txnid(1));
    invariant(g.cycle_exists_from_txnid(2));
    invariant(g.cycle_exists_from_txnid(3));

    // add 2,4. should not have a cycle from 4, but yes from 2.
    g.add_edge(2, 4);
    invariant(!g.cycle_exists_from_txnid(4));
    invariant(g.cycle_exists_from_txnid(2));

    g.destroy();
}

static void test_find_cycles() {
    wfg g;
    g.create();

    // TODO: verify that finding cycles works

    g.destroy();
}

} /* namespace toku */

int main(void) {
    toku::test_add_cycle_exists();
    toku::test_find_cycles();
    return 0;
}
