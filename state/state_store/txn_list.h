#pragma once

#include "util/common.h"

/**
 * TxnList in StateNode is a fixed length array, whose length equals to MAX_CONNECTION_NUM.
 * There is a bitmap to maintain the free space of txn_lsit 
 * and a latch to protect the concurreny access of the bitmap.
 * When the MasterNode wants to add a new transaction into the txn_list, it first uses CAS to
 * obtain the access control to txn_list. Then it uses READ to get the bitmap and find a
 * free space for the current new transaction. After getting the free space, it uses WRITE&CAS
 * to update the bitmap and release the access control in order. Then it uses WRITE to write 
 * the new transaction into remote txn_list. To ensure the integrity of WRITE, we add a hash code 
 * at the end of each TxnItem.
*/

struct TxnItem {
    uint8_t txn_state;
    uint32_t in_depth;
    uint32_t in_innodb;
    bool abort;
    tx_id_t no;
    tx_id_t id;
    uint64_t hash_code;
};

struct TxnListBitmap {
    uint64_t map1;
};