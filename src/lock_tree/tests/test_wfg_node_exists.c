// test the wfg_node_exists function

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

    TXNID max_ids = 1000;
    for (TXNID id = 1; id < max_ids; id++)
        wfg_add_edge(wfg, id, id+1);
    assert(!wfg_node_exists(wfg, 0));
    for (TXNID id = 1; id <= max_ids; id++)
        assert(wfg_node_exists(wfg, id));
    assert(!wfg_node_exists(wfg, max_ids+2));
               
    wfg_free(wfg);

    return 0;
}
