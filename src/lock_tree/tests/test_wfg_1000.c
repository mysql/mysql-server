// find cycles in a 1000 node WFG

#include "test.h"
#include "wfg.h"

struct verify_extra {
    int next_id;
    TXNID ids[2];
};

static int verify_nodes(TXNID id, void *extra) {
    struct verify_extra *verify_extra = (struct verify_extra *) extra;
    assert(verify_extra->next_id < 2 && verify_extra->ids[verify_extra->next_id] == id);
    verify_extra->next_id++;
    return 0;
}

static void verify_nodes_in_cycle(struct wfg *cycles, TXNID a, TXNID b) {
    struct verify_extra verify_extra = { .next_id = a, .ids = { a, b } };
    wfg_apply_nodes(cycles, verify_nodes, &verify_extra);
    assert(verify_extra.next_id == 2);
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

    struct wfg *wfg = wfg_new();
    struct wfg *cycles = wfg_new();

    for (int id = 1; id <= 1000; id++)
        wfg_add_edge(wfg, 0, id);

    for (int id = 0; id <= 1000; id++) {
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, id, cycles) == 0);
    }

    for (int id = 1; id <= 1000; id++) {
        wfg_add_edge(wfg, id, 0);
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, 0, cycles) == id);
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, id, cycles) == 1);
        verify_nodes_in_cycle(cycles, 0, id);
    }

    wfg_free(wfg);
    wfg_free(cycles);

    return 0;
}
