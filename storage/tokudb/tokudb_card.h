namespace tokudb {

    // Set the key_info cardinality counters for the table.
    void set_card_in_key_info(TABLE *table, uint rec_per_keys, uint64_t rec_per_key[]) {
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
    
    // Put the cardinality counters into the status dictionary.
    void set_card_in_status(DB *status_db, DB_TXN *txn, uint rec_per_keys, uint64_t rec_per_key[]) {
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
        int error = write_to_status(status_db, hatoku_cardinality, b.data(), b.size(), txn);
        assert(error == 0);
    }

    // Get the cardinality counters from the status dictionary.
    int get_card_from_status(DB *status_db, DB_TXN *txn, uint rec_per_keys, uint64_t rec_per_key[]) {
        // read cardinality from status
        void *buf = 0; size_t buf_size = 0;
        int error = get_status_realloc(status_db, txn, hatoku_cardinality, &buf, &buf_size);
        if (error == 0) {
            // decode cardinality from the buffer
            tokudb::buffer b(buf, 0, buf_size);
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
        free(buf);
        return error;
    }

    // Delete the cardinality counters from the status dictionary.
    void delete_card_from_status(DB *status_db, DB_TXN *txn) {
        int error = remove_from_status(status_db, hatoku_cardinality, txn);
        assert(error == 0);
    }

    bool find_index_of_key(const char *key_name, TABLE_SHARE *table_share, uint *index_offset_ptr) {
        for (uint i = 0; i < table_share->keys; i++) {
            if (strcmp(key_name, table_share->key_info[i].name) == 0) {
                *index_offset_ptr = i;
                return true;
            }
        }
        return false;
    }

    // Altered table cardinality = select cardinality data from current table cardinality for keys that exist 
    // in the altered table and the current table.
    void set_card_from_status(DB *status_db, DB_TXN *txn, TABLE_SHARE *table_share, TABLE_SHARE *altered_table_share) {
        int error;
        // read existing cardinality data from status
        uint64_t rec_per_key[table_share->key_parts];
        error = get_card_from_status(status_db, txn, table_share->key_parts, rec_per_key);
        // set altered records per key to unknown
        uint64_t altered_rec_per_key[altered_table_share->key_parts];
        for (uint i = 0; i < altered_table_share->key_parts; i++)
            altered_rec_per_key[i] = 0;
        // compute the beginning of the key offsets in the original table
        uint orig_key_offset[table_share->keys];
        uint orig_key_parts = 0;
        for (uint i = 0; i < table_share->keys; i++) {
            orig_key_offset[i] = orig_key_parts;
            orig_key_parts += table_share->key_info[i].key_parts;
        }
        // if orig card data exists, then use it to compute new card data
        if (error == 0) {
            uint key_parts = 0;
            for (uint i = 0; error == 0 && i < altered_table_share->keys; i++) {
                uint orig_key_index;
                if (find_index_of_key(altered_table_share->key_info[i].name, table_share, &orig_key_index)) {
                    memcpy(&altered_rec_per_key[key_parts], &rec_per_key[orig_key_offset[orig_key_index]], altered_table_share->key_info[i].key_parts);
                }
                key_parts += altered_table_share->key_info[i].key_parts;
            }
        }
        if (error == 0)
            set_card_in_status(status_db, txn, altered_table_share->key_parts, altered_rec_per_key);
        else
            delete_card_from_status(status_db, txn);
    }

    // Compute records per key for all key parts of the ith key of the table.
    // For each key part, put records per key part in *rec_per_key_part[key_part_index].
    // Returns 0 if success, otherwise an error number.
    // TODO statistical dives into the FT
    int analyze_card(DB *db, DB_TXN *txn, bool is_unique __attribute__((unused)), uint64_t num_key_parts, uint64_t *rec_per_key_part,
                     int (*key_compare)(DB *, const DBT *, const DBT *, uint),
                     int (*analyze_progress)(void *extra, uint64_t rows), void *progress_extra) {
        int error = 0;
        uint64_t rows = 0;
        uint64_t unique_rows[num_key_parts];
        if (is_unique && num_key_parts == 1) {
            // dont compute for unique keys with a single part.  we already know the answer.
            rows = unique_rows[0] = 1;
        } else {
            DBC *cursor = NULL;
            error = db->cursor(db, txn, &cursor, 0);
            if (error == 0) {
                for (uint64_t i = 0; i < num_key_parts; i++)
                    unique_rows[i] = 1;
                // stop looking when the entire dictionary was analyzed, or a cap on execution time was reached, or the analyze was killed.
                DBT key = {}; key.flags = DB_DBT_REALLOC;
                DBT prev_key = {}; prev_key.flags = DB_DBT_REALLOC;
                while (1) {
                    error = cursor->c_get(cursor, &key, 0, DB_NEXT);
                    if (error != 0) {
                        if (error == DB_NOTFOUND)
                        error = 0; // eof is not an error
                        break;
                    }
                    rows++;
                    // first row is a unique row, otherwise compare with the previous key
                    bool copy_key = false;
                    if (rows == 1) {
                        copy_key = true;
                    } else {
                        // compare this key with the previous key.  ignore appended PK for SK's.
                        // TODO if a prefix is different, then all larger keys that include the prefix are also different.
                        // TODO if we are comparing the entire primary key or the entire unique secondary key, then the cardinality must be 1,
                        // so we can avoid computing it.
                        for (uint64_t i = 0; i < num_key_parts; i++) {
                            int cmp = key_compare(db, &prev_key, &key, i+1);
                            if (cmp != 0) {
                                unique_rows[i]++;
                                copy_key = true;
                            }
                        }
                    }
                    // prev_key = key
                    if (copy_key) {
                        prev_key.data = realloc(prev_key.data, key.size);
                        assert(prev_key.data);
                        prev_key.size = key.size;
                        memcpy(prev_key.data, key.data, prev_key.size);
                    }
                    // check for limit
                    if (analyze_progress && (rows % 1000) == 0) {
                        error = analyze_progress(progress_extra, rows);
                        if (error)
                            break;
                    }
                }
                // cleanup
                free(key.data);
                free(prev_key.data);
                int close_error = cursor->c_close(cursor);
                assert(close_error == 0);
            }
        }
        // return cardinality
        if (error == 0 || error == ETIME) {
            for (uint64_t i = 0; i < num_key_parts; i++)
                rec_per_key_part[i]  = rows / unique_rows[i];
        }
        return error;
    }
}
