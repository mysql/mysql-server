int TOKUDB_SHARE::get_status(DB_TXN *txn, HA_METADATA_KEY k, DBT *val) {
    DBT key = {}; key.data = &k; key.size = sizeof k;
    int error = status_block->get(status_block, txn, &key, val, 0);
    return error;
}

int TOKUDB_SHARE::get_status(DB_TXN *txn, HA_METADATA_KEY k, void *p, size_t s) {
    DBT key = {}; key.data = &k; key.size = sizeof k;
    DBT val = {}; val.data = p; val.size = (uint32_t) s; val.flags = DB_DBT_USERMEM;
    int error = status_block->get(status_block, txn, &key, &val, 0);
    return error;
}

int TOKUDB_SHARE::put_status(DB_TXN *txn, HA_METADATA_KEY k, void *p, size_t s) {
    DBT key = {}; key.data = &k; key.size = sizeof k;
    DBT val = {}; val.data = p; val.size = (uint32_t) s;
    int error = status_block->put(status_block, txn, &key, &val, 0);
    return error;
}

int TOKUDB_SHARE::delete_status(DB_TXN *txn, HA_METADATA_KEY k) {
    DBT key = {}; key.data = &k; key.size = sizeof k;
    int error = status_block->del(status_block, txn, &key, DB_DELETE_ANY);
    return error;
}    

void TOKUDB_SHARE::set_card_in_key_info(TABLE *table, uint rec_per_keys, uint64_t rec_per_key[]) {
    uint next_key_part = 0;
    for (uint i = 0; i < table->s->keys; i++) {
        bool is_unique_key = (i == table->s->primary_key) || (table->key_info[i].flags & HA_NOSAME);
        uint num_key_parts = get_key_parts(&table->key_info[i]);
        for (uint j = 0; j < num_key_parts; j++) {
            assert(next_key_part < rec_per_keys);
            ulong val = rec_per_key[next_key_part++];
            if (is_unique_key && j == num_key_parts-1)
                val = 1;
            table->key_info[i].rec_per_key[j] = val;
        }
    }
}

#include "tokudb_buffer.h"

void TOKUDB_SHARE::set_card_in_status(DB_TXN *txn, uint rec_per_keys, uint64_t rec_per_key[]) {
    // encode cardinality into the buffer
    tokudb::buffer b;
    size_t s;
    s = b.append_ui<uint32_t>(rec_per_keys);
    assert(s > 0);
    for (uint i = 0; i < rec_per_keys; i++) {
        s = b.append_ui<uint64_t>(rec_per_key[i]);
        assert(s > 0);
    }
    // write cardinality to status
    int error = put_status(txn, hatoku_cardinality, b.data(), b.size());
    assert(error == 0);
}

int TOKUDB_SHARE::get_card_from_status(DB_TXN *txn, uint rec_per_keys, uint64_t rec_per_key[]) {
    // read cardinality from status
    DBT val = {}; val.flags = DB_DBT_REALLOC;
    int error = get_status(txn, hatoku_cardinality, &val);
    if (error == 0) {
        // decode cardinality from the buffer
        tokudb::buffer b(val.data, 0, val.size);
        size_t s;
        uint32_t num_parts;
        s = b.consume_ui<uint32_t>(&num_parts);
        if (s == 0 || num_parts != rec_per_keys) 
            error = EINVAL;
        if (error == 0) {
            for (uint i = 0; i < rec_per_keys; i++) {
                s = b.consume_ui<uint64_t>(&rec_per_key[i]);
                if (s == 0) {
                    error = EINVAL;
                    break;
                }
            }
        }
    }
    // cleanup
    free(val.data);
    return error;
}

void TOKUDB_SHARE::delete_card_from_status(DB_TXN *txn) {
    int error = delete_status(txn, hatoku_cardinality);
    assert(error == 0);
}
