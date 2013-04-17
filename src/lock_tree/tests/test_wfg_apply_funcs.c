// find cycles in a simple WFG

#include "test.h"
#include "wfg.h"

struct print_node_extra {
    TXNID max_id;
};

static int print_some_nodes(TXNID id, void *extra) {
    struct print_node_extra *print_node_extra = (struct print_node_extra *) extra;
    if (verbose) printf("%lu ", id);
    return id == print_node_extra->max_id ? -1 : 0;
}       

struct print_edge_extra {
    TXNID max_id;
};

static int print_some_edges(TXNID node_id, TXNID edge_id, void *extra) {
    struct print_edge_extra *print_edge_extra = (struct print_edge_extra *) extra;
    if (verbose) printf("(%lu %lu) ", node_id, edge_id);
    return edge_id == print_edge_extra->max_id ? -1 : 0;
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

    const int max_ids = 10;

    for (int id = 0; id < max_ids; id++)
        for (int edge_id = 0; edge_id < max_ids; edge_id++)
            wfg_add_edge(wfg, id, edge_id);

    struct print_node_extra print_node_extra = { max_ids/2 };
    wfg_apply_nodes(wfg, print_some_nodes, &print_node_extra);
    if (verbose) printf("\n");

    struct print_edge_extra print_edge_extra = { max_ids/2 };
    wfg_apply_edges(wfg, max_ids/2, print_some_edges, &print_edge_extra);
    if (verbose) printf("\n");

    wfg_free(wfg);

    return 0;
}
