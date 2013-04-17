// find no cycles in an empty WFG

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

    assert(!wfg_exist_cycle_from_txnid(wfg, 0)); assert(wfg_find_cycles_from_txnid(wfg, 0, cycles) == 0);
    wfg_print(wfg); wfg_print(cycles);
    wfg_add_edge(wfg, 1, 2);
    wfg_add_edge(wfg, 2, 1);
    assert(wfg_exist_cycle_from_txnid(wfg, 1)); assert(wfg_find_cycles_from_txnid(wfg, 1, cycles) != 0);
    wfg_print(wfg); wfg_print(cycles);

    wfg_free(wfg);
    wfg_free(cycles);

    return 0;
}
