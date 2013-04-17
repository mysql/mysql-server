// find cycles in a simple WFG

#include "test.h"
#include "wfg.h"

struct verify_extra {
    TXNID next_id;
};

static int verify_nodes(TXNID id, void *extra) {
    struct verify_extra *verify_extra = (struct verify_extra *) extra;
    assert(verify_extra->next_id == id);
    verify_extra->next_id++;
    return 0;
}

static void verify_nodes_in_cycle_12(struct wfg *cycles) {
    struct verify_extra verify_extra = { 1 };
    wfg_apply_nodes(cycles, verify_nodes, &verify_extra);
    assert(verify_extra.next_id == 3);
}

int main(int argc, const char *argv[]) {

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        assert(0);
    }

    // setup
    struct wfg *wfg = wfg_new();
    struct wfg *cycles = wfg_new();

    wfg_add_edge(wfg, 1, 2);
    assert(!wfg_exist_cycle_from_txnid(wfg, 1)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 1, cycles) == 0); 
    assert(!wfg_exist_cycle_from_txnid(wfg, 2)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 1, cycles) == 0);

    wfg_add_edge(wfg, 2, 1);
    assert(wfg_exist_cycle_from_txnid(wfg, 1)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 1, cycles) == 1);
    if (verbose) {
        wfg_print(wfg); wfg_print(cycles);
    }
    verify_nodes_in_cycle_12(cycles);
    assert(wfg_exist_cycle_from_txnid(wfg, 2)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 2, cycles) == 1);
    if (verbose) {
        wfg_print(wfg); wfg_print(cycles);
    }
    verify_nodes_in_cycle_12(cycles);

    wfg_add_edge(wfg, 1, 3);
    assert(!wfg_exist_cycle_from_txnid(wfg, 3)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 3, cycles) == 0);
    assert(wfg_exist_cycle_from_txnid(wfg, 1)); wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 1, cycles) == 1);
    if (verbose) {
        wfg_print(wfg); wfg_print(cycles);
    }
    verify_nodes_in_cycle_12(cycles);
    assert(wfg_exist_cycle_from_txnid(wfg, 2));wfg_reinit(cycles); assert(wfg_find_cycles_from_txnid(wfg, 2, cycles) == 1);
    if (verbose) {
        wfg_print(wfg); wfg_print(cycles);
    }
    verify_nodes_in_cycle_12(cycles);

    wfg_free(wfg);
    wfg_free(cycles);

    return 0;
}
