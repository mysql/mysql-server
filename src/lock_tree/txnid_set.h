#include "omt.h"

typedef struct txnid_set {
    OMT ids; // private: set of ids
} txnid_set;

void txnid_set_init(txnid_set *txnids);

void txnid_set_destroy(txnid_set *txnids);

// Return true if the given transaction id is a member of the set.
// Otherwise, return false.
bool txnid_set_is_member(txnid_set *txnids, TXNID id);

// Add a given id to the set of ids.
void txnid_set_add(txnid_set *txnids, TXNID id);

// Delete a given id from the set.
void txnid_set_delete(txnid_set *txnids, TXNID id);

// Return the number of id's in the set
size_t txnid_set_size(txnid_set *txnids);

// Get the ith id in the set, assuming that the set is sorted.
TXNID txnid_set_get(txnid_set *txnids, size_t ith);

