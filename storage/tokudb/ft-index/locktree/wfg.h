/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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
