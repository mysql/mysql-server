#include "omt.h"

struct wfg {
    OMT nodes; // private: set of nodes
};

// Allocate and initialize a wfg
struct wfg *wfg_new(void);

// Destroy and free a wfg
void wfg_free(struct wfg *wfg);

void wfg_init(struct wfg *wfg);

void wfg_reinit(struct wfg *wfg);

void wfg_destroy(struct wfg *wfg);

void wfg_print(struct wfg *wfg);

// Add an edge (a_id, b_id) to the graph
void wfg_add_edge(struct wfg *wfg, TXNID a_id, TXNID b_id);

// Return true if there exists a cycle from a given transaction id in the graph.
// Return false otherwise.
bool wfg_exist_cycle_from_txnid(struct wfg *wfg, TXNID id);

// Find all cycles rooted with the given transaction id.
// Return the number of cycles found.
// Return a subset of the graph that covers the cycles
int wfg_find_cycles_from_txnid(struct wfg *wfg, TXNID id, struct wfg *cycles);

// Return true if a node with the given transaction id exists in the graph.
// Return false otherwise.
bool wfg_node_exists(struct wfg *wfg, TXNID id);

// Apply a given function f to all of the nodes in the graph.  The apply function
// returns when the function f is called for all of the nodes in the graph, or the 
// function f returns non-zero.
void wfg_apply_nodes(struct wfg *wfg, int (*f)(TXNID id, void *extra), void *extra);

// Apply a given function f to all of the edges whose origin is a given node id.  The apply function
// returns when the function f is called for all edges in the graph rooted at node id, or the
// function f returns non-zero.
void wfg_apply_edges(struct wfg *wfg, TXNID node_id, int (*f)(TXNID node_id, TXNID edge_id, void *extra), void *extra);

// Delete the node associated with the given transaction id from the graph.
// Delete all edges to the node from all other nodes.
void wfg_delete_node_for_txnid(struct wfg *wfg, TXNID id);

