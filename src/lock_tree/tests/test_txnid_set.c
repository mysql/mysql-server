// verify that the txnid set works

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

    struct txnid_set set_static;
    struct txnid_set *set = &set_static;
    txnid_set_init(set);

    const int max_ids = 1000;
    int ids[max_ids];
    for (int i = 0; i < max_ids; i++)
        ids[i] = i+1;

    // verify random adds
    for (int n = max_ids; n > 0; n--) {
        int idx = random() % n;
        int id = ids[idx];
        txnid_set_add(set, id);
        assert(txnid_set_size(set) == (size_t) (max_ids - n + 1));
        ids[idx] = ids[n-1];
    }

    // verify duplicate set add
    for (int id = 1; id <= max_ids; id++) {
        txnid_set_add(set, id);
        assert(txnid_set_size(set) == (size_t) max_ids);
    }

    // verify sorted set
    for (int ith = 0; ith < max_ids; ith++)
        assert(txnid_set_get(set, ith) == (TXNID) (ith+1));

    // verify deletes of non members
    txnid_set_delete(set, 0);
    txnid_set_delete(set, max_ids+1);

    // verify random delete
    for (int i = 0; i < max_ids; i++)
        ids[i] = i+1;
    for (int n = max_ids; n > 0; n--) {
        assert(txnid_set_size(set) == (size_t) n);
        int idx = random() % n;
        int id = ids[idx];
        txnid_set_delete(set, id);
        assert(!txnid_set_is_member(set, id));
        ids[idx] = ids[n-1];
        for (int i = 0; i < n-1; i++)
            assert(txnid_set_is_member(set, ids[i]));
        txnid_set_delete(set, id);  // try it again
    }
    assert(txnid_set_size(set) == 0);

    txnid_set_destroy(set);

    return 0;
}
