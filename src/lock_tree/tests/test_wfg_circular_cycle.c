// find cycles in a simple WFG

#include "test.h"
#include "wfg.h"

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
    for (TXNID i = 1; i <= 2; i++) {
        assert(!wfg_exist_cycle_from_txnid(wfg, i));
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, i, cycles) == 0);
    }

    wfg_add_edge(wfg, 2, 3);
    for (TXNID i = 1; i <= 3; i++) {
        assert(!wfg_exist_cycle_from_txnid(wfg, i));
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, i, cycles) == 0);
    }

    wfg_add_edge(wfg, 3, 4);
    for (TXNID i = 1; i <= 4; i++) {
        assert(!wfg_exist_cycle_from_txnid(wfg, i));
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, i, cycles) == 0);
    }

    wfg_add_edge(wfg, 4, 1);
    for (TXNID i = 1; i <= 4; i++) {
        assert(wfg_exist_cycle_from_txnid(wfg, i));
        wfg_reinit(cycles);
        assert(wfg_find_cycles_from_txnid(wfg, i, cycles) == 1);
        if (verbose) wfg_print(cycles);
    }

    wfg_free(wfg);
    wfg_free(cycles);

    return 0;
}
