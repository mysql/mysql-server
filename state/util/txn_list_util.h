#pragma once

#include "common.h"

// from right to left, 0-->bitmap_size-1
static int GetFirstFreeBit(unsigned char* bitmap, size_t bitmap_size) {
    for(int i = 0; i < bitmap_size; ++i) {
        if (static_cast<unsigned char>(bitmap[i]) != 0xFF) {
            int offset = 0;
            while((bitmap[i] & (1 << offset)) != 0) {
                ++offset;
            }
            return i * 8 + offset;
        }
    }
    return -1;
}

// change the specific bit from 0 to 1
static void SetBitToUsed(unsigned char* bitmap, int pos) {
    bitmap[pos / 8] |= (1 << (pos % 8));
}

// change the specific bit from 1 to 0
static void SetBitToFree(unsigned char* bitmap, int pos) {
    bitmap[pos / 8] &= ~(1 << (pos % 8));
}

static uint64_t HashFunc(const void* data, size_t size) {
    const uint64_t fnv_prime = 1099511628211ULL;
    uint64_t hash = 14695981039346656037ULL;

    const uint8_t* byte_data = static_cast<const uint8_t*>(data);

    for(size_t i = 0; i < size; ++i) {
        hash ^= byte_data[i];
        hash *= fnv_prime;
    }

    return hash;
}

static void GetHashCodeForTxn(TxnItem* txn) {
    txn->hash_code ^= HashFunc(&txn->txn_state, sizeof(uint8_t));
    txn->hash_code ^= HashFunc(&txn->in_depth, sizeof(uint32_t));
    txn->hash_code ^= HashFunc(&txn->in_innodb, sizeof(uint32_t));
    txn->hash_code ^= HashFunc(&txn->abort, sizeof(bool));
    txn->hash_code ^= HashFunc(&txn->no, sizeof(tx_id_t));
    txn->hash_code ^= HashFunc(&txn->id, sizeof(tx_id_t));
}