#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "toku_assert.h"
#include "brttypes.h"
#include "memory.h"
#include "toku_list.h"
#include "txnid_set.h"
#include "wfg.h"

struct wfg_node {
    txnid_set edges; // set of edges
    TXNID id;
    bool visited;
};

static struct wfg_node *
wfg_new_node(TXNID id) {
    struct wfg_node *node = (struct wfg_node *) toku_xmalloc(sizeof (struct wfg_node));
    node->id = id;
    node->visited = false;
    txnid_set_init(&node->edges);
    return node;
}    

static void 
wfg_node_free(struct wfg_node *node) {
    txnid_set_destroy(&node->edges);
    toku_free(node);
}

struct wfg *
wfg_new(void) {
    struct wfg *wfg = (struct wfg *) toku_xmalloc(sizeof (struct wfg));
    wfg_init(wfg);
    return wfg;
}

void 
wfg_free(struct wfg *wfg) {
    wfg_destroy(wfg);
    toku_free(wfg);
}

void 
wfg_init(struct wfg *wfg) {
    int r = toku_omt_create(&wfg->nodes); 
    assert_zero(r);
}

void 
wfg_destroy(struct wfg *wfg) {
    size_t n_nodes = toku_omt_size(wfg->nodes);
    for (size_t i = 0; i < n_nodes; i++) {
        OMTVALUE v;
        int r = toku_omt_fetch(wfg->nodes, i, &v); 
        assert_zero(r);
        struct wfg_node *node = (struct wfg_node *) v;
        wfg_node_free(node);
    }
    toku_omt_destroy(&wfg->nodes); 
}

void 
wfg_reinit(struct wfg *wfg) {
    wfg_destroy(wfg);
    wfg_init(wfg);
}

static int 
wfg_compare_nodes(OMTVALUE a, void *b) {
    struct wfg_node *a_node = (struct wfg_node *) a;
    TXNID a_id = a_node->id;
    TXNID b_id = * (TXNID *) b;
    int r;
    if (a_id < b_id) 
        r = -1;
    else if (a_id > b_id) 
        r = +1;
    else
        r = 0;
    return r;
}

// find node by id
static struct wfg_node *
wfg_find_node(struct wfg *wfg, TXNID id) {
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(wfg->nodes, wfg_compare_nodes, &id, &v, &idx);
    struct wfg_node *node;
    if (r == DB_NOTFOUND)
        node = NULL;
    else
        node = (struct wfg_node *) v;
    return node;
}

bool 
wfg_node_exists(struct wfg *wfg, TXNID id) {
    struct wfg_node *node = wfg_find_node(wfg, id);
    return node != NULL;
}

// insert a new node
static struct wfg_node *
wfg_find_create_node(struct wfg *wfg, TXNID id) {
    int r;
    OMTVALUE v;
    u_int32_t idx;
    r = toku_omt_find_zero(wfg->nodes, wfg_compare_nodes, &id, &v, &idx);
    struct wfg_node *node;
    if (r == DB_NOTFOUND) {
        node = wfg_new_node(id); 
        assert(node);
        r = toku_omt_insert_at(wfg->nodes, node, idx); 
        assert_zero(r);
    } else {
        node = (struct wfg_node *) v;
    }
    return node;
}

void 
wfg_add_edge(struct wfg *wfg, TXNID a_id, TXNID b_id) {
    struct wfg_node *a_node = wfg_find_create_node(wfg, a_id);
    struct wfg_node *b_node = wfg_find_create_node(wfg, b_id);
    txnid_set_add(&a_node->edges, b_node->id);
}

static int 
wfg_find_cycles_from_node(struct wfg *wfg, struct wfg_node *target, struct wfg_node *head, struct wfg *cycles) {
    int n_cycles = 0;
    head->visited = true;
    size_t n_edges = txnid_set_size(&head->edges);
    for (size_t i = 0; i < n_edges; i++) {
        TXNID edge_id = txnid_set_get(&head->edges, i);
        if (target->id == edge_id) {
            wfg_add_edge(cycles, head->id, edge_id);
            n_cycles += 1;
        } else {
            struct wfg_node *new_head = wfg_find_node(wfg, edge_id);
            if (new_head && !new_head->visited) {
                int this_n_cycles = wfg_find_cycles_from_node(wfg, target, new_head, cycles);
                if (this_n_cycles) {
                    wfg_add_edge(cycles, head->id, edge_id);
                    n_cycles += this_n_cycles;
                }
            }
        }
    }
    head->visited = false;
    return n_cycles;
}

int 
wfg_find_cycles_from_txnid(struct wfg *wfg, TXNID a, struct wfg *cycles) {
    struct wfg_node *a_node = wfg_find_node(wfg, a);
    int n_cycles = 0;
    if (a_node)
        n_cycles = wfg_find_cycles_from_node(wfg, a_node, a_node, cycles);
    return n_cycles;
}

static bool 
wfg_exist_cycle_from_node(struct wfg *wfg, struct wfg_node *target, struct wfg_node *head) {
    bool cycle_found = false;
    head->visited = true;
    size_t n_edges = txnid_set_size(&head->edges);
    for (size_t i = 0; i < n_edges && !cycle_found; i++) {
        TXNID edge_id = txnid_set_get(&head->edges, i);
        if (target->id == edge_id) { 
            cycle_found = true;
        } else {
            struct wfg_node *new_head = wfg_find_node(wfg, edge_id);
            if (new_head && !new_head->visited)
                cycle_found = wfg_exist_cycle_from_node(wfg, target, new_head);
        }
    }
    head->visited = false;
    return cycle_found;
}

bool 
wfg_exist_cycle_from_txnid(struct wfg *wfg, TXNID a) {
    struct wfg_node *a_node = wfg_find_node(wfg, a);
    bool cycles_found = false;
    if (a_node)
        cycles_found = wfg_exist_cycle_from_node(wfg, a_node, a_node);
    return cycles_found;
}

static int 
print_node_id(TXNID node_id, void *extra UU()) {
    printf("%lu ", node_id);
    return 0;
}

static int 
print_edge(TXNID node_id, TXNID edge_id, void *extra UU()) {
    printf("(%lu %lu) ", node_id, edge_id);
    return 0;
}

static int 
print_all_edges_from_node(TXNID node_id, void *extra) {
    struct wfg *wfg = (struct wfg *) extra;
    wfg_apply_edges(wfg, node_id, print_edge, extra);
    return 0;
}

void 
wfg_print(struct wfg *wfg) {
    printf("nodes: ");
    wfg_apply_nodes(wfg, print_node_id, wfg);
    printf("\n");
    printf("edges: ");
    wfg_apply_nodes(wfg, print_all_edges_from_node, wfg);
    printf("\n");
}

void 
wfg_apply_nodes(struct wfg *wfg, int (*f)(TXNID id, void *extra), void *extra) {
    int r;
    size_t n_nodes = toku_omt_size(wfg->nodes);
    for (size_t i = 0; i < n_nodes; i++) {
        OMTVALUE v;
        r = toku_omt_fetch(wfg->nodes, i, &v);
        assert_zero(r);
        struct wfg_node *i_node = (struct wfg_node *) v;
        r = f(i_node->id, extra);
        if (r != 0)
            break;
    }
}

void 
wfg_apply_edges(struct wfg *wfg, TXNID node_id, int (*f)(TXNID node_id, TXNID edge_id, void *extra), void *extra) {
    struct wfg_node *node = wfg_find_node(wfg, node_id);
    if (node) {
        size_t n_edges = txnid_set_size(&node->edges);
        for (size_t i = 0; i < n_edges; i++) {
            int r = f(node_id, txnid_set_get(&node->edges, i), extra);
            if (r != 0)
                break;
        }
    }
}

    
